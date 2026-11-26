#pragma once
#include "wizard/core/catalog.hpp"
#include "wizard/core/config.hpp"
#include "wizard/text_catalog.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace wizard {
using Json=nlohmann::json;
struct Content {
    CardCatalog catalog;
    std::vector<std::string> deck;
    std::vector<std::string> sideDeck;
    std::string initialFormation{"balance"};
    std::filesystem::path dataDirectory;
    std::filesystem::path textDirectory;
    std::shared_ptr<const TextCatalog> textCatalog{};
    PlayerDeck defaultPlayerDeck() const { return {initialFormation, deck, sideDeck}; }
    std::string renderText(const TextRef &, std::string_view locale = {},
                           const TextCatalog::Diagnostic &diagnostic = {}) const;
};
Content loadContent(const std::filesystem::path& directory);
CardCatalog parseCatalog(const Json&,const std::string& source="cards.json");
// Explicit manifest order is the canonical card order; legacy cards.json remains importable.
Json readCatalogJson(const std::filesystem::path& directory);
std::vector<std::string> parseDeck(const Json&,const CardCatalog&,const std::string& source="deck.json");
Json readJson(const std::filesystem::path&);
void writeJson(const std::filesystem::path&,const Json&);
void writeAtomicJson(const std::filesystem::path&,const Json&);
void createAtomicJson(const std::filesystem::path&,const Json&);
}
