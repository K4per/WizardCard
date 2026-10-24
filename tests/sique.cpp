#include "wizard/application.hpp"
#include "wizard/command_codec.hpp"
#include "wizard/core/card_rules.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;
namespace {
CardCatalog siqueCatalog() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "tests/fixtures/alpha-v251-production").catalog; }
CardCatalog siqueProductionCatalog() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog; }
GameState siqueState(Phase phase = Phase::Main) {
    GameState s; s.phase = phase; s.phaseStep = PhaseStep::Body; s.phaseGate = 1;
    s.globalTurn = 5; s.rng = 37;
    for (auto &p : s.players) { p.mana = 20; p.ownTurn = 2; }
    return s;
}
CardInstance &siquePut(GameState &s, CardId id, const std::string &def, PlayerId p, Zone zone,
                       CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance c; c.id = id; c.definition = def; c.owner = p; c.zone = zone;
    c.host = zone == Zone::Analysis || zone == Zone::Attached ? host : 0;
    c.sourceFormation = host; c.spell = spell;
    if (zone == Zone::Analysis) c.fieldOrder = s.nextFieldOrder++;
    s.cards.emplace(id, c);
    if (zone == Zone::Deck) s.players[p].deck.push_back(id);
    return s.cards.at(id);
}
void siqueSend(GameEngine &e, PlayerId p, const Command &command) {
    auto r = e.submit(p, command); INFO(r.error); REQUIRE(r.accepted);
}
void siqueSettle(GameEngine &e) {
    for (int n = 0; n < 60 && e.state().decision; ++n) {
        const auto d = *e.state().decision;
        if (d.kind == DecisionKind::Response) siqueSend(e, d.player, PassResponse{d.id});
        else if (d.kind == DecisionKind::TriggerOrder) siqueSend(e, d.player, Choose{d.id, d.options.front()});
        else return;
    }
}
void siqueChoose(GameEngine &e, CardId id) {
    siqueSettle(e); REQUIRE(e.state().decision);
    const auto d = *e.state().decision;
    siqueSend(e, d.player, Choose{d.id, id}); siqueSettle(e);
}
void cauldron(GameState &s) { siquePut(s, 1, "sique-cauldron", 0, Zone::Analysis); }
void advanceSique(GameEngine &e) {
    siqueSettle(e); REQUIRE_FALSE(e.state().decision);
    siqueSend(e, e.state().active, AdvancePhase{e.state().phaseGate}); siqueSettle(e);
}
} // namespace

TEST_CASE("Sique pool has all sixteen numbered executable cards", "[sique][content]") {
    const auto cat = siqueCatalog();
    REQUIRE(std::count_if(cat.cards.begin(), cat.cards.end(), [](const auto &entry) {
        return card_rules::member(entry.second);
    }) == 16);
    for (const auto &[id, d] : cat.cards) if (card_rules::member(d)) {
        CAPTURE(id); REQUIRE(d.number.size() == 6); REQUIRE(d.illustrationId == id);
    }
    REQUIRE(cat.at("sique-mushroom-soup").analysisTurns == 2);
    REQUIRE(cat.at("sique-pancakes").analysisTurns == 1);
    REQUIRE(cat.at("sique-divine-stew").analysisTurns == 5);
}

TEST_CASE("Sique woodpile uniqueness and campfire recipe consume the specified materials", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState();
    siquePut(s, 1, "sique-woodpile", 0, Zone::Analysis);
    siquePut(s, 2, "sique-woodpile", 0, Zone::SideDeck);
    siquePut(s, 3, "sique-campfire", 0, Zone::SideDeck);
    siquePut(s, 10, "spark", 0, Zone::Hand);
    siquePut(s, 11, "recall", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, SetFormation{2}).accepted);
    REQUIRE_FALSE(e.submit(0, SetFormation{3, 0, {1, 11}}).accepted);
    siqueSend(e, 0, SetFormation{3, 0, {1, 10}}); siqueSettle(e);
    REQUIRE(e.state().cards.at(1).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(3).zone == Zone::Analysis);
    REQUIRE(Rules::level(e.state(), cat, 3) == 3);
}

TEST_CASE("Sique cauldron requires campfire and two hand cards", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState();
    siquePut(s, 1, "sique-cauldron", 0, Zone::SideDeck);
    siquePut(s, 10, "spark", 0, Zone::Hand); siquePut(s, 11, "recall", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, SetFormation{1, 0, {10, 11}}).accepted);
    siquePut(s, 2, "sique-campfire", 0, Zone::Analysis);
    e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, SetFormation{1, 0, {10, 10}}).accepted);
    siqueSend(e, 0, SetFormation{1, 0, {10, 11}}); siqueSettle(e);
    REQUIRE(e.state().players[0].mana == 18);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
}

