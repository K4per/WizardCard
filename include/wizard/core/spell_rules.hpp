#pragma once
#include "wizard/core/rules.hpp"

namespace wizard::spell_rules {
bool active(const CardInstance &);
bool blocked(const GameState &, ModifierKind, PlayerId);
int loadMultiplier(const GameState &, PlayerId);
void addModifier(MutationContext &, const Trigger &, ModifierKind, ModifierExpiry, int amount = 1,
                 PlayerId player = -1, CardId target = 0, DamageType type = DamageType::Force);
void expire(MutationContext &, const CardCatalog &, bool end);
void setup(MutationContext &, const CardCatalog &, CardId, bool ambush);
void wordCast(MutationContext &, const CardCatalog &, CardId);
void damaged(MutationContext &, const CardCatalog &, PlayerId, const Trigger &, int amount, std::optional<DamageType>);
bool preventLethal(MutationContext &, const CardCatalog &, PlayerId, int amount);
bool playerTargetAllowed(const GameState &, const CardCatalog &, PlayerId actor, PlayerId target);
}
