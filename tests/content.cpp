#include "wizard/replay.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
#include <chrono>
using namespace wizard;
TEST_CASE("manifest preserves canonical card order hash and legacy imports") {
    const auto assets = std::filesystem::path(WIZARD_SOURCE_DIR) / "assets";
    const auto legacy = readJson(assets / "cards.json");
    const auto canonical = readCatalogJson(assets);
    REQUIRE(canonical == legacy);
    REQUIRE(parseCatalog(canonical).contentHash == parseCatalog(legacy).contentHash);
    REQUIRE(loadContent(assets).catalog.contentHash == parseCatalog(legacy).contentHash);
}
TEST_CASE("manifest rejects duplicates ID mismatch traversal and missing definitions") {
    struct TemporaryDirectory {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("wizard-catalog-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TemporaryDirectory() { std::filesystem::create_directories(path / "cards"); }
        ~TemporaryDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } temporary;
    const auto assets = std::filesystem::path(WIZARD_SOURCE_DIR) / "assets";
    auto legacy = readJson(assets / "cards.json");
    writeJson(temporary.path / "cards.json", legacy);
    REQUIRE(readCatalogJson(temporary.path) == legacy); // Legacy fixtures remain supported.
    auto definition = legacy.at("cards").at(0);
    writeJson(temporary.path / "cards/one.json", definition);
    legacy.erase("cards");
    Json manifest = {{"format", 1}, {"metadata", legacy},
                     {"definitions", Json::array({{{"id", definition.at("id")}, {"file", "cards/one.json"}}})}};
    auto check = [&](Json value) {
        writeJson(temporary.path / "catalog.json", value);
        REQUIRE_THROWS(readCatalogJson(temporary.path));
    };
    auto bad = manifest;
    bad["definitions"].push_back(bad["definitions"][0]);
    check(bad);
    bad = manifest; bad["definitions"][0]["id"] = "wrong"; check(bad);
    for (const auto *filename : {"../cards.json", "C:/cards.json", "/cards.json", "cards\\one.json", "cards/missing.json"}) {
        bad = manifest; bad["definitions"][0]["file"] = filename; check(bad);
    }
    bad = manifest; bad["metadata"]["cards"] = Json::array(); check(bad);
    writeJson(temporary.path / "catalog.json", manifest);
    REQUIRE(readCatalogJson(temporary.path).at("cards").at(0) == definition);
}
TEST_CASE("content validation includes unknown effects duplicates references and counts") {
    const auto dir=std::filesystem::path(WIZARD_SOURCE_DIR)/"assets";auto j=readJson(dir/"cards.json");auto c=loadContent(dir);
    REQUIRE(c.catalog.cards.size()==30);REQUIRE(c.deck.size()==30);
    auto bad=j;bad["cards"][0]["body"]=-1;REQUIRE_THROWS_WITH(parseCatalog(bad),"cards.json/cards/0/body: expected integer in [0,1000]");
    bad=j;bad["cards"].push_back(bad["cards"][0]);REQUIRE_THROWS(parseCatalog(bad));
    bad=j;bad["cards"][1]["effects"][0]["kind"]="script";REQUIRE_THROWS(parseCatalog(bad));
    auto deck=readJson(dir/"deck.json");deck[0]["id"]="missing";REQUIRE_THROWS(parseDeck(deck,c.catalog));
    deck=readJson(dir/"deck.json");deck[0]["count"]=4;REQUIRE_THROWS(parseDeck(deck,c.catalog));
}
TEST_CASE("every command variant round trips") {
    std::vector<Command> commands={SetFormation{1,2},RemoveFormation{1},StartAnalysis{1,2},PrepareCast{1,2,3},PlayAction{1,2},PreloadWord{1},AttachSeal{1,2},RemoveSeal{1},Abandon{1},Advance{},Choose{5,6},Surrender{}};
    for(const auto& c:commands) REQUIRE(encodeCommand(decodeCommand(encodeCommand(c)))==encodeCommand(c));
    REQUIRE_THROWS(decodeCommand(Json{{"type",0},{"card",4294967296ull},{"replace",0}}));
    REQUIRE_THROWS(decodeCommand(Json{{"type",9.5}}));
}
TEST_CASE("replay detects incompatible content versions and tampering") {
    auto c=loadContent(std::filesystem::path(WIZARD_SOURCE_DIR)/"assets");MatchSession session(c,42);auto p=session.engine().state().active;
    REQUIRE(session.submit(p,AdvancePhase{session.engine().state().phaseGate}).accepted);REQUIRE(session.submit(1-p,Surrender{}).accepted);
    auto j=session.recording();REQUIRE(replay(c,j).digest()==session.engine().digest());
    j["contentHash"]="changed";REQUIRE_THROWS(replay(c,j));j=session.recording();j["commands"][0]["digest"]="wrong";REQUIRE_THROWS(replay(c,j));
}
