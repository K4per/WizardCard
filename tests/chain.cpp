#include "wizard/content.hpp"
#include "wizard/interaction.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace wizard;
namespace {
struct Arena {
    Content content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "tests/fixtures/stage2");
    GameState state;
    Arena() {
        state.phase = Phase::Main;
        state.flow = Flow::Main;
        state.phaseStep = PhaseStep::Body;
        state.rng = 7;
        state.nextLoad = 100;
        for (int p = 0; p < 2; ++p) {
            state.players[p].mana = 6;
            state.players[p].life = 20;
            state.players[p].ownTurn = 2;
            CardInstance base;
            base.id = static_cast<CardId>(p + 1);
            base.definition = "balance";
            base.owner = p;
            base.zone = Zone::Analysis;
            base.base = true;
            state.cards.emplace(base.id, base);
            for (CardId id = static_cast<CardId>(100 + 30 * p); id < static_cast<CardId>(120 + 30 * p);
                 ++id) {
                CardInstance c;
                c.id = id;
                c.owner = p;
                c.definition = "spark";
                state.cards.emplace(id, c);
                state.players[p].deck.push_back(id);
            }
        }
    }
    CardInstance &card(CardId id, const std::string &def, int p = 0, Zone zone = Zone::Hand) {
        CardInstance c;
        c.id = id;
        c.definition = def;
        c.owner = p;
        c.zone = zone;
        state.cards[id] = c;
        return state.cards.at(id);
    }
    CardInstance &ready(CardId id, const std::string &def = "fireball", int p = 0, CardId host = 1) {
        auto &c = card(id, def, p, Zone::Analysis);
        c.host = host;
        c.sourceFormation = host;
        c.spell = SpellState::Ready;
        c.analysisLoad = content.catalog.at(def).cost;
        return c;
    }
    void responder(const std::string &id, TargetKind target, std::vector<Effect> effects, int cost = 1,
                   std::vector<ResponseWindow> windows = {ResponseWindow::Prepare}) {
        CardDefinition d;
        d.id = id;
        d.name = id;
        d.type = CardType::Action;
        ResponseAbility a;
        a.id = "response";
        a.fromHand = true;
        a.handCost = cost;
        a.target = target;
        a.effects = std::move(effects);
        a.windows = std::move(windows);
        d.responses.push_back(a);
        content.catalog.cards[id] = std::move(d);
    }
    GameEngine engine() {
        return GameEngine::scenario(content.catalog, state);
    }
};
void ok(GameEngine &e, int p, const Command &c) {
    auto r = e.submit(p, c);
    INFO(r.error);
    REQUIRE(r.accepted);
    REQUIRE(Rules::invariants(e.state(), e.catalog()).empty());
}
void reject(GameEngine &e, int p, const Command &c) {
    auto before = e.canonicalState();
    auto digest = e.digest();
    REQUIRE_FALSE(e.submit(p, c).accepted);
    REQUIRE(e.canonicalState() == before);
    REQUIRE(e.digest() == digest);
}
void advance(GameEngine &e) {
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().phaseGate != 0);
    ok(e, e.state().active, AdvancePhase{e.state().phaseGate});
}
Respond response(GameEngine &e, CardId card, LinkId link = 0) {
    REQUIRE(e.state().decision);
    auto v = e.viewFor(e.state().decision->player);
    for (const auto &a : v.actions)
        if (auto r = std::get_if<Respond>(&a.command))
            if (r->card == card && (!link || r->link == link))
                return *r;
    FAIL("response not projected");
    return {};
}
void respond(GameEngine &e, CardId card, LinkId link = 0) {
    auto r = response(e, card, link);
    ok(e, e.state().decision->player, r);
}
void pass(GameEngine &e) {
    auto d = *e.state().decision;
    ok(e, d.player, PassResponse{d.id});
}
void choose(GameEngine &e) {
    auto d = *e.state().decision;
    ok(e, d.player, Choose{d.id, d.options.front()});
}
void untilMain(GameEngine &e) {
    for (int n = 0; n < 10 && e.state().result == -1 && !e.state().decision && e.state().phase != Phase::Main;
         ++n)
        advance(e);
}
int events(const GameEngine &e, const std::string &kind) {
    return static_cast<int>(std::count_if(e.state().events.begin(), e.state().events.end(),
                                          [&](const GameEvent &event) { return event.kind == kind; }));
}
} // namespace

