#include "wizard/core/engine.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace wizard;
namespace {
struct ResourceFixture {
    CardCatalog catalog;
    GameState state;
    ResourceFixture() {
        catalog.rulesVersion = catalog.cardSetVersion = "2.0.0-draft";
        catalog.alphaV2Draft = true;
        catalog.advancedRules = true;
        catalog.baseLoadCapacity = 2;
        CardDefinition formation; formation.id = "balance"; formation.name = "测试阵法";
        formation.type = CardType::Formation; formation.body = 1; formation.capacity = 8;
        formation.rings = 3; formation.maxRank = 1; formation.baseEligible = true;
        catalog.cards.emplace(formation.id, formation);
        state.phase = Phase::Main; state.phaseStep = PhaseStep::Body; state.flow = Flow::Main;
        state.rng = 42; state.phaseGate = 1; state.nextDecision = 2;
        for (int player = 0; player < 2; ++player) {
            state.players[player].ownTurn = 1;
            CardInstance card; card.id = static_cast<CardId>(player + 1); card.definition = "balance";
            card.owner = player; card.zone = Zone::Analysis; card.base = true;
            state.cards.emplace(card.id, card);
        }
    }
    void load(PlayerId player, int amount) {
        state.temporary.push_back({state.nextLoad++, player, 0, amount, 0, true});
    }
    void effect(EffectKind kind, int amount, PlayerId player = 0) {
        Trigger trigger; trigger.owner = player;
        EffectResolver::apply(state, catalog, trigger, {kind, amount});
    }
};
}
TEST_CASE("Alpha v2 overload counts the entire positive increment and repeated increases") {
    ResourceFixture fixture; fixture.load(0, 9);
    fixture.effect(EffectKind::AddIndependent, 4);
    REQUIRE(Rules::load(fixture.state, 0) == 13);
    REQUIRE(fixture.state.players[0].life == 32); // All 4, not the 3 beyond capacity.
    REQUIRE(fixture.state.result == -1);
    fixture.effect(EffectKind::AddIndependent, 1);
    REQUIRE(fixture.state.players[0].life == 30);
    fixture.effect(EffectKind::AddIndependent, 0);
    REQUIRE(fixture.state.players[0].life == 30);
}
TEST_CASE("Alpha v2 overload at exact capacity is harmless and uses temporary life first") {
    ResourceFixture fixture; fixture.load(0, 9);
    fixture.effect(EffectKind::AddTemporaryLife, 5);
    fixture.effect(EffectKind::AddIndependent, 1);
    REQUIRE(fixture.state.players[0].temporaryLife == 5);
    fixture.effect(EffectKind::AddIndependent, 4);
    REQUIRE(fixture.state.players[0].temporaryLife == 0);
    REQUIRE(fixture.state.players[0].life == 37);
}
TEST_CASE("Alpha v2 cap decrease deals no increment damage and final threshold checks both players") {
    ResourceFixture fixture; fixture.load(0, 9);
    fixture.state.cards.erase(1);
    StateMaintenance::check(fixture.state, fixture.catalog);
    REQUIRE(fixture.state.players[0].life == 40);
    REQUIRE(fixture.state.result == -1);
    StateMaintenance::check(fixture.state, fixture.catalog, true);
    REQUIRE(fixture.state.result == 1);
    ResourceFixture simultaneous; simultaneous.load(0, 20); simultaneous.load(1, 20);
    StateMaintenance::check(simultaneous.state, simultaneous.catalog);
    REQUIRE(simultaneous.state.result == -1);
    StateMaintenance::check(simultaneous.state, simultaneous.catalog, true);
    REQUIRE(simultaneous.state.result == 2);
}
TEST_CASE("Alpha v2 transient double capacity survives if reduced before end cleanup") {
    ResourceFixture fixture; fixture.load(0, 9);
    fixture.effect(EffectKind::AddTemporary, 12);
    REQUIRE(fixture.state.players[0].life == 16);
    REQUIRE(Rules::load(fixture.state, 0) == 21);
    REQUIRE(fixture.state.result == -1);
    fixture.effect(EffectKind::ClearAllTemporary, 0);
    REQUIRE(Rules::load(fixture.state, 0) == 9);
    StateMaintenance::check(fixture.state, fixture.catalog, true);
    REQUIRE(fixture.state.result == -1);
    REQUIRE(fixture.state.players[0].life == 16);
}
TEST_CASE("Alpha v2 end threshold runs after expiring temporary loads in the actual engine") {
    ResourceFixture fixture; fixture.load(0, 9);
    fixture.effect(EffectKind::AddTemporary, 12);
    fixture.state.phase = Phase::End; fixture.state.flow = Flow::EndTriggers;
    auto engine = GameEngine::scenario(fixture.catalog, fixture.state);
    REQUIRE(engine.submit(0, AdvancePhase{engine.state().phaseGate}).accepted);
    REQUIRE(engine.state().result == -1);
    REQUIRE(engine.state().players[0].life == 16);
    REQUIRE(Rules::load(engine.state(), 0) == 9);
}
TEST_CASE("Alpha v2 empty draws deal cumulative per-player damage rather than instant defeat") {
    ResourceFixture fixture;
    fixture.effect(EffectKind::Draw, 3);
    REQUIRE(fixture.state.players[0].life == 34);
    REQUIRE(fixture.state.players[0].exhaustion == 3);
    REQUIRE_FALSE(fixture.state.players[0].drawFailed);
    REQUIRE(fixture.state.result == -1);
    fixture.effect(EffectKind::Draw, 1);
    REQUIRE(fixture.state.players[0].life == 30);
    fixture.effect(EffectKind::Draw, 1, 1);
    REQUIRE(fixture.state.players[1].life == 39);
    REQUIRE(fixture.state.players[1].exhaustion == 1);
}
TEST_CASE("Alpha v2 mixed draws process each failure individually and stop at life zero") {
    ResourceFixture fixture;
    CardInstance card; card.id = 10; card.definition = "balance"; card.zone = Zone::Deck;
    fixture.state.cards.emplace(card.id, card); fixture.state.players[0].deck.push_back(card.id);
    fixture.state.players[0].life = 3;
    fixture.effect(EffectKind::Draw, 5);
    REQUIRE(fixture.state.cards.at(10).zone == Zone::Hand);
    REQUIRE(fixture.state.players[0].exhaustion == 2);
    REQUIRE(fixture.state.players[0].life == 0);
    REQUIRE(fixture.state.result == 1);
}
TEST_CASE("Alpha v2 rules damage bypasses spell immunity and can be absorbed by temporary life") {
    ResourceFixture fixture;
    CardDefinition spell; spell.id = "immune"; spell.type = CardType::Analytic;
    spell.immunities.push_back(DamageType::Force); fixture.catalog.cards.emplace(spell.id, spell);
    CardInstance instance; instance.id = 10; instance.definition = spell.id;
    instance.zone = Zone::Casting; instance.spell = SpellState::Active;
    fixture.state.cards.emplace(instance.id, instance);
    REQUIRE(Rules::damage(fixture.state, fixture.catalog, 0, {EffectKind::Damage, 10}) == 0);
    fixture.load(0, 9); fixture.effect(EffectKind::AddTemporaryLife, 3);
    fixture.effect(EffectKind::AddIndependent, 4);
    REQUIRE(fixture.state.players[0].life == 35);
}
TEST_CASE("Alpha v2 draft fixtures are not accidentally usable as production matches") {
    ResourceFixture fixture; MatchConfig configuration;
    REQUIRE_THROWS(GameEngine(fixture.catalog, configuration));
    ResourceFixture zero; zero.catalog.baseLoadCapacity = 0;
    zero.state.cards.clear();
    StateMaintenance::check(zero.state, zero.catalog, true);
    REQUIRE(zero.state.result == -1);
}
TEST_CASE("Alpha v2 life zero interrupts the remaining effect frame immediately") {
    ResourceFixture fixture;
    CardDefinition action; action.id = "terminal-test"; action.name = "测试中断";
    action.type = CardType::Action;
    action.effects = {{EffectKind::Damage, 40}, {EffectKind::Heal, 40}};
    fixture.catalog.cards.emplace(action.id, action);
    CardInstance card; card.id = 10; card.definition = action.id; card.zone = Zone::Hand;
    fixture.state.cards.emplace(card.id, card);
    auto engine = GameEngine::scenario(fixture.catalog, fixture.state);
    REQUIRE(engine.submit(0, PlayAction{10}).accepted);
    REQUIRE(engine.state().result == 1);
    REQUIRE(engine.state().players[0].life == 0);
    REQUIRE_FALSE(engine.state().effect);
    REQUIRE_FALSE(engine.state().chain);
    auto finished = engine.state();
    const auto eventCount = finished.events.size();
    Trigger trigger; trigger.owner = 0;
    EffectResolver::apply(finished, fixture.catalog, trigger, {EffectKind::Heal, 40});
    REQUIRE(finished.players[0].life == 0);
    REQUIRE(finished.events.size() == eventCount);
}
TEST_CASE("Alpha v2 nonzero load with zero capacity loses only at end cleanup") {
    ResourceFixture fixture; fixture.catalog.baseLoadCapacity = 0; fixture.state.cards.clear();
    fixture.load(0, 1);
    StateMaintenance::check(fixture.state, fixture.catalog);
    REQUIRE(fixture.state.result == -1);
    REQUIRE(fixture.state.players[0].life == 40);
    StateMaintenance::check(fixture.state, fixture.catalog, true);
    REQUIRE(fixture.state.result == 1);
}
