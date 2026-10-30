#pragma once
#include "wizard/content.hpp"
#include "wizard/core/engine.hpp"
#include "wizard/command_codec.hpp"
#include "wizard/session.hpp"
#include <chrono>
#include <functional>
namespace wizard {
Json encodeDebugOperation(const DebugOperation &);
DebugOperation decodeDebugOperation(const Json &);
CommandResult applyRecordedCommand(GameEngine &, const Json &, bool legacy = false);
struct RecordedCommand { PlayerId actor{}; Command command; std::string digest; std::optional<DebugOperation> debug; };
class MatchSession : public Session {
public:
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;
    MatchSession(Content content,std::uint32_t seed);
    MatchSession(Content content,MatchConfig config);
    CommandResult submit(PlayerId,const Command&) override;
    void enableDebug(bool enabled);
    bool debugEnabled() const { return debugEnabled_; }
    bool canRewindPreviousTurn() const;
    CommandResult debug(const DebugOperation &);
    GameView viewFor(PlayerId viewer) const override;
    std::uint64_t revision() const override { return revision_; }
    bool started() const override { return true; }
    bool blocked() const override { return technicalPaused_; }
    bool technicalPaused() const override { return technicalPaused_; }
    const MatchClockView &clock() const { return clock_; }
    MatchClockView clockView() const override { return clock_; }
    std::size_t commandCount() const { return commands_.size(); }
    EventBatch eventsSince(PlayerId, std::uint64_t epoch, std::uint64_t cursor) const override;
    ActionOptions queryActions(PlayerId, std::uint64_t generation, std::uint64_t revision,
                               const ActionIntent &) override;
    const Json &diagnostic() const { return diagnostic_; }
    void saveDiagnostic(const std::filesystem::path &) const;
    std::size_t debugCheckpointCount() const { return turnCheckpoints_.size(); }
    void tick() override;
    void enableClock(bool automaticPhases, TimeSource now = [] { return std::chrono::steady_clock::now(); });
    void freezeClock(bool frozen);
    const GameEngine& engine() const { return engine_; }
    Json recording() const;
    void save(const std::filesystem::path&) const;
private:
    Content content_; MatchConfig config_; GameEngine engine_; std::vector<RecordedCommand> commands_;
    std::vector<GameEvent> events_;
    std::array<std::vector<std::size_t>, 2> visibleEvents_;
    std::uint64_t revision_{}, eventEpoch_{1};
    bool technicalPaused_{};
    Json diagnostic_;
    mutable std::array<std::optional<GameView>, 2> viewCache_;
    mutable std::array<std::uint64_t, 2> viewRevision_{~std::uint64_t{0}, ~std::uint64_t{0}};
    MatchClockView clock_;
    TimeSource now_;
    Clock::time_point clockAt_{};
    std::array<int, 2> replenishedTurn_{};
    bool clockFrozen_{}, automaticClockPhases_{true};
    struct TurnCheckpoint {
        GameEngine engine;
        std::size_t position;
        MatchClockView clock;
        std::array<int, 2> replenishedTurn;
        std::size_t eventCount{};
    };
    struct TurnMarker {
        std::uint64_t turn{};
        std::size_t position{}, eventCount{};
        MatchClockView clock;
        std::array<int, 2> replenishedTurn;
    };
    std::vector<TurnCheckpoint> turnCheckpoints_;
    std::vector<TurnMarker> turnMarkers_;
    bool debugEnabled_{};
    void captureTurn();
    void syncClock();
    void appendEvents(const std::vector<GameEvent> &);
    void truncateEvents(std::size_t);
    void changed();
    void handleFault(PlayerId, const Command &, const CommandResult &);
};
// Observer runs only after each state has passed its recorded digest check.
GameEngine replay(const Content&, const Json&,
                  const std::function<void(std::size_t, const GameEngine&)> &observer = {},
                  const std::function<void(std::size_t, const std::vector<GameEvent>&)> &eventObserver = {});
}
