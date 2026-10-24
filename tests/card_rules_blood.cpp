#include "wizard/core/card_rules.hpp"
#include "wizard/core/engine.hpp"
#include "wizard/core/rules.hpp"
#include "wizard/content.hpp"
#include "wizard/application.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <filesystem>

using namespace wizard;
namespace {
CardCatalog bloodCatalog() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
}
CardInstance &bloodCard(GameState &s, CardId id, const char *definition, PlayerId owner, Zone zone,
                        SpellState spell = SpellState::None, CardId host = 0) {
    CardInstance card;
    card.id = id; card.definition = definition; card.owner = owner; card.zone = zone;
    card.spell = spell; card.host = host; card.sourceFormation = host;
    if (zone == Zone::Analysis) card.fieldOrder = s.nextFieldOrder++;
    s.cards[id] = card;
    return s.cards.at(id);
}
}

TEST_CASE("Alpha v2.5.4 blood life limits clamp and continuous blood mark does not heal on removal",
          "[card-rules][blood][life-limit]") {
    const auto cat = bloodCatalog();
    GameState s;
    s.players[0].life = 40;
    card_rules::changeLifeLimit(s, cat, 0, -5);
    CHECK(card_rules::lifeLimit(s, cat, 0) == 35);
    CHECK(s.players[0].life == 35);

    bloodCard(s, 1, "blood-altar", 0, Zone::Analysis);
    bloodCard(s, 2, "blood-mark", 0, Zone::Attached, SpellState::None, 1);
    card_rules::normalizeLife(s, cat, 0);
    CHECK(card_rules::lifeLimit(s, cat, 0) == 29);
    CHECK(s.players[0].life == 29);
    StateMaintenance::leave(s, 2, &cat);
    card_rules::normalizeLife(s, cat, 0);
    CHECK(card_rules::lifeLimit(s, cat, 0) == 35);
    CHECK(s.players[0].life == 29);

    card_rules::changeLifeLimit(s, cat, 0, -100);
    CHECK(card_rules::lifeLimit(s, cat, 0) == 1);
    CHECK(s.players[0].life == 1);
    card_rules::changeLifeLimit(s, cat, 0, 100);
    CHECK(card_rules::lifeLimit(s, cat, 0) == 40);
    CHECK(s.players[0].life == 1);
}

TEST_CASE("Alpha v2.5.4 blood payment leaves one life and permits mixed mana and life",
          "[card-rules][blood][payment]") {
    const auto cat = bloodCatalog();
    GameState s;
    s.players[0].mana = 1;
    s.players[0].life = 8;
    bloodCard(s, 1, "blood-altar", 0, Zone::Analysis);
    const auto choices = card_rules::paymentChoices(s, cat, 0, 4);
    REQUIRE(choices.size() == 1);
    CHECK(choices.front().mana == 1);
    CHECK(choices.front().life == 6);
    CHECK_FALSE(card_rules::canPayLife(s, 0, 8));
    CHECK(card_rules::canPayLife(s, 0, 7));

    s.selectedPayment = choices.front();
    card_rules::pay(s, cat, 0, 4);
    CHECK(s.players[0].mana == 0);
    CHECK(s.players[0].life == 2);
}

TEST_CASE("Alpha v2.5.4 blood sacrifice table rewards only the first payment in a turn",
          "[card-rules][blood][payment]") {
    const auto cat = bloodCatalog();
    GameState s;
    s.active = 0; s.globalTurn = 5; s.players[0].ownTurn = 2; s.players[0].life = 20;
    bloodCard(s, 1, "blood-sacrificial-table", 0, Zone::Analysis);
    bloodCard(s, 10, "spark", 0, Zone::Deck);
    bloodCard(s, 11, "spark", 0, Zone::Deck);
    s.players[0].deck = {10, 11};

    card_rules::payLife(s, cat, 0, 2, 100);
    CHECK(s.players[0].life == 18);
    CHECK(s.players[0].deck == std::vector<CardId>{10});
    CHECK(s.players[0].firstLifePaymentTurn == 5);
    card_rules::payLife(s, cat, 0, 2, 101);
    CHECK(s.players[0].life == 16);
    CHECK(s.players[0].deck == std::vector<CardId>{10});

    s.globalTurn = 6;
    card_rules::payLife(s, cat, 0, 1, 102);
    CHECK(s.players[0].life == 15);
    CHECK(s.players[0].deck.empty());
    CHECK(std::none_of(s.events.begin(), s.events.end(), [](const GameEvent &event) {
        return event.kind == "damage";
    }));
}

