#pragma once
#include "wizard/network.hpp"

namespace wizard::central {
inline constexpr std::size_t replayLimit = 32 * net::maxMessage;
Json hello(const Content &);
std::string compatibilityError(const Content &, const Json &);
Json encodeDeck(const PlayerDeck &);
PlayerDeck decodeDeck(const Json &, const CardCatalog &);
Json error(const std::string &code, const std::string &message);
} // namespace wizard::central
