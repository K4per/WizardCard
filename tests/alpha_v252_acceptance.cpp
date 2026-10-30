#include "support/observed_engine.hpp"
#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;
namespace {
CardCatalog acceptanceCatalog() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
}
MutationContext acceptanceState(Phase phase = Phase::Main) {
    MutationContext s;
    s.phase = phase; s.phaseStep = PhaseStep::Body; s.phaseGate = 1;
    s.globalTurn = 5; s.rng = 43;
    for (auto &p : s.players) { p.mana = 20; p.ownTurn = 2; }
    return s;
}
CardInstance &acceptanceCard(MutationContext &s, CardId id, const std::string &definition, PlayerId owner,
                             Zone zone, CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance card;
    card.id = id; card.definition = definition; card.owner = owner; card.zone = zone;
    card.host = zone == Zone::Analysis || zone == Zone::Attached ? host : 0;
    card.sourceFormation = host; card.spell = spell;
    if (zone == Zone::Analysis) card.fieldOrder = s.nextFieldOrder++;
    s.cards.emplace(id, card);
    if (zone == Zone::Deck) s.players[owner].deck.push_back(id);
    return s.cards.at(id);
}
void acceptanceSend(test::ObservedEngine &e, PlayerId p, const Command &command) {
    const auto result = e.submit(p, command);
    INFO(result.error);
    REQUIRE(result.accepted);
}
void acceptanceSettle(test::ObservedEngine &e) {
    for (int n = 0; n < 96 && e.state().decision && e.state().result == -1; ++n) {
        const auto d = *e.state().decision;
        if (d.kind == DecisionKind::Response) acceptanceSend(e, d.player, PassResponse{d.id});
        else if (!d.options.empty()) acceptanceSend(e, d.player, Choose{d.id, d.options.front()});
        else if (d.mayPass) acceptanceSend(e, d.player, Choose{d.id, 0});
        else FAIL("A mandatory acceptance decision has no legal option");
    }
}
}

TEST_CASE("Alpha v2.5.2 Empress heals and clears two loads only on its owners end start", "[alpha-v252][acceptance]") {
    const auto cat = acceptanceCatalog();
    for (PlayerId active : {0, 1}) {
        DYNAMIC_SECTION(active) {
            auto s = acceptanceState(Phase::Cast); s.active = active;
            acceptanceCard(s, 1, "hexagram", 0, Zone::Analysis);
            acceptanceCard(s, 2, "hexagram", 1, Zone::Analysis);
            auto &empress = acceptanceCard(s, 10, "arcana-empress", 0, Zone::Analysis, 1, SpellState::Active);
            empress.ringIndex = 0; empress.remaining = 4;
            s.players[0].life = 30;
            s.temporary.push_back({s.nextLoad++, 0, 0, 3, 0, true});
            auto e = test::ObservedEngine::scenario(cat, s);
            acceptanceSend(e, active, AdvancePhase{e.state().phaseGate}); acceptanceSettle(e);
            REQUIRE(e.state().phase == Phase::End);
            CHECK(e.state().players[0].life == (active == 0 ? 32 : 30));
            CHECK(Rules::load(e.state(), 0) == (active == 0 ? 1 : 3));
            CHECK(e.state().cards.at(10).remaining == 4);
        }
    }
}

TEST_CASE("Alpha v2.5.2 Tower respects effect indestructibility on both selected formations", "[alpha-v252][acceptance]") {
    const auto cat = acceptanceCatalog();
    for (bool protectOwn : {false, true}) {
        DYNAMIC_SECTION(protectOwn) {
            auto s = acceptanceState(Phase::Cast);
            acceptanceCard(s, 1, protectOwn ? "hexagram" : "reservoir", 0, Zone::Analysis);
            acceptanceCard(s, 2, protectOwn ? "reservoir" : "hexagram", 1, Zone::Analysis);
            acceptanceCard(s, 10, "arcana-tower", 0, Zone::Casting, 1, SpellState::Ready);
            auto e = test::ObservedEngine::scenario(cat, s);
            acceptanceSend(e, 0, ActivateSpell{10, 1, {1}}); acceptanceSettle(e);
            CHECK(e.state().cards.at(protectOwn ? 1 : 2).zone == Zone::Analysis);
            if (!protectOwn) CHECK(e.state().cards.at(1).zone == Zone::Ash);
        }
    }
}

TEST_CASE("Alpha v2.5.2 Vinegar grants attach and detach life without its old damage reduction", "[alpha-v252][acceptance]") {
    const auto cat = acceptanceCatalog(); auto s = acceptanceState();
    acceptanceCard(s, 1, "sique-cauldron", 0, Zone::Analysis);
    acceptanceCard(s, 2, "sique-campfire", 0, Zone::Analysis);
    acceptanceCard(s, 10, "sique-vinegar", 0, Zone::Hand);
    auto e = test::ObservedEngine::scenario(cat, s);
    const auto before = e.digest();
    CHECK_FALSE(e.submit(0, AttachSeal{10, 2}).accepted); CHECK(e.digest() == before);
    acceptanceSend(e, 0, AttachSeal{10, 1}); acceptanceSettle(e);
    REQUIRE(e.state().players[0].temporaryLife == 2);
    CHECK(e.state().players[0].mana == 18);
    auto damaged = test::mutation(e.state());
    Trigger damage; damage.owner = 1; damage.targetPlayer = 0;
    EffectResolver::apply(damaged, cat, damage, {EffectKind::Damage, 5});
    CHECK(damaged.players[0].temporaryLife == 0);
    CHECK(damaged.players[0].life == startingLife - 3);
    acceptanceSend(e, 0, RemoveFormation{1}); acceptanceSettle(e);
    CHECK(e.state().cards.at(10).zone == Zone::Ash);
    CHECK(e.state().players[0].temporaryLife == 6);
}

TEST_CASE("Alpha v2.5.2 Chili deals one damage per actual opponent draw", "[alpha-v252][acceptance]") {
    const auto cat = acceptanceCatalog(); auto s = acceptanceState();
    acceptanceCard(s, 1, "sique-cauldron", 0, Zone::Analysis);
    acceptanceCard(s, 10, "sique-chili", 0, Zone::Attached, 1);
    acceptanceCard(s, 2, "hexagram", 1, Zone::Analysis);
    acceptanceCard(s, 30, "recall", 1, Zone::Hand);
    acceptanceCard(s, 20, "spark", 1, Zone::Deck);
    s.active = 1;
    auto e = test::ObservedEngine::scenario(cat, s);
    acceptanceSend(e, 1, PreloadWord{30, true, 2}); acceptanceSettle(e);
    CHECK(e.state().players[1].life == startingLife - 2); // One actual draw triggers Chili once.
    CHECK(e.state().players[1].exhaustion == 1);
    CHECK(e.state().cards.at(20).zone == Zone::Hand);
}