TEST_CASE("Sique cauldron restricts activations and protects campfire while campfire boosts fire spells", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 2, "sique-campfire", 0, Zone::Analysis);
    siquePut(s, 10, "recall", 0, Zone::Hand); siquePut(s, 11, "spark", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, PreloadWord{10, true, 2}).accepted);
    siqueSend(e, 0, SetAmbush{10, 2}); siqueSettle(e);
    REQUIRE(e.state().cards.at(10).faceDown);
    siqueSend(e, 0, PreloadWord{11, true, 2}); siqueSettle(e);
    REQUIRE(e.state().players[1].life == 36);
    Trigger destroy; destroy.owner = 1; destroy.targetCard = 2;
    EffectResolver::apply(s, cat, destroy, {EffectKind::Destroy});
    REQUIRE(s.cards.at(2).zone == Zone::Analysis);
    StateMaintenance::leave(s, 1, &cat);
    EffectResolver::apply(s, cat, destroy, {EffectKind::Destroy});
    REQUIRE(s.cards.at(2).zone == Zone::Ash);
}

TEST_CASE("Sique vinegar and sugar supply passives and detached rewards", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 2, "sique-campfire", 0, Zone::Analysis);
    siquePut(s, 10, "sique-vinegar", 0, Zone::Hand);
    siquePut(s, 11, "sique-sugar", 0, Zone::Hand);
    siquePut(s, 12, "sique-pancakes", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, AttachSeal{10, 2}).accepted);
    siqueSend(e, 0, AttachSeal{10, 1}); siqueSettle(e);
    siqueSend(e, 0, AttachSeal{11, 1}); siqueSettle(e);
    REQUIRE(Rules::damage(e.state(), cat, 0, {EffectKind::Damage, 7, DamageType::Fire}) == 5);
    REQUIRE(Rules::analysisCost(e.state(), cat, 12, 1) == 1);
    REQUIRE(Rules::castCost(e.state(), cat, 12) == 0);
    siqueSend(e, 0, RemoveSeal{10}); siqueSettle(e);
    REQUIRE(e.state().players[0].temporaryLife == 4);
    auto detached = e.state(); detached.players[0].sealRemovals = 0;
    e = GameEngine::scenario(cat, detached);
    const int mana = e.state().players[0].mana;
    siqueSend(e, 0, RemoveSeal{11}); siqueSettle(e);
    REQUIRE(e.state().players[0].mana == mana + 2);
    REQUIRE(Rules::castCost(e.state(), cat, 12) == 1);
}

TEST_CASE("Sique salt mills a rune then recovers a spell on detachment", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 10, "sique-salt", 0, Zone::Hand);
    siquePut(s, 20, "sique-sugar", 0, Zone::Deck);
    siquePut(s, 21, "sique-refire", 0, Zone::Ash);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, AttachSeal{10, 1}); siqueChoose(e, 20);
    REQUIRE(e.state().cards.at(20).zone == Zone::Ash);
    siqueSend(e, 0, RemoveSeal{10}); siqueChoose(e, 21);
    REQUIRE(e.state().cards.at(21).zone == Zone::Hand);
}

TEST_CASE("Sique parsley mills a spell then recovers a rune on detachment", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 10, "sique-parsley", 0, Zone::Hand);
    siquePut(s, 20, "sique-refire", 0, Zone::Deck);
    siquePut(s, 21, "sique-sugar", 0, Zone::Ash);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, AttachSeal{10, 1}); siqueChoose(e, 20);
    REQUIRE(e.state().cards.at(20).zone == Zone::Ash);
    siqueSend(e, 0, RemoveSeal{10}); siqueChoose(e, 21);
    REQUIRE(e.state().cards.at(21).zone == Zone::Hand);
}

TEST_CASE("Sique chili fires once per drawn card and again when detached", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    s.active = 1;
    siquePut(s, 2, "small-hexagram", 1, Zone::Analysis);
    siquePut(s, 10, "sique-chili", 0, Zone::Attached, 1);
    siquePut(s, 11, "recall", 1, Zone::Hand);
    siquePut(s, 20, "spark", 1, Zone::Deck); siquePut(s, 21, "spark", 1, Zone::Deck);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 1, PreloadWord{11, true, 2}); siqueSettle(e);
    REQUIRE(e.state().players[1].life == 36);
    s = e.state(); s.active = 0; s.phase = Phase::Main; s.phaseStep = PhaseStep::Body;
    e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, RemoveSeal{10}); siqueSettle(e);
    REQUIRE(e.state().players[1].life == 32);
}

