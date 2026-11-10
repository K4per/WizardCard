#include "support/observed_engine.hpp"
#include "wizard/content.hpp"
#include "wizard/replay.hpp"
#include "wizard/bridge.hpp"
#include "wizard/core/spell_rules.hpp"
#include "wizard/core/action_query.hpp"
#include "wizard/core/card_rules.hpp"
#include <catch2/catch_test_macros.hpp>
using namespace wizard;
namespace {
CardCatalog currentCatalog() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260").catalog;
}
MutationContext mainState263() {
    MutationContext s;
    s.phase = Phase::Main; s.phaseStep = PhaseStep::Body; s.flow = Flow::Main;
    s.phaseGate = 1; s.globalTurn = 5; s.rng = 73;
    for (auto &p : s.players) { p.mana = 20; p.ownTurn = 2; }
    return s;
}
CardInstance &put263(MutationContext &s, CardId id, const char *definition, Zone zone, PlayerId owner = 0) {
    CardInstance c;
    c.id = id; c.definition = definition; c.zone = zone; c.owner = owner;
    if (zone == Zone::Analysis) c.fieldOrder = s.nextFieldOrder++;
    s.cards[id] = c;
    if (zone == Zone::Deck) s.players[owner].deck.push_back(id);
    return s.cards.at(id);
}
void pass263(test::ObservedEngine &engine) {
    for (int n = 0; n < 60 && engine.state().decision && engine.state().decision->kind == DecisionKind::Response; ++n) {
        const auto decision = *engine.state().decision;
        REQUIRE(engine.submit(decision.player, PassResponse{decision.id}).accepted);
    }
}
}

TEST_CASE("v263 reinforced recipe accepts controlled facedown spells without waiting and rejects faceup materials", "[alpha-v263]") {
    auto cat = currentCatalog();
    REQUIRE(cat.mechanicsRevision == 12);
    REQUIRE(cat.cards.size() == 90);
    for (const auto *definition : {"v260-analytic-0001", "v260-word-0001", "v260-talisman-0001"}) {
        for (bool borrowed : {false, true}) {
            INFO(definition); INFO(borrowed);
            auto s = mainState263();
            put263(s, 1, "v260-formation-0010", Zone::Analysis);
            put263(s, 2, "v260-formation-0011", Zone::SideDeck);
            auto &material = put263(s, 3, definition, Zone::Analysis, borrowed ? 1 : 0);
            material.controller = 0; material.faceDown = true;
            material.host = 1; material.ringIndex = 0; material.ambushedTurn = s.globalTurn;
            const SetFormation command{2, 0, {1, 3}};
            auto engine = test::ObservedEngine::scenario(cat, s);
            bool offered = false;
            engine.visitLegalActions(0, [&](const LegalAction &action) {
                if (const auto *formation = std::get_if<SetFormation>(&action.command))
                    offered |= formation->card == 2 && formation->materials.size() == 2;
                return true;
            });
            REQUIRE(offered);
            auto result = engine.submit(0, command); INFO(result.error);
            REQUIRE(result.accepted);
            CHECK(engine.state().cards.at(3).zone == Zone::Ash);
            CHECK(engine.state().cards.at(2).zone == Zone::Analysis);
            CHECK(engine.state().players[0].mana == 20);
            CHECK_FALSE(engine.submit(0, command).accepted);

            s.cards.at(3).faceDown = false;
            auto invalid = test::ObservedEngine::scenario(cat, s);
            const auto before = invalid.digest();
            CHECK_FALSE(invalid.submit(0, command).accepted);
            CHECK(invalid.digest() == before);
            s.cards.at(3).faceDown = true; s.cards.at(3).controller = 1;
            put263(s, 4, "v260-formation-0010", Zone::Analysis, 1);
            s.cards.at(3).host = 4;
            auto enemy = test::ObservedEngine::scenario(cat, s);
            CHECK_FALSE(enemy.submit(0, command).accepted);
        }
    }
}

