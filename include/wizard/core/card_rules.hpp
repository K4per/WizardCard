#pragma once
#include "wizard/core/rules.hpp"

namespace wizard::card_rules {
bool member(const CardDefinition &, const char *series);
// Legacy spell/recipe aliases are kept inline while callers migrate to the
// neutral names below; no series-specific implementation remains behind them.
inline bool member(const CardDefinition &d) { return member(d, "味契"); }
bool isSpell(const CardDefinition &);
inline bool spell(const CardDefinition &d) { return isSpell(d); }
bool hasActiveTrait(const GameState &, const CardCatalog &, const char *trait, PlayerId owner = -1);
inline bool active(const GameState &s, const CardCatalog &c, const char *t, PlayerId p = -1) {
    return hasActiveTrait(s, c, t, p);
}
bool field(const CardInstance &);
int lifeLimit(const GameState &, const CardCatalog &, PlayerId);
void normalizeLife(GameState &, const CardCatalog &, PlayerId);
void changeLifeLimit(GameState &, const CardCatalog &, PlayerId, int);
bool canPayLife(const GameState &, PlayerId, int);
void payLife(GameState &, const CardCatalog &, PlayerId, int, CardId = 0);
int heal(GameState &, const CardCatalog &, PlayerId, int, CardId = 0);
int paymentRatio(const GameState &, const CardCatalog &, PlayerId);
std::vector<PaymentChoice> paymentChoices(const GameState &, const CardCatalog &, PlayerId, int);
bool canPay(const GameState &, const CardCatalog &, PlayerId, int);
void pay(GameState &, const CardCatalog &, PlayerId, int);
bool canPay(const GameState &, PlayerId, int, const CardCatalog * = nullptr);
void pay(GameState &, PlayerId, int, const CardCatalog * = nullptr);
bool damageImmune(const GameState &, const CardCatalog &, PlayerId);
void damaged(GameState &, const CardCatalog &, PlayerId, CardId, int);
void expiredLife(GameState &, const CardCatalog &, PlayerId, int);
bool effectImmune(const GameState &, const CardCatalog &, CardId);
int aspectMode(const GameState &, const CardCatalog &, PlayerId);
void queueFormationSetup(GameState &, const CardCatalog &, CardId);
bool validLinkedResponse(const GameState &, const CardCatalog &, PlayerId, LinkId);
std::vector<CardId> formationUpgradeSources(const GameState &, const CardCatalog &, PlayerId, bool = false);
void returnCard(GameState &, const CardCatalog &, CardId, bool hand);
std::string activationError(const GameState &, const CardCatalog &, PlayerId, CardId);
std::string activationError(const GameState &, const CardCatalog &, PlayerId, CardId, CardId target);
std::vector<CardId> costOptions(const GameState &, const CardCatalog &, PlayerId, CardId);
bool validCost(const GameState &, const CardCatalog &, PlayerId, CardId, CardId);
void payCost(GameState &, const CardCatalog &, PlayerId, CardId, CardId);
void eraseCard(GameState &, const CardCatalog &, CardId);
void advanceAnalysis(GameState &, const CardCatalog &, CardId, int);
void onAshEntered(GameState &, const CardCatalog &, PlayerId owner = -1);
void onFireDamage(GameState &, const CardCatalog &);
void onManaSpent(GameState &, const CardCatalog &, PlayerId, int);
inline void advance(GameState &s, const CardCatalog &c, CardId id, int amount) {
    advanceAnalysis(s, c, id, amount);
}
inline void ashEntered(GameState &s, const CardCatalog &c, PlayerId owner = -1) {
    onAshEntered(s, c, owner);
}
inline void fireDamage(GameState &s, const CardCatalog &c) { onFireDamage(s, c); }
inline void manaSpent(GameState &s, const CardCatalog &c, PlayerId p, int amount) {
    onManaSpent(s, c, p, amount);
}
bool hasFormation(const GameState &, PlayerId, const std::string &);
bool isFireSpell(const CardDefinition &);
inline bool fireSpell(const CardDefinition &d) { return isFireSpell(d); }
bool activationAllowed(const GameState &, const CardCatalog &, PlayerId, const CardDefinition &);
bool protectedFormation(const GameState &, CardId);
inline bool protectedCampfire(const GameState &s, CardId id) { return protectedFormation(s, id); }
int attachedCount(const GameState &, PlayerId, const std::string &);
int detachCost(const CardDefinition &);
bool validDetach(const GameState &, const CardCatalog &, PlayerId, const CardDefinition &,
                 const std::vector<CardId> &);
std::vector<std::vector<CardId>> detachOptions(const GameState &, const CardCatalog &, PlayerId,
                                              const CardDefinition &);
void attached(GameState &, const CardCatalog &, CardId);
bool threatensFormation(const GameState &, const CardCatalog &, const ChainLink &, PlayerId);
inline void erase(GameState &s, const CardCatalog &c, CardId id) { eraseCard(s, c, id); }
bool validExtraCost(const GameState &, const CardCatalog &, PlayerId, CardId, CardId);
void payExtraCost(GameState &, const CardCatalog &, PlayerId, CardId, CardId);
inline void payCost(GameState &s, const CardCatalog &c, PlayerId p, CardId id, CardId material) {
    payExtraCost(s, c, p, id, material);
}
}
