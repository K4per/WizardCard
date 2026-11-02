#pragma once
#include "wizard/core/state.hpp"

namespace wizard {
struct Rules {
    static int load(const GameState &, PlayerId);
    static int capacity(const GameState &, const CardCatalog &, PlayerId);
    static int income(const GameState &, const CardCatalog &, CardId);
    static int level(const GameState &, const CardCatalog &, CardId);
    static int rings(const GameState &, const CardCatalog &, CardId);
    static int maxRank(const GameState &, const CardCatalog &, CardId);
    static int analysisCost(const GameState &, const CardCatalog &, CardId, CardId);
    static int castCost(const GameState &, const CardCatalog &, CardId);
    static int concentrationUpkeep(const GameState &, const CardCatalog &, CardId);
    static int damage(const GameState &, const CardCatalog &, PlayerId, const Effect &);
    static int occupied(const GameState &, CardId);
    static int castingCapacity(const GameState &, const CardCatalog &, PlayerId);
    static int freeRing(const GameState &, const CardCatalog &, CardId);
    static bool canHost(const GameState &, const CardCatalog &, CardId spell, CardId formation,
                        bool faceDown = false);
    static bool targetValid(const GameState &, const CardDefinition &, PlayerId, CardId);
    static bool targetValid(const GameState &, TargetKind, PlayerId, CardId);
    static bool linkTargetValid(const GameState &, TargetKind, LinkId);
    static std::vector<std::string> deckErrors(const CardCatalog &, const PlayerDeck &);
    static std::vector<std::string> invariants(const GameState &, const CardCatalog &);
};
struct StateMaintenance {
    static void leave(MutationContext &, CardId, const CardCatalog * = nullptr);
    static void check(MutationContext &, const CardCatalog &, bool endCleanup = false);
};
struct ResourceRules {
    static bool canPayMana(const GameState &, PlayerId, int amount);
    static void payMana(MutationContext &, PlayerId, int amount);
    // Each grant expires at the start of the recipient's indicated draw phase.
    // Source 0 denotes a migrated aggregate with unknown historical provenance.
    static void normalizeTemporaryLife(MutationContext &, PlayerId);
    static void grantTemporaryLife(MutationContext &, PlayerId, CardId source, int amount, int expiryOwnTurn);
    static void absorbTemporaryLife(MutationContext &, PlayerId, int amount);
    static void clearTemporaryLife(MutationContext &, PlayerId, CardId source = 0);
    static void expireTemporaryLife(MutationContext &, PlayerId);
    // Rules damage bypasses spell resistance, consumes temporary life first.
    // EffectResolver checks terminal state before executing the next effect.
    static void damage(MutationContext &, PlayerId, int amount, const std::string &reason, const CardCatalog *catalog = nullptr);
    // Call immediately after each individual positive mutation, with its gross increase.
    static void increasedLoad(MutationContext &, const CardCatalog &, PlayerId, int increment);
    static void drawOne(MutationContext &, const CardCatalog &, PlayerId);
};
struct EffectResolver {
    static void apply(MutationContext &, const CardCatalog &, const Trigger &, const Effect &);
};
} // namespace wizard
