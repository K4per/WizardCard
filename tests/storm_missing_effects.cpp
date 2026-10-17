#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;

namespace {
Content stormContent() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
}

CardInstance &put(GameState &state, CardId id, const std::string &definition, PlayerId owner,
                  Zone zone, CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance card;
    card.id = id;
    card.definition = definition;
    card.owner = owner;
    card.zone = zone;
    card.host = zone == Zone::Analysis ? host : 0;
    card.sourceFormation = host;
    card.spell = spell;
    if (zone == Zone::Analysis)
        card.fieldOrder = state.nextFieldOrder++;
    state.cards.emplace(id, std::move(card));
    return state.cards.at(id);
}

GameState stateAt(Phase phase) {
    GameState state;
    state.phase = phase;
    state.phaseStep = PhaseStep::Body;
    state.phaseGate = 1;
    state.globalTurn = 5;
    state.rng = 17;
    state.players[0].mana = state.players[1].mana = 20;
    state.players[0].ownTurn = state.players[1].ownTurn = 2;
    return state;
}

void send(GameEngine &engine, PlayerId player, const Command &command) {
    const auto result = engine.submit(player, command);
    INFO(result.error);
    REQUIRE(result.accepted);
}

void passResponses(GameEngine &engine) {
    for (int count = 0; count < 16 && engine.state().decision &&
         engine.state().decision->kind == DecisionKind::Response; ++count) {
        const auto decision = *engine.state().decision;
        send(engine, decision.player, PassResponse{decision.id});
    }
}

void advanceCurrentPhase(GameEngine &engine, std::uint32_t effectChoice = 0) {
    REQUIRE(engine.state().phaseStep == PhaseStep::Body);
    send(engine, engine.state().active, AdvancePhase{engine.state().phaseGate});
    for (int count = 0; count < 16; ++count) {
        if (!engine.state().decision) {
            REQUIRE(engine.state().phaseStep == PhaseStep::Body);
            return;
        }
        const auto decision = *engine.state().decision;
        if (decision.kind == DecisionKind::Response) {
            send(engine, decision.player, PassResponse{decision.id});
        } else if (decision.kind == DecisionKind::EffectChoice && effectChoice &&
                   std::find(decision.options.begin(), decision.options.end(), effectChoice) !=
                       decision.options.end()) {
            send(engine, decision.player, Choose{decision.id, effectChoice});
            effectChoice = 0;
        } else {
            FAIL("unexpected phase decision");
        }
    }
    FAIL("phase did not advance");
}

void advanceToPhase(GameEngine &engine, Phase phase, PlayerId active) {
    for (int count = 0; count < 32; ++count) {
        if (engine.state().phase == phase && engine.state().active == active &&
            engine.state().phaseStep == PhaseStep::Body)
            return;
        advanceCurrentPhase(engine);
    }
    FAIL("target phase was not reached");
}

void choose(GameEngine &engine, std::uint32_t option) {
    REQUIRE(engine.state().decision);
    const auto decision = *engine.state().decision;
    send(engine, decision.player, Choose{decision.id, option});
}

void startStormGuidance(GameEngine &engine) {
    send(engine, 0, PreloadWord{10, true, 1});
    passResponses(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), CardId{2}) !=
            engine.state().decision->options.end());
    choose(engine, 2);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
}

void stormGuidanceState(GameState &state, const std::string &deckDefinition = {}) {
    put(state, 1, "storm-heart", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    put(state, 10, "storm-guidance", 0, Zone::Hand);
    if (!deckDefinition.empty()) {
        put(state, 20, deckDefinition, 0, Zone::Deck);
        state.players[0].deck.push_back(20);
    }
}
} // namespace

