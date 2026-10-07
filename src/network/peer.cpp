#include "wizard/network.hpp"
namespace wizard::net {
namespace {
using Clock = std::chrono::steady_clock;
using namespace std::chrono_literals;
} // namespace
Peer::Peer(Content c, std::filesystem::path path) : content_(std::move(c)), replayPath_(std::move(path)) {
    received_ = heartbeat_ = lost_ = retry_ = connecting_ = Clock::now();
}
void Peer::host(unsigned short port, const PlayerDeck &d) {
    tcp_.listen(port);
    host_ = std::make_unique<HostSession>(content_);
    host_->deck(d);
    room_ = host_->room();
    notice_ = "房间已创建，等待对方连接";
}
void Peer::join(const std::string &address, unsigned short port) {
    address_ = address;
    port_ = port;
    guest_ = std::make_unique<GuestSession>(content_);
    tcp_.connect(address, port);
    connecting_ = Clock::now();
    notice_ = "正在连接房主…";
}
bool Peer::persist() {
    if (!host_ || !host_->started())
        return true;
    try {
        writeAtomicJson(replayPath_, host_->recording());
        saveError_.clear();
        return true;
    } catch (const std::exception &e) {
        saveError_ = std::string("复盘保存失败：") + e.what();
        notice_ = saveError_;
        return false;
    }
}
void Peer::tick() {
    if (ended_ && !departure_)
        return;
    auto now = Clock::now();
    try {
        auto messages = tcp_.poll();
        if (tcp_.connected() && !connected_) {
            connected_ = true;
            authenticated_ = false;
            received_ = heartbeat_ = now;
            sentRevision_ = ~std::uint64_t{0};
            if (guest_)
                tcp_.send(guest_->canResume() ? guest_->resume() : hello(content_));
        }
        for (const auto &j : messages) {
            auto type = j.at("type").get<std::string>();
            if (type == "Heartbeat") {
                if (!authenticated_)
                    throw std::runtime_error("握手未完成");
                received_ = now;
                continue;
            }
            if (host_) {
                if (!authenticated_ && type != "Hello" && type != "Resume")
                    throw std::runtime_error("请先完成握手");
                if (type == "Hello" || type == "Resume") {
                    if (type == "Resume" && lost_ != Clock::time_point{} && now - lost_ > 60s)
                        throw std::runtime_error("重连期限已过");
                    auto response = host_->receive(j);
                    tcp_.send(response);
                    authenticated_ = true;
                    host_->freeze(false);
                    reset_ = type == "Resume";
                    notice_ = reset_ ? "对方已重连" : "对方已加入";
                } else if (type == "ReplayReceived") {
                    if (j.at("session") != host_->session() || !host_->started() ||
                        host_->view().result == -1)
                        throw std::runtime_error("非法复盘确认");
                    replayDelivered_ = true;
                } else if (type == "End") {
                    host_->end();
                    ended_ = true;
                    notice_ = started() && view_.result != -1 ? "对方已离开房间" : "对方已离开，对局中止";
                    persist();
                } else {
                    auto response = host_->receive(j);
                    if (type == "Command" && response.at("type") == "Accepted")
                        persist();
                    if (response.at("type") == "Replay")
                        queueReplay(response.at("recording"));
                    else
                        tcp_.send(response);
                }
            } else {
                if (type == "End") {
                    ended_ = true;
                    notice_ = "房主已关闭房间，对局结束";
                } else if (type == "Error") {
                    notice_ = j.at("error");
                    ended_ = true;
                } else if (type == "ReplayBegin") {
                    if (!authenticated_ || !guest_->started() || guest_->view().result == -1)
                        throw std::runtime_error("终局前禁止完整复盘");
                    replaySize_ = static_cast<std::size_t>(number(j.at("size")));
                    if (replaySize_ > 32 * maxMessage)
                        throw std::runtime_error("复盘超过32MiB");
                    incomingReplay_.clear();
                } else if (type == "ReplayChunk") {
                    if (!replaySize_ || !guest_->started() || guest_->view().result == -1)
                        throw std::runtime_error("非法复盘数据");
                    auto data = j.at("data").get<std::string>();
                    if (data.size() > replaySize_ - incomingReplay_.size())
                        throw std::runtime_error("复盘数据超长");
                    incomingReplay_ += data;
                } else if (type == "ReplayComplete") {
                    if (incomingReplay_.size() != replaySize_)
                        throw std::runtime_error("复盘数据不完整");
                    auto record = Json::parse(incomingReplay_);
                    (void)replay(content_, record);
                    replay_ = std::move(record);
                    incomingReplay_.clear();
                    replaySize_ = 0;
                    std::string e;
                    if (!save(e))
                        notice_ = e;
                    tcp_.send({{"type", "ReplayReceived"}, {"session", guest_->room().at("session")}});
                } else {
                    guest_->receive(j);
                    if (type == "Joined" || type == "Resumed") {
                        authenticated_ = true;
                        notice_ = "已连接房主";
                        if (type == "Resumed" && guest_->retry())
                            tcp_.send(*guest_->retry());
                        if(type=="Resumed" && !replay_.is_null())
                            tcp_.send({{"type","ReplayReceived"},{"session",guest_->room().at("session")}});
                    }
                    if (type == "Rejected") {
                        notice_ = j.at("error");
                        closing_ = false; // let a stale leave request be retried from confirmation UI
                    } else if (type == "Accepted")
                        notice_ = "操作已确认";
                }
            }
            received_ = now;
        }
        if (connected_ && !tcp_.connected() && !ended_)
            throw std::runtime_error("对方已断开连接");
        if (authenticated_ && connected_ && now - received_ >= 6s)
            throw std::runtime_error("连接超时，等待重连");
        if (connected_ && !authenticated_ && now - received_ >= 6s)
            throw std::runtime_error("握手超时或版本不匹配");
        if (authenticated_ && connected_ && now - heartbeat_ >= 2s) {
            tcp_.send({{"type", "Heartbeat"}});
            heartbeat_ = now;
        }
        if (host_) {
            if (authenticated_ && connected_ && !closing_ && host_->advance())
                persist();
            room_ = host_->room();
            if (host_->started()) {
                revision_ = host_->revision();
                view_ = host_->view();
                if (authenticated_ && connected_ && revision_ != sentRevision_) {
                    tcp_.send(host_->snapshot(1 - host_->player()));
                    sentRevision_ = revision_;
                }
            }
            if (authenticated_ && connected_ && room_ != roomSent_) {
                tcp_.send(room_);
                roomSent_ = room_;
            }
        } else if (guest_) {
            room_ = guest_->room();
            if (guest_->started()) {
                revision_ = guest_->revision();
                view_ = guest_->view();
                reset_ = guest_->takeResumed() || reset_;
                if (view_.result != -1 && replay_.is_null() && authenticated_ && connected_ &&
                    !replayRequested_) {
                    tcp_.send({{"type", "Replay"}, {"session", room_.at("session")}});
                    replayRequested_ = true;
                }
            }
        }
        streamReplay();
        if (closing_ && (!started() || view_.result != -1 || !connected())) {
            std::string e;
            leave(e);
        }
        if (guest_ && !connected_ && !ended_ && now - retry_ >= 2s) {
            if (!guest_->canResume()) {
                if (now - connecting_ > 6s) {
                    ended_ = true;
                    notice_ = "连接失败，请检查房主地址、端口与防火墙";
                }
            } else if (now - lost_ <= 60s) {
                retry_ = now;
                tcp_.connect(address_, port_);
            } else {
                ended_ = true;
                notice_ = "重连超时，对局中止";
            }
        }
    } catch (const std::exception &e) {
        if (guest_ && guest_->started()) {
            view_ = guest_->view();
            revision_ = guest_->revision();
        }
        notice_ = e.what();
        bool was = authenticated_;
        if (host_ && tcp_.connected()) {
            try {
                tcp_.send({{"type", "Error"}, {"error", notice_}});
                tcp_.poll();
            } catch (...) {
            }
        }
        tcp_.disconnect();
        outgoingReplay_.clear();
        incomingReplay_.clear();
        replaySize_ = 0;
        connected_ = authenticated_ = false;
        replayRequested_ = false;
        if (was) {
            lost_ = now;
            reset_ = true;
        }
        if (host_)
            host_->freeze(true);
    }
}
void Peer::deck(const PlayerDeck &d) {
    if (blocked())
        throw std::runtime_error("连接尚未就绪");
    if (host_)
        host_->deck(d);
    else
        tcp_.send({{"type", "Deck"},
                   {"session", room_.at("session")},
                   {"deck", {{"baseFormation", d.baseFormation}, {"cards", d.cards}}}});
}
void Peer::configure(bool first, bool automatic) {
    if (!host_)
        throw std::runtime_error("仅房主可修改房间");
    host_->configure(first, automatic);
    room_ = host_->room();
}
void Peer::ready() {
    if (blocked())
        throw std::runtime_error("连接尚未就绪");
    if (host_) {
        host_->ready();
        persist();
    } else
        tcp_.send({{"type", "Ready"}, {"session", room_.at("session")}});
}
CommandResult Peer::submit(const Command &c) {
    if (blocked())
        return {false, "等待连接恢复或操作确认", {}, {}, "network_wait"};
    if (host_) {
        auto r = host_->submit(c);
        if (r.accepted) {
            persist();
            revision_ = host_->revision();
            view_ = host_->view();
        }
        return r;
    }
    tcp_.send(guest_->command(c));
    return {true, {}, {}, {}, {}, true};
}
bool Peer::save(std::string &error) {
    if (host_) {
        if (!host_->started()) {
            error = "对局尚未开始";
            return false;
        }
        if (persist())
            return true;
        error = saveError_;
        return false;
    }
    if (replay_.is_null()) {
        error = "完整复盘仅在终局后由房主提供";
        return false;
    }
    try {
        writeAtomicJson(replayPath_, replay_);
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool Peer::leave(std::string &error, bool force) {
    if (force) {
        if (connected_) {
            try {
                tcp_.send({{"type", "End"}});
                tcp_.poll();
            } catch (...) {
            }
        }
        stop();
        return true;
    }
    if (departure_) {
        if ((!tcp_.queued() && outgoingReplay_.empty() && endQueued_) || !connected_) {
            stop();
            return true;
        }
        error = "等待发送最后的对局记录…";
        return false;
    }
    if (started() && view_.result == -1 && connected() && !ended_) {
        if (!closing_) {
            auto r = submit(Surrender{});
            if (!r.accepted) {
                error = r.error;
                return false;
            }
            closing_ = true;
        }
        error = "等待投降确认…";
        return false;
    }
    if (host_ && host_->started() && !save(error)) {
        closing_ = false;
        return false;
    }
    if (guest_ && connected() && started() && view_.result != -1 && replay_.is_null() && !ended_) {
        closing_ = true;
        error = "等待房主发送终局复盘…";
        return false;
    }
    if (connected_) {
        try {
            if (host_ && host_->started() && view_.result != -1) {
                tcp_.send(host_->snapshot(1 - host_->player()));
                queueReplay(host_->recording());
            }
            departure_ = closing_ = true;
            streamReplay();
            // tick owns receive processing; polling here could consume an ACK
            // or a command without delivering it to the authoritative session.
            if (tcp_.queued() || !outgoingReplay_.empty() || !endQueued_) {
                error = "等待发送最后的对局记录…";
                return false;
            }
        } catch (const std::exception &e) {
            error = e.what();
        }
    }
    stop();
    return true;
}
void Peer::queueReplay(const Json &record) {
    if (!outgoingReplay_.empty())
        return;
    // ASCII JSON keeps byte-sized chunks valid UTF-8 even when a boundary would
    // otherwise split a Chinese code point. Concatenation restores exact JSON.
    outgoingReplay_ = record.dump(-1, ' ', true);
    if (outgoingReplay_.size() > 32 * maxMessage) {
        outgoingReplay_.clear();
        throw std::runtime_error("复盘超过32MiB，请从房主获取文件");
    }
    replayOffset_ = 0;
    replayDelivered_ = false;
    tcp_.send({{"type", "ReplayBegin"}, {"size", std::to_string(outgoingReplay_.size())}});
}
void Peer::streamReplay() {
    if (!connected_)
        return;
    while (!outgoingReplay_.empty() && tcp_.queued() < 2 * maxMessage) {
        auto size = std::min<std::size_t>(65536, outgoingReplay_.size() - replayOffset_);
        tcp_.send({{"type", "ReplayChunk"}, {"data", outgoingReplay_.substr(replayOffset_, size)}});
        replayOffset_ += size;
        if (replayOffset_ == outgoingReplay_.size()) {
            tcp_.send({{"type", "ReplayComplete"}});
            outgoingReplay_.clear();
        }
    }
    if (departure_ && outgoingReplay_.empty() && !endQueued_ &&
        (!host_ || !started() || view_.result == -1 || replayDelivered_)) {
        tcp_.send({{"type", "End"}});
        endQueued_ = true;
    }
}
void Peer::stop() {
    tcp_.close();
    ended_ = true;
    closing_ = false;
    connected_ = authenticated_ = false;
    if (host_)
        host_->end();
}
void Peer::reconnect() {
    if (!guest_ || !guest_->canResume() || ended_)
        throw std::runtime_error("当前连接不能重连");
    tcp_.disconnect();
    connected_ = authenticated_ = false;
    lost_ = Clock::now();
    retry_ = lost_ - 2s;
    reset_ = true;
    notice_ = "重新连接房主…";
}
} // namespace wizard::net
