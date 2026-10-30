#pragma once
#include "wizard/core/commands.hpp"
#include <functional>

namespace wizard {
// Query-local selections: never rule entity IDs or state mutations.
enum class ActionField { Operation, Source, Host, Replacement, Material, Target, Discard, Link, Ability, Ring, Choice, Confirm };
struct ActionSelection {
    ActionField field{ActionField::Operation};
    std::string value;
};
struct ActionIntent {
    std::vector<ActionSelection> selections;
    std::size_t offset{}, limit{64};
    CardId source{}; // Optional card scope, independent of the selection path.
};
struct ActionOption {
    std::string value, label;
    CardId card{};
    PlayerId player{-1};
};
struct ActionOptions {
    ActionField field{ActionField::Operation};
    std::vector<ActionOption> options;
    std::optional<Command> complete;
    bool hasMore{}, pending{};
    std::string error;
};
struct ActionSummary {
    bool actionable{}, hasNonPhaseAction{};
    std::optional<Command> safeAdvance;
};
}