TEST_CASE("Storm Guidance sacrifices a field card, exposes legal mode actions, and searches a Storm spell",
          "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    stormGuidanceState(state, "storm-vortex");
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    startStormGuidance(engine);
    const auto pending = *engine.state().decision;
    REQUIRE(std::find(pending.options.begin(), pending.options.end(), 0xfffffff1u) != pending.options.end());
    const auto view = engine.viewFor(0);
    REQUIRE(view.decision);
    REQUIRE(view.decision->options == pending.options);
    const auto mode = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *selection = std::get_if<Choose>(&action.command);
        return selection && selection->option == 0xfffffff1u;
    });
    REQUIRE(mode != view.actions.end());
    REQUIRE(mode->label.find("检索一张风暴法术") != std::string::npos);
    send(engine, 0, mode->command);

    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(engine.state().decision->options == std::vector<std::uint32_t>{20});
    choose(engine, 20);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Hand);
    REQUIRE(engine.state().cards.at(1).stormPoints == 1);
}

TEST_CASE("Storm Guidance can add two Storm points after sacrificing a field card", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    stormGuidanceState(state);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    startStormGuidance(engine);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(),
                      0xfffffff2u) != engine.state().decision->options.end());
    choose(engine, 0xfffffff2u);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(1).stormPoints == 3);
}

TEST_CASE("Storm Guidance can search its auxiliary analytic spells", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    stormGuidanceState(state, "cloud-mist");
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    startStormGuidance(engine);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(),
                      0xfffffff3u) != engine.state().decision->options.end());
    choose(engine, 0xfffffff3u);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->options == std::vector<std::uint32_t>{20});
    choose(engine, 20);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Hand);
}

TEST_CASE("Wind Whisper searches only Storm talismans of rank two or lower", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    put(state, 1, "storm-heart", 0, Zone::Analysis);
    put(state, 10, "wind-whisper", 0, Zone::Hand);
    put(state, 20, "storm-vortex", 0, Zone::Deck);
    put(state, 21, "wind-strike", 0, Zone::Deck);
    put(state, 22, "windmaking", 0, Zone::Deck);
    state.players[0].deck = {20, 21, 22};
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, PreloadWord{10, true, 1});
    passResponses(engine);

    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::SearchDeck);
    REQUIRE(engine.state().decision->options == std::vector<std::uint32_t>{20});
    choose(engine, 20);

    REQUIRE(engine.state().cards.at(20).zone == Zone::Hand);
    REQUIRE(engine.state().cards.at(21).zone == Zone::Deck);
    REQUIRE(engine.state().cards.at(22).zone == Zone::Deck);
}

TEST_CASE("Storm Vortex destroys an opponent's field spell through its activation choice", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    auto &vortex = put(state, 10, "storm-vortex", 0, Zone::Casting, 1, SpellState::Ready);
    (void)vortex;
    auto &enemySpell = put(state, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Ready);
    enemySpell.ringIndex = 0;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, ActivateSpell{10});
    passResponses(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), 20) !=
            engine.state().decision->options.end());
    choose(engine, 20);

    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(1).stormPoints == 1);
}

TEST_CASE("Storm Vortex can destroy an opponent's active spell in the casting zone", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    put(state, 10, "storm-vortex", 0, Zone::Casting, 1, SpellState::Ready);
    put(state, 20, "fireball", 1, Zone::Casting, 2, SpellState::Active);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, ActivateSpell{10});
    passResponses(engine);

    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(engine.state().decision->options == std::vector<std::uint32_t>{20});
    choose(engine, 20);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
}

TEST_CASE("Windmaking returns an opponent's active casting spell to hand", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    put(state, 10, "windmaking", 0, Zone::Hand);
    put(state, 20, "fireball", 1, Zone::Casting, 2, SpellState::Active);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, PreloadWord{10, true, 1});
    passResponses(engine);

    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(engine.state().decision->options == std::vector<std::uint32_t>{20});
    choose(engine, 20);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Hand);
    REQUIRE(engine.state().cards.at(20).spell == SpellState::None);
}

