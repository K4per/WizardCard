#include "wizard/central_server.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;
using namespace wizard::central;
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
