#include "wizard/central_server.hpp"
#include "wizard/central_journal.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <atomic>
#include <future>
#include <thread>

using namespace wizard;
using namespace wizard::central;

TEST_CASE("central admission to a busy room leaves the registry available", "[central][concurrency]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    unsigned serial{}, seed{41};
    std::atomic<bool> hold{false}, entered{false}, release{false};
    std::thread::id blockedThread;
    Authority server(content, "registry-test", [&] { return "token-" + std::to_string(++serial); },
        [&] { return ++seed; }, {}, [&] {
            if (hold && std::this_thread::get_id() == blockedThread) {
                entered = true;
                while (!release) std::this_thread::yield();
            }
            return std::chrono::steady_clock::time_point{};
        });
    server.receive(1, central::hello(content));
    server.receive(1, {{"type", "CreateRoom"}, {"deck", encodeDeck(content.defaultPlayerDeck())}});
    server.receive(2, central::hello(content));
    const auto code = server.roomCodes().front();
    auto busy = std::async(std::launch::async, [&] {
        blockedThread = std::this_thread::get_id(); hold = true; server.tickRoom(code);
    });
    while (!entered) std::this_thread::yield();
    auto admission = std::async(std::launch::async, [&] {
        return server.receive(2, {{"type", "JoinRoom"}, {"code", code}, {"deck", encodeDeck(content.defaultPlayerDeck())}});
    });
    admission.wait_for(std::chrono::milliseconds(30));
    auto registry = std::async(std::launch::async, [&] { return server.route(1); });
    const bool responsive = registry.wait_for(std::chrono::milliseconds(200)) == std::future_status::ready;
    release = true;
    busy.get(); admission.get();
    REQUIRE(registry.get() == code);
    REQUIRE(responsive);
}

