#include "wizard/core/rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
void StateMaintenance::leave(GameState &s, CardId id, const CardCatalog *cat) {
    std::vector<Trigger> captured;
    std::uint32_t batch = 0;
    auto leaveOne = [&](auto &&self, CardId source) -> void {
        auto i = s.cards.find(source);
        if (i == s.cards.end() || i->second.zone == Zone::Ash || i->second.base)
            return;
        auto &c = i->second;
        std::vector<CardId> children;
        for (const auto &[cid, x] : s.cards)
            if (x.host == source && (x.zone == Zone::Attached || x.zone == Zone::Analysis))
                children.push_back(cid);
        const bool publicSource = c.zone != Zone::Hand && c.zone != Zone::Deck;
        if (cat && publicSource && !cat->at(c.definition).onLeave.empty()) {
            if (!batch)
                batch = s.nextBatch++;
            const auto &d = cat->at(c.definition);
            Trigger t;
            t.id = s.nextTrigger++;
            t.batch = batch;
            t.owner = c.owner;
            t.source = source;
            t.target = d.target;
            t.targetCard = c.targetCard;
            t.targetCards = c.targets;
            t.targetPlayer = c.targetPlayer;
            t.effects = d.onLeave;
            captured.push_back(std::move(t));
        }
        for (auto child : children)
            self(self, child);
        for (auto &pending : s.deferredPreparations)
            if (pending.item.source == source)
                pending.sourceLost = true;
        if (s.chain)
            for (auto &link : s.chain->links)
                if (link.item.source == source)
                    link.sourceLost = true;
        auto &deck = s.players[c.owner].deck;
        deck.erase(std::remove(deck.begin(), deck.end(), source), deck.end());
        c.zone = Zone::Ash;
        c.spell = SpellState::None;
        c.host = 0;
        c.analysisLoad = 0;
        c.castLoad = 0;
        c.remaining = 0;
        c.faceDown = false;
        {
            s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                             [&](const TemporaryLoad &t) {
                                                 return t.source == source &&
                                                        (t.sourceBound ||
                                                         (cat && cat->at(c.definition).concentration));
                                             }),
                              s.temporary.end());
        }
        s.events.push_back({"leave", "卡牌进入灰烬区", -1, source});
    };
    leaveOne(leaveOne, id);
    // Capture one associated departure batch before mutation; publish only after
    // the entire host tree has left. Each owner chooses its order in the pump.
    for (auto &trigger : captured)
        s.queue.append(std::move(trigger));
}
void StateMaintenance::check(GameState &s, const CardCatalog &cat) {
    if (s.result != -1)
        return;
    bool loses[2]{};
    for (int p = 0; p < 2; ++p)
        loses[p] = s.players[p].life <= 0 || s.players[p].drawFailed || s.players[p].surrendered ||
                   Rules::load(s, p) > Rules::capacity(s, cat, p);
    if (loses[0] || loses[1]) {
        s.result = loses[0] && loses[1] ? 2 : loses[0] ? 1 : 0;
        for (int p = 0; p < 2; ++p)
            if (loses[p]) {
                std::string reason = "玩家 " + std::to_string(p + 1) + " 败北：";
                if (s.players[p].life <= 0)
                    reason += "生命归零 ";
                if (s.players[p].drawFailed)
                    reason += "抽牌失败 ";
                if (s.players[p].surrendered)
                    reason += "投降 ";
                if (Rules::load(s, p) > Rules::capacity(s, cat, p))
                    reason += "魔力过载 ";
                s.events.push_back({"defeat_reason", reason});
            }
        s.events.push_back(
            {"game_over", s.result == 2 ? "平局" : "玩家 " + std::to_string(s.result + 1) + " 获胜"});
        s.queue.items.clear();
        s.deferredPreparations.clear();
        s.decision.reset();
        s.chain.reset();
        s.effect.reset();
        s.phaseGate = 0;
    }
}
} // namespace wizard