TEST_CASE("Sique soup requires distinct chosen runes and heals after paying them", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(Phase::Cast); cauldron(s);
    s.players[0].life = 20;
    siquePut(s, 10, "sique-vinegar", 0, Zone::Attached, 1);
    siquePut(s, 11, "sique-sugar", 0, Zone::Attached, 1);
    siquePut(s, 12, "sique-chili", 0, Zone::Attached, 1);
    siquePut(s, 20, "sique-mushroom-soup", 0, Zone::Casting, 1, SpellState::Ready);
    auto e = GameEngine::scenario(cat, s);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, ActivateSpell{20}).accepted);
    REQUIRE_FALSE(e.submit(0, ActivateSpell{20, 0, {10, 10}}).accepted);
    REQUIRE(e.digest() == before);
    const auto actions = e.viewFor(0).actions;
    REQUIRE(std::count_if(actions.begin(), actions.end(), [](const LegalAction &action) {
        return std::holds_alternative<ActivateSpell>(action.command) && action.source == 20;
    }) == 3);
    ActivateSpell command{20, 0, {10, 11}};
    REQUIRE(std::get<ActivateSpell>(decodeCommand(encodeCommand(command))).targets == command.targets);
    siqueSend(e, 0, command); siqueSettle(e);
    REQUIRE(e.state().players[0].life == 24);
    REQUIRE(e.state().players[0].temporaryLife == 6);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).zone == Zone::Attached);
}

TEST_CASE("Sique pancakes cap individual spell and rules damage until next own prepare begins", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(Phase::Cast); cauldron(s);
    siquePut(s, 10, "sique-sugar", 0, Zone::Attached, 1);
    siquePut(s, 11, "sique-sugar", 0, Zone::Attached, 1);
    siquePut(s, 20, "sique-pancakes", 0, Zone::Casting, 1, SpellState::Ready);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, ActivateSpell{20, 0, {10, 11}}); siqueSettle(e);
    REQUIRE(Rules::damage(e.state(), cat, 0, {EffectKind::Damage, 30}) == 4);
    auto capped = e.state();
    ResourceRules::damage(capped, 0, 15, "overload"); REQUIRE(capped.players[0].life == 36);
    capped.phase = Phase::Draw; capped.phaseStep = PhaseStep::Body; capped.active = 1;
    capped.globalTurn = 6; capped.players[1].ownTurn = 3;
    e = GameEngine::scenario(cat, capped); advanceSique(e);
    REQUIRE(e.state().players[0].siqueDamageCap == 4);
    capped = e.state(); capped.phase = Phase::Draw; capped.phaseStep = PhaseStep::Body;
    capped.active = 0; capped.globalTurn = 7; capped.players[0].ownTurn = 3;
    e = GameEngine::scenario(cat, capped); advanceSique(e);
    REQUIRE(e.state().phase == Phase::Prepare);
    REQUIRE(e.state().players[0].siqueDamageCap == 0);
}

TEST_CASE("Sique divine stew replaces mana costs with opponent life only in the current turn", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(Phase::Cast); cauldron(s);
    for (CardId id : {10u, 11u, 12u}) siquePut(s, id, "sique-vinegar", 0, Zone::Attached, 1);
    siquePut(s, 20, "sique-divine-stew", 0, Zone::Casting, 1, SpellState::Ready);
    siquePut(s, 21, "spark", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, ActivateSpell{20, 0, {10, 11, 12}}); siqueSettle(e);
    auto paid = e.state(); paid.players[0].mana = 0;
    e = GameEngine::scenario(cat, paid);
    siqueSend(e, 0, PreloadWord{21, true, 1}); siqueSettle(e);
    REQUIRE(e.state().players[0].mana == 0);
    REQUIRE(e.state().players[1].life == 35);
    ++paid.globalTurn; paid.cards.at(21).zone = Zone::Hand;
    e = GameEngine::scenario(cat, paid);
    REQUIRE_FALSE(e.submit(0, PreloadWord{21, true, 1}).accepted);
    paid.globalTurn = 5; paid.players[1].life = 1;
    e = GameEngine::scenario(cat, paid);
    siqueSend(e, 0, PreloadWord{21, true, 1});
    REQUIRE(e.state().result == 0);
    REQUIRE_FALSE(e.state().chain);
}

