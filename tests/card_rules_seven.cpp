#include "wizard/replay.hpp"
#include "wizard/core/card_rules.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;
namespace {
CardCatalog sevenCatalog() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog; }
GameState sevenState(int ownTurn = 1, Phase phase = Phase::Main) {
    GameState s; s.phase = phase; s.phaseStep = PhaseStep::Body; s.phaseGate = 1;
    s.globalTurn = 5; s.rng = 149;
    for (auto &p : s.players) { p.ownTurn = ownTurn; p.mana = 20; }
    return s;
}
CardInstance &putSeven(GameState &s, CardId id, const char *definition, PlayerId owner, Zone zone,
                      CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance c; c.id = id; c.definition = definition; c.owner = owner; c.zone = zone;
    c.host = host; c.sourceFormation = host; c.spell = spell;
    if (zone == Zone::Analysis) c.fieldOrder = s.nextFieldOrder++;
    if (host) { c.ringIndex = 0; c.sourceRingIndex = 0; }
    if (zone == Zone::Casting) { c.host = 0; c.ringIndex = -1; }
    s.cards[id] = c;
    if (zone == Zone::Deck) s.players[owner].deck.push_back(id);
    return s.cards.at(id);
}
void sendSeven(GameEngine &e, PlayerId p, const Command &command) {
    const auto result = e.submit(p, command); INFO(result.error); REQUIRE(result.accepted);
}
void passSeven(GameEngine &e) {
    for (int i = 0; e.state().decision && i < 100; ++i) {
        const auto d = *e.state().decision;
        if (d.kind == DecisionKind::Response) sendSeven(e, d.player, PassResponse{d.id});
        else if (d.kind == DecisionKind::TriggerOrder) sendSeven(e, d.player, Choose{d.id, d.options.front()});
        else return;
    }
}
void chooseSeven(GameEngine &e, CardId choice) {
    passSeven(e); REQUIRE(e.state().decision);
    const auto d = *e.state().decision; sendSeven(e, d.player, Choose{d.id, choice}); passSeven(e);
}
void castSeven(GameEngine &e, CardId source) {
    sendSeven(e, 0, ActivateSpell{source}); passSeven(e);
}
}

TEST_CASE("2.5.4 seven aspects use each owner's turn and shift expires on next own turn", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    CHECK(card_rules::aspectMode(s, cat, 0) == 0);
    putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis);
    putSeven(s, 2, "qixing-tianshu", 1, Zone::Analysis);
    CHECK(card_rules::aspectMode(s, cat, 0) == 1);
    s.players[1].ownTurn = 2; CHECK(card_rules::aspectMode(s, cat, 1) == 2);
    s.active = 1; ++s.globalTurn; CHECK(card_rules::aspectMode(s, cat, 0) == 1);
    s.players[0].sevenShifted = true; s.players[0].sevenShiftUntilOwnTurn = 2;
    CHECK(card_rules::aspectMode(s, cat, 0) == 2);
    s.players[0].ownTurn = 2; CHECK(card_rules::aspectMode(s, cat, 0) == 2);
    s.players[0].ownTurn = 3; CHECK(card_rules::aspectMode(s, cat, 0) == 1);
}

TEST_CASE("2.5.4 seven higher formations cannot be normally set and Tianshu pays its recipe", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    putSeven(s, 1, "qixing-tianshu", 0, Zone::SideDeck);
    putSeven(s, 2, "qixing-tianxuan", 0, Zone::SideDeck);
    auto e = GameEngine::scenario(cat, s); const auto before = e.digest();
    CHECK_FALSE(e.submit(0, SetFormation{2}).accepted); CHECK(e.digest() == before);
    sendSeven(e, 0, SetFormation{1}); passSeven(e);
    CHECK(e.state().cards.at(1).zone == Zone::Analysis); CHECK(e.state().players[0].mana == 18);
}

