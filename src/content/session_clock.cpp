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
        if (clockFrozen_ || technicalPaused_ || state.result != -1) continue;
        const auto summary = engine_.actionSummary(p);
        const bool actionable = summary.actionable;
        const bool automatic = automaticClockPhases_ && !summary.hasNonPhaseAction &&
            state.phase != Phase::Main && state.phaseStep == PhaseStep::Body &&
            !state.decision && !state.chain;
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
    if (clockFrozen_ || technicalPaused_ || engine_.state().result != -1) { clockAt_ = now; return; }
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
        if (result.accepted) {
            commands_.push_back({expired, Timeout{}, engine_.digest()});
            appendEvents(result.events);
            changed();
        } else handleFault(expired, Timeout{}, result);
    }
    if (expired >= 0) syncClock();
    else if (elapsed) ++clock_.sequence;
}
void MatchSession::freezeClock(bool frozen) {
    tick();
    clockFrozen_ = frozen || technicalPaused_;
    if (now_) clockAt_ = now_();
    syncClock();
}
ActionOptions MatchSession::queryActions(PlayerId viewer, std::uint64_t, std::uint64_t revision,
                                               const ActionIntent &intent) {
    ActionOptions out;
    if (viewer < 0 || viewer > 1) { out.error = "invalid_viewer"; return out; }
    if (revision != revision_) { out.error = "stale_query"; return out; }
    if (technicalPaused_) { out.error = "technical_paused"; return out; }
    return engine_.queryActions(viewer, intent);
}
GameView MatchSession::viewFor(PlayerId viewer) const {
    if (viewer < 0 || viewer > 1) throw std::invalid_argument("invalid viewer");
    if (!viewCache_[viewer] || viewRevision_[viewer] != revision_) {
        viewCache_[viewer] = engine_.viewFor(viewer);
        viewCache_[viewer]->actionSummary = engine_.actionSummary(viewer);
        viewRevision_[viewer] = revision_;
    }
    auto view = *viewCache_[viewer];
    view.clock = clock_;
    view.technicalPaused = technicalPaused_;
    return view;
}
} // namespace wizard
