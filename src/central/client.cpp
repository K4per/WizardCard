#include "wizard/central_client.hpp"
#include <algorithm>

namespace wizard::central {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
}

ClientRoom::ClientRoom(Content content, std::filesystem::path replayPath)
    : content_(std::move(content)), replayPath_(std::move(replayPath)), guest_(content_) {}

void ClientRoom::connect(const RoomConnectOptions &options, const PlayerDeck &deck) {
    if (configured_) throw std::logic_error("central room is already configured");
    if (options.endpoint.hostname.empty() || !options.endpoint.port)
        throw std::invalid_argument("server hostname and port are required");
    if (options.entry == RoomEntry::Join && options.roomCode.size() != 8)
        throw std::invalid_argument("an eight-character room code is required");
    const auto problems = Rules::deckErrors(content_.catalog, deck);
    if (!problems.empty()) throw std::invalid_argument(problems.front());
    options_ = options;
    deck_ = deck;
    entry_ = {{"type", options.entry == RoomEntry::Create ? "CreateRoom" : "JoinRoom"},
              {"deck", encodeDeck(deck_)}};
    if (options.entry == RoomEntry::Join) entry_["code"] = options.roomCode;
    room_ = {{"type", "Room"}, {"session", ""}, {"code", ""}, {"owner", options.entry == RoomEntry::Create},
             {"seat", options.entry == RoomEntry::Create ? 0 : 1}, {"joined", false},
             {"ready", Json::array({false, false})}, {"phase", "waiting"}, {"started", false},
             {"ended", false}, {"reconnecting", false}, {"storageBlocked", false},
             {"technicalPaused", false}};
    configured_ = true;
    notice_ = "等待服务器连接";
}

void ClientRoom::host(unsigned short port, const PlayerDeck &deck) {
    RoomConnectOptions options;
    options.endpoint.port = port;
    options.entry = RoomEntry::Create;
    connect(options, deck);
}

void ClientRoom::join(const std::string &hostname, unsigned short port) {
    (void)hostname;
    (void)port;
    throw std::invalid_argument("central room join requires a room code");
}

void ClientRoom::queue(Json message) {
    outgoing_.push_back(std::move(message));
}

void ClientRoom::sendEntry() {
    if (!entrySent_) {
        queue(entry_);
        entrySent_ = true;
    }
}

void ClientRoom::transportOpened() {
    if (!configured_ || ended_) return;
    transportOpen_ = true;
    connected_ = false;
    lastReceive_ = Clock::now();
    queue(central::hello(content_));
    notice_ = "正在验证服务器证书…";
}

void ClientRoom::transportLost(const std::string &reason) {
    if (!transportOpen_) {
        if (!ended_ && !reason.empty()) notice_ = reason;
        return;
    }
    transportOpen_ = false;
    connected_ = false;
    resumePending_ = false;
    entrySent_ = false;
    replayRequested_ = false;
    replayRetryAt_ = {};
    leaveSent_ = false;
    outgoing_.clear();
    if (!ended_) {
        reset_ = true;
        notice_ = reason.empty() ? "连接中断，等待重新连接" : reason;
    }
}