TEST_CASE("v263 setting commands query and honor the chosen ring and reject omitted occupied or invalid rings", "[alpha-v263][ring]") {
    const auto cat = currentCatalog();
    for (const int operation : {2, 15, 5}) {
        INFO(operation);
        auto s = mainState263();
        put263(s, 1, "v260-formation-0017", Zone::Analysis);
        put263(s, 2, operation == 5 ? "v260-word-0001" : "v260-analytic-0001", Zone::Hand);
        auto &occupied = put263(s, 3, "v260-analytic-0001", Zone::Analysis);
        occupied.host = 1; occupied.ringIndex = 0; occupied.faceDown = true;
        auto engine = test::ObservedEngine::scenario(cat, s);
        ActionIntent intent;
        intent.selections = {{ActionField::Operation, std::to_string(operation)},
            {ActionField::Source, "2"}, {ActionField::Host, "1"}};
        const auto rings = engine.queryActions(0, intent);
        REQUIRE(rings.field == ActionField::Ring);
        REQUIRE(std::none_of(rings.options.begin(), rings.options.end(),
            [](const ActionOption &option) { return option.value == "0"; }));
        Json command = {{"type", operation}, {"card", 2}, {"formation", 1}, {"release", true},
            {"target", operation == 5 ? playerTarget(1) : 0}, {"ringIndex", 2}};
        for (int invalid : {-1, 0, 99}) {
            auto bad = command;
            if (invalid < 0) bad.erase("ringIndex"); else bad["ringIndex"] = invalid;
            const auto before = engine.digest();
            CHECK_FALSE(engine.submit(0, decodeCommand(bad)).accepted);
            CHECK(engine.digest() == before);
        }
        const auto result = engine.submit(0, decodeCommand(command)); INFO(result.error);
        REQUIRE(result.accepted);
        CHECK(engine.state().cards.at(2).sourceRingIndex == 2);
        CHECK(encodeCommand(decodeCommand(command)).at("ringIndex") == 2);
    }
}

TEST_CASE("v263 Engeas accelerates once per global turn without adding load", "[alpha-v263][formation]") {
    const auto cat = currentCatalog();
    auto s = mainState263();
    put263(s, 1, "v260-formation-0004", Zone::Analysis);
    put263(s, 2, "v260-analytic-0004", Zone::Hand);
    put263(s, 3, "v260-analytic-0004", Zone::Hand);
    auto engine = test::ObservedEngine::scenario(cat, s);
    REQUIRE(engine.submit(0, StartAnalysis{2, 1, 0, {}, 0}).accepted);
    CHECK(engine.state().cards.at(2).spell == SpellState::Ready);
    CHECK(Rules::load(engine.state(), 0) == 2);
    pass263(engine);
    REQUIRE(engine.submit(0, PrepareCast{2}).accepted);
    pass263(engine);
    REQUIRE(engine.submit(0, StartAnalysis{3, 1, 0, {}, 0}).accepted);
    CHECK(engine.state().cards.at(3).spell == SpellState::Analyzing);
    CHECK(engine.state().cards.at(3).analysisProgress == 0);
    CHECK(Rules::load(engine.state(), 0) == 4);
}

TEST_CASE("v263 earth eruption halves summed casting capacity with independent owner preparation expiries", "[alpha-v263][capacity]") {
    auto cat = currentCatalog();
    cat.cards.at("v260-formation-0017").rings = 7;
    auto s = mainState263();
    put263(s, 1, "v260-formation-0017", Zone::Analysis, 1);
    Trigger trigger; trigger.owner = 0;
    const auto effect = cat.at("v260-analytic-0037").effects.back();
    CHECK(Rules::castingCapacity(s, cat, 1) == 7);
    EffectResolver::apply(s, cat, trigger, effect);
    CHECK(Rules::castingCapacity(s, cat, 1) == 3);
    ++s.players[0].ownTurn;
    EffectResolver::apply(s, cat, trigger, effect);
    CHECK(Rules::castingCapacity(s, cat, 1) == 1);
    s.phase = Phase::Prepare; s.active = 0;
    spell_rules::expire(s, cat, false);
    CHECK(Rules::castingCapacity(s, cat, 1) == 3);
    s.active = 1; ++s.players[1].ownTurn;
    spell_rules::expire(s, cat, false);
    CHECK(Rules::castingCapacity(s, cat, 1) == 3);
    s.active = 0; ++s.players[0].ownTurn;
    spell_rules::expire(s, cat, false);
    CHECK(Rules::castingCapacity(s, cat, 1) == 7);
}

