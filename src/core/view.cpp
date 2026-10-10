#include "wizard/core/engine.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wizard {
GameView GameEngine::viewFor(PlayerId p) const {
    if (p < 0 || p > 1)
        throw std::out_of_range("invalid viewer");
    GameView v;
    v.viewer = p;
    v.active = state_.active;
    v.phase = state_.phase;
    v.result = state_.result;
    v.phaseStep = state_.phaseStep;
    v.phaseGate = state_.phaseGate;
    v.chain = state_.chain;
    for (int i = 0; i < 2; ++i) {
        const auto &pl = state_.players[i];
        int hand = 0;
        for (const auto &[id, c] : state_.cards)
            if (c.owner == i && c.zone == Zone::Hand)
                ++hand;
        v.players[i] = {pl.life,
                        pl.mana,
                        Rules::load(state_, i),
                        Rules::capacity(state_, catalog_, i),
                        hand,
                        static_cast<int>(pl.deck.size()),
                        pl.ownTurn,
                        pl.actionsPlayed,
                        pl.temporaryLife,
                        pl.blockedActionTurn};
        for (const auto &[id, c] : state_.cards)
            if (c.owner == i && c.zone == Zone::Casting && c.spell == SpellState::Active) {
                const auto &d = catalog_.at(c.definition);
                v.players[i].resistances.insert(v.players[i].resistances.end(), d.resistances.begin(),
                                                d.resistances.end());
                v.players[i].immunities.insert(v.players[i].immunities.end(), d.immunities.begin(),
                                               d.immunities.end());
            }
    }
    for (const auto &[id, c] : state_.cards)
        if ((c.zone != Zone::Deck ||
             (state_.decision && state_.decision->player == p &&
              state_.decision->kind == DecisionKind::SearchDeck &&
              std::find(state_.decision->options.begin(), state_.decision->options.end(), id) !=
                  state_.decision->options.end())) &&
            (c.zone != Zone::Hand || c.owner == p)) {
            const auto &def = catalog_.at(c.definition);
            CardView card{c, def};
            card.effectiveCastCost = Rules::castCost(state_, catalog_, id);
            if (c.faceDown && c.owner != p) {
                card.hidden = true;
                card.instance = {};
                card.instance.id = id;
                card.instance.owner = c.owner;
                card.instance.zone = c.zone;
                card.instance.host = c.host;
                card.instance.faceDown = true;
                card.definition = {};
                card.definition.name = "埋伏卡";
                card.effectiveCastCost = 0;
                v.cards.push_back(std::move(card));
                continue;
            }
            if (def.type == CardType::Formation && c.zone == Zone::Analysis) {
                card.effectiveRings = Rules::rings(state_, catalog_, id);
                card.occupiedRings = Rules::occupied(state_, id);
            }
            if (c.spell == SpellState::Analyzing)
                card.turnsToReady =
                    std::max(0, def.analysisTurns - (state_.players[c.owner].ownTurn - c.analysisStarted));
            v.cards.push_back(std::move(card));
        }
    if (state_.decision && state_.decision->player == p)
        v.decision = state_.decision;
    for (const auto &e : state_.events)
        if (e.audience == -1 || e.audience == p)
            v.events.push_back(e);
    v.temporary = state_.temporary;
    v.actions = legalActions(p);
    if (v.decision && v.decision->kind == DecisionKind::TriggerOrder)
        for (const auto &t : state_.queue.items)
            if (std::find(v.decision->options.begin(), v.decision->options.end(), t.id) !=
                    v.decision->options.end() &&
                std::any_of(v.cards.begin(), v.cards.end(),
                            [&](const CardView &c) { return c.instance.id == t.source; }))
                v.triggers.push_back(t);
    return v;
}
} // namespace wizard
