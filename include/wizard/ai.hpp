#pragma once
#include "wizard/core/view.hpp"
#include <cstddef>
#include <functional>

namespace wizard::ai {
enum class Difficulty { Easy, Normal, Hard };
std::string difficultyLabel(Difficulty);
struct Selection {
    Command command;
    double score{};
    std::size_t evaluated{};
};
// Receives only the acting player's projection. No engine, catalog, RNG, opponent hand or deck.
using CandidateSource = std::function<void(const std::function<bool(const LegalAction &)> &)>;
std::optional<Selection> choose(const GameView &, const CandidateSource &, Difficulty, std::size_t budget = 512);
// Explicit, already visible candidates for isolated decision tests.
std::optional<Selection> choose(const GameView &, const std::vector<LegalAction> &, Difficulty, std::size_t budget = 512);
} // namespace wizard::ai