TEST_CASE("2.5.4 seven insight upgrades leave attachments and cannot bypass exclusive Yaoguang", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis).stormPoints = 4;
    putSeven(s, 2, "qixing-tianxuan", 0, Zone::SideDeck);
    putSeven(s, 3, "qixing-banner", 0, Zone::Attached, 1);
    putSeven(s, 10, "qixing-divination", 0, Zone::Casting, 1, SpellState::Ready);
    putSeven(s, 20, "spark", 0, Zone::Hand);
    putSeven(s, 21, "qixing-shift", 0, Zone::Deck);
    auto e = GameEngine::scenario(cat, s);
    sendSeven(e, 0, ActivateSpell{10, 0, {}, 20}); passSeven(e);
    chooseSeven(e, 1); chooseSeven(e, 2); chooseSeven(e, 21);
    CHECK(e.state().cards.at(1).zone == Zone::Ash);
    CHECK(e.state().cards.at(3).zone == Zone::Ash);
    CHECK(e.state().cards.at(2).zone == Zone::Analysis);
    CHECK(e.state().cards.at(2).stormPoints == 0);
    CHECK(e.state().cards.at(21).zone == Zone::Hand);
    auto boundary = sevenState(); putSeven(boundary, 1, "qixing-kaiyang", 0, Zone::Analysis);
    putSeven(boundary, 2, "qixing-yaoguang", 0, Zone::SideDeck);
    CHECK(card_rules::formationUpgradeSources(boundary, cat, 0).empty());
    CHECK(card_rules::formationUpgradeSources(boundary, cat, 0, true) == std::vector<CardId>{1});
}

TEST_CASE("2.5.4 seven formation setup retrieves the specified deck and ash cards", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog();
    for (const auto *name : {"qixing-tianji", "qixing-tianquan", "qixing-yuheng"}) {
        DYNAMIC_SECTION(name) {
            auto s = sevenState(); putSeven(s, 1, name, 0, Zone::Analysis);
            const bool deck = std::string(name) == "qixing-tianquan";
            putSeven(s, 20, std::string(name) == "qixing-tianji" ? "qixing-shift" : "qixing-banner", 0, deck ? Zone::Deck : Zone::Ash);
            card_rules::queueFormationSetup(s, cat, 1);
            auto e = GameEngine::scenario(cat, s);
            sendSeven(e, 0, Surrender{});
            // Setup triggers execute through the normal engine pump in the
            // following live scenario rather than invoking an effect directly.
            s.players[0].surrendered = false;
            s.decision = PendingDecision{9, 0, DecisionKind::TriggerOrder, {s.queue.items.front().id}, false};
            auto active = GameEngine::scenario(cat, s);
            sendSeven(active, 0, Choose{9, s.queue.items.front().id}); passSeven(active); chooseSeven(active, 20);
            CHECK(active.state().cards.at(20).zone == Zone::Hand);
        }
    }
}

TEST_CASE("2.5.4 seven divination inspects fewer than seven and bottoms in original order without shuffle", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState(2);
    putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis);
    putSeven(s, 10, "qixing-divination", 0, Zone::Casting, 1, SpellState::Ready);
    putSeven(s, 20, "spark", 0, Zone::Deck); putSeven(s, 21, "qixing-shift", 0, Zone::Deck);
    putSeven(s, 22, "mend", 0, Zone::Deck); const auto rng = s.rng;
    auto e = GameEngine::scenario(cat, s); castSeven(e, 10); chooseSeven(e, 21);
    CHECK(e.state().cards.at(21).zone == Zone::Hand);
    CHECK(e.state().players[0].deck == std::vector<CardId>{20, 22}); CHECK(e.state().rng == rng);
}

