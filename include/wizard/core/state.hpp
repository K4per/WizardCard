#pragma once
#include "wizard/core/instances.hpp"
#include "wizard/core/chain.hpp"
#include "wizard/core/config.hpp"
#include "wizard/core/commands.hpp"
#include "wizard/core/events.hpp"

namespace wizard {
struct PlayerState {
    int life{startingLife}, mana{}, ownTurn{}, actionsPlayed{}, formations{}, sealRemovals{};
    bool drawFailed{}, surrendered{};
    std::vector<CardId> deck;
    int temporaryLife{}, blockedActionTurn{-1};
    int exhaustion{};
    int damageReduction{}, forceDamageReduction{};
    bool nextDamageImmune{};
    int wordLockUntilOwnDraw{};
    int siqueDamageCap{}, siqueCapUntilOwnTurn{};
    std::uint64_t siqueLifePaymentTurn{};
    bool debugInfiniteLife{}, debugInfiniteMana{}, debugInfiniteLoad{};
    int lifeLimit{maximumLife}; // One-time changes; continuous modifiers are derived from field sources.
    std::uint64_t firstLifePaymentTurn{};
    int sevenShiftUntilOwnTurn{};
    bool sevenShifted{};
};
struct PaymentChoice {
    int mana{}, life{}, lifePlayer{-1};
};
struct PendingPayment {
    PlayerId player{};
    Command command;
    std::optional<PendingDecision> previousDecision;
    std::vector<PaymentChoice> choices;
    std::string key;
};
enum class PhasePreparationStage { Income, Draw, FormationTriggers, SpellTriggers, Concentration, Analysis, Complete };
enum class PhaseCleanupStage { FormationTriggers, SpellTriggers, Duration, Casting, Discard, Complete };
struct PhaseEntryContinuation { PhasePreparationStage stage{PhasePreparationStage::Income}; std::uint64_t after{}; };
struct PhaseCleanupContinuation { PhaseCleanupStage stage{PhaseCleanupStage::FormationTriggers}; std::uint64_t after{}; };
using PhaseContinuation = std::variant<PhaseEntryContinuation, PhaseCleanupContinuation>;
struct ConcentrationContinuation { CardId source{}; };
struct ReturnContinuation { CardId source{}; };
using MaintenanceContinuation = std::variant<std::monostate, ConcentrationContinuation, ReturnContinuation>;
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
    std::vector<TemporaryLife> temporaryLives;
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
    PhaseContinuation phaseContinuation;
    MaintenanceContinuation maintenance;
    std::uint64_t nextFieldOrder{1};
    bool returnRequested{}, timingPending{};
    bool skipCastPhase{}; // Keep the end-turn intent while response and cleanup choices resolve.
    bool opening{};
    std::array<CardId, 2> initialFormations{};
    bool debugUsed{};
    std::optional<PendingPayment> payment;
    CardId maintenanceCard() const {
        return std::visit([](const auto &value) -> CardId {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_same_v<T, std::monostate>) return 0;
            else return value.source;
        }, maintenance);
    }

};
// Per-transaction mutation surface. Only the GameState base is a checkpoint.
// Events and payment input are outputs/inputs of execution, never rule history.
struct MutationContext : GameState {
    std::vector<GameEvent> events;
    std::optional<PaymentChoice> paymentInput;
};
} // namespace wizard