TEST_CASE("PH-01 PH-02 five distinct phase gates preserve draw and income") {
    auto c = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    GameEngine e(c.catalog, {c.deck, c.deck}, 42);
    auto first = e.state().first;
    REQUIRE(e.state().phase == Phase::Draw);
    REQUIRE(e.viewFor(first).players[first].handCount == 5);
    REQUIRE(e.state().players[first].mana == 1);
    reject(e, first, Advance{});
    auto old = e.state().phaseGate;
    advance(e);
    REQUIRE(e.state().phase == Phase::Prepare);
    REQUIRE(e.state().players[first].mana == 3);
    reject(e, first, AdvancePhase{old});
    advance(e);
    REQUIRE(e.state().phase == Phase::Main);
    advance(e);
    REQUIRE(e.state().phase == Phase::Cast);
    advance(e);
    REQUIRE(e.state().phase == Phase::End);
    advance(e);
    REQUIRE(e.state().active == 1 - first);
    REQUIRE(e.state().phase == Phase::Draw);
    REQUIRE(e.viewFor(1 - first).players[1 - first].handCount == 6);
    advance(e);
    REQUIRE(e.state().players[1 - first].mana == 3);
    std::vector<std::string> ended;
    for (const auto &event : e.state().events)
        if (event.kind == "phase_end")
            ended.push_back(event.text);
    REQUIRE(ended.size() == 6);
    for (int i = 0; i < 5; ++i)
        REQUIRE(ended[i] == phaseName(static_cast<Phase>(i)) + "阶段结束");
    REQUIRE(events(e, "chain_open") == 13); // seven entry and six exit windows
}
TEST_CASE("LC-01 S1 paid preparation and actual release are separate chains") {
    Arena a;
    a.state.players[0].mana = 2;
    a.ready(10);
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    REQUIRE(e.state().players[0].mana == 1);
    REQUIRE(Rules::load(e.state(), 0) == 4);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    advance(e);
    REQUIRE(e.state().decision->kind == DecisionKind::CastOrder);
    reject(e, 0, AdvancePhase{e.state().phaseGate});
    reject(e, 0, PlayAction{10, 0});
    choose(e);
    REQUIRE(e.state().players[1].life == 14);
    REQUIRE(Rules::load(e.state(), 0) == 0);
    REQUIRE(e.state().phase == Phase::Cast);
    REQUIRE(events(e, "prepared") == 1);
    REQUIRE(events(e, "chain_closed") == 4);
}
TEST_CASE("LC-01 S2 cancel preparation keeps costs until its own cleanup") {
    Arena a;
    a.state.players[0].mana = 2;
    a.state.players[1].mana = 1;
    a.ready(10);
    a.card(11, "barbs", 1, Zone::Words).analysisLoad = 4;
    // A spare explicit response keeps the chain open after the barbs declaration.
    a.responder("watch", TargetKind::None, {{EffectKind::GainMana, 0}}, 0);
    a.card(12, "watch");
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
    respond(e, 11);
    REQUIRE(Rules::load(e.state(), 0) == 4);
    REQUIRE(Rules::load(e.state(), 1) == 4);
    REQUIRE(e.state().chain->links.size() == 2);
    pass(e);
    REQUIRE_FALSE(e.state().chain);
    REQUIRE(Rules::load(e.state(), 0) == 3);
    REQUIRE(Rules::load(e.state(), 1) == 0);
    REQUIRE(e.state().players[0].mana == 1);
    REQUIRE(e.state().cards.at(10).canceledTurn == 2);
    REQUIRE(e.state().cards.at(10).payments.back().canceled);
    reject(e, 0, PrepareCast{10, 0, 0});
    advance(e);
    advance(e);
    advance(e);
    untilMain(e);
    advance(e);
    advance(e);
    advance(e);
    untilMain(e);
    ok(e, 0, PrepareCast{10, 0, 0});
    REQUIRE(e.state().cards.at(10).payments.size() == 2);
    REQUIRE_FALSE(e.state().cards.at(10).payments.back().canceled);
}
TEST_CASE("LC-01 S3 counter-counter resolves in reverse order") {
    Arena a;
    a.state.players[0].mana = 3;
    a.state.players[1].mana = 1;
    a.ready(10);
    a.card(11, "barbs", 1, Zone::Words).analysisLoad = 4;
    a.responder("counter", TargetKind::PendingLink, {{EffectKind::NegateLink, 0}});
    a.card(12, "counter");
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    respond(e, 11);
    auto id = e.state().chain->links.back().id;
    respond(e, 12, id);
    REQUIRE(e.state().players[0].mana == 1);
    REQUIRE(Rules::load(e.state(), 0) == 4);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).zone == Zone::Ash);
    REQUIRE_FALSE(e.state().cards.at(10).payments.back().canceled);
    std::vector<CardId> order;
    for (const auto &event : e.state().events)
        if (event.kind == "link_resolved" || event.kind == "link_skipped")
            order.push_back(event.card);
    REQUIRE((order == std::vector<CardId>{12, 11, 10}));
    advance(e);
    choose(e);
    REQUIRE(e.state().players[1].life == 14);
}
TEST_CASE("LC-01 S4 upper overload stops lower clear and all later work") {
    Arena a;
    a.state.players[0].mana = 2;
    a.state.players[1].mana = 2;
    a.ready(10);
    a.ready(11).analysisLoad = 3;
    a.card(12, "clarity");
    a.state.temporary.push_back({1, 0, 0, 2, 2});
    a.responder("overload", TargetKind::Opponent, {{EffectKind::AddTemporary, 2}}, 2,
                {ResponseWindow::Action});
    a.card(13, "overload", 1);
    auto e = a.engine();
    ok(e, 0, PlayAction{12, 0});
    REQUIRE(e.state().players[0].mana == 1);
    respond(e, 13);
    REQUIRE(e.state().result == 1);
    REQUIRE(Rules::load(e.state(), 0) == 10);
    REQUIRE(e.state().players[1].mana == 0);
    REQUIRE(e.state().cards.at(13).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).zone == Zone::Action);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE_FALSE(e.state().chain);
    REQUIRE(e.state().queue.items.empty());
    reject(e, 0, AdvancePhase{1});
}
TEST_CASE("CH-01 CH-03 CH-04 CH-05 CH-07 priority passes reset and old commands reject atomically") {
    Arena a;
    a.ready(10);
    a.responder("counter", TargetKind::PendingLink, {{EffectKind::NegateLink, 0}}, 0);
    a.card(11, "counter", 1);
    a.card(12, "counter");
    a.card(13, "counter", 1);
    a.card(14, "counter");
    a.card(15, "counter");
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    auto stale = response(e, 11);
    reject(e, 0, stale);
    auto root = e.state().chain->links.front().id;
    pass(e);
    REQUIRE(e.state().chain->passes == 1);
    respond(e, 12, root);
    REQUIRE(e.state().chain->passes == 0);
    REQUIRE(e.state().decision->player == 1);
    reject(e, 1, stale);
    respond(e, 11, root);
    respond(e, 14, root);
    REQUIRE(e.state().chain->links.size() == 4);
    REQUIRE(e.state().decision->player == 1);
    auto invalid = response(e, 13);
    invalid.card = 11;
    reject(e, 1, invalid);
    invalid = response(e, 13);
    invalid.link = 999999;
    reject(e, 1, invalid);
    pass(e);
    REQUIRE(e.state().chain->passes == 1);
    REQUIRE(e.state().decision->player == 0);
    pass(e);
    REQUIRE_FALSE(e.state().chain);
    REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
}
TEST_CASE("CH-02 LC-03 IV-01 implicit hand permissions and private response candidates stay hidden") {
    Arena a;
    a.ready(10);
    a.card(11, "barbs", 1);
    a.card(12, "recall", 1);
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(11).zone == Zone::Hand);
    REQUIRE(events(e, "response_pass") == 2);
    a.card(13, "barbs", 1, Zone::Words).analysisLoad = 4;
    a.responder("private", TargetKind::None, {{EffectKind::GainMana, 0}}, 0);
    a.card(14, "private");
    e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    auto old = response(e, 13);
    REQUIRE(e.viewFor(0).actions.empty());
    REQUIRE_FALSE(e.viewFor(0).decision);
    for (const auto &c : e.viewFor(0).cards)
        REQUIRE(c.instance.id != 11);
    respond(e, 13);
    REQUIRE(e.state().decision->player == 0);
    REQUIRE(e.viewFor(1).actions.empty());
    REQUIRE(e.viewFor(0).chain->links.back().item.source == 13);
    reject(e, 1, old);
    for (const auto &event : e.state().events)
        if (event.kind == "response_pass") {
            REQUIRE(event.audience == -1);
            REQUIRE(event.card == 0);
            REQUIRE(event.amount == 0);
        }
}
TEST_CASE("LC-02 LC-04 LC-05 hand word mode costs and temporary burden remain distinct") {
    Arena a;
    a.ready(10);
    a.responder("handword", TargetKind::None, {{EffectKind::GainMana, 0}}, 2);
    auto &d = a.content.catalog.cards.at("handword");
    d.type = CardType::Word;
    d.cost = 7;
    d.responses[0].preloadedCost = 5;
    d.responses[0].extraDiscard = 1;
    d.responses[0].burden = 1;
    a.card(11, "handword", 1);
    a.card(12, "spark", 1);
    a.responder("watch", TargetKind::None, {{EffectKind::GainMana, 0}}, 0);
    a.card(13, "watch");
    a.state.temporary = {{1, 1, 0, 5, 3}};
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    auto r = response(e, 11);
    REQUIRE(r.discard == 12);
    auto bad = r;
    bad.discard = 11;
    reject(e, 1, bad);
    respond(e, 11);
    REQUIRE(e.state().players[1].mana == 4);
    REQUIRE(Rules::load(e.state(), 1) == 8);
    REQUIRE(e.state().cards.at(11).castLoad == 2);
    REQUIRE(e.state().cards.at(12).zone == Zone::Ash);
    pass(e);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(Rules::load(e.state(), 1) == 6);
    REQUIRE(e.state().temporary.back().expiryTurn == 3);
    a.state.temporary.front().amount = 6;
    auto full = a.engine();
    ok(full, 0, PrepareCast{10, 0, 0});
    REQUIRE(full.state().decision->player == 0);
    pass(full);
    REQUIRE_FALSE(full.state().decision);
    REQUIRE(full.state().players[1].mana == 6);
    REQUIRE(full.state().cards.at(12).zone == Zone::Hand);
}
TEST_CASE("LC-07 response snapshots survive source loss unless explicitly dependent") {
    for (bool dependent : {false, true}) {
        Arena a;
        a.ready(10);
        a.responder("lower", TargetKind::Opponent, {{EffectKind::Damage, 2}}, 0);
        a.content.catalog.cards.at("lower").responses[0].requiresSource = dependent;
        a.responder("destroyer", TargetKind::EnemyCard, {{EffectKind::Destroy, 0}}, 0);
        a.card(11, "lower", 1);
        a.card(12, "destroyer");
        auto e = a.engine();
        ok(e, 0, PrepareCast{10, 0, 0});
        respond(e, 11);
        auto r = response(e, 12);
        REQUIRE(r.target == 11);
        respond(e, 12);
        REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
        REQUIRE(e.state().players[0].life == (dependent ? 20 : 18));
        REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    }
}
TEST_CASE("LC-08 source formation loss cleans dependents before preparation checks") {
    Arena a;
    a.card(9, "reservoir", 0, Zone::Analysis);
    a.ready(10, "spark", 0, 9);
    a.card(11, "ring", 0, Zone::Attached).host = 10;
    a.responder("destroyer", TargetKind::EnemyCard, {{EffectKind::Destroy, 0}}, 0);
    a.card(12, "destroyer", 1);
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    auto r = response(e, 12);
    r.target = 9;
    r.targets = {9};
    ok(e, 1, r);
    for (CardId id : {9u, 10u, 11u})
        REQUIRE(e.state().cards.at(id).zone == Zone::Ash);
    REQUIRE(Rules::load(e.state(), 0) == 0);
    REQUIRE(e.state().result == -1);
    REQUIRE(e.state().cards.at(10).payments.back().canceled);
}
TEST_CASE("LC-10 partial multi targets fizzle only the unavailable part") {
    Arena a;
    a.content.catalog.cards.at("unravel").target = TargetKind::EnemyCard;
    a.content.catalog.cards.at("unravel").targetCount = 2;
    a.ready(10, "unravel");
    a.card(11, "spark");
    a.card(12, "reservoir", 1, Zone::Analysis);
    a.card(13, "conduit", 1, Zone::Analysis);
    a.responder("destroyer", TargetKind::OwnCard, {{EffectKind::Destroy, 0}}, 0, {ResponseWindow::Cast});
    a.card(14, "destroyer", 1);
    auto e = a.engine();
    auto v = e.viewFor(0);
    REQUIRE(std::any_of(v.actions.begin(), v.actions.end(), [](const LegalAction &action) {
        auto c = std::get_if<PrepareCast>(&action.command);
        return c && c->targets == std::vector<CardId>{12, 13};
    }));
    ok(e, 0, PrepareCast{10, 12, 11, {12, 13}});
    advance(e);
    choose(e);
    auto r = response(e, 14);
    r.target = 12;
    r.targets = {12};
    ok(e, 1, r);
    REQUIRE(e.state().cards.at(12).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(13).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("LC-11 cancelling cast action and trigger gives each its own cleanup") {
    SECTION("actual release cancelled without seal refund") {
        Arena a;
        a.ready(10);
        a.card(11, "ring", 0, Zone::Attached).host = 10;
        a.responder("counter", TargetKind::PendingLink, {{EffectKind::NegateLink, 0}}, 0,
                    {ResponseWindow::Cast});
        a.card(12, "counter", 1);
        auto e = a.engine();
        ok(e, 0, PrepareCast{10, 0, 0});
        advance(e);
        choose(e);
        respond(e, 12);
        REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
        REQUIRE(e.state().players[0].mana == 5);
        REQUIRE(e.state().players[1].life == 20);
        REQUIRE(events(e, "refund") == 0);
    }
    SECTION("action burden is not refunded") {
        Arena a;
        a.card(10, "recall");
        a.responder("counter", TargetKind::PendingLink, {{EffectKind::NegateLink, 0}}, 0,
                    {ResponseWindow::Action});
        a.card(11, "counter", 1);
        auto e = a.engine();
        ok(e, 0, PlayAction{10, 0});
        respond(e, 11);
        REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
        REQUIRE(e.state().players[0].mana == 4);
        REQUIRE(Rules::load(e.state(), 0) == 1);
        REQUIRE(e.viewFor(0).players[0].handCount == 0);
    }
    SECTION("cancelled trigger leaves its persistent source") {
        Arena a;
        auto &c = a.ready(10, "ward");
        c.zone = Zone::Casting;
        c.host = 0;
        c.spell = SpellState::Active;
        c.remaining = 2;
        c.targetPlayer = 1;
        a.content.catalog.cards.at("ward").onMain = {{EffectKind::Damage, 2}};
        a.responder("counter", TargetKind::PendingLink, {{EffectKind::NegateLink, 0}}, 0,
                    {ResponseWindow::Trigger});
        a.card(11, "counter", 1);
        a.state.phase = Phase::Prepare;
        auto e = a.engine();
        advance(e);
        respond(e, 11);
        REQUIRE(e.state().players[1].life == 20);
        REQUIRE(e.state().cards.at(10).spell == SpellState::Active);
        REQUIRE(e.state().cards.at(10).remaining == 2);
    }
}
TEST_CASE("PH-06 PH-07 end response then cleanup triggers recheck hand but not duration") {
    Arena a;
    auto &c = a.ready(10, "ward");
    c.zone = Zone::Casting;
    c.host = 0;
    c.spell = SpellState::Active;
    c.remaining = 1;
    a.content.catalog.cards.at("ward").onLeave = {{EffectKind::Draw, 2}};
    a.responder("enddraw", TargetKind::None, {{EffectKind::Draw, 2}}, 0, {ResponseWindow::PhaseEnd});
    a.card(11, "enddraw");
    for (CardId id = 20; id < 27; ++id)
        a.card(id, "spark");
    a.state.phase = Phase::End;
    auto e = a.engine();
    advance(e);
    REQUIRE(e.state().phase == Phase::End);
    REQUIRE(e.state().cards.at(10).remaining == 1);
    respond(e, 11);
    REQUIRE(e.state().decision->kind == DecisionKind::Discard);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().queue.items.size() == 1);
    choose(e);
    REQUIRE(e.state().decision->kind == DecisionKind::Discard);
    choose(e);
    choose(e);
    REQUIRE(e.state().phase == Phase::Draw);
    REQUIRE(e.state().active == 1);
    REQUIRE(e.viewFor(0).players[0].handCount == 8);
    REQUIRE(events(e, "chain_open") == 3);
    REQUIRE(events(e, "phase_end") == 1);
}
TEST_CASE("ST-04 deferred captured triggers cannot interrupt a current chain") {
    Arena a;
    a.ready(10);
    a.card(11, "reservoir", 0, Zone::Analysis);
    a.content.catalog.cards.at("reservoir").onLeave = {{EffectKind::GainMana, 1}};
    a.responder("destroyer", TargetKind::EnemyCard, {{EffectKind::Destroy, 0}}, 0);
    a.card(12, "destroyer", 1);
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    auto r = response(e, 12);
    r.target = 11;
    r.targets = {11};
    ok(e, 1, r);
    REQUIRE(e.state().players[0].mana == 6);
    std::vector<std::string> kinds;
    for (const auto &event : e.state().events)
        if (event.kind == "chain_open" || event.kind == "chain_closed")
            kinds.push_back(event.kind);
    REQUIRE((kinds == std::vector<std::string>{"chain_open", "chain_closed", "chain_open", "chain_closed"}));
}
TEST_CASE("ST-08 logical cycle key ignores allocations but waiting choices stop automation") {
    Arena a;
    auto &c = a.ready(10, "ward");
    c.zone = Zone::Casting;
    c.host = 0;
    c.spell = SpellState::Active;
    c.remaining = 2;
    a.content.catalog.cards.at("ward").onMana = {{EffectKind::GainMana, 0}};
    a.card(11, "clarity");
    a.content.catalog.cards.at("clarity").effects = {{EffectKind::GainMana, 0}};
    auto e = a.engine();
    auto alternate = a.state;
    alternate.nextTrigger = 123;
    alternate.nextChain = 456;
    alternate.nextLink = 789;
    alternate.nextBatch = 900;
    auto other = GameEngine::scenario(a.content.catalog, alternate);
    REQUIRE(e.cycleKey() == other.cycleKey());
    REQUIRE(e.digest() != other.digest());
    ok(e, 0, PlayAction{11, 0});
    REQUIRE(e.state().result == 2);
    REQUIRE(events(e, "chain_open") < 10);
    a.content.catalog.cards.at("ward").onMana.clear();
    a.content.catalog.cards.at("clarity").effects = {{EffectKind::ClearTemporary, 1}};
    a.state.temporary = {{1, 0, 0, 1, 2}};
    e = a.engine();
    ok(e, 0, PlayAction{11, 0});
    REQUIRE(e.state().decision->kind == DecisionKind::ClearLoad);
    REQUIRE(e.state().result == -1);
    reject(e, 0, PassResponse{e.state().decision->id});
    reject(e, 0, PlayAction{11, 0});
}
TEST_CASE("CF-01 CF-02 CF-03 CF-04 base qualification and two independent configurations") {
    auto c = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    MatchConfig config;
    for (auto &p : config.players)
        p.cards = c.deck;
    config.players[1].baseFormation = "reservoir";
    REQUIRE_THROWS(GameEngine(c.catalog, config));
    c.catalog.cards.at("reservoir").baseEligible = true;
    c.catalog.cards.at("reservoir").rings = 1;
    c.catalog.cards.at("reservoir").maxRank = 1;
    auto e = GameEngine(c.catalog, config);
    REQUIRE(e.state().cards.at(32).definition == "reservoir");
    REQUIRE(Rules::capacity(e.state(), c.catalog, 1) == 5 + c.catalog.baseLoadCapacity);
    REQUIRE(Rules::rings(e.state(), c.catalog, 32) == 1);
    config.players[0].baseFormation = "missing";
    REQUIRE_THROWS(GameEngine(c.catalog, config));
    config.players[0].baseFormation = "balance";
    c.catalog.cards.at("reservoir").capacity = 0;
    REQUIRE_THROWS(GameEngine(c.catalog, config));
    auto j = readJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/cards.json");
    j["cards"][1]["baseEligible"] = true;
    REQUIRE_THROWS(parseCatalog(j));
    for (const auto *field : {"capacity", "rings", "maxRank"}) {
        j = readJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/cards.json");
        j["cards"][0][field] = 0;
        REQUIRE_THROWS(parseCatalog(j));
    }
    Arena a;
    a.card(10, "balance", 0, Zone::Analysis);
    auto s = a.state;
    StateMaintenance::leave(s, 1);
    StateMaintenance::leave(s, 10);
    REQUIRE(s.cards.at(1).zone == Zone::Analysis);
    REQUIRE(s.cards.at(10).zone == Zone::Ash);
    PlayerDeck deck;
    deck.cards = a.content.deck;
    for (auto &id : deck.cards)
        if (id == "fireball")
            id = "balance";
    REQUIRE(Rules::deckErrors(a.content.catalog, deck).empty());
    REQUIRE_NOTHROW(GameEngine(a.content.catalog, {deck.cards, deck.cards}, 42));
}
TEST_CASE("RP-01 RP-02 RP-03 format two records configurations and detects protocol tampering") {
    auto c = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    c.catalog.cards.at("reservoir").baseEligible = true;
    c.catalog.cards.at("reservoir").rings = 1;
    c.catalog.cards.at("reservoir").maxRank = 1;
    MatchConfig config;
    config.players[0].cards = c.deck;
    config.players[1].cards = c.deck;
    std::reverse(config.players[1].cards.begin(), config.players[1].cards.end());
    config.players[1].baseFormation = "reservoir";
    MatchSession session(c, config);
    for (int n = 0; n < 3000 && session.engine().state().result == -1; ++n) {
        auto &e = session.engine();
        auto p = e.state().decision ? e.state().decision->player : e.state().active;
        auto v = e.viewFor(p);
        auto i = std::find_if(v.actions.begin(), v.actions.end(), [](const LegalAction &a) {
            return !std::holds_alternative<Surrender>(a.command) &&
                   !std::holds_alternative<Abandon>(a.command) &&
                   !std::holds_alternative<RemoveFormation>(a.command) &&
                   !std::holds_alternative<RemoveSeal>(a.command);
        });
        REQUIRE(i != v.actions.end());
        REQUIRE(session.submit(p, i->command).accepted);
        if (n % 20 == 0)
            REQUIRE(replay(c, session.recording()).digest() == session.engine().digest());
    }
    REQUIRE(session.engine().state().result != -1);
    auto j = session.recording();
    REQUIRE(j["format"] == 2);
    REQUIRE(j["players"][1]["baseFormation"] == "reservoir");
    REQUIRE(j["players"][0]["cards"] != j["players"][1]["cards"]);
    REQUIRE(replay(c, j).canonicalState() == session.engine().canonicalState());
    auto bad = j;
    bad["format"] = 1;
    REQUIRE_THROWS(replay(c, bad));
    bad = j;
    bad["commands"][0]["digest"] = "wrong";
    REQUIRE_THROWS(replay(c, bad));
    bad = j;
    bad["seed"] = -1;
    REQUIRE_THROWS(replay(c, bad));
    bad = j;
    bad["players"][1]["baseFormation"] = "conduit";
    REQUIRE_THROWS(replay(c, bad));
    std::vector<Command> cmds = {Respond{4294967297ull, 1, "response", 0, 4294967298ull, 2},
                                 PassResponse{4294967299ull}, AdvancePhase{4294967300ull},
                                 PrepareCast{1, 2, 3, {2, 4}}};
    for (const auto &cmd : cmds)
        REQUIRE(encodeCommand(decodeCommand(encodeCommand(cmd))) == encodeCommand(cmd));
    REQUIRE_THROWS(decodeCommand(Json{{"type", 13}, {"decision", 0}}));
    REQUIRE_THROWS(decodeCommand(Json{{"type", 14}, {"gate", 2.5}}));
    Arena a;
    a.card(10, "clarity");
    a.state.temporary = {{1, 0, 0, 1, 2}};
    auto e = a.engine();
    ok(e, 0, PlayAction{10, 0});
    auto s = e.state();
    s.durationApplied = !s.durationApplied;
    auto altered = GameEngine::scenario(a.content.catalog, s);
    REQUIRE(altered.digest() != e.digest());
    s = e.state();
    s.chain->links.back().paid++;
    altered = GameEngine::scenario(a.content.catalog, s);
    REQUIRE(altered.digest() != e.digest());
}
TEST_CASE("content response schema requires explicit source cost window and target") {
    auto j = readJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/cards.json");
    auto bad = j;
    bad["cards"][2]["responses"][0]["sources"] = {"hand"};
    REQUIRE_THROWS(parseCatalog(bad));
    bad = j;
    bad["cards"][2]["responses"][0]["windows"] = {"anytime"};
    REQUIRE_THROWS(parseCatalog(bad));
    bad = j;
    bad["cards"][2]["responses"][0]["target"] = "pending_link";
    REQUIRE_THROWS(parseCatalog(bad));
    bad = j;
    bad["cards"][0]["baseEligible"] = 1;
    REQUIRE_THROWS(parseCatalog(bad));
    bad = j;
    bad["cards"][2]["responses"][0]["requiresSource"] = 1;
    REQUIRE_THROWS(parseCatalog(bad));
}
TEST_CASE("RP-02 accepted commands replay active chains and mandatory effect decisions") {
    auto dir = std::filesystem::path(WIZARD_SOURCE_DIR) / "tests/fixtures/stage2";
    // Historical mechanics fixture isolates command/replay regression from formal v1 source restrictions.
    auto definitions = readJson(dir / "cards.json");
    for (auto &definition : definitions["cards"]) {
        if (!definition.contains("responses"))
            definition["responses"] = Json::array();
        definition["responses"].push_back(
            {{"id", "test_clear"},
             {"sources", {"hand"}},
             {"windows", {"action"}},
             {"target", "none"},
             {"handCost", 0},
             {"burden", 1},
             {"effects", Json::array({{{"kind", "clear_temporary"}, {"amount", 1}}})}});
    }
    Content content;
    content.catalog = parseCatalog(definitions);
    content.deck = parseDeck(readJson(dir / "deck.json"), content.catalog);
    MatchSession session(content, 42);
    while (session.engine().state().phase != Phase::Main)
        REQUIRE(
            session.submit(session.engine().state().active, AdvancePhase{session.engine().state().phaseGate})
                .accepted);
    auto active = session.engine().state().active;
    auto view = session.engine().viewFor(active);
    auto action = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &a) {
        return std::holds_alternative<PlayAction>(a.command);
    });
    REQUIRE(action != view.actions.end());
    REQUIRE(session.submit(active, action->command).accepted);
    REQUIRE(session.engine().state().chain);
    REQUIRE(replay(content, session.recording()).canonicalState() == session.engine().canonicalState());
    view = session.engine().viewFor(1 - active);
    action = std::find_if(view.actions.begin(), view.actions.end(),
                          [](const LegalAction &a) { return std::holds_alternative<Respond>(a.command); });
    REQUIRE(action != view.actions.end());
    REQUIRE(session.submit(1 - active, action->command).accepted);
    REQUIRE(replay(content, session.recording()).canonicalState() == session.engine().canonicalState());
    for (int n = 0; n < 2; ++n) {
        auto d = *session.engine().state().decision;
        REQUIRE(session.submit(d.player, PassResponse{d.id}).accepted);
    }
    REQUIRE(session.engine().state().decision->kind == DecisionKind::ClearLoad);
    REQUIRE(session.engine().state().chain->mode == ChainMode::Resolving);
    REQUIRE(replay(content, session.recording()).canonicalState() == session.engine().canonicalState());
    auto bad = session.recording();
    for (auto &row : bad["commands"])
        if (row["command"]["type"] == 12) {
            row["command"]["link"] = 999;
            break;
        }
    REQUIRE_THROWS(replay(content, bad));
    auto d = *session.engine().state().decision;
    REQUIRE(session.submit(d.player, Choose{d.id, d.options.front()}).accepted);
    REQUIRE(replay(content, session.recording()).digest() == session.engine().digest());
}
TEST_CASE("LC-04 cancelled hand word keeps mana and discard but releases bound load") {
    Arena a;
    a.ready(10);
    a.responder("handword", TargetKind::None, {{EffectKind::GainMana, 0}}, 2);
    auto &word = a.content.catalog.cards.at("handword");
    word.type = CardType::Word;
    word.cost = 7;
    word.responses[0].preloadedCost = 9;
    word.responses[0].burden = 1;
    word.responses[0].extraDiscard = 1;
    a.responder("counter", TargetKind::PendingLink, {{EffectKind::NegateLink, 0}}, 0);
    a.card(11, "handword", 1);
    a.card(12, "spark", 1);
    a.card(13, "counter");
    a.state.players[1].mana = 3;
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    respond(e, 11);
    auto link = e.state().chain->links.back().id;
    REQUIRE(Rules::load(e.state(), 1) == 3);
    respond(e, 13, link);
    REQUIRE(e.state().players[1].mana == 1);
    REQUIRE(Rules::load(e.state(), 1) == 1);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(11).payments.back().canceled);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
}
TEST_CASE("PH-05 automatic phase driver respects disabled main and forced decisions") {
    auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    MatchSession session(content, 42);
    auto active = session.engine().state().active;
    auto view = session.engine().viewFor(active);
    auto before = session.engine().digest();
    REQUIRE_FALSE(ui::automaticAdvance(view, false));
    REQUIRE(session.engine().digest() == before);
    auto gate = ui::automaticAdvance(view, true);
    REQUIRE(gate);
    REQUIRE(session.submit(active, *gate).accepted);
    REQUIRE(session.recording()["commands"][0]["command"]["type"] == 14);
    REQUIRE(session.submit(active, *ui::automaticAdvance(session.engine().viewFor(active), true)).accepted);
    REQUIRE_FALSE(ui::automaticAdvance(session.engine().viewFor(active), true));
    Arena a;
    a.ready(10);
    auto e = a.engine();
    ok(e, 0, PrepareCast{10, 0, 0});
    advance(e);
    REQUIRE(e.state().decision);
    REQUIRE_FALSE(ui::automaticAdvance(e.viewFor(0), true));
}
