#include "wizard/central_server.hpp"
#include <iomanip>
#include <random>
#include <sstream>

namespace wizard::central {
namespace {
using Clock = std::chrono::steady_clock;
std::string phaseName(RoomPhase phase) {
    switch (phase) {
    case RoomPhase::Waiting: return "waiting";
    case RoomPhase::Playing: return "playing";
    case RoomPhase::Completed: return "completed";
    case RoomPhase::Aborted: return "aborted";
    }
    return "aborted";
}
}

Authority::Authority(Content content, std::string instanceId, TokenSource tokenSource,
                     SeedSource seedSource, std::filesystem::path dataDirectory, TimeSource now)
    : content_(std::move(content)), instanceId_(std::move(instanceId)), tokenSource_(std::move(tokenSource)),
      seedSource_(std::move(seedSource)), dataDirectory_(std::move(dataDirectory)), now_(std::move(now)) {
    if (!tokenSource_ || !seedSource_) throw std::invalid_argument("authority requires secure random sources");
    if (!now_) now_ = [] { return Clock::now(); };
}
std::chrono::steady_clock::time_point Authority::currentTime() const { return now_(); }
std::string Authority::newToken() { return tokenSource_(); }
std::uint32_t Authority::newSeed() { return seedSource_(); }
std::string Authority::newCode() {
    static constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    std::string code;
    do {
        code.clear();
        for (int i = 0; i < 8; ++i) code.push_back(alphabet[newSeed() % (sizeof(alphabet) - 1)]);
    } while (rooms_.count(code));
    return code;
}
std::size_t Authority::roomCount() const {
    std::lock_guard lock(mutex_);
    return rooms_.size();
}
Authority::Room &Authority::roomFor(const Client &client) {
    auto found = rooms_.find(client.room);
    if (found == rooms_.end()) throw std::runtime_error("房间不存在");
    return found->second;
}
const Authority::Room &Authority::roomFor(const Client &client) const {
    auto found = rooms_.find(client.room);
    if (found == rooms_.end()) throw std::runtime_error("房间不存在");
    return found->second;
}
Json Authority::roomJson(const Room &room, int seat) const {
    Json ready = Json::array();
    for (bool value : room.ready) ready.push_back(value);
    return {{"type", "Room"}, {"session", room.code}, {"code", room.code}, {"owner", seat == 0},
            {"seat", seat}, {"joined", room.connections[1] != 0}, {"ready", ready},
            {"ownerFirst", room.ownerFirst}, {"automatic", room.automatic},
            {"phase", phaseName(room.phase)}, {"aborted", room.phase == RoomPhase::Aborted},
            {"abortReason", room.abortReason}};
}
Json Authority::snapshot(const Room &room, int seat) const {
    if (!room.match) return roomJson(room, seat);
    return {{"type", "StateSnapshot"}, {"session", room.code}, {"code", room.code}, {"phase", phaseName(room.phase)},
            {"revision", std::to_string(room.match->revision())}, {"technicalPaused", room.match->technicalPaused()},
            {"view", net::encodeView(room.match->viewFor(seat))}, {"clock", net::encodeClock(room.match->clockView())}};
}
std::vector<Delivery> Authority::roomSnapshot(const Room &room, bool state) const {
    std::vector<Delivery> result;
    for (int seat = 0; seat < 2; ++seat) {
        if (!room.connections[seat]) continue;
        result.push_back({room.connections[seat], roomJson(room, seat)});
        if (state && room.match) {
            result.push_back({room.connections[seat], snapshot(room, seat)});
            result.push_back({room.connections[seat], {{"type", "EventBatch"}, {"session", room.code},
                {"revision", std::to_string(room.match->revision())},
                {"batch", net::encodeEvents(room.match->eventsSince(seat, 0, 0))}}});
        }
    }
    return result;
}
void Authority::begin(Room &room) {
    MatchConfig config;
    config.players = room.decks;
    const auto firstSeat = [&config](std::uint32_t seed) {
        std::uint32_t rng = seed ? seed : 0x9e3779b9u;
        const auto bounded = [&](std::uint32_t n) {
            const auto threshold = (std::uint32_t{0} - n) % n;
            std::uint32_t value;
            do { rng ^= rng << 13; rng ^= rng >> 17; rng ^= rng << 5; value = rng; } while (value < threshold);
            return value % n;
        };
        for (const auto &deck : config.players)
            for (std::size_t size = deck.cards.size(); size > 1; --size) (void)bounded(static_cast<std::uint32_t>(size));
        return bounded(2);
    };
    bool found = false;
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        room.seed = newSeed();
        if (firstSeat(room.seed) == (room.ownerFirst ? 0u : 1u)) { found = true; break; }
    }
    if (!found) throw std::runtime_error("无法生成指定先后手种子");
    config.seed = room.seed;
    room.match = std::make_unique<MatchSession>(content_, config);
    room.match->enableClock(room.automatic, now_);
    room.phase = RoomPhase::Playing;
}
void Authority::abort(Room &room, const std::string &reason) {
    room.phase = RoomPhase::Aborted;
    room.abortReason = reason;
    if (room.match) room.match->freezeClock(true);
}
std::vector<Delivery> Authority::receive(Connection id, const Json &message) {
    std::lock_guard lock(mutex_);
    try { return receiveLocked(id, message); }
    catch (const std::exception &e) { return {{id, error("invalid_request", e.what())}}; }
}
std::vector<Delivery> Authority::receiveLocked(Connection id, const Json &message) {
    auto &client = clients_[id];
    const auto type = message.value("type", std::string{});
    if (type == "Hello") {
        const auto mismatch = compatibilityError(content_, message);
        if (!mismatch.empty()) return {{id, error("incompatible", mismatch)}};
        if (!client.welcomed) {
            client.token = newToken();
            client.welcomed = true;
        }
        client.connected = true;
        return {{id, {{"type", "Welcome"}, {"protocol", 5}, {"instanceId", instanceId_},
                       {"token", client.token}, {"maxRooms", 20}, {"maxPlayers", 40}}}};
    }
    if (!client.welcomed) return {{id, error("handshake_required", "请先完成中央服务器握手")}};
    if (type == "Resume") {
        if (message.value("instanceId", std::string{}) != instanceId_ ||
            message.value("token", std::string{}) != client.token || client.room.empty())
            return {{id, error("resume_invalid", "重连凭据无效")}};
        auto &room = roomFor(client);
        if (room.phase == RoomPhase::Aborted) return {{id, error("room_aborted", room.abortReason)}};
        client.connected = true;
        client.disconnected = {};
        return roomSnapshot(room, true);
    }
    if (type == "CreateRoom") {
        if (!client.room.empty()) return {{id, error("already_in_room", "当前连接已有房间")}};
        if (rooms_.size() >= 20) return {{id, error("capacity", "服务器房间容量已满")}};
        Room room;
        room.code = newCode();
        room.owner = id;
        room.connections[0] = id;
        room.tokens[0] = client.token;
        room.decks[0] = decodeDeck(message.at("deck"), content_.catalog);
        room.created = currentTime();
        client.room = room.code;
        client.seat = 0;
        rooms_.emplace(room.code, std::move(room));
        auto &created = rooms_.at(client.room);
        return {{id, {{"type", "RoomJoined"}, {"room", roomJson(created, 0)}, {"token", client.token},
                       {"instanceId", instanceId_}}}};
    }
    if (type == "JoinRoom") {
        if (!client.room.empty()) return {{id, error("already_in_room", "当前连接已有房间")}};
        auto found = rooms_.find(message.value("code", std::string{}));
        if (found == rooms_.end()) return {{id, error("room_missing", "房间不存在")}};
        auto &room = found->second;
        if (room.connections[1] || room.phase != RoomPhase::Waiting)
            return {{id, error("room_full", "房间已满或已开始")}};
        room.decks[1] = decodeDeck(message.at("deck"), content_.catalog);
        room.connections[1] = id;
        room.tokens[1] = client.token;
        client.room = room.code;
        client.seat = 1;
        return {{id, {{"type", "RoomJoined"}, {"room", roomJson(room, 1)}, {"token", client.token},
                       {"instanceId", instanceId_}}},
                {room.owner, roomJson(room, 0)}};
    }
    if (client.room.empty()) return {{id, error("room_required", "请先创建或加入房间")}};
    auto &room = roomFor(client);
    if (type == "Sync") return roomSnapshot(room, room.phase == RoomPhase::Playing || room.phase == RoomPhase::Completed);
    return roomMessage(room, id, message);
}
std::vector<Delivery> Authority::roomMessage(Room &room, Connection sender, const Json &message) {
    int seat = sender && clients_.count(sender) ? clients_.at(sender).seat : -1;
    if (seat < 0) return {{sender, error("connection_invalid", "连接未绑定房间席位")}};
    if (message.value("seat", seat) != seat) return {{sender, error("wrong_player", "连接身份与席位不一致")}};
    const auto type = message.value("type", std::string{});
    if (type == "Configure") {
        if (sender != room.owner) return {{sender, error("owner_only", "只有房主可以配置房间")}};
        if (room.phase != RoomPhase::Waiting) return {{sender, error("already_started", "对局已开始")}};
        room.ownerFirst = message.value("ownerFirst", true);
        room.automatic = message.value("automatic", true);
        room.ready = {};
        return roomSnapshot(room, false);
    }
    if (type == "Deck") {
        if (room.phase != RoomPhase::Waiting) return {{sender, error("already_started", "对局已开始")}};
        room.decks[seat] = decodeDeck(message.at("deck"), content_.catalog);
        room.ready = {};
        return roomSnapshot(room, false);
    }
    if (type == "Ready") {
        if (room.connections[1] == 0) return {{sender, error("waiting_player", "等待另一位玩家加入")}};
        if (room.phase != RoomPhase::Waiting) return roomSnapshot(room, true);
        room.ready[seat] = true;
        if (room.ready[0] && room.ready[1]) {
            begin(room);
            return roomSnapshot(room, true);
        }
        return roomSnapshot(room, false);
    }
    if (type == "ActionQuery") {
        ActionOptions options;
        const auto decision = room.match && room.match->viewFor(seat).decision ? room.match->viewFor(seat).decision->id : 0;
        if (!room.match || room.phase != RoomPhase::Playing) options.error = "not_playing";
        else if (message.contains("viewer") && message.at("viewer") != seat) options.error = "wrong_player";
        else if (net::number(message.at("decision")) != decision) options.error = "stale_query";
        else options = room.match->queryActions(seat, net::number(message.at("generation")),
                       net::number(message.at("revision")), net::decodeIntent(message.at("intent")));
        return {{sender, {{"type", "ActionOptions"}, {"session", room.code}, {"request", message.at("request")},
                          {"generation", message.at("generation")}, {"revision", message.at("revision")},
                          {"viewer", seat}, {"decision", std::to_string(decision)}, {"options", net::encodeOptions(options)}}}};
    }
    if (type == "Command") {
        if (room.phase != RoomPhase::Playing || !room.match) return {{sender, error("not_playing", "当前没有进行中的对局")}};
        if (message.contains("actor") && message.at("actor") != seat)
            return {{sender, {{"type", "Rejected"}, {"code", "wrong_player"}, {"request", message.value("request", "")}}}};
        const auto before = room.match->revision();
        auto result = room.match->submit(seat, net::unwireCommand(message.at("command")));
        Json response = {{"type", result.accepted ? "Accepted" : "Rejected"}, {"request", message.value("request", "")},
                         {"sequence", message.value("sequence", "")}, {"revision", std::to_string(room.match->revision())}};
        if (!result.accepted) { response["code"] = result.errorCode; response["error"] = result.error; }
        else if (room.match->viewFor(0).result != -1) room.phase = RoomPhase::Completed;
        (void)before;
        auto output = roomSnapshot(room, true);
        output.insert(output.begin(), {sender, response});
        return output;
    }
    if (type == "Leave") {
        if (room.phase == RoomPhase::Playing) abort(room, "player_left");
        else room.phase = RoomPhase::Aborted;
        return roomSnapshot(room, true);
    }
    return {{sender, error("unknown_request", "未知房间操作")}};
}
std::vector<Delivery> Authority::disconnect(Connection id) {
    std::lock_guard lock(mutex_);
    auto found = clients_.find(id);
    if (found == clients_.end()) return {};
    found->second.connected = false;
    found->second.disconnected = currentTime();
    return {};
}
std::vector<Delivery> Authority::tick() {
    std::lock_guard lock(mutex_);
    const auto now = currentTime();
    for (auto &[code, room] : rooms_) {
        if (room.phase != RoomPhase::Playing) continue;
        bool expired = false;
        for (int seat = 0; seat < 2; ++seat) {
            const auto connection = room.connections[seat];
            if (!connection || clients_.at(connection).connected) continue;
            if (now - clients_.at(connection).disconnected >= std::chrono::seconds(60)) expired = true;
        }
        if (expired) abort(room, "reconnect_timeout");
        if (room.match) room.match->tick();
    }
    return {};
}
} // namespace wizard::central