void ClientRoom::receiveTransportMessage(const Json &message) {
    if (!message.is_object()) throw std::invalid_argument("server message must be an object");
    lastReceive_ = Clock::now();
    const auto type = message.value("type", std::string{});
    if (type == "Heartbeat") return;
    if (type == "Welcome") {
        candidateToken_ = message.at("token").get<std::string>();
        candidateInstance_ = message.at("instanceId").get<std::string>();
        if (candidateToken_.size() != 64 || candidateToken_.find_first_not_of("0123456789abcdef") != std::string::npos)
            throw std::runtime_error("server issued an invalid resume token");
        if (token_.empty()) {
            token_ = candidateToken_;
            instanceId_ = candidateInstance_;
            sendEntry();
            return;
        }
        if (candidateInstance_ != instanceId_) {
            ended_ = true;
            notice_ = "服务器已重启，未完成的房间已中止";
            return;
        }
        Json resume{{"type", "Resume"}, {"token", token_}, {"instanceId", instanceId_}};
        const auto code = room_.value("code", std::string{});
        if (!code.empty()) resume["code"] = code;
        resumePending_ = true;
        queue(std::move(resume));
        notice_ = "正在恢复房间席位…";
        return;
    }
    if (type == "Error") {
        const auto code = message.value("code", std::string{});
        if (code == "replay_not_ready") {
            replayRequested_ = false;
            replayRetryAt_ = Clock::now() + 1s;
            return;
        }
        if (resumePending_) {
            if (code == "already_connected") {
                transportLost("等待旧连接释放后重新连接…");
                return;
            }
            resumePending_ = false;
            if (code == "resume_invalid" && room_.value("code", std::string{}).empty() &&
                candidateInstance_ == instanceId_) {
                token_ = candidateToken_;
                sendEntry();
                return;
            }
            ended_ = true;
        }
        notice_ = message.value("error", "服务器拒绝了请求");
        return;
    }
    if (type == "RoomJoined") {
        guest_.receive(message);
        room_ = message.at("room");
        connected_ = transportOpen_;
        resumePending_ = false;
        entrySent_ = true;
        if (message.value("resumed", false)) {
            reset_ = true;
            if (guest_.retry()) queue(*guest_.retry());
        }
        notice_ = message.value("resumed", false) ? "房间连接已恢复" : "已加入中央服务器房间";
        updateRoom(room_);
        return;
    }
    if (type == "Room") {
        guest_.receive(message);
        updateRoom(message);
        return;
    }
    if (type == "Left") {
        ended_ = true;
        closing_ = false;
        notice_ = "已离开房间";
        return;
    }
    if (type == "ReplayChunk") {
        const auto offset = net::number(message.at("offset"));
        const auto size = net::number(message.at("size"));
        const auto data = message.at("data").get<std::string>();
        if (size > replayLimit || offset != replayBytes_.size() || replayBytes_.size() > size ||
            data.size() > size - replayBytes_.size())
            throw std::runtime_error("invalid replay chunk");
        replaySize_ = static_cast<std::size_t>(size);
        replayBytes_ += data;
        replayRequested_ = false;
        if (message.at("complete").get<bool>()) completeReplay();
        else requestReplay();
        return;
    }
    const auto pending = guest_.retry();
    if (type == "Rejected" && closing_ && pending && pending->at("request") == message.at("request")) {
        closing_ = false;
        notice_ = message.value("error", "投降未被接受，请重试");
    }
    guest_.receive(message);
    revision_ = guest_.revision();
    if (guest_.started()) updateRoom(guest_.room());
}

void ClientRoom::updateRoom(const Json &room) {
    room_ = room;
    const auto phase = room_.value("phase", std::string("waiting"));
    if (phase == "aborted") {
        ended_ = true;
        notice_ = "对局已中止：" + room_.value("abortReason", std::string("unknown"));
    } else if (phase == "completed") {
        requestReplay();
    }
}

void ClientRoom::requestReplay() {
    if (!connected() || replayDelivered_ || replayRequested_ || room_.value("phase", std::string{}) != "completed") return;
    if (Clock::now() < replayRetryAt_) return;
    queue({{"type", "Replay"}, {"offset", std::to_string(replayBytes_.size())}});
    replayRequested_ = true;
    replayRetryAt_ = Clock::now() + 2s;
}

void ClientRoom::completeReplay() {
    if (replayBytes_.size() != replaySize_) throw std::runtime_error("incomplete replay transfer");
    auto record = Json::parse(replayBytes_);
    (void)wizard::replay(content_, record);
    writeAtomicJson(replayPath_, record);
    replay_ = std::move(record);
    replayBytes_.clear();
    replaySize_ = 0;
    replayDelivered_ = true;
    notice_ = "终局复盘已验证并保存";
    advanceLeave();
}

void ClientRoom::advanceLeave() {
    if (!closing_ || !connected() || leaveSent_ || ended_) return;
    if (started() && guest_.view().result == -1) return;
    if (room_.value("phase", std::string{}) == "completed" && !replayDelivered_) return;
    queue({{"type", "Leave"}, {"session", room_.at("session")}});
    leaveSent_ = true;
}

std::vector<Json> ClientRoom::drainTransport() {
    std::vector<Json> messages;
    messages.reserve(outgoing_.size());
    while (!outgoing_.empty()) {
        messages.push_back(std::move(outgoing_.front()));
        outgoing_.pop_front();
    }
    return messages;
}

void ClientRoom::deck(const PlayerDeck &deck) {
    const auto problems = Rules::deckErrors(content_.catalog, deck);
    if (!problems.empty()) throw std::invalid_argument(problems.front());
    if (!connected()) throw std::runtime_error("服务器连接尚未就绪");
    deck_ = deck;
    queue({{"type", "Deck"}, {"session", room_.at("session")}, {"deck", encodeDeck(deck_)}});
}

void ClientRoom::configure(bool hostFirst, bool automatic) {
    if (!isHost()) throw std::runtime_error("仅房主可修改房间");
    if (!connected()) throw std::runtime_error("服务器连接尚未就绪");
    queue({{"type", "Configure"}, {"session", room_.at("session")},
           {"ownerFirst", hostFirst}, {"automatic", automatic}});
}