TEST_CASE("Great Cyclone destroys all spells under the selected formation and spends five Storm points",
          "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Cast);
    auto &crest = put(state, 1, "storm-crest", 0, Zone::Analysis);
    crest.stormPoints = 8;
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    put(state, 10, "great-cyclone", 0, Zone::Casting, 1, SpellState::Ready);
    auto &analysisSpell = put(state, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Ready);
    analysisSpell.ringIndex = 0;
    auto &castingSpell = put(state, 21, "magic-missile", 1, Zone::Casting, 2, SpellState::Active);
    castingSpell.ringIndex = -1;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));
    const auto enemyLife = engine.state().players[1].life;

    send(engine, 0, ActivateSpell{10});
    passResponses(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), CardId{2}) !=
            engine.state().decision->options.end());
    choose(engine, 2);

    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(21).zone == Zone::Ash);
    REQUIRE(engine.state().players[1].life == enemyLife - 4);
    REQUIRE(engine.state().cards.at(1).stormPoints == 4);
}

TEST_CASE("Cutting Wind sacrifices an idle formation and deals rank-scaled damage", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 0, Zone::Analysis);
    put(state, 10, "cutting-wind", 0, Zone::Casting, 1, SpellState::Ready);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));
    const auto enemyLife = engine.state().players[1].life;

    send(engine, 0, ActivateSpell{10});
    passResponses(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), 2) !=
            engine.state().decision->options.end());
    choose(engine, 2);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(1).zone == Zone::Analysis);
    REQUIRE(engine.state().players[1].life == enemyLife - 5);
    REQUIRE(engine.state().cards.at(1).stormPoints == 1);
}

TEST_CASE("Great Storm consumes the Heart's Storm points as its timer and counts destroyed field cards",
          "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Cast);
    auto &heart = put(state, 1, "storm-heart", 0, Zone::Analysis);
    heart.stormPoints = 3;
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    put(state, 10, "great-storm", 0, Zone::Casting, 1, SpellState::Ready);
    auto &enemySpell = put(state, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Ready);
    enemySpell.ringIndex = 0;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));
    const auto enemyLife = engine.state().players[1].life;

    send(engine, 0, ActivateSpell{10});
    passResponses(engine);

    REQUIRE(engine.state().cards.at(1).stormPoints == 1);
    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().players[1].life == enemyLife - 8);
    REQUIRE(engine.state().cards.at(10).remaining == 3);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Active);
}

TEST_CASE("Great Storm destroys the enemy Storm Heart when its owner declines token replacement",
          "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Cast);
    auto &heart = put(state, 1, "storm-heart", 0, Zone::Analysis);
    heart.stormPoints = 1;
    auto &enemyHeart = put(state, 2, "storm-heart", 1, Zone::Analysis);
    enemyHeart.stormPoints = 1;
    put(state, 10, "great-storm", 0, Zone::Casting, 1, SpellState::Ready);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));
    const auto enemyLife = engine.state().players[1].life;

    send(engine, 0, ActivateSpell{10});
    passResponses(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(engine.state().decision->player == 1);
    REQUIRE(engine.state().decision->mayPass);
    choose(engine, 0);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().players[1].life == enemyLife - 4);
}

TEST_CASE("Great Storm blocks setting analytics while it remains on Storm Heart", "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    put(state, 1, "storm-heart", 0, Zone::Analysis);
    auto &greatStorm = put(state, 10, "great-storm", 0, Zone::Analysis, 1, SpellState::Active);
    greatStorm.remaining = 2;
    put(state, 20, "fireball", 0, Zone::Hand);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    const auto setup = engine.submit(0, StartAnalysis{20, 1});
    REQUIRE_FALSE(setup.accepted);
    REQUIRE(setup.error.find("great_storm_lock") != std::string::npos);
    const auto ambush = engine.submit(0, SetAmbush{20, 1});
    REQUIRE_FALSE(ambush.accepted);
    REQUIRE(ambush.error.find("great_storm_lock") != std::string::npos);

    const auto view = engine.viewFor(0);
    REQUIRE(std::none_of(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *command = std::get_if<StartAnalysis>(&action.command);
        return command && command->card == 20;
    }));
}

