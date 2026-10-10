#pragma once
#include "wizard/core/state.hpp"

namespace wizard {
struct Rules {
    static int load(const GameState &, PlayerId);
    static int capacity(const GameState &, const CardCatalog &, PlayerId);
    static int rings(const GameState &, const CardCatalog &, CardId);
    static int maxRank(const GameState &, const CardCatalog &, CardId);
    static int analysisCost(const GameState &, const CardCatalog &, CardId, CardId);
    static int castCost(const GameState &, const CardCatalog &, CardId);
    static int damage(const GameState &, const CardCatalog &, PlayerId, const Effect &);
    static int occupied(const GameState &, CardId);
    static bool targetValid(const GameState &, const CardDefinition &, PlayerId, CardId);
    static bool targetValid(const GameState &, TargetKind, PlayerId, CardId);
    static bool linkTargetValid(const GameState &, TargetKind, LinkId);
    static std::vector<std::string> deckErrors(const CardCatalog &, const PlayerDeck &);
    static std::vector<std::string> invariants(const GameState &, const CardCatalog &);
};
struct StateMaintenance {
    static void leave(GameState &, CardId, const CardCatalog * = nullptr);
    static void check(GameState &, const CardCatalog &, bool endCleanup = false);
};
struct ResourceRules {
    // Rules damage bypasses spell resistance, consumes temporary life first.
    // EffectResolver checks terminal state before executing the next effect.
    static void damage(GameState &, PlayerId, int amount, const std::string &reason);
    // Call immediately after each individual positive mutation, with its gross increase.
    static void increasedLoad(GameState &, const CardCatalog &, PlayerId, int increment);
    static void drawOne(GameState &, const CardCatalog &, PlayerId);
};
struct EffectResolver {
    static void apply(GameState &, const CardCatalog &, const Trigger &, const Effect &);
};
} // namespace wizard
