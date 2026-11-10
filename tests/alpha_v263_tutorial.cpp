#include "wizard/bridge.hpp"
#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace wizard;

TEST_CASE("v263 all five tutorial chapters start with legal current content", "[alpha-v263][tutorial-v263]") {
    const auto assets = std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260";
    for (int chapter = 1; chapter <= 5; ++chapter) {
        INFO(chapter);
        const auto directory = std::filesystem::temp_directory_path() /
            ("wizard-v263-chapter-" + std::to_string(chapter) + "-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        bridge::LocalSession session;
        REQUIRE(session.initialize(assets, directory).at("ok") == true);
        const auto started = session.start({{"mode", "tutorial"}, {"chapter", chapter}});
        INFO(started.dump());
        REQUIRE(started.at("ok") == true);
        const auto snapshot = session.snapshot(0).at("data");
        REQUIRE(snapshot.at("tutorial").at("chapter") == chapter);
        REQUIRE(snapshot.at("opening") == (chapter == 2));
        std::filesystem::remove_all(directory);
    }
}

TEST_CASE("v263 tutorial exercises execute real costs damage and replay", "[alpha-v263][tutorial-v263]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    for (int chapter = 2; chapter <= 5; ++chapter) {
        INFO(chapter);
        const auto directory = std::filesystem::temp_directory_path() /
            ("wizard-v263-exercise-" + std::to_string(chapter) + "-" +
             std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        app::Application application(content, directory);
        std::string error;
        REQUIRE(application.startTutorial(error, chapter));
        const auto initial = application.match().engine().digest();
        REQUIRE_FALSE(application.submit(0, RemoveFormation{9}, application.generation()).accepted);
        REQUIRE(application.match().engine().digest() == initial);
        bool optionalMissileChoice = false;
        bool shieldPaid = false;
        bool formationPaid = false;
        int steps = 0;
        for (; steps < 400 && application.tutorial()->lesson != app::Lesson::Complete; ++steps) {
            const auto &state = application.match().engine().state();
            if (state.decision && state.decision->kind == DecisionKind::DestroyAmbush) optionalMissileChoice = true;
            const auto mana0 = state.players[0].mana;
            Command command;
            CommandResult result;
            if (application.aiTurn()) {
                auto plan = application.planAi();
                INFO("AI step " << steps << " tutorial " << application.tutorial()->step);
                REQUIRE(plan);
                command = plan->selection.command;
                result = application.commitAi(*plan);
            } else {
                std::optional<Command> selected;
                application.visitActions(0, [&](const LegalAction &action) {
                    if (std::holds_alternative<Surrender>(action.command)) return true;
                    // Rock wins the scripted opponent's scissors, without bypassing opening.
                    if (const auto *gesture = std::get_if<SubmitOpeningGesture>(&action.command); gesture && gesture->gesture != 0) return true;
                    selected = action.command; return false;
                });
                INFO("learner step " << steps << " tutorial " << application.tutorial()->step << " phase " << int(state.phase));
                REQUIRE(selected);
                command = *selected;
                result = application.submit(0, command, application.generation());
                if (std::holds_alternative<SetFormation>(command)) {
                    REQUIRE(application.match().engine().state().players[0].mana == mana0 - 2);
                    formationPaid = true;
                }
                if (std::holds_alternative<Respond>(command) || std::holds_alternative<FlipAmbush>(command)) {
                    REQUIRE(application.match().engine().state().players[0].mana == mana0 - 3);
                    shieldPaid = true;
                }
            }
            INFO(result.error);
            REQUIRE(result.accepted);
        }
        INFO("completed commands " << steps);
        REQUIRE(application.tutorial()->lesson == app::Lesson::Complete);
        const auto &state = application.match().engine().state();
        if (chapter == 2) REQUIRE(formationPaid);
        if (chapter == 3) REQUIRE(state.players[1].life == 33);
        if (chapter == 4) REQUIRE(state.players[1].life == 37);
        if (chapter == 5) {
            REQUIRE(shieldPaid);
            REQUIRE(optionalMissileChoice);
            REQUIRE(state.players[0].life == 40);
        }
        REQUIRE(replay(content, application.match().recording()).digest() == application.match().engine().digest());
        std::filesystem::remove_all(directory);
    }
}
