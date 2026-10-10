#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
struct PlayerDeck {
    std::string baseFormation{"balance"};
    std::vector<std::string> cards;
};
struct MatchConfig {
    std::array<PlayerDeck, 2> players;
    std::uint32_t seed{42};
};
} // namespace wizard