TEST_CASE("central waiting guest churn retains only current resume credentials", "[central][credentials]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    unsigned serial{}, seed{41};
    auto now = std::chrono::steady_clock::time_point{};
    Authority server(content, "churn-test", [&] { return "token-" + std::to_string(++serial); },
        [&] { return ++seed; }, {}, [&] { return now; });
    server.receive(1, central::hello(content));
    server.receive(1, {{"type", "CreateRoom"}, {"deck", encodeDeck(content.defaultPlayerDeck())}});
    const auto code = server.roomCodes().front();
    for (unsigned index = 0; index < 20; ++index) {
        const Connection id = index + 2;
        server.receive(id, central::hello(content));
        server.receive(id, {{"type", "JoinRoom"}, {"code", code}, {"deck", encodeDeck(content.defaultPlayerDeck())}});
        REQUIRE(server.credentialCount() == 2);
        if (index % 2 == 0) server.receive(id, {{"type", "Leave"}});
        server.disconnect(id);
        if (index % 2) { now += std::chrono::seconds(60); server.tickRoom(code); }
        REQUIRE(server.credentialCount() == 1);
    }
}
namespace {
struct Fixture {
    Content content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    std::chrono::steady_clock::time_point now{};
    unsigned serial{}, seed{41};
    Authority server{content, "instance-a", [this] { return "token-" + std::to_string(++serial); },
                     [this] { return ++seed; }, {}, [this] { return now; }};
    std::string code;
    Json find(const std::vector<Delivery> &messages, Connection id, const std::string &type) {
        auto found = std::find_if(messages.begin(), messages.end(), [&](const auto &m) {
            return m.connection == id && m.message.at("type") == type;
        });
        INFO("missing " << type << " for " << id);
        REQUIRE(found != messages.end());
        return found->message;
    }
    Json send(Connection id, const Json &message, const std::string &type) {
        return find(server.receive(id, message), id, type);
    }
    Json welcome(Connection id) { return send(id, central::hello(content), "Welcome"); }
    void room() {
        welcome(1);
        auto joined = send(1, {{"type", "CreateRoom"}, {"deck", encodeDeck(content.defaultPlayerDeck())}}, "RoomJoined");
        code = joined.at("room").at("code");
        welcome(2);
        send(2, {{"type", "JoinRoom"}, {"code", code}, {"deck", encodeDeck(content.defaultPlayerDeck())}}, "RoomJoined");
    }
    Json status(Connection id) { return send(id, {{"type", "Sync"}}, "Room"); }
    void start(bool ownerFirst = true) {
        room();
        send(1, {{"type", "Configure"}, {"ownerFirst", ownerFirst}, {"automatic", false}}, "Room");
        send(1, {{"type", "Ready"}}, "Room");
        send(2, {{"type", "Ready"}}, "StateSnapshot");
    }
    Json snapshot(Connection id) { return send(id, {{"type", "Sync"}}, "StateSnapshot"); }
    Json command(Connection id, const Command &command, std::uint64_t sequence = 1) {
        const auto state = snapshot(id);
        const auto view = net::decodeView(state.at("view"), content.catalog);
        return {{"type", "Command"}, {"sequence", std::to_string(sequence)}, {"request", "request-" + std::to_string(sequence)},
                {"revision", state.at("revision")}, {"decision", std::to_string(view.decision ? view.decision->id : 0)},
                {"command", net::wireCommand(command)}};
    }
};
}
TEST_CASE("central authority requires handshake and caps room admission", "[central][rooms]") {
    Fixture f;
    REQUIRE(f.send(1, {{"type", "CreateRoom"}}, "Error").at("code") == "handshake_required");
    f.room();
    f.welcome(3);
    REQUIRE(f.send(3, {{"type", "JoinRoom"}, {"code", f.code}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}}, "Error").at("code") == "room_full");
    REQUIRE(f.status(1).at("seat") == 0);
    REQUIRE(f.status(2).at("seat") == 1);
    AuthorityLimits limits; limits.rooms = 1;
    unsigned serial{};
    Authority one(f.content, "one-room", [&] { return "token-" + std::to_string(++serial); }, [] { return 42u; }, limits);
    one.receive(1, central::hello(f.content));
    one.receive(2, central::hello(f.content));
    const Json create = {{"type", "CreateRoom"}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}};
    REQUIRE(f.find(one.receive(1, create), 1, "RoomJoined").at("room").at("seat") == 0);
    REQUIRE(f.find(one.receive(2, create), 2, "Error").at("code") == "capacity");
    REQUIRE(one.roomCount() == 1);
}
TEST_CASE("central authority separates room ownership from private player projection", "[central][rooms]") {
    Fixture f;
    f.start(false);
    REQUIRE(f.status(1).at("owner") == true);
    REQUIRE(f.status(2).at("owner") == false);
    REQUIRE(f.send(2, {{"type", "Configure"}, {"ownerFirst", true}}, "Error").at("code") == "owner_only");
    for (Connection id : {1, 2}) {
        const auto state = f.snapshot(id);
        const auto view = net::decodeView(state.at("view"), f.content.catalog);
        REQUIRE(view.viewer == static_cast<int>(id - 1));
        for (const auto &card : view.cards)
            REQUIRE_FALSE((card.instance.owner != view.viewer &&
                           (card.instance.zone == Zone::Hand || card.instance.zone == Zone::Deck)));
        REQUIRE_FALSE(state.contains("seed"));
        REQUIRE_FALSE(f.status(id).contains("deck"));
    }
}
TEST_CASE("central authority changes invalidate readiness and obey configured first seat", "[central][rooms]") {
    Fixture f;
    f.room();
    f.send(1, {{"type", "Ready"}}, "Room");
    f.send(2, {{"type", "Deck"}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}}, "Room");
    REQUIRE(f.status(1).at("ready") == Json::array({false, false}));
    f.send(1, {{"type", "Configure"}, {"ownerFirst", false}, {"automatic", false}}, "Room");
    f.send(1, {{"type", "Ready"}}, "Room");
    f.send(2, {{"type", "Ready"}}, "StateSnapshot");
    REQUIRE(f.status(1).at("phase") == "playing");
    for (Connection id : {1, 2}) {
        const auto view = net::decodeView(f.snapshot(id).at("view"), f.content.catalog);
        const auto formation = std::find_if(view.cards.begin(), view.cards.end(), [&](const auto &card) {
            return card.instance.owner == view.viewer && card.instance.zone == Zone::SideDeck && card.definition.level == 1;
        });
        REQUIRE(formation != view.cards.end());
        f.send(id, f.command(id, SelectInitialFormation{formation->instance.id}), "Accepted");
    }
    REQUIRE(net::decodeView(f.snapshot(1).at("view"), f.content.catalog).active == 1);
}
TEST_CASE("central rooms accept commands only for the bound seat and deliver terminal projections", "[central][rooms]") {
    Fixture f;
    f.start();
    auto request = f.command(2, Surrender{});
    request["actor"] = 0;
    REQUIRE(f.send(2, request, "Rejected").at("code") == "wrong_player");
    REQUIRE(f.status(1).at("phase") == "playing");
    request = f.command(2, Surrender{}, 2);
    f.send(2, request, "Accepted");
    REQUIRE(f.status(1).at("phase") == "completed");
    REQUIRE(net::decodeView(f.snapshot(1).at("view"), f.content.catalog).result == 0);
}
TEST_CASE("central rooms expose action queries and only viewer-local incremental events", "[central][rooms]") {
    Fixture f;
    f.start();
    const auto state = f.snapshot(1);
    ActionIntent intent;
    auto options = f.send(1, {{"type", "ActionQuery"}, {"generation", "8"}, {"revision", state.at("revision")},
                              {"request", "q1"}, {"decision", "0"}, {"intent", net::encodeIntent(intent)}}, "ActionOptions");
    REQUIRE_FALSE(net::decodeOptions(options.at("options")).options.empty());
    REQUIRE(options.at("viewer") == 0);
    auto wrong = f.send(1, {{"type", "ActionQuery"}, {"generation", "8"}, {"revision", "999"},
                            {"request", "q2"}, {"decision", "0"}, {"intent", net::encodeIntent(intent)}}, "ActionOptions");
    REQUIRE(net::decodeOptions(wrong.at("options")).error == "stale_query");
    for (Connection id : {1, 2}) {
        const auto messages = f.server.receive(id, {{"type", "Sync"}});
        const auto event = f.find(messages, id, "EventBatch");
        for (const auto &row : net::decodeEvents(event.at("batch")).events)
            REQUIRE((row.audience == -1 || row.audience == static_cast<int>(id - 1)));
    }
}
TEST_CASE("central rooms isolate revisions and refuse invalid decks without reserving a seat", "[central][rooms]") {
    Fixture f;
    f.start();
    const auto first = f.code;
    f.welcome(3);
    auto invalid = encodeDeck(f.content.defaultPlayerDeck());
    invalid["cards"] = Json::array();
    f.send(3, {{"type", "CreateRoom"}, {"deck", invalid}}, "Error");
    auto newRoom = f.send(3, {{"type", "CreateRoom"}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}}, "RoomJoined");
    REQUIRE(newRoom.at("room").at("code") != first);
    f.send(2, f.command(2, Surrender{}), "Accepted");
    REQUIRE(f.status(3).at("phase") == "waiting");
    REQUIRE(f.server.roomCount() == 2);
}
TEST_CASE("central resume recovers a lost room joined reply using the welcome token", "[central][reconnect]") {
    Fixture f;
    const auto welcome = f.welcome(1);
    f.send(1, {{"type", "CreateRoom"}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}}, "RoomJoined");
    f.server.disconnect(1);
    f.welcome(11);
    auto resumed = f.send(11, {{"type", "Resume"}, {"token", welcome.at("token")}, {"instanceId", "instance-a"}}, "RoomJoined");
    REQUIRE(resumed.at("room").at("seat") == 0);
    REQUIRE(f.server.roomCount() == 1);
    REQUIRE(f.status(11).at("owner") == true);
    f.welcome(12);
    REQUIRE(f.send(12, {{"type", "Resume"}, {"token", welcome.at("token")}, {"instanceId", "instance-a"}}, "Error").at("code") == "already_connected");
}
TEST_CASE("central commands deduplicate exact retries and reject changed payloads", "[central][reconnect]") {
    Fixture f;
    f.start();
    const auto view = net::decodeView(f.snapshot(1).at("view"), f.content.catalog);
    const auto initial = std::find_if(view.cards.begin(), view.cards.end(), [](const auto &c) {
        return c.instance.owner == 0 && c.instance.zone == Zone::SideDeck && c.definition.level == 1;
    });
    REQUIRE(initial != view.cards.end());
    auto request = f.command(1, SelectInitialFormation{initial->instance.id});
    auto accepted = f.send(1, request, "Accepted");
    REQUIRE(f.send(1, request, "Accepted").at("revision") == accepted.at("revision"));
    auto changed = request;
    changed["command"] = net::wireCommand(Surrender{});
    REQUIRE(f.send(1, changed, "Error").at("code") == "sequence_conflict");
    REQUIRE(f.snapshot(1).at("revision") == accepted.at("revision"));
    request = f.command(1, Surrender{}, 3);
    REQUIRE(f.send(1, request, "Error").at("code") == "sequence_gap");
}
TEST_CASE("central reconnect freezes clocks and expires at sixty seconds without a winner", "[central][reconnect]") {
    Fixture f;
    auto welcome = f.welcome(2);
    f.start();
    const auto clock = net::decodeView(f.snapshot(1).at("view"), f.content.catalog).clock.remainingMs;
    f.server.disconnect(2);
    f.now += std::chrono::seconds(59);
    f.server.tick();
    REQUIRE(f.status(1).at("reconnecting") == true);
    REQUIRE(net::decodeView(f.snapshot(1).at("view"), f.content.catalog).clock.remainingMs == clock);
    f.welcome(22);
    f.send(22, {{"type", "Resume"}, {"token", welcome.at("token")}, {"instanceId", "instance-a"}, {"code", f.code}}, "RoomJoined");
    REQUIRE(f.status(1).at("reconnecting") == false);
    f.server.disconnect(22);
    f.now += std::chrono::seconds(60);
    f.server.tick();
    REQUIRE(f.status(1).at("phase") == "aborted");
    REQUIRE(f.status(1).at("abortReason") == "reconnect_timeout");
    REQUIRE(net::decodeView(f.snapshot(1).at("view"), f.content.catalog).result == -1);
    f.welcome(222);
    f.send(222, {{"type", "Resume"}, {"token", welcome.at("token")}, {"instanceId", "instance-a"}}, "RoomJoined");
    REQUIRE(f.status(222).at("phase") == "aborted");
}
TEST_CASE("central pending reconnect rejects stale instances and waits for both players", "[central][reconnect]") {
    Fixture f;
    auto w1 = f.welcome(1), w2 = f.welcome(2);
    f.start();
    f.server.disconnect(1);
    f.server.disconnect(2);
    f.welcome(11);
    REQUIRE(f.send(11, {{"type", "Resume"}, {"token", w1.at("token")}, {"instanceId", "old-instance"}}, "Error").at("code") == "resume_invalid");
    f.send(11, {{"type", "Resume"}, {"token", w1.at("token")}, {"instanceId", "instance-a"}}, "RoomJoined");
    REQUIRE(f.status(11).at("reconnecting") == true);
    f.welcome(22);
    f.send(22, {{"type", "Resume"}, {"token", w2.at("token")}, {"instanceId", "instance-a"}}, "RoomJoined");
    REQUIRE(f.status(11).at("reconnecting") == false);
}
TEST_CASE("central room lifetime abort never executes a rule timeout or declares a winner", "[central][reconnect]") {
    Fixture f;
    f.start();
    f.now += std::chrono::hours(4);
    f.server.tick();
    REQUIRE(f.status(1).at("phase") == "aborted");
    REQUIRE(net::decodeView(f.snapshot(1).at("view"), f.content.catalog).result == -1);
    REQUIRE_FALSE(net::decodeView(f.snapshot(1).at("view"), f.content.catalog).clock.running);
}
TEST_CASE("central waiting guest departure frees its seat and owner departure closes the room", "[central][reconnect]") {
    Fixture f;
    f.room();
    f.send(2, {{"type", "Leave"}}, "Left");
    REQUIRE(f.status(1).at("joined") == false);
    f.welcome(3);
    f.send(3, {{"type", "JoinRoom"}, {"code", f.code}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}}, "RoomJoined");
    f.send(1, {{"type", "Leave"}}, "Left");
    REQUIRE(f.status(3).at("phase") == "aborted");
    REQUIRE(f.status(3).at("abortReason") == "owner_left");
}
TEST_CASE("central cumulative command and event budget aborts without changing rules result", "[central][storage]") {
    Fixture f;
    AuthorityLimits limits;
    limits.replayBytes = 1;
    Authority limited(f.content, "instance-budget", [&] { return "budget-token" + std::to_string(++f.serial); },
                      [&] { return ++f.seed; }, limits, [&] { return f.now; });
    auto welcome = limited.receive(1, central::hello(f.content));
    REQUIRE(welcome.front().message.at("type") == "Welcome");
    limited.receive(1, {{"type", "CreateRoom"}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}});
    auto joined = limited.receive(2, central::hello(f.content));
    REQUIRE(joined.front().message.at("type") == "Welcome");
    const auto code = limited.roomCodes().front();
    limited.receive(2, {{"type", "JoinRoom"}, {"code", code}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}});
    limited.receive(1, {{"type", "Ready"}}); limited.receive(2, {{"type", "Ready"}});
    const auto status = limited.receive(1, {{"type", "Sync"}});
    auto room = std::find_if(status.begin(), status.end(), [](const auto &d) { return d.message.at("type") == "Room"; });
    REQUIRE(room != status.end());
    REQUIRE(room->message.at("phase") == "aborted");
    auto state = std::find_if(status.begin(), status.end(), [](const auto &d) { return d.message.at("type") == "StateSnapshot"; });
    REQUIRE(net::decodeView(state->message.at("view"), f.content.catalog).result == -1);
}
TEST_CASE("central storage failure freezes authority and exact retries survive independent reconnect expiry", "[central][storage]") {
    Fixture f;
    f.start();
    auto write = f.server.takeWrite(f.code);
    REQUIRE(write);
    auto failed = JournalResult{f.code, "disk unavailable", write->serial, write->offset, {}};
    f.server.persisted(failed);
    REQUIRE(f.status(1).at("storageBlocked") == true);
    REQUIRE_FALSE(f.server.takeWrite(f.code));
    f.now += std::chrono::seconds(1);
    auto retry = f.server.takeWrite(f.code);
    REQUIRE(retry);
    REQUIRE(retry->offset == write->offset);
    REQUIRE(*retry->bytes == *write->bytes);
    f.server.disconnect(2);
    f.now += std::chrono::seconds(60);
    f.server.tick();
    f.server.persisted({f.code, {}, retry->serial, retry->offset + retry->bytes->size(), {}});
    REQUIRE(f.status(1).at("storageBlocked") == false);
    REQUIRE(f.status(1).at("phase") == "aborted");
    REQUIRE(f.status(1).at("abortReason") == "reconnect_timeout");
}
TEST_CASE("central delayed persistence cannot stop another room from processing commands", "[central][storage]") {
    Fixture f;
    f.start();
    const auto stalledRoom = f.code;
    auto pending = f.server.takeWrite(stalledRoom);
    REQUIRE(pending);
    f.welcome(3); f.welcome(4);
    auto joined = f.send(3, {{"type", "CreateRoom"}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}}, "RoomJoined");
    auto second = joined.at("room").at("code").get<std::string>();
    f.send(4, {{"type", "JoinRoom"}, {"code", second}, {"deck", encodeDeck(f.content.defaultPlayerDeck())}}, "RoomJoined");
    f.send(3, {{"type", "Ready"}}, "Room"); f.send(4, {{"type", "Ready"}}, "StateSnapshot");
    f.send(4, f.command(4, Surrender{}), "Accepted");
    REQUIRE(f.status(3).at("phase") == "completed");
    REQUIRE(f.status(1).at("phase") == "playing");
    REQUIRE_FALSE(f.server.takeWrite(stalledRoom));
}
TEST_CASE("central replay transfer requires persisted normal completion and preserves UTF8 boundaries", "[central][storage]") {
    Fixture f;
    f.start();
    REQUIRE(f.send(1, {{"type", "Replay"}}, "Error").at("code") == "replay_not_ready");
    f.send(2, f.command(2, Surrender{}), "Accepted");
    REQUIRE(f.send(1, {{"type", "Replay"}}, "Error").at("code") == "replay_not_ready");
    const auto directory = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "central-transfer" / net::token();
    JournalStore store(directory, f.content);
    for (unsigned i = 0; i < 8; ++i) {
        auto write = f.server.takeWrite(f.code);
        if (!write) break;
        auto result = store.write(*write);
        REQUIRE(result.error.empty());
        f.server.persisted(result);
    }
    std::string bytes;
    for (unsigned i = 0; i < 1024; ++i) {
        auto chunk = f.send(1, {{"type", "Replay"}, {"offset", std::to_string(bytes.size())}}, "ReplayChunk");
        REQUIRE(net::number(chunk.at("offset")) == bytes.size());
        bytes += chunk.at("data").get<std::string>();
        if (chunk.at("complete").get<bool>()) break;
    }
    auto record = Json::parse(bytes);
    REQUIRE(record.at("statistics").at("result") == 0);
    REQUIRE(record.at("commands").size() == 1);
    REQUIRE(replay(f.content, record).digest() == record.at("finalDigest"));
    std::error_code ignored; std::filesystem::remove_all(directory, ignored);
    Fixture aborted;
    aborted.start(); aborted.server.abortRoom(aborted.code, "server_shutdown");
    REQUIRE(aborted.send(1, {{"type", "Replay"}}, "Error").at("code") == "replay_not_ready");
}
