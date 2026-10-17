#include "wizard/application.hpp"
#include "wizard/command_codec.hpp"
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
    if (zone == Zone::Analysis) card.fieldOrder = state.nextFieldOrder++;
    state.cards.emplace(id, std::move(card));
    return state.cards.at(id);
}
GameState stateAt(Phase phase, PlayerId active = 0) {
    GameState state;
    state.phase = phase;
    state.phaseStep = PhaseStep::Body;
    state.phaseGate = 1;
    state.nextDecision = 2;
    state.globalTurn = 5;
    state.active = active;
    state.players[0].mana = state.players[1].mana = 12;
    state.players[0].ownTurn = state.players[1].ownTurn = 2;
    return state;
}
void send(GameEngine &engine, PlayerId player, const Command &command) {
    const auto result = engine.submit(player, command);
    INFO(result.error);
    REQUIRE(result.accepted);
}
void passResponses(GameEngine &engine) {
    for (int count = 0; count < 16 && engine.state().decision; ++count) {
        const auto decision = *engine.state().decision;
        REQUIRE(decision.kind == DecisionKind::Response);
        send(engine, decision.player, PassResponse{decision.id});
    }
    REQUIRE_FALSE(engine.state().decision);
}
void advanceToConcentration(GameEngine &engine) {
    for (int count = 0; count < 32; ++count) {
        if (engine.state().decision) {
            const auto decision = *engine.state().decision;
            if (decision.kind == DecisionKind::Concentration) return;
            REQUIRE(decision.kind == DecisionKind::Response);
            send(engine, decision.player, PassResponse{decision.id});
        } else {
            REQUIRE(engine.state().phaseStep == PhaseStep::Body);
            send(engine, engine.state().active, AdvancePhase{engine.state().phaseGate});
        }
    }
    FAIL("concentration upkeep decision was not reached");
}
bool hasAbility(const GameView &view, CardId source, const std::string &ability) {
    return std::any_of(view.actions.begin(), view.actions.end(), [&](const LegalAction &action) {
        const auto *use = std::get_if<ActivateAbility>(&action.command);
        return use && use->card == source && use->ability == ability;
    });
}
}

TEST_CASE("Snow-Rain Storm adds two mana to an opponent's timed spell activation", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Cast, 1);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    auto &rain = put(state, 10, "snow-rain-storm", 0, Zone::Casting);
    rain.spell = SpellState::Active;
    rain.sourceFormation = 1;
    auto &timed = put(state, 11, "cloud-mist", 1, Zone::Casting);
    timed.spell = SpellState::Ready;
    timed.sourceFormation = 2;
    const auto before = state.players[1].mana;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 1, ActivateSpell{11});

    REQUIRE(engine.state().players[1].mana == before - 5);
    REQUIRE(engine.state().cards.at(11).payments.back().paid == 5);
}

TEST_CASE("Snow-Rain Storm doubles an opponent's concentration upkeep", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Draw, 1);
    put(state, 1, "storm-heart", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    auto &rain = put(state, 10, "snow-rain-storm", 0, Zone::Analysis, 1, SpellState::Active);
    rain.ringIndex = 0;
    rain.spell = SpellState::Active;
    rain.sourceFormation = 1;
    auto &focus = put(state, 11, "ward", 1, Zone::Analysis);
    focus.host = 2;
    focus.sourceFormation = 2;
    focus.ringIndex = 0;
    focus.spell = SpellState::Active;
    focus.concentrationCost = 3;
    auto &draw = put(state, 20, "wind-whisper", 1, Zone::Deck);
    state.players[1].deck.push_back(draw.id);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    advanceToConcentration(engine);
    const auto decision = *engine.state().decision;
    const auto before = engine.state().players[1].mana;
    REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Active);
    REQUIRE(Rules::concentrationUpkeep(engine.state(), content.catalog, 11) == 6);
    send(engine, 1, Choose{decision.id, 11});

    REQUIRE(engine.state().players[1].mana == before - 6);
    REQUIRE(engine.state().cards.at(11).payments.back().paid == 6);
}

