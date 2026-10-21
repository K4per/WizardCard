#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;

namespace {
Content loadStormCloudContent() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "tests/fixtures/alpha-v251-production");
}

CardInstance fieldCard(CardId id, const std::string &definition, PlayerId owner, Zone zone,
                       CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance card;
    card.id = id;
    card.definition = definition;
    card.owner = owner;
    card.zone = zone;
    card.host = host;
    card.sourceFormation = host;
    card.spell = spell;
    return card;
}

GameState cloudMistState() {
    GameState state;
    state.phase = Phase::Cast;
    state.flow = Flow::Cast;
    state.phaseStep = PhaseStep::Body;
    state.phaseGate = 1;
    state.active = 0;
    state.globalTurn = 3;
    state.rng = 17;
    state.players[0].mana = 20;
    state.players[1].mana = 10;

    state.cards.emplace(1, fieldCard(1, "hexagram", 0, Zone::Analysis));
    state.cards.emplace(2, fieldCard(2, "hexagram", 1, Zone::Analysis));
    auto cloudMist = fieldCard(20, "cloud-mist", 0, Zone::Casting, 0, SpellState::Ready);
    state.cards.emplace(20, cloudMist);
    state.cards.emplace(21, fieldCard(21, "magic-missile", 1, Zone::Analysis, 2, SpellState::Ready));
    state.cards.emplace(22, fieldCard(22, "magic-missile", 1, Zone::Analysis, 2, SpellState::Ready));
    auto hidden = fieldCard(23, "magic-missile", 1, Zone::Analysis, 2);
    hidden.faceDown = true;
    state.cards.emplace(23, hidden);
    state.cards.emplace(24, fieldCard(24, "magic-missile", 0, Zone::Analysis, 1, SpellState::Ready));
    return state;
}
}

TEST_CASE("cloud mist locks up to two field analytic spells until its duration ends", "[storm][rc-content]") {
    const auto content = loadStormCloudContent();
    const auto &catalog = content.catalog;
    REQUIRE(catalog.at("cloud-mist").effects.size() == 1);
    REQUIRE(catalog.at("cloud-mist").effects.front().kind == EffectKind::LockAnalysis);
    REQUIRE(catalog.at("cloud-mist").tags.empty());
    REQUIRE(catalog.at("thunder-summon").tags.empty());

    auto engine = GameEngine::scenario(catalog, cloudMistState());
    auto passResponses = [&] {
        while (engine.state().decision && engine.state().decision->kind == DecisionKind::Response) {
            const auto response = *engine.state().decision;
            const auto passed = engine.submit(response.player, PassResponse{response.id});
            INFO(passed.error);
            REQUIRE(passed.accepted);
        }
    };
    auto activate = engine.submit(0, ActivateSpell{20});
    INFO(activate.error);
    REQUIRE(activate.accepted);
    passResponses();
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), CardId{21}) !=
            engine.state().decision->options.end());
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), CardId{22}) !=
            engine.state().decision->options.end());
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), CardId{23}) ==
            engine.state().decision->options.end());

    auto first = engine.submit(0, Choose{engine.state().decision->id, 21});
    INFO(first.error);
    REQUIRE(first.accepted);
    REQUIRE(engine.state().decision);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), CardId{21}) ==
            engine.state().decision->options.end());
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), CardId{24}) !=
            engine.state().decision->options.end());

    auto second = engine.submit(0, Choose{engine.state().decision->id, 24});
    INFO(second.error);
    REQUIRE(second.accepted);
    passResponses();
    const auto advance = engine.submit(0, AdvancePhase{engine.state().phaseGate});
    INFO(advance.error);
    REQUIRE(advance.accepted);
    passResponses();
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::ReturnSpell);
    auto returnToField = engine.submit(0, Choose{engine.state().decision->id, 1});
    INFO(returnToField.error);
    REQUIRE(returnToField.accepted);
    passResponses();

    const auto &activeCloudMist = engine.state().cards.at(20);
    REQUIRE(activeCloudMist.zone == Zone::Analysis);
    REQUIRE(activeCloudMist.spell == SpellState::Active);
    REQUIRE(activeCloudMist.remaining == 2);
    REQUIRE((activeCloudMist.targets == std::vector<CardId>{21, 24}));

    auto lockState = engine.state();
    lockState.active = 1;
    lockState.phase = Phase::Main;
    lockState.flow = Flow::Main;
    lockState.phaseStep = PhaseStep::Body;
    lockState.phaseGate = 1;
    auto lockedEngine = GameEngine::scenario(catalog, lockState);
    const auto blocked = lockedEngine.submit(1, PrepareCast{21});
    INFO(blocked.error);
    REQUIRE_FALSE(blocked.accepted);
    REQUIRE(blocked.error.find("preparation_locked") != std::string::npos);

    const auto unselected = lockedEngine.submit(1, PrepareCast{22});
    INFO(unselected.error);
    REQUIRE(unselected.accepted);

    lockState.cards.at(20).remaining = 0;
    auto expiredEngine = GameEngine::scenario(catalog, lockState);
    const auto afterExpiry = expiredEngine.submit(1, PrepareCast{21});
    INFO(afterExpiry.error);
    REQUIRE(afterExpiry.accepted);
}