TEST_CASE("Sique cauldron leaves response flip and prepare as setup operations", "[sique]") {
    auto cat = siqueCatalog();
    cat.cards.at("ray-of-frost").speed = 2;
    auto s = siqueState(); s.active = 1; s.globalTurn = 6;
    siquePut(s, 1, "sique-cauldron", 0, Zone::Analysis);
    siquePut(s, 2, "small-hexagram", 1, Zone::Analysis);
    siquePut(s, 10, "ray-of-frost", 0, Zone::Analysis, 1, SpellState::None).faceDown = true;
    auto &hidden = s.cards.at(10); hidden.wasAmbushed = true; hidden.ambushedTurn = 4;
    siquePut(s, 20, "spark", 1, Zone::Hand);
    siquePut(s, 21, "ray-of-frost", 0, Zone::Analysis, 1, SpellState::Ready);
    s.cards.at(21).wasAmbushed = true; s.cards.at(21).ambushedTurn = 4;
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 1, PreloadWord{20, true, 2});
    const auto actions = e.viewFor(0).actions;
    REQUIRE(std::any_of(actions.begin(), actions.end(), [](const LegalAction &action) {
        const auto *flip = std::get_if<FlipAmbush>(&action.command);
        return flip && flip->card == 10;
    }));
    REQUIRE(std::any_of(actions.begin(), actions.end(), [](const LegalAction &action) {
        const auto *prepare = std::get_if<PrepareCast>(&action.command);
        return prepare && prepare->card == 21;
    }));
    siqueSend(e, 0, FlipAmbush{10, 0, {}, e.state().decision->id}); siqueSettle(e);
    REQUIRE_FALSE(e.state().cards.at(10).faceDown);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Analyzing);

    s.active = 0; s.phase = Phase::Cast;
    s.cards.at(21).zone = Zone::Casting; s.cards.at(21).host = 0;
    e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, ActivateSpell{21}).accepted);
}

TEST_CASE("Sique divine stew replaces mana only; activation still adds load and overload damage", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(Phase::Cast); cauldron(s);
    s.players[0].mana = 0; s.players[0].siqueLifePaymentTurn = s.globalTurn;
    s.temporary.push_back({1, 0, 0, 13, 0, true, false});
    siquePut(s, 10, "fireball", 0, Zone::Casting, 1, SpellState::Ready);
    siquePut(s, 2, "octagram", 1, Zone::Analysis);
    auto &counter = siquePut(s, 11, "counter-spell", 1, Zone::Analysis, 2);
    counter.faceDown = counter.wasAmbushed = true; counter.ambushedTurn = 1;
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, ActivateSpell{10});
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->kind == DecisionKind::Response);
    REQUIRE(e.state().players[0].mana == 0);
    REQUIRE(e.state().players[0].life == 38);
    REQUIRE(e.state().players[1].life == 39);
    REQUIRE(Rules::load(e.state(), 0) == 14);
    siqueSettle(e);
    REQUIRE(e.state().players[1].life == 29);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Resolved);
    REQUIRE(Rules::load(e.state(), 0) == 14);
}

TEST_CASE("Sique ingredients choose two runes and dispose the partner only in the search turn", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 10, "sique-ingredients", 0, Zone::Hand);
    siquePut(s, 20, "sique-vinegar", 0, Zone::Deck); siquePut(s, 21, "sique-sugar", 0, Zone::Deck);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, PreloadWord{10, true, 1}); siqueChoose(e, 20); siqueChoose(e, 21);
    REQUIRE(e.state().cards.at(20).zone == Zone::Hand);
    REQUIRE(e.state().cards.at(21).zone == Zone::Hand);
    const auto searched = e.state();
    siqueSend(e, 0, AttachSeal{20, 1}); siqueSettle(e);
    REQUIRE(e.state().cards.at(21).zone == Zone::Ash);
    auto later = searched; ++later.globalTurn;
    e = GameEngine::scenario(cat, later);
    siqueSend(e, 0, AttachSeal{20, 1}); siqueSettle(e);
    REQUIRE(e.state().cards.at(21).zone == Zone::Hand);
}

TEST_CASE("Sique flavor shift pays a detached rune then chooses deck rune and Sique formation", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 2, "sique-campfire", 0, Zone::Analysis);
    siquePut(s, 10, "sique-vinegar", 0, Zone::Attached, 1);
    siquePut(s, 20, "sique-flavor-shift", 0, Zone::Hand);
    siquePut(s, 21, "sique-sugar", 0, Zone::Deck);
    auto e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, PreloadWord{20, true, 1}).accepted);
    siqueSend(e, 0, PreloadWord{20, true, 1, 0, {10}}); siqueChoose(e, 21); siqueChoose(e, 2);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(21).zone == Zone::Attached);
    REQUIRE(e.state().cards.at(21).host == 2);
    REQUIRE(e.state().players[0].temporaryLife == 4);
}

