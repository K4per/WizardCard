#pragma once
#include "wizard/core/instances.hpp"
#include "wizard/core/catalog.hpp"
#include "wizard/core/commands.hpp"
#include "wizard/core/events.hpp"
#include "wizard/core/chain.hpp"
#include "wizard/core/action_query.hpp"

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
    PhaseStep phaseStep{};
    DecisionId phaseGate{};
    std::optional<ChainState> chain;
    std::vector<Trigger> triggers; // only visible choices of this viewer's trigger-order decision
    bool opening{};
    std::array<bool, 2> initialFormationConfirmed{};
    CardId initialFormation{}; // Only this viewer's confirmed entity while opening.
    std::uint64_t globalTurn{1};
    MatchClockView clock;
    ActionSummary actionSummary;
    bool technicalPaused{};
};
} // namespace wizard
