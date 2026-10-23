#pragma once
#include "wizard/core/types.hpp"
namespace wizard {
// Deliberately separate from Command: never accepted by the multiplayer codec.
struct DebugOperation {
    std::string operation;
    PlayerId player{};
    int amount{};
    std::string cardId, loadKind;
    CardId sourceId{};
    bool enabled{};
};
}