TEST_CASE("Sique refire recycles up to five cards to the correct deck then offers one draw", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 10, "sique-refire", 0, Zone::Hand);
    siquePut(s, 20, "sique-woodpile", 0, Zone::Ash);
    for (CardId id = 21; id <= 25; ++id) siquePut(s, id, "sique-sugar", 0, Zone::Ash);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, PreloadWord{10, true, 1});
    for (CardId id = 20; id <= 24; ++id) siqueChoose(e, id);
    REQUIRE(e.state().cards.at(20).zone == Zone::SideDeck);
    REQUIRE(e.state().cards.at(25).zone == Zone::Ash);
    REQUIRE(e.state().players[0].deck.size() == 4);
    const auto before = e.viewFor(0).actions;
    REQUIRE(std::any_of(before.begin(), before.end(), [](const LegalAction &a) { return a.label == "抽取效果指定数量的牌"; }));
    siqueChoose(e, 0xffffffe1u);
    REQUIRE(e.state().players[0].deck.size() == 3);
}

TEST_CASE("Sique boiling surge counters only an opponent spell threatening a Sique formation", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(Phase::Cast); cauldron(s);
    s.active = 1;
    siquePut(s, 2, "small-hexagram", 1, Zone::Analysis);
    auto &surge = siquePut(s, 10, "sique-boiling-surge", 0, Zone::Casting, 1, SpellState::Ready);
    surge.wasAmbushed = true; surge.ambushedTurn = 1;
    siquePut(s, 20, "unravel", 1, Zone::Casting, 2, SpellState::Ready);
    siquePut(s, 21, "small-hexagram", 1, Zone::SideDeck);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 1, ActivateSpell{20, 1});
    REQUIRE(e.state().decision); REQUIRE(e.state().decision->kind == DecisionKind::Response);
    const auto link = e.state().chain->links.front().id;
    siqueSend(e, 0, Respond{e.state().decision->id, 10, "boiling_surge", 0, link}); siqueSettle(e);
    REQUIRE(e.state().cards.at(1).zone == Zone::Analysis);
    REQUIRE(e.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(e.state().players[1].life == 30);
    REQUIRE(Rules::load(e.state(), 0) == 4);
    s.cards.at(20).definition = "fireball";
    e = GameEngine::scenario(cat, s); siqueSend(e, 1, ActivateSpell{20}); siqueSettle(e);
    REQUIRE(e.state().players[0].life < 40);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Ready);
}

TEST_CASE("Sique aroma lure requires cauldron and gives each player an independent redraw choice", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(Phase::Cast);
    siquePut(s, 1, "hexagram", 0, Zone::Analysis);
    siquePut(s, 10, "sique-aroma-lure", 0, Zone::Casting, 1, SpellState::Ready);
    for (CardId id = 20; id < 22; ++id) siquePut(s, id, "spark", 0, Zone::Hand);
    for (CardId id = 30; id < 33; ++id) siquePut(s, id, "spark", 1, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, ActivateSpell{10}).accepted);
    s.cards.at(1).definition = "sique-cauldron";
    e = GameEngine::scenario(cat, s); siqueSend(e, 0, ActivateSpell{10}); siqueSettle(e);
    REQUIRE(e.state().players[0].deck.size() == 2);
    REQUIRE(e.state().players[1].deck.size() == 3);
    siqueChoose(e, 0xffffffe1u); siqueChoose(e, 0);
    REQUIRE(e.state().players[0].deck.empty());
    REQUIRE(e.state().players[1].deck.size() == 3);
    REQUIRE(e.viewFor(0).players[0].handCount == 2);
    REQUIRE(e.viewFor(1).players[1].handCount == 0);
}

TEST_CASE("Sique cauldron detaches all runes at own end phase start", "[sique]") {
    const auto cat = siqueCatalog(); auto s = siqueState(Phase::Cast); cauldron(s);
    siquePut(s, 10, "sique-vinegar", 0, Zone::Attached, 1);
    siquePut(s, 11, "sique-sugar", 0, Zone::Attached, 1);
    siquePut(s, 12, "sique-chili", 0, Zone::Attached, 1);
    auto e = GameEngine::scenario(cat, s); advanceSique(e);
    REQUIRE(e.state().phase == Phase::End);
    for (CardId id : {10u, 11u, 12u}) REQUIRE(e.state().cards.at(id).zone == Zone::Ash);
    REQUIRE(e.state().players[0].temporaryLife == 4);
    REQUIRE(e.state().players[0].mana == maximumMana);
    REQUIRE(e.state().players[1].life == 36);
}

