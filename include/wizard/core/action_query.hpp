#pragma once
#include "wizard/core/commands.hpp"
#include "wizard/core/text_ref.hpp"
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
    std::optional<TextRef> labelRef{};
};
struct ActionOptions {
    ActionField field{ActionField::Operation};
    std::vector<ActionOption> options;
    std::optional<Command> complete;
    bool hasMore{}, pending{};
    std::string error;
    std::string costQuote;
    int overloadPreview{};
    std::optional<TextRef> costQuoteRef{};
    std::optional<TextRef> errorRef{};
};
struct ActionSummary {
    bool actionable{}, hasNonPhaseAction{};
    std::optional<Command> safeAdvance;
};
}
