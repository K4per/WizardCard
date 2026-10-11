#pragma once
#include "wizard/core/rules.hpp"
#include "wizard/core/view.hpp"
#include "wizard/core/config.hpp"

namespace wizard {
class GameEngine {
  public:
    GameEngine(CardCatalog catalog, std::array<std::vector<std::string>, 2> decks, std::uint32_t seed);
    GameEngine(CardCatalog catalog, const MatchConfig &);
    // Explicit scenario entry point, used by regression tools; never accepts client state.
    static GameEngine scenario(CardCatalog catalog, GameState state);
    CommandResult submit(PlayerId actor, const Command &command);
    GameView viewFor(PlayerId viewer) const;
    const GameState &state() const {
        return state_;
    }
    const CardCatalog &catalog() const {
        return catalog_;
    }
    std::string canonicalState() const;
    std::string cycleKey() const;
    std::string digest() const;

  private:
    GameEngine() = default;
    CardCatalog catalog_;
    GameState state_;
    std::string execute(PlayerId, const Command &);
    std::string executeV2(PlayerId, const Command &);
    bool phaseV2();
    bool returnSpellsV2();
    bool contentEffectV2();
    bool chooseContentEffectV2(PlayerId, DecisionKind, CardId);
    void actionTimingV2(ResponseWindow = ResponseWindow::Action);
    std::string responseSetupV2(PlayerId, const Command &);
    std::vector<LegalAction> responseSetupsV2(PlayerId, DecisionId) const;
    void analysisCompletedV2(CardId);
    void quickAnalysisV2(CardId, CardId);
    void castingTraitsV2(CardId);
    void pump();
    void decision(PlayerId, DecisionKind, std::vector<std::uint32_t>, bool = false);
    void enqueuePhase(bool end);
    void beginEffect(CardId, AfterEffect);
    void finishEffect();
    void startChain(ResponseWindow, ChainLink, PlayerId priority);
    void finishLink(bool success);
    void passResponse();
    std::string validateResponse(PlayerId, const Respond &) const;
    std::vector<Respond> responsesFor(PlayerId, DecisionId) const;
    void startTrigger(Trigger);
    std::string prepare(PlayerId, const PrepareCast &, bool deferred);
    std::vector<PrepareCast> preparationsFor(PlayerId, DecisionId) const;
    std::string stateKey(bool logical) const;
    std::vector<LegalAction> legalActions(PlayerId) const;
};
} // namespace wizard