TEST_CASE("Alpha 2.5.3 Sique selections require one through five cards and handle empty choices", "[sique][alpha-v253]") {
    const auto cat = siqueProductionCatalog();
    REQUIRE(cat.usesAlphaV252());

    auto s = siqueState(Phase::Cast); cauldron(s);
    siquePut(s, 20, "sique-backfire", 0, Zone::Casting, 1, SpellState::Ready);
    siquePut(s, 30, "sique-sugar", 1, Zone::Ash);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, ActivateSpell{20});
    REQUIRE(e.state().decision);
    REQUIRE_FALSE(e.state().decision->mayPass);
    const auto before = e.digest();
    const auto decision = *e.state().decision;
    REQUIRE_FALSE(e.submit(0, Choose{decision.id, 0}).accepted);
    REQUIRE(e.digest() == before);
    siqueChoose(e, 30);
    REQUIRE(e.state().cards.at(30).zone == Zone::Deck);
    REQUIRE(e.state().players[1].life == 39);

    s = siqueState(); cauldron(s);
    siquePut(s, 20, "sique-refire", 0, Zone::Hand);
    siquePut(s, 30, "sique-sugar", 0, Zone::Ash);
    siquePut(s, 31, "sique-salt", 0, Zone::Ash);
    siquePut(s, 40, "spark", 0, Zone::Deck);
    e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, PreloadWord{20, true, 1});
    REQUIRE(e.state().decision);
    REQUIRE_FALSE(e.state().decision->mayPass);
    const auto firstChoice = *e.state().decision;
    REQUIRE_FALSE(e.submit(0, Choose{firstChoice.id, 0}).accepted);
    siqueChoose(e, 30);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->mayPass);
    siqueChoose(e, 0);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->mayPass);
    siqueChoose(e, 0);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(30).zone == Zone::Deck);
    REQUIRE(e.state().cards.at(31).zone == Zone::Ash);

    s = siqueState(); cauldron(s);
    siquePut(s, 20, "sique-refire", 0, Zone::Hand);
    siquePut(s, 40, "spark", 0, Zone::Deck);
    e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, PreloadWord{20, true, 1});
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->mayPass);
    REQUIRE(e.state().decision->options == std::vector<CardId>{0xffffffe1u});
    siqueChoose(e, 0);
    REQUIRE_FALSE(e.state().decision);

    s = siqueState(Phase::Cast); cauldron(s); s.players[0].life = 20;
    siquePut(s, 20, "sique-backfire", 0, Zone::Casting, 1, SpellState::Ready);
    for (CardId id = 30; id < 36; ++id) siquePut(s, id, "sique-sugar", 1, Zone::Ash);
    e = GameEngine::scenario(cat, s); siqueSend(e, 0, ActivateSpell{20});
    for (CardId id = 30; id < 35; ++id) siqueChoose(e, id);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(35).zone == Zone::Ash);
    REQUIRE(e.state().players[1].life == 35);
    REQUIRE(e.state().players[0].life == 30);

    s = siqueState(Phase::Cast); cauldron(s);
    siquePut(s, 20, "sique-backfire", 0, Zone::Casting, 1, SpellState::Ready);
    e = GameEngine::scenario(cat, s); siqueSend(e, 0, ActivateSpell{20}); siqueSettle(e);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().players[1].life == startingLife);
}

TEST_CASE("Alpha 2.5.3 Sique flavor shift can only attach from deck to the cauldron", "[sique][alpha-v253]") {
    const auto cat = siqueProductionCatalog();
    auto s = siqueState(); cauldron(s);
    siquePut(s, 2, "sique-campfire", 0, Zone::Analysis);
    siquePut(s, 10, "sique-vinegar", 0, Zone::Attached, 1);
    siquePut(s, 20, "sique-flavor-shift", 0, Zone::Hand);
    siquePut(s, 21, "sique-sugar", 0, Zone::Deck);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, PreloadWord{20, true, 1, 0, {10}});
    siqueChoose(e, 21);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->options == std::vector<CardId>{1});
    const auto decision = *e.state().decision;
    REQUIRE_FALSE(e.submit(0, Choose{decision.id, 2}).accepted);
    siqueChoose(e, 1);
    REQUIRE(e.state().cards.at(21).zone == Zone::Attached);
    REQUIRE(e.state().cards.at(21).host == 1);
}

