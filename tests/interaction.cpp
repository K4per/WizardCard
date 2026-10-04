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
