#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
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
};
} // namespace wizard
