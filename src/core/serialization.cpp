#include "engine_internal.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wizard {
std::vector<std::vector<Effect>> GameEngine::effectSequences() const {
    std::vector<std::vector<Effect>> result;
    const auto append = [&](const Trigger &trigger) { result.push_back(trigger.effects); };
    for (const auto &trigger : state_.queue.items) append(trigger);
    for (const auto &link : state_.deferredPreparations) append(link.item);
    if (state_.effect) append(state_.effect->item);
    if (state_.chain) for (const auto &link : state_.chain->links) append(link.item);
    return result;
}
std::string GameEngine::stateKey(bool logical, bool compactEffects, bool currentFormat) const {
    const auto &s = state_;
    std::ostringstream o;
    o << (currentFormat ? "wizardcard-state-v6:" : "alpha-v2-final:")
          << s.players[0].exhaustion << ',' << s.players[1].exhaustion << ';';
    o << s.active << ',' << s.first << ',' << static_cast<int>(s.phase) << ','
      << static_cast<int>(s.phaseStep) << ',' << static_cast<int>(s.flow) << ',' << s.result << ',' << s.rng
      << ',' << s.durationApplied << ',' << (logical ? s.globalTurn % 2 : s.globalTurn) << ';';
    const auto legacyPhaseCursor = std::visit([&](const auto &resume) {
        using T = std::decay_t<decltype(resume)>;
        const auto stage = static_cast<int>(resume.stage);
        if constexpr (std::is_same_v<T, PhaseCleanupContinuation>) return stage;
        else if (s.phase == Phase::Prepare) return stage;
        else if (s.phase == Phase::Draw) return std::min(4, stage);
        else return std::min(2, std::max(0, stage - 2));
    }, s.phaseContinuation);
    const auto legacyPhaseOrder = std::visit([](const auto &resume) { return resume.after; }, s.phaseContinuation);
    o << "v2:" << legacyPhaseCursor << ',' << s.maintenanceCard() << ',' << legacyPhaseOrder << ','
          << s.returnRequested << ',' << s.timingPending << ',' << s.nextFieldOrder << ';';
    if (compactEffects || currentFormat) {
        o << "typed-phase:" << s.phaseContinuation.index() << ':';
        std::visit([&](const auto &resume) { o << static_cast<int>(resume.stage) << ',' << resume.after; }, s.phaseContinuation);
        o << ";typed-maintenance:" << s.maintenance.index() << ',' << s.maintenanceCard() << ';';
        if (s.effect) {
            const auto encodeEffectContinuation = [&](const EffectContinuation &continuation) {
                o << continuation.index() << ':';
                std::visit([&](const auto &value) {
                    using T = std::decay_t<decltype(value)>;
                    if constexpr (std::is_same_v<T, RepeatedChoiceContinuation>) o << value.remaining << ',' << value.completed;
                    else if constexpr (std::is_same_v<T, EntityChoiceContinuation>)
                        o << value.entity.has_value() << ',' << value.entity.value_or(0) << ',' << value.completed << ',' << value.inspected;
                    else if constexpr (std::is_same_v<T, GuidanceContinuation>) o << static_cast<int>(value.stage);
                    else if constexpr (std::is_same_v<T, TargetWalkContinuation>)
                        o << value.next.has_value() << ',' << value.next.value_or(0) << ',' << value.destroyed;
                    else if constexpr (std::is_same_v<T, PlayerWalkContinuation> || std::is_same_v<T, CatalogWalkContinuation>)
                        o << value.next.has_value() << ',' << value.next.value_or(0);
                    else if constexpr (std::is_same_v<T, OnceContinuation>) o << value.performed;
                    else o << value.offerDamage << ',' << value.destroyed;
                }, continuation);
            };
            o << "typed-effect:" << s.effect->continuationCursor << ',' << static_cast<int>(s.effect->continuationKind) << ',';
            encodeEffectContinuation(s.effect->continuation);
            if (s.effect->suspendedContinuation) {
                o << "suspended:" << s.effect->suspendedContinuation->first << ',';
                encodeEffectContinuation(s.effect->suspendedContinuation->second);
            }
            o << ';';
        }
    }
    o << "opening:" << s.opening << ',' << s.initialFormations[0] << ',' << s.initialFormations[1] << ';';
    if (s.skipCastPhase) o << "skip_cast;";
    if (s.debugUsed) o << "debug_used;";
    if (!logical)
        o << s.nextLoad << ',' << s.nextTrigger << ',' << s.nextBatch << ',' << s.nextDecision << ','
          << s.nextChain << ',' << s.nextLink << ',' << s.phaseGate << ';';
    auto linkId = [&](LinkId id) {
        if (!logical || !id)
            return id;
        if (s.chain)
            for (std::size_t i = 0; i < s.chain->links.size(); ++i)
                if (s.chain->links[i].id == id)
                    return static_cast<LinkId>(i + 1);
        return LinkId{0};
    };
    for (const auto &p : s.players) {
        o << "v254:" << p.lifeLimit << ',' << p.firstLifePaymentTurn << ',' << p.sevenShifted << ',' << p.sevenShiftUntilOwnTurn << ';';
        if (p.debugInfiniteLife || p.debugInfiniteMana || p.debugInfiniteLoad)
            o << "debug:" << p.debugInfiniteLife << ',' << p.debugInfiniteMana << ',' << p.debugInfiniteLoad << ';';
        o << p.life << ',' << p.mana << ',' << p.ownTurn << ',' << p.actionsPlayed << ',' << p.formations
          << ',' << p.sealRemovals << ',' << p.drawFailed << ',' << p.surrendered << ',' << p.temporaryLife
          << ',' << p.blockedActionTurn << ':';
        for (auto id : p.deck)
            o << id << ',';
        o << ';';
        o << "exhaustion:" << p.exhaustion << ';';
        if (p.damageReduction || p.forceDamageReduction) o << "shield:" << p.damageReduction << ',' << p.forceDamageReduction << ';';
        if (p.nextDamageImmune) o << "next_damage_immune;";
        if (p.wordLockUntilOwnDraw) o << "word_lock:" << p.wordLockUntilOwnDraw << ';';
        if (p.siqueDamageCap || p.siqueLifePaymentTurn)
            o << "sique:" << p.siqueDamageCap << ',' << p.siqueCapUntilOwnTurn << ','
              << (logical ? p.siqueLifePaymentTurn == s.globalTurn : p.siqueLifePaymentTurn) << ';';
    }
    for (const auto &[id, c] : s.cards) {
        o << id << ',' << std::quoted(c.definition) << ',' << c.owner << ',';
        o << controllerOf(c) << ',';
        o << static_cast<int>(c.zone)
          << ',' << static_cast<int>(c.spell) << ',' << c.host << ',' << c.sourceFormation << ','
          << c.targetCard << ',' << c.targetPlayer << ',' << c.base << ',' << c.analysisStarted << ','
          << c.remaining << ',' << c.analysisLoad << ',' << c.castLoad << ',' << c.canceledTurn << ','
          << c.settingPaid << ',' << c.concentrationCost << ',' << c.faceDown << ','
          << (logical ? (c.faceDown && c.ambushedTurn == s.globalTurn) : c.ambushedTurn) << ','
          << c.quickAnalysisTurn;
        o << ',' << c.stormAbilityTurn;
        o << ':';
        o << "v2:" << c.analysisProgress << ',' << c.fieldOrder << ',' << c.ringIndex << ','
              << c.wasAmbushed << ',' << c.sourceRingIndex << ':';
        if (c.publiclyRevealed) o << "shown:";
        if (c.siquePartner) o << "pair:" << c.siquePartner << ','
            << (logical ? c.siquePairTurn == s.globalTurn : c.siquePairTurn) << ':';
        for (const auto &pay : c.payments)
            o << pay.turn << ',' << pay.paid << ',' << pay.load << ',' << pay.canceled << '/';
        o << ':';
        for (auto target : c.targets)
            o << target << ',';
        if ((c.stormPoints != 0 || c.stormReleaseProgress != 0))
            o << "storm:" << c.stormPoints << ',' << c.stormReleaseProgress << ';';
        o << ';';
    }
    for (const auto &t : s.temporary)
        o << 'L' << (logical ? 0 : t.id) << ',' << t.owner << ',' << t.source << ',' << t.amount << ','
          << t.expiryTurn << ',' << t.independent << ',' << t.sourceBound << ';';
    for (const auto &t : s.temporaryLives)
        o << 'H' << (logical ? 0 : t.id) << ',' << t.owner << ',' << t.source << ',' << t.amount << ','
          << t.expiryOwnTurn << ',' << (logical ? 0 : t.acquiredTurn) << ',' << static_cast<int>(t.acquiredPhase) << ';';
    std::map<std::uint32_t, std::uint32_t> batches;
    std::uint32_t nextBatch = 1;
    auto trigger = [&](const Trigger &t) {
        auto batch = t.batch;
        if (logical) {
            auto [it, inserted] = batches.emplace(batch, nextBatch);
            if (inserted)
                ++nextBatch;
            batch = it->second;
        }
        o << (logical ? 0 : t.id) << ',' << batch << ',' << t.owner << ',' << t.source << ',' << t.targetCard
          << ',' << t.targetPlayer << ',' << static_cast<int>(t.target) << ',' << t.requiresSource << ','
          << linkId(t.targetLink) << ':';
        if (currentFormat) o << "targets:" << t.targetCards.size() << ':';
        for (auto id : t.targetCards) o << id << ',';
        o << ':';
        if (compactEffects) o << "effects-hash:" << t.effectsKey() << ',' << t.effects.size();
        else {
            if (currentFormat) o << "effects:" << t.effects.size() << ':';
            for (const auto &e : t.effects)
                o << static_cast<int>(e.kind) << ',' << e.amount << ',' << static_cast<int>(e.damageType) << ','
                  << static_cast<int>(e.recipient) << '/';
        }
        o << ';';
    };
    for (const auto &t : s.queue.items) {
        o << 'Q';
        trigger(t);
    }
    for (const auto &l : s.deferredPreparations) {
        o << 'P' << l.sourceLost << ',' << l.paymentIndex << ',' << l.preparationLoad << ',' << l.paid << ','
          << l.addedLoad << ',' << l.discarded << ',' << l.speed << ',' << l.castCost << ';';
        trigger(l.item);
    }
    if (s.effect) {
        o << 'E' << s.effect->cursor << ',' << static_cast<int>(s.effect->after) << ','
          << s.effect->legacyStage();
        o << ',' << s.effect->legacyCount();
        o << ',' << linkId(s.effect->link) << ';';
        trigger(s.effect->item);
    }
    if (s.chain) {
        const auto &c = *s.chain;
        o << 'C' << (logical ? 0 : c.id) << ',' << static_cast<int>(c.window) << ','
          << static_cast<int>(c.mode) << ',' << c.initiator << ',' << c.priority << ',' << c.passes << ';';
        o << "actual:" << c.hadActualLinks << ';';
        for (const auto &l : c.links) {
            o << 'N' << linkId(l.id) << ',' << static_cast<int>(l.kind) << ',' << l.canceled << ','
              << l.sourceLost << ',' << std::quoted(l.ability) << ',' << l.paymentIndex << ','
              << l.preparationLoad << ',' << l.paid << ',' << l.addedLoad << ',' << l.discarded << ','
              << l.speed << ',' << l.castCost << ';';
            trigger(l.item);
        }
        o << 'R';
        for (auto id : c.responded)
            o << id << ',';
    }
    if (s.decision) {
        const auto &d = *s.decision;
        o << 'D' << d.id << ',' << d.player << ',' << static_cast<int>(d.kind) << ',' << d.mayPass << ':';
        for (auto option : d.options)
            if (currentFormat) o << detail::choiceKey(option).size() << ':' << detail::choiceKey(option) << ',';
            else o << detail::legacyChoice(option, d.kind) << ',';
    }
    if (s.payment) {
        o << "payment:" << s.payment->player << ',' << s.payment->key << ':';
        for (const auto &choice : s.payment->choices) o << choice.mana << ',' << choice.life << ',' << choice.lifePlayer << ';';
        if (s.payment->previousDecision) o << "previous:" << s.payment->previousDecision->id;
    }
    return o.str();
}
std::string GameEngine::canonicalState() const {
    return stateKey(false, false, true);
}
std::string GameEngine::cycleKey() const {
    return stateKey(true, true, true);
}
std::string GameEngine::legacyDigest() const {
    return fingerprint(stateKey(false));
}
std::string GameEngine::digest() const {
    std::ostringstream out;
    const auto state = canonicalState();
    out << "wizardcard-digest-v2:" << state.size() << ':' << state;
    if (state_.payment && state_.payment->previousDecision) {
        const auto &d = *state_.payment->previousDecision;
        out << "previous_decision:" << d.id << ',' << d.player << ',' << static_cast<int>(d.kind) << ',' << d.mayPass << ':';
        for (auto option : d.options) out << detail::choiceKey(option) << ',';
    }
    return fingerprint(out.str());
}
} // namespace wizard
