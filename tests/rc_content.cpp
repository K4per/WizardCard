#include "support/observed_engine.hpp"
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
    MutationContext state;
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
    test::ObservedEngine casting(const std::string &name) {
        auto &card = add(10, name, 0, Zone::Casting);
        card.spell = SpellState::Ready; card.sourceFormation = 1;
        return test::ObservedEngine::scenario(content.catalog, state);
    }
};
void send(test::ObservedEngine &engine, int actor, const Command &command) {
    const auto result = engine.submit(actor, command);
    INFO(result.error);
    REQUIRE(result.accepted);
}
void choose(test::ObservedEngine &engine, CardId option) {
    REQUIRE(engine.state().decision);
    const auto decision = *engine.state().decision;
    send(engine, decision.player, Choose{decision.id, option});
}
}
TEST_CASE("rc content is a numbered executable 78 card pool with legal 40 plus 10 presets", "[rc-content]") {
    const auto content = rcContent();
    REQUIRE(content.catalog.cards.size() == 78);
    REQUIRE(content.catalog.rulesVersion == "2.5.5");
    REQUIRE(content.catalog.mechanicsRevision == 9);
    REQUIRE(content.catalog.at("reservoir").name == "阿克乌姆之弧");
    REQUIRE(content.catalog.at("reservoir").level == 0);
    REQUIRE_FALSE(content.catalog.cards.count("balance"));
    REQUIRE(content.catalog.at("hexagram").name == "均衡之六芒星");
    REQUIRE(content.catalog.at("hexagram").illustrationId == "balance");
    REQUIRE(content.catalog.at("recall").cost == 3);
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
        REQUIRE_NOTHROW(test::ObservedEngine(content.catalog, config));
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

    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
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
TEST_CASE("preview2 formation bodies cap at five and deduct recipe and replacement bodies", "[preview2][body]") {
    CardScenario f;
    f.state.phase = Phase::Main;
    f.state.phaseStep = PhaseStep::Body;
    f.state.flow = Flow::Main;
    f.state.phaseGate = 1;
    f.add(3, "hexagram", 0, Zone::Analysis);
    f.add(4, "small-hexagram", 0, Zone::Analysis);
    f.add(10, "hexagram", 0, Zone::SideDeck);
    REQUIRE(f.content.catalog.at("hexagram").body == 2);
    REQUIRE(f.content.catalog.at("small-hexagram").body == 1);
    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    REQUIRE(Rules::invariants(engine.state(), f.content.catalog).empty());
    const auto before = engine.digest();
    const auto rejected = engine.submit(0, SetFormation{10, 0, {4}});
    REQUIRE_FALSE(rejected.accepted);
    REQUIRE(rejected.error == "formation_capacity");
    REQUIRE(engine.digest() == before);
    for (const auto &action : engine.viewFor(0).actions)
        if (const auto *setting = std::get_if<SetFormation>(&action.command);
            setting && setting->card == 10)
            REQUIRE(setting->replace != 0);
    send(engine, 0, SetFormation{10, 3, {4}});
    REQUIRE(engine.state().cards.at(3).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(4).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(1).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(Rules::invariants(engine.state(), f.content.catalog).empty());
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
    REQUIRE(Rules::castCost(f.state, f.content.catalog, 10) == 2);
}
TEST_CASE("preview2 sunburst can be set without ash after saving twenty mana", "[preview2][sunburst]") {
    CardScenario f;
    f.state.phase = Phase::Main;
    f.state.cards.at(1).definition = "sun";
    f.add(3, "small-hexagram", 0, Zone::Analysis);
    f.state.players[0].mana = 0;
    f.add(10, "sunburst");
    Trigger income; income.owner = 0;
    EffectResolver::apply(f.state, f.content.catalog, income, {EffectKind::GainMana, 30});
    REQUIRE(f.state.players[0].mana == 20);
    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    const auto result = engine.submit(0, StartAnalysis{10, 1});
    INFO(result.error);
    REQUIRE(result.accepted);
    REQUIRE(engine.state().players[0].mana == 0);
    REQUIRE(engine.state().cards.at(10).settingPaid == 20);
}
TEST_CASE("preview2 sunburst ash discount affects only own evocation spell setting", "[preview2][sunburst]") {
    CardScenario f;
    f.state.cards.at(1).definition = "sun";
    f.add(10, "sunburst");
    f.add(20, "fireball", 0, Zone::Ash);
    f.add(21, "spark", 0, Zone::Ash);
    f.add(22, "fireball", 1, Zone::Ash);
    f.add(23, "mend", 0, Zone::Ash);
    f.add(24, "conduit", 0, Zone::Ash);
    f.add(25, "fireball", 0, Zone::Casting);
    CHECK(Rules::analysisCost(f.state, f.content.catalog, 10, 1) == 18);
    CHECK(Rules::castCost(f.state, f.content.catalog, 10) == 2);
    CHECK(Rules::analysisCost(f.state, f.content.catalog, 10, 0) == 18);
}
TEST_CASE("preview2 healing can choose either player and damage remains opponent only", "[preview2][card-sync]") {
    CardScenario f;
    f.state.players[0].life = f.state.players[1].life = 20;
    auto engine = f.casting("mend");
    const auto initialState = test::mutation(engine.state());
    const auto view = engine.viewFor(0);
    REQUIRE(std::count_if(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *activate = std::get_if<ActivateSpell>(&action.command); return activate && activate->card == 10;
    }) == 2);
    const auto original = engine.digest();
    REQUIRE_FALSE(engine.submit(0, ActivateSpell{10, 1}).accepted);
    REQUIRE(engine.digest() == original);
    send(engine, 0, ActivateSpell{10, playerTarget(1)});
    REQUIRE(engine.state().players[0].life == 20);
    REQUIRE(engine.state().players[1].life == 24);
    auto replayed = test::ObservedEngine::scenario(f.content.catalog, initialState);
    send(replayed, 0, decodeCommand(encodeCommand(ActivateSpell{10, playerTarget(1)})));
    REQUIRE(replayed.digest() == engine.digest());
    engine = f.casting("scorching-ray");
    REQUIRE_FALSE(engine.submit(0, ActivateSpell{10, playerTarget(0)}).accepted);
    send(engine, 0, ActivateSpell{10});
    REQUIRE(engine.state().players[1].life == 5);
}
TEST_CASE("preview2 shield reduces every hit this turn and then expires", "[preview2][card-sync]") {
    CardScenario f;
    auto engine = f.casting("shield");
    send(engine, 0, ActivateSpell{10});
    REQUIRE(engine.state().players[0].mana == 9);
    auto state = test::mutation(engine.state()); Trigger damage; damage.owner = 1; damage.targetPlayer = 0;
    EffectResolver::apply(state, f.content.catalog, damage, {EffectKind::Damage, 8, DamageType::Fire});
    EffectResolver::apply(state, f.content.catalog, damage, {EffectKind::Damage, 8, DamageType::Fire});
    EffectResolver::apply(state, f.content.catalog, damage, {EffectKind::Damage, 12, DamageType::Force});
    REQUIRE(state.players[0].life == 32);
    state.phase = Phase::End; state.phaseStep = PhaseStep::Body; state.phaseGate = 1;
    engine = test::ObservedEngine::scenario(f.content.catalog, state);
    send(engine, 0, AdvancePhase{engine.state().phaseGate});
    REQUIRE(engine.state().players[0].damageReduction == 0);
    REQUIRE(engine.state().players[0].forceDamageReduction == 0);
}
TEST_CASE("preview2 talismans set for free and cannot activate before waiting", "[preview2][card-sync]") {
    for (const auto &name : {"barbs", "shield", "counter-spell", "protective-flame", "funny-box"}) {
        CardScenario f; f.state.phase = Phase::Main; f.state.cards.at(1).definition = "sun";
        f.add(10, name); auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
        REQUIRE(f.content.catalog.at(name).type == CardType::Talisman);
        REQUIRE_FALSE(engine.submit(0, PreloadWord{10, true, 1}).accepted);
        send(engine, 0, SetAmbush{10, 1});
        REQUIRE(engine.state().players[0].mana == 12);
        REQUIRE(Rules::load(engine.state(), 0) == 0);
        REQUIRE_FALSE(engine.submit(0, FlipAmbush{10}).accepted);
    }
}
TEST_CASE("preview2 true strike pays a hand spell before any response and rejects missing cost", "[preview2][card-sync]") {
    CardScenario f; f.state.phase = Phase::Main;
    f.add(10, "true-strike"); f.add(20, "fireball");
    f.add(21, "magic-missile", 0, Zone::Deck); f.state.players[0].deck.push_back(21);
    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    const auto original = engine.digest();
    REQUIRE_FALSE(engine.submit(0, PreloadWord{10, true, 1}).accepted);
    REQUIRE(engine.digest() == original);
    const auto command = PreloadWord{10, true, 1, 0, {}, 20};
    send(engine, 0, decodeCommand(encodeCommand(command)));
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().players[0].mana == 9);
    REQUIRE(engine.state().decision->options == std::vector<ChoiceValue>{ChoiceValue::card(21)});
    choose(engine, 21);
    REQUIRE(engine.state().cards.at(21).zone == Zone::Hand);
}
TEST_CASE("preview2 sun inspection preserves concealed cards and only owner sees identities", "[preview2][card-sync]") {
    CardScenario f; f.state.cards.at(1).definition = "sun";
    auto &hidden = f.spell(20, "fireball", 1); hidden.faceDown = true;
    hidden.wasAmbushed = true; hidden.ambushedTurn = 1;
    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    const auto view = engine.viewFor(0);
    auto card = std::find_if(view.cards.begin(), view.cards.end(), [](const CardView &value) { return value.instance.id == 20; });
    REQUIRE(card != view.cards.end()); REQUIRE_FALSE(card->hidden); REQUIRE(card->instance.faceDown);
    REQUIRE(card->definition.id == "fireball");
    StateMaintenance::leave(f.state, 1, &f.content.catalog);
    engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    const auto concealedView = engine.viewFor(0);
    card = std::find_if(concealedView.cards.begin(), concealedView.cards.end(), [](const CardView &value) { return value.instance.id == 20; });
    REQUIRE(card->hidden); REQUIRE(card->definition.id.empty());
}
TEST_CASE("preview2 healing circle heals at its owners end phase before timed expiry", "[preview2][card-sync]") {
    CardScenario f; f.state.phase = Phase::Cast; f.state.players[0].life = 20;
    auto &circle = f.spell(10, "healing-circle"); circle.spell = SpellState::Active; circle.remaining = 1;
    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    send(engine, 0, AdvancePhase{engine.state().phaseGate});
    REQUIRE(engine.state().phase == Phase::End);
    REQUIRE(engine.state().players[0].life == 22);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
    send(engine, 0, AdvancePhase{engine.state().phaseGate});
    REQUIRE(engine.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("preview2 ward deals maintenance damage only after focus is paid", "[preview2][card-sync]") {
    for (bool maintain : {false, true}) {
        CardScenario f; f.state.phase = Phase::Draw;
        auto &ward = f.spell(10, "ward"); ward.spell = SpellState::Active; ward.concentrationCost = 1; ward.targetPlayer = 1;
        auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
        send(engine, 0, AdvancePhase{engine.state().phaseGate});
        REQUIRE(engine.state().decision); REQUIRE(engine.state().decision->kind == DecisionKind::Concentration);
        choose(engine, maintain ? 10 : 0);
        REQUIRE(engine.state().players[1].life == startingLife - (maintain ? 2 : 0));
        REQUIRE(engine.state().players[0].mana == 12 - (maintain ? 1 : 0));
    }
}
TEST_CASE("preview2 counterspell load uses printed setting cost rather than cast fee", "[preview2][card-sync]") {
    CardScenario f;
    auto &fireball = f.add(10, "fireball", 1, Zone::Casting); fireball.spell = SpellState::Pending;
    ChainLink spell; spell.id = 1; spell.kind = LinkKind::Spell; spell.item.source = 10; spell.castCost = 1;
    f.state.chain = ChainState{}; f.state.chain->links.push_back(spell);
    Trigger counter; counter.owner = 0; counter.targetLink = 1;
    EffectResolver::apply(f.state, f.content.catalog, counter, {EffectKind::CounterSpell});
    REQUIRE(Rules::load(f.state, 0) == 3);
    REQUIRE(f.state.cards.at(10).zone == Zone::Ash);
    REQUIRE(f.state.chain->links.front().canceled);
    CardScenario talisman;
    auto &shield = talisman.add(10, "shield", 1, Zone::Casting); shield.spell = SpellState::Pending;
    talisman.state.chain = ChainState{}; talisman.state.chain->links.push_back(spell);
    EffectResolver::apply(talisman.state, talisman.content.catalog, counter, {EffectKind::CounterSpell});
    REQUIRE(Rules::load(talisman.state, 0) == 0);
}
TEST_CASE("preview2 funny box selects an eligible ash spell before paying and includes level five", "[preview2][card-sync]") {
    CardScenario f; f.state.cards.at(1).definition = "sun";
    f.add(20, "scorching-ray", 1, Zone::Ash); f.add(21, "sunburst", 1, Zone::Ash);
    auto engine = f.casting("funny-box");
    const auto original = engine.digest();
    REQUIRE_FALSE(engine.submit(0, ActivateSpell{10}).accepted);
    REQUIRE_FALSE(engine.submit(0, ActivateSpell{10, 21}).accepted);
    REQUIRE(engine.digest() == original);
    send(engine, 0, ActivateSpell{10, 20}); choose(engine, 1);
    REQUIRE(engine.state().cards.at(20).owner == 1);
    REQUIRE(engine.state().cards.at(20).controller == 0);
    REQUIRE(engine.state().cards.at(20).faceDown);
}
TEST_CASE("preview2 sun ignores attached rune modifiers including income", "[preview2][card-sync]") {
    CardScenario f; f.state.cards.at(1).definition = "sun";
    auto &seal = f.add(10, "uplift", 0, Zone::Attached); seal.host = 1;
    // Give the test rune every modifier to prove the immunity at each boundary.
    auto &definition = f.content.catalog.cards.at("uplift");
    definition.capacityBonus = 9; definition.ringBonus = 2; definition.rankBonus = 3; definition.incomeBonus = 4;
    REQUIRE(Rules::capacity(f.state, f.content.catalog, 0) == 17);
    REQUIRE(Rules::rings(f.state, f.content.catalog, 1) == 7);
    REQUIRE(Rules::maxRank(f.state, f.content.catalog, 1) == 7);
    f.state.phase = Phase::End; f.state.active = 1; f.state.players[0].mana = 0;
    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    send(engine, 1, AdvancePhase{engine.state().phaseGate});
    REQUIRE(engine.state().active == 0); REQUIRE(engine.state().players[0].mana == 8);
}
TEST_CASE("preview2 guiding bolt shows the full concealed face and concealment resets it", "[preview2][card-sync]") {
    CardScenario f;
    auto &hidden = f.spell(20, "fireball", 1); hidden.faceDown = true; hidden.wasAmbushed = true;
    auto engine = f.casting("guiding-bolt");
    send(engine, 0, ActivateSpell{10}); choose(engine, 20);
    REQUIRE(engine.state().cards.at(20).faceDown); REQUIRE(engine.state().cards.at(20).publiclyRevealed);
    const auto view = engine.viewFor(0);
    const auto face = std::find_if(view.cards.begin(), view.cards.end(), [](const CardView &card) { return card.instance.id == 20; });
    REQUIRE(face != view.cards.end()); REQUIRE_FALSE(face->hidden); REQUIRE(face->definition.id == "fireball");
    auto state = test::mutation(engine.state()); state.cards.at(20).faceDown = false;
    Trigger conceal; conceal.owner = 0; conceal.targetCard = 20;
    EffectResolver::apply(state, f.content.catalog, conceal, {EffectKind::Conceal});
    REQUIRE_FALSE(state.cards.at(20).publiclyRevealed);
    engine = test::ObservedEngine::scenario(f.content.catalog, state);
    const auto concealed = engine.viewFor(0);
    const auto back = std::find_if(concealed.cards.begin(), concealed.cards.end(), [](const CardView &card) { return card.instance.id == 20; });
    REQUIRE(back->hidden);
    state.cards.at(20).publiclyRevealed = true;
    const auto visibleDigest = test::ObservedEngine::scenario(f.content.catalog, state).digest();
    state.cards.at(20).publiclyRevealed = false;
    REQUIRE(test::ObservedEngine::scenario(f.content.catalog, state).digest() != visibleDigest);
    state.cards.at(20).publiclyRevealed = true;
    StateMaintenance::leave(state, 20, &f.content.catalog);
    REQUIRE_FALSE(state.cards.at(20).publiclyRevealed);
}
TEST_CASE("preview2 frost delay rolls ready spells back and clamps zero progress", "[preview2][card-sync]") {
    for (int progress : {0, 1}) {
        CardScenario f; auto &target = f.spell(20, "fireball", 1); target.analysisProgress = progress;
        target.spell = progress ? SpellState::Ready : SpellState::Analyzing;
        auto engine = f.casting("ray-of-frost"); send(engine, 0, ActivateSpell{10}); choose(engine, 20);
        REQUIRE(engine.state().cards.at(20).analysisProgress == 0);
        REQUIRE(engine.state().cards.at(20).spell == SpellState::Analyzing);
    }
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
    SECTION("lightning destroys an analytic spell") {
        CardScenario f;
        auto &card = f.spell(20, "fireball", 1); card.analysisProgress = 1; card.analysisLoad = 3;
        auto engine = f.casting("lightning-bolt");
        send(engine, 0, ActivateSpell{10}); choose(engine, 20);
        REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
        REQUIRE(engine.state().cards.at(20).analysisLoad == 0);
    }
    SECTION("recovery transfers ownership and uses a real ring") {
        CardScenario f;
        f.add(20, "fireball", 1, Zone::Ash);
        auto engine = f.casting("funny-box");
        send(engine, 0, ActivateSpell{10, 20}); choose(engine, 1);
        const auto &card = engine.state().cards.at(20);
        REQUIRE(card.owner == 1); REQUIRE(card.controller == 0); REQUIRE(card.zone == Zone::Analysis);
        REQUIRE(card.faceDown); REQUIRE(card.host == 1); REQUIRE(card.ringIndex == 0);
    }
}
TEST_CASE("storm wind whisper searches only storm talismans of rank two or less", "[rc-content][storm]") {
    CardScenario f;
    f.state.cards.at(1).definition = "storm-crest";
    f.add(20, "storm-vortex", 0, Zone::Deck);
    f.add(21, "wind-strike", 0, Zone::Deck);
    f.add(22, "great-cyclone", 0, Zone::Deck);
    f.add(23, "shield", 0, Zone::Deck);
    f.add(24, "storm-guidance", 0, Zone::Deck);
    f.content.catalog.cards.at("shield").rank = 2;
    f.state.players[0].deck = {20, 21, 22, 23, 24};

    auto engine = f.casting("wind-whisper");
    send(engine, 0, ActivateSpell{10});

    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::SearchDeck);
    REQUIRE(engine.state().decision->options == std::vector<ChoiceValue>{ChoiceValue::card(20)});
    const auto view = engine.viewFor(0);
    REQUIRE(view.decision);
    REQUIRE(view.decision->options == std::vector<ChoiceValue>{ChoiceValue::card(20)});
    choose(engine, 20);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Hand);
    REQUIRE(engine.state().cards.at(21).zone == Zone::Deck);
    REQUIRE(engine.state().cards.at(22).zone == Zone::Deck);
    REQUIRE(engine.state().cards.at(23).zone == Zone::Deck);
    REQUIRE(engine.state().cards.at(24).zone == Zone::Deck);
}
TEST_CASE("storm swift wind returns only a low-level enemy formation to its side deck", "[rc-content][storm]") {
    CardScenario f;
    f.state.cards.at(1).definition = "storm-crest";
    f.add(20, "storm-crest", 1, Zone::Analysis);
    f.add(21, "small-hexagram", 1, Zone::Analysis);
    auto &attachedSpell = f.spell(23, "fireball", 1);
    attachedSpell.host = attachedSpell.sourceFormation = 20;
    attachedSpell.ringIndex = Rules::freeRing(f.state, f.content.catalog, 20);

    auto engine = f.casting("swift-wind");
    send(engine, 0, ActivateSpell{10});

    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(engine.state().decision->options == std::vector<ChoiceValue>{ChoiceValue::card(20), ChoiceValue::card(21)});
    choose(engine, 20);
    const auto &returned = engine.state().cards.at(20);
    REQUIRE(returned.owner == 1);
    REQUIRE(returned.zone == Zone::SideDeck);
    REQUIRE_FALSE(returned.base);
    REQUIRE(engine.state().cards.at(21).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(2).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(23).zone == Zone::Ash);
    const auto opponentView = engine.viewFor(1);
    const auto returnedView = std::find_if(opponentView.cards.begin(), opponentView.cards.end(), [](const CardView &card) {
        return card.instance.id == 20;
    });
    REQUIRE(returnedView != opponentView.cards.end());
    REQUIRE(returnedView->instance.zone == Zone::SideDeck);
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
    REQUIRE(engine.state().decision->options == std::vector<ChoiceValue>{ChoiceValue::card(20)});
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
    SECTION("healing permits a general shield response") {
        auto engine = f.casting("mend");
        send(engine, 0, ActivateSpell{10, playerTarget(0)});
        REQUIRE(engine.state().decision);
        const auto view = engine.viewFor(1);
        REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
            const auto *response = std::get_if<Respond>(&action.command); return response && response->card == 20;
        }));
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
        f.add(20, "fireball", 0, Zone::Hand);
        f.add(21, "magic-missile", 0, Zone::Deck); f.state.players[0].deck.push_back(21);
        auto engine = f.casting("true-strike");
        send(engine, 0, ActivateSpell{10, 0, {}, 20});
        REQUIRE(engine.state().decision);
        REQUIRE_FALSE(engine.state().decision->mayPass);
        REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
        choose(engine, 21);
        REQUIRE(engine.state().cards.at(21).zone == Zone::Hand);
    }
    SECTION("enemy ash recovery") {
        CardScenario f;
        f.add(20, "fireball", 1, Zone::Ash);
        auto engine = f.casting("funny-box");
        send(engine, 0, ActivateSpell{10, 20});
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
    auto engine = test::ObservedEngine::scenario(f.content.catalog, f.state);
    const auto before = engine.digest();
    REQUIRE_FALSE(engine.submit(0, PreloadWord{10, true, 1}).accepted);
    REQUIRE(engine.digest() == before);
    send(engine, 0, FlipAmbush{20});
    REQUIRE(engine.state().players[1].life == startingLife - 3);
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
        auto view = test::view(application.match().engine(),0);
        if (application.tutorial()->canContinue(view)) { application.continueTutorial(); continue; }
        if (application.aiTurn()) {
            const auto plan = application.planAi(); REQUIRE(plan);
            const auto result = application.commitAi(*plan); INFO(result.error); REQUIRE(result.accepted);
            continue;
        }
        view.actions.erase(std::remove_if(view.actions.begin(), view.actions.end(), [&](const LegalAction &a) {
            return !application.tutorial()->allows(view, a.command, [&](const auto &visitor) { application.match().engine().visitLegalActions(0,visitor); });
        }), view.actions.end());
        const auto selected = test::choose(view, ai::Difficulty::Normal); REQUIRE(selected);
        const auto result = application.submit(0, selected->command, application.generation());
        INFO(result.error); REQUIRE(result.accepted);
    }
    REQUIRE(application.tutorial()->countered);
    REQUIRE_FALSE(application.match().engine().state().opening);
    REQUIRE(application.match().engine().state().first == 0);
    REQUIRE(application.match().engine().state().players[1].life == startingLife - 10);
    REQUIRE(replay(content, application.match().recording()).digest() == application.match().engine().digest());
}
