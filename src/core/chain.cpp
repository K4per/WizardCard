#include "engine_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard {
using namespace detail;
void GameEngine::startChain(ResponseWindow window, ChainLink root, PlayerId priority) {
    auto &s = state_;
    if (s.chain)
        throw std::logic_error("nested chain");
    if (root.item.source) {
        const auto &d = catalog_.at(s.cards.at(root.item.source).definition);
        root.speed = catalog_.advancedRules ? d.speed : 0;
        const auto &source = s.cards.at(root.item.source);
        root.castCost =
            root.kind == LinkKind::Preparation ? root.preparationLoad
            : root.kind == LinkKind::Spell && d.type == CardType::Analytic && !source.payments.empty()
                ? source.payments.back().paid
            : root.kind == LinkKind::Spell && d.type == CardType::Word
                ? source.settingPaid + (source.payments.empty() ? 0 : source.payments.back().paid)
                : Rules::castCost(s, catalog_, root.item.source);
    }
    root.id = s.nextLink++;
    ChainState chain;
    chain.id = s.nextChain++;
    chain.window = window;
    chain.initiator = root.item.owner;
    chain.priority = priority;
    chain.links.push_back(std::move(root));
    s.chain = std::move(chain);
    s.phaseGate = 0;
    s.events.push_back({"chain_open", "建立连锁：" + windowName(window)});
    s.events.push_back({"link_declared", "宣告链节 #" + std::to_string(s.chain->links.back().id), -1,
                        s.chain->links.back().item.source});
}
void GameEngine::beginEffect(CardId id, AfterEffect after) {
    const auto &c = state_.cards.at(id);
    const auto &d = catalog_.at(c.definition);
    ChainLink root;
    root.kind = after == AfterEffect::Action ? LinkKind::Action : LinkKind::Spell;
    root.item = snapshot(c, d);
    if (after != AfterEffect::Action)
        for (const auto &[sid, seal] : state_.cards)
            if (seal.zone == Zone::Attached && seal.host == id && catalog_.at(seal.definition).damageBonus)
                root.item.effects.push_back({EffectKind::Damage, catalog_.at(seal.definition).damageBonus,
                                             DamageType::Force, EffectRecipient::Opponent});
    if (after == AfterEffect::Spell && !c.payments.empty())
        root.paymentIndex = static_cast<int>(c.payments.size()) - 1;
    root.item.requiresSource = after == AfterEffect::Spell;
    root.paid = after == AfterEffect::Action ? d.cost : 0;
    root.addedLoad = after == AfterEffect::Action ? d.burden : 0;
    startChain(after == AfterEffect::Action ? ResponseWindow::Action : ResponseWindow::Cast, std::move(root),
               1 - c.owner);
}
void GameEngine::startTrigger(Trigger t) {
    ChainLink root;
    root.kind = LinkKind::Trigger;
    root.item = std::move(t);
    const int priority = 1 - root.item.owner;
    startChain(ResponseWindow::Trigger, std::move(root), priority);
}
void GameEngine::passResponse() {
    auto &c = *state_.chain;
    state_.events.push_back({"response_pass", "玩家 " + std::to_string(c.priority + 1) + " 放弃响应"});
    c.priority = 1 - c.priority;
    if (++c.passes == 2) {
        c.mode = ChainMode::Resolving;
        state_.events.push_back({"chain_resolve", "双方连续放弃，逆序结算"});
    }
}
std::string GameEngine::validateResponse(PlayerId actor, const Respond &cmd) const {
    const auto &s = state_;
    if (!s.chain || s.chain->mode != ChainMode::Building || actor != s.chain->priority)
        return "priority: 当前没有响应优先权";
    auto i = s.cards.find(cmd.card);
    if (i == s.cards.end() || i->second.owner != actor)
        return "invalid_card: 无效响应来源";
    const auto &c = i->second;
    const auto &d = catalog_.at(c.definition);
    auto a = ability(d, cmd.ability);
    if (!a)
        return "ability: 未声明该响应能力";
    const bool hand = c.zone == Zone::Hand;
    const bool ambush = c.faceDown && a->fromAmbush && c.ambushedTurn < s.globalTurn;
    const bool analytic = !c.faceDown && c.zone == Zone::Analysis && c.spell == SpellState::Ready &&
                          a->fromAnalysis && c.canceledTurn != s.players[actor].ownTurn;
    if (catalog_.advancedRules) {
        const auto top = s.chain->links.back().speed;
        if (hand || d.speed == 1 || top == 4 || d.speed < top)
            return "spell_speed: 当前速度或来源不能加入连锁";
        if (d.speed == 2 && s.phase != Phase::Main && s.phase != Phase::Cast)
            return "spell_speed: 二速仅可在主要或施法阶段响应";
        if (!a->phases.empty() && std::find(a->phases.begin(), a->phases.end(), s.phase) == a->phases.end())
            return "response_phase: 不符合响应阶段";
    }
    if (!((hand && a->fromHand) ||
          (!c.faceDown && c.zone == Zone::Words && d.type == CardType::Word && a->fromWords) || ambush ||
          analytic))
        return "response_source: 当前区域不允许响应";
    if (std::find(s.chain->responded.begin(), s.chain->responded.end(), cmd.card) != s.chain->responded.end())
        return "response_used: 本连锁已使用该实体";
    if (std::find(a->windows.begin(), a->windows.end(), s.chain->window) == a->windows.end() ||
        (a->eventOwner == EventOwner::Self && actor != s.chain->initiator) ||
        (a->eventOwner == EventOwner::Opponent && actor == s.chain->initiator))
        return "response_window: 不符合响应时机";
    if (a->target == TargetKind::PendingLink || a->target == TargetKind::PreparationRoot ||
        a->target == TargetKind::EnemySpellLink) {
        if (cmd.target || !cmd.targets.empty() || !Rules::linkTargetValid(s, a->target, cmd.link))
            return "target: 无效链节目标";
        if (a->target == TargetKind::EnemySpellLink) {
            const auto l = std::find_if(s.chain->links.begin(), s.chain->links.end(),
                                        [&](const ChainLink &x) { return x.id == cmd.link; });
            if (l == s.chain->links.end() || l->item.owner == actor || !l->item.source ||
                (catalog_.at(s.cards.at(l->item.source).definition).type != CardType::Analytic &&
                 catalog_.at(s.cards.at(l->item.source).definition).type != CardType::Word))
                return "target: 必须指定对手释放中的法术";
        }
    } else if (cmd.link || !validTargets(s, a->target, a->targetCount, actor, cmd.target, cmd.targets))
        return "target: 无效响应目标";
    auto discard = s.cards.find(cmd.discard);
    if (a->extraDiscard) {
        if (a->extraDiscard != 1 || cmd.discard == cmd.card || discard == s.cards.end() ||
            discard->second.owner != actor || discard->second.zone != Zone::Hand)
            return "extra_cost: 需要另一张手牌";
    } else if (cmd.discard)
        return "extra_cost: 无需弃牌";
    const int paid =
        analytic ? Rules::castCost(s, catalog_, c.id)
        : ambush ? d.cost
        : hand
            ? a->handCost
            : a->preloadedCost + (d.type == CardType::Word ? Rules::castCost(s, catalog_, c.id) - d.cost : 0);
    const int bound = analytic || d.type == CardType::Word ? paid : 0;
    if (paid < 0 || a->burden < 0 || s.players[actor].mana < paid ||
        Rules::load(s, actor) + bound + a->burden > Rules::capacity(s, catalog_, actor))
        return "cost: 魔素或荷载空间不足";
    return {};
}
std::vector<Respond> GameEngine::responsesFor(PlayerId p, DecisionId decisionId) const {
    std::vector<Respond> out;
    const auto &s = state_;
    if (!s.chain || s.chain->mode != ChainMode::Building || s.chain->priority != p)
        return out;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && (c.zone == Zone::Hand || c.zone == Zone::Words || c.zone == Zone::Analysis))
            for (const auto &a : catalog_.at(c.definition).responses) {
                if ((c.zone == Zone::Hand && !a.fromHand) ||
                    std::find(a.windows.begin(), a.windows.end(), s.chain->window) == a.windows.end())
                    continue;
                std::vector<LinkId> links{0};
                if (a.target == TargetKind::PendingLink || a.target == TargetKind::PreparationRoot ||
                    a.target == TargetKind::EnemySpellLink) {
                    links.clear();
                    for (const auto &l : s.chain->links)
                        if (Rules::linkTargetValid(s, a.target, l.id))
                            links.push_back(l.id);
                }
                auto targets = sets(s, a.target, a.targetCount, p);
                std::vector<CardId> discards{0};
                if (a.extraDiscard) {
                    discards = inZone(s, p, Zone::Hand);
                    discards.erase(std::remove(discards.begin(), discards.end(), id), discards.end());
                }
                for (auto link : links)
                    for (const auto &ids : targets)
                        for (auto discard : discards) {
                            Respond r{decisionId, id,      a.id, ids.empty() ? 0 : ids.front(),
                                      link,       discard, ids};
                            if (validateResponse(p, r).empty())
                                out.push_back(std::move(r));
                        }
            }
    return out;
}
void GameEngine::finishLink(bool success) {
    auto &s = state_;
    const auto link = s.chain->links.back();
    auto i = s.cards.find(link.item.source);
    if (i != s.cards.end()) {
        auto &c = i->second;
        const auto &d = catalog_.at(c.definition);
        if (!success && link.paymentIndex >= 0)
            c.payments.at(static_cast<std::size_t>(link.paymentIndex)).canceled = true;
        if (link.kind == LinkKind::Preparation) {
            if (success) {
                c.zone = Zone::Casting;
                c.spell = SpellState::Pending;
                c.host = 0;
                s.events.push_back({"prepared", "法术进入施法区", -1, c.id});
            } else {
                if (c.zone == Zone::Analysis && !link.sourceLost) {
                    c.castLoad = std::max(0, c.castLoad - link.preparationLoad);
                    c.canceledTurn = s.players[c.owner].ownTurn;
                }
                if (link.paymentIndex >= 0) {
                    auto &pay = c.payments.at(static_cast<std::size_t>(link.paymentIndex));
                    pay.canceled = true;
                    pay.load = 0;
                }
            }
        } else if ((link.kind == LinkKind::Spell && c.zone == Zone::Casting) ||
                   (link.kind == LinkKind::Response && d.type == CardType::Word &&
                    c.zone == Zone::Resolving)) {
            if (success) {
                for (const auto &[sid, seal] : s.cards)
                    if (seal.host == c.id && seal.zone == Zone::Attached) {
                        const auto &sealDef = catalog_.at(seal.definition);
                        if (sealDef.refundSetting || (sealDef.refundCast && !c.payments.empty())) {
                            const int refund = sealDef.refundSetting ? c.settingPaid : c.payments.back().paid;
                            Trigger t;
                            t.owner = c.owner;
                            t.source = c.id;
                            EffectResolver::apply(s, catalog_, t, {EffectKind::GainMana, refund});
                            s.events.push_back(
                                {"refund",
                                 sealDef.refundSetting ? "成功释放：返还设置费用" : "成功释放：返还施法费用",
                                 -1, c.id, refund});
                        }
                    }
                if (d.concentration) {
                    std::vector<CardId> old;
                    for (const auto &[id, other] : s.cards)
                        if (id != c.id && other.owner == c.owner && other.zone == Zone::Casting &&
                            other.spell == SpellState::Active && catalog_.at(other.definition).concentration)
                            old.push_back(id);
                    for (auto id : old)
                        StateMaintenance::leave(s, id, &catalog_);
                    c.zone = Zone::Casting;
                    c.spell = SpellState::Active;
                    c.concentrationCost = Rules::castCost(s, catalog_, c.id);
                } else if (d.duration) {
                    c.spell = SpellState::Active;
                    c.remaining = d.duration;
                } else
                    StateMaintenance::leave(s, c.id, &catalog_);
            } else {
                if (link.paymentIndex >= 0)
                    c.payments.at(static_cast<std::size_t>(link.paymentIndex)).canceled = true;
                StateMaintenance::leave(s, c.id, &catalog_);
            }
        } else if (link.kind == LinkKind::Action || link.kind == LinkKind::Response) {
            if (!success && link.paymentIndex >= 0)
                c.payments.at(static_cast<std::size_t>(link.paymentIndex)).canceled = true;
            const auto expected = link.kind == LinkKind::Action ? Zone::Action : Zone::Resolving;
            if (c.zone == expected)
                StateMaintenance::leave(s, c.id, &catalog_);
        }
    }
    s.events.push_back({success ? "link_resolved" : "link_skipped",
                        (success ? "结算" : "取消或失效：") + std::string("链节 #") + std::to_string(link.id),
                        -1, link.item.source});
    s.events.back().spellReleased =
        success && (link.kind == LinkKind::Spell ||
                    (link.kind == LinkKind::Response &&
                     catalog_.at(s.cards.at(link.item.source).definition).type == CardType::Word));
    s.effect.reset();
    s.chain->links.pop_back();
    if (s.chain->links.empty()) {
        s.events.push_back({"chain_closed", "连锁结束"});
        s.chain.reset();
    }
    StateMaintenance::check(s, catalog_);
}
void GameEngine::finishEffect() {
    if (state_.effect->link) {
        finishLink(true);
        return;
    }
    auto frame = *state_.effect;
    state_.effect.reset();
    if (frame.after == AfterEffect::Action || frame.after == AfterEffect::Word)
        StateMaintenance::leave(state_, frame.item.source, &catalog_);
    StateMaintenance::check(state_, catalog_);
}
} // namespace wizard