TEST_CASE("2.5.4 seven awakening bright price is chosen and dark retrieval returns formations to side deck", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog();
    SECTION("bright") {
        auto s = sevenState(); putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis);
        putSeven(s, 10, "qixing-awaken", 0, Zone::Casting, 1, SpellState::Ready);
        putSeven(s, 20, "qixing-banner", 0, Zone::Deck);
        auto e = GameEngine::scenario(cat, s); castSeven(e, 10); chooseSeven(e, 20); chooseSeven(e, 0xffffffb1u);
        CHECK(e.state().players[0].life == 33); CHECK(e.state().cards.at(20).zone == Zone::Hand);
    }
    SECTION("dark") {
        auto s = sevenState(2); putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis);
        putSeven(s, 10, "qixing-awaken", 0, Zone::Casting, 1, SpellState::Ready);
        putSeven(s, 20, "qixing-tianxuan", 0, Zone::Ash);
        auto e = GameEngine::scenario(cat, s); castSeven(e, 10); chooseSeven(e, 20);
        CHECK(e.state().cards.at(20).zone == Zone::SideDeck); CHECK(e.state().players[0].life == 40);
    }
}

TEST_CASE("2.5.4 seven shift pays life separately from damage and dark shift upgrades", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis);
    putSeven(s, 10, "qixing-shift", 0, Zone::Casting, 1, SpellState::Ready);
    s.players[0].nextDamageImmune = true;
    auto e = GameEngine::scenario(cat, s); castSeven(e, 10);
    CHECK(e.state().players[0].life == 33); CHECK(e.state().players[0].nextDamageImmune);
    CHECK(card_rules::aspectMode(e.state(), cat, 0) == 2);
    auto dark = sevenState(2); putSeven(dark, 1, "qixing-tianshu", 0, Zone::Analysis);
    putSeven(dark, 2, "qixing-tianxuan", 0, Zone::SideDeck);
    putSeven(dark, 10, "qixing-shift", 0, Zone::Casting, 1, SpellState::Ready);
    putSeven(dark, 20, "spark", 0, Zone::Hand);
    auto upgrade = GameEngine::scenario(cat, dark); sendSeven(upgrade, 0, ActivateSpell{10, 0, {}, 20}); passSeven(upgrade);
    chooseSeven(upgrade, 1); chooseSeven(upgrade, 2); CHECK(upgrade.state().cards.at(2).zone == Zone::Analysis);
}

TEST_CASE("2.5.4 Kaiyang replaces to Yaoguang and resets resources with five draws", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    putSeven(s, 1, "qixing-kaiyang", 0, Zone::Analysis);
    putSeven(s, 2, "qixing-yaoguang", 0, Zone::SideDeck);
    putSeven(s, 3, "qixing-banner", 0, Zone::Attached, 1);
    putSeven(s, 20, "spark", 0, Zone::Hand); s.players[0].life = 5;
    for (CardId i = 30; i < 35; ++i) putSeven(s, i, "spark", 0, Zone::Deck);
    auto e = GameEngine::scenario(cat, s); sendSeven(e, 0, ActivateAbility{1, 0, "seven_kaiyang"}); passSeven(e);
    chooseSeven(e, 1); chooseSeven(e, 2);
    CHECK(e.state().cards.at(1).zone == Zone::Ash); CHECK(e.state().cards.at(3).zone == Zone::Ash);
    CHECK(e.state().cards.at(20).zone == Zone::Ash); CHECK(e.state().cards.at(2).zone == Zone::Analysis);
    CHECK(e.state().players[0].life == 40); CHECK(e.state().players[0].mana == 0);
    CHECK(e.state().players[0].deck.empty());
    CHECK(std::count_if(e.state().cards.begin(), e.state().cards.end(), [](const auto &entry) { return entry.second.zone == Zone::Hand; }) == 5);
}

TEST_CASE("2.5.4 Yaoguang protects other field seven cards and active lamp departing loses", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    putSeven(s, 1, "qixing-yaoguang", 0, Zone::Analysis);
    putSeven(s, 2, "qixing-banner", 0, Zone::Attached, 1);
    putSeven(s, 10, "qixing-lamp", 0, Zone::Casting, 1, SpellState::Ready);
    CHECK_FALSE(card_rules::effectImmune(s, cat, 1)); CHECK(card_rules::effectImmune(s, cat, 2));
    auto undeclared = s; StateMaintenance::leave(undeclared, 10, &cat); StateMaintenance::check(undeclared, cat);
    CHECK(undeclared.result == -1);
    auto active = s; active.cards.at(10).spell = SpellState::Active;
    ResourceRules::damage(active, 0, 20, "overload", &cat); CHECK(active.players[0].life == 40);
    StateMaintenance::leave(active, 10, &cat); StateMaintenance::check(active, cat); CHECK(active.result == 1);
}

