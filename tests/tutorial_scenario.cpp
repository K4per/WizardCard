#include "wizard/tutorial_scenario.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <chrono>
using namespace wizard;
namespace {
struct Courses {
    std::filesystem::path directory = std::filesystem::temp_directory_path() /
        ("wizard-scenarios-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    Courses() { std::filesystem::create_directories(directory); }
    ~Courses() { std::error_code error; std::filesystem::remove_all(directory, error); }
    void write(const Json &document, const std::string &suffix = {}) {
        std::ofstream file(directory / "fixture.json", std::ios::binary);
        file << document.dump(2) << suffix;
    }
};
const Content &content() {
    static const auto value = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    return value;
}
Json scene() {
    Json player = {{"life", 5}, {"mana", 4}, {"load", 2},
        {"deck", {"v260-analytic-0001", "v260-word-0001"}},
        {"hand", {"v260-talisman-0003"}}, {"formation", "v260-formation-0016"},
        {"resolved", {{{"card", "v260-analytic-0001"}, {"ring", 0}, {"load", 1}}}},
        {"faceDown", {{{"card", "v260-talisman-0003"}, {"ring", 1}}}}};
    return {{"id", "fixture"}, {"cardRevision", 13},
        {"scenario", {{"phase", "Main"}, {"firstPlayer", 0}, {"turn", 1}, {"players", {player, player}}}},
        {"steps", Json::array()}};
}
std::vector<std::string> deck(const GameEngine &engine, PlayerId player) {
    std::vector<std::string> result;
    const auto &ordered = engine.state().players[player].deck;
    for (auto it = ordered.rbegin(); it != ordered.rend(); ++it)
        result.push_back(engine.state().cards.at(*it).definition);
    return result;
}
}
TEST_CASE("trusted tutorial scene preserves both ordered short decks and legal presets", "[tutorial-scenario]") {
    Courses courses; const auto document = scene(); courses.write(document);
    const auto lesson = TutorialScenario::load(courses.directory, "fixture", content().catalog);
    CHECK(lesson.id() == "fixture"); CHECK(lesson.document() == document);
    const auto engine = lesson.start(content().catalog);
    CHECK(Rules::invariants(engine.state(), engine.catalog()).empty());
    CHECK(engine.digest() == lesson.start(content().catalog).digest());
    for (PlayerId p : {0, 1}) {
        CHECK(deck(engine, p) == document["scenario"]["players"][p]["deck"].get<std::vector<std::string>>());
        CHECK(engine.state().players[p].life == 5); CHECK(engine.state().players[p].mana == 4);
        CHECK(Rules::load(engine.state(), p) == 3);
        for (const auto &[id, card] : engine.state().cards) if (card.owner == p && card.host) {
            if (card.faceDown) { CHECK(card.analysisProgress == 0); CHECK(card.spell == SpellState::None); CHECK(card.analysisLoad == 0); }
            else { CHECK(card.analysisProgress == engine.catalog().at(card.definition).analysisTurns); CHECK(card.spell == SpellState::Ready); }
        }
    }
    courses.write(document, "\n");
    CHECK(lesson.digest() != TutorialScenario::load(courses.directory, "fixture", content().catalog).digest());
    auto changed = content().catalog; changed.contentHash += "changed";
    CHECK_THROWS(lesson.start(changed));
}
TEST_CASE("trusted real tutorial opening keeps order through RPS ties and formation choice", "[tutorial-scenario]") {
    Courses courses; auto document = scene();
    auto &scenario = document["scenario"]; scenario = {{"opening", true}, {"seed", 73}, {"players", Json::array()}};
    for (PlayerId p : {0, 1}) {
        auto ordered = content().deck;
        if (p) std::reverse(ordered.begin(), ordered.end());
        scenario["players"].push_back({{"deck", ordered}, {"sideDeck", content().sideDeck}});
    }
    courses.write(document);
    auto engine = TutorialScenario::load(courses.directory, "fixture", content().catalog).start(content().catalog);
    CHECK(engine.state().openingStage == OpeningStage::Gestures);
    REQUIRE(engine.submit(0, SubmitOpeningGesture{1, 1}).accepted);
    REQUIRE(engine.submit(1, SubmitOpeningGesture{1, 1}).accepted);
    CHECK(engine.state().openingRound == 2);
    REQUIRE(engine.submit(0, SubmitOpeningGesture{2, 0}).accepted);
    REQUIRE(engine.submit(1, SubmitOpeningGesture{2, 2}).accepted);
    CHECK(engine.state().first == 0);
    for (PlayerId p : {0, 1}) {
        const auto ordered = scenario["players"][p]["deck"].get<std::vector<std::string>>();
        CHECK(deck(engine, p) == std::vector<std::string>(ordered.begin() + 5, ordered.end()));
        std::vector<std::string> hand;
        CardId initial{};
        for (const auto &[id, card] : engine.state().cards) if (card.owner == p) {
            if (card.zone == Zone::Hand) hand.push_back(card.definition);
            if (card.zone == Zone::SideDeck && engine.catalog().at(card.definition).level == 1) initial = id;
        }
        CHECK(hand == std::vector<std::string>(ordered.begin(), ordered.begin() + 5));
        REQUIRE(engine.submit(p, SelectInitialFormation{initial}).accepted);
    }
    CHECK_FALSE(engine.state().opening);
    CHECK(Rules::invariants(engine.state(), engine.catalog()).empty());
    document["scenario"]["players"][0]["deck"].erase(0);
    courses.write(document); CHECK_THROWS(TutorialScenario::load(courses.directory, "fixture", content().catalog));
}
TEST_CASE("trusted tutorial rejects invalid identities revisions resources and placements", "[tutorial-scenario]") {
    Courses courses;
    for (const auto &id : {"../fixture", "fixture.json", "a/b", "A", "a\\b", ""})
        CHECK_THROWS(TutorialScenario::load(courses.directory, id, content().catalog));
    const auto valid = scene();
    const std::vector<std::pair<std::string, Json>> invalid = {
        {"/id", "other"}, {"/cardRevision", 12}, {"/scenario/players/0/deck", Json::array()},
        {"/scenario/players/1/deck/0", "missing-card"}, {"/scenario/players/0/mana", 21},
        {"/scenario/players/0/life", 0}, {"/scenario/players/0/load", -1},
        {"/scenario/players/0/sideDeck", {"v260-analytic-0001"}},
        {"/scenario/players/0/deck/0", "v260-formation-0016"},
        {"/scenario/players/0/formation", "v260-analytic-0001"},
        {"/scenario/players/0/faceDown/0/ring", 0}, {"/scenario/players/0/faceDown/0/ring", 99},
        {"/scenario/players/0/faceDown/0/card", "v260-formation-0016"},
        {"/scenario/players/0/resolved/0/card", "v260-talisman-0003"},
        {"/scenario/players/0/resolved/0/formation", 1},
        {"/scenario/activePlayer", 2}, {"/scenario/turn", 0}, {"/scenario/phase", "unknown"},
        {"/scenario/players/0/unknown", true}};
    for (const auto &[path, value] : invalid) {
        INFO(path); auto document = valid; document[Json::json_pointer(path)] = value; courses.write(document);
        CHECK_THROWS(TutorialScenario::load(courses.directory, "fixture", content().catalog));
    }
}
