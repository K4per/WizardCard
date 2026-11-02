#include "support/observed_engine.hpp"
#include "wizard/application.hpp"
#include "wizard/core/card_rules.hpp"
#include <catch2/catch_test_macros.hpp>
#include <set>
using namespace wizard;
namespace {
CardCatalog currentCatalog() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog; }
MutationContext contentState(Phase phase = Phase::Cast) {
    MutationContext s; s.phase = phase; s.phaseStep = PhaseStep::Body; s.phaseGate = 1;
    s.globalTurn = 5; s.rng = 19;
    for (auto &p : s.players) { p.ownTurn = 2; p.mana = 20; }
    return s;
}
CardInstance &add252(MutationContext &s, CardId id, const std::string &def, PlayerId owner, Zone zone,
                     CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance c; c.id = id; c.definition = def; c.owner = owner; c.zone = zone;
    c.sourceFormation = host; c.spell = spell;
    if (zone == Zone::Analysis || zone == Zone::Attached) c.host = host;
    if (zone == Zone::Analysis) c.fieldOrder = s.nextFieldOrder++;
    s.cards.emplace(id, c);
    if (zone == Zone::Deck) s.players[owner].deck.push_back(id);
    return s.cards.at(id);
}
void send252(test::ObservedEngine &e, PlayerId p, const Command &c) { auto r = e.submit(p, c); INFO(r.error); REQUIRE(r.accepted); }
void settle252(test::ObservedEngine &e) {
    for (int n = 0; n < 80 && e.state().decision; ++n) {
        const auto d = *e.state().decision;
        if (d.kind == DecisionKind::Response) send252(e, d.player, PassResponse{d.id});
        else if (d.kind == DecisionKind::TriggerOrder) send252(e, d.player, Choose{d.id, d.options.front()});
        else return;
    }
}
void choose252(test::ObservedEngine &e, CardId id) {
    settle252(e); REQUIRE(e.state().decision);
    const auto d = *e.state().decision; send252(e, d.player, Choose{d.id, id}); settle252(e);
}
}
TEST_CASE("Alpha v2.5.5 current pool has unique executable identities and excludes unfinished drafts", "[alpha-v252][alpha-v253][content]") {
    const auto cat = currentCatalog(); REQUIRE(cat.cards.size() == 119); REQUIRE(cat.mechanicsRevision == 9);
    CHECK(cat.rulesVersion == "2.5.5"); CHECK(cat.cardSetVersion == "2.5.5");
    CHECK(cat.mechanicsRevision == 9);
    for (const auto *draft : {"upright-cross", "inverted-cross", "ankh"}) CHECK(cat.cards.count(draft) == 0);
    std::set<std::string> numbers;
    for (const auto &[id, d] : cat.cards) {
        DYNAMIC_SECTION(id) {
            CHECK(numbers.insert(d.number).second); CHECK(d.number.size() == 6);
            CHECK_FALSE(d.illustrationId.empty());
            auto s = contentState(); add252(s, 10, id, 0, Zone::Hand);
            auto e = test::ObservedEngine::scenario(cat, s); const auto before = e.digest();
            // Every definition rejects an opponent's forged ownership command atomically.
            CHECK_FALSE(e.submit(1, Abandon{10}).accepted); CHECK(e.digest() == before);
        }
    }
    CHECK(cat.at("aid").type == CardType::Word);
    CHECK(cat.at("engeas-four-point-star").type == CardType::Formation);
    CHECK(cat.at("snow-rain-storm").type == CardType::Talisman);
    CHECK(cat.at("snow-rain-storm").number == "040013");
    CHECK(cat.at("counter-spell").dynamicCost);
}
TEST_CASE("Alpha v2.5.3 fire damage classification is independent of school and validated as a spell", "[alpha-v253][content]") {
    const auto cat = currentCatalog();
    REQUIRE(card_rules::isFireSpell(cat.at("sique-backfire")));
    auto classified = cat.at("sique-backfire"); classified.school = "transmutation";
    REQUIRE(card_rules::isFireSpell(classified));
    REQUIRE_FALSE(card_rules::isFireSpell(cat.at("sique-cauldron")));
    auto data = readCatalogJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    for (auto &card : data["cards"]) if (card["id"] == "hexagram") {
        card["traits"] = Json::array({"fire_damage_spell"});
    }
    REQUIRE_THROWS(parseCatalog(data));
}
TEST_CASE("Alpha v2.5.2 roast recovers any Sique card and enforces its campfire host", "[alpha-v252][content]") {
    auto cat = currentCatalog(); auto s = contentState();
    add252(s, 1, "sique-campfire", 0, Zone::Analysis);
    add252(s, 2, "hexagram", 0, Zone::Analysis);
    add252(s, 10, "sique-roast", 0, Zone::Casting, 1, SpellState::Ready);
    add252(s, 20, "sique-vinegar", 0, Zone::Ash);
    auto e = test::ObservedEngine::scenario(cat, s);
    const auto before = e.digest();
    CHECK_FALSE(e.submit(0, ActivateSpell{10, 999}).accepted); CHECK(e.digest() == before);
    send252(e, 0, ActivateSpell{10, playerTarget(1)}); choose252(e, 20);
    CHECK(e.state().cards.at(20).zone == Zone::Hand);
    CHECK(e.state().players[1].life == startingLife - 6);
    s.cards.at(10).zone = Zone::Hand;
    CHECK_FALSE(Rules::canHost(s, cat, 10, 2)); CHECK(Rules::canHost(s, cat, 10, 1));
}
TEST_CASE("Alpha v2.5.2 blowfire optional discard deals additional fire damage once", "[alpha-v252][content]") {
    auto cat = currentCatalog(); auto s = contentState();
    add252(s, 1, "sique-campfire", 0, Zone::Analysis);
    add252(s, 10, "sique-blowfire", 0, Zone::Casting, 1, SpellState::Ready);
    add252(s, 20, "recall", 0, Zone::Hand);
    auto invalid = s; invalid.cards.at(1).definition = "hexagram";
    auto denied = test::ObservedEngine::scenario(cat, invalid); CHECK_FALSE(denied.submit(0, ActivateSpell{10}).accepted);
    auto e = test::ObservedEngine::scenario(cat, s); send252(e, 0, ActivateSpell{10}); choose252(e, 20);
    CHECK(e.state().cards.at(20).zone == Zone::Ash);
    CHECK(e.state().players[1].life == startingLife - 7);
    CHECK_FALSE(e.submit(0, ActivateSpell{10}).accepted);
}
TEST_CASE("Alpha v2.5.2 backfire recycles opponents cards and scales damage and healing", "[alpha-v252][content]") {
    auto cat = currentCatalog(); auto s = contentState(); s.players[0].life = 20;
    add252(s, 1, "sique-cauldron", 0, Zone::Analysis);
    add252(s, 10, "sique-backfire", 0, Zone::Casting, 1, SpellState::Ready);
    add252(s, 20, "hexagram", 1, Zone::Ash); add252(s, 21, "spark", 1, Zone::Ash);
    add252(s, 22, "recall", 0, Zone::Ash);
    auto e = test::ObservedEngine::scenario(cat, s); send252(e, 0, ActivateSpell{10});
    settle252(e); REQUIRE(e.state().decision);
    CHECK_FALSE(e.submit(0, Choose{e.state().decision->id, 22}).accepted);
    choose252(e, 20); choose252(e, 21);
    CHECK(e.state().cards.at(20).zone == Zone::SideDeck); CHECK(e.state().cards.at(21).zone == Zone::Deck);
    CHECK(e.state().players[0].life == 24); CHECK(e.state().players[1].life == startingLife - 2);
}
TEST_CASE("Alpha v2.5.2 new star hosts shorten analysis and add the specified independent load", "[alpha-v252][content]") {
    auto cat = currentCatalog();
    for (const auto &host : {"three-star", "two-star"}) {
        DYNAMIC_SECTION(host) {
            auto s = contentState(Phase::Main); add252(s, 1, host, 0, Zone::Analysis);
            add252(s, 10, "sique-pancakes", 0, Zone::Hand);
            // Use an in-rank spell with more than one analysis round.
            s.cards.at(10).definition = host == std::string("three-star") ? "healing-circle" : "earthquake";
            auto e = test::ObservedEngine::scenario(cat, s);
            const int fee = Rules::analysisCost(s, cat, 10, 1);
            send252(e, 0, StartAnalysis{10, 1}); settle252(e);
            const int advance = host == std::string("three-star") ? 1 : 2;
            CHECK(e.state().cards.at(10).analysisProgress == advance);
            CHECK(Rules::load(e.state(), 0) == fee + advance);
        }
    }
}
TEST_CASE("Alpha v2.5.2 moon blocks face up setup and prevents overload while allowing ambush", "[alpha-v252][content]") {
    auto cat = currentCatalog(); auto s = contentState(Phase::Main);
    add252(s, 1, "hexagram", 0, Zone::Analysis); add252(s, 2, "moon", 1, Zone::Analysis);
    add252(s, 10, "fireball", 0, Zone::Hand);
    auto e = test::ObservedEngine::scenario(cat, s); CHECK_FALSE(e.submit(0, StartAnalysis{10, 1}).accepted);
    send252(e, 0, SetAmbush{10, 1}); settle252(e); CHECK(e.state().cards.at(10).faceDown);
    s.temporary.push_back({s.nextLoad++, 1, 0, 100, 3});
    ResourceRules::increasedLoad(s, cat, 1, 100); CHECK(s.players[1].life == startingLife);
    ResourceRules::damage(s, 1, 1, "exhaustion", &cat); CHECK(s.players[1].life == startingLife - 1);
}
TEST_CASE("Alpha v2.5.2 scorching rays choose a fresh player for each subsequent ray", "[alpha-v252][content]") {
    auto cat = currentCatalog(); auto s = contentState();
    add252(s, 1, "hexagram", 0, Zone::Analysis);
    add252(s, 10, "scorching-ray", 0, Zone::Casting, 1, SpellState::Ready);
    auto e = test::ObservedEngine::scenario(cat, s); send252(e, 0, ActivateSpell{10, playerTarget(1)});
    settle252(e); REQUIRE(e.state().decision);
    CHECK_FALSE(e.submit(0, Choose{e.state().decision->id, 999}).accepted);
    choose252(e, playerTarget(0)); choose252(e, playerTarget(1));
    CHECK(e.state().players[0].life == startingLife - 5);
    CHECK(e.state().players[1].life == startingLife - 10);
    CHECK(e.state().cards.at(10).spell == SpellState::Resolved);
    CHECK_FALSE(e.submit(0, ActivateSpell{10, playerTarget(1)}).accepted);
}
TEST_CASE("Alpha v2.5.2 ice storm optionally delays every enemy analysis including zero progress", "[alpha-v252][content]") {
    auto cat = currentCatalog(); auto s = contentState();
    add252(s, 1, "hexagram", 0, Zone::Analysis); add252(s, 2, "hexagram", 1, Zone::Analysis);
    add252(s, 10, "ice-storm", 0, Zone::Casting, 1, SpellState::Ready);
    add252(s, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Analyzing).ringIndex = 0;
    auto &ready = add252(s, 21, "mend", 1, Zone::Analysis, 2, SpellState::Ready);
    ready.ringIndex = 1; ready.analysisProgress = cat.at("mend").analysisTurns;
    auto e = test::ObservedEngine::scenario(cat, s); send252(e, 0, ActivateSpell{10, playerTarget(1)});
    choose252(e, 0xffffffd1u);
    CHECK(e.state().cards.at(20).analysisProgress == -1);
    CHECK(e.state().cards.at(21).spell == SpellState::Analyzing);
}
TEST_CASE("Alpha v2.5.2 instant circuit advances exactly two turns and sleep locks both players", "[alpha-v252][content]") {
    auto cat = currentCatalog(); auto s = contentState();
    add252(s, 1, "hexagram", 0, Zone::Analysis);
    auto &card = add252(s, 20, "arcana-chariot", 0, Zone::Analysis, 1, SpellState::Analyzing);
    card.ringIndex = 0; card.settingPaid = 5;
    Trigger t; t.owner = 0; t.source = 10; t.targetCard = 20;
    EffectResolver::apply(s, cat, t, {EffectKind::AccelerateAnalysis, 2});
    CHECK(s.cards.at(20).analysisProgress == 2);
    CHECK(s.cards.at(20).spell == SpellState::Analyzing);
    CHECK(Rules::load(s, 0) == 5);
    t.targetCard = 0; EffectResolver::apply(s, cat, t, {EffectKind::BlockActions, 0});
    add252(s, 10, "spark", 0, Zone::Casting, 1, SpellState::Ready);
    add252(s, 11, "spark", 1, Zone::Casting, 1, SpellState::Ready);
    auto e = test::ObservedEngine::scenario(cat, s);
    CHECK_FALSE(e.submit(0, ActivateSpell{10, playerTarget(1)}).accepted);
    s.active = 1; e = test::ObservedEngine::scenario(cat, s);
    CHECK_FALSE(e.submit(1, ActivateSpell{11, playerTarget(0)}).accepted);
    s.active = 0; ++s.players[0].ownTurn; e = test::ObservedEngine::scenario(cat, s);
    send252(e, 0, ActivateSpell{10, playerTarget(1)}); settle252(e);
    CHECK(e.state().players[1].life == startingLife - 3);
}
