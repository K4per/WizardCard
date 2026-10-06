#include "wizard/application.hpp"
#include "wizard/interaction.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
using namespace wizard;
namespace {
struct V1 {
    Content data = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    GameState s;
    V1() {
        s.phase = Phase::Main;
        s.phaseStep = PhaseStep::Body;
        s.flow = Flow::Main;
        s.rng = 7;
        s.nextLoad = 1000;
        data.catalog.cards.at("balance").capacity = 40;
        for (int p = 0; p < 2; ++p) {
            s.players[p].mana = 12;
            s.players[p].life = 20;
            s.players[p].ownTurn = 2;
            auto &b = add(static_cast<CardId>(p + 1), "balance", p, Zone::Analysis);
            b.base = true;
            for (CardId id = 100 + 30 * p; id < static_cast<CardId>(120 + 30 * p); ++id) {
                add(id, "spark", p, Zone::Deck);
                s.players[p].deck.push_back(id);
            }
        }
    }
    CardInstance &add(CardId id, const std::string &name, int p = 0, Zone z = Zone::Hand) {
        CardInstance c;
        c.id = id;
        c.definition = name;
        c.owner = p;
        c.zone = z;
        s.cards[id] = c;
        return s.cards.at(id);
    }
    CardInstance &ready(CardId id, const std::string &name, int p = 0) {
        auto &c = add(id, name, p, Zone::Analysis);
        c.spell = SpellState::Ready;
        c.host = static_cast<CardId>(p + 1);
        c.sourceFormation = c.host;
        c.analysisLoad = c.settingPaid = data.catalog.at(name).cost;
        return c;
    }
    CardInstance &word(CardId id, const std::string &name, int p = 0) {
        auto &c = add(id, name, p, Zone::Words);
        c.analysisLoad = c.settingPaid = data.catalog.at(name).cost;
        return c;
    }
    GameEngine engine() {
        return GameEngine::scenario(data.catalog, s);
    }
};
void send(GameEngine &e, int p, const Command &c) {
    auto r = e.submit(p, c);
    INFO(r.error);
    REQUIRE(r.accepted);
}
void pass(GameEngine &e) {
    for (int n = 0; e.state().decision && e.state().decision->kind == DecisionKind::Response && n < 12; ++n) {
        auto d = *e.state().decision;
        send(e, d.player, PassResponse{d.id});
    }
}
void next(GameEngine &e) {
    pass(e);
    REQUIRE_FALSE(e.state().decision);
    send(e, e.state().active, AdvancePhase{e.state().phaseGate});
}
void reach(GameEngine &e, int p, Phase phase) {
    if (e.state().active == p && e.state().phase == phase)
        next(e);
    for (int n = 0; n < 30 && (e.state().active != p || e.state().phase != phase); ++n)
        next(e);
    REQUIRE(e.state().active == p);
    REQUIRE(e.state().phase == phase);
}
Respond response(GameEngine &e, int p, CardId id) {
    auto v = e.viewFor(p);
    for (const auto &a : v.actions)
        if (auto r = std::get_if<Respond>(&a.command))
            if (r->card == id)
                return *r;
    FAIL("missing response");
    return {};
}
} // namespace
TEST_CASE("cast phase can stop with prepared spells and retain payment until a later release") {
    V1 f;
    f.ready(10, "magic-missile");
    f.ready(11, "ray-of-frost");
    f.add(14, "recall", 1, Zone::Words).faceDown = true;
    auto e = f.engine();
    send(e, 0, PrepareCast{10});
    send(e, 0, PrepareCast{11});
    reach(e, 0, Phase::Cast);
    REQUIRE(e.state().decision->mayPass);
    const auto mana = e.state().players[0].mana;
    const auto load = Rules::load(e.state(), 0);
    send(e, 0, Choose{e.state().decision->id, 0});
    pass(e);
    REQUIRE(e.state().phase == Phase::End);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Pending);
    REQUIRE(e.state().cards.at(11).zone == Zone::Casting);
    REQUIRE(e.state().players[0].mana == mana);
    REQUIRE(Rules::load(e.state(), 0) == load);
    REQUIRE(e.state().players[1].life == 20);
    reach(e, 0, Phase::Cast);
    const auto paymentCount = e.state().cards.at(10).payments.size();
    send(e, 0, Choose{e.state().decision->id, 10});
    REQUIRE(e.state().players[1].life == 17);
    REQUIRE(e.state().cards.at(10).payments.size() == paymentCount);
    REQUIRE(e.state().decision->kind == DecisionKind::DestroyAmbush);
    send(e, 0, Choose{e.state().decision->id, 0});
    REQUIRE(e.state().decision->kind == DecisionKind::CastOrder);
    send(e, 0, Choose{e.state().decision->id, 0});
    REQUIRE(e.state().cards.at(11).spell == SpellState::Pending);
    for (const Command &c : {Command{PreloadWord{12, false}}, Command{PreloadWord{12, true}}})
        REQUIRE(encodeCommand(decodeCommand(encodeCommand(c))) == encodeCommand(c));
}
TEST_CASE("zero rank words can be set alone while direct instant release remains explicit") {
    V1 f;
    f.add(10, "spark");
    f.add(11, "true-strike");
    f.add(12, "shield");
    f.add(13, "spark");
    auto e = f.engine();
    send(e, 0, PreloadWord{10});
    send(e, 0, PreloadWord{11});
    send(e, 0, PreloadWord{12});
    REQUIRE(e.state().players[1].life == 20);
    REQUIRE(e.state().cards.at(10).zone == Zone::Words);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE_FALSE(e.submit(0, PreloadWord{13}).accepted);
    send(e, 0, PreloadWord{13, true});
    pass(e);
    REQUIRE(e.state().players[1].life == 18);
    send(e, 0, ActivateSpell{10});
    pass(e);
    REQUIRE(e.state().players[1].life == 16);
    REQUIRE(e.state().cards.at(11).zone == Zone::Words);
    REQUIRE_FALSE(e.submit(0, PreloadWord{12, true}).accepted);
}
TEST_CASE("release supplement damage protection restoration and recovery resolve real effects") {
    for (const auto &id : {std::string("ray-of-frost"), std::string("lightning-bolt")}) {
        V1 f;
        f.ready(10, id);
        f.s.phase = Phase::Cast;
        auto e = f.engine();
        send(e, 0, ActivateSpell{10});
        REQUIRE(e.state().players[1].life == 20 - f.data.catalog.at(id).effects.front().amount);
        REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    }
    V1 f;
    f.ready(10, "lesser-restoration");
    f.s.phase = Phase::Cast;
    f.s.players[0].life = 18;
    f.s.temporary = {{1, 0, 0, 4, 3, false}, {2, 0, 0, 3, 0, true}};
    auto e = f.engine();
    send(e, 0, ActivateSpell{10});
    REQUIRE(e.state().players[0].life == 20);
    REQUIRE(e.state().temporary.size() == 1);
    REQUIRE(e.state().temporary.front().independent);
    f.s.temporary.clear();
    f.s.cards.at(10).definition = "false-life";
    e = f.engine();
    send(e, 0, ActivateSpell{10});
    REQUIRE(e.state().players[0].temporaryLife == 8);
    REQUIRE(e.state().temporary.front().amount == 2);
    REQUIRE(e.state().temporary.front().independent);
    V1 recovery;
    recovery.add(10, "arcane-recovery");
    recovery.s.players[0].mana = 11;
    e = recovery.engine();
    send(e, 0, PlayAction{10});
    REQUIRE(e.state().players[0].mana == 12);
    REQUIRE(e.state().temporary.front().amount == 2);
}
TEST_CASE("v1 cards expose typed damage schools speeds and untouched artwork entries") {
    V1 f;
    REQUIRE(f.data.catalog.cards.size() == 30);
    REQUIRE(f.data.catalog.advancedRules);
    for (auto name : {"fireball", "spark", "ward"}) {
        const auto &d = f.data.catalog.at(name);
        REQUIRE(d.school == "evocation");
        REQUIRE(d.effects.front().damageType == DamageType::Fire);
    }
    REQUIRE(f.data.catalog.at("barbs").speed == 2);
    REQUIRE(f.data.catalog.at("counter-spell").speed == 4);
    REQUIRE(f.data.catalog.at("true-strike").school == "divination");
    auto art = readJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/art/runtime.json");
    REQUIRE(art["cards"].size() == 30);
    for (const auto &[id, d] : f.data.catalog.cards)
        REQUIRE(std::filesystem::exists(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/art" /
                                        art["cards"][id].get<std::string>()));
}
TEST_CASE("intrinsic load capacity persists independently and overload uses the combined limit") {
    V1 f;
    REQUIRE(f.data.catalog.baseLoadCapacity == 2);
    REQUIRE(f.data.catalog.at("pentagram-of-life").capacity == 5);
    f.data.catalog.cards.at("balance").capacity = 8;
    for (const auto &base : {std::string("balance"), std::string("pentagram-of-life")}) {
        auto state = f.s;
        state.cards.at(1).definition = base;
        const int limit = base == "balance" ? 10 : 7;
        REQUIRE(Rules::capacity(state, f.data.catalog, 0) == limit);
        state.temporary.push_back({1, 0, 0, limit, 2});
        StateMaintenance::check(state, f.data.catalog);
        REQUIRE(state.result == -1);
        ++state.temporary.back().amount;
        StateMaintenance::check(state, f.data.catalog);
        REQUIRE(state.result == 1);
    }
    f.add(10, "reservoir", 0, Zone::Analysis);
    const auto before = Rules::capacity(f.s, f.data.catalog, 0);
    StateMaintenance::leave(f.s, 10, &f.data.catalog);
    REQUIRE(Rules::capacity(f.s, f.data.catalog, 0) == before - f.data.catalog.at("reservoir").capacity);
    f.s.cards.at(1).zone = Zone::Ash;
    REQUIRE(Rules::capacity(f.s, f.data.catalog, 0) == 2);
    const auto legacy = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "tests/fixtures/stage2");
    REQUIRE(legacy.catalog.baseLoadCapacity == 0);
}
TEST_CASE("shared drag destinations prefer analysis and preloading over ambush") {
    V1 f;
    f.add(10, "protective-flame");
    f.add(11, "magic-missile");
    f.add(12, "instant-circuit-overload");
    auto &analyzing = f.add(13, "magic-missile", 0, Zone::Analysis);
    analyzing.host = 1;
    analyzing.spell = SpellState::Analyzing;
    auto e = f.engine();
    ui::Interaction i;
    i.update(e.viewFor(0));
    REQUIRE(i.drop(10, 0, Zone::Words));
    REQUIRE(i.pending());
    REQUIRE(std::holds_alternative<PreloadWord>(i.pending()->command));
    i.cancel();
    REQUIRE(i.drop(11, 1, Zone::Analysis));
    REQUIRE(i.pending());
    REQUIRE(std::holds_alternative<StartAnalysis>(i.pending()->command));
    REQUIRE_FALSE(e.state().cards.at(10).faceDown);
    REQUIRE(e.state().cards.at(11).zone == Zone::Hand);
    i.cancel();
    REQUIRE(i.drop(12, 13, Zone::Analysis));
    REQUIRE(i.pending());
    REQUIRE(std::get<PlayAction>(i.pending()->command).target == 13);
}
TEST_CASE("temporary life is maximum rather than additive and absorbs typed resisted damage first") {
    V1 f;
    auto &c = f.word(10, "protective-flame");
    c.zone = Zone::Casting;
    c.spell = SpellState::Active;
    Trigger t;
    t.owner = t.targetPlayer = 0;
    EffectResolver::apply(f.s, f.data.catalog, t, {EffectKind::AddTemporaryLife, 5});
    EffectResolver::apply(f.s, f.data.catalog, t, {EffectKind::AddTemporaryLife, 3});
    REQUIRE(f.s.players[0].temporaryLife == 5);
    EffectResolver::apply(f.s, f.data.catalog, t, {EffectKind::Damage, 9, DamageType::Fire});
    REQUIRE(f.s.players[0].temporaryLife == 1);
    REQUIRE(f.s.players[0].life == 20);
    EffectResolver::apply(f.s, f.data.catalog, t, {EffectKind::Damage, 3, DamageType::Force});
    REQUIRE(f.s.players[0].temporaryLife == 0);
    REQUIRE(f.s.players[0].life == 18);
    f.data.catalog.cards.at("protective-flame").immunities = {DamageType::Fire};
    EffectResolver::apply(f.s, f.data.catalog, t, {EffectKind::Damage, 99, DamageType::Fire});
    REQUIRE(f.s.players[0].life == 18);
    StateMaintenance::leave(f.s, 10, &f.data.catalog);
    EffectResolver::apply(f.s, f.data.catalog, t, {EffectKind::Damage, 3, DamageType::Fire});
    REQUIRE(f.s.players[0].life == 15);
}
TEST_CASE("aid instantly analyzes and releases temporary life without an extra turn") {
    V1 f;
    f.add(10, "aid");
    auto e = f.engine();
    send(e, 0, StartAnalysis{10, 1});
    REQUIRE(e.state().cards.at(10).spell == SpellState::Ready);
    send(e, 0, PrepareCast{10});
    reach(e, 0, Phase::Cast);
    send(e, 0, Choose{e.state().decision->id, 10});
    REQUIRE(e.state().players[0].temporaryLife == 5);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    reach(e, 1, Phase::Main);
    REQUIRE(e.state().players[0].temporaryLife == 5);
}
TEST_CASE(
    "pentagram discounts only eligible schools on its base instance and refunds actual setting payment") {
    V1 f;
    f.s.cards.at(1).definition = "pentagram-of-life";
    f.data.catalog.cards.at("pentagram-of-life").capacity = 20;
    f.add(10, "magic-missile");
    f.add(11, "aid");
    auto &h = f.add(12, "pentagram-of-life", 0, Zone::Analysis);
    REQUIRE_FALSE(h.base);
    auto e = f.engine();
    send(e, 0, StartAnalysis{10, 1});
    send(e, 0, StartAnalysis{11, 12});
    REQUIRE(e.state().cards.at(10).settingPaid == 1);
    REQUIRE(e.state().cards.at(11).settingPaid == 1);
    REQUIRE(Rules::analysisCost(f.s, f.data.catalog, 10, 12) == 2);
}
TEST_CASE("four point star accelerates rank one once per own turn and never consumes the use for immediate "
          "analysis") {
    V1 f;
    f.add(10, "aid");
    f.add(11, "magic-missile");
    f.add(12, "mend");
    f.add(13, "engeas-four-point-star", 0, Zone::Analysis);
    f.data.catalog.cards.at("engeas-four-point-star").rings = 3;
    auto e = f.engine();
    send(e, 0, StartAnalysis{10, 13});
    REQUIRE(e.state().cards.at(13).quickAnalysisTurn == -1);
    send(e, 0, StartAnalysis{11, 13});
    send(e, 0, StartAnalysis{12, 13});
    REQUIRE(e.state().cards.at(11).spell == SpellState::Ready);
    REQUIRE(e.state().cards.at(12).spell == SpellState::Analyzing);
    next(e);
    reach(e, 0, Phase::Main);
    send(e, 0, Abandon{11});
    auto state = e.state();
    CardInstance c;
    c.id = 14;
    c.definition = "magic-missile";
    c.zone = Zone::Hand;
    state.cards[14] = c;
    auto again = GameEngine::scenario(f.data.catalog, state);
    send(again, 0, StartAnalysis{14, 13});
    REQUIRE(again.state().cards.at(14).spell == SpellState::Ready);
}
TEST_CASE(
    "ambush uses shared back slots costs one hides enemy identity and cannot flip in the same global turn") {
    V1 f;
    f.add(10, "spark");
    f.add(11, "recall");
    f.add(12, "counter-spell");
    f.add(13, "aid");
    auto e = f.engine();
    send(e, 0, SetAmbush{10});
    send(e, 0, SetAmbush{11});
    send(e, 0, SetAmbush{12});
    REQUIRE(e.state().players[0].mana == 9);
    REQUIRE_FALSE(e.submit(0, SetAmbush{13, 0}).accepted);
    REQUIRE_FALSE(e.submit(0, FlipAmbush{10}).accepted);
    const auto enemy = e.viewFor(1);
    for (const auto &c : enemy.cards)
        if (c.instance.id == 10) {
            REQUIRE(c.hidden);
            REQUIRE(c.definition.id.empty());
            REQUIRE(c.definition.effects.empty());
            REQUIRE(c.instance.definition.empty());
        }
    reach(e, 0, Phase::Main);
    send(e, 0, FlipAmbush{10});
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().players[1].life == 18);
}
TEST_CASE("ambushed analytic reserves a ring never progresses and pays analysis cost on a later flip") {
    V1 f;
    f.add(10, "fireball");
    auto e = f.engine();
    send(e, 0, SetAmbush{10, 1});
    REQUIRE(Rules::occupied(e.state(), 1) == 1);
    reach(e, 0, Phase::Main);
    REQUIRE(e.state().cards.at(10).spell == SpellState::None);
    send(e, 0, FlipAmbush{10});
    REQUIRE(e.state().cards.at(10).spell == SpellState::Analyzing);
    REQUIRE(e.state().cards.at(10).settingPaid == 3);
    reach(e, 0, Phase::Main);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Ready);
}
TEST_CASE("effect concealed ready spell loses analysis and cannot evade the ambush turn limit") {
    V1 f;
    f.ready(10, "fireball");
    Trigger t;
    t.owner = 0;
    t.targetCard = 10;
    EffectResolver::apply(f.s, f.data.catalog, t, {EffectKind::Conceal});
    auto e = f.engine();
    REQUIRE(e.state().cards.at(10).faceDown);
    REQUIRE(e.state().cards.at(10).spell == SpellState::None);
    REQUIRE_FALSE(e.submit(0, FlipAmbush{10}).accepted);
    reach(e, 0, Phase::Main);
    send(e, 0, FlipAmbush{10});
    REQUIRE(e.state().cards.at(10).spell == SpellState::Analyzing);
}
TEST_CASE("ready magic missile responds only in opponent cast phase paying real cast fee and optionally "
          "destroys hidden card") {
    V1 f;
    f.s.phase = Phase::Cast;
    f.ready(10, "magic-missile", 1);
    auto &hidden = f.add(11, "recall", 0, Zone::Words);
    hidden.faceDown = true;
    hidden.ambushedTurn = 0;
    auto e = f.engine();
    send(e, 0, AdvancePhase{e.state().phaseGate});
    REQUIRE(e.state().decision);
    auto r = response(e, 1, 10);
    send(e, 1, r);
    REQUIRE(e.state().decision->kind == DecisionKind::DestroyAmbush);
    REQUIRE(e.state().players[0].life == 17);
    REQUIRE(e.state().players[1].mana == 10);
    send(e, 1, Choose{e.state().decision->id, 11});
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("protective flame can resolve ahead of fireball in the same chain then maintain concentration") {
    V1 f;
    f.s.phase = Phase::Cast;
    auto &fire = f.ready(10, "fireball");
    fire.zone = Zone::Casting;
    fire.host = 0;
    fire.spell = SpellState::Pending;
    fire.targetPlayer = 1;
    f.word(11, "protective-flame", 1);
    f.s.decision = PendingDecision{1, 0, DecisionKind::CastOrder, {10}};
    auto e = f.engine();
    send(e, 0, Choose{1, 10});
    send(e, 1, response(e, 1, 11));
    REQUIRE(e.state().players[1].life == 15);
    REQUIRE(e.state().cards.at(11).spell == SpellState::Active);
    reach(e, 1, Phase::Prepare);
    REQUIRE(e.state().decision->kind == DecisionKind::Concentration);
    send(e, 1, Choose{e.state().decision->id, 0});
    REQUIRE(e.viewFor(1).players[1].resistances.empty());
}
TEST_CASE(
    "counter spell closes four speed chain destroys target and keeps next own end load with no refund") {
    V1 f;
    f.s.phase = Phase::Cast;
    auto &fire = f.ready(10, "fireball");
    fire.zone = Zone::Casting;
    fire.host = 0;
    fire.spell = SpellState::Pending;
    fire.targetPlayer = 1;
    f.word(11, "counter-spell", 1);
    f.word(12, "counter-spell", 0);
    f.s.decision = PendingDecision{1, 0, DecisionKind::CastOrder, {10}};
    auto e = f.engine();
    send(e, 0, Choose{1, 10});
    send(e, 1, response(e, 1, 11));
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().players[1].life == 20);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).zone == Zone::Words);
    REQUIRE(e.state().temporary.size() == 1);
    REQUIRE(e.state().temporary[0].amount == 1);
    reach(e, 1, Phase::End);
    REQUIRE(e.state().temporary.size() == 1);
    next(e);
    REQUIRE(e.state().temporary.empty());
}
TEST_CASE("v1 explicitly rejects hand responses and speed one response abilities") {
    V1 f;
    auto d = f.data.catalog.at("protective-flame");
    d.id = "test-hand";
    d.responses[0].fromHand = true;
    d.responses[0].handCost = 1;
    f.data.catalog.cards[d.id] = d;
    f.add(10, "test-hand", 1);
    f.add(11, "spark");
    auto e = f.engine();
    send(e, 0, PreloadWord{11, true});
    REQUIRE(e.state().players[1].life == 18);
    REQUIRE(e.state().cards.at(10).zone == Zone::Hand);
    f.word(12, "protective-flame", 1);
    f.data.catalog.cards.at("protective-flame").speed = 1;
    e = f.engine();
    send(e, 0, PreloadWord{11, true});
    REQUIRE(e.state().players[1].life == 18);
}
TEST_CASE("sleep blocks exactly the next enemy own turn including flipping ambushed actions") {
    V1 f;
    f.ready(10, "instantly-sleep");
    f.add(11, "recall", 1);
    auto &hidden = f.add(12, "clarity", 1, Zone::Words);
    hidden.faceDown = true;
    auto e = f.engine();
    send(e, 0, PrepareCast{10});
    reach(e, 0, Phase::Cast);
    send(e, 0, Choose{e.state().decision->id, 10});
    reach(e, 1, Phase::Main);
    REQUIRE_FALSE(e.submit(1, PlayAction{11}).accepted);
    REQUIRE_FALSE(e.submit(1, FlipAmbush{12}).accepted);
    reach(e, 0, Phase::Main);
    reach(e, 1, Phase::Main);
    send(e, 1, PlayAction{11});
}
TEST_CASE("instant circuit targets only analyzing own spells and burdens actual discounted analysis cost "
          "until next own end") {
    V1 f;
    f.add(10, "instant-circuit-overload");
    auto &c = f.ready(11, "magic-missile");
    c.spell = SpellState::Analyzing;
    c.settingPaid = c.analysisLoad = 1;
    f.ready(12, "aid");
    auto e = f.engine();
    REQUIRE_FALSE(e.submit(0, PlayAction{10, 12}).accepted);
    send(e, 0, PlayAction{10, 11});
    REQUIRE(e.state().cards.at(11).spell == SpellState::Ready);
    REQUIRE(e.state().temporary[0].amount == 1);
    reach(e, 1, Phase::Main);
    REQUIRE(e.state().temporary.size() == 1);
    reach(e, 0, Phase::End);
    next(e);
    REQUIRE(e.state().temporary.empty());
}
TEST_CASE(
    "true strike offers only eligible action cards searches privately and replay encodes new commands") {
    V1 f;
    f.add(10, "true-strike");
    f.s.cards.at(100).definition = "recall";
    f.s.cards.at(101).definition = "instant-circuit-overload";
    f.s.cards.at(102).definition = "counter-spell";
    auto e = f.engine();
    send(e, 0, PreloadWord{10, true});
    REQUIRE(e.state().decision->kind == DecisionKind::SearchDeck);
    REQUIRE(e.state().decision->options.size() == 2);
    const auto opponent = e.viewFor(1);
    REQUIRE(std::none_of(opponent.cards.begin(), opponent.cards.end(),
                         [](const CardView &c) { return c.instance.zone == Zone::Deck; }));
    auto own = e.viewFor(0);
    REQUIRE(ai::choose(own, ai::Difficulty::Hard));
    send(e, 0, Choose{e.state().decision->id, 101});
    REQUIRE(e.state().cards.at(101).zone == Zone::Hand);
    REQUIRE(e.state().players[0].deck.size() == 19);
    const auto after = e.viewFor(1);
    REQUIRE(std::none_of(after.events.begin(), after.events.end(),
                         [](const GameEvent &x) { return x.kind == "search"; }));
    for (Command cmd :
         std::vector<Command>{SetAmbush{10, 1}, FlipAmbush{10, 11, {11}}, ActivateSpell{10, 11, {11}}})
        REQUIRE(encodeCommand(decodeCommand(encodeCommand(cmd))) == encodeCommand(cmd));
}
TEST_CASE("messy wave load survives end cleanup and is removed with source including finite duration") {
    V1 f;
    f.ready(10, "messy-wave");
    auto e = f.engine();
    send(e, 0, PrepareCast{10});
    reach(e, 0, Phase::Cast);
    send(e, 0, Choose{e.state().decision->id, 10});
    REQUIRE(e.state().temporary[0].sourceBound);
    REQUIRE(e.state().temporary[0].amount == 5);
    reach(e, 1, Phase::End);
    next(e);
    REQUIRE(e.state().temporary[0].amount == 5);
    reach(e, 0, Phase::Cast);
    REQUIRE(e.state().temporary.size() == 2);
    REQUIRE(e.state().temporary[1].amount == 3);
    reach(e, 0, Phase::Main);
    reach(e, 0, Phase::Cast);
    reach(e, 0, Phase::End);
    next(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().temporary.empty());
}
TEST_CASE(
    "uplift raises formation capacity rank lowers income and adds paid force bonus to actual spell release") {
    V1 f;
    f.add(10, "uplift");
    f.add(11, "uplift");
    f.ready(12, "magic-missile");
    auto e = f.engine();
    send(e, 0, AttachSeal{10, 1});
    REQUIRE(Rules::capacity(e.state(), f.data.catalog, 0) == 41 + f.data.catalog.baseLoadCapacity);
    REQUIRE(Rules::maxRank(e.state(), f.data.catalog, 1) == 3);
    send(e, 0, AttachSeal{11, 12});
    REQUIRE(Rules::castCost(e.state(), f.data.catalog, 12) == 3);
    send(e, 0, PrepareCast{12});
    reach(e, 0, Phase::Cast);
    send(e, 0, Choose{e.state().decision->id, 12});
    REQUIRE(e.state().players[1].life == 15);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    reach(e, 0, Phase::Main);
    REQUIRE(e.state().players[0].mana == 8);
}
TEST_CASE(
    "new formation setting cost is paid separately from body and old actual zero setting costs remain") {
    V1 f;
    f.add(10, "pentagram-of-life");
    auto e = f.engine();
    send(e, 0, SetFormation{10});
    REQUIRE(e.state().players[0].mana == 11);
    REQUIRE(e.state().cards.at(10).settingPaid == 1);
    REQUIRE(f.data.catalog.at("balance").cost == 0);
    REQUIRE(f.data.catalog.at("reservoir").cost == 0);
}

