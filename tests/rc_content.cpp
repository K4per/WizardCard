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
TEST_CASE("rc content is a numbered executable 45 card pool with legal 40 plus 10 presets", "[rc-content]") {
    const auto content = rcContent();
    REQUIRE(content.catalog.cards.size() == 45);
    REQUIRE(content.catalog.alphaV2);
    REQUIRE(content.catalog.usesAlphaV25());
    REQUIRE(content.catalog.at("reservoir").name == "阿克乌姆之弧");
    REQUIRE(content.catalog.at("reservoir").level == 0);
    REQUIRE_FALSE(content.catalog.cards.count("balance"));
    REQUIRE(content.catalog.at("hexagram").name == "均衡之六芒星");
    REQUIRE(content.catalog.at("hexagram").illustrationId == "balance");
    REQUIRE(content.catalog.at("recall").cost == 2);
    REQUIRE(content.catalog.at("conduit").cost == 2);
    REQUIRE(content.catalog.at("engeas-four-point-star").income == 3);
    std::set<std::string> numbers;
    for (const auto &[id, card] : content.catalog.cards) {
        REQUIRE(card.number.size() == 6);
        REQUIRE(numbers.insert(card.number).second);
        REQUIRE_FALSE(card.illustrationId.empty());
        const std::string prefix = card.type == CardType::Formation ? "01" :
            card.type == CardType::Analytic ? "02" :
            card.type == CardType::Word ? "03" :
            card.type == CardType::Talisman ? "04" : "05";
        REQUIRE(card.number.substr(0, 2) == prefix);
    }
    std::set<std::string> illustrations;
    for (const auto &[id, card] : content.catalog.cards) {
        INFO(id);
        REQUIRE(illustrations.insert(card.illustrationId).second);
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
TEST_CASE("rc pentagram candidates consume exactly one hexagram and one ready spell", "[rc-content]") {
    CardScenario f;
    f.state.phase = Phase::Main;
    f.state.phaseStep = PhaseStep::Body;
    f.state.flow = Flow::Main;
    f.state.phaseGate = 1;
    f.state.cards.at(1).definition = "small-hexagram";
    f.state.cards.at(1).base = true;
    f.add(3, "hexagram", 0, Zone::Analysis);
    f.add(4, "hexagram", 0, Zone::Analysis);
    auto &ready = f.spell(5, "fireball");
    ready.host = ready.sourceFormation = 3;
    ready.ringIndex = Rules::freeRing(f.state, f.content.catalog, 3);
    f.add(10, "pentagram-of-life", 0, Zone::SideDeck);

    auto engine = GameEngine::scenario(f.content.catalog, f.state);
    std::vector<SetFormation> candidates;
    for (const auto &action : engine.viewFor(0).actions)
        if (const auto *command = std::get_if<SetFormation>(&action.command);
            command && command->card == 10 && command->replace == 0)
            candidates.push_back(*command);

    REQUIRE(candidates.size() == 2);
    for (const auto &candidate : candidates) {
        REQUIRE(candidate.materials.size() == 2);
        REQUIRE(std::count(candidate.materials.begin(), candidate.materials.end(), CardId{3}) +
                std::count(candidate.materials.begin(), candidate.materials.end(), CardId{4}) == 1);
        REQUIRE(std::find(candidate.materials.begin(), candidate.materials.end(), CardId{5}) != candidate.materials.end());
    }

    const auto result = engine.submit(0, SetFormation{10, 0, {3, 5}});
    INFO(result.error);
    REQUIRE(result.accepted);
    REQUIRE(engine.state().cards.at(3).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(5).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(4).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
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
TEST_CASE("printed uplift formation bonus adds a ring rather than load capacity", "[card-sync]") {
    CardScenario f;
    const auto capacity = Rules::capacity(f.state, f.content.catalog, 0);
    const auto rings = Rules::rings(f.state, f.content.catalog, 1);
    auto &seal = f.add(10, "uplift", 0, Zone::Attached); seal.host = 1;
    REQUIRE(Rules::rings(f.state, f.content.catalog, 1) == rings + 1);
    REQUIRE(Rules::capacity(f.state, f.content.catalog, 0) == capacity);
    REQUIRE(Rules::maxRank(f.state, f.content.catalog, 1) == 4);
}
TEST_CASE("magic missile destroys only opponent face down spells in analysis", "[card-sync]") {
    CardScenario f;
    f.spell(20, "instantly-sleep", 1).faceDown = true;
    f.add(21, "ankh", 1, Zone::Analysis).faceDown = true;
    f.add(22, "ankh", 1, Zone::Ash).faceDown = true;
    auto engine = f.casting("magic-missile");
    send(engine, 0, ActivateSpell{10});
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::DestroyAmbush);
    REQUIRE(engine.state().decision->options == std::vector<CardId>{20});
    choose(engine, 20);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(21).zone == Zone::Analysis);
}
TEST_CASE("shield reduces chromatic orb chosen damage and cannot target a healing spell", "[card-sync]") {
    CardScenario f;
    auto &shield = f.spell(20, "shield", 1);
    shield.faceDown = shield.wasAmbushed = true; shield.ambushedTurn = 4;
    SECTION("chosen damage") {
        auto engine = f.casting("chromatic-orb");
        send(engine, 0, ActivateSpell{10});
        const auto view = engine.viewFor(1);
        const auto response = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
            const auto *command = std::get_if<Respond>(&action.command);
            return command && command->card == 20;
        });
        REQUIRE(response != view.actions.end());
        send(engine, 1, response->command);
        REQUIRE(engine.state().decision->kind == DecisionKind::DamageChoice);
        choose(engine, static_cast<CardId>(DamageType::Fire) + 1);
        REQUIRE(engine.state().players[1].life == startingLife - 3);
    }
    SECTION("healing") {
        auto engine = f.casting("mend");
        send(engine, 0, ActivateSpell{10});
        REQUIRE_FALSE(engine.state().decision);
        REQUIRE(engine.state().cards.at(20).faceDown);
    }
}
TEST_CASE("accelerated analysis triggers octagram load removal", "[card-sync]") {
    CardScenario f;
    f.add(3, "octagram", 0, Zone::Analysis);
    auto &target = f.spell(20, "fireball");
    target.spell = SpellState::Analyzing; target.settingPaid = target.analysisLoad = 3;
    auto engine = f.casting("instant-circuit-overload");
    send(engine, 0, ActivateSpell{10, 20});
    REQUIRE(engine.state().cards.at(20).spell == SpellState::Ready);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::ClearAnyLoad);
    const auto load = Rules::load(engine.state(), 0);
    choose(engine, 20);
    REQUIRE(Rules::load(engine.state(), 0) == load - 1);
}
TEST_CASE("mandatory printed spell choices do not offer decline", "[card-sync]") {
    SECTION("same school search") {
        CardScenario f;
        f.spell(20, "fireball");
        f.add(21, "magic-missile", 0, Zone::Deck); f.state.players[0].deck.push_back(21);
        auto engine = f.casting("true-strike");
        send(engine, 0, ActivateSpell{10});
        REQUIRE(engine.state().decision);
        REQUIRE_FALSE(engine.state().decision->mayPass);
        choose(engine, 20); choose(engine, 21);
        REQUIRE(engine.state().cards.at(21).zone == Zone::Hand);
    }
    SECTION("enemy ash recovery") {
        CardScenario f;
        f.add(20, "fireball", 1, Zone::Ash);
        auto engine = f.casting("funny-box");
        send(engine, 0, ActivateSpell{10});
        REQUIRE(engine.state().decision);
        REQUIRE_FALSE(engine.state().decision->mayPass);
    }
}
TEST_CASE("sleep blocks next turn hand incantations while permitting an older ambush", "[card-sync]") {
    CardScenario f;
    f.state.phase = Phase::Main;
    Trigger trigger; trigger.owner = 1; trigger.targetPlayer = 0;
    EffectResolver::apply(f.state, f.content.catalog, trigger, {EffectKind::BlockActions});
    ++f.state.players[0].ownTurn;
    f.add(10, "spark");
    auto &ambush = f.spell(20, "spark");
    ambush.faceDown = ambush.wasAmbushed = true; ambush.ambushedTurn = 4;
    auto engine = GameEngine::scenario(f.content.catalog, f.state);
    const auto before = engine.digest();
    REQUIRE_FALSE(engine.submit(0, PreloadWord{10, true, 1}).accepted);
    REQUIRE(engine.digest() == before);
    send(engine, 0, FlipAmbush{20});
    REQUIRE(engine.state().players[1].life == startingLife - 2);
}
TEST_CASE("rc tutorial completes using normal 40 plus 10 rules and recorded shield response", "[rc-content]") {
    const auto content = rcContent();
    const auto path = std::filesystem::path(WIZARD_SOURCE_DIR) / "build/rc-tutorial-test";
    app::Application application(content, path);
    std::string error; REQUIRE(application.startTutorial(error));
    REQUIRE(application.configuration().players[0].cards.size() == 40);
    REQUIRE(application.configuration().players[0].sideDeck.size() == 10);
    REQUIRE(application.match().engine().state().opening);
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
    REQUIRE_FALSE(application.match().engine().state().opening);
    REQUIRE(application.match().engine().state().first == 0);
    REQUIRE(application.match().engine().state().players[1].life == startingLife - 5);
    REQUIRE(replay(content, application.match().recording()).digest() == application.match().engine().digest());
}
