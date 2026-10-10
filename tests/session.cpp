#include "wizard/application.hpp"
#include "wizard/network.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>

using namespace wizard;
TEST_CASE("local session interface preserves revision projection and rejected command atomicity") {
    auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    MatchSession local(content, 42);
    Session &session = local;
    const auto initial = session.viewFor(0);
    REQUIRE(session.started());
    REQUIRE_FALSE(session.blocked());
    REQUIRE(session.revision() == 0);
    REQUIRE_FALSE(session.submit(initial.active, AdvancePhase{initial.phaseGate + 1}).accepted);
    REQUIRE(session.revision() == 0);
    REQUIRE(session.submit(initial.active, AdvancePhase{initial.phaseGate}).accepted);
    REQUIRE(session.revision() == 1);
    REQUIRE(replay(content, local.recording()).digest() == local.engine().digest());
}
TEST_CASE("injected room sessions preserve pending authority privacy and polling while settings are open") {
    auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    const auto directory = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "session-tests" / net::token();
    app::Application host(content, directory / "host", {}, net::createRoom);
    app::Application guest(content, directory / "guest", {}, net::createRoom);
    PlayerDeck deck; deck.cards = content.deck;
    std::string error;
    unsigned short port{};
    for (unsigned short candidate = 32500; candidate < 32600; ++candidate)
        if (host.startNetwork(true, "", candidate, deck, error)) { port = candidate; break; }
    INFO(error);
    REQUIRE(port != 0);
    REQUIRE(guest.startNetwork(false, "127.0.0.1", port, deck, error));
    auto wait = [&](auto predicate) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!predicate() && std::chrono::steady_clock::now() < deadline) {
            host.tickNetwork(); guest.tickNetwork();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        INFO(host.notice() << " / " << guest.notice());
        REQUIRE(predicate());
    };
    wait([&] { return host.network()->connected() && guest.network()->connected(); });
    host.network()->configure(true, false);
    host.network()->ready(); guest.network()->ready();
    wait([&] { return host.hasMatch() && guest.hasMatch(); });
    Session &guestSession = *guest.network();
    const auto view = guestSession.viewFor(1);
    REQUIRE(view.viewer == 1);
    for (const auto &card : view.cards)
        REQUIRE_FALSE((card.instance.owner == 0 && (card.instance.zone == Zone::Hand || card.instance.zone == Zone::Deck)));
    REQUIRE_FALSE(guestSession.submit(0, Surrender{}).accepted);
    const auto revision = guestSession.revision();
    host.openSettings();
    REQUIRE(host.paused());
    const auto pending = guestSession.submit(1, Surrender{});
    REQUIRE(pending.pending);
    REQUIRE(pending.accepted); // Transport queued it; pending still excludes authority acceptance.
    REQUIRE(guestSession.revision() == revision);
    wait([&] { return guest.viewFor(1).result != -1 && host.viewFor(0).result != -1; });
    REQUIRE(guestSession.revision() > revision);
    REQUIRE(host.viewFor(0).result == 0);
    REQUIRE(host.page() == app::Page::Settings);
    REQUIRE(host.saveReplay(error));
    wait([&] { return guest.saveReplay(error); });
    REQUIRE(replay(content, readJson(host.replayPath())).digest() == replay(content, readJson(guest.replayPath())).digest());
    host.network()->stop(); guest.network()->stop();
}
