#include "wizard/replay.hpp"
#include "wizard/interaction.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace wizard;
using namespace wizard::ui;
namespace {
GameView interactionView() {
    GameView view; view.viewer=0; view.result=-1;
    for(CardId id : {1u,2u,3u,10u,11u,21u,22u,23u}) { CardView c; c.instance.id=id; view.cards.push_back(c); }
    return view;
}
ActionOptions options(ActionField field, std::initializer_list<CardId> ids) {
    ActionOptions result; result.field=field;
    for(auto id:ids) result.options.push_back({std::to_string(id), "选项", id});
    return result;
}
}
TEST_CASE("incremental interaction selects a host without owning action combinations") {
    auto view=interactionView(); std::size_t calls=0;
    Interaction ui;
    ui.update(view,[&](const ActionIntent &intent) {
        ++calls;
        REQUIRE(intent.source == 1);
        if(intent.selections.empty()) return options(ActionField::Operation,{static_cast<CardId>(CommandKind::StartAnalysis)});
        if(intent.selections.size()==1) return options(ActionField::Source,{1});
        if(intent.selections.size()==2) return options(ActionField::Host,{2,3});
        ActionOptions complete; complete.field=ActionField::Confirm;
        complete.complete=StartAnalysis{1,static_cast<CardId>(std::stoul(intent.selections.back().value))}; return complete;
    });
    REQUIRE_FALSE(ui.select(99)); REQUIRE(ui.select(1)); REQUIRE(ui.groups().size()==1); REQUIRE(ui.activate(0));
    REQUIRE(ui.choosingFormation()); REQUIRE((ui.candidates()==std::vector<CardId>{2,3}));
    REQUIRE_FALSE(ui.pick(99)); REQUIRE(ui.pick(3)); REQUIRE(ui.step()==Step::Confirm);
    REQUIRE(std::get<StartAnalysis>(ui.pending()->command).formation==3); REQUIRE(calls<10);
    ui.cancel(); REQUIRE_FALSE(ui.pending()); REQUIRE(ui.selected()==0);
}
TEST_CASE("incremental targeting retains chosen target when querying payment cards") {
    auto view=interactionView(); Interaction ui;
    ui.update(view,[](const ActionIntent &intent) {
        if(intent.selections.empty()) return options(ActionField::Operation,{static_cast<CardId>(CommandKind::PrepareCast)});
        if(intent.selections.size()==1) return options(ActionField::Source,{1});
        if(intent.selections.size()==2) return options(ActionField::Target,{10,11});
        if(intent.selections.size()==3) return intent.selections.back().value=="10" ? options(ActionField::Discard,{21,22}) : options(ActionField::Discard,{23});
        ActionOptions complete; complete.complete=PrepareCast{1,static_cast<CardId>(std::stoul(intent.selections[2].value)),static_cast<CardId>(std::stoul(intent.selections[3].value))}; return complete;
    });
    REQUIRE(ui.select(1)); REQUIRE(ui.activate(0)); REQUIRE(ui.pick(10)); REQUIRE(ui.step()==Step::Cost);
    REQUIRE((ui.candidates()==std::vector<CardId>{21,22})); REQUIRE_FALSE(ui.pick(23)); REQUIRE(ui.pick(22));
    REQUIRE(encodeCommand(ui.pending()->command)==encodeCommand(PrepareCast{1,10,22}));
    ui.update(view); REQUIRE_FALSE(ui.pending());
}
TEST_CASE("refresh discards response selection and stale offered decision identities") {
    auto view=interactionView(); view.decision=PendingDecision{9,0,DecisionKind::Response};
    Interaction ui; ui.update(view);
    const LegalAction old{"response",1,0,Respond{8,1,"counter",0,7}};
    ui.offer(old); REQUIRE_FALSE(ui.pending());
    ui.offer({"pass",0,0,PassResponse{9}}); REQUIRE(ui.pending());
    view.decision=PendingDecision{10,0,DecisionKind::Response}; ui.update(view); REQUIRE_FALSE(ui.pending());
    ui.offer({"pass",0,0,PassResponse{9}}); REQUIRE_FALSE(ui.pending());
    view.phaseGate=17; ui.update(view); ui.offer({"advance",0,0,AdvancePhase{16}}); REQUIRE_FALSE(ui.pending());
}
TEST_CASE("automatic phase scheduling consumes summary without enumerating candidates") {
    auto view=interactionView(); view.phase=Phase::Draw; view.phaseStep=PhaseStep::Body; view.active=view.viewer;
    view.actionSummary.safeAdvance=AdvancePhase{42};
    REQUIRE(automaticAdvance(view,true));
    view.actionSummary.hasNonPhaseAction=true; REQUIRE_FALSE(automaticAdvance(view,true));
    view.actionSummary.hasNonPhaseAction=false; view.technicalPaused=true; REQUIRE_FALSE(automaticAdvance(view,true));
}
TEST_CASE("drag is an uncommitted intent and never guesses removal") {
    auto view=interactionView(); Interaction ui;
    ui.update(view,[](const ActionIntent &intent) {
        if(intent.selections.empty()) return options(ActionField::Operation,{static_cast<CardId>(CommandKind::StartAnalysis),static_cast<CardId>(CommandKind::Abandon)});
        if(intent.selections.size()==1) return options(ActionField::Source,{1});
        if(intent.selections.size()==2) return options(ActionField::Host,{2});
        ActionOptions out; out.complete=StartAnalysis{1,2}; return out;
    });
    REQUIRE_FALSE(ui.drop(1,0,Zone::Ash)); REQUIRE_FALSE(ui.pending());
    REQUIRE(ui.drop(1,2,Zone::Analysis)); REQUIRE(ui.pending());
    REQUIRE(std::get<StartAnalysis>(ui.pending()->command).formation==2);
}
TEST_CASE("hand order is private presentation state and synchronizes draws and departures") {
    GameView v;v.viewer=0;for(CardId id:{1u,2u,3u}){CardView c;c.instance.id=id;c.instance.zone=Zone::Hand;c.definition.cost=static_cast<int>(4-id);v.cards.push_back(c);}
    HandOrder hand;hand.sync(v);REQUIRE(hand.moveBefore(0,3,1));REQUIRE((hand.cards(0)==std::vector<CardId>{3,1,2}));
    REQUIRE_FALSE(hand.moveBefore(0,99,1));REQUIRE_FALSE(hand.moveBefore(0,1,99));
    auto enemy=v;enemy.viewer=1;enemy.cards.clear();CardView c;c.instance.id=8;c.instance.owner=1;c.instance.zone=Zone::Hand;enemy.cards.push_back(c);hand.sync(enemy);
    REQUIRE((hand.cards(1)==std::vector<CardId>{8}));REQUIRE((hand.cards(0)==std::vector<CardId>{3,1,2}));
    v.cards[0].instance.zone=Zone::Ash;c.instance.id=4;c.instance.owner=0;v.cards.push_back(c);hand.sync(v);
    REQUIRE((hand.cards(0)==std::vector<CardId>{3,2,4}));hand.sort(v,true);REQUIRE((hand.cards(0)==std::vector<CardId>{4,3,2}));
    REQUIRE(v.cards[1].instance.id==2); // Sorting never reorders authoritative/projection data.
}

TEST_CASE("release starts at forty life and healing cannot exceed forty") {
    auto content=loadContent(std::filesystem::path(WIZARD_SOURCE_DIR)/"assets");
    MatchSession session(content,42);
    REQUIRE(session.engine().state().players[0].life==40);
    REQUIRE(session.engine().state().players[1].life==40);
    MutationContext state; static_cast<GameState &>(state)=session.engine().state();
    state.players[0].life=36;
    EffectResolver::apply(state,content.catalog,Trigger{},Effect{EffectKind::Heal,4});
    REQUIRE(state.players[0].life==40);
    EffectResolver::apply(state,content.catalog,Trigger{},Effect{EffectKind::Heal,20});
    REQUIRE(state.players[0].life==40);
}
