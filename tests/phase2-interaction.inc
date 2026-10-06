// Included by interaction.cpp to reuse its simple projection-only fixtures.
TEST_CASE("IV-02 stale response and phase offers cannot adopt refreshed ids") {
    GameView v;CardView card;card.instance.id=1;v.cards.push_back(card);
    LegalAction old{"响应",1,0,Respond{1,1,"counter",0,9,0}};v.actions={old};
    Interaction ui;ui.update(v);ui.select(1);ui.activate(0);REQUIRE(ui.pending());
    v.actions[0].command=Respond{2,1,"counter",0,9,0};ui.update(v);REQUIRE_FALSE(ui.pending());ui.offer(old);REQUIRE_FALSE(ui.pending());ui.offer(v.actions[0]);REQUIRE(std::get<Respond>(ui.pending()->command).decision==2);
    old={"结束阶段",0,0,AdvancePhase{3}};v.actions={old};ui.update(v);v.actions[0].command=AdvancePhase{4};ui.update(v);ui.offer(old);REQUIRE_FALSE(ui.pending());
}
TEST_CASE("response contexts distinguish earlier links and retain selected costs") {
    GameView v;CardView card;card.instance.id=1;v.cards.push_back(card);
    v.actions={{"a",1,0,Respond{5,1,"counter",0,7,2}},{"b",1,0,Respond{5,1,"counter",0,7,3}},{"c",1,0,Respond{5,1,"counter",0,8,2}}};
    Interaction ui;ui.update(v);ui.select(1);REQUIRE(ui.groups().size()==1);ui.activate(0);REQUIRE(ui.step()==Step::LinkTarget);REQUIRE((ui.linkCandidates()==std::vector<LinkId>{7,8}));REQUIRE_FALSE(ui.pickLink(9));REQUIRE(ui.pickLink(7));REQUIRE(ui.step()==Step::Cost);REQUIRE((ui.candidates()==std::vector<CardId>{2,3}));REQUIRE(ui.pick(3));REQUIRE(std::get<Respond>(ui.pending()->command).link==7);REQUIRE(std::get<Respond>(ui.pending()->command).discard==3);
    ui.cancel();REQUIRE(ui.drop(1,0,Zone::Casting));REQUIRE(ui.step()==Step::LinkTarget);REQUIRE_FALSE(ui.pending());
}
TEST_CASE("multi target contexts keep complete selected target sets") {
    GameView v;CardView card;card.instance.id=1;v.cards.push_back(card);
    v.actions={{"a",1,2,PrepareCast{1,2,0,{2,3}}},{"b",1,2,PrepareCast{1,2,0,{2,4}}}};
    Interaction ui;ui.update(v);ui.select(1);REQUIRE(ui.groups().size()==2);ui.activate(1);ui.pick(2);REQUIRE((std::get<PrepareCast>(ui.pending()->command).targets==std::vector<CardId>{2,4}));
}
