#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace wizard;

TEST_CASE("ice storm advances every analyzing opponent spell", "[storm][rc-content]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    const auto &catalog = content.catalog;
    const auto &iceStormDefinition = catalog.at("ice-storm");
    REQUIRE(iceStormDefinition.effects.size() == 2);
    REQUIRE(iceStormDefinition.effects[1].kind == EffectKind::AdvanceAllEnemyAnalysis);

    GameState state;
    state.phase = Phase::Cast;
    state.flow = Flow::Cast;
    state.phaseStep = PhaseStep::Body;
    state.phaseGate = 1;
    state.globalTurn = 3;
    state.rng = 17;
    state.players[0].mana = 20;
    state.players[1].mana = 10;

    auto add = [&](CardId id, const std::string &definition, PlayerId owner, Zone zone) -> CardInstance & {
        CardInstance card;
        card.id = id;
        card.definition = definition;
        card.owner = owner;
        card.zone = zone;
        card.fieldOrder = state.nextFieldOrder++;
        state.cards[id] = card;
        return state.cards.at(id);
    };

    add(1, "hexagram", 0, Zone::Analysis);
    add(2, "hexagram", 1, Zone::Analysis);
    auto &iceStorm = add(20, "ice-storm", 0, Zone::Casting);
    iceStorm.spell = SpellState::Ready;
    iceStorm.sourceFormation = 1;

    auto &firstEnemySpell = add(21, "fireball", 1, Zone::Analysis);
    firstEnemySpell.host = 2;
    firstEnemySpell.spell = SpellState::Analyzing;
    firstEnemySpell.analysisProgress = 0;

    auto &secondEnemySpell = add(22, "magic-missile", 1, Zone::Analysis);
    secondEnemySpell.host = 2;
    secondEnemySpell.spell = SpellState::Analyzing;
    secondEnemySpell.analysisProgress = 0;

    auto &hiddenEnemySpell = add(23, "fireball", 1, Zone::Analysis);
    hiddenEnemySpell.host = 2;
    hiddenEnemySpell.spell = SpellState::None;
    hiddenEnemySpell.faceDown = true;

    auto &ownSpell = add(24, "fireball", 0, Zone::Analysis);
    ownSpell.host = 1;
    ownSpell.spell = SpellState::Ready;
    ownSpell.analysisProgress = 1;

    auto engine = GameEngine::scenario(catalog, state);
    const auto released = engine.submit(0, ActivateSpell{20});
    INFO(released.error);
    REQUIRE(released.accepted);

    REQUIRE_FALSE(engine.state().decision);
    REQUIRE(engine.state().players[1].life == startingLife - 8);
    for (const auto id : {CardId{21}, CardId{22}}) {
        REQUIRE(engine.state().cards.at(id).analysisProgress == 1);
        REQUIRE(engine.state().cards.at(id).spell == SpellState::Ready);
    }
    REQUIRE(engine.state().cards.at(23).analysisProgress == 0);
    REQUIRE(engine.state().cards.at(23).analysisStarted == 0);
    REQUIRE(engine.state().cards.at(23).faceDown);
    REQUIRE(engine.state().cards.at(24).analysisProgress == 1);
    REQUIRE(engine.state().cards.at(24).analysisStarted == 0);
    REQUIRE(engine.state().cards.at(24).spell == SpellState::Ready);
}
