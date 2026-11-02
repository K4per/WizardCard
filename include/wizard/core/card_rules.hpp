#pragma once
#include "wizard/core/rules.hpp"

namespace wizard::card_rules {
bool member(const CardDefinition &, const char *series);
// Legacy spell/recipe aliases are kept inline while callers migrate to the
// neutral names below; no series-specific implementation remains behind them.
// The one-argument form identifies executable cards in the series. Formation
// cards are hosts/materials and are intentionally excluded from that pool
// count; callers that need the raw series tag use the explicit overload.
inline bool member(const CardDefinition &d) {
    return d.type != CardType::Formation && member(d, "味契");
}
bool isSpell(const CardDefinition &);
inline bool spell(const CardDefinition &d) { return isSpell(d); }
bool hasActiveTrait(const GameState &, const CardCatalog &, Capability, PlayerId owner = -1);
inline bool active(const GameState &s, const CardCatalog &c, Capability capability, PlayerId p = -1) {
    return hasActiveTrait(s, c, capability, p);
}
bool field(const CardInstance &);
int lifeLimit(const GameState &, const CardCatalog &, PlayerId);
void normalizeLife(MutationContext &, const CardCatalog &, PlayerId);
void changeLifeLimit(MutationContext &, const CardCatalog &, PlayerId, int);
bool canPayLife(const GameState &, PlayerId, int);
int activationLifeCost(const CardDefinition &);
int attachmentLifeCost(const CardDefinition &);
void payLife(MutationContext &, const CardCatalog &, PlayerId, int, CardId = 0);
int heal(MutationContext &, const CardCatalog &, PlayerId, int, CardId = 0);
int paymentRatio(const GameState &, const CardCatalog &, PlayerId);
std::vector<PaymentChoice> paymentChoices(const GameState &, const CardCatalog &, PlayerId, int);
bool canPay(const GameState &, const CardCatalog &, PlayerId, int, const PaymentChoice * = nullptr);
inline bool canPay(const MutationContext &s, const CardCatalog &cat, PlayerId p, int fee) {
    return canPay(static_cast<const GameState &>(s), cat, p, fee, s.paymentInput ? &*s.paymentInput : nullptr);
}
void pay(MutationContext &, const CardCatalog &, PlayerId, int);
bool canPay(const GameState &, PlayerId, int, const CardCatalog * = nullptr);
inline bool canPay(const MutationContext &s, PlayerId p, int fee, const CardCatalog *cat) {
    return cat ? canPay(s, *cat, p, fee) : canPay(static_cast<const GameState &>(s), p, fee, cat);
}
void pay(MutationContext &, PlayerId, int, const CardCatalog * = nullptr);
bool damageImmune(const GameState &, const CardCatalog &, PlayerId);
void damaged(MutationContext &, const CardCatalog &, PlayerId, CardId, int);
void expiredLife(MutationContext &, const CardCatalog &, PlayerId, int);
bool effectImmune(const GameState &, const CardCatalog &, CardId);
int aspectMode(const GameState &, const CardCatalog &, PlayerId);
void queueFormationSetup(MutationContext &, const CardCatalog &, CardId);
bool validLinkedResponse(const GameState &, const CardCatalog &, PlayerId, LinkId);
std::vector<CardId> formationUpgradeSources(const GameState &, const CardCatalog &, PlayerId, bool = false);
void returnCard(MutationContext &, const CardCatalog &, CardId, bool hand);
std::string activationError(const GameState &, const CardCatalog &, PlayerId, CardId);
std::string activationError(const GameState &, const CardCatalog &, PlayerId, CardId, CardId target);
std::vector<CardId> costOptions(const GameState &, const CardCatalog &, PlayerId, CardId);
bool validCost(const GameState &, const CardCatalog &, PlayerId, CardId, CardId);
void payCost(MutationContext &, const CardCatalog &, PlayerId, CardId, CardId);
void eraseCard(MutationContext &, const CardCatalog &, CardId);
void advanceAnalysis(MutationContext &, const CardCatalog &, CardId, int);
void onAshEntered(MutationContext &, const CardCatalog &, PlayerId owner = -1);
void onFireDamage(MutationContext &, const CardCatalog &);
void onManaSpent(MutationContext &, const CardCatalog &, PlayerId, int);
inline void advance(MutationContext &s, const CardCatalog &c, CardId id, int amount) {
    advanceAnalysis(s, c, id, amount);
}
inline void ashEntered(MutationContext &s, const CardCatalog &c, PlayerId owner = -1) {
    onAshEntered(s, c, owner);
}
inline void fireDamage(MutationContext &s, const CardCatalog &c) { onFireDamage(s, c); }
inline void manaSpent(MutationContext &s, const CardCatalog &c, PlayerId p, int amount) {
    onManaSpent(s, c, p, amount);
}
bool hasFormation(const GameState &, const CardCatalog &, PlayerId, Capability);
bool isFireSpell(const CardDefinition &);
inline bool fireSpell(const CardDefinition &d) { return isFireSpell(d); }
bool activationAllowed(const GameState &, const CardCatalog &, PlayerId, const CardDefinition &);
bool protectedFormation(const GameState &, const CardCatalog &, CardId);
inline bool protectedCampfire(const GameState &s, const CardCatalog &cat, CardId id) { return protectedFormation(s, cat, id); }
int attachedCount(const GameState &, const CardCatalog &, PlayerId, Capability);
int detachCost(const CardDefinition &);
bool validDetach(const GameState &, const CardCatalog &, PlayerId, const CardDefinition &,
                 const std::vector<CardId> &);
std::vector<std::vector<CardId>> detachOptions(const GameState &, const CardCatalog &, PlayerId,
                                              const CardDefinition &);
void attached(MutationContext &, const CardCatalog &, CardId);
bool threatensFormation(const GameState &, const CardCatalog &, const ChainLink &, PlayerId);
inline void erase(MutationContext &s, const CardCatalog &c, CardId id) { eraseCard(s, c, id); }
bool validExtraCost(const GameState &, const CardCatalog &, PlayerId, CardId, CardId);
void payExtraCost(MutationContext &, const CardCatalog &, PlayerId, CardId, CardId);
inline void payCost(MutationContext &s, const CardCatalog &c, PlayerId p, CardId id, CardId material) {
    payExtraCost(s, c, p, id, material);
}
}