TEST_CASE("v263 casting overflow waits for the complete chain and persistent returns and blocks new actions", "[alpha-v263][capacity]") {
    auto cat = currentCatalog();
    auto &eruption = cat.cards.at("v260-analytic-0037");
    eruption.effects = {eruption.effects.back(), {EffectKind::GainMana, 1, DamageType::Force, EffectRecipient::Owner}};
    auto s = mainState263(); s.phase = Phase::Cast;
    put263(s, 1, "v260-formation-0017", Zone::Analysis);
    put263(s, 2, "v260-formation-0010", Zone::Analysis, 1);
    put263(s, 10, "v260-analytic-0037", Zone::Casting).spell = SpellState::Ready;
    put263(s, 20, "v260-analytic-0004", Zone::Casting, 1).spell = SpellState::Resolved;
    put263(s, 21, "v260-analytic-0004", Zone::Casting, 1).spell = SpellState::Resolved;
    SECTION("controller chooses the excess after all effects") {
        auto engine = test::ObservedEngine::scenario(cat, s);
        REQUIRE(engine.submit(0, ActivateSpell{10}).accepted); pass263(engine);
        REQUIRE(engine.state().decision);
        const auto decision = *engine.state().decision;
        CHECK(decision.kind == DecisionKind::Overflow);
        CHECK(decision.player == 1);
        CHECK_FALSE(decision.mayPass);
        CHECK(engine.state().players[0].mana == 18);
        CHECK_FALSE(engine.state().chain);
        CHECK_FALSE(engine.state().effect);
        CHECK_FALSE(engine.submit(0, AdvancePhase{engine.state().phaseGate}).accepted);
        CHECK_FALSE(engine.submit(1, PassResponse{decision.id}).accepted);
        REQUIRE(engine.submit(1, Choose{decision.id, ChoiceValue::card(20)}).accepted);
        CHECK(engine.state().cards.at(20).zone == Zone::Ash);
        CHECK(engine.state().cards.at(21).zone == Zone::Casting);
        CHECK(engine.state().cards.at(10).zone == Zone::Casting);
    }
    SECTION("a persistent spell returns before capacity cleanup") {
        cat.cards.at("v260-analytic-0004").perpetual = true;
        s.cards.at(20).spell = SpellState::Active;
        auto engine = test::ObservedEngine::scenario(cat, s);
        REQUIRE(engine.submit(0, ActivateSpell{10}).accepted); pass263(engine);
        REQUIRE(engine.state().decision);
        REQUIRE(engine.state().decision->kind == DecisionKind::ReturnSpell);
        REQUIRE(engine.submit(1, Choose{engine.state().decision->id, ChoiceValue::card(2), 0}).accepted);
        CHECK(engine.state().cards.at(20).zone == Zone::Analysis);
        CHECK_FALSE((engine.state().decision && engine.state().decision->kind == DecisionKind::Overflow));
    }
}

TEST_CASE("v263 crosses trigger only at their controllers end start", "[alpha-v263][formation]") {
    const auto cat = currentCatalog();
    for (const auto *definition : {"v260-formation-0001", "v260-formation-0002"}) {
        for (PlayerId active : {0, 1}) {
            INFO(definition); INFO(active);
            auto s = mainState263(); s.active = active; s.phase = Phase::Cast;
            s.players[0].life = 20;
            put263(s, 1, definition, Zone::Analysis);
            auto engine = test::ObservedEngine::scenario(cat, s);
            REQUIRE(engine.submit(active, AdvancePhase{engine.state().phaseGate}).accepted);
            pass263(engine);
            const bool inverse = std::string(definition) == "v260-formation-0002";
            CHECK(engine.state().players[0].life == (active == 0 ? (inverse ? 19 : 21) : 20));
            CHECK(engine.state().players[0].temporaryLife == (active == 0 && inverse ? 3 : 0));
        }
    }
}

