#include "engine_internal.hpp"
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
        const auto &definition = catalog().at(spell.definition);
        if (!(definition.perpetual || definition.concentration || definition.duration > 0)) continue;
        std::vector<CardId> options;
        for (const auto &[hostId, host] : s.cards)
            if (controllerOf(host) == controllerOf(spell) && host.zone == Zone::Analysis &&
                catalog().at(host.definition).type == CardType::Formation &&
                (!hasCapability(definition, Capability::RequiresStormHeartSetup) ||
                 hasCapability(catalog().at(host.definition), Capability::StormHeart)) &&
                Rules::canHost(s, catalog(), id, hostId) &&
                Rules::freeRing(s, catalog(), hostId) >= 0)
                options.push_back(hostId);
        if (options.empty()) {
            StateMaintenance::leave(s, id, &catalog());
            // No legal host remains, so the maintenance source has been
            // consumed and must not leak into a later decision or snapshot.
            s.maintenance = std::monostate{};
            return true;
        }
        s.maintenance = ReturnContinuation{id};
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
        auto trigger = snapshot(card, catalog().at(card.definition));
        trigger.id = s.nextTrigger++;
        trigger.batch = s.nextBatch++;
        trigger.effects = effects;
        startTrigger(std::move(trigger));
        return true;
    };
    switch (s.phaseStep) {
    case PhaseStep::Enter: {
        if (s.phase == Phase::End)
            for (auto &player : s.players) player.nextDamageImmune = false;
        if (s.phase == Phase::Prepare && pl.siqueDamageCap && pl.ownTurn >= pl.siqueCapUntilOwnTurn) {
            pl.siqueDamageCap = 0; pl.siqueCapUntilOwnTurn = 0;
        }
        if (pl.siqueLifePaymentTurn != s.globalTurn) pl.siqueLifePaymentTurn = 0;
        s.phaseGate = 0;
        s.phaseContinuation = PhaseEntryContinuation{
            s.phase == Phase::Draw || s.phase == Phase::Prepare ? PhasePreparationStage::Income
                : PhasePreparationStage::FormationTriggers, 0};
        s.durationApplied = false;
        s.events.push_back({"phase_enter", "进入" + phaseName(s.phase) + "阶段"});
        s.phaseStep = PhaseStep::PrepareIncome;
        return true;
    }
    case PhaseStep::PrepareIncome: {
        auto &resume = std::get<PhaseEntryContinuation>(s.phaseContinuation);
        if (s.phase == Phase::Draw && resume.stage == PhasePreparationStage::Income) {
            // Draw begins the owner's expiry window. Clear ordinary temporary
            // load and expire only the owner's temporary-life entries before
            // income or exhaustion can observe them.
            s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &load) {
                const auto source = s.cards.find(load.source);
                const bool endExpiry = source != s.cards.end() &&
                    hasCapability(catalog().at(source->second.definition), Capability::ThunderSummon);
                return load.owner == s.active && !load.independent && !load.sourceBound && !endExpiry;
            }), s.temporary.end());
            s.events.push_back({"temporary_load_cleared", "抽牌阶段清除临时荷载", -1});
            const int temporaryBefore = pl.temporaryLife;
            ResourceRules::expireTemporaryLife(s, s.active);
            card_rules::expiredLife(s, catalog(), s.active, temporaryBefore - pl.temporaryLife);
            s.events.push_back({"temporary_life_cleared", "抽牌阶段清除到期临时生命", -1});
            int income = 0;
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) == s.active && card.zone == Zone::Analysis &&
                    catalog().at(card.definition).type == CardType::Formation) {
                    income += Rules::income(s, catalog(), id);
                    for (const auto &[sealId, seal] : s.cards)
                        if (seal.zone == Zone::Attached && seal.host == id &&
                            !hasCapability(catalog().at(card.definition), Capability::EffectImmune))
                            income += catalog().at(seal.definition).incomeBonus;
                }
            Trigger trigger; trigger.owner = s.active;
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::GainMana, std::max(0, income)});
            s.events.push_back({"income", "阵法收入 " + std::to_string(income)});
            resume.stage = PhasePreparationStage::Draw;
            return true; // Income-triggered choices finish before attempting to draw.
        }
        if (s.phase == Phase::Draw && resume.stage == PhasePreparationStage::Draw) {
            resume.stage = PhasePreparationStage::FormationTriggers;
            Trigger trigger; trigger.owner = s.active;
            if (!(s.active == s.first && pl.ownTurn == 1))
                EffectResolver::apply(s, catalog(), trigger, {EffectKind::Draw, 1});
            StateMaintenance::check(s, catalog());
            if (s.result != -1) return false;
            return true;
        }
        if (s.phase != Phase::Prepare) {
            if (resume.stage == PhasePreparationStage::FormationTriggers || resume.stage == PhasePreparationStage::SpellTriggers) {
                const auto next = nextFieldCard(s, catalog(), s.active, resume.stage == PhasePreparationStage::FormationTriggers, resume.after, true);
                if (next.id) {
                    resume.after = next.key;
                    const auto &def = catalog().at(s.cards.at(next.id).definition);
                    if (s.phase == Phase::End && hasCapability(def, Capability::DestroyEnemyAmbushEnd)) {
                        std::vector<CardId> concealed;
                        for (const auto &[id, card] : s.cards)
                            if (controllerOf(card) != s.active && card.zone == Zone::Analysis && card.faceDown &&
                                catalog().at(card.definition).type != CardType::Formation) concealed.push_back(id);
                        for (auto id : concealed) StateMaintenance::leave(s, id, &catalog());
                    }
                    triggered(next.id, s.phase == Phase::End ?
                        (hasCapability(def, Capability::EndPhaseStart) ? def.onEnd : std::vector<Effect>{}) :
                        s.phase == Phase::Draw ? def.onDraw : s.phase == Phase::Main ? def.onMain :
                        def.concentration ? std::vector<Effect>{} : def.onCast);
                    return true;
                }
                resume.stage = resume.stage == PhasePreparationStage::FormationTriggers
                    ? PhasePreparationStage::SpellTriggers : PhasePreparationStage::Complete;
                resume.after = 0; return true;
            }
            s.phaseStep = PhaseStep::StartWindow; return true;
        }
        if (resume.stage == PhasePreparationStage::Income) {
            resume.stage = PhasePreparationStage::Draw;
            return true;
        }
        if (resume.stage == PhasePreparationStage::Draw) {
            resume.stage = PhasePreparationStage::FormationTriggers; return true;
        }
        if (resume.stage == PhasePreparationStage::FormationTriggers || resume.stage == PhasePreparationStage::SpellTriggers) {
            auto next = nextFieldCard(s, catalog(), s.active, resume.stage == PhasePreparationStage::FormationTriggers, resume.after);
            if (next.id) {
                resume.after = next.key;
                const auto &effects = catalog().at(s.cards.at(next.id).definition).onPrepare;
                if (triggered(next.id, effects)) return true;
                return true;
            }
            resume.stage = resume.stage == PhasePreparationStage::FormationTriggers
                ? PhasePreparationStage::SpellTriggers : PhasePreparationStage::Concentration;
            resume.after = 0; return true;
        }
        if (resume.stage == PhasePreparationStage::Concentration) {
            OrderedCard next{std::numeric_limits<std::uint64_t>::max(), 0};
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) == s.active && card.zone == Zone::Analysis && card.spell == SpellState::Active &&
                    catalog().at(card.definition).concentration && card.host) {
                    const auto key = (s.cards.at(card.host).fieldOrder << 16) +
                                     static_cast<std::uint64_t>(card.ringIndex + 1);
                    if (key > resume.after && key < next.key) next = {key, id};
                }
            if (next.id) {
                resume.after = next.key;
                s.maintenance = ConcentrationContinuation{next.id};
                const int fee = Rules::concentrationUpkeep(s, catalog(), next.id);
                std::vector<CardId> options;
                if (card_rules::canPay(s, s.active, fee, &catalog())) options.push_back(next.id);
                decision(s.active, DecisionKind::Concentration, std::move(options), true);
                return false;
            }
            resume.stage = PhasePreparationStage::Analysis; resume.after = 0; return true;
        }
        if (resume.stage == PhasePreparationStage::Analysis) {
                std::vector<CardId> analyzing;
                for (const auto &[id, card] : s.cards)
                    if (controllerOf(card) == s.active && card.zone == Zone::Analysis && !card.faceDown &&
                        card.spell == SpellState::Analyzing) analyzing.push_back(id);
                for (auto id : analyzing) card_rules::advanceAnalysis(s, catalog(), id, 1);
                resume.stage = PhasePreparationStage::Complete; return true;
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
        s.phaseStep = PhaseStep::Cleanup; s.phaseContinuation = PhaseCleanupContinuation{}; return true;
    case PhaseStep::Cleanup: {
        auto &resume = std::get<PhaseCleanupContinuation>(s.phaseContinuation);
        if (s.phase != Phase::End) { s.phaseStep = PhaseStep::Finish; return true; }
        if (resume.stage == PhaseCleanupStage::FormationTriggers || resume.stage == PhaseCleanupStage::SpellTriggers) {
            auto next = nextFieldCard(s, catalog(), s.active, resume.stage == PhaseCleanupStage::FormationTriggers, resume.after, true);
            if (next.id) {
                resume.after = next.key;
                const auto &effects = catalog().at(s.cards.at(next.id).definition).onEnd;
                const auto &definition = catalog().at(s.cards.at(next.id).definition);
                if (!hasCapability(definition, Capability::EndPhaseStart) && triggered(next.id, effects)) return true;
                return true;
            }
            resume.stage = resume.stage == PhaseCleanupStage::FormationTriggers
                ? PhaseCleanupStage::SpellTriggers : PhaseCleanupStage::Duration;
            resume.after = 0; return true;
        }
        if (resume.stage == PhaseCleanupStage::Duration) {
            if (!s.durationApplied) {
                s.durationApplied = true;
                std::vector<CardId> expired;
                for (auto &[id, card] : s.cards) {
                    const auto &definition = catalog().at(card.definition);
                    const bool ownerTurnTimer = hasCapability(definition, Capability::GreatStorm);
                    if (card.zone == Zone::Analysis && card.spell == SpellState::Active &&
                        definition.duration > 0 && (!ownerTurnTimer || controllerOf(card) == s.active) &&
                        --card.remaining <= 0)
                        expired.push_back(id);
                }
                for (auto id : expired) StateMaintenance::leave(s, id, &catalog());
            }
            resume.stage = PhaseCleanupStage::Casting; return true;
        }
        if (resume.stage == PhaseCleanupStage::Casting) {
            for (const auto &[id, card] : s.cards)
                if (card.zone == Zone::Casting) { StateMaintenance::leave(s, id, &catalog()); return true; }
            resume.stage = PhaseCleanupStage::Discard; return true;
        }
        if (resume.stage == PhaseCleanupStage::Discard) {
            auto hand = inZone(s, s.active, Zone::Hand);
            if (hand.size() > 8) { decision(s.active, DecisionKind::Discard, std::move(hand)); return false; }
            resume.stage = PhaseCleanupStage::Complete; return true;
        }
        s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &load) {
                const auto source = s.cards.find(load.source);
                return load.owner == s.active && !load.independent && !load.sourceBound &&
                    source != s.cards.end() &&
                    hasCapability(catalog().at(source->second.definition), Capability::ThunderSummon);
            }), s.temporary.end());
        StateMaintenance::check(s, catalog(), true);
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
