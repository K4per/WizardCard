#include "wizard/core/engine.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wizard {
std::string GameEngine::stateKey(bool logical) const {
    const auto &s = state_;
    std::ostringstream o;
    if (catalog_.alphaV2Draft)
        o << "alpha-v2-resource-draft:" << s.players[0].exhaustion << ',' << s.players[1].exhaustion << ';';
    o << s.active << ',' << s.first << ',' << static_cast<int>(s.phase) << ','
      << static_cast<int>(s.phaseStep) << ',' << static_cast<int>(s.flow) << ',' << s.result << ',' << s.rng
      << ',' << s.durationApplied << ',' << (logical ? s.globalTurn % 2 : s.globalTurn) << ';';
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
        o << p.life << ',' << p.mana << ',' << p.ownTurn << ',' << p.actionsPlayed << ',' << p.formations
          << ',' << p.sealRemovals << ',' << p.drawFailed << ',' << p.surrendered << ',' << p.temporaryLife
          << ',' << p.blockedActionTurn << ':';
        for (auto id : p.deck)
            o << id << ',';
        o << ';';
    }
    for (const auto &[id, c] : s.cards) {
        o << id << ',' << std::quoted(c.definition) << ',' << c.owner << ',' << static_cast<int>(c.zone)
          << ',' << static_cast<int>(c.spell) << ',' << c.host << ',' << c.sourceFormation << ','
          << c.targetCard << ',' << c.targetPlayer << ',' << c.base << ',' << c.analysisStarted << ','
          << c.remaining << ',' << c.analysisLoad << ',' << c.castLoad << ',' << c.canceledTurn << ','
          << c.settingPaid << ',' << c.concentrationCost << ',' << c.faceDown << ','
          << (logical ? (c.faceDown && c.ambushedTurn == s.globalTurn) : c.ambushedTurn) << ','
          << c.quickAnalysisTurn << ':';
        for (const auto &pay : c.payments)
            o << pay.turn << ',' << pay.paid << ',' << pay.load << ',' << pay.canceled << '/';
        o << ':';
        for (auto target : c.targets)
            o << target << ',';
        o << ';';
    }
    for (const auto &t : s.temporary)
        o << 'L' << (logical ? 0 : t.id) << ',' << t.owner << ',' << t.source << ',' << t.amount << ','
          << t.expiryTurn << ',' << t.independent << ',' << t.sourceBound << ';';
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
        for (auto id : t.targetCards)
            o << id << ',';
        o << ':';
        for (const auto &e : t.effects)
            o << static_cast<int>(e.kind) << ',' << e.amount << ',' << static_cast<int>(e.damageType) << ','
              << static_cast<int>(e.recipient) << '/';
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
          << s.effect->clearRemaining << ',' << linkId(s.effect->link) << ';';
        trigger(s.effect->item);
    }
    if (s.chain) {
        const auto &c = *s.chain;
        o << 'C' << (logical ? 0 : c.id) << ',' << static_cast<int>(c.window) << ','
          << static_cast<int>(c.mode) << ',' << c.initiator << ',' << c.priority << ',' << c.passes << ';';
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
        for (auto id : d.options)
            o << id << ',';
    }
    return o.str();
}
std::string GameEngine::canonicalState() const {
    return stateKey(false);
}
std::string GameEngine::cycleKey() const {
    return stateKey(true);
}
std::string GameEngine::digest() const {
    return fingerprint(canonicalState());
}
} // namespace wizard
