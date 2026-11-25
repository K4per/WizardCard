#include "wizard/replay.hpp"
#include "wizard/tutorial_scenario.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace wizard;

TEST_CASE("local tutorial replays reconstruct only from the trusted course", "[tutorial-scenario][replay]") {
    const auto root = std::filesystem::path(WIZARD_SOURCE_DIR);
    const auto content = loadContent(root / "assets/release-v260");
    const auto directory = root / "build" / "test-tutorial-recording";
    const auto courses = directory / "courses";
    const auto saves = directory / "saves";
    const Json player = {{"deck", {"v260-word-0001"}}, {"formation", "v260-formation-0009"}};
    writeAtomicJson(courses / "sample.json", {{"id", "sample"}, {"cardRevision", 13},
        {"scenario", {{"players", {player, player}}}}});
    const auto lesson = TutorialScenario::load(courses, "sample", content.catalog);
    MatchSession match(content, lesson);
    match.enableDebug(true);
    REQUIRE_FALSE(match.debugEnabled());
    REQUIRE(match.submit(0, Surrender{}).accepted);
    const auto recording = match.recording();
    CHECK(recording.at("lessonId") == "sample");
    CHECK(recording.at("lessonDigest") == lesson.digest());
    CHECK_FALSE(recording.contains("players"));
    CHECK_FALSE(recording.contains("seed"));
    REQUIRE(replay(content, recording, {}, {}, &lesson).digest() == match.engine().digest());
    REQUIRE_THROWS(replay(content, recording));
    for (const auto *field : {"seed", "players", "scenario", "tutorialChapter"}) {
        auto forged = recording;
        forged[field] = field == std::string("players") ? Json::array({player, player}) : Json(0);
        CHECK_THROWS(replay(content, forged, {}, {}, &lesson));
    }
    auto changed = recording;
    changed["lessonDigest"] = "different";
    CHECK_THROWS(replay(content, changed, {}, {}, &lesson));
    auto debug = recording;
    debug["containsDebugOperations"] = true;
    CHECK_THROWS(replay(content, debug, {}, {}, &lesson));
    debug = recording;
    debug["commands"][0]["debug"] = Json::object();
    CHECK_THROWS(replay(content, debug, {}, {}, &lesson));

    // Imported player data always takes the standard construction path, including files in saves.
    auto ordinary = MatchSession(content, 42).recording();
    ordinary["players"][0]["cards"] = player.at("deck");
    const auto path = saves / "ordinary.json";
    writeAtomicJson(path, ordinary);
    CHECK_THROWS(replay(content, readJson(path)));
    CHECK_THROWS(replay(content, readJson(path), {}, {}, &lesson));
}
