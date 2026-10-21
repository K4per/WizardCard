#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;

namespace {
Content stormContent() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "tests/fixtures/alpha-v251-production");
}

GameState stormState() {
    GameState state;
    state.phase = Phase::Main;
    state.phaseStep = PhaseStep::Body;
    state.flow = Flow::Main;
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
    add(1, "storm-crest", 0, Zone::Analysis);
    add(2, "hexagram", 1, Zone::Analysis);
    add(10, "wind-whisper", 0, Zone::Hand);
    add(11, "storm-vortex", 0, Zone::Deck);
    state.players[0].deck.push_back(11);
    return state;
}
}

TEST_CASE("successfully releasing a storm spell adds a storm point", "[storm]") {
    auto content = stormContent();
    auto engine = GameEngine::scenario(content.catalog, stormState());

    const auto released = engine.submit(0, PreloadWord{10, true, 1});
    INFO(released.error);
    REQUIRE(released.accepted);
    REQUIRE(engine.state().decision);
    const auto search = engine.submit(0, Choose{engine.state().decision->id, 11});
    INFO(search.error);
    REQUIRE(search.accepted);
    REQUIRE(engine.state().cards.at(1).stormPoints == 1);
}

TEST_CASE("storm crest attributes follow its point thresholds in the match view", "[storm]") {
    auto content = stormContent();
    auto state = stormState();
    state.cards.at(1).stormPoints = 2;
    auto engine = GameEngine::scenario(content.catalog, state);
    auto findCrest = [](const GameView &view) -> const CardView & {
        return *std::find_if(view.cards.begin(), view.cards.end(), [](const CardView &card) {
            return card.instance.id == 1;
        });
    };

    auto view = engine.viewFor(0);
    REQUIRE(view.players[0].capacity == content.catalog.baseLoadCapacity + 5);
    REQUIRE(findCrest(view).effectiveRings == 1);
    REQUIRE(findCrest(view).definition.level == 1);

    state.cards.at(1).stormPoints = 4;
    engine = GameEngine::scenario(content.catalog, state);
    view = engine.viewFor(0);
    REQUIRE(view.players[0].capacity == content.catalog.baseLoadCapacity + 8);
    REQUIRE(findCrest(view).effectiveRings == 2);
    REQUIRE(findCrest(view).definition.level == 2);

    state.cards.at(1).stormPoints = 5;
    engine = GameEngine::scenario(content.catalog, state);
    view = engine.viewFor(0);
    REQUIRE(view.players[0].capacity == content.catalog.baseLoadCapacity + 10);
    REQUIRE(findCrest(view).effectiveRings == 3);
    REQUIRE(findCrest(view).definition.level == 3);
}

TEST_CASE("wind strike responds to a prepared analytic and uses its rank for damage and destruction", "[storm]") {
    auto content = stormContent();
    auto state = stormState();
    state.active = 1;
    state.phase = Phase::Main;
    state.cards.erase(10);
    auto &crest = state.cards.at(1);
    crest.stormPoints = 5;

    CardInstance strike;
    strike.id = 10;
    strike.definition = "wind-strike";
    strike.owner = 0;
    strike.zone = Zone::Analysis;
    strike.host = strike.sourceFormation = 1;
    strike.ringIndex = 0;
    strike.faceDown = strike.wasAmbushed = true;
    strike.ambushedTurn = 1;
    state.cards.emplace(strike.id, strike);

    CardInstance target;
    target.id = 20;
    target.definition = "cloud-mist";
    target.owner = 1;
    target.zone = Zone::Analysis;
    target.host = target.sourceFormation = 2;
    target.ringIndex = 0;
    target.spell = SpellState::Ready;
    state.cards.emplace(target.id, target);

    auto engine = GameEngine::scenario(content.catalog, state);
    const auto declared = engine.submit(1, PrepareCast{20});
    INFO(declared.error);
    REQUIRE(declared.accepted);
    REQUIRE(engine.state().decision);

    const auto actions = engine.viewFor(0).actions;
    const auto responseAction = std::find_if(actions.begin(), actions.end(), [](const LegalAction &action) {
        const auto *response = std::get_if<Respond>(&action.command);
        return response && response->card == 10 && response->ability == "wind-strike";
    });
    REQUIRE(responseAction != actions.end());
    const auto response = engine.submit(0, responseAction->command);
    INFO(response.error);
    REQUIRE(response.accepted);
    REQUIRE(engine.state().players[1].life == startingLife - 4);
    REQUIRE(engine.state().decision);
    const auto destroy = engine.submit(0, Choose{engine.state().decision->id, 20});
    INFO(destroy.error);
    REQUIRE(destroy.accepted);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
}
