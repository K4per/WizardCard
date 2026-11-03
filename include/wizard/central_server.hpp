#pragma once
#include "wizard/central.hpp"
#include "wizard/central_journal.hpp"
#include <chrono>
#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <optional>

namespace wizard::central {
using Connection = std::uint64_t;
struct Delivery { Connection connection{}; Json message; };
using TokenSource = std::function<std::string()>;
using SeedSource = std::function<std::uint32_t()>;
using TimeSource = std::function<std::chrono::steady_clock::time_point()>;
struct AuthorityLimits {
    std::size_t rooms{20}, connections{64};
    std::size_t replayBytes{32 * net::maxMessage};
    std::chrono::milliseconds reconnect{60000}, waiting{900000}, match{14400000}, terminal{600000};
};
// Registry locks protect bindings only; each room has independent authority
// serialization. The runtime additionally schedules each room on its own strand.
class Authority {
  public:
    Authority(Content, std::string instanceId, TokenSource, SeedSource,
              AuthorityLimits = {}, TimeSource = {});
    std::vector<Delivery> receive(Connection, const Json &);
    std::vector<Delivery> disconnect(Connection);
    std::vector<Delivery> tick();
    std::vector<Delivery> tickRoom(const std::string &);
    std::vector<Delivery> abortRoom(const std::string &, const std::string &reason);
    std::vector<std::string> roomCodes() const;
    std::string route(Connection) const;
    std::size_t roomCount() const;
    std::size_t credentialCount() const;
    std::optional<JournalWrite> takeWrite(const std::string &);
    std::vector<Delivery> persisted(const JournalResult &);
    const std::string &instanceId() const { return instanceId_; }

  private:
    using Clock = std::chrono::steady_clock;
    struct Client { std::string token, room; int seat{-1}; Json entry; };
    struct CachedReply { std::string payload; Json reply; };
    struct Room {
        mutable std::mutex mutex;
        std::string code, storageId;
        std::array<Connection, 2> connections{};
        std::array<std::string, 2> tokens;
        std::array<PlayerDeck, 2> decks;
        std::array<bool, 2> ready{}, left{};
        std::array<std::optional<Clock::time_point>, 2> deadline;
        std::array<std::uint64_t, 2> sequence{1, 1}, eventEpoch{}, eventCursor{};
        std::array<std::map<std::uint64_t, CachedReply>, 2> replies;
        std::array<std::size_t, 2> replyBytes{};
        bool ownerFirst{true}, automatic{true}, clockFrozen{}, storageBlocked{};
        RoomPhase phase{RoomPhase::Waiting};
        std::string abortReason;
        std::uint32_t seed{};
        std::shared_ptr<MatchSession> match;
        Clock::time_point touched{}, started{}, terminal{}, clockSent{};
        RecordingCursor recordingCursor{};
        std::size_t recordingBytes{};
        RecordingCursor journalCursor{};
        bool journalStarted{}, writeQueued{}, terminalStored{};
        std::uint64_t writeSerial{}, durableOffset{};
        Clock::time_point retryAt{};
        std::string journalTail, journalState;
        std::optional<JournalWrite> pendingWrite;
        std::shared_ptr<const std::string> replay;
    };
    struct Credential { std::weak_ptr<Room> room; int seat{}; };
    Content content_;
    std::string instanceId_;
    TokenSource tokenSource_;
    SeedSource seedSource_;
    AuthorityLimits limits_;
    TimeSource now_;
    mutable std::mutex mutex_;
    std::map<Connection, Client> clients_;
    std::map<std::string, std::shared_ptr<Room>> rooms_;
    std::map<std::string, Credential> credentials_;
    std::uint64_t roomSerial_{};

    std::vector<Delivery> entry(Connection, const Json &);
    std::vector<Delivery> roomMessage(Room &, Connection, int, const Json &);
    std::vector<Delivery> publish(Room &, bool state, Connection only = 0);
    std::shared_ptr<Room> lookup(const std::string &) const;
    std::string newCode();
    void begin(Room &);
    void releaseCredential(Room &, int);
    bool refresh(Room &);
    bool updateRecordingBudget(Room &);
    void capture(Room &);
    Json summary(const Room &) const;
    bool reconnecting(const Room &) const;
    void freeze(Room &);
    Json roomJson(const Room &, int) const;
    Json snapshot(const Room &, int) const;
    Json joined(const Room &, int, bool resumed = false) const;
    void abort(Room &, const std::string &);
};
} // namespace wizard::central
