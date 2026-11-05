#include "wizard/central.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace wizard;
TEST_CASE("central handshake selects protocol five while diagnostic peers keep four", "[central][protocol]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    REQUIRE(central::hello(content).at("protocol") == 5);
    REQUIRE(net::hello(content).at("protocol") == 4);
    REQUIRE(central::compatibilityError(content, central::hello(content)).empty());
    for (const auto *key : {"protocol", "program", "rules", "mechanics", "cards", "hash"}) {
        auto incompatible = central::hello(content);
        incompatible[key] = "wrong";
        REQUIRE_FALSE(central::compatibilityError(content, incompatible).empty());
    }
}
TEST_CASE("central deck admission validates both main and side deck", "[central][protocol]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    auto data = central::encodeDeck(content.defaultPlayerDeck());
    REQUIRE(central::decodeDeck(data, content.catalog).cards.size() == 40);
    data["cards"][0] = "unknown-card";
    REQUIRE_THROWS(central::decodeDeck(data, content.catalog));
    data = central::encodeDeck(content.defaultPlayerDeck());
    data["sideDeck"] = Json::array();
    REQUIRE_THROWS(central::decodeDeck(data, content.catalog));
}
TEST_CASE("central handshake survives bytewise framing", "[central][protocol]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    const auto bytes = net::frame(central::hello(content));
    net::Decoder decoder;
    std::vector<Json> received;
    for (auto byte : bytes) {
        auto batch = decoder.feed(&byte, 1);
        received.insert(received.end(), batch.begin(), batch.end());
    }
    REQUIRE(received.size() == 1);
    REQUIRE(received.front().at("protocol") == 5);
    REQUIRE(received.front().at("hash") == content.catalog.contentHash);
}
