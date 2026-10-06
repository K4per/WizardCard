#include "wizard/core.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
const CardDefinition &CardCatalog::at(const std::string &id) const {
    return cards.at(id);
}
void EffectQueue::append(Trigger t) {
    items.push_back(std::move(t));
}
int Rules::load(const GameState &s, PlayerId p) {
    int n = 0;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p)
            n += c.analysisLoad + c.castLoad;
    for (const auto &t : s.temporary)
        if (t.owner == p)
            n += t.amount;
    return n;
}
int Rules::capacity(const GameState &s, const CardCatalog &cat, PlayerId p) {
    int n = cat.baseLoadCapacity;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation) {
            n += cat.at(c.definition).capacity;
            for (const auto &[sid, seal] : s.cards)
                if (seal.zone == Zone::Attached && seal.host == id)
                    n += cat.at(seal.definition).capacityBonus;
        }
    return n;
}
int Rules::rings(const GameState &s, const CardCatalog &cat, CardId id) {
    int n = cat.at(s.cards.at(id).definition).rings;
    for (const auto &[cid, c] : s.cards)
        if (c.zone == Zone::Attached && c.host == id)
            n += cat.at(c.definition).ringBonus;
    return n;
}
int Rules::maxRank(const GameState &s, const CardCatalog &cat, CardId id) {
    int n = cat.at(s.cards.at(id).definition).maxRank;
    for (const auto &[cid, c] : s.cards)
        if (c.zone == Zone::Attached && c.host == id)
            n += cat.at(c.definition).rankBonus;
    return n;
}
int Rules::analysisCost(const GameState &s, const CardCatalog &cat, CardId id, CardId host) {
    const auto &d = cat.at(s.cards.at(id).definition);
    const auto &h = s.cards.at(host);
    return std::max(0, d.cost - (h.base && cat.at(h.definition).baseAnalysisDiscount &&
                                         (d.school == "evocation" || d.school == "conjuration")
                                     ? 1
                                     : 0));
}
int Rules::castCost(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &c = s.cards.at(id);
    const auto &d = cat.at(c.definition);
    int n = d.type == CardType::Word ? d.cost : d.castCost;
    for (const auto &[sid, seal] : s.cards)
        if (seal.zone == Zone::Attached && seal.host == id)
            n += cat.at(seal.definition).castCostBonus;
    return n;
}
int Rules::damage(const GameState &s, const CardCatalog &cat, PlayerId p, const Effect &e) {
    bool resistance = false;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Casting && c.spell == SpellState::Active) {
            const auto &d = cat.at(c.definition);
            if (std::find(d.immunities.begin(), d.immunities.end(), e.damageType) != d.immunities.end())
                return 0;
            resistance |=
                std::find(d.resistances.begin(), d.resistances.end(), e.damageType) != d.resistances.end();
        }
    return resistance ? e.amount / 2 : e.amount;
}
int Rules::occupied(const GameState &s, CardId host) {
    int n = 0;
    for (const auto &[id, c] : s.cards)
        if (c.host == host && c.zone == Zone::Analysis)
            ++n;
    return n;
}
bool Rules::targetValid(const GameState &s, const CardDefinition &d, PlayerId p, CardId target) {
    return targetValid(s, d.target, p, target);
}
bool Rules::targetValid(const GameState &s, TargetKind kind, PlayerId p, CardId target) {
    if (kind == TargetKind::PendingLink || kind == TargetKind::PreparationRoot ||
        kind == TargetKind::EnemySpellLink)
        return false;
    if (kind == TargetKind::None || kind == TargetKind::Self || kind == TargetKind::Opponent)
        return target == 0;
    auto i = s.cards.find(target);
    if (i == s.cards.end())
        return false;
    const auto &c = i->second;
    if (c.base || c.zone == Zone::Deck || c.zone == Zone::Hand || c.zone == Zone::Ash)
        return false;
    if (kind == TargetKind::OwnAnalyzingSpell)
        return c.owner == p && c.zone == Zone::Analysis && !c.faceDown && c.spell == SpellState::Analyzing;
    if (kind == TargetKind::EmptyEnemyFormation)
        return c.owner != p && c.zone == Zone::Analysis && c.spell == SpellState::None &&
               occupied(s, target) == 0;
    return kind == TargetKind::AnyCard || (kind == TargetKind::EnemyCard && c.owner != p) ||
           (kind == TargetKind::OwnCard && c.owner == p);
}
bool Rules::linkTargetValid(const GameState &s, TargetKind kind, LinkId id) {
    if (!s.chain || !id)
        return false;
    for (const auto &l : s.chain->links)
        if (l.id == id)
            return !l.canceled && l.kind != LinkKind::Phase && (!s.effect || s.effect->link != id) &&
                   (kind == TargetKind::PendingLink ||
                    (kind == TargetKind::PreparationRoot && l.kind == LinkKind::Preparation) ||
                    (kind == TargetKind::EnemySpellLink &&
                     (l.kind == LinkKind::Spell || l.kind == LinkKind::Response)));
    return false;
}
std::vector<std::string> Rules::deckErrors(const CardCatalog &cat, const PlayerDeck &deck) {
    std::vector<std::string> out;
    if (deck.cards.size() != 30)
        out.push_back("deck must contain exactly 30 cards");
    std::map<std::string, int> counts;
    for (const auto &id : deck.cards) {
        auto i = cat.cards.find(id);
        if (i == cat.cards.end()) {
            out.push_back("unknown card: " + id);
            continue;
        }
        if (++counts[i->second.name] > 3)
            out.push_back("at most 3 cards per name");
    }
    auto i = cat.cards.find(deck.baseFormation);
    if (i == cat.cards.end())
        out.push_back("unknown base formation");
    else {
        const auto &d = i->second;
        if (d.type != CardType::Formation || !d.baseEligible || d.body < 1 || d.body > 5 || d.capacity <= 0 ||
            d.rings <= 0 || d.maxRank <= 0)
            out.push_back("ineligible base formation");
    }
    return out;
}
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
std::vector<std::string> Rules::invariants(const GameState &s, const CardCatalog &cat) {
    std::vector<std::string> errors;
    for (const auto &[id, c] : s.cards)
        if (!cat.cards.count(c.definition)) {
            errors.push_back("unknown definition");
            return errors;
        }
    if (s.active < 0 || s.active > 1 || s.first < 0 || s.first > 1)
        errors.push_back("invalid active player");
    if (s.chain) {
        if (s.chain->links.empty() || s.chain->priority < 0 || s.chain->priority > 1 || s.chain->passes < 0 ||
            s.chain->passes > 2)
            errors.push_back("invalid chain");
        std::set<LinkId> ids;
        for (const auto &l : s.chain->links)
            if (!l.id || !ids.insert(l.id).second)
                errors.push_back("duplicate chain link");
    }
    if (s.result != -1 && (s.chain || s.effect || s.decision || !s.queue.items.empty() ||
                           !s.deferredPreparations.empty() || s.phaseGate))
        errors.push_back("terminal work remains");
    for (int p = 0; p < 2; ++p) {
        if (s.players[p].mana < 0 || s.players[p].mana > 12)
            errors.push_back("mana bounds");
        std::map<CardId, int> seen;
        for (auto id : s.players[p].deck) {
            auto i = s.cards.find(id);
            if (++seen[id] != 1 || i == s.cards.end() || i->second.owner != p || i->second.zone != Zone::Deck)
                errors.push_back("deck membership");
        }
        int body = 0, words = 0, concentration = 0;
        for (const auto &[id, c] : s.cards)
            if (c.owner == p) {
                if (c.zone == Zone::Deck && seen[id] != 1)
                    errors.push_back("missing deck card");
                if (c.zone == Zone::Words)
                    ++words;
                if (c.zone == Zone::Casting && c.spell == SpellState::Active &&
                    cat.at(c.definition).concentration)
                    ++concentration;
                if (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation)
                    body += cat.at(c.definition).body;
            }
        if (concentration > 1)
            errors.push_back("multiple concentration spells");
        if (body > 5 || words > 3)
            errors.push_back("zone capacity");
    }
    for (const auto &[id, c] : s.cards) {
        if (c.id != id || c.owner < 0 || c.owner > 1 || c.analysisLoad < 0 || c.castLoad < 0)
            errors.push_back("instance bounds");
        if (c.base && c.zone != Zone::Analysis)
            errors.push_back("base protection");
        if (c.zone == Zone::Ash && (c.analysisLoad || c.castLoad || c.host))
            errors.push_back("ash residue");
        if (c.host) {
            auto h = s.cards.find(c.host);
            if (h == s.cards.end() || h->second.owner != c.owner ||
                (h->second.zone != Zone::Analysis && h->second.zone != Zone::Casting &&
                 h->second.zone != Zone::Words && h->second.zone != Zone::Resolving))
                errors.push_back("host relation");
            else if (c.zone == Zone::Analysis && cat.at(h->second.definition).type != CardType::Formation)
                errors.push_back("analysis host is not formation");
        }
        if ((c.zone == Zone::Attached ||
             (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Analytic)) &&
            !c.host)
            errors.push_back("missing host");
        if (c.zone == Zone::Attached && cat.at(c.definition).type != CardType::Seal)
            errors.push_back("non-seal attachment");
        int seals = 0;
        for (const auto &[other, x] : s.cards)
            if (x.zone == Zone::Attached && x.host == id)
                ++seals;
        if (seals > 1)
            errors.push_back("multiple seals");
        if (c.zone == Zone::Casting && c.host)
            errors.push_back("casting still attached to formation");
    }
    return errors;
}
void EffectResolver::apply(GameState &s, const CardCatalog &cat, const Trigger &t, const Effect &e) {
    int p = t.targetPlayer >= 0 ? t.targetPlayer : t.owner;
    if (e.recipient == EffectRecipient::Owner)
        p = t.owner;
    if (e.recipient == EffectRecipient::Opponent)
        p = 1 - t.owner;
    const int previousTemporaryLife = s.players[p].temporaryLife;
    const int previousLife = s.players[p].life;
    const auto previousDeck = s.players[t.owner].deck.size();
    switch (e.kind) {
    case EffectKind::Damage: {
        int amount = Rules::damage(s, cat, p, e);
        const int shield = std::min(amount, s.players[p].temporaryLife);
        s.players[p].temporaryLife -= shield;
        s.players[p].life -= amount - shield;
        break;
    }
    case EffectKind::AddTemporaryLife:
        s.players[p].temporaryLife = std::max(s.players[p].temporaryLife, e.amount);
        break;
    case EffectKind::BlockActions:
        s.players[p].blockedActionTurn = s.players[p].ownTurn + 1;
        break;
    case EffectKind::GrantResistance:
        break; // Successful release installs the passive resistance on the active spell.
    case EffectKind::AddSourceTemporary:
        s.temporary.push_back({s.nextLoad++, p, t.source, e.amount, 0, false, true});
        break;
    case EffectKind::AccelerateAnalysis: {
        auto &c = s.cards.at(t.targetCard);
        c.spell = SpellState::Ready;
        s.temporary.push_back(
            {s.nextLoad++, t.owner, t.source, c.settingPaid, s.players[t.owner].ownTurn + 1});
        break;
    }
    case EffectKind::Conceal: {
        auto &c = s.cards.at(t.targetCard);
        if (c.zone == Zone::Analysis && !c.base && cat.at(c.definition).type == CardType::Analytic) {
            c.faceDown = true;
            c.ambushedTurn = s.globalTurn;
            c.spell = SpellState::None;
            c.analysisStarted = s.players[c.owner].ownTurn;
            s.events.push_back({"conceal", "法术失去解析进度，转为埋伏", -1, c.id});
        }
        break;
    }
    case EffectKind::CounterSpell: {
        if (s.chain)
            for (auto &l : s.chain->links)
                if (l.id == t.targetLink) {
                    const int burden = l.castCost;
                    l.canceled = true;
                    const auto source = l.item.source;
                    StateMaintenance::leave(s, source, &cat);
                    s.temporary.push_back(
                        {s.nextLoad++, t.owner, t.source, burden, s.players[t.owner].ownTurn + 1});
                    break;
                }
        break;
    }
    case EffectKind::Heal:
        s.players[p].life = std::min(maximumLife, s.players[p].life + e.amount);
        break;
    case EffectKind::GainMana: {
        s.players[t.owner].mana = std::min(12, s.players[t.owner].mana + e.amount);
        const auto batch = s.nextBatch++;
        for (const auto &[id, c] : s.cards)
            if (c.owner == t.owner &&
                ((c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation) ||
                 (c.zone == Zone::Casting && c.spell == SpellState::Active)) &&
                !cat.at(c.definition).onMana.empty()) {
                Trigger next;
                next.id = s.nextTrigger++;
                next.batch = batch;
                next.owner = c.owner;
                next.source = id;
                next.targetCard = c.targetCard;
                next.targetPlayer = c.targetPlayer;
                next.target = cat.at(c.definition).target;
                next.targetCards = c.targets;
                next.effects = cat.at(c.definition).onMana;
                s.queue.append(std::move(next));
            }
        break;
    }
    case EffectKind::Draw:
        for (int k = 0; k < e.amount; ++k) {
            auto &pl = s.players[t.owner];
            if (pl.deck.empty()) {
                pl.drawFailed = true;
                continue;
            }
            auto id = pl.deck.back();
            pl.deck.pop_back();
            s.cards.at(id).zone = Zone::Hand;
            s.events.push_back({"draw", "抽取一张牌", t.owner, id});
        }
        break;
    case EffectKind::AddTemporary:
        s.temporary.push_back(
            {s.nextLoad++, p, t.source, e.amount, s.players[p].ownTurn + (p == s.active ? 0 : 1)});
        break;
    case EffectKind::AddIndependent:
        s.temporary.push_back({s.nextLoad++, p, t.source, e.amount, 0, true});
        break;
    case EffectKind::ClearIndependent:
        s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                         [&](const TemporaryLoad &load) {
                                             return load.owner == t.owner && load.independent;
                                         }),
                          s.temporary.end());
        break;
    case EffectKind::Destroy: {
        auto destroy = [&](CardId id) {
            auto i = s.cards.find(id);
            if (i != s.cards.end() && i->second.owner != t.owner &&
                cat.at(i->second.definition).opponentDestroyProtected) {
                s.events.push_back({"protected", "对手效果不能销毁此卡", -1, id});
                return;
            }
            StateMaintenance::leave(s, id, &cat);
        };
        if (t.targetCards.empty())
            destroy(t.targetCard);
        else
            for (auto id : t.targetCards)
                destroy(id);
        break;
    }
    case EffectKind::ClearAllTemporary:
        s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                         [&](const TemporaryLoad &load) {
                                             return load.owner == t.owner && !load.independent;
                                         }),
                          s.temporary.end());
        break;
    case EffectKind::SearchAction:
    case EffectKind::DestroyAmbush:
    case EffectKind::OptionalDestroyOwnFormation:
    case EffectKind::DiscardHand:
    case EffectKind::DiscardFormation:
    case EffectKind::OptionalPrepare:
        throw std::logic_error("effect requires a decision");
    case EffectKind::CancelPreparation:
    case EffectKind::NegateLink:
        if (s.chain)
            for (auto &l : s.chain->links)
                if (l.id == t.targetLink && l.id != (s.effect ? s.effect->link : 0) &&
                    l.kind != LinkKind::Phase &&
                    (e.kind == EffectKind::NegateLink || l.kind == LinkKind::Preparation))
                    l.canceled = true;
        break;
    case EffectKind::ClearTemporary:
        throw std::logic_error("clear temporary requires a decision");
    }
    std::string message;
    switch (e.kind) {
    case EffectKind::Damage:
        message =
            "造成 " + std::to_string(Rules::damage(s, cat, p, e)) + " 点" + damageName(e.damageType) + "伤害";
        break;
    case EffectKind::Heal:
        message = "恢复 " + std::to_string(e.amount) + " 点生命";
        break;
    case EffectKind::Draw:
        message = "抽取 " + std::to_string(e.amount) + " 张牌";
        break;
    case EffectKind::AddIndependent:
        message = "增加 " + std::to_string(e.amount) + " 点独立荷载";
        break;
    case EffectKind::ClearIndependent:
        message = "清除独立荷载";
        break;
    case EffectKind::AddTemporary:
        message = "增加 " + std::to_string(e.amount) + " 点临时荷载";
        break;
    case EffectKind::OptionalDestroyOwnFormation:
    case EffectKind::DiscardHand:
    case EffectKind::DiscardFormation:
    case EffectKind::OptionalPrepare:
    case EffectKind::ClearAllTemporary:
    case EffectKind::ClearTemporary:
        message = "清除临时荷载";
        break;
    case EffectKind::Destroy:
        message = "尝试销毁目标卡牌";
        break;
    case EffectKind::CancelPreparation:
        message = "取消本次准备施法";
        break;
    case EffectKind::GainMana:
        message = "获得 " + std::to_string(e.amount) + " 魔素";
        break;
    case EffectKind::AddSourceTemporary:
        message = "增加来源关联临时荷载";
        break;
    case EffectKind::AddTemporaryLife:
        message = "获得临时生命之核";
        break;
    case EffectKind::BlockActions:
        message = "对手下个自己的回合不能使用行动卡";
        break;
    case EffectKind::CounterSpell:
        message = "无效并销毁目标法术，承担下个自己的结束阶段清除的荷载";
        break;
    case EffectKind::SearchAction:
        message = "检索行动卡";
        break;
    case EffectKind::DestroyAmbush:
        message = "销毁埋伏卡";
        break;
    case EffectKind::AccelerateAnalysis:
        message = "立即完成解析并承担临时荷载";
        break;
    case EffectKind::GrantResistance:
        message = "获得伤害抗性";
        break;
    case EffectKind::Conceal:
        message = "法术转为埋伏";
        break;
    case EffectKind::NegateLink:
        message = "取消链节 #" + std::to_string(t.targetLink);
        break;
    }
    GameEvent event{"effect", message, -1, t.source, e.amount};
    event.effectType = e.kind;
    event.affectedPlayer = p;
    event.actualAmount = std::abs(s.players[p].life - previousLife);
    if (e.kind == EffectKind::Damage)
        event.actualAmount += previousTemporaryLife - s.players[p].temporaryLife;
    if (e.kind == EffectKind::Draw) {
        event.actualAmount = static_cast<int>(previousDeck - s.players[t.owner].deck.size());
        event.affectedPlayer = t.owner;
    }
    s.events.push_back(std::move(event));
}
std::string fingerprint(const std::string &bytes) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 1099511628211ull;
    }
    std::ostringstream o;
    o << std::hex << std::setfill('0') << std::setw(16) << h;
    return o.str();
}
std::string damageName(DamageType d) {
    return std::array<const char *, 12>{"火焰", "寒霜", "光耀", "黯蚀", "毒素", "闪电",
                                        "心灵", "声波", "力场", "挥砍", "钝击", "穿刺"}
        .at(static_cast<std::size_t>(d));
}
std::string schoolName(const std::string &s) {
    const std::map<std::string, std::string> names{{"evocation", "塑能系"},   {"transmutation", "变化系"},
                                                   {"conjuration", "咒法系"}, {"enchantment", "惑控系"},
                                                   {"illusion", "幻术系"},    {"abjuration", "防护系"},
                                                   {"necromancy", "死灵系"},  {"divination", "预言系"}};
    auto i = names.find(s);
    return i == names.end() ? "" : i->second;
}
std::string phaseName(Phase p) {
    return std::array<const char *, 5>{"抽卡", "准备", "主要", "施法", "结束"}.at(
        static_cast<std::size_t>(p));
}
std::string zoneName(Zone p) {
    return std::array<const char *, 9>{"牌库", "手牌", "行动", "解析", "言灵", "施法", "灰烬", "符文", "响应"}
        .at(static_cast<std::size_t>(p));
}
std::string windowName(ResponseWindow p) {
    return std::array<const char *, 6>{"阶段开始", "阶段结束", "行动宣告", "准备施法", "释放法术", "被动触发"}
        .at(static_cast<std::size_t>(p));
}
std::string linkName(LinkKind p) {
    return std::array<const char *, 6>{"阶段", "行动宣告", "准备施法", "释放法术", "响应", "被动触发"}.at(
        static_cast<std::size_t>(p));
}
} // namespace wizard
