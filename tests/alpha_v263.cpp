#include "support/observed_engine.hpp"
#include "wizard/content.hpp"
#include "wizard/core/action_query.hpp"
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
