#include "wizard/central_client.hpp"
#include "wizard/central_server.hpp"
#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace wizard;

namespace {
struct Clients {
    Content content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    unsigned serial{}, seed{100};
    central::Authority authority{content, "client-test", [this] {
        return std::string(63, '0') + "123456789abcdef"[serial++ % 15];
    }, [this] { return ++seed; }};
    std::filesystem::path root = std::filesystem::path(WIZARD_SOURCE_DIR) / "build/central-client-regression";
    central::JournalStore store{root / "rooms", content};
    central::ClientRoom owner{content, root / "owner.json"}, guest{content, root / "guest.json"};
    std::map<central::Connection, central::ClientRoom *> clients{{1, &owner}, {2, &guest}, {3, &owner}, {4, &owner}};
    bool dropReplay{}, staleCommand{};
    Clients() { std::filesystem::remove_all(root); }
    ~Clients() { std::error_code ignored; std::filesystem::remove_all(root, ignored); }
    void deliver(std::vector<central::Delivery> messages) {
        for (const auto &row : messages) {
            if (dropReplay && row.message.at("type") == "ReplayChunk") continue;
            clients.at(row.connection)->receiveTransportMessage(row.message);
        }
    }
    void pump(central::Connection ownerId = 1) {
        for (unsigned round = 0; round < 64; ++round) {
            bool work = false;
            for (auto id : {ownerId, central::Connection{2}}) {
                for (auto message : clients.at(id)->drainTransport()) {
                    work = true;
                    if (staleCommand && message.at("type") == "Command") message["revision"] = "999999";
                    deliver(authority.receive(id, message));
                }
            }
            for (const auto &code : authority.roomCodes())
                if (auto write = authority.takeWrite(code)) { work = true; deliver(authority.persisted(store.write(*write))); }
            if (!work) return;
        }
        FAIL("central client transport did not quiesce");
    }
    void start() {
        owner.connect(RoomConnectOptions{}, content.defaultPlayerDeck());
        owner.transportOpened(); pump();
        RoomConnectOptions options;
        options.entry = RoomEntry::Join; options.roomCode = owner.room().at("code");
        guest.connect(options, content.defaultPlayerDeck());
        guest.transportOpened(); pump();
        owner.ready(); guest.ready(); pump();
        REQUIRE(owner.started()); REQUIRE(guest.started());
    }
};
}

TEST_CASE("central surrender and leave finishes after verified replay without a second click", "[central][client]") {
    Clients f; f.start();
    std::string error;
    REQUIRE_FALSE(f.owner.leave(error));
    f.pump();
    REQUIRE(f.owner.replayDelivered());
    REQUIRE(std::filesystem::is_regular_file(f.root / "owner.json"));
    REQUIRE(f.owner.ended());
    REQUIRE_FALSE(f.owner.closing());
    REQUIRE(f.guest.view().result == 1);
}

TEST_CASE("central rejected surrender lets the player try leaving again", "[central][client]") {
    Clients f; f.start();
    f.staleCommand = true;
    std::string error;
    REQUIRE_FALSE(f.owner.leave(error));
    f.pump();
    REQUIRE_FALSE(f.owner.closing());
    REQUIRE_FALSE(f.owner.ended());
    f.staleCommand = false;
    REQUIRE_FALSE(f.owner.leave(error));
    f.pump();
    REQUIRE(f.owner.ended());
    REQUIRE(f.owner.replayDelivered());
}

TEST_CASE("central replay download resumes after the first chunk is lost", "[central][client]") {
    Clients f; f.start();
    f.dropReplay = true;
    REQUIRE(f.owner.submit(Surrender{}).accepted);
    f.pump();
    REQUIRE_FALSE(f.owner.replayDelivered());
    f.owner.transportLost("lost replay response");
    f.deliver(f.authority.disconnect(1));
    f.dropReplay = false;
    f.owner.transportOpened(); f.pump(3);
    REQUIRE(f.owner.connected());
    REQUIRE(f.owner.replayDelivered());
}

TEST_CASE("central reconnect retries while the old socket is still bound", "[central][client]") {
    Clients f; f.start();
    f.owner.transportLost("blackholed socket");
    f.owner.transportOpened(); f.pump(3);
    REQUIRE_FALSE(f.owner.ended());
    REQUIRE_FALSE(f.owner.transportIsOpen());
    f.deliver(f.authority.disconnect(1));
    f.owner.transportOpened(); f.pump(4);
    REQUIRE(f.owner.connected());
    REQUIRE_FALSE(f.owner.ended());
}

