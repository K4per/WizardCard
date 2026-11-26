#include "wizard/application.hpp"
#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace wizard;

TEST_CASE("only a named local course can start tutorial mode", "[tutorial-retirement]") {
    const auto root = std::filesystem::path(WIZARD_SOURCE_DIR);
    const auto data = loadContent(root / "assets/release-v260");
    const auto dir =
        std::filesystem::temp_directory_path() /
        ("wizard-retired-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    app::Application app(data, dir);
    std::string error;
    MatchConfig config;
    for (auto &player : config.players) {
        player.cards = data.deck;
        player.sideDeck = data.sideDeck;
    }
    CHECK_FALSE(app.start(config, error, {app::MatchMode::Tutorial}));
    bridge::LocalSession session;
    REQUIRE(session.initialize(root / "assets/release-v260", dir).at("ok") == true);
    for (const auto *mode : {"tutorial", "ai", "hotseat"}) {
        for (const auto *removed : {"chapter", "tutorialChapter", "tutorial", "scenario", "lessonDigest"}) {
            CAPTURE(mode, removed);
            CHECK(session.start({{"mode", mode}, {removed, 1}}).at("ok") == false);
        }
    }
    CHECK(session.start({{"mode", "tutorial"}}).at("ok") == false);
    const auto ordinary = MatchSession(data, config).recording();
    REQUIRE_NOTHROW(replay(data, ordinary));
    for (const auto *field : {"chapter", "tutorialChapter", "tutorial", "scenario"}) {
        auto forged = ordinary;
        forged[field] = 1;
        CHECK_THROWS(replay(data, forged));
    }
    auto embedded = ordinary;
    embedded["mode"] = "tutorial";
    CHECK_THROWS(replay(data, embedded));
    std::filesystem::remove_all(dir);
}

TEST_CASE("course mode starts its ordered short deck and scripted opponent with debug and autosaves off",
          "[tutorial-retirement][tutorial-a4]") {
    const auto data = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    const auto dir =
        std::filesystem::temp_directory_path() /
        ("wizard-a4-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    app::Application app(data, dir);
    std::string error;
    REQUIRE(app.startTutorial(error, "lesson-04-spells"));
    REQUIRE_FALSE(app.debugAvailable());
    const auto &state = app.match().engine().state();
    CHECK(state.players[0].deck.size() == 5);
    std::vector<std::string> hand;
    for (const auto &[id, card] : state.cards)
        if (card.owner == 0 && card.zone == Zone::Hand)
            hand.push_back(card.definition);
    REQUIRE(hand == std::vector<std::string>{"v260-analytic-0006", "v260-word-0001", "v260-talisman-0003",
                                             "v260-word-0021", "v260-word-0021"});
    CHECK(app.lessonSnapshot().at("lessonId") == "lesson-04-spells");
    bool enemyPlayedSpark = false;
    for (int step = 0; state.result == -1; ++step) {
        REQUIRE(step < 200);
        if (app.lessonScript()->canContinue(app.viewFor(0))) {
            app.continueTutorial();
            continue;
        }
        if (app.aiTurn()) {
            auto plan = app.planAi();
            REQUIRE(plan);
            if (const auto *word = std::get_if<PreloadWord>(&plan->selection.command)) {
                CHECK(state.cards.at(word->card).definition == "v260-word-0001");
                CHECK(word->target == playerTarget(0));
                enemyPlayedSpark = true;
            }
            REQUIRE(app.commitAi(*plan).accepted);
        } else {
            std::optional<Command> command;
            app.visitActions(0, [&](const LegalAction &action) {
                if (std::holds_alternative<StartAnalysis>(action.command) ||
                    std::holds_alternative<AdvancePhase>(action.command) ||
                    std::holds_alternative<PassResponse>(action.command))
                    command = action.command;
                return !command;
            });
            REQUIRE(command);
            REQUIRE(app.submit(0, *command, app.generation()).accepted);
        }
    }
    CHECK(enemyPlayedSpark);
    CHECK(state.result == 1);
    CHECK_FALSE(app.lessonScript()->complete());
    CHECK_FALSE(std::filesystem::exists(app.saveDirectory() / "replays"));
    std::filesystem::remove_all(dir);
}