void ClientRoom::ready() {
    if (!connected() || blocked()) throw std::runtime_error("服务器连接尚未就绪");
    queue({{"type", "Ready"}, {"session", room_.at("session")}});
}

CommandResult ClientRoom::submit(const Command &command) {
    if (blocked()) return {false, "等待连接恢复或操作确认", {}, {}, "network_wait"};
    try {
        queue(guest_.command(command));
        return {true, {}, {}, {}, {}, true};
    } catch (const std::exception &error) {
        return {false, error.what(), {}, {}, "invalid_command"};
    }
}

CommandResult ClientRoom::submit(PlayerId actor, const Command &command) {
    if (!started() || actor != guest_.view().viewer)
        return {false, "无权操作对方席位", {}, {}, "wrong_player"};
    return submit(command);
}

bool ClientRoom::save(std::string &error) {
    if (replayDelivered_) return true;
    if (room_.value("phase", std::string{}) == "completed") {
        requestReplay();
        error = "等待服务器传输并验证终局复盘";
    } else {
        error = "完整复盘仅在正常终局后提供";
    }
    return false;
}

bool ClientRoom::leave(std::string &error, bool force) {
    if (force) { stop(); return true; }
    if (ended_) { stop(); return true; }
    if (started() && guest_.view().result == -1) {
        if (!closing_) {
            const auto result = submit(Surrender{});
            if (!result.accepted) { error = result.error; return false; }
            closing_ = true;
        }
        error = "等待服务器确认投降并保存复盘";
        return false;
    }
    if (room_.value("phase", std::string{}) == "completed" && !replayDelivered_) {
        closing_ = true;
        requestReplay();
        error = "等待服务器传输终局复盘";
        return false;
    }
    if (connected()) {
        closing_ = true;
        advanceLeave();
        error = "等待服务器确认离开";
        return false;
    }
    stop();
    return true;
}

void ClientRoom::stop() {
    connected_ = false;
    transportOpen_ = false;
    ended_ = true;
    closing_ = false;
    outgoing_.clear();
}

void ClientRoom::reconnect() {
    if (ended_ || !configured_) throw std::runtime_error("当前房间不能重连");
    transportLost("正在重新连接服务器…");
    reset_ = true;
}

bool ClientRoom::isHost() const {
    return room_.value("owner", options_.entry == RoomEntry::Create);
}

bool ClientRoom::takeReset() {
    const bool reset = reset_;
    reset_ = false;
    return reset;
}

GameView ClientRoom::viewFor(PlayerId viewer) const {
    if (!started() || viewer != guest_.view().viewer) throw std::invalid_argument("invalid central room viewer");
    return guest_.view();
}

MatchClockView ClientRoom::clockView() const {
    return started() ? guest_.view().clock : MatchClockView{};
}

bool ClientRoom::blocked() const {
    return !connected() || ended_ || closing_ || guest_.pending() || guest_.technicalPaused() ||
           room_.value("storageBlocked", false) || room_.value("reconnecting", false);
}

void ClientRoom::tick() {
    if (!transportOpen_ || ended_) return;
    const auto now = Clock::now();
    if (now - lastReceive_ >= 6s) { transportLost("服务器心跳超时"); return; }
    if (connected() && now - heartbeatAt_ >= 2s) {
        queue({{"type", "Heartbeat"}});
        heartbeatAt_ = now;
    }
    requestReplay();
    advanceLeave();
}

EventBatch ClientRoom::eventsSince(PlayerId viewer, std::uint64_t epoch, std::uint64_t cursor) const {
    if (!started() || viewer != guest_.view().viewer) throw std::invalid_argument("invalid central room event viewer");
    return guest_.eventsSince(epoch, cursor);
}

ActionOptions ClientRoom::queryActions(PlayerId viewer, std::uint64_t generation,
                                       std::uint64_t revision, const ActionIntent &intent) {
    ActionOptions result;
    if (!started() || viewer != guest_.view().viewer) { result.error = "invalid_viewer"; return result; }
    if (blocked()) { result.error = "network_wait"; return result; }
    if (auto current = guest_.queryResult(generation, revision, intent)) return *current;
    queue(guest_.query(generation, revision, intent));
    result.pending = true;
    return result;
}

std::unique_ptr<RoomSession> createClientRoom(Content content, std::filesystem::path replayPath) {
    return std::make_unique<ClientRoom>(std::move(content), std::move(replayPath));
}
} // namespace wizard::central