TEST_CASE("Alpha v2.5.4 blood heart heals actual damage after temporary life absorption",
          "[card-rules][blood][heart]") {
    const auto cat = bloodCatalog();
    GameState s;
    s.players[0].life = 20;
    s.players[1].life = 40;
    bloodCard(s, 1, "blood-heart", 0, Zone::Analysis);
    bloodCard(s, 10, "blood-blade", 0, Zone::Casting, SpellState::Active);
    ResourceRules::grantTemporaryLife(s, 1, 99, 6, 4);
    Trigger trigger; trigger.owner = 0; trigger.source = 10; trigger.targetPlayer = 1;
    EffectResolver::apply(s, cat, trigger, {EffectKind::Damage, 10, DamageType::Necrotic,
                                            EffectRecipient::Opponent});
    CHECK(s.players[1].life == 36);
    CHECK(s.players[1].temporaryLife == 0);
    CHECK(s.players[0].life == 24);
}

TEST_CASE("Alpha v2.5.4 same origin propagates only positive healing once and terminates cycles",
          "[card-rules][blood][same-origin]") {
    const auto cat = bloodCatalog();
    GameState s;
    s.players[0].life = 30;
    s.players[1].life = 38;
    bloodCard(s, 10, "blood-same-origin", 0, Zone::Analysis, SpellState::Active);
    bloodCard(s, 11, "blood-same-origin", 1, Zone::Analysis, SpellState::Active);
    bloodCard(s, 12, "blood-same-origin", 1, Zone::Casting, SpellState::Active);
    const auto restored = card_rules::heal(s, cat, 0, 5, 77);
    CHECK(restored == 5);
    CHECK(s.players[0].life == 37);
    CHECK(s.players[1].life == 40);
    const auto propagated = std::count_if(s.events.begin(), s.events.end(), [](const GameEvent &event) {
        return event.kind == "heal" && event.card != 77;
    });
    CHECK(propagated == 2);
    CHECK(std::all_of(s.events.begin(), s.events.end(), [](const GameEvent &event) {
        return event.kind != "heal" || event.amount > 0;
    }));
}

TEST_CASE("Alpha v2.5.4 blood pool restores only temporary life that expires",
          "[card-rules][blood][temporary-life]") {
    const auto cat = bloodCatalog();
    GameState s;
    s.active = 1; s.phase = Phase::End; s.phaseStep = PhaseStep::Body; s.phaseGate = 1;
    s.globalTurn = 5; s.players[0].ownTurn = 2; s.players[1].ownTurn = 2;
    s.players[0].life = 20; s.players[0].lifeLimit = 35;
    bloodCard(s, 1, "blood-pool", 0, Zone::Analysis);
    bloodCard(s, 10, "spark", 0, Zone::Deck);
    s.players[0].deck = {10};
    ResourceRules::grantTemporaryLife(s, 0, 99, 5, 3);
    s.players[0].ownTurn = 3;
    const int before = s.players[0].temporaryLife;
    ResourceRules::expireTemporaryLife(s, 0);
    CHECK(s.players[0].temporaryLife == 0);
    card_rules::expiredLife(s, cat, 0, before - s.players[0].temporaryLife);
    CHECK(s.players[0].life == 25);
}