TEST_CASE("central client creates and joins through an injected transport and resumes the same seat", "[central][client]") {
    auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    unsigned serial = 0, seed = 100;
    central::Authority authority(content, "test-instance", [&] {
        static constexpr char hex[] = "0123456789abcdef";
        std::string value(63, '0');
        value.push_back(hex[++serial % 16]);
        return value;
    },
                                 [&] { return ++seed; });
    const auto path = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "central-client";
    central::ClientRoom owner(content, path / "owner.json"), guest(content, path / "guest.json");
    std::map<central::Connection, central::ClientRoom *> clients{{1, &owner}, {2, &guest}, {3, &owner}};
    const auto pump = [&](central::ClientRoom &client, central::Connection id) {
        for (unsigned round = 0; round < 32; ++round) {
            auto outgoing = client.drainTransport();
            if (outgoing.empty()) return;
            for (const auto &message : outgoing)
                for (const auto &delivery : authority.receive(id, message))
                    clients.at(delivery.connection)->receiveTransportMessage(delivery.message);
        }
        FAIL("client transport did not quiesce");
    };

    RoomConnectOptions create;
    create.endpoint = {"localhost", 24443, "test-ca.crt"};
    create.entry = RoomEntry::Create;
    owner.connect(create, content.defaultPlayerDeck());
    REQUIRE(owner.endpoint().hostname == "localhost");
    owner.transportOpened();
    pump(owner, 1);
    REQUIRE(owner.connected());
    REQUIRE(owner.isHost());
    const auto code = owner.room().at("code").get<std::string>();

    RoomConnectOptions join = create;
    join.entry = RoomEntry::Join;
    join.roomCode = code;
    guest.connect(join, content.defaultPlayerDeck());
    guest.transportOpened();
    pump(guest, 2);
    REQUIRE(guest.connected());
    REQUIRE_FALSE(guest.isHost());
    REQUIRE(guest.room().at("code") == code);

    owner.ready();
    pump(owner, 1);
    guest.ready();
    pump(guest, 2);
    REQUIRE(owner.started());
    REQUIRE(guest.started());
    REQUIRE(owner.view().viewer == 0);
    REQUIRE(guest.view().viewer == 1);

    const auto submitted = owner.submit(Surrender{});
    REQUIRE(submitted.accepted);
    REQUIRE(submitted.pending);
    pump(owner, 1);
    REQUIRE(owner.view().result == 1);
    REQUIRE(guest.view().result == 1);

    owner.transportLost("test disconnect");
    for (const auto &delivery : authority.disconnect(1))
        clients.at(delivery.connection)->receiveTransportMessage(delivery.message);
    REQUIRE_FALSE(owner.connected());
    owner.reconnect();
    owner.transportOpened();
    pump(owner, 3);
    REQUIRE(owner.connected());
    REQUIRE(owner.room().at("code") == code);
    REQUIRE(owner.takeReset());
}

TEST_CASE("central client resends entry when its first create request never reached the server", "[central][client]") {
    auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260");
    unsigned serial = 0;
    central::Authority authority(content, "entry-instance", [&] {
        return std::string(63, '0') + static_cast<char>('1' + serial++);
    }, [] { return 42u; });
    central::ClientRoom client(content, std::filesystem::path(WIZARD_SOURCE_DIR) / "build/entry-retry.json");
    client.connect(RoomConnectOptions{}, content.defaultPlayerDeck());
    client.transportOpened();
    for (const auto &message : client.drainTransport())
        for (const auto &delivery : authority.receive(1, message)) client.receiveTransportMessage(delivery.message);
    auto lost = client.drainTransport();
    REQUIRE(lost.size() == 1);
    REQUIRE(lost.front().at("type") == "CreateRoom");
    authority.disconnect(1);
    client.transportLost("entry request lost");
    client.transportOpened();
    for (unsigned round = 0; round < 16; ++round) {
        auto messages = client.drainTransport();
        if (messages.empty()) break;
        for (const auto &message : messages)
            for (const auto &delivery : authority.receive(2, message)) client.receiveTransportMessage(delivery.message);
    }
    REQUIRE(client.connected());
    REQUIRE(authority.roomCount() == 1);
}

TEST_CASE("central bridge stops the external transport after a room abort", "[central][client]") {
    const auto assets = std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260";
    auto content = loadContent(assets);
    central::Authority authority(content, "abort-instance", [] { return std::string(64, '1'); }, [] { return 42u; });
    bridge::LocalSession bridge({}, central::createClientRoom);
    const auto directory = std::filesystem::path(WIZARD_SOURCE_DIR) / "build/central-bridge-abort";
    REQUIRE(bridge.initialize(assets, directory).at("ok") == true);
    REQUIRE(bridge.networkRequest({{"operation", "central_create"}, {"hostname", "localhost"}}).at("ok") == true);
    auto messages = bridge.networkTransportOpened().at("data").at("messages");
    for (unsigned round = 0; round < 8 && !messages.empty(); ++round) {
        auto next = Json::array();
        for (const auto &message : messages)
            for (const auto &delivery : authority.receive(1, message)) {
                auto outgoing = bridge.networkTransportMessage(delivery.message).at("data").at("messages");
                for (auto &row : outgoing) next.push_back(std::move(row));
            }
        messages = std::move(next);
    }
    REQUIRE(bridge.pollNetwork().at("data").at("connected") == true);
    const auto code = authority.roomCodes().front();
    for (const auto &delivery : authority.abortRoom(code, "server_shutdown"))
        REQUIRE(bridge.networkTransportMessage(delivery.message).at("ok") == true);
    REQUIRE(bridge.networkTransportState().at("data").value("ended", false));
    std::error_code ignored;
    std::filesystem::remove_all(directory, ignored);
}
