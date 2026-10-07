#pragma once
#include "wizard/content.hpp"
#include <chrono>
#include <memory>

namespace wizard::net {
inline constexpr std::size_t maxMessage = 1024 * 1024, maxReceive = 2 * maxMessage, maxSend = 4 * maxMessage;
Json encodeView(const GameView &);
GameView decodeView(const Json &, const CardCatalog &);
Json wireCommand(const Command &);
Command unwireCommand(Json);
std::uint64_t number(const Json &);
std::string token();
Json hello(const Content &);
std::vector<unsigned char> frame(const Json &);
class Decoder {
    std::vector<unsigned char> bytes_;

  public:
    std::vector<Json> feed(const unsigned char *, std::size_t);
    void clear() {
        bytes_.clear();
    }
};
// Protocol and authoritative rules have no socket or frontend dependency.
class HostSession {
    Content content_;
    std::unique_ptr<MatchSession> match_;
    std::array<PlayerDeck, 2> decks_;
    std::array<bool, 2> ready_{};
    std::map<std::uint64_t, Json> replies_;
    std::uint64_t nextSequence_{1}, revision_{};
    PlayerId hostPlayer_{};
    bool joined_{}, frozen_{}, ended_{}, automatic_{true};
    std::string session_{token()}, credential_{token()};

  public:
    explicit HostSession(Content);
    Json receive(const Json &); // only the guest connection calls this
    void deck(const PlayerDeck &);
    void configure(bool hostFirst, bool automatic);
    void ready();
    CommandResult submit(const Command &);
    bool advance();
    void freeze(bool value) {
        frozen_ = value;
    }
    void end() {
        ended_ = true;
        frozen_ = true;
    }
    bool started() const {
        return bool(match_);
    }
    bool frozen() const {
        return frozen_;
    }
    bool ended() const {
        return ended_;
    }
    PlayerId player() const {
        return hostPlayer_;
    }
    std::uint64_t revision() const {
        return revision_;
    }
    const std::string &session() const {
        return session_;
    }
    Json snapshot(PlayerId) const;
    Json room() const;
    Json recording() const;
    GameView view() const;
};
class GuestSession {
    Content content_;
    std::string session_, credential_;
    std::uint64_t sequence_{1}, revision_{};
    std::optional<Json> pending_;
    GameView view_;
    Json room_;
    bool started_{}, resumed_{};

  public:
    explicit GuestSession(Content c) : content_(std::move(c)) {}
    void receive(const Json &);
    Json command(const Command &);
    Json resume() const;
    bool canResume() const {
        return !credential_.empty();
    }
    bool pending() const {
        return bool(pending_);
    }
    const std::optional<Json> &retry() const {
        return pending_;
    }
    bool started() const {
        return started_;
    }
    bool takeResumed() {
        bool r = resumed_;
        resumed_ = false;
        return r;
    }
    std::uint64_t revision() const {
        return revision_;
    }
    const GameView &view() const {
        return view_;
    }
    const Json &room() const {
        return room_;
    }
};
// Nonblocking byte-stream adapter, replaceable by Godot transport.
class Tcp {
    struct Impl;
    std::unique_ptr<Impl> impl_;

  public:
    Tcp();
    ~Tcp();
    Tcp(const Tcp &) = delete;
    Tcp &operator=(const Tcp &) = delete;
    void listen(unsigned short);
    void connect(const std::string &, unsigned short);
    bool connected() const;
    bool listening() const;
    std::size_t queued() const;
    void close();
    void disconnect();
    void send(const Json &);
    std::vector<Json> poll();
};
// Frontend-independent controller: poll every frame, even while settings/animations are open.
class Peer {
    Content content_;
    Tcp tcp_;
    std::unique_ptr<HostSession> host_;
    std::unique_ptr<GuestSession> guest_;
    Json room_, roomSent_, replay_;
    GameView view_;
    bool connected_{}, authenticated_{}, closing_{}, ended_{}, reset_{}, replayRequested_{}, departure_{};
    std::uint64_t revision_{}, sentRevision_{~std::uint64_t{0}};
    std::string address_, notice_, saveError_;
    std::string outgoingReplay_, incomingReplay_;
    std::size_t replayOffset_{}, replaySize_{};
    bool endQueued_{};
    bool replayDelivered_{};
    unsigned short port_{};
    std::filesystem::path replayPath_;
    std::chrono::steady_clock::time_point received_, heartbeat_, lost_, retry_, connecting_;
    bool persist();
    void queueReplay(const Json &);
    void streamReplay();

  public:
    explicit Peer(Content, std::filesystem::path);
    void host(unsigned short, const PlayerDeck &);
    void join(const std::string &, unsigned short);
    void tick();
    void deck(const PlayerDeck &);
    void configure(bool hostFirst, bool automatic);
    void ready();
    CommandResult submit(const Command &);
    bool save(std::string &);
    bool leave(std::string &, bool force = false);
    void stop();
    void reconnect(); // preserves credentials and pending request; also used by integration probes
    bool isHost() const {
        return bool(host_);
    }
    bool started() const {
        return host_ ? host_->started() : guest_ && guest_->started();
    }
    bool connected() const {
        return authenticated_ && connected_;
    }
    bool ended() const {
        return ended_;
    }
    bool closing() const {
        return closing_;
    }
    bool replayDelivered() const {
        return replayDelivered_;
    }
    bool takeReset() {
        bool r = reset_;
        reset_ = false;
        return r;
    }
    bool blocked() const {
        return !connected() || ended_ || closing_ || (guest_ && guest_->pending());
    }
    std::uint64_t revision() const {
        return revision_;
    }
    const GameView &view() const {
        return view_;
    }
    const Json &room() const {
        return room_;
    }
    const std::string &notice() const {
        return notice_;
    }
};
} // namespace wizard::net
