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
            const auto &d = catalog().at(c.definition);
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
StepResult GameEngine::step(ExecutionContext &context) {
    auto &s = state_;
    if (s.opening || s.result != -1) return StepResult::Finished;
    if (s.decision) return StepResult::AwaitChoice;
    if (!context.remaining) {
        context.fault = "execution_budget_exhausted";
        return StepResult::Fault;
    }
    --context.remaining;
    const auto key = cycleKey();
    if (!context.checkpoint.empty() && key == context.checkpoint) {
        if (effectSequences() == context.checkpointEffects) {
            s.result = 2; s.queue.items.clear(); s.deferredPreparations.clear();
            s.effect.reset(); s.chain.reset(); s.phaseGate = 0;
            s.events.push_back({"game_over", "强制循环：平局"});
            return StepResult::Finished;
        }
    }
    if (context.checkpoint.empty() || context.cycleLength == context.cyclePower) {
        context.checkpoint = key;
        context.checkpointEffects = effectSequences();
        if (context.cycleLength) context.cyclePower *= 2;
        context.cycleLength = 0;
    }
    ++context.cycleLength;
        bool overflow = false;
        for (const auto &[id, c] : s.cards)
            if (c.zone == Zone::Analysis && catalog().at(c.definition).type == CardType::Formation &&
                Rules::occupied(s, id) > Rules::rings(s, catalog(), id)) {
                std::vector<CardId> opts;
                for (const auto &[sid, x] : s.cards)
                    if (x.host == id && x.zone == Zone::Analysis)
                        opts.push_back(sid);
                decision(c.owner, DecisionKind::Overflow, std::move(opts));
                overflow = true;
                break;
            }
        if (overflow)
            return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
        if (s.effect) {
            s.effect->beginCurrentEffect();
            if (contentEffectV2()) return StepResult::Progress;
            auto &f = *s.effect;
            if (f.cursor == f.item.effects.size()) {
                finishEffect();
                return StepResult::Progress;
            }
            auto e = f.item.effects[f.cursor];
            if (e.kind == EffectKind::ClearAnyLoad) {
                if (f.resume<RepeatedChoiceContinuation>().remaining < 0) f.resume<RepeatedChoiceContinuation>().remaining = e.amount;
                std::vector<ChoiceValue> opts;
                for (const auto &[id, card] : s.cards)
                    if (card.owner == f.item.owner && card.analysisLoad + card.castLoad > 0) opts.push_back(ChoiceValue::card(id));
                for (const auto &load : s.temporary)
                    if (load.owner == f.item.owner && load.amount > 0) opts.push_back(ChoiceValue::load(load.id));
                if (!f.resume<RepeatedChoiceContinuation>().remaining || opts.empty()) {
                    ++f.cursor; f.resume<RepeatedChoiceContinuation>().remaining = -1; return StepResult::Progress;
                }
                typedDecision(f.item.owner, DecisionKind::ClearAnyLoad, std::move(opts), true);
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            if (e.kind == EffectKind::ClearTemporary) {
                if (f.resume<RepeatedChoiceContinuation>().remaining < 0)
                    f.resume<RepeatedChoiceContinuation>().remaining = e.amount;
                std::vector<ChoiceValue> opts;
                for (const auto &t : s.temporary)
                    if (t.owner == f.item.owner && !t.independent && t.amount > 0)
                        opts.push_back(ChoiceValue::load(t.id));
                if (!f.resume<RepeatedChoiceContinuation>().remaining || opts.empty()) {
                    ++f.cursor;
                    f.resume<RepeatedChoiceContinuation>().remaining = -1;
                    return StepResult::Progress;
                }
                typedDecision(f.item.owner, DecisionKind::ClearLoad, std::move(opts), true);
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            if (e.kind == EffectKind::SearchAction || e.kind == EffectKind::DestroyAmbush) {
                std::vector<CardId> opts;
                const auto &source_definition = catalog().at(s.cards.at(f.item.source).definition);
                const bool storm_search = hasCapability(source_definition, Capability::StormSearch);
                const bool storm_search_talisman = hasCapability(source_definition, Capability::StormSearchTalisman);
                for (const auto &[id, c] : s.cards) {
                    const auto &d = catalog().at(c.definition);
                    if (e.kind == EffectKind::SearchAction
                            ? (c.owner == f.item.owner && c.zone == Zone::Deck &&
                               (storm_search || storm_search_talisman
                                    ? detail::stormTags(d.tags) &&
                                      (!storm_search_talisman ||
                                       (d.type == CardType::Talisman && d.rank <= 2))
                                    : d.type == CardType::Action && (d.rarity == "common" || d.rarity == "uncommon")))
                            : (c.owner != f.item.owner && c.faceDown &&
                               ((c.zone == Zone::Analysis &&
                                (d.type == CardType::Analytic || d.type == CardType::Word || d.type == CardType::Talisman)))))
                        opts.push_back(id);
                }
                if (opts.empty()) {
                    ++f.cursor;
                    return StepResult::Progress;
                }
                decision(f.item.owner,
                         e.kind == EffectKind::SearchAction ? DecisionKind::SearchDeck
                                                            : DecisionKind::DestroyAmbush,
                         std::move(opts), e.kind == EffectKind::DestroyAmbush);
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            if (e.kind == EffectKind::OptionalDestroyOwnFormation) {
                std::vector<CardId> opts;
                for (const auto &[id, c] : s.cards)
                    if (c.owner == f.item.owner && c.zone == Zone::Analysis && !c.base &&
                        catalog().at(c.definition).type == CardType::Formation && Rules::occupied(s, id) == 0)
                        opts.push_back(id);
                if (opts.empty()) {
                    ++f.cursor;
                    return StepResult::Progress;
                }
                decision(f.item.owner, DecisionKind::DestroyOwnFormation, std::move(opts), true);
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            if (e.kind == EffectKind::DiscardHand || e.kind == EffectKind::DiscardFormation) {
                auto opts = inZone(s, f.item.owner,
                    e.kind == EffectKind::DiscardFormation ? Zone::SideDeck : Zone::Hand);
                if (e.kind == EffectKind::DiscardFormation)
                    opts.erase(std::remove_if(opts.begin(), opts.end(),
                                              [&](CardId id) {
                                                  return catalog().at(s.cards.at(id).definition).type !=
                                                         CardType::Formation;
                                              }),
                               opts.end());
                if (opts.empty()) {
                    f.cursor = f.item.effects.size(); // dependent remainder cannot be performed
                    s.events.push_back(
                        {"effect_incomplete", e.kind == EffectKind::DiscardFormation && true
                             ? "副卡组没有可送去灰烬区的阵法，后续效果不执行"
                             : "没有可弃置手牌，后续效果不执行", -1, f.item.source});
                    return StepResult::Progress;
                }
                decision(f.item.owner, DecisionKind::EffectDiscard, std::move(opts));
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            if (e.kind == EffectKind::OptionalPrepare) {
                auto candidates = preparationsFor(f.item.owner, s.nextDecision);
                if (candidates.empty()) {
                    ++f.cursor;
                    return StepResult::Progress;
                }
                std::vector<CardId> opts;
                for (const auto &cmd : candidates)
                    if (std::find(opts.begin(), opts.end(), cmd.card) == opts.end())
                        opts.push_back(cmd.card);
                decision(f.item.owner, DecisionKind::EffectPrepare, std::move(opts), true);
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            EffectResolver::apply(s, catalog(), f.item, e);
            if (s.result != -1)
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished; // Immediate rules damage can clear the effect frame and entire chain.

            ++f.cursor;
            return StepResult::Progress;
        }
        if (s.chain) {
            auto &chain = *s.chain;
            if (chain.mode == ChainMode::Building) {
                auto responses = responsesFor(chain.priority, s.nextDecision);
                auto setups = responseSetupsV2(chain.priority, s.nextDecision);
                std::vector<CardId> opts;
                for (const auto &r : responses)
                    if (std::find(opts.begin(), opts.end(), r.card) == opts.end())
                        opts.push_back(r.card);
                for (const auto &setup : setups)
                    if (std::find(opts.begin(), opts.end(), setup.source) == opts.end()) opts.push_back(setup.source);
                decision(chain.priority, DecisionKind::Response, std::move(opts), true);
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            auto &link = chain.links.back();
            bool valid = !link.canceled;
            auto source = s.cards.find(link.item.source);
            if (link.kind == LinkKind::Preparation)
                valid = valid && !link.sourceLost && source != s.cards.end() &&
                        source->second.zone == Zone::Analysis && source->second.spell == SpellState::Ready;
            if (link.kind == LinkKind::Preparation)
                valid = valid && static_cast<int>(inZone(s, link.item.owner, Zone::Casting).size()) <
                                     Rules::castingCapacity(s, catalog(), link.item.owner);
            if (link.kind == LinkKind::Spell)
                valid = valid && !link.sourceLost && source != s.cards.end() &&
                        source->second.zone == Zone::Casting && source->second.spell == SpellState::Pending;
            else if (link.kind != LinkKind::Preparation && link.item.requiresSource)
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
                valid = valid && !ids.empty() && (ids.size() == requiredCount);
                item.targetCards = ids;
                item.targetCard = ids.empty() ? 0 : ids.front();
            }
            if (item.target == TargetKind::PendingLink || item.target == TargetKind::PreparationRoot ||
                item.target == TargetKind::EnemySpellLink)
                valid = valid && Rules::linkTargetValid(s, item.target, item.targetLink);
            if (!valid) {
                finishLink(false);
                return StepResult::Progress;
            }
            s.effect = EffectFrame{std::move(item), 0, AfterEffect::None, -1, 0, link.id};
            return StepResult::Progress;
        }
        if (!s.deferredPreparations.empty()) {
            auto root = std::move(s.deferredPreparations.front());
            s.deferredPreparations.pop_front();
            const int priority = 1 - root.item.owner;
            startChain(ResponseWindow::Prepare, std::move(root), priority);
            return StepResult::Progress;
        }
        if (s.returnRequested && returnSpellsV2())
            return StepResult::Progress;
        if (s.returnRequested && s.decision)
            return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
        // End cleanup batches are deferred until expiry, duration and forced discards finish.
        if (!s.queue.items.empty()) {
            const auto batch = s.queue.items.front().batch;
            PlayerId owner = s.queue.items.front().owner;
            for (const auto &t : s.queue.items)
                if (t.batch == batch && t.owner == s.active)
                    owner = s.active;
            std::vector<ChoiceValue> opts;
            for (const auto &t : s.queue.items)
                if (t.batch == batch && t.owner == owner)
                    opts.push_back({ChoiceKind::Trigger, t.id});
            if (opts.size() > 1) {
                typedDecision(owner, DecisionKind::TriggerOrder, std::move(opts));
                return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
            }
            // A trigger may have been reclassified while a preceding effect
            // resolved. Keep the queue progress-safe instead of dereferencing
            // an empty filtered batch; the queue front remains authoritative.
            if (opts.empty()) opts.push_back({ChoiceKind::Trigger, s.queue.items.front().id});
            auto i = std::find_if(s.queue.items.begin(), s.queue.items.end(),
                                  [&](const Trigger &t) { return t.id == opts.front().value; });
            auto t = *i;
            s.queue.items.erase(i);
            startTrigger(std::move(t));
            return StepResult::Progress;
        }
        if (s.timingPending) {
            s.timingPending = false;
            actionTimingV2(ResponseWindow::Action);
            return StepResult::Progress;
        }
        StateMaintenance::check(s, catalog());
        if (s.result != -1)
            return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
        if (phaseV2()) return StepResult::Progress;
        return StepResult::Finished;
    return s.decision ? StepResult::AwaitChoice : StepResult::Finished;
}
void GameEngine::pump() {
    ExecutionContext context;
    while (true) {
        const auto result = step(context);
        if (result == StepResult::Progress) continue;
        if (result == StepResult::Fault) throw ExecutionFault(context.fault);
        return;
    }
}
} // namespace wizard
