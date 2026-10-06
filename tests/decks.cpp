#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
using namespace wizard;
using namespace wizard::app;
TEST_CASE("rank and casting fee filters do not give unrelated card types artificial zero values") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    CardQuery query;
    query.rank = 0;
    REQUIRE(queryCards(content.catalog, query) == std::vector<std::string>{"true-strike", "spark"});
    query = {};
    query.castCost = 0;
    const auto rows = queryCards(content.catalog, query);
    REQUIRE_FALSE(rows.empty());
    for (const auto &id : rows)
        REQUIRE(content.catalog.at(id).type == CardType::Analytic);
    query.type = CardType::Action;
    REQUIRE(queryCards(content.catalog, query).empty());
}
namespace {
struct DeckFixture {
    Content content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    std::filesystem::path directory =
        std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "deck-tests" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    ~DeckFixture() {
        std::error_code e;
        std::filesystem::remove_all(directory, e);
    }
    DeckDraft legal(DeckLibrary &store) {
        auto d = store.create("自建解析");
        for (const auto &id : content.deck)
            ++d.cards[id];
        return d;
    }
};
} // namespace
TEST_CASE("deck drafts preserve stable ids counts and bases across restart including incomplete drafts") {
    DeckFixture f;
    DeckLibrary library(f.content.catalog, f.directory);
    std::string error;
    auto d = library.create("中文草稿");
    d.cards["fireball"] = 2;
    REQUIRE(library.save(d, error));
    REQUIRE_FALSE(library.problems(d).empty());
    DeckLibrary loaded(f.content.catalog, f.directory);
    REQUIRE(loaded.find(d.id));
    REQUIRE(encodeDraft(*loaded.find(d.id)) == encodeDraft(d));
    auto full = f.legal(library);
    REQUIRE(library.save(full, error));
    REQUIRE(library.problems(full).empty());
    REQUIRE(full.playerDeck().cards.size() == 30);
}
TEST_CASE("draft validation covers quantity same name limit unknown cards and eligible base") {
    DeckFixture f;
    DeckLibrary library(f.content.catalog, f.directory);
    auto d = f.legal(library);
    REQUIRE(deckProblems(f.content.catalog, d).empty());
    d.cards["fireball"] = 4;
    REQUIRE_FALSE(deckProblems(f.content.catalog, d).empty());
    d = f.legal(library);
    d.baseFormation = "fireball";
    REQUIRE_FALSE(deckProblems(f.content.catalog, d).empty());
    d.baseFormation = "deleted";
    REQUIRE_FALSE(deckProblems(f.content.catalog, d).empty());
    d = f.legal(library);
    d.cards["missing"] = 1;
    REQUIRE_FALSE(deckProblems(f.content.catalog, d).empty());
    d = f.legal(library);
    d.name = "   ";
    REQUIRE_FALSE(deckProblems(f.content.catalog, d).empty());
    auto cat = f.content.catalog;
    auto copy = cat.at("fireball");
    copy.id = "fireball-copy";
    cat.cards.emplace(copy.id, copy);
    d = f.legal(library);
    d.cards["fireball-copy"] = 1;
    REQUIRE_FALSE(deckProblems(cat, d).empty());
}
TEST_CASE("card search combines Chinese partial name type rarity setting casting rank and all tags") {
    DeckFixture f;
    CardQuery q;
    q.name = "火球";
    REQUIRE(queryCards(f.content.catalog, q) == std::vector<std::string>{"fireball"});
    q.type = CardType::Analytic;
    q.rarity = "rare";
    q.cost = 3;
    q.castCost = 1;
    q.rank = 2;
    q.tags = {"伤害", "销毁"};
    REQUIRE(queryCards(f.content.catalog, q) == std::vector<std::string>{"fireball"});
    q.cost = 0;
    REQUIRE(queryCards(f.content.catalog, q).empty());
    q.cost = 3;
    q.tags.push_back("不存在");
    REQUIRE(queryCards(f.content.catalog, q).empty());
    auto cat = f.content.catalog;
    cat.cards.at("fireball").name = "FireBall";
    cat.cards.at("fireball").tags = {"原创方向"};
    q = {};
    q.name = "fire";
    q.tags = {"原创方向"};
    REQUIRE(queryCards(cat, q) == std::vector<std::string>{"fireball"});
}
TEST_CASE("content upgrades retain removed cards and revalidate changed base eligibility") {
    DeckFixture f;
    DeckLibrary library(f.content.catalog, f.directory);
    auto d = f.legal(library);
    std::string error;
    REQUIRE(library.save(d, error));
    auto changed = f.content.catalog;
    changed.cards.erase("fireball");
    changed.cards.at("balance").baseEligible = false;
    changed.contentHash = "new-content";
    DeckLibrary loaded(changed, f.directory);
    REQUIRE(loaded.writable());
    REQUIRE_FALSE(loaded.notice().empty());
    REQUIRE(loaded.find(d.id)->cards.count("fireball") == 1);
    REQUIRE_FALSE(loaded.problems(*loaded.find(d.id)).empty());
    auto repaired = *loaded.find(d.id);
    repaired.cards.erase("fireball");
    REQUIRE(loaded.save(repaired, error));
}
TEST_CASE("atomic deck save and delete failures retain committed library and original file") {
    DeckFixture f;
    DeckLibrary library(f.content.catalog, f.directory);
    std::string error;
    auto d = f.legal(library);
    REQUIRE(library.save(d, error));
    auto json = readJson(f.directory / "decks.json");
    std::filesystem::rename(f.directory / "decks.json", f.directory / "original.json");
    std::filesystem::create_directory(f.directory / "decks.json");
    d.name = "未保存的修改";
    REQUIRE_FALSE(library.save(d, error));
    REQUIRE(library.find(d.id)->name == "自建解析");
    REQUIRE_FALSE(library.erase(d.id, error));
    REQUIRE(library.find(d.id));
    REQUIRE(readJson(f.directory / "original.json") == json);
    for (const auto &entry : std::filesystem::directory_iterator(f.directory))
        REQUIRE(entry.path().filename().string().find(".tmp-") == std::string::npos);
}
TEST_CASE("corrupted unsupported or duplicated draft storage cannot silently overwrite player data") {
    DeckFixture f;
    std::filesystem::create_directories(f.directory);
    std::string error;
    std::ofstream(f.directory / "decks.json") << "broken player file";
    DeckLibrary bad(f.content.catalog, f.directory);
    REQUIRE_FALSE(bad.writable());
    REQUIRE_FALSE(bad.save(bad.create(), error));
    std::ifstream stream(f.directory / "decks.json");
    std::string text;
    std::getline(stream, text);
    REQUIRE(text == "broken player file");
    stream.close();
    DeckLibrary seed(f.content.catalog, f.directory);
    auto d = f.legal(seed);
    for (const auto &j : {Json{{"format", 2}, {"decks", Json::array()}},
                          Json{{"format", 1}, {"decks", Json::array({encodeDraft(d), encodeDraft(d)})}}}) {
        writeJson(f.directory / "decks.json", j);
        DeckLibrary store(f.content.catalog, f.directory);
        REQUIRE_FALSE(store.writable());
        REQUIRE_FALSE(store.erase(d.id, error));
        REQUIRE(readJson(f.directory / "decks.json") == j);
    }
}
TEST_CASE("draft encoding rejects invalid counts while preserving valid obsolete ids for repair") {
    DeckFixture f;
    DeckLibrary library(f.content.catalog, f.directory);
    auto d = library.create();
    auto j = encodeDraft(d);
    j["cards"]["missing"] = 3;
    REQUIRE(decodeDraft(j).cards.at("missing") == 3);
    for (const auto &count : {Json(0), Json(-1), Json(1001), Json(2.5), Json("3")}) {
        auto invalid = j;
        invalid["cards"]["missing"] = count;
        REQUIRE_THROWS(decodeDraft(invalid));
    }
    j["cards"] = Json::array();
    REQUIRE_THROWS(decodeDraft(j));
}
TEST_CASE("saved valid user decks join independent match choices while drafts stay excluded") {
    DeckFixture f;
    Application a(f.content, f.directory);
    std::string error;
    auto d = f.legal(a.decks());
    auto second = d;
    second.id = a.decks().create().id;
    second.name = "另一构筑";
    --second.cards["fireball"];
    ++second.cards["conduit"];
    REQUIRE(a.decks().save(d, error));
    REQUIRE(a.decks().save(second, error));
    REQUIRE(a.decks().save(a.decks().create("未完成"), error));
    auto choices = a.playableDecks();
    REQUIRE(choices.size() == 3);
    MatchConfig c;
    c.seed = 123;
    c.players[0] = choices[1].deck;
    c.players[1] = choices[2].deck;
    REQUIRE(a.start(c, error));
    const auto record = a.match().recording();
    REQUIRE(record["players"][0]["cards"] != record["players"][1]["cards"]);
    REQUIRE(replay(f.content, record).digest() == a.match().engine().digest());
}
TEST_CASE("deleting one user deck preserves every other deck and persists after restart") {
    DeckFixture f;
    DeckLibrary library(f.content.catalog, f.directory);
    std::string error;
    auto a = library.create("A"), b = library.create("B");
    REQUIRE(library.save(a, error));
    REQUIRE(library.save(b, error));
    REQUIRE(library.erase(a.id, error));
    REQUIRE_FALSE(library.find(a.id));
    REQUIRE(library.find(b.id));
    DeckLibrary loaded(f.content.catalog, f.directory);
    REQUIRE(loaded.drafts().size() == 1);
    REQUIRE(loaded.find(b.id));
    REQUIRE_FALSE(loaded.erase(a.id, error));
}
