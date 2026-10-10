#pragma once
#include "wizard/core/view.hpp"
#include <cstddef>

namespace wizard::ai {
enum class Difficulty { Easy, Normal, Hard };
std::string difficultyLabel(Difficulty);
struct Selection {
    Command command;
    double score{};
    std::size_t evaluated{};
};
// Receives only the acting player's projection. No engine, catalog, RNG, opponent hand or deck.
std::optional<Selection> choose(const GameView &, Difficulty, std::size_t budget = 512);
} // namespace wizard::ai
