#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
struct PlayerDeck {
    std::string baseFormation{"balance"};
    std::vector<std::string> cards;
    std::vector<std::string> sideDeck;
};
struct MatchConfig {
    std::array<PlayerDeck, 2> players;
    std::uint32_t seed{42};
};
} // namespace wizard