TEST_CASE("Alpha 2.5.3 soup pays exactly one owned rune and sugar leaves for one mana", "[sique][alpha-v253]") {
    const auto cat = siqueProductionCatalog();
    auto s = siqueState(Phase::Cast); cauldron(s); s.players[0].life = 20;
    siquePut(s, 2, "hexagram", 1, Zone::Analysis);
    siquePut(s, 10, "sique-sugar", 0, Zone::Attached, 1);
    siquePut(s, 11, "sique-vinegar", 1, Zone::Attached, 2);
    siquePut(s, 20, "sique-mushroom-soup", 0, Zone::Casting, 1, SpellState::Ready);
    auto e = GameEngine::scenario(cat, s);
    const auto before = e.digest();
    for (const auto &ids : {std::vector<CardId>{}, std::vector<CardId>{10, 10}, std::vector<CardId>{11}}) {
        REQUIRE_FALSE(e.submit(0, ActivateSpell{20, 0, ids}).accepted);
        REQUIRE(e.digest() == before);
    }
    siqueSend(e, 0, ActivateSpell{20, 0, {10}}); siqueSettle(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(11).zone == Zone::Attached);
    REQUIRE(e.state().players[0].life == 24);
    REQUIRE(e.state().players[0].temporaryLife == 6);

    s = siqueState(); cauldron(s); s.players[0].mana = 7;
    siquePut(s, 10, "sique-sugar", 0, Zone::Attached, 1);
    e = GameEngine::scenario(cat, s); siqueSend(e, 0, RemoveSeal{10}); siqueSettle(e);
    REQUIRE(e.state().players[0].mana == 8);
}

TEST_CASE("Alpha 2.5.3 ingredients tracks the searched instances rather than matching names", "[sique][alpha-v253]") {
    const auto cat = siqueProductionCatalog(); auto s = siqueState(); cauldron(s);
    siquePut(s, 10, "sique-ingredients", 0, Zone::Hand);
    siquePut(s, 20, "sique-sugar", 0, Zone::Deck); siquePut(s, 21, "sique-sugar", 0, Zone::Deck);
    siquePut(s, 22, "sique-sugar", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s); siqueSend(e, 0, PreloadWord{10, true, 1});
    siqueChoose(e, 20); siqueChoose(e, 21);
    const auto searched = e.state();
    REQUIRE(searched.cards.at(20).zone == Zone::Hand); REQUIRE(searched.cards.at(21).zone == Zone::Hand);
    siqueSend(e, 0, AttachSeal{20, 1}); siqueSettle(e);
    REQUIRE(e.state().cards.at(21).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(22).zone == Zone::Hand);
    auto later = searched; ++later.globalTurn;
    e = GameEngine::scenario(cat, later); siqueSend(e, 0, AttachSeal{20, 1}); siqueSettle(e);
    REQUIRE(e.state().cards.at(21).zone == Zone::Hand);
    REQUIRE(e.state().cards.at(22).zone == Zone::Hand);
}

TEST_CASE("Alpha 2.5.3 divine stew substitutes only mana at declaration and retains opponent life", "[sique][alpha-v253]") {
    const auto cat = siqueProductionCatalog();
    auto s = siqueState(Phase::Cast); cauldron(s);
    for (CardId id : {10u, 11u, 12u}) siquePut(s, id, "sique-vinegar", 0, Zone::Attached, 1);
    siquePut(s, 20, "sique-divine-stew", 0, Zone::Casting, 1, SpellState::Ready);
    auto e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, ActivateSpell{20, 0, {10, 11, 12}}); siqueSettle(e);
    for (CardId id : {10u, 11u, 12u}) REQUIRE(e.state().cards.at(id).zone == Zone::Ash);
    REQUIRE(e.state().players[0].siqueLifePaymentTurn == s.globalTurn);
    s = e.state(); s.phase = Phase::Main; s.phaseStep = PhaseStep::Body;
    s.players[0].mana = 0; s.players[1].life = 3; s.players[1].temporaryLife = 20;
    siquePut(s, 21, "sique-ingredients", 0, Zone::Hand);
    e = GameEngine::scenario(cat, s);
    siqueSend(e, 0, PreloadWord{21, true, 1});
    // Declaration pays exactly two ordinary life before any effect choices.
    REQUIRE(e.state().players[1].life == 1);
    REQUIRE(e.state().players[1].temporaryLife == 20);
    REQUIRE(e.state().players[0].mana == 0);
    siqueSettle(e); REQUIRE(e.state().players[1].life == 1);

    s.players[1].life = 2;
    e = GameEngine::scenario(cat, s); const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, PreloadWord{21, true, 1}).accepted);
    REQUIRE(e.digest() == before);
    ++s.globalTurn; s.players[1].life = 40;
    e = GameEngine::scenario(cat, s);
    REQUIRE_FALSE(e.submit(0, PreloadWord{21, true, 1}).accepted);
}

