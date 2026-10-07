#include "wizard/network.hpp"
#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <thread>
using namespace wizard;
using namespace wizard::net;
namespace {
Content content() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
}
struct Pair {
    Content c = content();
    HostSession host{c};
    GuestSession guest{c};
    Pair() {
        guest.receive(host.receive(hello(c)));
        guest.receive(host.room());
        host.ready();
        Json ready = {{"type", "Ready"}, {"session", host.session()}};
        guest.receive(host.receive(ready));
        guest.receive(host.snapshot(1));
    }
};
} // namespace
TEST_CASE("network frames survive every split and coalesced packets") {
    auto j = hello(content());
    auto b = frame(j);
    for (std::size_t split = 0; split <= b.size(); ++split) {
        Decoder d;
        auto a = d.feed(b.data(), split);
        auto rest = d.feed(b.data() + split, b.size() - split);
        a.insert(a.end(), rest.begin(), rest.end());
        REQUIRE(a.size() == 1);
        CHECK(a[0] == j);
    }
    Decoder d;
    auto twice = b;
    twice.insert(twice.end(), b.begin(), b.end());
    CHECK(d.feed(twice.data(), twice.size()).size() == 2);
}
TEST_CASE("network decoder rejects invalid length JSON and receive limits") {
    Decoder d;
    unsigned char large[] = {0, 16, 0, 1};
    CHECK_THROWS(d.feed(large, 4));
    d.clear();
    unsigned char empty[] = {0, 0, 0, 0};
    CHECK_THROWS(d.feed(empty, 4));
    d.clear();
    unsigned char bad[] = {0, 0, 0, 1, '!'};
    CHECK_THROWS(d.feed(bad, 5));
    d.clear();
    std::vector<unsigned char> huge(maxReceive + 1);
    CHECK_THROWS(d.feed(huge.data(), huge.size()));
    CHECK_THROWS(frame(Json{{"data", std::string(maxMessage, 'x')}}));
}
TEST_CASE("network exact identifiers retain values beyond double precision") {
    DecisionId id = (std::uint64_t{1} << 63) + 17;
    auto j = wireCommand(Respond{id, 1, "response", 0, id});
    CHECK(j.at("decision") == std::to_string(id));
    CHECK(j.at("link") == std::to_string(id));
    CHECK(encodeCommand(unwireCommand(j)) == encodeCommand(Respond{id, 1, "response", 0, id}));
    CHECK_THROWS(number(Json("18446744073709551616")));
    CHECK_THROWS(number(Json("-1")));
    CHECK_THROWS(number(Json(3)));
}
TEST_CASE("network handshake keeps program diagnostic and rejects content mismatch") {
    auto c = content();
    HostSession h(c);
    auto j = hello(c);
    j["program"] = "future-godot-client";
    CHECK(h.receive(j).at("type") == "Joined");
    CHECK_THROWS(h.receive(j));
    for (const char *k : {"protocol", "rules", "cards", "hash"}) {
        HostSession other(c);
        auto wrong = hello(c);
        wrong[k] = "different";
        CHECK_THROWS(other.receive(wrong));
    }
}
TEST_CASE("network guest view contains no opponent hand deck seed or digest") {
    Pair p;
    auto raw = p.host.snapshot(1);
    auto v = decodeView(raw.at("view"), p.c.catalog);
    CHECK(encodeView(v) == raw.at("view"));
    for (auto &c : v.cards) {
        CHECK_FALSE(
            (c.instance.owner == 0 && (c.instance.zone == Zone::Hand || c.instance.zone == Zone::Deck)));
    }
    CHECK_FALSE(raw.contains("seed"));
    CHECK_FALSE(raw.contains("digest"));
    for (const auto &e : v.events)
        CHECK((e.audience == -1 || e.audience == 1));
}
TEST_CASE("network authority duplicate requests do not execute twice") {
    Pair p;
    auto request = p.guest.command(Surrender{});
    auto result = p.host.receive(request);
    REQUIRE(result.at("type") == "Accepted");
    auto revision = p.host.revision();
    CHECK(p.host.receive(request) == result);
    CHECK(p.host.revision() == revision);
    auto changed = request;
    changed["request"] = "different";
    CHECK_THROWS(p.host.receive(changed));
    p.guest.receive(result);
    p.guest.receive(p.host.snapshot(1));
    CHECK(p.guest.view().result == 0);
    CHECK(replay(p.c, p.host.recording()).digest() == p.host.recording().at("finalDigest"));
}
TEST_CASE("network rejects stale revisions decisions and unauthorized commands") {
    Pair p;
    auto command = p.guest.command(AdvancePhase{p.guest.view().phaseGate});
    auto result = p.host.receive(command);
    CHECK(result.at("type") == "Rejected");
    p.guest.receive(result);
    command = p.guest.command(Surrender{});
    command["revision"] = "0";
    CHECK(p.host.receive(command).at("type") == "Rejected");
    p.guest.receive(p.host.receive(command));
    command = p.guest.command(Surrender{});
    command["decision"] = "999999";
    CHECK(p.host.receive(command).at("type") == "Rejected");
}
TEST_CASE("network resume preserves pending request after lost acknowledgement") {
    Pair p;
    auto request = p.guest.command(Surrender{});
    auto result = p.host.receive(request);
    p.host.freeze(true);
    auto resume = p.guest.resume();
    auto wrong = resume;
    wrong["credential"] = "wrong";
    CHECK_THROWS(p.host.receive(wrong));
    p.guest.receive(p.host.receive(resume));
    REQUIRE(p.guest.retry());
    CHECK(p.host.receive(*p.guest.retry()) == result);
    p.guest.receive(result);
    CHECK_FALSE(p.guest.pending());
    CHECK_FALSE(p.host.frozen());
}
TEST_CASE("network replay is available only after normal terminal result") {
    Pair p;
    Json request = {{"type", "Replay"}, {"session", p.host.session()}};
    CHECK_THROWS(p.host.receive(request));
    REQUIRE(p.host.submit(Surrender{}).accepted);
    auto r = p.host.receive(request);
    CHECK(replay(p.c, r.at("recording")).state().result == 1);
}
TEST_CASE("network room changes clear both readiness flags and host seat swaps") {
    auto c = content();
    HostSession h(c);
    h.receive(hello(c));
    h.ready();
    h.configure(false, false);
    CHECK(h.player() == 1);
    CHECK(h.room().at("ready") == Json::array({false, false}));
    PlayerDeck d;
    d.cards = c.deck;
    h.deck(d);
    h.ready();
    h.receive({{"type", "Ready"}, {"session", h.session()}});
    CHECK(h.started());
    CHECK(h.view().viewer == 1);
    CHECK(h.recording().at("statistics").at("firstPlayer") == 0);
}
TEST_CASE("network memory sessions run a complete match with matching replay") {
    Pair p;
    std::size_t steps = 0;
    while (p.host.view().result == -1 && steps++ < 3000) {
        auto hv = p.host.view();
        auto gv = p.guest.view();
        auto actor = hv.decision ? hv.viewer : (!gv.actions.empty() ? gv.viewer : hv.active);
        const auto &v = actor == hv.viewer ? hv : gv;
        auto selection = ai::choose(v, ai::Difficulty::Normal);
        REQUIRE(selection);
        if (actor == hv.viewer)
            REQUIRE(p.host.submit(selection->command).accepted);
        else {
            auto command = p.guest.command(selection->command);
            auto result = p.host.receive(command);
            REQUIRE(result.at("type") == "Accepted");
            p.guest.receive(result);
        }
        p.guest.receive(p.host.snapshot(1));
    }
    REQUIRE(p.host.view().result != -1);
    CHECK(replay(p.c, p.host.recording()).digest() == p.host.recording().at("finalDigest"));
}
TEST_CASE("network real TCP peers support settings independent progress and terminal replay") {
    auto c = content();
    auto directory = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "network-tests" / token();
    Peer host(c, directory / "host.json"), guest(c, directory / "guest.json");
    PlayerDeck d;
    d.cards = c.deck;
    unsigned short port = 0;
    for (unsigned short p = 32000; p < 32100; ++p) {
        try {
            host.host(p, d);
            port = p;
            break;
        } catch (...) {
        }
    }
    REQUIRE(port);
    guest.join("127.0.0.1", port);
    auto wait = [&](auto condition) {
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!condition() && std::chrono::steady_clock::now() < until) {
            host.tick();
            guest.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        INFO("host=" << host.notice() << " guest=" << guest.notice() << " hostRoom=" << host.room()
                     << " guestRoom=" << guest.room());
        REQUIRE(condition());
    };
    wait([&] { return host.connected() && guest.connected(); });
    host.configure(true, false);
    for (int i = 0; i < 10; ++i) {
        host.tick();
        guest.tick();
    }
    host.ready();
    guest.ready();
    wait([&] { return host.started() && guest.started(); });
    REQUIRE(guest.submit(Surrender{}).accepted);
    wait([&] { return guest.view().result != -1; });
    std::string error;
    wait([&] { return guest.save(error); });
    CHECK(replay(c, readJson(directory / "guest.json")).state().result == 0);
    bool closed = host.leave(error);
    wait([&] {
        if (!closed)
            closed = host.leave(error);
        return closed;
    });
    guest.tick();
    CHECK(guest.save(error));
}
TEST_CASE("network keeps Alpha 1.0 replay compatibility") {
    Pair p;
    auto j = p.host.recording();
    j["programVersion"] = "1.0.0-alpha";
    CHECK_NOTHROW(replay(p.c, j));
    j["programVersion"] = "unknown";
    CHECK_THROWS(replay(p.c, j));
}
TEST_CASE("network bounded event history keeps stable IDs and only consumes new events") {
    auto c = content();
    HostSession h(c);
    h.receive(hello(c));
    h.ready();
    h.receive({{"type", "Ready"}, {"session", h.session()}});
    auto v = h.view();
    v.events.resize(600);
    auto raw = encodeView(v);
    auto before = decodeView(raw, c.catalog);
    CHECK(before.eventBase == 88);
    CHECK(before.events.size() == 512);
    CHECK(raw.at("events")[0].at("id") == "89");
    v.events.push_back({"command", "new"});
    auto after = decodeView(encodeView(v), c.catalog);
    CHECK(firstNewEvent(before, after) == 511);
    CHECK(after.events.back().text == "new");
    CHECK(encodeView(after) == encodeView(v));
}
TEST_CASE("network rejects excessive JSON nesting before parsing") {
    std::string s = "{\"a\":" + std::string(70, '[') + "0" + std::string(70, ']') + "}";
    std::vector<unsigned char> b;
    auto n = static_cast<unsigned>(s.size());
    for (int shift : {24, 16, 8, 0})
        b.push_back(static_cast<unsigned char>(n >> shift));
    b.insert(b.end(), s.begin(), s.end());
    Decoder d;
    CHECK_THROWS(d.feed(b.data(), b.size()));
}
TEST_CASE("network invalid decks and frozen sessions cannot advance authoritative rules") {
    Pair p;
    auto before = p.host.recording();
    p.host.freeze(true);
    CHECK_FALSE(p.host.submit(Surrender{}).accepted);
    CHECK_FALSE(p.host.advance());
    auto command = p.guest.command(Surrender{});
    CHECK(p.host.receive(command).at("type") == "Rejected");
    CHECK(p.host.recording() == before);
    auto c = content();
    HostSession room(c);
    room.receive(hello(c));
    CHECK_THROWS(room.receive({{"type", "Deck"},
                               {"session", room.session()},
                               {"deck", {{"baseFormation", "unknown"}, {"cards", c.deck}}}}));
    CHECK_THROWS(room.receive({{"type", "Ready"}, {"session", "old-room"}}));
}
TEST_CASE("network save failure keeps the authoritative match until explicit exit") {
    auto c = content();
    auto directory = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "network-tests" / token();
    writeJson(directory / "blocked", Json{{"file", true}});
    Peer host(c, directory / "blocked" / "host.json"), guest(c, directory / "guest.json");
    PlayerDeck d;
    d.cards = c.deck;
    unsigned short port = 0;
    for (unsigned short p = 32300; p < 32400; ++p) {
        try {
            host.host(p, d);
            port = p;
            break;
        } catch (...) {
        }
    }
    REQUIRE(port);
    guest.join("127.0.0.1", port);
    auto wait = [&](auto condition) {
        auto until = std::chrono::steady_clock::now() + std::chrono::seconds(10);
        while (!condition() && std::chrono::steady_clock::now() < until) {
            host.tick();
            guest.tick();
            std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        INFO(host.notice() << " / " << guest.notice());
        REQUIRE(condition());
    };
    wait([&] { return host.connected() && guest.connected(); });
    host.configure(true, false);
    host.ready();
    guest.ready();
    wait([&] { return host.started() && guest.started(); });
    REQUIRE(host.submit(Surrender{}).accepted);
    std::string error;
    CHECK_FALSE(host.save(error));
    CHECK_FALSE(error.empty());
    CHECK_FALSE(host.leave(error));
    CHECK_FALSE(host.ended());
    CHECK(host.started());
    CHECK(host.leave(error, true));
    CHECK(host.ended());
}
