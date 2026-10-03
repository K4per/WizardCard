#include "wizard/content.hpp"
#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers.hpp>
using namespace wizard;
TEST_CASE("content validation includes unknown effects duplicates references and counts") {
    const auto dir=std::filesystem::path(WIZARD_SOURCE_DIR)/"assets";auto j=readJson(dir/"cards.json");auto c=loadContent(dir);
    REQUIRE(c.catalog.cards.size()==13);REQUIRE(c.deck.size()==30);
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
    REQUIRE(session.submit(p,Advance{}).accepted);REQUIRE(session.submit(1-p,Surrender{}).accepted);
    auto j=session.recording();REQUIRE(replay(c,j).digest()==session.engine().digest());
    j["contentHash"]="changed";REQUIRE_THROWS(replay(c,j));j=session.recording();j["commands"][0]["digest"]="wrong";REQUIRE_THROWS(replay(c,j));
}