TEST_CASE("v263 Ankh departure optionally exiles itself then recovers another formation", "[alpha-v263][formation]") {
    const auto cat = currentCatalog();
    for (bool activate : {false, true}) {
        auto s = mainState263();
        put263(s, 1, "v260-formation-0003", Zone::Analysis);
        put263(s, 2, "v260-formation-0009", Zone::Ash);
        put263(s, 3, "v260-analytic-0001", Zone::Ash);
        auto engine = test::ObservedEngine::scenario(cat, s);
        REQUIRE(engine.submit(0, RemoveFormation{1}).accepted); pass263(engine);
        REQUIRE(engine.state().decision);
        REQUIRE(engine.state().decision->mayPass);
        REQUIRE(engine.submit(0, Choose{engine.state().decision->id, activate ? ChoiceValue::card(1) : ChoiceValue{}}).accepted);
        pass263(engine);
        if (activate) {
            CHECK(engine.state().cards.at(1).zone == Zone::Exile);
            REQUIRE(engine.state().decision);
            CHECK(engine.state().decision->options == std::vector<ChoiceValue>{ChoiceValue::card(2)});
            REQUIRE(engine.submit(0, Choose{engine.state().decision->id, ChoiceValue::card(2)}).accepted);
            CHECK(engine.state().cards.at(2).zone == Zone::SideDeck);
        } else {
            CHECK(engine.state().cards.at(1).zone == Zone::Ash);
            CHECK(engine.state().cards.at(2).zone == Zone::Ash);
        }
    }
}

TEST_CASE("v263 arc offers the threatened controller a real optional destruction replacement", "[alpha-v263][formation]") {
    const auto cat = currentCatalog();
    for (bool replace : {false, true}) {
        auto s = mainState263(); s.phase = Phase::Cast;
        put263(s, 1, "v260-formation-0017", Zone::Analysis);
        put263(s, 2, "v260-formation-0010", Zone::Analysis, 1);
        put263(s, 3, "v260-formation-0007", Zone::Analysis, 1);
        put263(s, 10, "v260-analytic-0036", Zone::Casting).spell = SpellState::Ready;
        put263(s, 11, "v260-formation-0009", Zone::SideDeck);
        auto engine = test::ObservedEngine::scenario(cat, s);
        REQUIRE(engine.submit(0, ActivateSpell{10, 2, {}, 11}).accepted); pass263(engine);
        REQUIRE(engine.state().decision);
        CHECK(engine.state().decision->player == 1);
        CHECK(engine.state().decision->mayPass);
        REQUIRE(engine.submit(1, Choose{engine.state().decision->id, replace ? ChoiceValue::card(3) : ChoiceValue{}}).accepted);
        pass263(engine);
        CHECK(engine.state().cards.at(2).zone == (replace ? Zone::Analysis : Zone::Ash));
        CHECK(engine.state().cards.at(3).zone == (replace ? Zone::Ash : Zone::Analysis));
    }
}

TEST_CASE("v263 sulfur deals two damage for each controlled casting spell leaving and no analysis departure", "[alpha-v263][formation]") {
    const auto cat = currentCatalog();
    auto s = mainState263();
    put263(s, 1, "v260-formation-0008", Zone::Analysis);
    for (CardId id : {10u, 11u}) {
        auto &card = put263(s, id, "v260-word-0001", Zone::Casting, id == 11 ? 1 : 0);
        card.controller = 0; card.spell = SpellState::Resolved;
    }
    auto &analysis = put263(s, 12, "v260-analytic-0001", Zone::Analysis);
    analysis.host = 1; analysis.ringIndex = 0; analysis.spell = SpellState::Ready;
    for (CardId id : {10u, 11u, 12u}) {
        StateMaintenance::leave(s, id, &cat);
    }
    REQUIRE(s.queue.items.size() == 2);
    for (const auto &trigger : s.queue.items)
        for (const auto &effect : trigger.effects) EffectResolver::apply(s, cat, trigger, effect);
    CHECK(s.players[1].life == 36);
}

TEST_CASE("v263 sulfur excludes selectable fire and chaos compares only other faceup schools", "[alpha-v263][formation]") {
    auto cat = currentCatalog();
    auto s = mainState263();
    put263(s, 1, "v260-formation-0008", Zone::Analysis);
    put263(s, 2, "v260-analytic-0005", Zone::Hand);
    cat.cards.at("v260-analytic-0005").effects.front().damageType = DamageType::Fire;
    cat.compileAbilities();
    CHECK_FALSE(card_rules::isFireSpell(cat.at("v260-analytic-0005")));
    CHECK_FALSE(Rules::canHost(s, cat, 2, 1));
    CHECK(Rules::canHost(s, cat, 2, 1, true));
    s.cards.at(1).definition = "v260-formation-0006";
    auto &existing = put263(s, 3, "v260-analytic-0001", Zone::Analysis);
    existing.host = 1; existing.ringIndex = 0; existing.faceDown = true;
    CHECK(Rules::canHost(s, cat, 2, 1));
    existing.faceDown = false;
    CHECK_FALSE(Rules::canHost(s, cat, 2, 1));
    CHECK(Rules::canHost(s, cat, 2, 1, true));
}

