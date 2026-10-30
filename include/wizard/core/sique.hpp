#pragma once

#include "wizard/core/card_rules.hpp"

// Compatibility surface for clients that still include the historical Sique
// header. Implementations live in card_rules; new code should include that
// header directly. Queries take the immutable catalog and typed capability so
// this wrapper cannot silently infer behavior from a card id string.
namespace wizard::sique {
inline bool member(const CardDefinition &d) { return card_rules::member(d, "味契"); }
inline bool spell(const CardDefinition &d) { return card_rules::isSpell(d); }
inline bool hasFormation(const GameState &s, const CardCatalog &cat, PlayerId p, Capability capability) {
    return card_rules::hasFormation(s, cat, p, capability);
}
inline bool fireSpell(const CardDefinition &d) { return card_rules::isFireSpell(d); }
inline bool activationAllowed(const GameState &s, const CardCatalog &cat, PlayerId p, const CardDefinition &d) {
    return card_rules::activationAllowed(s, cat, p, d);
}
inline bool protectedCampfire(const GameState &s, const CardCatalog &cat, CardId id) {
    return card_rules::protectedFormation(s, cat, id);
}
inline int attachedCount(const GameState &s, const CardCatalog &cat, PlayerId p, Capability capability) {
    return card_rules::attachedCount(s, cat, p, capability);
}
inline int detachCost(const CardDefinition &d) { return card_rules::detachCost(d); }
inline bool validDetach(const GameState &s, const CardCatalog &cat, PlayerId p, const CardDefinition &d,
                        const std::vector<CardId> &ids) {
    return card_rules::validDetach(s, cat, p, d, ids);
}
inline std::vector<std::vector<CardId>> detachOptions(const GameState &s, const CardCatalog &cat, PlayerId p,
                                                       const CardDefinition &d) {
    return card_rules::detachOptions(s, cat, p, d);
}
inline bool canPay(const GameState &s, PlayerId p, int amount, const CardCatalog *cat = nullptr) {
    return card_rules::canPay(s, p, amount, cat);
}
inline void pay(MutationContext &s, PlayerId p, int amount, const CardCatalog *cat = nullptr) {
    card_rules::pay(s, p, amount, cat);
}
inline void attached(MutationContext &s, const CardCatalog &cat, CardId id) { card_rules::attached(s, cat, id); }
inline bool threatensFormation(const GameState &s, const CardCatalog &cat, const ChainLink &link, PlayerId p) {
    return card_rules::threatensFormation(s, cat, link, p);
}
} // namespace wizard::sique
