#include "wizard/replay.hpp"
#include <algorithm>

namespace wizard {
void MatchSession::enableClock(bool automaticPhases, TimeSource now) {
    clock_ = {};
    clock_.enabled = true;
    automaticClockPhases_ = automaticPhases;
    now_ = std::move(now);
    clockAt_ = now_();
    replenishedTurn_ = {};
    syncClock();
}
void MatchSession::syncClock() {
    if (!clock_.enabled) return;
    const auto &state = engine_.state();
    clock_.runningPlayers = {};
    for (int p = 0; p < 2; ++p) {
        const int turn = state.players[p].ownTurn;
        if (turn > replenishedTurn_[p]) {
            clock_.remainingMs[p] = std::min<std::int64_t>(300000,
                clock_.remainingMs[p] + 60000LL * (turn - replenishedTurn_[p]));
            replenishedTurn_[p] = turn;
        }
        if (clockFrozen_ || state.result != -1) continue;
        const auto view = engine_.viewFor(p);
        bool actionable = false, onlyPhase = true;
        for (const auto &action : view.actions) {
            if (std::holds_alternative<Surrender>(action.command)) continue;
            actionable = true;
            if (!std::holds_alternative<AdvancePhase>(action.command)) onlyPhase = false;
        }
        const bool automatic = automaticClockPhases_ && onlyPhase &&
            view.phase != Phase::Main && view.phaseStep == PhaseStep::Body &&
            !view.decision && !view.chain;
        clock_.runningPlayers[p] = actionable && !automatic;
    }
    clock_.running = clock_.runningPlayers[0] || clock_.runningPlayers[1];
    clock_.runningPlayer = clock_.runningPlayers[0] == clock_.runningPlayers[1] ? -1 :
        (clock_.runningPlayers[0] ? 0 : 1);
    ++clock_.sequence;
    if (now_) clockAt_ = now_(); // Exclude synchronous rule/automatic processing.
}
void MatchSession::tick() {
    if (!clock_.enabled) return;
    const auto now = now_();
    const auto elapsed = std::max<std::int64_t>(0,
        std::chrono::duration_cast<std::chrono::milliseconds>(now - clockAt_).count());
    clockAt_ += std::chrono::milliseconds(elapsed);
    if (clockFrozen_ || engine_.state().result != -1) { clockAt_ = now; return; }
    PlayerId expired = -1;
    std::int64_t firstExpiry = elapsed + 1;
    for (int p = 0; p < 2; ++p) if (clock_.runningPlayers[p]) {
        if (clock_.remainingMs[p] <= elapsed && clock_.remainingMs[p] < firstExpiry) {
            expired = p; firstExpiry = clock_.remainingMs[p];
        }
    }
    const auto charge = expired < 0 ? elapsed : firstExpiry;
    for (int p = 0; p < 2; ++p) if (clock_.runningPlayers[p])
        clock_.remainingMs[p] = std::max<std::int64_t>(0, clock_.remainingMs[p] - charge);
    if (expired >= 0) {
        const auto result = engine_.timeout(expired);
        if (result.accepted) commands_.push_back({expired, Timeout{}, engine_.digest()});
    }
    if (expired >= 0) syncClock();
    else if (elapsed) ++clock_.sequence;
}
void MatchSession::freezeClock(bool frozen) {
    tick();
    clockFrozen_ = frozen;
    if (now_) clockAt_ = now_();
    syncClock();
}
GameView MatchSession::viewFor(PlayerId viewer) const {
    auto view = engine_.viewFor(viewer);
    view.clock = clock_;
    return view;
}
} // namespace wizard
