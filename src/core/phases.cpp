#include "engine_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard {
using namespace detail;
void GameEngine::enqueuePhase(bool) {
    auto &s = state_;
    const auto batch = s.nextBatch++;
    for (const auto &[id, c] : s.cards)
        if (c.owner == s.active && c.spell == SpellState::Active && c.zone == Zone::Casting) {
            const auto &d = catalog_.at(c.definition);
            const auto &effects = s.phase == Phase::Draw      ? d.onDraw
                                  : s.phase == Phase::Prepare ? d.onPrepare
                                  : s.phase == Phase::Main    ? d.onMain
                                  : s.phase == Phase::Cast    ? d.onCast
                                                              : d.onEnd;
            if (!effects.empty()) {
                auto t = snapshot(c, d);
                t.id = s.nextTrigger++;
                t.batch = batch;
                t.effects = effects;
                s.queue.append(std::move(t));
            }
        }
}
void GameEngine::pump() {
    auto &s = state_;
    std::set<std::string> seen;
    for (int steps = 0; s.result == -1 && !s.decision; ++steps) {
        if (steps >= 10000 || !seen.insert(cycleKey()).second) {
            s.result = 2;
            s.queue.items.clear();
            s.deferredPreparations.clear();
            s.effect.reset();
            s.chain.reset();
            s.phaseGate = 0;
            s.events.push_back({"game_over", "强制循环：平局"});
            break;
        }
        bool overflow = false;
        for (const auto &[id, c] : s.cards)
            if (c.zone == Zone::Analysis && catalog_.at(c.definition).type == CardType::Formation &&
                Rules::occupied(s, id) > Rules::rings(s, catalog_, id)) {
                std::vector<CardId> opts;
                for (const auto &[sid, x] : s.cards)
                    if (x.host == id && x.zone == Zone::Analysis)
                        opts.push_back(sid);
                decision(c.owner, DecisionKind::Overflow, std::move(opts));
                overflow = true;
                break;
            }
        if (overflow)
            break;
        if (s.effect) {
            auto &f = *s.effect;
            if (f.cursor == f.item.effects.size()) {
                finishEffect();
                continue;
            }
            auto e = f.item.effects[f.cursor];
            if (e.kind == EffectKind::ClearTemporary) {
                if (f.clearRemaining < 0)
                    f.clearRemaining = e.amount;
                std::vector<std::uint32_t> opts;
                for (const auto &t : s.temporary)
                    if (t.owner == f.item.owner && !t.independent && t.amount > 0)
                        opts.push_back(t.id);
                if (!f.clearRemaining || opts.empty()) {
                    ++f.cursor;
                    f.clearRemaining = -1;
                    continue;
                }
                decision(f.item.owner, DecisionKind::ClearLoad, std::move(opts), true);
                break;
            }
            if (e.kind == EffectKind::SearchAction || e.kind == EffectKind::DestroyAmbush) {
                std::vector<CardId> opts;
                for (const auto &[id, c] : s.cards) {
                    const auto &d = catalog_.at(c.definition);
                    if (e.kind == EffectKind::SearchAction
                            ? (c.owner == f.item.owner && c.zone == Zone::Deck &&
                               d.type == CardType::Action && (d.rarity == "common" || d.rarity == "uncommon"))
                            : (c.owner != f.item.owner && c.faceDown))
                        opts.push_back(id);
                }
                if (opts.empty()) {
                    ++f.cursor;
                    continue;
                }
                decision(f.item.owner,
                         e.kind == EffectKind::SearchAction ? DecisionKind::SearchDeck
                                                            : DecisionKind::DestroyAmbush,
                         std::move(opts), e.kind == EffectKind::DestroyAmbush);
                break;
            }
            if (e.kind == EffectKind::OptionalDestroyOwnFormation) {
                std::vector<CardId> opts;
                for (const auto &[id, c] : s.cards)
                    if (c.owner == f.item.owner && c.zone == Zone::Analysis && !c.base &&
                        catalog_.at(c.definition).type == CardType::Formation && Rules::occupied(s, id) == 0)
                        opts.push_back(id);
                if (opts.empty()) {
                    ++f.cursor;
                    continue;
                }
                decision(f.item.owner, DecisionKind::DestroyOwnFormation, std::move(opts), true);
                break;
            }
            if (e.kind == EffectKind::DiscardHand || e.kind == EffectKind::DiscardFormation) {
                auto opts = inZone(s, f.item.owner, Zone::Hand);
                if (e.kind == EffectKind::DiscardFormation)
                    opts.erase(std::remove_if(opts.begin(), opts.end(),
                                              [&](CardId id) {
                                                  return catalog_.at(s.cards.at(id).definition).type !=
                                                         CardType::Formation;
                                              }),
                               opts.end());
                if (opts.empty()) {
                    f.cursor = f.item.effects.size(); // dependent remainder cannot be performed
                    s.events.push_back(
                        {"effect_incomplete", "没有可弃置手牌，后续效果不执行", -1, f.item.source});
                    continue;
                }
                decision(f.item.owner, DecisionKind::EffectDiscard, std::move(opts));
                break;
            }
            if (e.kind == EffectKind::OptionalPrepare) {
                auto candidates = preparationsFor(f.item.owner, s.nextDecision);
                if (candidates.empty()) {
                    ++f.cursor;
                    continue;
                }
                std::vector<CardId> opts;
                for (const auto &cmd : candidates)
                    if (std::find(opts.begin(), opts.end(), cmd.card) == opts.end())
                        opts.push_back(cmd.card);
                decision(f.item.owner, DecisionKind::EffectPrepare, std::move(opts), true);
                break;
            }
            EffectResolver::apply(s, catalog_, f.item, e);
            if (s.result != -1)
                break; // Immediate rules damage can clear the effect frame and entire chain.
            ++f.cursor;
            continue;
        }
        if (s.chain) {
            auto &chain = *s.chain;
            if (chain.mode == ChainMode::Building) {
                auto responses = responsesFor(chain.priority, s.nextDecision);
                if (responses.empty()) {
                    passResponse();
                    continue;
                }
                std::vector<CardId> opts;
                for (const auto &r : responses)
                    if (std::find(opts.begin(), opts.end(), r.card) == opts.end())
                        opts.push_back(r.card);
                decision(chain.priority, DecisionKind::Response, std::move(opts), true);
                break;
            }
            auto &link = chain.links.back();
            bool valid = !link.canceled;
            auto source = s.cards.find(link.item.source);
            if (link.kind == LinkKind::Preparation)
                valid = valid && !link.sourceLost && source != s.cards.end() &&
                        source->second.zone == Zone::Analysis && source->second.spell == SpellState::Ready;
            else if (link.kind == LinkKind::Spell)
                valid = valid && !link.sourceLost && source != s.cards.end() &&
                        source->second.zone == Zone::Casting && source->second.spell == SpellState::Pending;
            else if (link.item.requiresSource)
                valid =
                    valid && !link.sourceLost && source != s.cards.end() && source->second.zone != Zone::Ash;
            auto item = link.item;
            if (cardTarget(item.target)) {
                auto ids = targetCards(item.targetCard, item.targetCards);
                const auto requiredCount = ids.size();
                ids.erase(std::remove_if(
                              ids.begin(), ids.end(),
                              [&](CardId id) { return !Rules::targetValid(s, item.target, item.owner, id); }),
                          ids.end());
                valid = valid && !ids.empty() && (!catalog_.advancedRules || ids.size() == requiredCount);
                item.targetCards = ids;
                item.targetCard = ids.empty() ? 0 : ids.front();
            }
            if (item.target == TargetKind::PendingLink || item.target == TargetKind::PreparationRoot ||
                item.target == TargetKind::EnemySpellLink)
                valid = valid && Rules::linkTargetValid(s, item.target, item.targetLink);
            if (!valid) {
                finishLink(false);
                continue;
            }
            s.effect = EffectFrame{std::move(item), 0, AfterEffect::None, -1, link.id};
            continue;
        }
        if (!s.deferredPreparations.empty()) {
            auto root = std::move(s.deferredPreparations.front());
            s.deferredPreparations.pop_front();
            const int priority = 1 - root.item.owner;
            startChain(ResponseWindow::Prepare, std::move(root), priority);
            continue;
        }
        // End cleanup batches are deferred until expiry, duration and forced discards finish.
        if (!s.queue.items.empty() && !(s.phase == Phase::End && s.phaseStep == PhaseStep::Cleanup)) {
            const auto batch = s.queue.items.front().batch;
            PlayerId owner = s.queue.items.front().owner;
            for (const auto &t : s.queue.items)
                if (t.batch == batch && t.owner == s.active)
                    owner = s.active;
            std::vector<std::uint32_t> opts;
            for (const auto &t : s.queue.items)
                if (t.batch == batch && t.owner == owner)
                    opts.push_back(t.id);
            if (opts.size() > 1) {
                decision(owner, DecisionKind::TriggerOrder, std::move(opts));
                break;
            }
            auto i = std::find_if(s.queue.items.begin(), s.queue.items.end(),
                                  [&](const Trigger &t) { return t.id == opts.front(); });
            auto t = *i;
            s.queue.items.erase(i);
            startTrigger(std::move(t));
            continue;
        }
        StateMaintenance::check(s, catalog_);
        if (s.result != -1)
            break;
        auto &pl = s.players[s.active];
        switch (s.phaseStep) {
        case PhaseStep::Enter: {
            s.phaseGate = 0;
            s.durationApplied = false;
            s.events.push_back({"phase_enter", "进入" + phaseName(s.phase) + "阶段"});
            if (s.phase == Phase::Draw) {
                s.flow = Flow::Draw;
                if (!(s.active == s.first && pl.ownTurn == 1)) {
                    Trigger t;
                    t.owner = s.active;
                    EffectResolver::apply(s, catalog_, t, {EffectKind::Draw, 1});
                }
                StateMaintenance::check(s, catalog_);
                if (s.result != -1)
                    break;
            }
            if (s.phase == Phase::Prepare) {
                s.phaseStep = PhaseStep::PrepareIncome;
                for (const auto &[id, c] : s.cards)
                    if (c.owner == s.active && c.zone == Zone::Casting && c.spell == SpellState::Active &&
                        catalog_.at(c.definition).concentration) {
                        std::vector<CardId> opts;
                        if (pl.mana >= c.concentrationCost &&
                            Rules::load(s, s.active) + c.concentrationCost <=
                                Rules::capacity(s, catalog_, s.active))
                            opts.push_back(id);
                        decision(s.active, DecisionKind::Concentration, std::move(opts), true);
                        break;
                    }
                break;
            }
            enqueuePhase(false);
            s.phaseStep = PhaseStep::StartWindow;
            break;
        }
        case PhaseStep::PrepareIncome: {
            s.flow = Flow::Income;
            int income = 1;
            for (auto id : inZone(s, s.active, Zone::Analysis)) {
                const auto &d = catalog_.at(s.cards.at(id).definition);
                if (d.type == CardType::Formation) {
                    income += d.income;
                    for (const auto &[sid, seal] : s.cards)
                        if (seal.zone == Zone::Attached && seal.host == id)
                            income += catalog_.at(seal.definition).incomeBonus;
                }
            }
            Trigger t;
            t.owner = s.active;
            EffectResolver::apply(s, catalog_, t, {EffectKind::GainMana, std::max(0, income)});
            s.events.push_back({"income", "准备阶段收入 " + std::to_string(income)});
            for (auto &[id, c] : s.cards)
                if (c.owner == s.active && !c.faceDown && c.spell == SpellState::Analyzing &&
                    pl.ownTurn - c.analysisStarted >= catalog_.at(c.definition).analysisTurns)
                    c.spell = SpellState::Ready;
            enqueuePhase(false);
            s.phaseStep = PhaseStep::StartWindow;
            break;
        }
        case PhaseStep::StartWindow: {
            s.phaseStep = PhaseStep::Body;
            ChainLink root;
            root.kind = LinkKind::Phase;
            root.item.owner = s.active;
            startChain(ResponseWindow::PhaseStart, std::move(root), s.active);
            break;
        }
        case PhaseStep::Body:
            if (s.phase == Phase::Main)
                s.flow = Flow::Main;
            if (s.phase == Phase::Cast) {
                s.flow = Flow::Cast;
                std::vector<CardId> opts;
                for (auto id : inZone(s, s.active, Zone::Casting))
                    if (s.cards.at(id).spell == SpellState::Pending)
                        opts.push_back(id);
                if (!opts.empty()) {
                    s.phaseGate = 0;
                    decision(s.active, DecisionKind::CastOrder, std::move(opts), catalog_.advancedRules);
                    return;
                }
            }
            if (!s.phaseGate)
                s.phaseGate = s.nextDecision++;
            return;
        case PhaseStep::EndWindow: {
            s.phaseStep = PhaseStep::EndTriggers;
            ChainLink root;
            root.kind = LinkKind::Phase;
            root.item.owner = s.active;
            startChain(ResponseWindow::PhaseEnd, std::move(root), s.active);
            break;
        }
        case PhaseStep::EndTriggers:
            s.phaseStep = PhaseStep::Cleanup;
            break;
        case PhaseStep::Cleanup:
            if (s.phase != Phase::End) {
                s.phaseStep = PhaseStep::Finish;
                break;
            }
            s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                             [&](const TemporaryLoad &t) {
                                                 return t.amount == 0 ||
                                                        (!t.independent && !t.sourceBound &&
                                                         t.owner == s.active && t.expiryTurn <= pl.ownTurn);
                                             }),
                              s.temporary.end());
            if (!s.durationApplied) {
                s.durationApplied = true;
                std::vector<CardId> expired;
                for (auto &[id, c] : s.cards)
                    if (c.owner == s.active && c.spell == SpellState::Active &&
                        !catalog_.at(c.definition).concentration && --c.remaining <= 0)
                        expired.push_back(id);
                for (auto id : expired)
                    StateMaintenance::leave(s, id, &catalog_);
                StateMaintenance::check(s, catalog_);
                if (s.result != -1)
                    break;
            }
            {
                auto hand = inZone(s, s.active, Zone::Hand);
                if (hand.size() > 8) {
                    decision(s.active, DecisionKind::Discard, std::move(hand));
                    return;
                }
            }
            s.phaseStep = PhaseStep::CleanupTriggers;
            break;
        case PhaseStep::CleanupTriggers:
            if (inZone(s, s.active, Zone::Hand).size() > 8 ||
                std::any_of(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &t) {
                    return t.amount == 0 || (!t.independent && !t.sourceBound && t.owner == s.active &&
                                             t.expiryTurn <= pl.ownTurn);
                })) {
                s.phaseStep = PhaseStep::Cleanup;
                break;
            }
            s.phaseStep = PhaseStep::Finish;
            break;
        case PhaseStep::Finish:
            if (catalog_.alphaV2Draft && s.phase == Phase::End) {
                StateMaintenance::check(s, catalog_, true);
                if (s.result != -1)
                    break;
            }
            s.events.push_back({"phase_end", phaseName(s.phase) + "阶段结束"});
            if (s.phase == Phase::End) {
                pl.actionsPlayed = 0;
                pl.formations = 0;
                pl.sealRemovals = 0;
                for (auto &[id, c] : s.cards)
                    if (c.owner == s.active)
                        c.canceledTurn = -1;
                ++s.globalTurn;
                s.active = 1 - s.active;
                ++s.players[s.active].ownTurn;
                s.phase = Phase::Draw;
                s.flow = Flow::Draw;
            } else
                s.phase = static_cast<Phase>(static_cast<int>(s.phase) + 1);
            s.phaseStep = PhaseStep::Enter;
            s.phaseGate = 0;
            break;
        }
    }
}
} // namespace wizard
