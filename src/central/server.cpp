#include "wizard/central_server.hpp"
#include "wizard/interaction.hpp"
#include <algorithm>

namespace wizard::central {
namespace {
std::string phaseName(RoomPhase phase) {
    switch (phase) {
    case RoomPhase::Waiting: return "waiting";
    case RoomPhase::Playing: return "playing";
    case RoomPhase::Completed: return "completed";
    case RoomPhase::Aborted: return "aborted";
    }
    throw std::logic_error("invalid room phase");
}
void append(std::vector<Delivery> &to, std::vector<Delivery> from) {
    to.insert(to.end(), std::make_move_iterator(from.begin()), std::make_move_iterator(from.end()));
}
}
Authority::Authority(Content content, std::string instanceId, TokenSource tokens,
                     SeedSource seeds, AuthorityLimits limits, TimeSource now)
    : content_(std::move(content)), instanceId_(std::move(instanceId)), tokenSource_(std::move(tokens)),
      seedSource_(std::move(seeds)), limits_(limits), now_(std::move(now)) {
    if (!tokenSource_ || !seedSource_ || instanceId_.empty()) throw std::invalid_argument("authority requires secure random sources and instance ID");
    if (!now_) now_ = [] { return Clock::now(); };
}
std::shared_ptr<Authority::Room> Authority::lookup(const std::string &code) const {
    std::lock_guard lock(mutex_);
    auto found = rooms_.find(code);
    return found == rooms_.end() ? nullptr : found->second;
}
std::size_t Authority::roomCount() const { std::lock_guard lock(mutex_); return rooms_.size(); }
std::size_t Authority::credentialCount() const { std::lock_guard lock(mutex_); return credentials_.size(); }
std::vector<std::string> Authority::roomCodes() const {
    std::lock_guard lock(mutex_);
    std::vector<std::string> codes;
    for (const auto &[code, room] : rooms_) { (void)room; codes.push_back(code); }
    return codes;
}
std::string Authority::route(Connection id) const {
    std::lock_guard lock(mutex_);
    auto found = clients_.find(id);
    return found == clients_.end() ? std::string{} : found->second.room;
}
std::string Authority::newCode() {
    static constexpr char alphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
    for (unsigned attempt = 0; attempt < 128; ++attempt) {
        std::string code;
        for (int i = 0; i < 8; ++i) code.push_back(alphabet[seedSource_() % 32]);
        if (!rooms_.count(code)) return code;
    }
    throw std::runtime_error("无法分配房间码");
}
bool Authority::reconnecting(const Room &room) const {
    for (int seat = 0; seat < 2; ++seat)
        if (!room.tokens[seat].empty() && !room.connections[seat] && !room.left[seat]) return true;
    return false;
}
Json Authority::roomJson(const Room &room, int seat) const {
    std::int64_t remaining = 0;
    for (const auto &deadline : room.deadline)
        if (deadline) {
            auto next = std::max<std::int64_t>(0, std::chrono::duration_cast<std::chrono::milliseconds>(*deadline - now_()).count());
            remaining = remaining ? std::min(remaining, next) : next;
        }
    return {{"type", "Room"}, {"session", room.code}, {"code", room.code}, {"owner", seat == 0},
            {"seat", seat}, {"player", room.match ? seat : -1}, {"joined", !room.tokens[1].empty()},
            {"ready", room.ready}, {"occupied", std::array<bool, 2>{!room.tokens[0].empty(), !room.tokens[1].empty()}},
            {"connected", std::array<bool, 2>{bool(room.connections[0]), bool(room.connections[1])}},
            {"ownerFirst", room.ownerFirst}, {"automatic", room.automatic}, {"phase", phaseName(room.phase)},
            {"started", bool(room.match)}, {"ended", room.phase == RoomPhase::Completed || room.phase == RoomPhase::Aborted},
            {"aborted", room.phase == RoomPhase::Aborted}, {"abortReason", room.abortReason},
            {"reconnecting", reconnecting(room)}, {"reconnectRemainingMs", remaining},
            {"technicalPaused", room.match && room.match->technicalPaused()}, {"storageBlocked", room.storageBlocked},
            {"nextSequence", std::to_string(room.sequence[seat])}};
}
Json Authority::joined(const Room &room, int seat, bool resumed) const {
    return {{"type", "RoomJoined"}, {"session", room.code}, {"instanceId", instanceId_},
            {"room", roomJson(room, seat)}, {"resumed", resumed}};
}
Json Authority::snapshot(const Room &room, int seat) const {
    return {{"type", "StateSnapshot"}, {"session", room.code}, {"revision", std::to_string(room.match->revision())},
            {"technicalPaused", room.match->technicalPaused()}, {"view", net::encodeView(room.match->viewFor(seat))}};
}
std::vector<Delivery> Authority::publish(Room &room, bool state, Connection only) {
    capture(room);
    std::vector<Delivery> result;
    for (int seat = 0; seat < 2; ++seat) {
        const auto id = room.connections[seat];
        if (!id || (only && only != id)) continue;
        result.push_back({id, roomJson(room, seat)});
        if (state && room.match) {
            result.push_back({id, snapshot(room, seat)});
            auto events = room.match->eventsSince(seat, room.eventEpoch[seat], room.eventCursor[seat]);
            room.eventEpoch[seat] = events.epoch;
            room.eventCursor[seat] = events.next;
            result.push_back({id, {{"type", "EventBatch"}, {"session", room.code},
                {"revision", std::to_string(room.match->revision())}, {"batch", net::encodeEvents(events)}}});
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
        room.seed = seedSource_();
        if (content_.catalog.mechanicsRevision >= 12 || firstSeat(room.seed) == (room.ownerFirst ? 0u : 1u)) { found = true; break; }
    }
    if (!found) throw std::runtime_error("无法生成指定先后手种子");
    config.seed = room.seed;
    room.match = std::make_unique<MatchSession>(content_, config);
    room.match->enableClock(room.automatic, now_);
    room.phase = RoomPhase::Playing;
    room.started = now_();
    room.recordingBytes = room.match->recordingHeader().dump().size() + 1024 * 1024;
    room.recordingCursor = {};
}
void Authority::freeze(Room &room) {
    const bool frozen = room.phase != RoomPhase::Playing || reconnecting(room) || room.storageBlocked;
    if (room.match && room.clockFrozen != frozen) { room.match->freezeClock(frozen); room.clockFrozen = frozen; }
}
void Authority::abort(Room &room, const std::string &reason) {
    if (room.phase == RoomPhase::Completed || room.phase == RoomPhase::Aborted) return;
    room.phase = RoomPhase::Aborted;
    room.abortReason = reason;
    room.terminal = now_();
    // MatchSession::freezeClock intentionally settles elapsed wall time before
    // freezing. Lifecycle aborts must skip that settlement: a reconnect or
    // lifetime expiry is not a rules timeout and cannot manufacture a winner.
    if (room.match) room.match->freezeClock(true, false);
    room.clockFrozen = true;
}
bool Authority::refresh(Room &room) {
    const auto previous = room.phase;
    const auto now = now_();
    if (room.phase == RoomPhase::Waiting || room.phase == RoomPhase::Playing) {
        for (int seat = 0; seat < 2; ++seat) if (room.deadline[seat] && now >= *room.deadline[seat]) {
            if (room.phase == RoomPhase::Waiting && seat == 1) {
                releaseCredential(room, seat);
                room.tokens[seat].clear(); room.deadline[seat].reset(); room.ready = {}; room.left[seat] = false;
            } else abort(room, "reconnect_timeout");
        }
        if (room.phase == RoomPhase::Waiting && now - room.touched >= limits_.waiting) abort(room, "waiting_timeout");
        if (room.phase == RoomPhase::Playing && now - room.started >= limits_.match) abort(room, "match_lifetime");
    }
    freeze(room);
    if (room.phase == RoomPhase::Playing && !reconnecting(room) && !room.storageBlocked) {
        room.match->tick();
        for (unsigned i = 0; i < 16 && room.automatic && !room.match->technicalPaused(); ++i) {
            const auto view = room.match->viewFor(room.match->engine().state().active);
            const auto command = ui::automaticAdvance(view, true);
            if (!command || !room.match->submit(view.viewer, *command).accepted) break;
        }
        if (!updateRecordingBudget(room)) abort(room, "replay_budget");
        if (room.match->engine().state().result != -1 && room.phase == RoomPhase::Playing) { room.phase = RoomPhase::Completed; room.terminal = now; freeze(room); }
    }
    return previous != room.phase;
}
void Authority::releaseCredential(Room &room, int seat) {
    std::lock_guard registry(mutex_);
    credentials_.erase(room.tokens[seat]);
}
bool Authority::updateRecordingBudget(Room &room) {
    if (!room.match) return true;
    const auto delta = room.match->recordingSince(room.recordingCursor);
    room.recordingCursor = delta.next;
    for (const auto &row : delta.commands) room.recordingBytes += row.dump().size() + 1;
    for (const auto &row : delta.events) room.recordingBytes += row.dump().size() + 1;
    return room.recordingBytes <= limits_.replayBytes && room.match->commandCount() < 100000;
}
std::vector<Delivery> Authority::receive(Connection id, const Json &message) {
    try {
        if (!id || !message.is_object()) return {{id, error("invalid_request", "非法消息")}};
        const auto type = message.at("type").get<std::string>();
        if (type == "Hello" || type == "CreateRoom" || type == "JoinRoom" || type == "Resume") return entry(id, message);
        std::shared_ptr<Room> room;
        int seat = -1;
        {
            std::lock_guard lock(mutex_);
            const auto client = clients_.find(id);
            if (client == clients_.end()) return {{id, error("handshake_required", "请先完成中央服务器握手")}};
            if (client->second.room.empty()) return {{id, error("room_required", "请先创建或加入房间")}};
            auto found = rooms_.find(client->second.room);
            if (found == rooms_.end()) return {{id, error("room_missing", "房间已关闭")}};
            room = found->second; seat = client->second.seat;
        }
        std::lock_guard lock(room->mutex);
        if (room->connections[seat] != id) return {{id, error("connection_invalid", "连接已过期")}};
        refresh(*room);
        if (type == "Sync") return publish(*room, true, id);
        return roomMessage(*room, id, seat, message);
    } catch (const std::exception &e) { return {{id, error("invalid_request", e.what())}}; }
}
std::vector<Delivery> Authority::entry(Connection id, const Json &message) {
    std::unique_lock registry(mutex_);
    const auto type = message.at("type").get<std::string>();
    if (type == "Hello") {
        const auto mismatch = compatibilityError(content_, message);
        if (!mismatch.empty()) return {{id, error("incompatible", mismatch)}};
        if (!clients_.count(id)) {
            if (clients_.size() >= limits_.connections) return {{id, error("capacity", "连接容量已满")}};
            Client client; client.token = tokenSource_();
            if (client.token.empty()) throw std::runtime_error("无法生成连接凭据");
            clients_.emplace(id, std::move(client));
        }
        return {{id, {{"type", "Welcome"}, {"protocol", 5}, {"instanceId", instanceId_}, {"token", clients_.at(id).token}}}};
    }
    const auto foundClient = clients_.find(id);
    if (foundClient == clients_.end()) return {{id, error("handshake_required", "请先完成中央服务器握手")}};
    auto &client = foundClient->second;
    if (type == "Resume") {
        if (message.value("instanceId", std::string{}) != instanceId_ || !client.room.empty())
            return {{id, error("resume_invalid", "服务器已重启或凭据无效")}};
        auto found = credentials_.find(message.value("token", std::string{}));
        auto room = found == credentials_.end() ? nullptr : found->second.room.lock();
        if (!room) return {{id, error("resume_invalid", "重连凭据无效或房间已关闭")}};
        const auto seat = found->second.seat;
        registry.unlock();
        std::lock_guard lock(room->mutex);
        refresh(*room);
        if (room->tokens[seat] != message.at("token") || room->left[seat] ||
            (message.contains("code") && !message.at("code").get<std::string>().empty() && message.at("code") != room->code))
            return {{id, error("resume_invalid", "重连房间与凭据不匹配")}};
        if (room->connections[seat]) return {{id, error("already_connected", "席位仍在线")}};
        {
            std::lock_guard binding(mutex_);
            auto current = clients_.find(id);
            if (current == clients_.end() || !current->second.room.empty() || !rooms_.count(room->code))
                return {{id, error("resume_invalid", "连接或房间已关闭")}};
            current->second.token = room->tokens[seat]; current->second.room = room->code; current->second.seat = seat;
        }
        room->connections[seat] = id; room->deadline[seat].reset(); room->eventEpoch[seat] = room->eventCursor[seat] = 0;
        freeze(*room);
        auto output = publish(*room, true);
        output.insert(output.begin(), {id, joined(*room, seat, true)});
        return output;
    }
    if (!client.room.empty()) {
        auto found = rooms_.find(client.room);
        if (client.entry != message || found == rooms_.end()) return {{id, error("already_in_room", "当前连接已有房间")}};
        const auto room = found->second;
        const auto seat = client.seat;
        registry.unlock();
        std::lock_guard lock(room->mutex);
        return {{id, joined(*room, seat)}};
    }
    // Validation precedes every binding mutation: invalid decks cannot reserve a seat.
    registry.unlock();
    auto deck = decodeDeck(message.at("deck"), content_.catalog);
    registry.lock();
    if (!clients_.count(id) || !clients_.at(id).room.empty()) return {{id, error("connection_invalid", "连接已过期")}};
    std::shared_ptr<Room> room;
    int seat = 0;
    if (type == "CreateRoom") {
        if (rooms_.size() >= limits_.rooms) return {{id, error("capacity", "服务器房间容量已满")}};
        room = std::make_shared<Room>(); room->code = newCode(); room->touched = now_();
        room->storageId = room->code + "-" + instanceId_ + "-" + std::to_string(++roomSerial_);
        rooms_.emplace(room->code, room);
    } else {
        auto found = rooms_.find(message.at("code").get<std::string>());
        if (found == rooms_.end()) return {{id, error("room_missing", "房间不存在")}};
        room = found->second; seat = 1;
    }
    registry.unlock();
    std::lock_guard lock(room->mutex);
    refresh(*room);
    if (room->phase != RoomPhase::Waiting || !room->tokens[seat].empty()) return {{id, error("room_full", "房间已满或已开始")}};
    {
        std::lock_guard binding(mutex_);
        auto current = clients_.find(id);
        if (current == clients_.end() || !current->second.room.empty() || !rooms_.count(room->code))
            return {{id, error("connection_invalid", "连接或房间已关闭")}};
        auto &bound = current->second;
        room->tokens[seat] = bound.token;
        credentials_[bound.token] = {room, seat}; bound.room = room->code; bound.seat = seat; bound.entry = message;
    }
    room->decks[seat] = std::move(deck); room->connections[seat] = id;
    room->ready = {}; room->left[seat] = false; room->touched = now_();
    auto output = publish(*room, false);
    output.insert(output.begin(), {id, joined(*room, seat)});
    return output;
}
std::vector<Delivery> Authority::roomMessage(Room &room, Connection sender, int seat, const Json &message) {
    const auto type = message.at("type").get<std::string>();
    if (message.contains("session") && message.at("session") != room.code)
        return {{sender, error("wrong_session", "房间会话不匹配")}};
    if (type == "Configure") {
        if (seat != 0) return {{sender, error("owner_only", "只有房主可以配置房间")}};
        if (room.phase != RoomPhase::Waiting) return {{sender, error("already_started", "对局已开始")}};
        room.ownerFirst = content_.catalog.mechanicsRevision >= 12 || message.value("ownerFirst", true);
        room.automatic = message.value("automatic", true);
        room.ready = {}; room.touched = now_(); return publish(room, false);
    }
    if (type == "Deck") {
        if (room.phase != RoomPhase::Waiting) return {{sender, error("already_started", "对局已开始")}};
        room.decks[seat] = decodeDeck(message.at("deck"), content_.catalog);
        room.ready = {}; room.touched = now_(); return publish(room, false);
    }
    if (type == "Ready") {
        if (room.phase != RoomPhase::Waiting) return publish(room, true, sender);
        if (!room.connections[0] || !room.connections[1]) return {{sender, error("waiting_player", "等待双方在线")}};
        room.ready[seat] = true; room.touched = now_();
        if (room.ready[0] && room.ready[1]) begin(room);
        return publish(room, bool(room.match));
    }
    if (type == "ActionQuery") {
        ActionOptions options;
        const auto view = room.match ? room.match->viewFor(seat) : GameView{};
        const auto decision = view.decision ? view.decision->id : 0;
        if (!room.match || room.phase != RoomPhase::Playing) options.error = "not_playing";
        else if (message.contains("viewer") && message.at("viewer") != seat) options.error = "wrong_player";
        else if (reconnecting(room) || room.storageBlocked) options.error = "room_paused";
        else if (net::number(message.at("decision")) != decision) options.error = "stale_query";
        else options = room.match->queryActions(seat, net::number(message.at("generation")),
                       net::number(message.at("revision")), net::decodeIntent(message.at("intent")));
        return {{sender, {{"type", "ActionOptions"}, {"session", room.code}, {"request", message.at("request")},
            {"generation", message.at("generation")}, {"revision", message.at("revision")}, {"viewer", seat},
            {"decision", std::to_string(decision)}, {"options", net::encodeOptions(options)}}}};
    }
    if (type == "Command") {
        const auto payload = message.dump();
        if (payload.size() > 16384) return {{sender, error("command_too_large", "命令超出大小上限")}};
        const auto sequence = net::number(message.at("sequence"));
        auto &cache = room.replies[seat];
        const auto cached = cache.find(sequence);
        if (cached != cache.end()) {
            if (cached->second.payload != payload) return {{sender, error("sequence_conflict", "请求序号内容冲突")}};
            return {{sender, cached->second.reply}};
        }
        if (sequence != room.sequence[seat]) {
            auto response = error(sequence < room.sequence[seat] ? "resync_required" : "sequence_gap", "请求序号过期或乱序");
            response["expectedSequence"] = std::to_string(room.sequence[seat]); return {{sender, response}};
        }
        Json response = {{"type", "Rejected"}, {"session", room.code}, {"request", message.at("request")},
                         {"sequence", message.at("sequence")}, {"code", "not_playing"}, {"error", "当前没有进行中的对局"}};
        if ((message.contains("actor") && message.at("actor") != seat) || (message.contains("viewer") && message.at("viewer") != seat)) {
            response["code"] = "wrong_player"; response["error"] = "连接身份与玩家不一致";
        } else if (room.match && room.phase == RoomPhase::Playing) {
            const auto view = room.match->viewFor(seat);
            if (reconnecting(room) || room.storageBlocked) { response["code"] = "room_paused"; response["error"] = "等待重连或存储恢复"; }
            else if (net::number(message.at("revision")) != room.match->revision() ||
                     net::number(message.at("decision")) != (view.decision ? view.decision->id : 0)) {
                response["code"] = "stale_command"; response["error"] = "操作视图已过期";
            } else {
                try {
                    const auto command = net::unwireCommand(message.at("command"));
                    if (std::holds_alternative<Timeout>(command)) throw std::invalid_argument("规则超时只允许服务器执行");
                    const auto result = room.match->submit(seat, command);
                    response["type"] = result.accepted ? "Accepted" : "Rejected";
                    response["code"] = result.errorCode; response["error"] = result.error;
                } catch (const std::exception &e) { response["code"] = "invalid_command"; response["error"] = e.what(); }
            }
        }
        refresh(room);
        response["revision"] = std::to_string(room.match ? room.match->revision() : 0);
        ++room.sequence[seat]; cache.emplace(sequence, CachedReply{payload, response}); room.replyBytes[seat] += payload.size();
        while (cache.size() > 256 || room.replyBytes[seat] > net::maxMessage) {
            room.replyBytes[seat] -= cache.begin()->second.payload.size(); cache.erase(cache.begin());
        }
        auto output = publish(room, true); output.insert(output.begin(), {sender, response}); return output;
    }
    if (type == "Leave") {
        if (room.phase == RoomPhase::Playing) return {{sender, error("surrender_required", "对局中请先投降或断开连接")}};
        if (room.phase == RoomPhase::Waiting && seat == 0) abort(room, "owner_left");
        room.left[seat] = true; room.connections[seat] = 0; room.deadline[seat].reset();
        if (room.phase == RoomPhase::Waiting && seat == 1) { releaseCredential(room, seat); room.tokens[seat].clear(); room.ready = {}; }
        auto output = publish(room, bool(room.match)); output.push_back({sender, {{"type", "Left"}}}); return output;
    }
    if (type == "Replay") {
        if (room.phase != RoomPhase::Completed || !room.replay)
            return {{sender, error("replay_not_ready", "完整复盘仅在正常终局并保存后提供")}};
        const auto offset = message.contains("offset") ? net::number(message.at("offset")) : 0;
        const auto &bytes = *room.replay;
        if (offset > bytes.size() || (offset < bytes.size() && (static_cast<unsigned char>(bytes[offset]) & 0xc0) == 0x80))
            return {{sender, error("invalid_offset", "复盘偏移无效")}};
        auto end = std::min<std::size_t>(bytes.size(), offset + 65536);
        while (end < bytes.size() && (static_cast<unsigned char>(bytes[end]) & 0xc0) == 0x80) --end;
        return {{sender, {{"type", "ReplayChunk"}, {"session", room.code}, {"size", std::to_string(bytes.size())},
            {"offset", std::to_string(offset)}, {"data", bytes.substr(offset, end - offset)}, {"complete", end == bytes.size()}}}};
    }
    return {{sender, error("unknown_request", "未知房间操作")}};
}
std::vector<Delivery> Authority::disconnect(Connection id) {
    std::shared_ptr<Room> room;
    int seat = -1;
    {
        std::lock_guard lock(mutex_);
        auto client = clients_.find(id);
        if (client == clients_.end()) return {};
        auto found = rooms_.find(client->second.room);
        if (found != rooms_.end()) { room = found->second; seat = client->second.seat; }
        clients_.erase(client);
    }
    if (!room || seat < 0) return {};
    std::lock_guard lock(room->mutex);
    if (room->connections[seat] != id) return {};
    room->connections[seat] = 0;
    if (!room->left[seat] && (room->phase == RoomPhase::Waiting || room->phase == RoomPhase::Playing))
        room->deadline[seat] = now_() + limits_.reconnect;
    freeze(*room); refresh(*room); return publish(*room, true);
}
std::vector<Delivery> Authority::tickRoom(const std::string &code) {
    auto room = lookup(code);
    if (!room) return {};
    std::vector<Delivery> output;
    bool retire = false;
    {
        std::lock_guard lock(room->mutex);
        const auto revision = room->match ? room->match->revision() : 0;
        const auto oldRoom = roomJson(*room, 0);
        refresh(*room);
        if (oldRoom != roomJson(*room, 0) || (room->match && revision != room->match->revision())) output = publish(*room, true);
        else if (room->match && now_() - room->clockSent >= std::chrono::milliseconds(250)) {
            room->clockSent = now_();
            for (auto id : room->connections) if (id) output.push_back({id, {{"type", "ClockUpdate"}, {"session", room->code},
                {"revision", std::to_string(room->match->revision())}, {"clock", net::encodeClock(room->match->clockView())}}});
        }
        capture(*room);
        retire = room->terminalStored && !room->pendingWrite && room->journalTail.empty() &&
                 (room->phase == RoomPhase::Completed || room->phase == RoomPhase::Aborted) &&
                 ((room->left[0] && (room->left[1] || room->tokens[1].empty())) || now_() - room->terminal >= limits_.terminal);
    }
    if (retire) {
        std::lock_guard lock(mutex_);
        rooms_.erase(code);
        for (auto it = credentials_.begin(); it != credentials_.end();)
            if (it->second.room.expired() || it->second.room.lock() == room) it = credentials_.erase(it); else ++it;
    }
    return output;
}
std::vector<Delivery> Authority::tick() {
    std::vector<Delivery> output;
    for (const auto &code : roomCodes()) append(output, tickRoom(code));
    return output;
}
std::vector<Delivery> Authority::abortRoom(const std::string &code, const std::string &reason) {
    auto room = lookup(code);
    if (!room) return {};
    std::lock_guard lock(room->mutex); abort(*room, reason); return publish(*room, true);
}
Json Authority::summary(const Room &room) const {
    return {{"format", 1}, {"code", room.code}, {"instanceId", instanceId_}, {"phase", phaseName(room.phase)},
            {"revision", std::to_string(room.match ? room.match->revision() : 0)}, {"abortReason", room.abortReason},
            {"technicalPaused", room.match && room.match->technicalPaused()}, {"storageBlocked", room.storageBlocked}};
}
void Authority::capture(Room &room) {
    if (room.match && !room.journalStarted) {
        room.journalTail += Json{{"kind", "start"}, {"recording", room.match->recordingHeader()}}.dump() + '\n';
        room.journalStarted = true;
    }
    if (room.match) {
        auto delta = room.match->recordingSince(room.journalCursor);
        room.journalCursor = delta.next;
        if (!delta.commands.empty() || !delta.events.empty())
            room.journalTail += Json{{"kind", "delta"}, {"revision", std::to_string(room.match->revision())},
                {"commands", std::move(delta.commands)}, {"events", std::move(delta.events)}}.dump() + '\n';
    }
    auto state = summary(room);
    auto encoded = state.dump();
    if (encoded != room.journalState) {
        room.journalTail += Json{{"kind", "lifecycle"}, {"summary", std::move(state)}}.dump() + '\n';
        room.journalState = std::move(encoded);
    }
    if (room.journalTail.size() > net::maxMessage) { room.storageBlocked = true; freeze(room); }
}
std::optional<JournalWrite> Authority::takeWrite(const std::string &code) {
    auto room = lookup(code);
    if (!room) return {};
    std::lock_guard lock(room->mutex);
    capture(*room);
    if (room->writeQueued || now_() < room->retryAt) return {};
    if (!room->pendingWrite) {
        if (room->journalTail.empty()) return {};
        JournalWrite write;
        write.room = code; write.serial = ++room->writeSerial; write.offset = room->durableOffset;
        write.bytes = std::make_shared<const std::string>(std::move(room->journalTail)); room->journalTail.clear();
        write.summary = summary(*room);
        write.summary["storageId"] = room->storageId;
        if (!room->terminalStored && (room->phase == RoomPhase::Completed || room->phase == RoomPhase::Aborted)) write.finished = room->match;
        room->pendingWrite = std::move(write);
    }
    room->writeQueued = true;
    return room->pendingWrite;
}
std::vector<Delivery> Authority::persisted(const JournalResult &result) {
    auto room = lookup(result.room);
    if (!room) return {};
    std::lock_guard lock(room->mutex);
    if (!room->pendingWrite || room->pendingWrite->serial != result.serial) return {};
    room->writeQueued = false;
    if (!result.error.empty()) {
        room->storageBlocked = true; room->retryAt = now_() + std::chrono::seconds(1); freeze(*room);
    } else {
        room->durableOffset = result.offset;
        if (room->pendingWrite->summary.at("phase") == "completed" || room->pendingWrite->summary.at("phase") == "aborted")
            room->terminalStored = true;
        if (result.replay) room->replay = result.replay;
        room->pendingWrite.reset(); room->storageBlocked = false; room->retryAt = {}; freeze(*room);
    }
    refresh(*room);
    return publish(*room, true);
}
} // namespace wizard::central
