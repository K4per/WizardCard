#pragma once
#include "wizard/core/engine.hpp"
#include "wizard/ai.hpp"

namespace wizard::test {
// Test-side observer: assertions may enumerate candidates and retain accepted
// events without restoring those responsibilities to production GameView/State.
struct ObservedView : GameView {
    std::vector<LegalAction> actions;
    ObservedView() = default;
    std::vector<GameEvent> events;
    ObservedView(GameView view) : GameView(std::move(view)) {}
};
inline ObservedView view(const GameEngine &engine, PlayerId viewer) {
    ObservedView result(engine.viewFor(viewer));
    engine.visitLegalActions(viewer,[&](const LegalAction &action) {
        result.actions.push_back(action); return true;
    });
    result.actionSummary = engine.actionSummary(viewer);
    return result;
}
class ObservedEngine : public GameEngine {
    std::vector<GameEvent> events_;
  public:
    using GameEngine::GameEngine;
    ObservedEngine(GameEngine engine) : GameEngine(std::move(engine)) {
        events_ = takeInitialEvents();
    }
    static ObservedEngine scenario(CardCatalog catalog, GameState state) {
        return GameEngine::scenario(std::move(catalog),std::move(state));
    }
    CommandResult submit(PlayerId player,const Command &command) {
        auto result = GameEngine::submit(player,command);
        if(result.accepted) events_.insert(events_.end(),result.events.begin(),result.events.end());
        return result;
    }
    CommandResult debug(const DebugOperation &operation) {
        auto result = GameEngine::debug(operation);
        if (result.accepted) events_.insert(events_.end(), result.events.begin(), result.events.end());
        return result;
    }
    CommandResult timeout(PlayerId player) {
        auto result = GameEngine::timeout(player);
        if (result.accepted) events_.insert(events_.end(), result.events.begin(), result.events.end());
        return result;
    }
    ObservedView viewFor(PlayerId viewer) const {
        auto result = test::view(static_cast<const GameEngine &>(*this),viewer);
        result.events = events_;
        return result;
    }
    const std::vector<GameEvent> &events() const { return events_; }
};
inline std::optional<ai::Selection> choose(ObservedView view,ai::Difficulty difficulty,std::size_t budget=512) {
    view.actionSummary.safeAdvance.reset();
    for(const auto &action:view.actions)
        if(std::holds_alternative<PassResponse>(action.command) || std::holds_alternative<AdvancePhase>(action.command)) {
            view.actionSummary.safeAdvance=action.command; break;
        }
    return ai::choose(view,view.actions,difficulty,budget);
}
inline MutationContext mutation(const GameState &state) {
    MutationContext result; static_cast<GameState&>(result)=state; return result;
}
}
