#pragma once
#include "wizard/core/instances.hpp"
#include "wizard/core/chain.hpp"
#include "wizard/core/config.hpp"
#include "wizard/core/commands.hpp"
#include "wizard/core/events.hpp"

namespace wizard {
struct PlayerState {
    int life{startingLife}, mana{1}, ownTurn{}, actionsPlayed{}, formations{}, sealRemovals{};
    bool drawFailed{}, surrendered{};
    std::vector<CardId> deck;
    int temporaryLife{}, blockedActionTurn{-1};
    int exhaustion{};
};
enum class Flow {
    Draw,
    Income,
    Progress,
    PrepareTriggers,
    Main,
    Cast,
    EndTriggers,
    Expire,
    Duration,
    Discard,
    Finish
};
struct GameState {
    std::array<PlayerState, 2> players;
    std::map<CardId, CardInstance> cards;
    std::vector<TemporaryLoad> temporary;
    PlayerId active{}, first{};
    std::uint64_t globalTurn{1};
    Phase phase{Phase::Draw};
    Flow flow{Flow::Draw};
    PhaseStep phaseStep{PhaseStep::Enter};
    DecisionId phaseGate{};
    bool durationApplied{};
    int result{-1}; // -1 running, 0/1 winner, 2 draw
    std::uint32_t rng{}, nextLoad{1}, nextTrigger{1}, nextBatch{1};
    DecisionId nextDecision{1};
    ChainId nextChain{1};
    LinkId nextLink{1};
    EffectQueue queue;
    std::optional<PendingDecision> decision;
    std::optional<EffectFrame> effect;
    std::optional<ChainState> chain;
    std::deque<ChainLink> deferredPreparations;
    std::vector<GameEvent> events;
};
} // namespace wizard
