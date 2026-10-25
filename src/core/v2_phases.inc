#include "engine_internal.hpp"
#include "wizard/core/card_rules.hpp"
#include "wizard/core/card_rules.hpp"
#include "wizard/core/card_rules.hpp"
#include <algorithm>
#include <limits>
namespace wizard {
using namespace detail;
namespace {
struct OrderedCard { std::uint64_t key{}; CardId id{}; };
OrderedCard nextFieldCard(const GameState &state, const CardCatalog &catalog, PlayerId owner,
                          bool formation, std::uint64_t after, bool anyPersistent = false) {
    OrderedCard result{std::numeric_limits<std::uint64_t>::max(), 0};
    for (const auto &[id, card] : state.cards) {
        if (controllerOf(card) != owner || card.zone != Zone::Analysis) continue;
        const auto &def = catalog.at(card.definition);
        if (formation != (def.type == CardType::Formation)) continue;
        if (!formation && (card.faceDown || card.spell != SpellState::Active || (!anyPersistent && !def.perpetual) || !card.host)) continue;
        std::uint64_t key = formation ? card.fieldOrder :
            (state.cards.at(card.host).fieldOrder << 16) + static_cast<std::uint64_t>(card.ringIndex + 1);
        if (key > after && key < result.key) result = {key, id};
    }
    return result;
}
}
bool GameEngine::returnSpellsV2() {
    auto &s = state_;
    if (!s.returnRequested || s.decision || s.chain || s.effect) return false;
    std::vector<CardId> pending;
    for (const auto &[id, spell] : s.cards)
        if (spell.zone == Zone::Casting && spell.spell == SpellState::Active) pending.push_back(id);
    auto order = [&](CardId id) {
        const auto &spell = s.cards.at(id);
        const auto host = s.cards.find(spell.sourceFormation);
        const auto formationOrder = host == s.cards.end() ? spell.fieldOrder : host->second.fieldOrder;
        return std::make_tuple(controllerOf(spell) != s.active, formationOrder, spell.sourceRingIndex, spell.fieldOrder, id);
    };
    std::sort(pending.begin(), pending.end(), [&](CardId a, CardId b) { return order(a) < order(b); });
    for (const auto id : pending) {
        const auto &spell = s.cards.at(id);
        if (spell.zone != Zone::Casting || spell.spell != SpellState::Active) continue;
        const auto &definition = catalog_.at(spell.definition);
        if (!(definition.perpetual || definition.concentration || definition.duration > 0)) continue;
        std::vector<std::uint32_t> options;
        for (const auto &[hostId, host] : s.cards)
            if (controllerOf(host) == controllerOf(spell) && host.zone == Zone::Analysis &&
                catalog_.at(host.definition).type == CardType::Formation &&
                (std::find(definition.traits.begin(), definition.traits.end(), "requires_storm_heart_setup") ==
                     definition.traits.end() || catalog_.at(host.definition).id == "storm-heart") &&
                Rules::canHost(s, catalog_, id, hostId) &&
                Rules::freeRing(s, catalog_, hostId) >= 0)
                options.push_back(hostId);
        if (options.empty()) {
            StateMaintenance::leave(s, id, &catalog_);
            // No legal host remains, so the maintenance source has been
            // consumed and must not leak into a later decision or snapshot.
            s.maintenanceSource = 0;
            return true;
        }
        s.maintenanceSource = id;
        decision(controllerOf(spell), DecisionKind::ReturnSpell, std::move(options));
        return true;
    }
    s.returnRequested = false;
    return false;
}
bool GameEngine::phaseV2() {
    auto &s = state_;
    auto &pl = s.players[s.active];
    auto triggered = [&](CardId id, const std::vector<Effect> &effects) {
        if (effects.empty()) return false;
        const auto &card = s.cards.at(id);
        auto trigger = snapshot(card, catalog_.at(card.definition));
        trigger.id = s.nextTrigger++;
        trigger.batch = s.nextBatch++;
        trigger.effects = effects;
        startTrigger(std::move(trigger));
        return true;
    };
    switch (s.phaseStep) {
    case PhaseStep::Enter: {
        if (catalog_.usesAlphaV252() && s.phase == Phase::End)
            for (auto &player : s.players) player.nextDamageImmune = false;
        if (s.phase == Phase::Prepare && pl.siqueDamageCap && pl.ownTurn >= pl.siqueCapUntilOwnTurn) {
            pl.siqueDamageCap = 0; pl.siqueCapUntilOwnTurn = 0;
        }
        if (pl.siqueLifePaymentTurn != s.globalTurn) pl.siqueLifePaymentTurn = 0;
        s.phaseGate = 0; s.phaseCursor = 0; s.phaseOrderCursor = 0;
        s.durationApplied = false;
        s.events.push_back({"phase_enter", "进入" + phaseName(s.phase) + "阶段"});
        s.phaseStep = s.phase == Phase::End && !catalog_.usesAlphaV25() ? PhaseStep::StartWindow : PhaseStep::PrepareIncome;
        return true;
    }
    case PhaseStep::PrepareIncome: {
        if (s.phase == Phase::Draw && s.phaseCursor == 0) {
            if (catalog_.usesAlphaV25()) {
                s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &load) {
                    const auto source = s.cards.find(load.source);
                    const bool endExpiry = catalog_.usesAlphaV252() && source != s.cards.end() && source->second.definition == "thunder-summon";
                    return load.owner == s.active && !load.independent && !load.sourceBound && !endExpiry;
                }), s.temporary.end());
                s.events.push_back({"temporary_load_cleared", "抽牌阶段清除临时荷载", -1});
                if (catalog_.usesAlphaV252()) {
                    const int temporaryBefore = pl.temporaryLife;
                    ResourceRules::expireTemporaryLife(s, s.active);
                    if (catalog_.usesCardRules())
                        card_rules::expiredLife(s, catalog_, s.active, temporaryBefore - pl.temporaryLife);
                }
                else pl.temporaryLife = 0;
                s.events.push_back({"temporary_life_cleared", "抽牌阶段清除到期临时生命", -1});
            }
            int income = 0;
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) == s.active && card.zone == Zone::Analysis &&
                    catalog_.at(card.definition).type == CardType::Formation) {
                    income += Rules::income(s, catalog_, id);
                    for (const auto &[sealId, seal] : s.cards)
                        if (seal.zone == Zone::Attached && seal.host == id &&
                            std::find(catalog_.at(card.definition).traits.begin(), catalog_.at(card.definition).traits.end(), "effect_immune") == catalog_.at(card.definition).traits.end())
                            income += catalog_.at(seal.definition).incomeBonus;
                }
            Trigger trigger; trigger.owner = s.active;
            EffectResolver::apply(s, catalog_, trigger, {EffectKind::GainMana, std::max(0, income)});
            s.events.push_back({"income", "阵法收入 " + std::to_string(income)});
            ++s.phaseCursor;
            return true; // Income-triggered choices finish before attempting to draw.
        }
        if (s.phase == Phase::Draw && s.phaseCursor == 1) {
            ++s.phaseCursor;
            Trigger trigger; trigger.owner = s.active;
            if (!(s.active == s.first && pl.ownTurn == 1))
                EffectResolver::apply(s, catalog_, trigger, {EffectKind::Draw, 1});
            StateMaintenance::check(s, catalog_);
            if (s.result != -1) return false;
            return true;
        }
        if (s.phase != Phase::Prepare) {
            const int first = s.phase == Phase::Draw ? 2 : 0;
            if (s.phaseCursor < first + 2) {
                const auto next = nextFieldCard(s, catalog_, s.active, s.phaseCursor == first, s.phaseOrderCursor, true);
                if (next.id) {
                    s.phaseOrderCursor = next.key;
                    const auto &def = catalog_.at(s.cards.at(next.id).definition);
                    if (catalog_.usesAlphaV252() && s.phase == Phase::End &&
                        std::find(def.traits.begin(), def.traits.end(), "destroy_enemy_ambush_end") != def.traits.end()) {
                        std::vector<CardId> concealed;
                        for (const auto &[id, card] : s.cards)
                            if (controllerOf(card) != s.active && card.zone == Zone::Analysis && card.faceDown &&
                                catalog_.at(card.definition).type != CardType::Formation) concealed.push_back(id);
                        for (auto id : concealed) StateMaintenance::leave(s, id, &catalog_);
                    }
                    triggered(next.id, s.phase == Phase::End ?
                        (std::find(def.traits.begin(), def.traits.end(), "end_phase_start") != def.traits.end() ? def.onEnd : std::vector<Effect>{}) :
                        s.phase == Phase::Draw ? def.onDraw : s.phase == Phase::Main ? def.onMain :
                        catalog_.usesAlphaV25() && def.concentration ? std::vector<Effect>{} : def.onCast);
                    return true;
                }
                ++s.phaseCursor; s.phaseOrderCursor = 0; return true;
            }
            s.phaseStep = PhaseStep::StartWindow; return true;
        }
        if (s.phaseCursor == 0) {
            if (!catalog_.usesAlphaV25())
                s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &load) {
                    const auto source = s.cards.find(load.source);
                    const bool endExpiry = catalog_.usesAlphaV252() && source != s.cards.end() && source->second.definition == "thunder-summon";
                    return load.owner == s.active && !load.independent && !load.sourceBound && !endExpiry;
                }), s.temporary.end());
            ++s.phaseCursor; return true;
        }
        if (s.phaseCursor == 1) {
            if (!catalog_.usesAlphaV25()) pl.temporaryLife = 0;
            ++s.phaseCursor; return true;
        }
        if (s.phaseCursor == 2 || s.phaseCursor == 3) {
            auto next = nextFieldCard(s, catalog_, s.active, s.phaseCursor == 2, s.phaseOrderCursor);
            if (next.id) {
                s.phaseOrderCursor = next.key;
                const auto &effects = catalog_.at(s.cards.at(next.id).definition).onPrepare;
                if (triggered(next.id, effects)) return true;
                return true;
            }
            ++s.phaseCursor; s.phaseOrderCursor = 0; return true;
        }
        if (s.phaseCursor == 4) {
            OrderedCard next{std::numeric_limits<std::uint64_t>::max(), 0};
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) == s.active && card.zone == Zone::Analysis && card.spell == SpellState::Active &&
                    catalog_.at(card.definition).concentration && card.host) {
                    const auto key = (s.cards.at(card.host).fieldOrder << 16) +
                                     static_cast<std::uint64_t>(card.ringIndex + 1);
                    if (key > s.phaseOrderCursor && key < next.key) next = {key, id};
                }
            if (next.id) {
                s.phaseOrderCursor = next.key;
                s.maintenanceSource = next.id;
                const int fee = Rules::concentrationUpkeep(s, catalog_, next.id);
                std::vector<std::uint32_t> options;
                if (card_rules::canPay(s, s.active, fee, &catalog_)) options.push_back(next.id);
                decision(s.active, DecisionKind::Concentration, std::move(options), true);
                return false;
            }
            ++s.phaseCursor; s.phaseOrderCursor = 0; return true;
        }
        if (s.phaseCursor == 5) {
            if (catalog_.usesAlphaV252()) {
                std::vector<CardId> analyzing;
                for (const auto &[id, card] : s.cards)
                    if (controllerOf(card) == s.active && card.zone == Zone::Analysis && !card.faceDown &&
                        card.spell == SpellState::Analyzing) analyzing.push_back(id);
                for (auto id : analyzing) card_rules::advanceAnalysis(s, catalog_, id, 1);
                ++s.phaseCursor; return true;
            }
            for (auto &[id, card] : s.cards)
                if (controllerOf(card) == s.active && card.zone == Zone::Analysis && !card.faceDown &&
                    card.spell == SpellState::Analyzing &&
                    ++card.analysisProgress >= catalog_.at(card.definition).analysisTurns) {
                    card.spell = SpellState::Ready;
                    analysisCompletedV2(id);
                }
            ++s.phaseCursor; return true;
        }
        s.phaseStep = PhaseStep::StartWindow; return true;
    }
    case PhaseStep::StartWindow: {
        s.phaseStep = PhaseStep::Body;
        ChainLink root; root.kind = LinkKind::Phase; root.item.owner = s.active;
        startChain(ResponseWindow::PhaseStart, std::move(root), 1 - s.active);
        return true;
    }
    case PhaseStep::Body:
        if (s.phase == Phase::End && s.skipCastPhase) {
            s.phaseStep = PhaseStep::EndWindow;
            return true;
        }
        if (!s.phaseGate) s.phaseGate = s.nextDecision++;
        return false;
    case PhaseStep::EndWindow: {
        s.phaseStep = PhaseStep::EndTriggers;
        ChainLink root; root.kind = LinkKind::Phase; root.item.owner = s.active;
        startChain(ResponseWindow::PhaseEnd, std::move(root), 1 - s.active);
        return true;
    }
    case PhaseStep::EndTriggers:
        if (s.phase == Phase::Cast) s.returnRequested = true;
        s.phaseStep = PhaseStep::Cleanup; s.phaseCursor = 0; s.phaseOrderCursor = 0; return true;
    case PhaseStep::Cleanup: {
        if (s.phase != Phase::End) { s.phaseStep = PhaseStep::Finish; return true; }
        if (s.phaseCursor == 0 || s.phaseCursor == 1) {
            auto next = nextFieldCard(s, catalog_, s.active, s.phaseCursor == 0, s.phaseOrderCursor, true);
            if (next.id) {
                s.phaseOrderCursor = next.key;
                const auto &effects = catalog_.at(s.cards.at(next.id).definition).onEnd;
                const auto &traits = catalog_.at(s.cards.at(next.id).definition).traits;
                if (std::find(traits.begin(), traits.end(), "end_phase_start") == traits.end() && triggered(next.id, effects)) return true;
                return true;
            }
            ++s.phaseCursor; s.phaseOrderCursor = 0; return true;
        }
        if (s.phaseCursor == 2) {
            if (!s.durationApplied) {
                s.durationApplied = true;
                std::vector<CardId> expired;
                for (auto &[id, card] : s.cards) {
                    const auto &definition = catalog_.at(card.definition);
                    const bool ownerTurnTimer = std::find(definition.traits.begin(), definition.traits.end(),
                                                          "great_storm") != definition.traits.end();
                    if (card.zone == Zone::Analysis && card.spell == SpellState::Active &&
                        definition.duration > 0 && (!ownerTurnTimer || controllerOf(card) == s.active) &&
                        --card.remaining <= 0)
                        expired.push_back(id);
                }
                for (auto id : expired) StateMaintenance::leave(s, id, &catalog_);
            }
            ++s.phaseCursor; return true;
        }
        if (s.phaseCursor == 3) {
            for (const auto &[id, card] : s.cards)
                if (card.zone == Zone::Casting) { StateMaintenance::leave(s, id, &catalog_); return true; }
            ++s.phaseCursor; return true;
        }
        if (s.phaseCursor == 4) {
            auto hand = inZone(s, s.active, Zone::Hand);
            if (hand.size() > 8) { decision(s.active, DecisionKind::Discard, std::move(hand)); return false; }
            ++s.phaseCursor; return true;
        }
        if (catalog_.usesAlphaV252())
            s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &load) {
                const auto source = s.cards.find(load.source);
                return load.owner == s.active && !load.independent && !load.sourceBound &&
                    source != s.cards.end() && source->second.definition == "thunder-summon";
            }), s.temporary.end());
        StateMaintenance::check(s, catalog_, true);
        if (s.result != -1) return false;
        s.phaseStep = PhaseStep::Finish; return true;
    }
    case PhaseStep::Finish:
        s.events.push_back({"phase_end", phaseName(s.phase) + "阶段结束"});
        if (s.phase == Phase::End) {
            for (auto &player : s.players) player.damageReduction = player.forceDamageReduction = 0;
            s.skipCastPhase = false;
            pl.actionsPlayed = 0; pl.formations = 0; pl.sealRemovals = 0;
            for (auto &[id, card] : s.cards)
                if (controllerOf(card) == s.active) card.canceledTurn = -1;
            ++s.globalTurn; s.active = 1 - s.active; ++s.players[s.active].ownTurn;
            s.phase = Phase::Draw;
        } else s.phase = s.phase == Phase::Main && s.skipCastPhase ? Phase::End :
            static_cast<Phase>(static_cast<int>(s.phase) + 1);
        s.phaseStep = PhaseStep::Enter; s.phaseGate = 0; return true;
    default:
        s.phaseStep = PhaseStep::StartWindow; return true;
    }
}
}
