#pragma once
#include "wizard/core/engine.hpp"
#include <algorithm>
namespace wizard::detail {
inline bool stormTags(const std::vector<std::string> &tags) {
    return std::find(tags.begin(), tags.end(), "风契") != tags.end() ||
           std::find(tags.begin(), tags.end(), "风暴") != tags.end();
}
std::uint32_t random32(GameState &);
std::uint32_t bounded(GameState &, std::uint32_t);
std::vector<CardId> inZone(const GameState &, PlayerId, Zone);
int seals(const GameState &, CardId);
int usedBody(const GameState &, const CardCatalog &, PlayerId);
bool hasOverflow(const GameState &, const CardCatalog &);
bool cardTarget(TargetKind);
std::vector<CardId> targetCards(CardId, const std::vector<CardId> &);
bool validTargets(const GameState &, TargetKind, int, PlayerId, CardId, const std::vector<CardId> &);
std::vector<std::vector<CardId>> sets(const GameState &, TargetKind, int, PlayerId);
const ResponseAbility *ability(const CardDefinition &, const std::string &);
MatchConfig shared(std::array<std::vector<std::string>, 2>, std::uint32_t);
Trigger snapshot(const CardInstance &, const CardDefinition &);
inline bool spellSpeedAllowed(const GameState &state, const CardDefinition &definition, bool setting = false) {
    if (state.chain) {
        const int top = state.chain->links.back().speed;
        return definition.speed >= 2 && top != 4 && definition.speed >= top &&
            (definition.speed != 2 || state.phase == Phase::Main || state.phase == Phase::Cast);
    }
    return definition.speed >= 3 ||
        (definition.speed == 2 && (state.phase == Phase::Main || state.phase == Phase::Cast)) ||
        (definition.speed == 1 && state.phase == (setting ? Phase::Main : Phase::Cast));
}
inline bool hasStormFormation(const GameState &state, const CardCatalog &catalog, PlayerId owner) {
    return std::any_of(state.cards.begin(), state.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        if (card.owner != owner || card.zone != Zone::Analysis || card.faceDown ||
            catalog.at(card.definition).type != CardType::Formation)
            return false;
        const auto &tags = catalog.at(card.definition).tags;
        return detail::stormTags(tags);
    });
}
inline bool greatStormLocksAnalysis(const GameState &state, const CardCatalog &, PlayerId owner) {
    return std::any_of(state.cards.begin(), state.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        return card.owner == owner && card.definition == "great-storm" && card.zone == Zone::Analysis;
    });
}
}
