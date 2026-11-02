#pragma once
#include "wizard/core/rules.hpp"
#include "wizard/core/view.hpp"
#include "wizard/core/config.hpp"
#include "wizard/core/debug.hpp"
#include "wizard/core/action_query.hpp"
#include <memory>
#include <functional>
#include <stdexcept>

namespace wizard {
class ExecutionFault : public std::runtime_error {
  public:
    explicit ExecutionFault(const std::string &code) : std::runtime_error(code) {}
};
struct CommandPlan {
    PlayerId actor{};
    Command command;
    std::optional<PaymentChoice> payment;
};
struct PaymentRequest { PendingPayment continuation; };
using PlanningResult = std::variant<std::string, CommandPlan, PaymentRequest>;
enum class StepResult { Progress, AwaitChoice, Finished, Fault };
struct ExecutionContext {
    std::size_t remaining{10000};
    std::string fault;
    // Brent cycle detector: one exact canonical sample, independent of step budget.
    std::string checkpoint;
    std::vector<std::vector<Effect>> checkpointEffects;
    std::size_t cyclePower{1}, cycleLength{};
};
class GameEngine {
  public:
    GameEngine(CardCatalog catalog, std::array<std::vector<std::string>, 2> decks, std::uint32_t seed);
    GameEngine(CardCatalog catalog, const MatchConfig &);
    // Explicit scenario entry point, used by regression tools; never accepts client state.
    static GameEngine scenario(CardCatalog catalog, GameState state);
    CommandResult submit(PlayerId actor, const Command &command);
    CommandResult timeout(PlayerId actor);
    // Authority-only local tooling; session mode must be checked by the caller.
    CommandResult debug(const DebugOperation &);
    GameView viewFor(PlayerId viewer) const;
    const GameState &state() const {
        return state_;
    }
    const CardCatalog &catalog() const {
        return *catalog_;
    }
    std::string canonicalState() const;
    std::string cycleKey() const;
    std::string digest() const;
    std::string legacyDigest() const;
    ActionOptions queryActions(PlayerId, const ActionIntent &, const std::function<bool(const Command &)> &filter = {}) const;
    ActionSummary actionSummary(PlayerId) const;
    void visitLegalActions(PlayerId, const std::function<bool(const LegalAction &)> &, const ActionIntent *intent = nullptr) const;
    std::vector<GameEvent> takeInitialEvents();
    // Pure rule validation. Never advances RNG, allocates rule IDs or executes commands.
    std::string validate(PlayerId, const Command &) const;


  private:
    GameEngine() = default;
    std::shared_ptr<const CardCatalog> catalog_;
    MutationContext state_;
    std::string execute(PlayerId, const Command &);
    void finishOpening();
    CommandResult submitAuthority(PlayerId, const Command &);
    std::string executeV2(PlayerId, const Command &);
    bool phaseV2();
    bool returnSpellsV2();
    bool contentEffectV2();
    bool cardEffect();
    bool chooseCardEffect(PlayerId, ChoiceValue);
    bool resolveResourceEffect();
    bool chooseResourceEffect(PlayerId, ChoiceValue);
    bool resolveFormationEffect();
    bool chooseFormationEffect(PlayerId, ChoiceValue);
    bool resolveSpellEffect();
    bool chooseSpellEffect(PlayerId, ChoiceValue);
    bool resolveRecipeEffect();
    bool chooseRecipeEffect(PlayerId, ChoiceValue);
    std::optional<std::string> sevenAbility(PlayerId, const ActivateAbility &);
    std::optional<PaymentRequest> paymentRequest(PlayerId, const Command &) const;
    PlanningResult plan(PlayerId, const Command &, std::optional<PaymentChoice> = {}) const;
    std::string apply(const CommandPlan &);
    StepResult step(ExecutionContext &);
    bool chooseContentEffectV2(PlayerId, DecisionKind, ChoiceValue);
    void actionTimingV2(ResponseWindow = ResponseWindow::Action);
    std::string responseSetupV2(PlayerId, const Command &);
    std::string activateSpellV252(PlayerId, CardId, CardId, const std::vector<CardId> &, CardId, bool,
                                 DecisionId = 0);
    std::vector<LegalAction> responseSetupsV2(PlayerId, DecisionId) const;
    void visitResponseSetups(PlayerId, DecisionId, const std::function<void(const LegalAction &)> &, const ActionIntent * = nullptr) const;
    void analysisCompletedV2(CardId);
    void quickAnalysisV2(CardId, CardId);
    void castingTraitsV2(CardId);
    void settingCompletedV253(CardId, CardId);
    void pump();
    // Card-only decisions accept entity IDs; every other choice is explicit.
    void decision(PlayerId, DecisionKind, std::vector<CardId>, bool = false);
    void typedDecision(PlayerId, DecisionKind, std::vector<ChoiceValue>, bool = false);
    Command normalizeCommand(const Command &) const;
    void enqueuePhase(bool end);
    void beginEffect(CardId, AfterEffect);
    void finishEffect();
    void startChain(ResponseWindow, ChainLink, PlayerId priority);
    void finishLink(bool success);
    void passResponse();
    std::string validateResponse(PlayerId, const Respond &) const;
    std::string validateActivation(PlayerId, CardId, CardId, const std::vector<CardId> &, CardId, bool, DecisionId) const;
    std::string validatePreparation(PlayerId, const PrepareCast &, bool) const;
    std::string validateResponseSetup(PlayerId, const Command &) const;
    std::string validateAction(PlayerId, const Command &) const;
    std::vector<Respond> responsesFor(PlayerId, DecisionId) const;
    void visitResponses(PlayerId, DecisionId, const std::function<void(const Respond &)> &, const ActionIntent * = nullptr) const;
    void startTrigger(Trigger);
    std::string prepare(PlayerId, const PrepareCast &, bool deferred);
    std::vector<PrepareCast> preparationsFor(PlayerId, DecisionId) const;
    std::string stateKey(bool logical, bool compactEffects = false, bool currentFormat = false) const;
    std::vector<std::vector<Effect>> effectSequences() const;
};
} // namespace wizard