TEST_CASE("Great Storm hosted by Storm Heart is protected from generic destroy effects",
          "[storm][storm-effects]") {
    auto content = stormContent();
    CardDefinition destroyer;
    destroyer.id = "test-destroyer";
    destroyer.name = "Test Destroyer";
    destroyer.type = CardType::Action;
    destroyer.target = TargetKind::EnemyCard;
    destroyer.effects = {{EffectKind::Destroy}};
    content.catalog.cards.emplace(destroyer.id, destroyer);

    auto state = stateAt(Phase::Main);
    put(state, 1, "storm-heart", 0, Zone::Analysis);
    auto &greatStorm = put(state, 20, "great-storm", 0, Zone::Analysis, 1, SpellState::Active);
    greatStorm.remaining = 2;
    put(state, 10, "test-destroyer", 1, Zone::Hand);
    state.active = 1;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 1, PlayAction{10, 20});
    passResponses(engine);

    REQUIRE(engine.state().cards.at(20).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(20).spell == SpellState::Active);
}

TEST_CASE("Storm Heart can spend a Storm point to replace generic destruction", "[storm][storm-effects]") {
    auto content = stormContent();
    CardDefinition destroyer;
    destroyer.id = "test-destroyer";
    destroyer.name = "Test Destroyer";
    destroyer.type = CardType::Action;
    destroyer.target = TargetKind::EnemyCard;
    destroyer.effects = {{EffectKind::Destroy}};
    content.catalog.cards.emplace(destroyer.id, destroyer);

    auto state = stateAt(Phase::Main);
    auto &heart = put(state, 2, "storm-heart", 1, Zone::Analysis);
    heart.stormPoints = 2;
    put(state, 10, "test-destroyer", 0, Zone::Hand);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, PlayAction{10, 2});
    passResponses(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(engine.state().decision->player == 1);
    REQUIRE(engine.state().decision->mayPass);
    choose(engine, 2);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(2).stormPoints == 1);
}

TEST_CASE("Storm Heart is destroyed when its owner declines generic token replacement", "[storm][storm-effects]") {
    auto content = stormContent();
    CardDefinition destroyer;
    destroyer.id = "test-destroyer";
    destroyer.name = "Test Destroyer";
    destroyer.type = CardType::Action;
    destroyer.target = TargetKind::EnemyCard;
    destroyer.effects = {{EffectKind::Destroy}};
    content.catalog.cards.emplace(destroyer.id, destroyer);

    auto state = stateAt(Phase::Main);
    auto &heart = put(state, 2, "storm-heart", 1, Zone::Analysis);
    heart.stormPoints = 2;
    put(state, 10, "test-destroyer", 0, Zone::Hand);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, PlayAction{10, 2});
    passResponses(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    choose(engine, 0);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
}

TEST_CASE("Great Storm counts its timer and resolves one end effect per owner's turn",
          "[storm][storm-effects]") {
    auto content = stormContent();
    auto state = stateAt(Phase::End);
    put(state, 1, "storm-heart", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    auto &greatStorm = put(state, 10, "great-storm", 0, Zone::Analysis, 1, SpellState::Active);
    greatStorm.remaining = 3;
    greatStorm.ringIndex = 0;
    auto &enemySpell = put(state, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Ready);
    enemySpell.ringIndex = 0;
    for (CardId id = 100; id < 110; ++id) {
        put(state, id, "wind-whisper", 1, Zone::Deck);
        state.players[1].deck.push_back(id);
    }
    auto engine = GameEngine::scenario(content.catalog, std::move(state));
    const auto enemyLife = engine.state().players[1].life;

    advanceCurrentPhase(engine, 20);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(10).remaining == 2);
    REQUIRE(engine.state().players[1].life == enemyLife - 8);

    advanceToPhase(engine, Phase::End, 1);
    advanceCurrentPhase(engine);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(10).remaining == 2);
    REQUIRE(engine.state().players[1].life == enemyLife - 8);

    advanceToPhase(engine, Phase::End, 0);
    advanceCurrentPhase(engine);
    REQUIRE(engine.state().cards.at(10).remaining == 1);
    REQUIRE(engine.state().players[1].life == enemyLife - 16);

    advanceToPhase(engine, Phase::End, 1);
    advanceCurrentPhase(engine);
    REQUIRE(engine.state().cards.at(10).remaining == 1);

    advanceToPhase(engine, Phase::End, 0);
    advanceCurrentPhase(engine);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(engine.state().players[1].life == enemyLife - 24);
}