TEST_CASE("cancelled preparation cannot bypass its retry restriction via direct cast") {
    V1 f;
    auto &c = f.ready(10, "magic-missile");
    c.canceledTurn = 2;
    f.s.phase = Phase::Cast;
    auto e = f.engine();
    REQUIRE_FALSE(e.submit(0, ActivateSpell{10}).accepted);
    const auto v = e.viewFor(0);
    REQUIRE(std::none_of(v.actions.begin(), v.actions.end(), [](const LegalAction &a) {
        return std::holds_alternative<ActivateSpell>(a.command);
    }));
    reach(e, 0, Phase::Cast);
    send(e, 0, ActivateSpell{10});
    REQUIRE(e.state().players[1].life == 17);
}
TEST_CASE("new representative decks finish every AI difficulty and replay all accepted commands") {
    const auto data = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    const auto presets = app::loadPresets(data, std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    for (int d = 0; d < 3; ++d)
        for (std::size_t k = 4; k < presets.size(); ++k) {
            MatchConfig cfg;
            cfg.seed = 42;
            cfg.players[0] = presets[k].deck;
            cfg.players[1] = presets[(k - 4 + d) % 3 + 4].deck;
            MatchSession session(data, cfg);
            for (int n = 0; session.engine().state().result == -1; ++n) {
                REQUIRE(n < 1500);
                const auto &state = session.engine().state();
                const auto p = state.decision ? state.decision->player : state.active;
                const auto selected = ai::choose(session.engine().viewFor(p), static_cast<ai::Difficulty>(d));
                REQUIRE(selected);
                auto result = session.submit(p, selected->command);
                INFO(result.error);
                REQUIRE(result.accepted);
            }
            REQUIRE(replay(data, session.recording()).canonicalState() == session.engine().canonicalState());
            session.save(std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "alpha-v1-new" /
                         (presets[k].id + "-" + std::to_string(d) + ".json"));
        }
}

TEST_CASE("speed three responds across phases and speed two cannot follow a faster link") {
    V1 f;
    auto fast = f.data.catalog.at("protective-flame");
    fast.id = "fast-guard";
    fast.speed = 3;
    f.data.catalog.cards[fast.id] = fast;
    auto root = f.data.catalog.at("spark");
    root.id = "fast-root";
    root.speed = 3;
    f.data.catalog.cards[root.id] = root;
    f.word(10, "fast-guard", 1);
    f.word(11, "protective-flame", 1);
    f.add(12, "fast-root");
    auto e = f.engine();
    send(e, 0, PreloadWord{12, true});
    REQUIRE(e.state().decision);
    const auto v = e.viewFor(1);
    REQUIRE(std::none_of(v.actions.begin(), v.actions.end(),
                         [](const LegalAction &a) { return a.source == 11; }));
    auto slow = response(e, 1, 10);
    slow.card = 11;
    REQUIRE_FALSE(e.submit(1, slow).accepted);
    send(e, 1, response(e, 1, 10));
    REQUIRE(e.state().players[1].life == 19);
    f.s.phase = Phase::Draw;
    e = f.engine();
    send(e, 0, AdvancePhase{e.state().phaseGate});
    REQUIRE(e.state().decision);
    REQUIRE(response(e, 1, 10).card == 10);
    const auto drawView = e.viewFor(1);
    REQUIRE(std::none_of(drawView.actions.begin(), drawView.actions.end(),
                         [](const LegalAction &a) { return a.source == 11; }));
}
TEST_CASE("ambushed counter pays on reveal only after global turn passes and privately held counter cannot "
          "respond") {
    V1 f;
    f.ready(10, "fireball");
    f.add(11, "counter-spell", 1);
    auto &ambush = f.add(12, "counter-spell", 1, Zone::Words);
    ambush.faceDown = true;
    ambush.ambushedTurn = f.s.globalTurn;
    auto e = f.engine();
    send(e, 0, PrepareCast{10});
    reach(e, 0, Phase::Cast);
    send(e, 0, Choose{e.state().decision->id, 10});
    REQUIRE(e.state().players[1].life == 10);
    REQUIRE(e.state().players[1].mana == 12);
    f.s.phase = Phase::Cast;
    f.s.globalTurn = 2;
    auto &pending = f.s.cards.at(10);
    pending.zone = Zone::Casting;
    pending.host = 0;
    pending.spell = SpellState::Pending;
    pending.targetPlayer = 1;
    f.s.decision = PendingDecision{1, 0, DecisionKind::CastOrder, {10}};
    e = f.engine();
    send(e, 0, Choose{1, 10});
    send(e, 1, response(e, 1, 12));
    REQUIRE(e.state().players[1].mana == 8);
    REQUIRE(e.state().players[1].life == 20);
    REQUIRE(e.state().cards.at(11).zone == Zone::Hand);
}

TEST_CASE("counter load snapshots real prepared payment even after casting cost rune was removed") {
    V1 f;
    f.ready(10, "fireball");
    f.add(11, "uplift");
    f.word(12, "counter-spell", 1);
    auto e = f.engine();
    send(e, 0, AttachSeal{11, 10});
    send(e, 0, PrepareCast{10});
    send(e, 0, RemoveSeal{11});
    REQUIRE(e.state().cards.at(10).payments.back().paid == 2);
    reach(e, 0, Phase::Cast);
    send(e, 0, Choose{e.state().decision->id, 10});
    send(e, 1, response(e, 1, 12));
    REQUIRE(e.state().temporary.front().amount == 2);
    REQUIRE(e.state().cards.at(10).payments.back().canceled);
}
TEST_CASE("required multi target release skips its entire effect when one target leaves in response") {
    V1 f;
    auto &d = f.data.catalog.cards.at("fireball");
    d.target = TargetKind::EnemyCard;
    d.targetCount = 2;
    auto word = f.data.catalog.at("protective-flame");
    word.id = "break-own";
    word.concentration = false;
    word.resistances.clear();
    word.responses[0].target = TargetKind::OwnCard;
    word.responses[0].effects = {{EffectKind::Destroy}};
    word.responses[0].windows = {ResponseWindow::Cast};
    f.data.catalog.cards[word.id] = word;
    f.ready(10, "fireball");
    f.add(20, "reservoir", 1, Zone::Analysis);
    f.add(21, "reservoir", 1, Zone::Analysis);
    f.word(30, "break-own", 1);
    auto e = f.engine();
    send(e, 0, PrepareCast{10, 20, 0, {20, 21}});
    reach(e, 0, Phase::Cast);
    send(e, 0, Choose{e.state().decision->id, 10});
    auto r = response(e, 1, 30);
    r.target = 20;
    r.targets = {20};
    send(e, 1, r);
    REQUIRE(e.state().players[0].life == 20);
    REQUIRE(e.state().players[1].life == 20);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("full new card replay covers ambush flip search and direct release through legal projection") {
    const auto data = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    const auto presets = app::loadPresets(data, std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    MatchConfig cfg;
    cfg.seed = 42;
    cfg.players[0] = cfg.players[1] = presets[4].deck;
    for (auto &player : cfg.players)
        std::replace(player.cards.begin(), player.cards.end(), std::string("arcane-recovery"),
                     std::string("recall"));
    MatchSession session(data, cfg);
    std::array<bool, 2> set{};
    bool flipped = false, searched = false;
    for (int n = 0; session.engine().state().result == -1; ++n) {
        REQUIRE(n < 1500);
        const auto &state = session.engine().state();
        const auto p = state.decision ? state.decision->player : state.active;
        const auto v = session.engine().viewFor(p);
        auto selected = ai::choose(v, ai::Difficulty::Easy);
        REQUIRE(selected);
        if (!set[p])
            for (const auto &a : v.actions)
                if (auto c = std::get_if<SetAmbush>(&a.command)) {
                    auto source = std::find_if(v.cards.begin(), v.cards.end(),
                                               [&](const CardView &x) { return x.instance.id == c->card; });
                    if (source != v.cards.end() && source->definition.id == "recall") {
                        selected->command = a.command;
                        set[p] = true;
                        break;
                    }
                }
        for (const auto &a : v.actions)
            if (std::holds_alternative<FlipAmbush>(a.command)) {
                selected->command = a.command;
                break;
            }
        flipped |= std::holds_alternative<FlipAmbush>(selected->command);
        searched |= v.decision && v.decision->kind == DecisionKind::SearchDeck;
        auto result = session.submit(p, selected->command);
        INFO(result.error);
        REQUIRE(result.accepted);
    }
    REQUIRE(flipped);
    REQUIRE(searched);
    REQUIRE(replay(data, session.recording()).digest() == session.engine().digest());
    session.save(std::filesystem::path(WIZARD_SOURCE_DIR) / "build/alpha-v1-new/ambush-search-ui.json");
}