TEST_CASE("v263 healing circle triggers before each owner countdown including its last", "[alpha-v263][spells]") {
    const auto cat = currentCatalog();
    for (int remaining : {4, 1}) {
        auto s = mainState263(); s.phase = Phase::Cast; s.players[0].life = 20;
        put263(s, 1, "v260-formation-0017", Zone::Analysis);
        auto &card = put263(s, 10, "v260-analytic-0021", Zone::Analysis);
        card.host = 1; card.ringIndex = 0; card.spell = SpellState::Active; card.remaining = remaining;
        auto engine = test::ObservedEngine::scenario(cat, s);
        REQUIRE(engine.submit(0, AdvancePhase{engine.state().phaseGate}).accepted); pass263(engine);
        CHECK(engine.state().players[0].life == 22);
        CHECK(engine.state().cards.at(10).remaining == remaining - 1);
    }
}

TEST_CASE("v263 chaos flux waits for an opponent activation before granting load", "[alpha-v263][spells]") {
    const auto cat = currentCatalog();
    auto s = mainState263(); s.phase = Phase::Cast;
    put263(s, 1, "v260-formation-0017", Zone::Analysis);
    put263(s, 2, "v260-formation-0017", Zone::Analysis, 1);
    put263(s, 10, "v260-analytic-0029", Zone::Casting).spell = SpellState::Ready;
    auto engine = test::ObservedEngine::scenario(cat, s);
    REQUIRE(engine.submit(0, ActivateSpell{10}).accepted); pass263(engine);
    CHECK(Rules::load(engine.state(), 1) == 0);
}

TEST_CASE("v263 capture strike negates a physical spell and sets it facedown under new control", "[alpha-v263][spells]") {
    const auto cat = currentCatalog();
    for (bool physical : {false, true}) {
        auto s = mainState263(); s.phase = Phase::Cast;
        put263(s, 1, "v260-formation-0017", Zone::Analysis);
        put263(s, 2, "v260-formation-0017", Zone::Analysis, 1);
        put263(s, 10, physical ? "v260-analytic-0013" : "v260-analytic-0001", Zone::Casting).spell = SpellState::Ready;
        auto &capture = put263(s, 20, "v260-talisman-0002", Zone::Analysis, 1);
        capture.host = 2; capture.ringIndex = 0; capture.faceDown = true; capture.ambushedTurn = 1;
        auto engine = test::ObservedEngine::scenario(cat, s);
        REQUIRE(engine.submit(0, ActivateSpell{10, physical ? 0u : playerTarget(1)}).accepted);
        std::optional<Respond> response;
        engine.visitLegalActions(1, [&](const LegalAction &action) {
            if (action.source == 20) if (const auto *candidate = std::get_if<Respond>(&action.command)) response = *candidate;
            return true;
        });
        CHECK(response.has_value() == physical);
        if (!physical) continue;
        REQUIRE(response);
        REQUIRE(engine.submit(1, *response).accepted); pass263(engine);
        REQUIRE(engine.state().decision);
        CHECK(engine.state().decision->player == 1);
        REQUIRE(engine.submit(1, Choose{engine.state().decision->id, ChoiceValue::card(2), 3}).accepted);
        pass263(engine);
        CHECK(engine.state().cards.at(10).zone == Zone::Analysis);
        CHECK(engine.state().cards.at(10).faceDown);
        CHECK(engine.state().cards.at(10).ringIndex == 3);
        CHECK(controllerOf(engine.state().cards.at(10)) == 1);
        CHECK(engine.state().cards.at(10).owner == 0);
        CHECK(engine.state().players[1].life == 40);
        CHECK(engine.state().players[1].mana == 15);
    }
}

