#pragma once
#include "wizard/central.hpp"
#include "wizard/network.hpp"
#include <deque>

namespace wizard::central {
class ClientRoom final : public RoomSession, public ExternalRoomTransport {
    Content content_;
    std::filesystem::path replayPath_;
    net::GuestSession guest_;
    RoomConnectOptions options_;
    PlayerDeck deck_;
    Json entry_, room_ = Json::object(), replay_;
    std::deque<Json> outgoing_;
    std::string token_, instanceId_, candidateToken_, candidateInstance_;
    std::string notice_, replayBytes_;
    std::uint64_t revision_{};
    std::size_t replaySize_{};
    std::chrono::steady_clock::time_point lastReceive_{}, heartbeatAt_{}, replayRetryAt_{};
    bool configured_{}, transportOpen_{}, connected_{}, ended_{}, closing_{}, reset_{};
    bool resumePending_{}, entrySent_{}, replayDelivered_{}, replayRequested_{}, leaveSent_{};

    void queue(Json);
    void sendEntry();
    void requestReplay();
    void completeReplay();
    void advanceLeave();
    void updateRoom(const Json &);

  public:
    ClientRoom(Content, std::filesystem::path);
    void connect(const RoomConnectOptions &, const PlayerDeck &) override;
    void host(unsigned short, const PlayerDeck &) override;
    void join(const std::string &, unsigned short) override;
    void deck(const PlayerDeck &) override;
    void configure(bool hostFirst, bool automatic) override;
    void ready() override;
    CommandResult submit(const Command &) override;
    CommandResult submit(PlayerId, const Command &) override;
    bool save(std::string &) override;
    bool leave(std::string &, bool force = false) override;
    void stop() override;
    void reconnect() override;
    bool isHost() const override;
    bool connected() const override { return connected_ && transportOpen_; }
    bool ended() const override { return ended_; }
    bool closing() const override { return closing_; }
    bool replayDelivered() const override { return replayDelivered_; }
    bool takeReset() override;
    const GameView &view() const override { return guest_.view(); }
    const Json &room() const override { return room_; }
    const std::string &notice() const override { return notice_; }

    GameView viewFor(PlayerId) const override;
    MatchClockView clockView() const override;
    std::uint64_t revision() const override { return guest_.revision(); }
    bool started() const override { return guest_.started(); }
    bool blocked() const override;
    void tick() override;
    EventBatch eventsSince(PlayerId, std::uint64_t, std::uint64_t) const override;
    bool technicalPaused() const override { return guest_.technicalPaused(); }
    ActionOptions queryActions(PlayerId, std::uint64_t, std::uint64_t,
                               const ActionIntent &) override;

    const ServerEndpoint &endpoint() const override { return options_.endpoint; }
    bool transportIsOpen() const override { return transportOpen_; }
    void transportOpened() override;
    void transportLost(const std::string &) override;
    void receiveTransportMessage(const Json &) override;
    std::vector<Json> drainTransport() override;
};

std::unique_ptr<RoomSession> createClientRoom(Content, std::filesystem::path);
} // namespace wizard::central
