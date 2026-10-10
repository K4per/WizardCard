#pragma once
#include "wizard/session.hpp"
#include "wizard/content.hpp"
#include "wizard/core/config.hpp"
#include <functional>
#include <memory>

namespace wizard {
// Room lifecycle is application business; the chosen adapter owns its transport.
class RoomSession : public Session {
  public:
    virtual void host(unsigned short, const PlayerDeck &) = 0;
    virtual void join(const std::string &, unsigned short) = 0;
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
using RoomFactory = std::function<std::unique_ptr<RoomSession>(Content, std::filesystem::path)>;
} // namespace wizard