TEST_CASE("v263 whispers return only an opposing facedown analysis card", "[alpha-v263][spells]") {
    auto s = mainState263();
    put263(s, 1, "v260-formation-0017", Zone::Analysis);
    put263(s, 2, "v260-formation-0017", Zone::Analysis, 1);
    put263(s, 10, "v260-word-0013", Zone::Hand);
    for (CardId id : {20u, 21u, 22u}) {
        auto &card = put263(s, id, "v260-analytic-0001", id == 22 ? Zone::Casting : Zone::Analysis, 1);
        card.host = id == 22 ? 0 : 2; card.ringIndex = id == 22 ? -1 : int(id - 20);
        card.faceDown = id == 20; card.spell = SpellState::Ready;
    }
    auto engine = test::ObservedEngine::scenario(currentCatalog(), s);
    REQUIRE(engine.submit(0, PreloadWord{10, true, 1, playerTarget(1), {}, 0, 0, 0}).accepted); pass263(engine);
    REQUIRE(engine.state().decision);
    CHECK(engine.state().decision->options == std::vector<ChoiceValue>{ChoiceValue::card(20)});
}

TEST_CASE("v263 funny box can set a higher rank stolen spell on a level one formation", "[alpha-v263][spells]") {
    auto s = mainState263(); s.phase = Phase::Cast;
    put263(s, 1, "v260-formation-0001", Zone::Analysis);
    put263(s, 10, "v260-talisman-0010", Zone::Casting).spell = SpellState::Ready;
    put263(s, 20, "v260-analytic-0012", Zone::Ash, 1);
    auto engine = test::ObservedEngine::scenario(currentCatalog(), s);
    REQUIRE(engine.submit(0, ActivateSpell{10, 20}).accepted); pass263(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.submit(0, Choose{engine.state().decision->id, ChoiceValue::card(1)}).accepted); pass263(engine);
    CHECK(engine.state().cards.at(20).zone == Zone::Analysis);
    CHECK(engine.state().cards.at(20).faceDown);
    CHECK(controllerOf(engine.state().cards.at(20)) == 0);
}

TEST_CASE("v263 call lightning independent load clears at the affected end start", "[alpha-v263][spells]") {
    auto s = mainState263(); s.phase = Phase::Cast;
    put263(s, 1, "v260-formation-0017", Zone::Analysis);
    put263(s, 2, "v260-formation-0017", Zone::Analysis, 1);
    put263(s, 10, "v260-analytic-0012", Zone::Casting).spell = SpellState::Ready;
    auto cat = currentCatalog();
    auto engine = test::ObservedEngine::scenario(cat, s);
    REQUIRE(engine.submit(0, ActivateSpell{10, playerTarget(1)}).accepted); pass263(engine);
    CHECK(Rules::load(engine.state(), 1) == 5);
    auto end = test::mutation(engine.state());
    end.active = 1; ++end.players[1].ownTurn; end.phase = Phase::Cast; end.phaseStep = PhaseStep::Body;
    end.phaseGate = 100; end.decision.reset(); end.chain.reset();
    auto ending = test::ObservedEngine::scenario(cat, end);
    REQUIRE(ending.submit(1, AdvancePhase{100}).accepted); pass263(ending);
    CHECK(Rules::load(ending.state(), 1) == 0);
}

TEST_CASE("v263 response context reports ambush setting without revealing hidden spell metadata", "[alpha-v263][privacy]") {
    auto s = mainState263();
    put263(s, 1, "v260-formation-0017", Zone::Analysis);
    put263(s, 10, "v260-analytic-0009", Zone::Hand);
    auto engine = test::ObservedEngine::scenario(currentCatalog(), s);
    REQUIRE(engine.submit(0, SetAmbush{10, 1, 0}).accepted);
    auto view = engine.viewFor(1);
    REQUIRE(view.chain);
    CHECK(view.chain->links.front().castCost == 0);
    CHECK(view.chain->links.front().speed == 0);
    CHECK(view.chain->links.front().item.effects.empty());
    const auto dto = bridge::viewDto(view);
    REQUIRE(dto.contains("responseContext"));
    CHECK(dto.at("responseContext").at("text") == "对方设置了1张盖伏卡");
    CHECK(dto.dump().find("阳炎爆") == std::string::npos);
}
