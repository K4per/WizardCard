#pragma once
#include "wizard/central.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace wizard::central {
using Connection = std::uint64_t;
struct Delivery {
    Connection connection{};
    Json message;
};
using TokenSource = std::function<std::string()>;
using SeedSource = std::function<std::uint32_t()>;
using TimeSource = std::function<std::chrono::steady_clock::time_point()>;

// In-process authoritative room registry. The TLS runtime feeds decoded JSON
// here; no socket type crosses this boundary, which keeps rule tests fast and
// lets the production runtime serialize a room on its own strand.
class Authority {
  public:
    Authority(Content, std::string instanceId, TokenSource = {}, SeedSource = {},
              std::filesystem::path dataDirectory = {}, TimeSource = {});
    std::vector<Delivery> receive(Connection, const Json &);
    std::vector<Delivery> disconnect(Connection);
    std::vector<Delivery> tick();
    std::size_t roomCount() const;
    const std::string &instanceId() const { return instanceId_; }

  private:
    struct Client {
        std::string token;
        bool welcomed{};
        std::string room;
        int seat{-1};
        bool connected{};
        std::chrono::steady_clock::time_point disconnected{};
    };
    struct Room {
        std::string code;
        Connection owner{};
        std::array<Connection, 2> connections{};
        std::array<std::string, 2> tokens;
        std::array<PlayerDeck, 2> decks;
        std::array<bool, 2> ready{};
        bool ownerFirst{true};
        bool automatic{true};
        RoomPhase phase{RoomPhase::Waiting};
        std::string abortReason;
        std::uint32_t seed{};
        std::unique_ptr<MatchSession> match;
        std::chrono::steady_clock::time_point created{};
        std::chrono::steady_clock::time_point disconnectedAt{};
    };
    Content content_;
    std::string instanceId_;
    TokenSource tokenSource_;
    SeedSource seedSource_;
    std::filesystem::path dataDirectory_;
    TimeSource now_;
    mutable std::mutex mutex_;
    std::map<Connection, Client> clients_;
    std::map<std::string, Room> rooms_;
    std::uint64_t codeCounter_{};

    std::vector<Delivery> receiveLocked(Connection, const Json &);
    std::vector<Delivery> roomMessage(Room &, Connection, const Json &);
    std::vector<Delivery> roomSnapshot(const Room &, bool state) const;
    std::string newCode();
    std::string newToken();
    std::uint32_t newSeed();
    std::chrono::steady_clock::time_point currentTime() const;
    Room &roomFor(const Client &);
    const Room &roomFor(const Client &) const;
    void begin(Room &);
    Json roomJson(const Room &, int) const;
    Json snapshot(const Room &, int) const;
    void abort(Room &, const std::string &);
};
} // namespace wizard::central
