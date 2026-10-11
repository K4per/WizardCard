#include "wizard/application.hpp"
#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <chrono>
#include <set>
using namespace wizard;
namespace {
Content rcContent() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"); }
struct CardScenario {
    Content content = rcContent();
    GameState state;
    CardScenario() {
        state.phase = Phase::Cast;
        state.phaseStep = PhaseStep::Body;
        state.rng = 123;
        state.globalTurn = 5;
        for (int player = 0; player < 2; ++player) {
            state.players[player].mana = 12;
            state.players[player].ownTurn = 3;
            add(1 + player, "hexagram", player, Zone::Analysis);
        }
    }
    CardInstance &add(CardId id, const std::string &name, PlayerId player = 0, Zone zone = Zone::Hand) {
        CardInstance card;
        card.id = id; card.definition = name; card.owner = player; card.zone = zone;
        card.fieldOrder = state.nextFieldOrder++;
        state.cards[id] = card;
        return state.cards.at(id);
    }
    CardInstance &spell(CardId id, const std::string &name, PlayerId player = 0) {
        auto &card = add(id, name, player, Zone::Analysis);
        card.host = card.sourceFormation = 1 + player;
        card.ringIndex = Rules::freeRing(state, content.catalog, card.host);
        card.spell = SpellState::Ready;
        return card;
    }
    GameEngine casting(const std::string &name) {
        auto &card = add(10, name, 0, Zone::Casting);
        card.spell = SpellState::Ready; card.sourceFormation = 1;
        return GameEngine::scenario(content.catalog, state);
    }
};
void send(GameEngine &engine, int actor, const Command &command) {
    const auto result = engine.submit(actor, command);
    INFO(result.error);
    REQUIRE(result.accepted);
}
void choose(GameEngine &engine, CardId option) {
    REQUIRE(engine.state().decision);
    const auto decision = *engine.state().decision;
    send(engine, decision.player, Choose{decision.id, option});
}
}
TEST_CASE("rc content is a numbered executable 46 card pool with legal 40 plus 10 presets", "[rc-content]") {
    const auto content = rcContent();
    REQUIRE(content.catalog.cards.size() == 46);
    REQUIRE(content.catalog.alphaV2);
    REQUIRE(content.catalog.at("reservoir").name == "阿克乌姆之弧");
    REQUIRE(content.catalog.at("reservoir").level == 0);
    std::set<std::string> numbers;
    for (const auto &[id, card] : content.catalog.cards) {
        REQUIRE(card.number.size() == 6);
        REQUIRE(numbers.insert(card.number).second);
        REQUIRE_FALSE(card.illustrationId.empty());
    }
    const auto presets = app::loadPresets(content, std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    REQUIRE(presets.size() == 10);
    for (const auto &preset : presets) {
        INFO(preset.id);
        REQUIRE(preset.deck.cards.size() == 40);
        REQUIRE(preset.deck.sideDeck.size() == 10);
        REQUIRE(Rules::deckErrors(content.catalog, preset.deck).empty());
        MatchConfig config; config.players = {preset.deck, preset.deck};
        REQUIRE_NOTHROW(GameEngine(content.catalog, config));
    }
}
TEST_CASE("rc source formations modify healing damage and sunburst fees", "[rc-content]") {
    CardScenario f;
    f.state.cards.at(1).definition = "elemental-ring";
    f.add(10, "fireball", 0, Zone::Casting).sourceFormation = 1;
    Trigger trigger; trigger.owner = 0; trigger.source = 10; trigger.targetPlayer = 1;
    EffectResolver::apply(f.state, f.content.catalog, trigger, {EffectKind::Damage, 10, DamageType::Fire});
    REQUIRE(f.state.players[1].life == startingLife - 11);
    f.state.cards.at(1).definition = "upright-cross";
    f.state.cards.at(10).definition = "mend";
    f.state.players[0].life = 20; trigger.targetPlayer = 0;
    EffectResolver::apply(f.state, f.content.catalog, trigger, {EffectKind::Heal, 4});
    REQUIRE(f.state.players[0].life == 25);
    f.state.cards.at(10).definition = "sunburst";
    for (CardId id = 20; id < 30; ++id) f.add(id, "fireball", 0, Zone::Ash);
    REQUIRE(Rules::analysisCost(f.state, f.content.catalog, 10, 1) == 10);
    REQUIRE(Rules::castCost(f.state, f.content.catalog, 10) == 0);
}
TEST_CASE("rc chromatic damage chooses type and sleep delays the next hand incantation turn", "[rc-content]") {
    CardScenario f;
    auto engine = f.casting("chromatic-orb");
    send(engine, 0, ActivateSpell{10});
    REQUIRE(engine.state().decision->kind == DecisionKind::DamageChoice);
    choose(engine, static_cast<CardId>(DamageType::Acid) + 1);
    REQUIRE(engine.state().players[1].life == startingLife - 8);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Resolved);
    Trigger trigger; trigger.owner = 0; trigger.targetPlayer = 1;
    EffectResolver::apply(f.state, f.content.catalog, trigger, {EffectKind::BlockActions});
    REQUIRE(f.state.players[1].blockedActionTurn == f.state.players[1].ownTurn + 1);
}
TEST_CASE("rc ray delay conceal and recovery resolve through explicit card choices", "[rc-content]") {
    SECTION("delay") {
        CardScenario f;
        f.spell(20, "fireball", 1).analysisProgress = 1;
        auto engine = f.casting("ray-of-frost");
        send(engine, 0, ActivateSpell{10}); choose(engine, 20);
        REQUIRE(engine.state().cards.at(20).spell == SpellState::Analyzing);
        REQUIRE(engine.state().cards.at(20).analysisProgress == 0);
    }
    SECTION("conceal clears bound load and resets wait") {
        CardScenario f;
        auto &card = f.spell(20, "fireball", 1); card.analysisProgress = 1; card.analysisLoad = 3;
        auto engine = f.casting("lightning-bolt");
        send(engine, 0, ActivateSpell{10}); choose(engine, 20);
        REQUIRE(engine.state().cards.at(20).faceDown);
        REQUIRE(engine.state().cards.at(20).analysisLoad == 0);
        REQUIRE(engine.state().cards.at(20).ambushedTurn == engine.state().globalTurn);
    }
    SECTION("recovery transfers ownership and uses a real ring") {
        CardScenario f;
        f.add(20, "fireball", 1, Zone::Ash);
        auto engine = f.casting("funny-box");
        send(engine, 0, ActivateSpell{10}); choose(engine, 20); choose(engine, 1);
        const auto &card = engine.state().cards.at(20);
        REQUIRE(card.owner == 0); REQUIRE(card.zone == Zone::Analysis);
        REQUIRE(card.faceDown); REQUIRE(card.host == 1); REQUIRE(card.ringIndex == 0);
    }
}
TEST_CASE("rc sunburst respects protected formations and sulfur captures departure once", "[rc-content]") {
    CardScenario f;
    f.state.cards.at(1).definition = "sulfur";
    f.add(10, "fireball", 0, Zone::Casting);
    StateMaintenance::leave(f.state, 10, &f.content.catalog);
    REQUIRE(f.state.queue.items.size() == 1);
    REQUIRE(f.state.queue.items.front().source == 1);
    StateMaintenance::leave(f.state, 10, &f.content.catalog);
    REQUIRE(f.state.queue.items.size() == 1);
    Trigger trigger; trigger.owner = 0; trigger.targetCard = 2;
    EffectResolver::apply(f.state, f.content.catalog, trigger, {EffectKind::Destroy});
    REQUIRE(f.state.cards.at(2).zone == Zone::Analysis);
    f.state.cards.at(2).definition = "ankh";
    StateMaintenance::leave(f.state, 2, &f.content.catalog);
    REQUIRE(f.state.cards.at(2).zone == Zone::Ash);
    REQUIRE(f.state.cards.at(2).faceDown);
}
TEST_CASE("rc tutorial completes using normal 40 plus 10 rules and recorded shield response", "[rc-content]") {
    const auto content = rcContent();
    const auto path = std::filesystem::path(WIZARD_SOURCE_DIR) / "build/rc-tutorial-test";
    app::Application application(content, path);
    std::string error; REQUIRE(application.startTutorial(error));
    REQUIRE(application.configuration().players[0].cards.size() == 40);
    REQUIRE(application.configuration().players[0].sideDeck.size() == 10);
    REQUIRE(application.match().engine().state().first == 0);
    for (int step = 0; application.tutorial()->lesson != app::Lesson::Complete; ++step) {
        INFO(step); INFO(static_cast<int>(application.tutorial()->lesson));
        REQUIRE(step < 250);
        REQUIRE(application.match().engine().state().result == -1);
        auto view = application.match().engine().viewFor(0);
        if (application.tutorial()->canContinue(view)) { application.continueTutorial(); continue; }
        if (application.aiTurn()) {
            const auto plan = application.planAi(); REQUIRE(plan);
            const auto result = application.commitAi(*plan); INFO(result.error); REQUIRE(result.accepted);
            continue;
        }
        view.actions.erase(std::remove_if(view.actions.begin(), view.actions.end(), [&](const LegalAction &a) {
            return !application.tutorial()->allows(view, a.command);
        }), view.actions.end());
        const auto selected = ai::choose(view, ai::Difficulty::Normal); REQUIRE(selected);
        const auto result = application.submit(0, selected->command, application.generation());
        INFO(result.error); REQUIRE(result.accepted);
    }
    REQUIRE(application.tutorial()->countered);
    REQUIRE(application.match().engine().state().players[1].life == startingLife - 5);
    REQUIRE(replay(content, application.match().recording()).digest() == application.match().engine().digest());
}
