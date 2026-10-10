#pragma once
#include "wizard/core/engine.hpp"
namespace wizard::detail {
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
}
