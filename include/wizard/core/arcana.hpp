#pragma once

#include "wizard/core/card_rules.hpp"

// Compatibility surface for clients that still include the historical Arcana
// header. Implementations live in card_rules; new code should include that
// header directly.
namespace wizard::arcana {
inline bool spell(const CardDefinition &d) { return card_rules::isSpell(d); }
inline bool active(const GameState &s, const CardCatalog &cat, Capability capability, PlayerId owner = -1) {
    return card_rules::hasActiveTrait(s, cat, capability, owner);
}
inline std::string activationError(const GameState &s, const CardCatalog &cat, PlayerId p, CardId id, CardId target) {
    return card_rules::activationError(s, cat, p, id, target);
}
inline std::vector<CardId> costOptions(const GameState &s, const CardCatalog &cat, PlayerId p, CardId id) {
    return card_rules::costOptions(s, cat, p, id);
}
inline bool validCost(const GameState &s, const CardCatalog &cat, PlayerId p, CardId id, CardId material) {
    return card_rules::validCost(s, cat, p, id, material);
}
inline void payCost(MutationContext &s, const CardCatalog &cat, PlayerId p, CardId id, CardId material) {
    card_rules::payCost(s, cat, p, id, material);
}
inline void erase(MutationContext &s, const CardCatalog &cat, CardId id) { card_rules::eraseCard(s, cat, id); }
inline void advance(MutationContext &s, const CardCatalog &cat, CardId id, int amount) {
    card_rules::advanceAnalysis(s, cat, id, amount);
}
inline void ashEntered(MutationContext &s, const CardCatalog &cat, PlayerId owner = -1) {
    card_rules::onAshEntered(s, cat, owner);
}
inline void fireDamage(MutationContext &s, const CardCatalog &cat) { card_rules::onFireDamage(s, cat); }
inline void manaSpent(MutationContext &s, const CardCatalog &cat, PlayerId p, int amount) {
    card_rules::onManaSpent(s, cat, p, amount);
}
} // namespace wizard::arcana
