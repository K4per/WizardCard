#include "wizard/content.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;
namespace {
struct Fixture {
    Content content=loadContent(std::filesystem::path(WIZARD_SOURCE_DIR)/"assets"); GameState state;
    Fixture() {
        state.phase=Phase::Main; state.flow=Flow::Main; state.rng=7;
        for(int p=0;p<2;++p) {
            state.players[p].mana=12; state.players[p].ownTurn=1;
            CardInstance c; c.id=static_cast<CardId>(p+1); c.definition="balance"; c.owner=p; c.zone=Zone::Analysis; c.base=true; state.cards[c.id]=c;
            for(int n=0;n<20;++n) { c={}; c.id=static_cast<CardId>(100+p*20+n); c.owner=p; c.definition="spark"; state.cards[c.id]=c; state.players[p].deck.push_back(c.id); }
        }
    }
    CardInstance& card(CardId id,std::string name,PlayerId p=0,Zone zone=Zone::Hand) {
        CardInstance c; c.id=id;c.definition=std::move(name);c.owner=p;c.zone=zone;state.cards[id]=c;return state.cards.at(id);
    }
    CardInstance& ready(CardId id,std::string name="fireball",PlayerId p=0,CardId host=1) {
        auto& c=card(id,std::move(name),p,Zone::Analysis);c.host=host;c.spell=SpellState::Ready;c.analysisLoad=content.catalog.at(c.definition).cost;return c;
    }
    GameEngine engine() { return GameEngine::scenario(content.catalog,state); }
};
void accept(GameEngine& e,PlayerId p,Command c) { auto r=e.submit(p,c); INFO(r.error); REQUIRE(r.accepted); }
void chooseFirst(GameEngine& e) { auto d=*e.state().decision; accept(e,d.player,Choose{d.id,d.options.front()}); }
}
TEST_CASE("load boundary is inclusive and rejected commands are atomic") {
    Fixture f; f.ready(10); f.state.temporary.push_back({1,0,0,4,2}); auto e=f.engine();
    accept(e,0,PrepareCast{10,0,0}); REQUIRE(Rules::load(e.state(),0)==8);
    f.state.temporary[0].amount=5; auto bad=f.engine(); auto before=bad.canonicalState();
    REQUIRE_FALSE(bad.submit(0,PrepareCast{10,0,0}).accepted); REQUIRE(bad.canonicalState()==before);
}
TEST_CASE("cancel keeps analysis and spent mana and prevents retry") {
    Fixture f; f.ready(10); auto& word=f.card(11,"barbs",1,Zone::Words);word.analysisLoad=4;
    auto e=f.engine(); accept(e,0,PrepareCast{10,0,0}); REQUIRE(e.state().decision->player==1);
    auto decision=e.state().decision->id; auto before=e.digest();
    REQUIRE_FALSE(e.submit(0,Choose{decision,11}).accepted); REQUIRE(e.digest()==before);
    accept(e,1,Choose{decision,11}); const auto& c=e.state().cards.at(10);
    REQUIRE(c.zone==Zone::Analysis); REQUIRE(c.analysisLoad==3); REQUIRE(c.castLoad==0); REQUIRE(e.state().players[0].mana==11);
    REQUIRE_FALSE(e.submit(0,PrepareCast{10,0,0}).accepted); REQUIRE(e.state().cards.at(11).zone==Zone::Ash);
    REQUIRE_FALSE(e.submit(1,Choose{decision,11}).accepted);
}
TEST_CASE("each responder gets at most one word and canceled window closes") {
    Fixture f; f.ready(10); for(auto id:{11u,12u}) { auto& c=f.card(id,"barbs",1,Zone::Words);c.analysisLoad=4; }
    auto e=f.engine(); accept(e,0,PrepareCast{10,0,0}); chooseFirst(e);
    REQUIRE_FALSE(e.state().decision); REQUIRE(e.state().cards.at(12).zone==Zone::Words);
}
TEST_CASE("passing response completes preparation and releases original ring") {
    Fixture f; f.ready(10); f.card(11,"barbs",1,Zone::Words).analysisLoad=4;
    auto e=f.engine(); accept(e,0,PrepareCast{10,0,0}); auto d=*e.state().decision;
    accept(e,1,Choose{d.id,0}); REQUIRE(e.state().cards.at(10).zone==Zone::Casting); REQUIRE(e.state().cards.at(10).host==0);
    REQUIRE(Rules::occupied(e.state(),1)==0);
}
TEST_CASE("formation and dependents leave before overload check") {
    Fixture f; f.card(10,"balance",0,Zone::Analysis); f.ready(11,"fireball",0,10).analysisLoad=6;
    f.ready(12).analysisLoad=7; f.card(13,"ring",0,Zone::Attached).host=11;
    StateMaintenance::leave(f.state,10); StateMaintenance::check(f.state,f.content.catalog);
    REQUIRE(f.state.result==-1); REQUIRE(Rules::load(f.state,0)==7); REQUIRE(f.state.cards.at(13).zone==Zone::Ash);
    REQUIRE(f.state.players[0].sealRemovals==0);
}
TEST_CASE("destroying empty formation can cause overload and base is protected") {
    Fixture f; f.card(10,"balance",1,Zone::Analysis); f.state.temporary.push_back({1,1,0,10,2});
    StateMaintenance::leave(f.state,10); StateMaintenance::check(f.state,f.content.catalog); REQUIRE(f.state.result==0);
    StateMaintenance::leave(f.state,1); REQUIRE(f.state.cards.at(1).zone==Zone::Analysis);
}
TEST_CASE("prepared spell survives former formation destruction") {
    Fixture f;f.card(10,"balance",0,Zone::Analysis);f.ready(11,"spark",0,10);auto e=f.engine();accept(e,0,PrepareCast{11,0,0});
    auto s=e.state();StateMaintenance::leave(s,10);REQUIRE(s.cards.at(11).zone==Zone::Casting);REQUIRE(s.cards.at(11).analysisLoad==1);
}
TEST_CASE("instant success refunds only cast payment before leaving") {
    Fixture f; f.ready(10); f.card(11,"ring",0,Zone::Attached).host=10; f.state.players[0].mana=4;
    auto e=f.engine();accept(e,0,PrepareCast{10,0,0});accept(e,0,Advance{});chooseFirst(e);
    REQUIRE(e.state().players[0].mana==4);REQUIRE(e.state().players[1].life==24);REQUIRE(Rules::load(e.state(),0)==0);REQUIRE(e.state().cards.at(11).zone==Zone::Ash);
}
TEST_CASE("duration two includes casting turn and periodic damage does not refund twice") {
    Fixture f;f.ready(10,"ward");f.card(11,"ring",0,Zone::Attached).host=10;f.state.players[0].mana=4;
    auto e=f.engine();accept(e,0,PrepareCast{10,0,0});accept(e,0,Advance{});chooseFirst(e);
    REQUIRE(e.state().cards.at(10).remaining==1);REQUIRE(Rules::load(e.state(),0)==3);REQUIRE(e.state().players[1].life==28);
    accept(e,1,Advance{});REQUIRE(e.state().players[1].life==26);REQUIRE(e.state().players[0].mana==6);
    accept(e,0,Advance{});REQUIRE(e.state().cards.at(10).zone==Zone::Ash);REQUIRE(Rules::load(e.state(),0)==0);
}
TEST_CASE("invalidated target fizzles without reward or refund") {
    Fixture f;f.card(10,"reservoir",1,Zone::Analysis);auto& c=f.ready(11,"unravel");c.zone=Zone::Casting;c.host=0;c.spell=SpellState::Pending;c.castLoad=2;c.payments.push_back({1,2,2,false});c.targetCard=10;
    f.card(12,"ring",0,Zone::Attached).host=11;f.ready(13,"spark",1,10);f.state.players[0].mana=3;
    auto e=f.engine();accept(e,0,Advance{});chooseFirst(e);REQUIRE(e.state().cards.at(11).zone==Zone::Ash);REQUIRE(e.state().cards.at(10).zone==Zone::Analysis);REQUIRE(e.state().players[0].mana==3);
}
TEST_CASE("action burden outlives card and expires at end") {
    Fixture f;f.card(10,"recall");auto e=f.engine();accept(e,0,PlayAction{10,0});
    REQUIRE(e.state().cards.at(10).zone==Zone::Ash);REQUIRE(Rules::load(e.state(),0)==1);accept(e,0,Advance{});REQUIRE(Rules::load(e.state(),0)==0);
}
TEST_CASE("clear temporary uses player allocation and leaves bound load") {
    Fixture f;f.card(10,"clarity");f.ready(11);f.state.temporary={{1,0,0,1,2},{2,0,0,2,2}};auto e=f.engine();accept(e,0,PlayAction{10,0});
    auto d=*e.state().decision;accept(e,0,Choose{d.id,2});chooseFirst(e);
    REQUIRE_FALSE(e.state().decision);REQUIRE(Rules::load(e.state(),0)==4);REQUIRE(e.state().cards.at(11).analysisLoad==3);
}
TEST_CASE("draw failure persists until full effect completes and ends game") {
    Fixture f;f.card(10,"recall");for(auto id:f.state.players[0].deck) f.state.cards.at(id).zone=Zone::Ash;f.state.players[0].deck.clear();
    auto e=f.engine();accept(e,0,PlayAction{10,0});REQUIRE(e.state().players[0].drawFailed);REQUIRE(e.state().cards.at(10).zone==Zone::Ash);REQUIRE(e.state().result==1);
}
TEST_CASE("simultaneous state failures draw") {
    Fixture f;f.state.players[0].life=0;f.state.players[1].drawFailed=true;StateMaintenance::check(f.state,f.content.catalog);REQUIRE(f.state.result==2);
}
TEST_CASE("replacement is atomic and charges one operation") {
    Fixture f;f.card(10,"reservoir",0,Zone::Analysis);f.card(11,"conduit");f.card(12,"conduit",0,Zone::Analysis);auto e=f.engine();auto before=e.digest();
    REQUIRE_FALSE(e.submit(0,SetFormation{11,10}).accepted);REQUIRE(e.digest()==before);
    accept(e,0,SetFormation{11,12});REQUIRE(e.state().players[0].mana==10);REQUIRE(e.state().players[0].formations==1);
}
TEST_CASE("seal removal must not leave occupied rings over capacity") {
    Fixture f;f.card(10,"ring",0,Zone::Attached).host=1;for(auto id:{11u,12u,13u,14u}) f.ready(id,"spark");auto e=f.engine();
    REQUIRE_FALSE(e.submit(0,RemoveSeal{10}).accepted);accept(e,0,Abandon{14});accept(e,0,RemoveSeal{10});REQUIRE(e.state().players[0].sealRemovals==1);
}
TEST_CASE("forced ring removal asks owner to clean up before checking") {
    Fixture f;f.card(10,"ring",0,Zone::Attached).host=1;for(auto id:{11u,12u,13u,14u}) f.ready(id,"spark");
    f.state.effect=EffectFrame{Trigger{1,1,1,0,10,0,{{EffectKind::Destroy,0}}},0,AfterEffect::None,-1};
    // Seed a driver decision to resume this scenario through the public command boundary.
    f.state.decision=PendingDecision{1,0,DecisionKind::Discard,{119},false};f.state.cards.at(119).zone=Zone::Hand;f.state.players[0].deck.pop_back();
    auto e=f.engine();accept(e,0,Choose{1,119});REQUIRE(e.state().decision->kind==DecisionKind::Overflow);chooseFirst(e);REQUIRE(Rules::occupied(e.state(),1)==3);
}
TEST_CASE("hand overflow pauses end and requires owning player") {
    Fixture f;for(CardId i=10;i<20;++i) f.card(i,"spark");auto e=f.engine();accept(e,0,Advance{});
    REQUIRE(e.state().decision->kind==DecisionKind::Discard);chooseFirst(e);chooseFirst(e);REQUIRE(e.state().active==1);
}
TEST_CASE("periodic triggers preserve chosen order and queued source snapshot") {
    Fixture f;for(CardId id:{10u,11u}) {auto& c=f.ready(id,"ward");c.zone=Zone::Casting;c.host=0;c.spell=SpellState::Active;c.remaining=2;c.targetPlayer=1;}
    f.state.active=1;auto e=f.engine();accept(e,1,Advance{});REQUIRE(e.state().decision->kind==DecisionKind::TriggerOrder);
    auto d=*e.state().decision;accept(e,0,Choose{d.id,d.options.back()});REQUIRE(e.state().players[1].life==26);
}
TEST_CASE("first player skips only first base draw and both get income") {
    auto c=loadContent(std::filesystem::path(WIZARD_SOURCE_DIR)/"assets");GameEngine e(c.catalog,{c.deck,c.deck},42);int first=e.state().first;
    REQUIRE(e.viewFor(first).players[first].handCount==5);REQUIRE(e.state().players[first].mana==3);
    accept(e,first,Advance{});REQUIRE(e.viewFor(1-first).players[1-first].handCount==6);REQUIRE(e.state().players[1-first].mana==3);
}
TEST_CASE("views never expose enemy hand or deck identities") {
    Fixture f;f.card(10,"barbs",1);f.card(11,"recall");auto e=f.engine();accept(e,0,PlayAction{11,0});
    auto v=e.viewFor(0);for(const auto& c:v.cards) {REQUIRE(c.instance.zone!=Zone::Deck);REQUIRE_FALSE((c.instance.owner==1 && c.instance.zone==Zone::Hand));}
    auto enemy=e.viewFor(1);for(const auto& event:enemy.events) REQUIRE(event.audience!=0);
}
TEST_CASE("extra costs validate atomically and survive a canceled preparation") {
    Fixture f;f.card(10,"reservoir",1,Zone::Analysis);f.ready(11,"unravel");f.card(12,"spark");f.card(13,"barbs",1,Zone::Words).analysisLoad=4;
    auto e=f.engine();auto initial=e.digest();REQUIRE_FALSE(e.submit(0,PrepareCast{11,10,999}).accepted);REQUIRE(e.digest()==initial);
    accept(e,0,PrepareCast{11,10,12});chooseFirst(e);REQUIRE(e.state().cards.at(12).zone==Zone::Ash);REQUIRE(e.state().players[0].mana==10);REQUIRE(Rules::load(e.state(),0)==4);
}
TEST_CASE("no state check between sentences of one effect") {
    Fixture f;f.content.catalog.cards.at("fireball").effects={{EffectKind::Damage,6},{EffectKind::Heal,10}};f.state.players[1].life=4;f.ready(10);
    auto e=f.engine();accept(e,0,PrepareCast{10,0,0});accept(e,0,Advance{});chooseFirst(e);REQUIRE(e.state().result==-1);REQUIRE(e.state().players[1].life==8);
}
TEST_CASE("draw failure is not undone by returning a card before the check") {
    Fixture f;for(auto id:f.state.players[0].deck)f.state.cards.at(id).zone=Zone::Ash;f.state.players[0].deck.clear();
    Trigger t;t.owner=0;EffectResolver::apply(f.state,f.content.catalog,t,{EffectKind::Draw,1});
    f.state.cards.at(100).zone=Zone::Deck;f.state.players[0].deck.push_back(100);StateMaintenance::check(f.state,f.content.catalog);REQUIRE(f.state.result==1);
}
TEST_CASE("queued effects remain after their source leaves and stop after defeat") {
    Fixture f;f.card(10,"ward",0,Zone::Ash);f.state.players[1].life=2;
    f.state.queue.append({1,1,0,10,0,1,{{EffectKind::Damage,2}}});
    f.state.queue.append({2,2,0,10,0,1,{{EffectKind::Damage,9}}});
    f.state.decision=PendingDecision{1,0,DecisionKind::TriggerOrder,{1},false};auto e=f.engine();accept(e,0,Choose{1,1});
    REQUIRE(e.state().players[1].life==0);REQUIRE(e.state().result==0);REQUIRE(e.state().queue.items.empty());
}
TEST_CASE("same-event order is active owner then non-active owner and appended batches wait") {
    Fixture f;f.state.queue.append({1,1,0,0,0,1,{{EffectKind::Damage,1}}});f.state.queue.append({2,1,0,0,0,1,{{EffectKind::Damage,2}}});
    f.state.queue.append({3,1,1,0,0,0,{{EffectKind::Damage,3}}});f.state.queue.append({4,2,0,0,0,1,{{EffectKind::Damage,4}}});
    f.state.decision=PendingDecision{1,0,DecisionKind::TriggerOrder,{1,2},false};auto e=f.engine();accept(e,0,Choose{1,2});
    REQUIRE(e.state().players[0].life==27);REQUIRE(e.state().players[1].life==23);
    std::vector<int> values;for(const auto& event:e.state().events)if(event.kind=="effect")values.push_back(event.amount);
    REQUIRE(values.size()==4);REQUIRE(values[0]==2);REQUIRE(values[1]==1);REQUIRE(values[2]==3);REQUIRE(values[3]==4);
}
TEST_CASE("new formations do not grant immediate income and analysis completes next own turn") {
    Fixture f;f.card(10,"conduit");f.card(11,"spark");f.state.players[0].mana=3;auto e=f.engine();accept(e,0,SetFormation{10,0});REQUIRE(e.state().players[0].mana==3);
    accept(e,0,StartAnalysis{11,1});REQUIRE(e.state().cards.at(11).spell==SpellState::Analyzing);accept(e,0,Advance{});REQUIRE(e.state().cards.at(11).spell==SpellState::Analyzing);
    accept(e,1,Advance{});REQUIRE(e.state().cards.at(11).spell==SpellState::Ready);REQUIRE(e.state().players[0].mana==6);
}
TEST_CASE("end triggers resolve and check before expiring temporary load") {
    Fixture f;f.content.catalog.cards.at("ward").onEnd={{EffectKind::Damage,2}};auto& c=f.ready(10,"ward");c.zone=Zone::Casting;c.host=0;c.spell=SpellState::Active;c.remaining=2;c.targetPlayer=1;
    f.state.players[1].life=2;f.state.temporary.push_back({1,0,0,1,1});auto e=f.engine();accept(e,0,Advance{});REQUIRE(e.state().result==0);REQUIRE(e.state().temporary.size()==1);
}
TEST_CASE("long legal and illegal command sequences preserve state invariants") {
    auto content=loadContent(std::filesystem::path(WIZARD_SOURCE_DIR)/"assets");
    for(std::uint32_t seed=1;seed<=8;++seed) {
        GameEngine e(content.catalog,{content.deck,content.deck},seed);std::uint32_t rng=seed;
        for(int n=0;n<100 && e.state().result==-1;++n) {
            int p=e.state().decision?e.state().decision->player:e.state().active;auto view=e.viewFor(p);std::vector<Command> commands;
            for(const auto& a:view.actions)if(!std::holds_alternative<Surrender>(a.command))commands.push_back(a.command);
            REQUIRE_FALSE(commands.empty());auto before=e.digest();REQUIRE_FALSE(e.submit(p,Choose{999999,0}).accepted);REQUIRE(e.digest()==before);
            rng=rng*1664525u+1013904223u;accept(e,p,commands[rng%commands.size()]);REQUIRE(Rules::invariants(e.state(),content.catalog).empty());
        }
    }
}