TEST_CASE("Storm seals enforce Storm formation hosts and the Annihilation Crest still attaches to spells", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    put(state, 2, "small-hexagram", 0, Zone::Analysis);
    put(state, 10, "storm-brand", 0, Zone::Hand);
    put(state, 11, "storm-annihilation-crest", 0, Zone::Hand);
    auto &spell = put(state, 20, "cloud-mist", 0, Zone::Casting);
    spell.spell = SpellState::Ready;
    spell.sourceFormation = 1;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    REQUIRE_FALSE(engine.submit(0, AttachSeal{10, 2}).accepted);
    REQUIRE_FALSE(engine.submit(0, AttachSeal{10, 20}).accepted);
    REQUIRE_FALSE(engine.submit(0, AttachSeal{11, 2}).accepted);
    const auto view = engine.viewFor(0);
    REQUIRE(std::none_of(view.actions.begin(), view.actions.end(),
        [](const LegalAction &action) {
            const auto *attach = std::get_if<AttachSeal>(&action.command);
            return attach && ((attach->card == 10 && attach->host == 2) ||
                              (attach->card == 11 && attach->host == 2));
        }));
    send(engine, 0, AttachSeal{10, 1});
    passResponses(engine);

    auto crestState = stateAt(Phase::Main);
    put(crestState, 1, "storm-crest", 0, Zone::Analysis);
    auto &hostSpell = put(crestState, 20, "cloud-mist", 0, Zone::Casting);
    hostSpell.spell = SpellState::Ready;
    hostSpell.sourceFormation = 1;
    put(crestState, 11, "storm-annihilation-crest", 0, Zone::Hand);
    auto crestEngine = GameEngine::scenario(content.catalog, std::move(crestState));
    send(crestEngine, 0, AttachSeal{11, 20});
    REQUIRE(crestEngine.state().cards.at(11).zone == Zone::Attached);
}

TEST_CASE("Storm Brand grants an extra point on every second successful Storm spell release", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto testSpell = content.catalog.at("storm-vortex");
    testSpell.id = "storm-test-spell";
    testSpell.name = "风暴测试法术";
    testSpell.effects.clear();
    testSpell.traits.clear();
    testSpell.tags = {"风暴"};
    testSpell.cost = 1;
    content.catalog.cards.emplace(testSpell.id, testSpell);
    auto state = stateAt(Phase::Cast);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    auto &brand = put(state, 10, "storm-brand", 0, Zone::Attached);
    brand.host = 1;
    auto &first = put(state, 20, testSpell.id, 0, Zone::Casting);
    first.spell = SpellState::Ready;
    auto &second = put(state, 21, testSpell.id, 0, Zone::Casting);
    second.spell = SpellState::Ready;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, ActivateSpell{20});
    passResponses(engine);
    REQUIRE(engine.state().cards.at(1).stormPoints == 1);
    REQUIRE(engine.state().cards.at(10).stormReleaseProgress == 1);
    send(engine, 0, ActivateSpell{21});
    passResponses(engine);

    REQUIRE(engine.state().cards.at(1).stormPoints == 3);
    REQUIRE(engine.state().cards.at(10).stormReleaseProgress == 2);
}

TEST_CASE("Storm Crest searches a Storm spell once per turn for two Storm points", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    auto &crest = put(state, 1, "storm-crest", 0, Zone::Analysis);
    crest.stormPoints = 4;
    auto &spell = put(state, 20, "wind-whisper", 0, Zone::Deck);
    state.players[0].deck.push_back(spell.id);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));
    REQUIRE(hasAbility(engine.viewFor(0), 1, "storm_search"));

    send(engine, 0, ActivateAbility{1, 0, "storm_search"});
    const auto decision = *engine.state().decision;
    REQUIRE(decision.kind == DecisionKind::StormSearch);
    REQUIRE(engine.state().cards.at(1).stormPoints == 2);
    REQUIRE(engine.state().cards.at(1).stormAbilityTurn == 2);
    send(engine, 0, Choose{decision.id, 20});

    REQUIRE(engine.state().cards.at(20).zone == Zone::Hand);
    REQUIRE(engine.state().players[0].deck.empty());
    REQUIRE_FALSE(engine.submit(0, ActivateAbility{1, 0, "storm_search"}).accepted);
}

