#include "wizard/interaction.hpp"
#include "wizard/network.hpp"
#include <random>
namespace wizard::net {
namespace {
PlayerDeck readDeck(const Json &j, const CardCatalog &c) {
    PlayerDeck d{j.at("baseFormation"), j.at("cards").get<std::vector<std::string>>()};
    auto e = Rules::deckErrors(c, d);
    if (!e.empty())
        throw std::runtime_error(e.front());
    return d;
}
Json deckJson(const PlayerDeck &d) {
    return {{"baseFormation", d.baseFormation}, {"cards", d.cards}};
}
} // namespace
HostSession::HostSession(Content c) : content_(std::move(c)) {
    for (auto &d : decks_)
        d.cards = content_.deck;
}
void HostSession::deck(const PlayerDeck &d) {
    if (started())
        throw std::runtime_error("对局已开始");
    decks_[hostPlayer_] = readDeck(deckJson(d), content_.catalog);
    ready_ = {};
}
void HostSession::configure(bool first, bool automatic) {
    if (started())
        throw std::runtime_error("对局已开始");
    if (hostPlayer_ != (first ? 0 : 1))
        std::swap(decks_[0], decks_[1]);
    hostPlayer_ = first ? 0 : 1;
    automatic_ = automatic;
    ready_ = {};
}
void HostSession::ready() {
    if (!joined_ || ended_)
        throw std::runtime_error("等待另一位玩家加入");
    ready_[hostPlayer_] = true;
    if (ready_[0] && ready_[1] && !match_) {
        MatchConfig c;
        c.players = decks_;
        // Existing rules choose the first seat from the seed. Select a seed whose
        // first seat is zero, so room hostPlayer_ implements the requested order
        // without changing rules, command semantics or replay format.
        for (unsigned attempt = 0; attempt < 128; ++attempt) {
            c.seed = std::random_device{}();
            auto next = std::make_unique<MatchSession>(content_, c);
            if (next->engine().state().first == 0) {
                match_ = std::move(next);
                break;
            }
        }
        if (!match_)
            throw std::runtime_error("无法生成房间先后手种子，请重试");
        ++revision_;
    }
}
GameView HostSession::view() const {
    if (!match_)
        throw std::runtime_error("对局未开始");
    return match_->engine().viewFor(hostPlayer_);
}
Json HostSession::room() const {
    return {{"type", "Room"},    {"session", session_},     {"hostPlayer", hostPlayer_}, {"ready", ready_},
            {"joined", joined_}, {"automatic", automatic_}, {"started", started()},      {"ended", ended_}};
}
Json HostSession::snapshot(PlayerId p) const {
    if (!match_)
        return room();
    return {{"type", "View"},
            {"session", session_},
            {"revision", std::to_string(revision_)},
            {"view", encodeView(match_->engine().viewFor(p))}};
}
Json HostSession::recording() const {
    if (!match_)
        throw std::runtime_error("没有可保存的对局");
    return match_->recording();
}
CommandResult HostSession::submit(const Command &c) {
    if (!match_ || frozen_ || ended_)
        return {false, "连接中断或对局已结束", {}, {}, "disconnected"};
    auto r = match_->submit(hostPlayer_, c);
    if (r.accepted)
        ++revision_;
    r.events.erase(
        std::remove_if(r.events.begin(), r.events.end(),
                       [&](const GameEvent &e) { return e.audience != -1 && e.audience != hostPlayer_; }),
        r.events.end());
    if (r.waiting && r.waiting->player != hostPlayer_)
        r.waiting.reset();
    return r;
}
bool HostSession::advance() {
    if (!match_ || frozen_ || ended_ || !automatic_)
        return false;
    const auto &s = match_->engine().state();
    auto p = s.decision ? s.decision->player : s.active;
    auto a = ui::automaticAdvance(match_->engine().viewFor(p), true);
    if (!a)
        return false;
    auto r = match_->submit(p, *a);
    if (r.accepted)
        ++revision_;
    return r.accepted;
}
Json HostSession::receive(const Json &j) {
    const auto type = j.at("type").get<std::string>();
    if (ended_)
        throw std::runtime_error("房间已结束");
    if (type == "Hello") {
        auto expected = hello(content_);
        for (const auto *k : {"protocol", "rules", "cards", "hash"})
            if (j.at(k) != expected.at(k))
                throw std::runtime_error("协议、规则或卡池版本不一致");
        if (joined_)
            throw std::runtime_error("房间已满，请使用重连凭据");
        joined_ = true;
        return {{"type", "Joined"}, {"session", session_}, {"credential", credential_}, {"room", room()}};
    }
    if (type == "Resume") {
        if (!joined_ || j.at("session") != session_ || j.at("credential") != credential_)
            throw std::runtime_error("重连凭据无效");
        frozen_ = false;
        return {{"type", "Resumed"}, {"room", room()}, {"snapshot", snapshot(1 - hostPlayer_)}};
    }
    if (!joined_ || j.at("session") != session_)
        throw std::runtime_error("房间会话已过期");
    if (type == "Deck") {
        if (started())
            throw std::runtime_error("对局已开始");
        decks_[1 - hostPlayer_] = readDeck(j.at("deck"), content_.catalog);
        ready_ = {};
        return room();
    }
    if (type == "Ready") {
        if (started())
            return room();
        ready_[1 - hostPlayer_] = true;
        if (ready_[hostPlayer_])
            ready();
        return room();
    }
    if (type == "Command") {
        auto seq = number(j.at("sequence"));
        auto found = replies_.find(seq);
        if (found != replies_.end()) {
            if (found->second.at("request") != j.at("request"))
                throw std::runtime_error("请求序号冲突");
            return found->second;
        }
        Json result = {{"type", "Rejected"},
                       {"request", j.at("request")},
                       {"sequence", j.at("sequence")},
                       {"error", "过期或无效的操作"},
                       {"revision", std::to_string(revision_)}};
        if (seq != nextSequence_) {
            result["error"] = "请求序号已过期或乱序";
            return result;
        }
        ++nextSequence_;
        if (match_ && !frozen_ && number(j.at("revision")) == revision_) {
            try {
                auto command = unwireCommand(j.at("command"));
                auto v = match_->engine().viewFor(1 - hostPlayer_);
                auto decision = v.decision ? v.decision->id : 0;
                if (number(j.at("decision")) != decision)
                    throw std::runtime_error("决策已过期");
                auto r = match_->submit(1 - hostPlayer_, command);
                result["error"] = r.error;
                if (r.accepted) {
                    ++revision_;
                    result["type"] = "Accepted";
                    result["revision"] = std::to_string(revision_);
                }
            } catch (const std::exception &e) {
                result["error"] = e.what();
            }
        }
        replies_[seq] = result;
        while (replies_.size() > 256)
            replies_.erase(replies_.begin());
        return result;
    }
    if (type == "Replay") {
        if (!match_ || match_->engine().state().result == -1)
            throw std::runtime_error("完整复盘仅在正常终局后提供");
        return {{"type", "Replay"}, {"recording", recording()}};
    }
    throw std::runtime_error("未知消息类型");
}
void GuestSession::receive(const Json &j) {
    auto type = j.at("type").get<std::string>();
    if (type == "Joined") {
        session_ = j.at("session");
        credential_ = j.at("credential");
        room_ = j.at("room");
    } else if (type == "Resumed") {
        room_ = j.at("room");
        resumed_ = true;
        receive(j.at("snapshot"));
    } else if (type == "Room") {
        if (j.at("session") != session_)
            throw std::runtime_error("房间会话不一致");
        room_ = j;
    } else if (type == "View") {
        if (j.at("session") != session_)
            throw std::runtime_error("视图会话不一致");
        auto revision = number(j.at("revision"));
        if (revision < revision_ || (started_ && revision == revision_ && !resumed_))
            return;
        auto view = decodeView(j.at("view"), content_.catalog);
        view_ = std::move(view);
        revision_ = revision;
        started_ = true;
    } else if (type == "Accepted" || type == "Rejected") {
        if (pending_ && pending_->at("request") == j.at("request")) {
            pending_.reset();
            ++sequence_;
        }
    } else
        throw std::runtime_error("未知主机消息类型");
}
Json GuestSession::command(const Command &c) {
    if (!started_ || pending_)
        throw std::runtime_error("等待对局或操作确认");
    pending_ = Json{{"type", "Command"},
                    {"session", session_},
                    {"sequence", std::to_string(sequence_)},
                    {"request", token()},
                    {"revision", std::to_string(revision_)},
                    {"decision", std::to_string(view_.decision ? view_.decision->id : 0)},
                    {"command", wireCommand(c)}};
    return *pending_;
}
Json GuestSession::resume() const {
    return {{"type", "Resume"}, {"session", session_}, {"credential", credential_}};
}
} // namespace wizard::net
