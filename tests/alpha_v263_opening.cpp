#include "wizard/network.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
using namespace wizard;
namespace {
Command gesture(unsigned round, int value) {
    return decodeCommand({{"type", 21}, {"round", round}, {"gesture", value}});
}
CardId initial(const GameView &view) {
    for (const auto &card : view.cards)
        if (card.instance.zone == Zone::SideDeck && card.definition.level == 1) return card.instance.id;
    throw std::logic_error("missing initial formation");
}
}
TEST_CASE("v263 opening keeps gestures private across ties and draws before formation selection without clock charge", "[alpha-v263][opening]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    for (PlayerId winner : {0, 1}) {
        INFO(winner);
        MatchSession match(content, 73);
        auto now = MatchSession::Clock::time_point{};
        match.enableClock(true, [&] { return now; });
        const auto operations = match.engine().queryActions(0, {});
        REQUIRE(std::any_of(operations.options.begin(), operations.options.end(),
            [](const ActionOption &option) { return option.value == "21"; }));
        ActionIntent gestureMenu;
        gestureMenu.selections = {{ActionField::Operation, "21"}};
        REQUIRE(match.engine().queryActions(0, gestureMenu).options.size() == 3);
        REQUIRE_FALSE(match.submit(0, SelectInitialFormation{initial(match.viewFor(0))}).accepted);
        for (unsigned round = 1; round <= 3; ++round) {
            const auto before = match.engine().digest();
            REQUIRE_FALSE(match.submit(0, gesture(round + 1, 1)).accepted);
            REQUIRE(match.engine().digest() == before);
            REQUIRE(match.submit(0, gesture(round, 1)).accepted);
            const auto hidden = net::encodeView(match.viewFor(1));
            CHECK(hidden.at("openingGesture") == -1);
            CHECK(hidden.at("gestureConfirmed") == Json::array({true, false}));
            const auto confirmed = match.engine().digest();
            REQUIRE_FALSE(match.submit(0, gesture(round, 2)).accepted);
            CHECK(match.engine().digest() == confirmed);
            now += std::chrono::hours(1); match.tick();
            CHECK(match.viewFor(0).clock.remainingMs == std::array<std::int64_t, 2>{300000, 300000});
            REQUIRE(match.submit(1, gesture(round, 1)).accepted);
            const auto tied = net::encodeView(match.viewFor(0));
            CHECK(tied.at("openingRound") == round + 1);
            CHECK(tied.at("revealedGestures") == Json::array({1, 1}));
            CHECK(match.viewFor(0).players[0].handCount == 0);
        }
        REQUIRE(match.submit(1, gesture(4, winner == 1 ? 0 : 2)).accepted);
        REQUIRE(match.submit(0, gesture(4, winner == 0 ? 0 : 2)).accepted);
        CHECK(match.engine().state().first == winner);
        for (PlayerId p : {0, 1}) {
            CHECK(match.viewFor(p).players[p].handCount == 5);
            CHECK(match.viewFor(p).clock.runningPlayers == std::array<bool, 2>{true, true});
            auto wire = net::encodeView(match.viewFor(p));
            CHECK(wire.at("openingStage") == "formation");
            CHECK(net::encodeView(net::decodeView(wire, content.catalog)) == wire);
        }
        now += std::chrono::seconds(7); match.tick();
        CHECK(match.viewFor(0).clock.remainingMs == std::array<std::int64_t, 2>{293000, 293000});
        REQUIRE(match.submit(0, SelectInitialFormation{initial(match.viewFor(0))}).accepted);
        CHECK(match.viewFor(1).initialFormation == 0);
        CHECK(match.viewFor(0).players[0].handCount == 5);
        REQUIRE(match.submit(1, SelectInitialFormation{initial(match.viewFor(1))}).accepted);
        CHECK_FALSE(match.viewFor(0).opening);
        CHECK(match.engine().state().active == winner);
        CHECK(match.recording().at("format") == 6);
        CHECK(match.recording().at("digestSchema") == 4);
        CHECK(replay(content, match.recording()).digest() == match.engine().digest());
        CHECK(match.engine().canonicalState().find("wizardcard-state-v8:") == 0);
    }
}