TEST_CASE("Storm Heart's once-per-turn choice recovers a Storm card to the shuffled deck", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    auto &heart = put(state, 1, "storm-heart", 0, Zone::Analysis);
    heart.stormPoints = 4;
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    put(state, 20, "wind-whisper", 0, Zone::Ash);
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, ActivateAbility{1, 0, "storm_heart_choice"});
    const auto firstDecision = *engine.state().decision;
    REQUIRE(firstDecision.kind == DecisionKind::StormHeartChoice);
    REQUIRE(firstDecision.options == std::vector<std::uint32_t>{1, 2});
    send(engine, 0, Choose{firstDecision.id, 2});
    const auto recoverDecision = *engine.state().decision;
    REQUIRE(recoverDecision.kind == DecisionKind::StormHeartRecover);
    send(engine, 0, Choose{recoverDecision.id, 20});

    REQUIRE(engine.state().cards.at(1).stormPoints == 2);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Deck);
    REQUIRE(std::find(engine.state().players[0].deck.begin(), engine.state().players[0].deck.end(), CardId{20}) !=
            engine.state().players[0].deck.end());
}

TEST_CASE("Storm Heart requires ten Storm points to upgrade and searches Great Storm when set", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto setup = [&](int points) {
        auto state = stateAt(Phase::Main);
        auto &crest = put(state, 1, "storm-crest", 0, Zone::Analysis);
        crest.stormPoints = points;
        put(state, 2, "small-hexagram", 1, Zone::Analysis);
        put(state, 10, "storm-heart", 0, Zone::SideDeck);
        put(state, 20, "great-storm", 0, Zone::Deck);
        state.players[0].deck.push_back(20);
        return state;
    };
    auto insufficient = GameEngine::scenario(content.catalog, setup(9));
    REQUIRE_FALSE(insufficient.submit(0, SetFormation{10, 0, {1}}).accepted);
    REQUIRE(insufficient.state().cards.at(10).zone == Zone::SideDeck);

    auto engine = GameEngine::scenario(content.catalog, setup(10));
    send(engine, 0, SetFormation{10, 0, {1}});

    REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Hand);
    REQUIRE(engine.state().players[0].deck.empty());
}

TEST_CASE("Storm Annihilation Crest destroys an idle enemy formation for two Storm points", "[storm][storm-modifiers]") {
    auto content = stormContent();
    auto state = stateAt(Phase::Main);
    auto &crest = put(state, 1, "storm-crest", 0, Zone::Analysis);
    crest.stormPoints = 3;
    put(state, 2, "small-hexagram", 1, Zone::Analysis);
    auto &seal = put(state, 10, "storm-annihilation-crest", 0, Zone::Attached);
    seal.host = 1;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));
    REQUIRE(hasAbility(engine.viewFor(0), 10, "storm_annihilation_destroy"));
    const ActivateAbility command{10, 2, "storm_annihilation_destroy"};
    REQUIRE(std::get<ActivateAbility>(decodeCommand(encodeCommand(command))).ability == command.ability);

    send(engine, 0, command);
    REQUIRE(engine.state().cards.at(1).stormPoints == 1);
    passResponses(engine);

    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
}

TEST_CASE("Storm Annihilation Crest adds thunder damage to its attached spell", "[storm][storm-modifiers]") {
    auto content = stormContent();
    content.catalog.cards.at("cloud-mist").effects.clear();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "storm-crest", 0, Zone::Analysis);
    auto &spell = put(state, 20, "cloud-mist", 0, Zone::Casting);
    spell.spell = SpellState::Ready;
    spell.sourceFormation = 1;
    auto &seal = put(state, 10, "storm-annihilation-crest", 0, Zone::Attached);
    seal.host = 20;
    state.players[1].forceDamageReduction = 4;
    auto engine = GameEngine::scenario(content.catalog, std::move(state));

    send(engine, 0, ActivateSpell{20});
    passResponses(engine);

    REQUIRE(engine.state().players[1].life == startingLife - 4);
}
