#pragma once
#include "wizard/content.hpp"
#include "wizard/core/engine.hpp"
#include "wizard/command_codec.hpp"
#include "wizard/session.hpp"
#include <chrono>
#include <functional>
namespace wizard {
struct RecordedCommand { PlayerId actor{}; Command command; std::string digest; };
class MatchSession : public Session {
public:
    MatchSession(Content content,std::uint32_t seed);
    MatchSession(Content content,MatchConfig config);
    CommandResult submit(PlayerId,const Command&) override;
    GameView viewFor(PlayerId viewer) const override;
    std::uint64_t revision() const override { return commands_.size(); }
    bool started() const override { return true; }
    bool blocked() const override { return false; }
    void tick() override;
    using Clock = std::chrono::steady_clock;
    using TimeSource = std::function<Clock::time_point()>;
    void enableClock(bool automaticPhases, TimeSource now = [] { return Clock::now(); });
    void freezeClock(bool frozen);
    const GameEngine& engine() const { return engine_; }
    Json recording() const;
    void save(const std::filesystem::path&) const;
private:
    Content content_; MatchConfig config_; GameEngine engine_; std::vector<RecordedCommand> commands_;
    MatchClockView clock_;
    TimeSource now_;
    Clock::time_point clockAt_{};
    std::array<int, 2> replenishedTurn_{};
    bool clockFrozen_{}, automaticClockPhases_{true};
    void syncClock();
};
GameEngine replay(const Content&,const Json&);
}
