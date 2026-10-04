#include "wizard/content.hpp"
#include "wizard/interaction.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace wizard;
using namespace wizard::ui;
TEST_CASE("card context chooses a legal host then confirms the original command") {
    GameView v;CardView card;card.instance.id=10;v.cards.push_back(card);card.instance.id=20;v.cards.push_back(card);
    v.actions={{"解析A",10,20,StartAnalysis{10,20}},{"解析B",10,21,StartAnalysis{10,21}},{"结束",0,0,Advance{}}};
    Interaction ui;ui.update(v);REQUIRE(ui.select(10));REQUIRE(ui.groups().size()==1);REQUIRE(ui.activate(0));
    REQUIRE(ui.step()==Step::Target);REQUIRE(ui.candidates().size()==2);REQUIRE_FALSE(ui.pick(99));REQUIRE(ui.pick(20));
    REQUIRE(ui.step()==Step::Confirm);REQUIRE(std::get<StartAnalysis>(ui.pending()->command).formation==20);
    ui.update(v);REQUIRE_FALSE(ui.pending());REQUIRE(ui.selected()==0);
}
TEST_CASE("target selection retains only the matching extra cost candidates") {
    GameView v;CardView c;c.instance.id=1;v.cards.push_back(c);
    v.actions={{"a",1,10,PrepareCast{1,10,21}},{"b",1,10,PrepareCast{1,10,22}},{"c",1,11,PrepareCast{1,11,23}}};
    Interaction ui;ui.update(v);ui.select(1);ui.activate(0);REQUIRE(ui.pick(10));REQUIRE(ui.step()==Step::Cost);
    REQUIRE((ui.candidates()==std::vector<CardId>{21,22}));REQUIRE_FALSE(ui.pick(23));REQUIRE(ui.pick(22));
    REQUIRE(encodeCommand(ui.pending()->command)==encodeCommand(PrepareCast{1,10,22}));
}
TEST_CASE("set and replace are separate contexts and hidden cards cannot be selected") {
    GameView v;CardView c;c.instance.id=1;v.cards.push_back(c);v.actions={{"set",1,0,SetFormation{1,0}},{"replace",1,10,SetFormation{1,10}}};
    Interaction ui;ui.update(v);REQUIRE_FALSE(ui.select(99));REQUIRE(ui.select(1));REQUIRE(ui.groups().size()==2);
    ui.activate(0);REQUIRE(ui.step()==Step::Confirm);ui.cancel();ui.select(1);ui.activate(1);REQUIRE(ui.step()==Step::Target);
}

