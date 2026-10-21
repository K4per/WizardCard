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
    v.opening = state_.opening;
    v.initialFormationConfirmed = {state_.initialFormations[0] != 0, state_.initialFormations[1] != 0};
    v.initialFormation = state_.initialFormations[p];
    v.globalTurn = state_.globalTurn;
    v.temporaryLives = state_.temporaryLives;
    const bool inspectEnemyAmbush = std::any_of(state_.cards.begin(), state_.cards.end(), [&](const auto &item) {
        const auto &card = item.second;
        const auto &traits = catalog_.at(card.definition).traits;
        return card.owner == p && card.zone == Zone::Analysis && !card.faceDown &&
            std::find(traits.begin(), traits.end(), "inspect_enemy_ambush") != traits.end();
    });
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
            if (c.owner == i && (c.zone == Zone::Casting || (catalog_.usesAlphaV2() && c.zone == Zone::Analysis)) &&
                !c.faceDown && c.spell == SpellState::Active) {
                const auto &d = catalog_.at(c.definition);
                v.players[i].resistances.insert(v.players[i].resistances.end(), d.resistances.begin(),
                                                d.resistances.end());
                v.players[i].immunities.insert(v.players[i].immunities.end(), d.immunities.begin(),
                                               d.immunities.end());
            }
    }
    for (int player = 0; player < 2; ++player) {
        v.players[player].exhaustion = state_.players[player].exhaustion;
        for (const auto &[id, card] : state_.cards)
            if (card.owner == player && card.zone == Zone::SideDeck) ++v.players[player].sideDeckCount;
    }
    for (const auto &[id, c] : state_.cards)
        if (c.publiclyRevealed || ((c.zone != Zone::Deck ||
             (state_.decision && state_.decision->player == p &&
              (state_.decision->kind == DecisionKind::SearchDeck ||
               (catalog_.usesAlphaV2() && state_.decision->kind == DecisionKind::EffectChoice)) &&
              std::find(state_.decision->options.begin(), state_.decision->options.end(), id) !=
                  state_.decision->options.end())) &&
            (c.zone != Zone::Hand || c.owner == p) && (c.zone != Zone::SideDeck || c.owner == p))) {
            const auto &def = catalog_.at(c.definition);
            CardView card{c, def};
            card.effectiveCastCost = Rules::castCost(state_, catalog_, id);
            card.effectiveSettingCost = def.type == CardType::Talisman ? 0 : def.cost;
            if (def.type == CardType::Analytic) {
                // No host yet in hand: show the source's current setting cost.
                // Once placed, also include the actual host's modifiers.
                card.effectiveSettingCost = Rules::analysisCost(state_, catalog_, id,
                    c.zone == Zone::Analysis ? c.host : 0);
            }
            if (c.faceDown && c.owner != p && !c.publiclyRevealed && !(inspectEnemyAmbush && c.zone == Zone::Analysis)) {
                card.hidden = true;
                card.instance = {};
                card.instance.id = id;
                card.instance.owner = c.owner;
                card.instance.zone = c.zone;
                card.instance.host = c.host;
                card.instance.ringIndex = c.ringIndex;
                card.instance.fieldOrder = c.fieldOrder;
                card.instance.faceDown = true;
                card.definition = {};
                card.definition.name = "埋伏卡";
                card.effectiveCastCost = 0;
                card.effectiveSettingCost = 0;
                v.cards.push_back(std::move(card));
                continue;
            }
            if (def.type == CardType::Formation && c.zone == Zone::Analysis) {
                card.effectiveRings = Rules::rings(state_, catalog_, id);
                card.occupiedRings = Rules::occupied(state_, id);
                card.definition.level = Rules::level(state_, catalog_, id);
                card.definition.maxRank = Rules::maxRank(state_, catalog_, id);
                card.definition.rings = card.effectiveRings;
                if (def.id == "storm-crest") {
                    card.definition.capacity = state_.cards.at(id).stormPoints >= 5 ? 10 :
                        state_.cards.at(id).stormPoints >= 3 ? 8 : 5;
                    card.definition.income = Rules::income(state_, catalog_, id);
                }
            }
            if (c.spell == SpellState::Analyzing)
                card.turnsToReady =
                    std::max(0, def.analysisTurns - (catalog_.usesAlphaV2() ? c.analysisProgress :
                        state_.players[c.owner].ownTurn - c.analysisStarted));
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
