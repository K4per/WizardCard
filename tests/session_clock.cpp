#include "wizard/network.hpp"
#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace wizard;
namespace {
Content clockContent() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"); }
Command initialChoice(const GameView &v) {
    for (const auto &card : v.cards)
        if (card.instance.owner == v.viewer && card.instance.zone == Zone::SideDeck) return SelectInitialFormation{card.instance.id};
    throw std::runtime_error("missing initial formation choice");
}
}
TEST_CASE("LAN clock charges only unconfirmed opening decisions and ordinary submissions cannot forge timeout", "[alpha-v25][clock]") {
    auto c = clockContent(); MatchSession session(c, 42);
    auto now = MatchSession::Clock::time_point{};
    session.enableClock(true, [&] { return now; });
    REQUIRE(session.viewFor(0).clock.runningPlayers == std::array<bool, 2>{true, true});
    now += std::chrono::seconds(10); session.tick();
    REQUIRE(session.viewFor(0).clock.remainingMs == std::array<std::int64_t, 2>{290000, 290000});
    REQUIRE(session.revision() == 0);
    REQUIRE(session.submit(0, initialChoice(session.viewFor(0))).accepted);
    REQUIRE(session.viewFor(0).clock.runningPlayer == 1);
    now += std::chrono::seconds(20); session.tick();
    REQUIRE(session.viewFor(0).clock.remainingMs == std::array<std::int64_t, 2>{290000, 270000});
    const auto revision = session.revision();
    REQUIRE_FALSE(session.submit(1, Timeout{}).accepted); REQUIRE(session.revision() == revision);
    session.freezeClock(true);
    now += std::chrono::hours(1); session.tick();
    REQUIRE(session.viewFor(0).clock.remainingMs[1] == 270000);
    session.freezeClock(false);
    now += std::chrono::milliseconds(270000);
    REQUIRE_FALSE(session.submit(1, initialChoice(session.viewFor(1))).accepted);
    REQUIRE(session.viewFor(0).result == 0); REQUIRE_FALSE(session.viewFor(0).clock.running);
    REQUIRE(session.recording().at("commands").back().at("command").at("type") == 19);
    REQUIRE(replay(c, session.recording()).digest() == session.engine().digest());
}
TEST_CASE("LAN clock replenishes the turn player caps at 300 seconds and excludes automatic gates", "[alpha-v25][clock]") {
    auto c = clockContent(); MatchSession session(c, 42);
    auto now = MatchSession::Clock::time_point{};
    session.enableClock(true, [&] { return now; });
    now += std::chrono::seconds(90); session.tick();
    REQUIRE(session.submit(0, initialChoice(session.viewFor(0))).accepted);
    REQUIRE(session.submit(1, initialChoice(session.viewFor(1))).accepted);
    auto active = session.engine().state().active;
    REQUIRE(session.viewFor(active).clock.remainingMs[active] == 270000);
    REQUIRE(session.viewFor(active).clock.remainingMs[1 - active] == 210000);
    // Pass all mandatory timing windows and automatic gates until the Main body.
    for (int step = 0; step < 40 && session.engine().state().phase != Phase::Main; ++step) {
        const auto &s = session.engine().state();
        const auto actor = s.decision ? s.decision->player : s.active;
        const auto v = session.viewFor(actor);
        if (s.decision && s.decision->kind == DecisionKind::Response)
            REQUIRE(v.clock.runningPlayers[actor]);
        REQUIRE(v.actionSummary.safeAdvance);
        REQUIRE(session.submit(actor, *v.actionSummary.safeAdvance).accepted);
    }
    REQUIRE(session.engine().state().phase == Phase::Main);
    while (session.engine().state().decision) {
        const auto pending = *session.engine().state().decision;
        REQUIRE(pending.kind == DecisionKind::Response);
        REQUIRE(session.submit(pending.player, PassResponse{pending.id}).accepted);
    }
    REQUIRE(session.viewFor(active).clock.runningPlayer == active);
    now += std::chrono::seconds(7); session.tick();
    REQUIRE(session.viewFor(active).clock.remainingMs[active] == 263000);
    REQUIRE(session.viewFor(active).clock.remainingMs[1 - active] == 210000);
}
TEST_CASE("offline sessions never enable clocks and settings persist a validated built in background", "[alpha-v25][clock]") {
    MatchSession local(clockContent(), 42); local.tick();
    REQUIRE_FALSE(local.viewFor(0).clock.enabled);
    auto settings = app::Settings{}; settings.fieldBackground = "ember";
    REQUIRE(app::decodeSettings(app::encodeSettings(settings)).fieldBackground == "ember");
    auto old = app::encodeSettings(settings); old.erase("fieldBackground");
    REQUIRE(app::decodeSettings(old).fieldBackground == "arcane");
    old["fieldBackground"] = "missing"; REQUIRE_THROWS(app::decodeSettings(old));
}
TEST_CASE("clock wire updates preserve action revision and opening privacy", "[alpha-v25][clock]") {
    auto c = clockContent(); net::HostSession host(c); net::GuestSession guest(c);
    guest.receive(host.receive(net::hello(c))); host.ready();
    guest.receive(host.receive({{"type", "Ready"}, {"session", host.session()}}));
    guest.receive(host.snapshot(1)); auto revision = guest.revision();
    auto clockUpdate = host.clockUpdate();
    auto clock = net::decodeClock(clockUpdate.at("clock"));
    clock.remainingMs[1] -= 1234; ++clock.sequence;
    clockUpdate["clock"] = net::encodeClock(clock); guest.receive(clockUpdate);
    REQUIRE(guest.view().clock.remainingMs[1] == clock.remainingMs[1]);
    REQUIRE(guest.revision() == revision); REQUIRE(guest.view().opening);
    REQUIRE(guest.view().initialFormation == 0);
    REQUIRE(net::encodeClock(guest.view().clock) == clockUpdate.at("clock"));
    REQUIRE_FALSE(clockUpdate.contains("view"));
    REQUIRE_FALSE(host.submit(Timeout{}).accepted);
    REQUIRE(host.receive(guest.command(Timeout{})).at("type") == "Rejected");
}
TEST_CASE("rejected commands retain submillisecond clock accumulation", "[alpha-v25][clock]") {
    MatchSession session(clockContent(), 42);
    auto now = MatchSession::Clock::time_point{};
    session.enableClock(false, [&] { return now; });
    for (int attempt = 0; attempt < 10; ++attempt) {
        now += std::chrono::microseconds(100);
        REQUIRE_FALSE(session.submit(0, AdvancePhase{}).accepted);
    }
    REQUIRE(session.viewFor(0).clock.remainingMs == std::array<std::int64_t, 2>{299999, 299999});
    REQUIRE(session.revision() == 0);
}
