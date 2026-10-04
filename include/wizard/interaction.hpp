#pragma once
#include "wizard/core.hpp"

namespace wizard::ui {
enum class Step { Inspect, Target, Cost, Confirm };
class HandOrder {
public:
    void sync(const GameView&);
    bool moveBefore(PlayerId, CardId source, CardId before); // before=0 appends
    void sort(const GameView&, bool byCost);
    const std::vector<CardId>& cards(PlayerId p) const { return order_.at(p); }
private:
    std::array<std::vector<CardId>,2> order_;
};
struct ActionGroup { std::string title; std::vector<LegalAction> options; };
// Presentation-only selection state. Rules still validate every submitted command.
class Interaction {
public:
    void update(GameView view);
    bool select(CardId card);
    bool activate(std::size_t group);
    bool pick(CardId card);
    void offer(const LegalAction& action);
    void cancel();
    bool drop(CardId source, CardId target, Zone destination); // Select intent only; never submit.
    CardId selected() const { return selected_; }
    Step step() const { return step_; }
    std::vector<ActionGroup> groups() const;
    std::vector<CardId> candidates() const;
    const std::optional<LegalAction>& pending() const { return pending_; }
    const GameView& view() const { return view_; }
    static std::string intent(const LegalAction&,const std::optional<PendingDecision>&);
    static CardId target(const Command&);
    static CardId costCard(const Command&);
private:
    GameView view_; CardId selected_{}; Step step_{Step::Inspect};
    std::vector<LegalAction> options_; std::optional<LegalAction> pending_;
    void advance();
};
}
