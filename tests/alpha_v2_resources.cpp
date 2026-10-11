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
TEST_CASE("Alpha v2 cap decrease deals no increment damage and final threshold checks only the active player") {
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
    REQUIRE(simultaneous.state.result == 1);
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
TEST_CASE("Alpha v2 end threshold retains temporary loads until the next preparation") {
    ResourceFixture fixture; fixture.load(0, 9);
    fixture.effect(EffectKind::AddTemporary, 12);
    fixture.state.phase = Phase::End; fixture.state.flow = Flow::EndTriggers;
    auto engine = GameEngine::scenario(fixture.catalog, fixture.state);
    REQUIRE(engine.submit(0, AdvancePhase{engine.state().phaseGate}).accepted);
    REQUIRE(engine.state().result == 1);
    REQUIRE(engine.state().players[0].life == 16);
    REQUIRE(Rules::load(engine.state(), 0) == 21);
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
    action.type = CardType::Word; action.speed = 2; action.rank = 1;
    action.effects = {{EffectKind::Damage, 40}, {EffectKind::Heal, 40}};
    fixture.catalog.cards.emplace(action.id, action);
    CardInstance card; card.id = 10; card.definition = action.id; card.zone = Zone::Hand;
    fixture.state.cards.emplace(card.id, card);
    auto engine = GameEngine::scenario(fixture.catalog, fixture.state);
    REQUIRE(engine.submit(0, PreloadWord{10, true, 1}).accepted);
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
namespace {
struct FinalRulesFixture {
    CardCatalog catalog;
    GameState state;
    FinalRulesFixture() {
        catalog.alphaV2 = true; catalog.advancedRules = true;
        catalog.rulesVersion = catalog.cardSetVersion = "2.0.0"; catalog.baseLoadCapacity = 2;
        CardDefinition formation; formation.id = "formation"; formation.name = "起始阵法";
        formation.type = CardType::Formation; formation.level = 1; formation.maxRank = 1;
        formation.body = 1; formation.rings = 3; formation.capacity = 8; formation.income = 2;
        formation.baseEligible = true; catalog.cards.emplace(formation.id, formation);
        CardDefinition spell; spell.id = "spell"; spell.name = "解析法术"; spell.type = CardType::Analytic;
        spell.rank = 1; spell.cost = 2; spell.castCost = 2; spell.analysisTurns = 1;
        spell.target = TargetKind::Opponent; spell.effects = {{EffectKind::Damage, 5}};
        catalog.cards.emplace(spell.id, spell);
        state.phase = Phase::Main; state.phaseStep = PhaseStep::Body; state.flow = Flow::Main;
        state.rng = 42; state.phaseGate = 1; state.nextDecision = 2;
        for (int p = 0; p < 2; ++p) {
            state.players[p].ownTurn = 2; state.players[p].mana = 12;
            CardInstance card; card.id = static_cast<CardId>(p + 1); card.definition = "formation";
            card.owner = p; card.zone = Zone::Analysis; card.base = true;
            state.cards.emplace(card.id, card);
        }
    }
    CardInstance &spell(CardId id = 10, Zone zone = Zone::Hand, PlayerId owner = 0) {
        CardInstance card; card.id = id; card.definition = "spell"; card.owner = owner; card.zone = zone;
        if (zone == Zone::Analysis) { card.host = static_cast<CardId>(owner + 1); card.spell = SpellState::Ready; }
        return state.cards.emplace(id, card).first->second;
    }
};
void nextPhase(GameEngine &engine) {
    const auto result = engine.submit(engine.state().active, AdvancePhase{engine.state().phaseGate});
    INFO(result.error);
    REQUIRE(result.accepted);
}
}
TEST_CASE("Alpha v2 free ambush reserves a ring and enforces the global turn wait", "[alpha-v2]") {
    FinalRulesFixture f; f.spell();
    auto e = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(e.submit(0, SetAmbush{10, 1}).accepted);
    REQUIRE(e.state().players[0].mana == 12);
    REQUIRE(Rules::load(e.state(), 0) == 0);
    REQUIRE(Rules::occupied(e.state(), 1) == 1);
    const auto unchanged = e.digest();
    REQUIRE_FALSE(e.submit(0, FlipAmbush{10}).accepted);
    REQUIRE(e.digest() == unchanged);
    auto state = e.state(); ++state.globalTurn;
    e = GameEngine::scenario(f.catalog, state);
    REQUIRE(e.submit(0, FlipAmbush{10}).accepted);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Analyzing);
    REQUIRE(e.state().players[0].mana == 10);
    REQUIRE(Rules::load(e.state(), 0) == 2);
}
TEST_CASE("Alpha v2 preparation moves to casting before a separate one speed activation", "[alpha-v2]") {
    FinalRulesFixture f; f.spell(10, Zone::Analysis);
    auto e = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(e.submit(0, PrepareCast{10}).accepted);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    REQUIRE(e.state().players[0].mana == 12);
    REQUIRE_FALSE(e.submit(0, ActivateSpell{10}).accepted);
    nextPhase(e);
    REQUIRE(e.state().phase == Phase::Cast);
    REQUIRE(e.submit(0, ActivateSpell{10}).accepted);
    REQUIRE(e.state().players[1].life == 35);
    REQUIRE(e.state().players[0].mana == 10);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Resolved);
    REQUIRE_FALSE(e.submit(0, ActivateSpell{10}).accepted);
    nextPhase(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    nextPhase(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("Alpha v2 bound payment may overload and rejects unaffordable payment atomically", "[alpha-v2]") {
    FinalRulesFixture f; f.spell();
    f.state.temporary.push_back({1, 0, 0, 9, 0, true}); f.state.nextLoad = 2;
    auto e = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(e.submit(0, StartAnalysis{10, 1}).accepted);
    REQUIRE(Rules::load(e.state(), 0) == 11);
    REQUIRE(e.state().players[0].life == 36);
    REQUIRE(e.state().result == -1);
    auto state = f.state; state.players[0].mana = 1;
    e = GameEngine::scenario(f.catalog, state);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, StartAnalysis{10, 1}).accepted);
    REQUIRE(e.digest() == before);
}
TEST_CASE("Alpha v2 preparation clears old temporary resources before formation effects", "[alpha-v2]") {
    FinalRulesFixture f;
    f.catalog.cards.at("formation").onPrepare = {{EffectKind::AddTemporaryLife, 5}, {EffectKind::AddTemporary, 2}};
    f.state.phase = Phase::Draw; f.state.players[0].temporaryLife = 8;
    f.state.temporary.push_back({1, 0, 0, 4, 9, false});
    f.state.temporary.push_back({2, 0, 1, 1, 0, false, true}); f.state.nextLoad = 3;
    auto &spell = f.spell(10, Zone::Analysis); spell.spell = SpellState::Analyzing;
    auto e = GameEngine::scenario(f.catalog, f.state);
    nextPhase(e);
    REQUIRE(e.state().phase == Phase::Prepare);
    REQUIRE(e.state().players[0].temporaryLife == 5);
    REQUIRE(Rules::load(e.state(), 0) == 3);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Ready);
    REQUIRE(e.state().cards.at(10).analysisProgress == 1);
}
TEST_CASE("Alpha v2 duration returns to a chosen formation and ticks at every global end", "[alpha-v2]") {
    FinalRulesFixture f; f.catalog.cards.at("spell").speed = 2; f.catalog.cards.at("spell").duration = 2;
    auto &spell = f.spell(10, Zone::Casting); spell.spell = SpellState::Ready;
    auto e = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(e.submit(0, ActivateSpell{10}).accepted);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->kind == DecisionKind::ReturnSpell);
    REQUIRE(e.submit(0, Choose{e.state().decision->id, 1}).accepted);
    REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(e.state().cards.at(10).remaining == 2);
    nextPhase(e); nextPhase(e); nextPhase(e);
    REQUIRE(e.state().cards.at(10).remaining == 1);
    REQUIRE(e.state().active == 1);
    for (int i = 0; i < 5; ++i) nextPhase(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("Alpha v2 replacement recipes are atomic and may consume the initial formation", "[alpha-v2]") {
    FinalRulesFixture f; auto upgraded = f.catalog.at("formation"); upgraded.id = "upgraded";
    upgraded.name = "二级阵法"; upgraded.level = 2;
    upgraded.recipe = {{RecipeCostKind::Mana, 3}, {RecipeCostKind::SacrificeFormation, 1}};
    f.catalog.cards.emplace(upgraded.id, upgraded);
    CardInstance candidate; candidate.id = 10; candidate.definition = "upgraded"; candidate.zone = Zone::SideDeck;
    f.state.cards.emplace(candidate.id, candidate);
    auto e = GameEngine::scenario(f.catalog, f.state);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, SetFormation{10, 0, {2}}).accepted);
    REQUIRE(e.digest() == before);
    REQUIRE(e.submit(0, SetFormation{10, 0, {1}}).accepted);
    REQUIRE(e.state().cards.at(1).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(e.state().players[0].mana == 9);
    REQUIRE(e.state().players[0].formations == 1);
    REQUIRE_FALSE(e.submit(0, SetFormation{10, 0, {1}}).accepted);
}
TEST_CASE("Alpha v2 forty plus ten construction and side deck privacy", "[alpha-v2]") {
    FinalRulesFixture f;
    PlayerDeck deck; deck.baseFormation = "formation";
    for (int i = 0; i < 14; ++i) {
        auto card = f.catalog.at("spell"); card.id = "spell" + std::to_string(i); card.name = card.id;
        f.catalog.cards.emplace(card.id, card);
        for (int n = 0; n < (i == 13 ? 1 : 3); ++n) deck.cards.push_back(card.id);
    }
    for (int i = 0; i < 4; ++i) {
        auto card = f.catalog.at("formation");
        if (i) { card.id = "formation" + std::to_string(i); card.name = card.id; f.catalog.cards.emplace(card.id, card); }
        for (int n = 0; n < (i == 3 ? 1 : 3); ++n) deck.sideDeck.push_back(card.id);
    }
    REQUIRE(Rules::deckErrors(f.catalog, deck).empty());
    MatchConfig config; config.players = {deck, deck};
    GameEngine e(f.catalog, config);
    const auto view = e.viewFor(0);
    REQUIRE(view.players[0].handCount == 5);
    REQUIRE(view.players[0].deckCount == 35);
    REQUIRE(view.players[0].sideDeckCount == 9);
    REQUIRE(view.players[1].sideDeckCount == 9);
    REQUIRE(std::count_if(view.cards.begin(), view.cards.end(), [](const CardView &card) {
        return card.instance.zone == Zone::SideDeck && card.instance.owner == 0;
    }) == 9);
    REQUIRE(std::none_of(view.cards.begin(), view.cards.end(), [](const CardView &card) {
        return card.instance.zone == Zone::SideDeck && card.instance.owner == 1;
    }));
}
namespace {
void passAll(GameEngine &engine) {
    for (int n = 0; engine.state().decision && engine.state().decision->kind == DecisionKind::Response && n < 40; ++n) {
        const auto d = *engine.state().decision;
        REQUIRE(engine.submit(d.player, PassResponse{d.id}).accepted);
    }
}
ResponseAbility generalResponse() {
    ResponseAbility response; response.id = "cast"; response.fromAmbush = response.fromCasting = true;
    response.windows = {ResponseWindow::PhaseStart, ResponseWindow::PhaseEnd, ResponseWindow::Action,
                        ResponseWindow::Prepare, ResponseWindow::Cast, ResponseWindow::Trigger};
    response.target = TargetKind::Opponent;
    return response;
}
}
TEST_CASE("Alpha v2 opponents may flip and prepare only previously ambushed analytic sources", "[alpha-v2]") {
    FinalRulesFixture f; f.catalog.cards.at("spell").speed = 2;
    f.catalog.cards.at("spell").analysisTurns = 0;
    f.catalog.cards.at("spell").responses = {generalResponse()};
    f.spell(10); auto &enemy = f.spell(11, Zone::Analysis, 1);
    enemy.faceDown = true; enemy.wasAmbushed = true; enemy.ambushedTurn = 1; enemy.spell = SpellState::None;
    f.state.globalTurn = 2;
    auto engine = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(engine.submit(0, SetAmbush{10, 1}).accepted);
    REQUIRE(engine.state().decision); REQUIRE(engine.state().decision->player == 1);
    REQUIRE(engine.submit(1, FlipAmbush{11, 0, {}, engine.state().decision->id}).accepted);
    REQUIRE(engine.state().cards.at(11).spell == SpellState::Ready);
    REQUIRE(engine.state().players[1].mana == 10);
    REQUIRE(engine.submit(1, PrepareCast{11, 0, 0, {}, engine.state().decision->id}).accepted);
    REQUIRE(engine.state().cards.at(11).zone == Zone::Casting);
    REQUIRE(engine.state().decision); REQUIRE(engine.state().decision->player == 1);
    REQUIRE(engine.submit(1, Respond{engine.state().decision->id, 11, "cast"}).accepted);
    passAll(engine);
    REQUIRE(engine.state().players[0].life == 35);
    REQUIRE(engine.state().cards.at(11).spell == SpellState::Resolved);
    REQUIRE(engine.state().cards.at(11).zone == Zone::Casting);
}
TEST_CASE("Alpha v2 preparation has a cancellable root and does not prepay activation", "[alpha-v2]") {
    FinalRulesFixture f; f.spell(10, Zone::Analysis);
    auto counter = f.catalog.at("spell"); counter.id = "counter"; counter.type = CardType::Word;
    counter.speed = 2; counter.cost = 1; counter.responseOnly = true;
    auto ability = generalResponse(); ability.target = TargetKind::PreparationRoot;
    ability.windows = {ResponseWindow::Prepare}; ability.effects = {{EffectKind::CancelPreparation, 0}};
    counter.responses = {ability}; f.catalog.cards.emplace(counter.id, counter);
    auto &enemy = f.spell(11, Zone::Analysis, 1); enemy.definition = "counter";
    enemy.faceDown = true; enemy.wasAmbushed = true; enemy.ambushedTurn = 0; enemy.spell = SpellState::None;
    auto engine = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(engine.submit(0, PrepareCast{10}).accepted);
    REQUIRE(engine.state().decision); REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
    const auto root = engine.state().chain->links.front().id;
    REQUIRE(engine.submit(1, Respond{engine.state().decision->id, 11, "cast", 0, root}).accepted);
    passAll(engine);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(engine.state().players[0].mana == 12);
    REQUIRE_FALSE(engine.submit(0, PrepareCast{10}).accepted);
}
TEST_CASE("Alpha v2 actual chain completion creates one timing and empty timing cannot loop", "[alpha-v2]") {
    FinalRulesFixture f; auto &definition = f.catalog.cards.at("spell");
    definition.speed = 2; definition.responses = {generalResponse()};
    f.spell(10, Zone::Casting).spell = SpellState::Ready;
    f.spell(11, Zone::Casting).spell = SpellState::Ready;
    auto engine = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(engine.submit(0, ActivateSpell{10}).accepted);
    REQUIRE(engine.state().decision);
    const auto first = *engine.state().decision;
    REQUIRE(engine.submit(first.player, PassResponse{first.id}).accepted);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().chain->window == ResponseWindow::Action);
    passAll(engine);
    REQUIRE_FALSE(engine.state().chain); REQUIRE(engine.state().result == -1);
    REQUIRE(engine.state().phaseGate != 0);
}
TEST_CASE("Alpha v2 sustained return respects field order and permits choosing any free ring", "[alpha-v2]") {
    FinalRulesFixture f; f.catalog.cards.at("spell").duration = 2;
    f.state.phase = Phase::Cast;
    f.state.cards.at(1).fieldOrder = 3;
    auto &first = f.spell(20, Zone::Casting); first.spell = SpellState::Active; first.remaining = 2;
    first.sourceFormation = 1; first.sourceRingIndex = 0;
    auto &second = f.spell(10, Zone::Casting); second.spell = SpellState::Active; second.remaining = 2;
    second.sourceFormation = 1; second.sourceRingIndex = 2;
    auto engine = GameEngine::scenario(f.catalog, f.state);
    nextPhase(engine);
    REQUIRE(engine.state().maintenanceSource == 20);
    const auto oldDecision = engine.state().decision->id;
    REQUIRE(engine.submit(0, Choose{oldDecision, 1, 2}).accepted);
    REQUIRE(engine.state().cards.at(20).ringIndex == 2);
    REQUIRE(engine.state().maintenanceSource == 10);
    const auto unchanged = engine.digest();
    REQUIRE_FALSE(engine.submit(0, Choose{oldDecision, 1, 0}).accepted);
    REQUIRE(engine.digest() == unchanged);
    REQUIRE_FALSE(engine.submit(0, Choose{engine.state().decision->id, 1, 2}).accepted);
    REQUIRE(engine.submit(0, Choose{engine.state().decision->id, 1, 0}).accepted);
    REQUIRE(engine.state().cards.at(10).ringIndex == 0);
    REQUIRE(engine.state().phase == Phase::End);
}
TEST_CASE("Alpha v2 income effects complete before exhaustion and preparation clears only the owner", "[alpha-v2]") {
    FinalRulesFixture f;
    f.catalog.cards.at("formation").onMana = {{EffectKind::AddTemporaryLife, 3}};
    f.state.active = 1; f.state.phase = Phase::End;
    f.state.players[0].life = 1; f.state.players[1].temporaryLife = 7;
    auto engine = GameEngine::scenario(f.catalog, f.state);
    nextPhase(engine);
    REQUIRE(engine.state().phase == Phase::Draw);
    REQUIRE(engine.state().players[0].life == 1);
    REQUIRE(engine.state().players[0].temporaryLife == 2);
    nextPhase(engine);
    REQUIRE(engine.state().players[0].temporaryLife == 0);
    REQUIRE(engine.state().players[1].temporaryLife == 7);
}
TEST_CASE("Alpha v2 sustained passive phase effects continue after returning to analysis", "[alpha-v2]") {
    FinalRulesFixture f; f.catalog.cards.at("spell").duration = 3;
    f.catalog.cards.at("spell").onDraw = {{EffectKind::Heal, 4, DamageType::Force, EffectRecipient::Owner}};
    f.state.active = 1; f.state.phase = Phase::End; f.state.players[0].life = 20;
    auto &spell = f.spell(10, Zone::Analysis); spell.spell = SpellState::Active; spell.remaining = 3;
    auto engine = GameEngine::scenario(f.catalog, f.state); nextPhase(engine);
    REQUIRE(engine.state().players[0].life == 23);
    REQUIRE(engine.state().cards.at(10).remaining == 2);
}
TEST_CASE("Alpha v2 concentration and paused cleanup resume without repeating payment or expiry", "[alpha-v2]") {
    FinalRulesFixture f; f.catalog.cards.at("spell").concentration = true;
    f.catalog.cards.at("formation").onPrepare = {{EffectKind::AddTemporaryLife, 4}};
    auto &spell = f.spell(10, Zone::Analysis); spell.spell = SpellState::Active; spell.concentrationCost = 2;
    f.state.phase = Phase::Draw;
    auto engine = GameEngine::scenario(f.catalog, f.state); nextPhase(engine);
    REQUIRE(engine.state().decision->kind == DecisionKind::Concentration);
    auto token = engine.state().decision->id;
    REQUIRE(engine.submit(0, Choose{token, 10}).accepted);
    REQUIRE(engine.state().players[0].mana == 10);
    REQUIRE(engine.state().players[0].temporaryLife == 4);
    REQUIRE(Rules::load(engine.state(), 0) == 2);
    const auto before = engine.digest();
    REQUIRE_FALSE(engine.submit(0, Choose{token, 10}).accepted);
    REQUIRE(engine.digest() == before);
}
TEST_CASE("Alpha v2 lethal overload during a response stops before dereferencing the cleared chain", "[alpha-v2]") {
    FinalRulesFixture f; f.catalog.cards.at("spell").speed = 2;
    f.catalog.cards.at("spell").responses = {generalResponse()};
    f.spell(10); auto &enemy = f.spell(11, Zone::Casting, 1);
    enemy.spell = SpellState::Ready; enemy.wasAmbushed = true; enemy.ambushedTurn = 0;
    f.state.players[1].life = 2; f.state.temporary.push_back({1, 1, 0, 9, 0, true}); f.state.nextLoad = 2;
    auto engine = GameEngine::scenario(f.catalog, f.state);
    REQUIRE(engine.submit(0, SetAmbush{10, 1}).accepted);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.submit(1, Respond{engine.state().decision->id, 11, "cast"}).accepted);
    REQUIRE(engine.state().result == 0);
    REQUIRE_FALSE(engine.state().chain); REQUIRE_FALSE(engine.state().decision);
    REQUIRE(engine.state().players[0].life == 40);
}