TEST_CASE("drag selects a legal intent but invalid drops preserve the current choice") {
    GameView v;v.viewer=0;
    for(CardId id:{1u,2u,3u}){CardView c;c.instance.id=id;v.cards.push_back(c);}
    v.actions={{"parse",1,2,StartAnalysis{1,2}},{"set",3,0,SetFormation{3,0}},{"replace",3,2,SetFormation{3,2}}};
    Interaction ui;ui.update(v);REQUIRE_FALSE(ui.drop(1,3,Zone::Words));REQUIRE(ui.selected()==0);
    REQUIRE(ui.drop(1,2,Zone::Analysis));REQUIRE(ui.step()==Step::Confirm);
    REQUIRE(encodeCommand(ui.pending()->command)==encodeCommand(StartAnalysis{1,2}));
    REQUIRE_FALSE(ui.drop(3,0,Zone::Analysis));REQUIRE(ui.selected()==1);
    ui.cancel();REQUIRE(ui.drop(3,2,Zone::Analysis));REQUIRE(std::get<SetFormation>(ui.pending()->command).replace==2);
    ui.cancel();REQUIRE(ui.drop(3,0,Zone::Analysis));REQUIRE(std::get<SetFormation>(ui.pending()->command).replace==0);
}
TEST_CASE("drag targeting and discard cost still stop for confirmation") {
    GameView v;v.viewer=0;for(CardId id:{1u,2u,3u,4u}){CardView c;c.instance.id=id;v.cards.push_back(c);}
    v.actions={{"prepare",1,2,PrepareCast{1,2,3}}};Interaction ui;ui.update(v);
    REQUIRE(ui.drop(1,4,Zone::Casting));REQUIRE(ui.step()==Step::Target);
    REQUIRE_FALSE(ui.drop(4,2,Zone::Analysis));REQUIRE(ui.drop(1,2,Zone::Analysis));REQUIRE(ui.step()==Step::Cost);
    REQUIRE_FALSE(ui.drop(3,0,Zone::Action));REQUIRE_FALSE(ui.drop(4,0,Zone::Ash));REQUIRE_FALSE(ui.pending());
    REQUIRE(ui.drop(3,0,Zone::Ash));REQUIRE(ui.step()==Step::Confirm);
    REQUIRE(encodeCommand(ui.pending()->command)==encodeCommand(PrepareCast{1,2,3}));
}
TEST_CASE("drag does not infer destructive removal and routes card-specific destinations") {
    GameView v;CardView c;c.instance.id=1;v.cards.push_back(c);Interaction ui;
    for(Command cmd:{Command{PlayAction{1,0}},Command{PreloadWord{1}},Command{AttachSeal{1,2}}}) {
        v.actions={{"operation",1,0,cmd}};ui.update(v);REQUIRE_FALSE(ui.drop(1,0,Zone::Ash));
        auto zone=std::holds_alternative<PlayAction>(cmd)?Zone::Action:std::holds_alternative<PreloadWord>(cmd)?Zone::Words:Zone::Analysis;
        REQUIRE(ui.drop(1,std::holds_alternative<AttachSeal>(cmd)?2:0,zone));REQUIRE(ui.pending());
    }
    v.actions={{"abandon",1,0,Abandon{1}},{"remove",1,0,RemoveFormation{1}}};ui.update(v);
    REQUIRE_FALSE(ui.drop(1,0,Zone::Ash));REQUIRE_FALSE(ui.pending());
}
TEST_CASE("forced drag choices retain their decision id and cannot use the wrong destination") {
    GameView v;CardView c;c.instance.id=1;v.cards.push_back(c);Interaction ui;
    for(auto kind:{DecisionKind::Discard,DecisionKind::Overflow,DecisionKind::Response,DecisionKind::CastOrder}) {
        v.decision=PendingDecision{73,0,kind,{1},false};v.actions={{"choose",1,0,Choose{73,1}}};ui.update(v);
        REQUIRE_FALSE(ui.drop(1,0,Zone::Action));
        REQUIRE(ui.drop(1,0,(kind==DecisionKind::Discard || kind==DecisionKind::Overflow)?Zone::Ash:Zone::Casting));
        REQUIRE(std::get<Choose>(ui.pending()->command).decision==73);
    }
    ui.update(v);REQUIRE_FALSE(ui.pending());REQUIRE(ui.selected()==0);
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

TEST_CASE("analysis area auto matches one legal formation and prompts for multiple") {
    GameView v;CardView c;c.instance.id=1;v.cards.push_back(c);Interaction ui;
    ui.update(v);REQUIRE_FALSE(ui.drop(1,0,Zone::Analysis));
    v.actions={{"a",1,2,StartAnalysis{1,2}}};ui.update(v);
    REQUIRE(ui.drop(1,0,Zone::Analysis));REQUIRE(ui.pending());
    REQUIRE(std::get<StartAnalysis>(ui.pending()->command).formation==2);
    ui.cancel();REQUIRE(ui.drop(1,99,Zone::Analysis));REQUIRE(ui.pending());
    v.actions.push_back({"b",1,3,StartAnalysis{1,3}});ui.update(v);
    REQUIRE(ui.drop(1,0,Zone::Analysis));REQUIRE(ui.choosingFormation());REQUIRE_FALSE(ui.pending());
    REQUIRE_FALSE(ui.pick(99));REQUIRE(ui.pick(3));REQUIRE(ui.pending());
    REQUIRE(std::get<StartAnalysis>(ui.pending()->command).formation==3);
    ui.cancel();REQUIRE(ui.drop(1,2,Zone::Analysis));REQUIRE(ui.pending());
    REQUIRE(std::get<StartAnalysis>(ui.pending()->command).formation==2);
    ui.cancel();REQUIRE(ui.drop(1,99,Zone::Analysis));REQUIRE(ui.choosingFormation());
    ui.update(v);REQUIRE_FALSE(ui.choosingFormation());REQUIRE_FALSE(ui.pending());
}
TEST_CASE("starting life is twenty with the existing thirty life ceiling") {
    auto content=loadContent(std::filesystem::path(WIZARD_SOURCE_DIR)/"assets");
    MatchSession session(content,42);
    REQUIRE(session.engine().state().players[0].life==20);
    REQUIRE(session.engine().state().players[1].life==20);
    GameState state=session.engine().state();
    EffectResolver::apply(state,content.catalog,Trigger{},Effect{EffectKind::Heal,4});
    REQUIRE(state.players[0].life==24);
    EffectResolver::apply(state,content.catalog,Trigger{},Effect{EffectKind::Heal,20});
    REQUIRE(state.players[0].life==30);
}
