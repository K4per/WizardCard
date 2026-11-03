#pragma once
#include "wizard/session.hpp"
#include "wizard/content.hpp"
#include "wizard/core/config.hpp"
#include <functional>
#include <memory>

namespace wizard {
struct ServerEndpoint {
    std::string hostname{"127.0.0.1"};
    unsigned short port{24443};
    std::string trustedCa;
};
enum class RoomEntry { Create, Join };
struct RoomConnectOptions {
    ServerEndpoint endpoint;
    RoomEntry entry{RoomEntry::Create};
    std::string roomCode;
};
enum class RoomPhase { Waiting, Playing, Completed, Aborted };
// Room lifecycle is application business; the chosen adapter owns its transport.
class RoomSession : public Session {
  public:
    using Session::submit;
    virtual void host(unsigned short, const PlayerDeck &) = 0;
    virtual void join(const std::string &, unsigned short) = 0;
    virtual void connect(const RoomConnectOptions &options, const PlayerDeck &deck) {
        if (options.entry == RoomEntry::Create) host(options.endpoint.port, deck);
        else join(options.endpoint.hostname, options.endpoint.port);
    }
    virtual void deck(const PlayerDeck &) = 0;
    virtual void configure(bool hostFirst, bool automatic) = 0;
    virtual void ready() = 0;
    virtual CommandResult submit(const Command &) = 0;
    virtual bool save(std::string &) = 0;
    virtual bool leave(std::string &, bool force = false) = 0;
    virtual void stop() = 0;
    virtual void reconnect() = 0;
    virtual bool isHost() const = 0;
    virtual bool connected() const = 0;
    virtual bool ended() const = 0;
    virtual bool closing() const = 0;
    virtual bool replayDelivered() const = 0;
    virtual bool takeReset() = 0;
    virtual const GameView &view() const = 0;
    virtual const Json &room() const = 0;
    virtual const std::string &notice() const = 0;
};
class ExternalRoomTransport {
  public:
    virtual ~ExternalRoomTransport() = default;
    virtual const ServerEndpoint &endpoint() const = 0;
    virtual bool transportIsOpen() const = 0;
    virtual void transportOpened() = 0;
    virtual void transportLost(const std::string &) = 0;
    virtual void receiveTransportMessage(const Json &) = 0;
    virtual std::vector<Json> drainTransport() = 0;
};
using RoomFactory = std::function<std::unique_ptr<RoomSession>(Content, std::filesystem::path)>;
} // namespace wizard
