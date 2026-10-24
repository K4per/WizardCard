#pragma once
#include "wizard/core/instances.hpp"
#include "wizard/core/catalog.hpp"
#include "wizard/core/commands.hpp"
#include "wizard/core/events.hpp"
#include "wizard/core/chain.hpp"

namespace wizard {
struct CommandResult {
    bool accepted{};
    std::string error;
    std::vector<GameEvent> events;
    std::optional<PendingDecision> waiting;
    std::string errorCode;
    bool pending{}; // frontend submission queued; authority has not acknowledged it yet
};
struct LegalAction {
    std::string label;
    CardId source{}, target{};
    Command command;
};
struct PlayerView {
    int life{}, mana{}, load{}, capacity{}, handCount{}, deckCount{}, ownTurn{}, actionsPlayed{};
    int temporaryLife{}, blockedActionTurn{-1};
    std::vector<DamageType> resistances, immunities;
    int sideDeckCount{}, exhaustion{};
    bool infiniteLife{}, infiniteMana{}, infiniteLoad{};
    int lifeLimit{maximumLife}, sevenMode{};
};
struct CardView {
    CardInstance instance;
    CardDefinition definition;
    int effectiveRings{}, occupiedRings{}, turnsToReady{};
    bool hidden{};
    int effectiveCastCost{};
    int effectiveSettingCost{};
};
// Session-owned projection. The rule kernel never derives elapsed wall time.
struct MatchClockView {
    bool enabled{}, running{};
    std::array<std::int64_t, 2> remainingMs{300000, 300000};
    std::array<bool, 2> runningPlayers{};
    PlayerId runningPlayer{-1};
    std::uint64_t sequence{};
};
struct GameView {
    PlayerId viewer{}, active{};
    Phase phase{};
    int result{};
    std::array<PlayerView, 2> players;
    std::vector<CardView> cards;
    std::vector<TemporaryLoad> temporary;
    std::vector<TemporaryLife> temporaryLives;
    std::optional<PendingDecision> decision;
    std::vector<GameEvent> events;
    std::vector<LegalAction> actions;
    PhaseStep phaseStep{};
    DecisionId phaseGate{};
    std::optional<ChainState> chain;
    std::vector<Trigger> triggers; // only visible choices of this viewer's trigger-order decision
    std::uint64_t eventBase{}; // projection-only offset for bounded network event history
    bool opening{};
    std::array<bool, 2> initialFormationConfirmed{};
    CardId initialFormation{}; // Only this viewer's confirmed entity while opening.
    std::uint64_t globalTurn{1};
    MatchClockView clock;
};
inline std::size_t firstNewEvent(const GameView& before,const GameView& after) {
    auto end=before.eventBase+before.events.size();
    return end<=after.eventBase?0:static_cast<std::size_t>(std::min<std::uint64_t>(after.events.size(),end-after.eventBase));
}
} // namespace wizard
