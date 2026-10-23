#pragma once
#include "wizard/core/rules.hpp"

namespace wizard::sique {
bool member(const CardDefinition &);
bool spell(const CardDefinition &);
bool hasFormation(const GameState &, PlayerId, const std::string &);
bool fireSpell(const CardDefinition &);
bool activationAllowed(const GameState &, const CardCatalog &, PlayerId, const CardDefinition &);
bool protectedCampfire(const GameState &, CardId);
int attachedCount(const GameState &, PlayerId, const std::string &);
int detachCost(const CardDefinition &);
bool validDetach(const GameState &, const CardCatalog &, PlayerId, const CardDefinition &,
                 const std::vector<CardId> &);
std::vector<std::vector<CardId>> detachOptions(const GameState &, const CardCatalog &, PlayerId,
                                              const CardDefinition &);
bool canPay(const GameState &, PlayerId, int, const CardCatalog * = nullptr);
void pay(GameState &, PlayerId, int, const CardCatalog * = nullptr);
void attached(GameState &, const CardCatalog &, CardId);
bool threatensFormation(const GameState &, const CardCatalog &, const ChainLink &, PlayerId);
} // namespace wizard::sique
