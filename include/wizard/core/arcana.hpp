#pragma once
#include "wizard/core/rules.hpp"

namespace wizard::arcana {
bool spell(const CardDefinition &);
bool active(const GameState &, const CardCatalog &, const char *trait, PlayerId owner = -1);
std::string activationError(const GameState &, const CardCatalog &, PlayerId, CardId, CardId target);
std::vector<CardId> costOptions(const GameState &, const CardCatalog &, PlayerId, CardId);
bool validCost(const GameState &, const CardCatalog &, PlayerId, CardId, CardId);
void payCost(GameState &, const CardCatalog &, PlayerId, CardId, CardId);
void erase(GameState &, const CardCatalog &, CardId);
void advance(GameState &, const CardCatalog &, CardId, int);
void ashEntered(GameState &, const CardCatalog &);
void fireDamage(GameState &, const CardCatalog &);
void manaSpent(GameState &, const CardCatalog &, PlayerId, int);
} // namespace wizard::arcana
