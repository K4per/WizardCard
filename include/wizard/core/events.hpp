#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
struct CardTravel {
    Zone from{}, to{};
    PlayerId fromPlayer{}, toPlayer{};
    CardId fromHost{}, toHost{};
    bool back{};
    int fromRing{-1}, toRing{-1};
};
struct GameEvent {
    std::string kind, text;
    int audience{-1};
    CardId card{};
    int amount{};
    // Presentation metadata. Logs retain their text; consumers need not parse localized prose.
    std::optional<EffectKind> effectType{};
    int actualAmount{};
    PlayerId affectedPlayer{-1};
    bool spellReleased{};
    int chainIndex{}; // Public, one-based position in the current chain.
    std::optional<CardTravel> travel{};
    std::vector<CardId> publicTargets{};
};
} // namespace wizard