TEST_CASE("Alpha 2.5.3 divine stew respects independent infinite resource switches", "[sique][alpha-v253]") {
    const auto cat = siqueProductionCatalog(); auto s = siqueState();
    s.players[0].mana = 0; s.players[0].siqueLifePaymentTurn = s.globalTurn;
    s.players[1].life = 1;
    REQUIRE_FALSE(card_rules::canPay(s, 0, 2, &cat));
    s.players[1].debugInfiniteLife = true;
    REQUIRE(card_rules::canPay(s, 0, 2, &cat)); card_rules::pay(s, 0, 2, &cat);
    REQUIRE(s.players[1].life == 1); REQUIRE(s.players[0].mana == 0);
    s.players[1].debugInfiniteLife = false; s.players[0].debugInfiniteMana = true;
    REQUIRE(card_rules::canPay(s, 0, 2, &cat)); card_rules::pay(s, 0, 2, &cat);
    REQUIRE(s.players[1].life == 1); REQUIRE(s.players[0].mana == 0);
    s.players[0].debugInfiniteMana = false;
    REQUIRE_FALSE(card_rules::canPay(s, 0, 2, &cat));
}

TEST_CASE("Alpha 2.5.3 hanged man pays rounded ordinary life and exchanges only nonformations", "[arcana][alpha-v253]") {
    auto cat = siqueProductionCatalog(); cat.cards.at("hexagram").capacity = 50;
    auto s = siqueState(Phase::Cast); s.players[0].life = 11; s.players[0].temporaryLife = 30;
    siquePut(s, 1, "hexagram", 0, Zone::Analysis);
    siquePut(s, 20, "arcana-hanged-man", 0, Zone::Casting, 1, SpellState::Ready);
    siquePut(s, 30, "spark", 0, Zone::Deck); siquePut(s, 31, "recall", 0, Zone::Deck);
    siquePut(s, 40, "mend", 0, Zone::Ash); siquePut(s, 41, "hexagram", 0, Zone::Ash);
    siquePut(s, 50, "spark", 1, Zone::Deck); siquePut(s, 60, "mend", 1, Zone::Ash);
    auto e = GameEngine::scenario(cat, s); siqueSend(e, 0, ActivateSpell{20});
    REQUIRE(e.state().players[0].life == 5); REQUIRE(e.state().players[0].temporaryLife == 30);
    siqueSettle(e);
    REQUIRE(e.state().cards.at(41).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(40).zone == Zone::Deck);
    REQUIRE(e.state().cards.at(30).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(31).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(50).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(60).zone == Zone::Deck);
    REQUIRE(e.state().players[0].deck == std::vector<CardId>{40});

    s.players[0].life = 1;
    e = GameEngine::scenario(cat, s); siqueSend(e, 0, ActivateSpell{20});
    REQUIRE(e.state().players[0].life == 0); REQUIRE(e.state().result == 1);
    REQUIRE(e.state().cards.at(40).zone == Zone::Ash); // The cost ends the match before the exchange.
    s.players[0].debugInfiniteLife = true;
    e = GameEngine::scenario(cat, s); siqueSend(e, 0, ActivateSpell{20}); siqueSettle(e);
    REQUIRE(e.state().players[0].life == 1); REQUIRE(e.state().players[0].temporaryLife == 30);
}

TEST_CASE("Alpha 2.5.3 magician lets the opponent choose from three revealed distinct names", "[arcana][alpha-v253]") {
    const auto cat = siqueProductionCatalog(); auto s = siqueState(Phase::Cast);
    siquePut(s, 1, "hexagram", 0, Zone::Analysis);
    siquePut(s, 20, "arcana-magician", 0, Zone::Casting, 1, SpellState::Ready);
    siquePut(s, 30, "spark", 0, Zone::Deck); siquePut(s, 31, "recall", 0, Zone::Deck);
    siquePut(s, 32, "mend", 0, Zone::Deck); siquePut(s, 33, "spark", 0, Zone::Deck);
    auto e = GameEngine::scenario(cat, s); siqueSend(e, 0, ActivateSpell{20});
    siqueChoose(e, 30);
    REQUIRE(e.state().decision);
    REQUIRE(std::find(e.state().decision->options.begin(), e.state().decision->options.end(), 33) == e.state().decision->options.end());
    siqueChoose(e, 31); siqueChoose(e, 32);
    REQUIRE(e.state().decision); REQUIRE(e.state().decision->player == 1);
    REQUIRE(e.state().decision->options == std::vector<CardId>{30, 31, 32});
    const auto decision = *e.state().decision; const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, Choose{decision.id, 31}).accepted);
    REQUIRE_FALSE(e.submit(1, Choose{decision.id, 33}).accepted);
    REQUIRE(e.digest() == before);
    siqueChoose(e, 31);
    REQUIRE(e.state().cards.at(31).zone == Zone::Hand);
    REQUIRE(e.state().cards.at(30).zone == Zone::Exile);
    REQUIRE(e.state().cards.at(32).zone == Zone::Exile);
    REQUIRE(e.state().cards.at(33).zone == Zone::Deck);
}