TEST_CASE("2.5.4 seven ruin and punishment use the correct aspect damage recipients", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog();
    for (int turn : {1, 2}) for (const auto *name : {"qixing-ruin", "qixing-punishment"}) {
        DYNAMIC_SECTION(name << turn) {
            auto s = sevenState(turn, Phase::Cast); putSeven(s, 1, "qixing-yaoguang", 0, Zone::Analysis);
            putSeven(s, 10, name, 0, Zone::Casting, 1, SpellState::Ready);
            auto e = GameEngine::scenario(cat, s); castSeven(e, 10);
            CHECK(e.state().players[0].life == (std::string(name) == "qixing-ruin" ? 20 : 40));
            CHECK(e.state().players[1].life == (std::string(name) == "qixing-ruin" ? 20 : 28));
        }
    }
}

TEST_CASE("2.5.4 seven return requires one card then permits stopping up to seven", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis);
    putSeven(s, 10, "qixing-return", 0, Zone::Casting, 1, SpellState::Ready);
    putSeven(s, 20, "qixing-tianxuan", 0, Zone::Ash); putSeven(s, 21, "qixing-banner", 0, Zone::Ash);
    auto e = GameEngine::scenario(cat, s); castSeven(e, 10);
    REQUIRE(e.state().decision); CHECK_FALSE(e.state().decision->mayPass);
    const auto before = e.digest(); CHECK_FALSE(e.submit(0, Choose{e.state().decision->id, 0}).accepted); CHECK(e.digest() == before);
    chooseSeven(e, 20); REQUIRE(e.state().decision); CHECK(e.state().decision->mayPass); chooseSeven(e, 0);
    CHECK(e.state().cards.at(20).zone == Zone::SideDeck); CHECK(e.state().cards.at(21).zone == Zone::Ash);
}

TEST_CASE("2.5.4 seven gaze accepts only the enemy spell directly chaining an own seven source", "[alpha-v254][seven]") {
    const auto cat = sevenCatalog(); auto s = sevenState();
    putSeven(s, 10, "qixing-awaken", 0, Zone::Casting, 0, SpellState::Pending);
    putSeven(s, 20, "spark", 1, Zone::Casting, 0, SpellState::Pending);
    ChainState chain; chain.priority = 0; chain.initiator = 0;
    ChainLink own; own.id = 1; own.kind = LinkKind::Spell; own.item.owner = 0; own.item.source = 10;
    ChainLink enemy; enemy.id = 2; enemy.kind = LinkKind::Spell; enemy.item.owner = 1; enemy.item.source = 20;
    chain.links = {own, enemy}; s.chain = chain;
    CHECK(card_rules::validLinkedResponse(s, cat, 0, 2)); CHECK_FALSE(card_rules::validLinkedResponse(s, cat, 1, 1));
    s.cards.at(10).definition = "spark"; CHECK_FALSE(card_rules::validLinkedResponse(s, cat, 0, 2));
}

TEST_CASE("2.5.4 seven banner discounts spell activation but not formation costs", "[alpha-v254][seven][cost]") {
    const auto cat = sevenCatalog();
    auto s = sevenState(1, Phase::Cast);
    putSeven(s, 1, "qixing-tianshu", 0, Zone::Analysis);
    putSeven(s, 10, "fireball", 0, Zone::Casting, 1, SpellState::Ready);
    putSeven(s, 2, "qixing-banner", 0, Zone::Attached, 10);
    putSeven(s, 11, "qixing-kaiyang", 0, Zone::SideDeck);
    CHECK(Rules::castCost(s, cat, 10) == 0);
    CHECK(Rules::analysisCost(s, cat, 11, 1) == cat.at("qixing-kaiyang").cost);
}
