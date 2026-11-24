#include "support/observed_engine.hpp"
#include "wizard/content.hpp"
#include "wizard/core/spell_rules.hpp"
#include "wizard/core/card_rules.hpp"
#include "wizard/core/action_query.hpp"
#include "wizard/decks.hpp"
#include "wizard/network.hpp"
#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>

using namespace wizard;
namespace {
CardCatalog feedbackCatalog() {
    static const auto catalog = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/release-v260").catalog;
    return catalog;
}

MutationContext feedbackState() {
    MutationContext s;
    s.phase = Phase::Main; s.phaseStep = PhaseStep::Body; s.flow = Flow::Main;
    s.phaseGate = 1; s.nextDecision = 2; s.globalTurn = 5; s.rng = 73;
    for (auto &p : s.players) { p.mana = 20; p.ownTurn = 2; }
    for (int p = 0; p < 2; ++p) {
        CardInstance c; c.id = static_cast<CardId>(p + 1); c.owner = p;
        c.definition = "v260-formation-0017"; c.zone = Zone::Analysis;
        c.fieldOrder = s.nextFieldOrder++; s.cards[c.id] = c;
    }
    return s;
}
CardInstance &feedbackCard(MutationContext &s, CardId id, const char *definition, PlayerId owner,
                           Zone zone, SpellState spell = SpellState::None) {
    CardInstance c; c.id = id; c.owner = owner; c.definition = definition; c.zone = zone; c.spell = spell;
    if (zone == Zone::Analysis) {
        c.host = c.sourceFormation = static_cast<CardId>(owner + 1);
        c.ringIndex = c.sourceRingIndex = Rules::freeRing(s, feedbackCatalog(), c.host);
        c.fieldOrder = s.nextFieldOrder++;
    }
    s.cards[id] = c;
    if (zone == Zone::Deck) s.players[owner].deck.push_back(id);
    return s.cards.at(id);
}
void feedbackSend(test::ObservedEngine &e, PlayerId p, const Command &c) {
    const auto result = e.submit(p, c); INFO(result.error); REQUIRE(result.accepted);
}
void feedbackPass(test::ObservedEngine &e) {
    for (int i = 0; i < 60 && e.state().decision && e.state().decision->kind == DecisionKind::Response; ++i) {
        const auto d = *e.state().decision; feedbackSend(e, d.player, PassResponse{d.id});
    }
}
}

TEST_CASE("v262 overload charges only the newly excessive load", "[alpha-v262][resources]") {
    auto cat = feedbackCatalog(); auto s = feedbackState();
    const int cap = Rules::capacity(s, cat, 0);
    s.temporary.push_back({s.nextLoad++, 0, 0, cap - 1, 0, true});
    Trigger t; t.owner = 0;
    EffectResolver::apply(s, cat, t, {EffectKind::AddIndependent, 2});
    CHECK(Rules::load(s, 0) == cap + 1);
    CHECK(s.players[0].life == 38);
    EffectResolver::apply(s, cat, t, {EffectKind::AddIndependent, 2});
    CHECK(s.players[0].life == 34);
}

TEST_CASE("v262 true damage bypasses temporary life and ordinary prevention", "[alpha-v262][resources]") {
    auto cat = feedbackCatalog(); auto s = feedbackState();
    ResourceRules::grantTemporaryLife(s, 0, 0, 10, 3);
    s.players[0].nextDamageImmune = true; s.players[0].siqueDamageCap = 1;
    ResourceRules::damage(s, 0, 6, "overload", &cat);
    CHECK(s.players[0].life == 34);
    CHECK(s.players[0].temporaryLife == 10);
    CHECK(s.players[0].nextDamageImmune);
    ResourceRules::damage(s, 0, 80, "exhaustion", &cat);
    CHECK(s.players[0].life == 0);
}

TEST_CASE("v262 ordinary lethal damage clamps life at zero", "[alpha-v262][resources]") {
    auto cat = feedbackCatalog(); auto s = feedbackState(); s.players[1].life = 3;
    Trigger t; t.owner = 0; t.targetPlayer = 1;
    EffectResolver::apply(s, cat, t, {EffectKind::Damage, 10});
    CHECK(s.players[1].life == 0);
}

TEST_CASE("v262 true damage enters actual damage watchers without an elemental attribute", "[alpha-v262][damage-watchers]") {
    auto s=feedbackState(); const auto cat=feedbackCatalog();
    feedbackCard(s,10,"v260-talisman-0007",0,Zone::Analysis,SpellState::Active);
    feedbackCard(s,11,"v260-analytic-0017",0,Zone::Analysis,SpellState::Active);
    feedbackCard(s,12,"v260-talisman-0001",0,Zone::Casting,SpellState::Ready);
    ResourceRules::damage(s,0,4,"overload",&cat);
    CHECK(s.players[0].life==36); CHECK(s.cards.at(10).damageCounter==1);
    REQUIRE(s.queue.items.size()==2);
    CHECK(s.queue.items[0].effects.front().kind==EffectKind::Damage);
    CHECK(s.queue.items[1].damageAmount==4);
    CHECK(s.players[0].mana==20);
}

TEST_CASE("v262 concentration upkeep pays mana without increasing load", "[alpha-v262][concentration]") {
    auto s = feedbackState();
    auto &c = feedbackCard(s, 10, "v260-analytic-0017", 0, Zone::Analysis, SpellState::Active);
    c.concentrationCost = 1; c.castLoad = 3;
    s.maintenance = ConcentrationContinuation{10};
    s.decision = PendingDecision{2, 0, DecisionKind::Concentration, {ChoiceValue::card(10)}, true};
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
    const auto load = Rules::load(e.state(), 0);
    feedbackSend(e, 0, Choose{2, ChoiceValue::card(10)});
    CHECK(e.state().players[0].mana == 19);
    CHECK(Rules::load(e.state(), 0) == load);
}

TEST_CASE("v262 timers do not decrement on the opponent end phase", "[alpha-v262][timers]") {
    auto s = feedbackState();
    feedbackCard(s, 10, "v260-talisman-0012", 0, Zone::Analysis, SpellState::Active).remaining = 2;
    s.active = 1; s.phase = Phase::End;
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
    feedbackSend(e, 1, AdvancePhase{e.state().phaseGate}); feedbackPass(e);
    CHECK(e.state().cards.at(10).remaining == 2);
}

TEST_CASE("v262 sunburst speed and death ward text match feedback", "[alpha-v262][content]") {
    const auto cat = feedbackCatalog();
    CHECK(cat.at("v260-analytic-0009").speed == 2);
    CHECK(cat.at("v260-analytic-0023").text.find("免疫那次伤害") != std::string::npos);
}

TEST_CASE("v262 passive damage watchers have real resource effects", "[alpha-v262][damage-watchers]") {
    const auto cat = feedbackCatalog();
    for (const auto definition : {"v260-talisman-0007", "v260-analytic-0017", "v260-analytic-0031"}) {
        CAPTURE(definition); auto s = feedbackState(); s.players[1].mana = 10;
        feedbackCard(s, 10, definition, 1, Zone::Analysis, SpellState::Active).remaining = 3;
        feedbackCard(s, 20, "v260-analytic-0001", 0, Zone::Casting, SpellState::Ready);
        s.phase = Phase::Cast;
        auto e = test::ObservedEngine::scenario(cat, s);
        feedbackSend(e, 0, ActivateSpell{20, playerTarget(1)}); feedbackPass(e);
        CHECK(e.state().players[1].life < 40);
        if (std::string(definition) == "v260-analytic-0017") CHECK(e.state().players[1].mana == 11);
        else CHECK(e.state().players[0].life < 40);
    }
}

TEST_CASE("v262 death ward consumes itself before lethal damage and ignores true damage", "[alpha-v262][death-ward]") {
    const auto cat = feedbackCatalog(); auto s = feedbackState(); s.players[1].life = 4;
    feedbackCard(s, 10, "v260-analytic-0023", 1, Zone::Analysis, SpellState::Active);
    Trigger t; t.owner = 0; t.targetPlayer = 1;
    EffectResolver::apply(s, cat, t, {EffectKind::Damage, 20});
    CHECK(s.players[1].life == 1); CHECK(s.cards.at(10).zone == Zone::Exile); CHECK(s.result == -1);
    ResourceRules::damage(s, 1, 2, "overload", &cat); CHECK(s.players[1].life == 0);
}

TEST_CASE("Fireball feedback separates prevented damage from the Death Ward life change", "[hotfix-v08]") {
    const auto catalog = feedbackCatalog();
    for (const bool ward : {false, true}) {
        CAPTURE(ward);
        auto state = feedbackState();
        state.players[1].life = 4;
        ResourceRules::grantTemporaryLife(state, 1, 0, 1, 3);
        const auto &fireball = feedbackCard(state, 20, "v260-analytic-0001", 0, Zone::Casting, SpellState::Ready);
        if (ward) feedbackCard(state, 10, "v260-analytic-0023", 1, Zone::Analysis, SpellState::Active);
        Trigger trigger;
        trigger.owner = 0; trigger.source = fireball.id; trigger.targetPlayer = 1;
        EffectResolver::apply(state, catalog, trigger, catalog.at(fireball.definition).effects.front());
        const auto result = std::find_if(state.events.begin(), state.events.end(), [](const GameEvent &event) {
            return event.kind == "effect" && event.effectType == EffectKind::Damage && event.card == 20;
        });
        REQUIRE(result != state.events.end());
        const int damage = ward ? 0 : 5;
        CHECK(result->actualAmount == damage);
        CHECK(result->text.find(std::to_string(damage) + " 点") != std::string::npos);
        CHECK(state.players[1].life == (ward ? 1 : 0));
        CHECK(state.players[1].temporaryLife == (ward ? 1 : 0));
        CHECK(std::any_of(state.events.begin(), state.events.end(), [ward](const GameEvent &event) {
            return event.kind == "life_delta" && event.affectedPlayer == 1 && event.actualAmount == (ward ? -3 : -4) &&
                bridge::eventDto(event).value("damageType", std::string{}) == (ward ? "" : "fire");
        }));
    }
}

TEST_CASE("Typed life deltas survive local, recording and network event projection", "[damage-feedback]") {
    const auto catalog = feedbackCatalog();
    const std::pair<DamageType, const char *> types[] = {
        {DamageType::Fire, "fire"}, {DamageType::Cold, "cold"}, {DamageType::Acid, "acid"},
        {DamageType::Radiant, "radiant"}, {DamageType::Necrotic, "necrotic"}, {DamageType::Poison, "poison"},
        {DamageType::Lightning, "lightning"}, {DamageType::Psychic, "psychic"}, {DamageType::Thunder, "thunder"},
        {DamageType::Force, "force"}, {DamageType::Slashing, "slashing"}, {DamageType::Bludgeoning, "bludgeoning"},
        {DamageType::Piercing, "piercing"},
    };
    for (const auto &[type, id] : types) {
        CAPTURE(id);
        auto state = feedbackState();
        ResourceRules::grantTemporaryLife(state, 0, 0, 3, 3);
        Trigger trigger;
        trigger.owner = 1;
        trigger.targetPlayer = 0;
        EffectResolver::apply(state, catalog, trigger, {EffectKind::Damage, 1, type}); // Fully absorbed.
        EffectResolver::apply(state, catalog, trigger, {EffectKind::Damage, 5, type}); // Two shield, three life.
        state.players[0].nextDamageImmune = true;
        EffectResolver::apply(state, catalog, trigger, {EffectKind::Damage, 5, type});
        const EventBatch batch{1, 0, state.events.size(), false, state.events};
        const auto received = net::decodeEvents(net::encodeEvents(batch));
        REQUIRE(received.events.size() == state.events.size());
        int lifeDeltas = 0;
        for (std::size_t i = 0; i < state.events.size(); ++i) {
            const auto &event = state.events[i];
            const auto local = bridge::eventDto(event);
            CHECK(bridge::eventDto(received.events[i]) == local);
            if (event.kind == "life_delta") {
                ++lifeDeltas;
                CHECK(local.at("affectedPlayer") == 0);
                CHECK(local.at("actualAmount") == -3);
                CHECK(local.value("damageType", std::string{}) == id);
                CHECK(encodeEvent(event).value("damageType", std::string{}) == id);
            } else {
                CHECK_FALSE(local.contains("damageType"));
            }
        }
        CHECK(lifeDeltas == 1);
    }
}

TEST_CASE("v262 setup delay changes only later hand settings", "[alpha-v262][setup]") {
    const auto cat = feedbackCatalog(); auto s = feedbackState(); s.active = 1;
    auto &old = feedbackCard(s, 10, "v260-analytic-0001", 1, Zone::Analysis, SpellState::Analyzing); old.analysisProgress = 0;
    feedbackCard(s, 11, "v260-analytic-0001", 1, Zone::Hand);
    Trigger t; t.owner = 0; EffectResolver::apply(s, cat, t, {EffectKind::SetupDelay, 1});
    auto e = test::ObservedEngine::scenario(cat, s);
    feedbackSend(e, 1, StartAnalysis{11, 2, 0, {}, Rules::freeRing(e.state(), e.catalog(), 2)}); feedbackPass(e);
    CHECK(e.state().cards.at(10).analysisProgress == 0); CHECK(e.state().cards.at(11).analysisProgress == -1);
}

TEST_CASE("v262 thorns triggers on settings and ambush", "[alpha-v262][setup]") {
    const auto cat = feedbackCatalog();
    for (bool ambush : {false, true}) {
        auto s = feedbackState(); s.active = 1;
        feedbackCard(s, 10, "v260-talisman-0012", 0, Zone::Analysis, SpellState::Active).remaining = 2;
        feedbackCard(s, 11, "v260-analytic-0001", 1, Zone::Hand);
        auto e = test::ObservedEngine::scenario(cat, s);
        feedbackSend(e, 1, ambush ? Command(SetAmbush{11, 2, Rules::freeRing(e.state(), e.catalog(), 2)}) : Command(StartAnalysis{11, 2, 0, {}, Rules::freeRing(e.state(), e.catalog(), 2)})); feedbackPass(e);
        CHECK(e.state().players[1].life == 38);
    }
}

TEST_CASE("v262 true strike pays exactly one discard and searches its school", "[alpha-v262][costs]") {
    auto s = feedbackState(); feedbackCard(s, 10, "v260-word-0017", 0, Zone::Hand);
    feedbackCard(s, 11, "v260-analytic-0001", 0, Zone::Hand);
    feedbackCard(s, 12, "v260-analytic-0001", 0, Zone::Deck);
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
    feedbackSend(e, 0, PreloadWord{10, true, 1, 0, {}, 11, 0, Rules::freeRing(e.state(), e.catalog(), 1)}); feedbackPass(e);
    REQUIRE(e.state().decision); CHECK(e.state().decision->options == std::vector<ChoiceValue>{ChoiceValue::card(12)});
    feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue::card(12)}); feedbackPass(e);
    CHECK(e.state().cards.at(11).zone == Zone::Ash); CHECK(e.state().cards.at(12).zone == Zone::Hand);
    CHECK_FALSE(e.state().decision);
}

TEST_CASE("v262 stinking cloud waits until either preparation phase", "[alpha-v262][phase-trigger]") {
    const auto cat = feedbackCatalog(); auto s = feedbackState(); s.phase = Phase::Cast;
    feedbackCard(s, 10, "v260-analytic-0014", 0, Zone::Casting, SpellState::Ready);
    auto e = test::ObservedEngine::scenario(cat, s); feedbackSend(e, 0, ActivateSpell{10}); feedbackPass(e);
    CHECK(e.state().players[0].life == 40); CHECK(e.state().players[1].life == 40);
    for (int active : {0, 1}) {
        auto phase = feedbackState(); phase.active = active; phase.phase = Phase::Draw;
        feedbackCard(phase, 10, "v260-analytic-0014", 0, Zone::Analysis, SpellState::Active);
        auto turn = test::ObservedEngine::scenario(cat, phase);
        feedbackSend(turn, active, AdvancePhase{turn.state().phaseGate}); feedbackPass(turn);
        CHECK(turn.state().players[0].life == 39); CHECK(turn.state().players[1].life == 39);
    }
}

TEST_CASE("v262 blessing triggers before all three owner countdowns", "[alpha-v262][timers]") {
    const auto cat = feedbackCatalog();
    for (int remaining : {3, 2, 1}) {
        auto s = feedbackState(); s.phase = Phase::Cast; s.players[0].life = 20;
        feedbackCard(s, 10, "v260-analytic-0032", 0, Zone::Analysis, SpellState::Active).remaining = remaining;
        auto e = test::ObservedEngine::scenario(cat, s); feedbackSend(e, 0, AdvancePhase{e.state().phaseGate}); feedbackPass(e);
        REQUIRE(e.state().decision); REQUIRE(e.state().decision->kind == DecisionKind::EffectChoice);
        feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue::mode(ChoiceMode::SpellFirst)}); feedbackPass(e);
        CHECK(e.state().players[0].life == 22);
        CHECK(e.state().cards.at(10).remaining == remaining - 1);
        CHECK((e.state().cards.at(10).zone == Zone::Ash) == (remaining == 1));
    }
}

TEST_CASE("v262 grease halves rings down and expires on next preparation", "[alpha-v262][modifiers]") {
    auto cat = feedbackCatalog(); auto s = feedbackState();
    const int before = Rules::rings(s, cat, 2); REQUIRE(before > 0);
    Trigger t; t.owner = 0; t.targetCard = 2;
    EffectResolver::apply(s, cat, t, {EffectKind::HalfRings});
    CHECK(Rules::rings(s, cat, 2) == before / 2);
    s.phase = Phase::Prepare; ++s.players[0].ownTurn; spell_rules::expire(s, cat, false);
    CHECK(Rules::rings(s, cat, 2) == before);
}

TEST_CASE("v262 haste doubles only newly acquired load", "[alpha-v262][haste]") {
    auto cat = feedbackCatalog(); auto s = feedbackState();
    s.temporary.push_back({s.nextLoad++, 0, 0, 2, 0, true});
    Trigger t; t.owner = 0; EffectResolver::apply(s, cat, t, {EffectKind::ExtraTurn, 1});
    EffectResolver::apply(s, cat, t, {EffectKind::DoubleLoad, 2});
    EffectResolver::apply(s, cat, t, {EffectKind::AddIndependent, 3});
    CHECK(Rules::load(s, 0) == 8);
    CHECK(s.extraTurns == std::deque<PlayerId>{0});
}

TEST_CASE("v262 response query retains all hand word sources", "[alpha-v262][response]") {
    auto s = feedbackState();
    feedbackCard(s, 10, "v260-word-0001", 0, Zone::Hand);
    feedbackCard(s, 11, "v260-word-0001", 0, Zone::Hand);
    ChainState chain; chain.id = 1; chain.priority = 0; chain.window = ResponseWindow::Action;
    ChainLink root; root.id = 1; root.kind = LinkKind::Phase; chain.links.push_back(root); s.chain = chain;
    s.decision = PendingDecision{2, 0, DecisionKind::Response, {ChoiceValue::card(10), ChoiceValue::card(11)}, true};
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
    ActionIntent intent; intent.selections = {{ActionField::Operation, std::to_string(static_cast<unsigned>(CommandKind::PreloadWord))}};
    const auto query = e.queryActions(0, intent); REQUIRE(query.field == ActionField::Source);
    CHECK(query.options.size() == 2);
}


TEST_CASE("v262 hellish rebuke responds to actual received damage", "[alpha-v262][response]") {
    auto s = feedbackState(); s.phase = Phase::Cast;
    feedbackCard(s, 10, "v260-talisman-0001", 1, Zone::Casting, SpellState::Ready);
    feedbackCard(s, 20, "v260-analytic-0001", 0, Zone::Casting, SpellState::Ready);
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
    feedbackSend(e, 0, ActivateSpell{20, playerTarget(1)});
    for (int n = 0; n < 20 && (!e.state().chain || e.state().chain->window != ResponseWindow::Damage); ++n) {
        REQUIRE(e.state().decision); feedbackSend(e, e.state().decision->player, PassResponse{e.state().decision->id});
    }
    REQUIRE(e.state().chain); REQUIRE(e.state().chain->window == ResponseWindow::Damage);
    const int damage = 40 - e.state().players[1].life; REQUIRE(damage > 0);
    REQUIRE(e.state().decision); feedbackSend(e, 1, Respond{e.state().decision->id, 10, "respond"}); feedbackPass(e);
    CHECK(e.state().players[0].life == 40 - damage);
}

TEST_CASE("v262 dispel magic returns the opposing setting", "[alpha-v262][response]") {
    auto s = feedbackState();
    feedbackCard(s, 10, "v260-talisman-0006", 1, Zone::Casting, SpellState::Ready);
    feedbackCard(s, 20, "v260-analytic-0001", 0, Zone::Hand);
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
    feedbackSend(e, 0, StartAnalysis{20, 1, 0, {}, Rules::freeRing(e.state(), e.catalog(), 1)});
    REQUIRE(e.state().chain); const auto root = e.state().chain->links.front().id;
    feedbackSend(e, 1, Respond{e.state().decision->id, 10, "respond", 0, root}); feedbackPass(e);
    CHECK(e.state().cards.at(20).zone == Zone::Hand); CHECK(e.state().cards.at(20).analysisLoad == 0);
}

TEST_CASE("v262 counterspell pays actual setting plus activation costs", "[alpha-v262][response]") {
    auto s = feedbackState(); s.phase = Phase::Cast;
    feedbackCard(s, 10, "v260-talisman-0004", 1, Zone::Casting, SpellState::Ready);
    auto &spell = feedbackCard(s, 20, "v260-analytic-0001", 0, Zone::Casting, SpellState::Ready); spell.settingPaid = 3;
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s); const int mana = e.state().players[1].mana;
    feedbackSend(e, 0, ActivateSpell{20, playerTarget(1)});
    const int expected = 3 + e.state().cards.at(20).payments.back().paid;
    feedbackSend(e, 1, Respond{e.state().decision->id, 10, "respond", 0, e.state().chain->links.front().id}); feedbackPass(e);
    CHECK(e.state().players[1].mana == mana - expected); CHECK(e.state().cards.at(20).zone == Zone::Ash);
    CHECK(e.state().players[1].life == 40);
    const auto counter = std::find_if(e.events().begin(), e.events().end(), [](const GameEvent &event) {
        return event.kind == "effect" && event.card == 10 && event.effectType == EffectKind::CounterSpell;
    });
    REQUIRE(counter != e.events().end());
    CHECK(counter->chainIndex == 2);
    CHECK(counter->text.find("C1") != std::string::npos);
    CHECK(counter->text.find("火球术") != std::string::npos);
}

TEST_CASE("v262 detect evil and good normally casts and routes all spell types", "[alpha-v262][inspection]") {
    for (const auto target : {"v260-analytic-0001", "v260-word-0001", "v260-talisman-0001"}) {
        CAPTURE(target); auto s = feedbackState(); s.phase = Phase::Cast;
        feedbackCard(s, 10, "v260-talisman-0011", 0, Zone::Casting, SpellState::Ready);
        feedbackCard(s, 20, target, 1, Zone::Analysis).faceDown = true;
        auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
        feedbackSend(e, 0, ActivateSpell{10, 20}); feedbackPass(e);
        REQUIRE(e.state().decision); REQUIRE(e.state().cards.at(20).faceDown);
        feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue::mode(ChoiceMode::Confirm)}); feedbackPass(e);
        CHECK(e.state().cards.at(20).zone == (std::string(target) == "v260-talisman-0001" ? Zone::Ash : Zone::Deck));
    }
}

TEST_CASE("v262 mental prison triggers only at the opposing end", "[alpha-v262][phase-trigger]") {
    for (bool hand : {false, true}) {
        auto s = feedbackState(); s.active = 1; s.phase = Phase::Cast;
        feedbackCard(s, 10, "v260-analytic-0028", 0, Zone::Analysis, SpellState::Active);
        if (hand) feedbackCard(s, 20, "v260-word-0001", 1, Zone::Hand);
        auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
        feedbackSend(e, 1, AdvancePhase{e.state().phaseGate}); feedbackPass(e);
        if (hand) {
            REQUIRE(e.state().decision); CHECK(e.state().decision->player == 1);
            feedbackSend(e, 1, Choose{e.state().decision->id, ChoiceValue::card(20)}); feedbackPass(e);
            CHECK(e.state().cards.at(20).zone == Zone::Ash); CHECK(e.state().players[1].life == 40);
        } else CHECK(e.state().players[1].life == 30);
    }
}

TEST_CASE("v262 concentration prohibition removes legal upkeep and expires", "[alpha-v262][modifiers]") {
    const auto cat = feedbackCatalog(); auto s = feedbackState(); s.phase = Phase::Draw;
    feedbackCard(s, 10, "v260-analytic-0017", 0, Zone::Analysis, SpellState::Active);
    Trigger t; t.owner = 1; EffectResolver::apply(s, cat, t, {EffectKind::BlockConcentration});
    auto e = test::ObservedEngine::scenario(cat, s); feedbackSend(e, 0, AdvancePhase{e.state().phaseGate}); feedbackPass(e);
    REQUIRE(e.state().decision); CHECK(e.state().decision->kind == DecisionKind::Concentration); CHECK(e.state().decision->options.empty());
    CHECK(bridge::viewDto(e.viewFor(0)).at("decision").value("upkeepUnavailable", std::string{}) == "当前效果禁止维持专注");
    const int mana = e.state().players[0].mana;
    CHECK_FALSE(e.submit(0, Choose{e.state().decision->id, ChoiceValue::card(10)}).accepted);
    CHECK(e.state().players[0].mana == mana);
    feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue{}}); feedbackPass(e); CHECK(e.state().cards.at(10).zone == Zone::Ash);
    s.phase = Phase::Main; s.active = 1; ++s.players[1].ownTurn; spell_rules::expire(s, cat, false); CHECK(s.modifiers.empty());
}

TEST_CASE("v262 drowsiness blocks analytic activation until the next owner draw", "[alpha-v262][modifiers]") {
    const auto cat = feedbackCatalog(); auto s = feedbackState(); s.phase = Phase::Cast;
    feedbackCard(s, 10, "v260-analytic-0001", 0, Zone::Casting, SpellState::Ready);
    Trigger t; t.owner = 0; EffectResolver::apply(s, cat, t, {EffectKind::BlockAnalytic});
    auto e = test::ObservedEngine::scenario(cat, s); CHECK_FALSE(e.submit(0, ActivateSpell{10, playerTarget(1)}).accepted);
    s.phase = Phase::Draw; ++s.players[0].ownTurn; spell_rules::expire(s, cat, false);
    s.phase = Phase::Cast; auto next = test::ObservedEngine::scenario(cat, s); feedbackSend(next, 0, ActivateSpell{10, playerTarget(1)});
}

TEST_CASE("v262 bestow curse and ray of sickness trigger and expire", "[alpha-v262][modifiers]") {
    const auto cat = feedbackCatalog();
    for (bool word : {false, true}) {
        auto s = feedbackState(); s.active = 1;
        feedbackCard(s, 20, word ? "v260-word-0001" : "v260-analytic-0001", 1, Zone::Hand);
        Trigger t; t.owner = 0; EffectResolver::apply(s, cat, t, {word ? EffectKind::WordCastDamage : EffectKind::SetupDamage, word ? 2 : 1, word ? DamageType::Necrotic : DamageType::Poison});
        auto e = test::ObservedEngine::scenario(cat, s);
        feedbackSend(e, 1, word ? Command(PreloadWord{20, true, 2, playerTarget(0), {}, 0, 0, Rules::freeRing(e.state(), e.catalog(), 2)}) : Command(StartAnalysis{20, 2, 0, {}, Rules::freeRing(e.state(), e.catalog(), 2)})); feedbackPass(e);
        CHECK(e.state().players[1].life == (word ? 38 : 39));
        s.phase = Phase::Draw; s.active = 0; ++s.players[0].ownTurn; spell_rules::expire(s, cat, false); CHECK(s.modifiers.empty());
    }
}

TEST_CASE("v262 otherworldly perception pays one discard and routes the top three", "[alpha-v262][inspection]") {
    auto s = feedbackState(); feedbackCard(s, 10, "v260-word-0019", 0, Zone::Hand);
    feedbackCard(s, 11, "v260-word-0001", 0, Zone::Hand);
    for (CardId id : {20,21,22,23,24}) feedbackCard(s, id, "v260-analytic-0001", 1, Zone::Deck);
    auto e = test::ObservedEngine::scenario(feedbackCatalog(), s);
    const auto digest = e.digest(); CHECK_FALSE(e.submit(0, PreloadWord{10, true, 1, 0, {}, 0, 0, Rules::freeRing(e.state(), e.catalog(), 1)}).accepted); CHECK(e.digest() == digest);
    feedbackSend(e, 0, PreloadWord{10, true, 1, 0, {}, 11, 0, Rules::freeRing(e.state(), e.catalog(), 1)}); feedbackPass(e);
    REQUIRE(e.state().decision); CHECK(e.state().decision->options.size() == 3);
    CHECK(e.viewFor(0).cards.size() == 7); // formations, source, discarded card, and three inspected cards
    feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue::card(23)}); feedbackPass(e);
    CHECK(e.state().cards.at(11).zone == Zone::Ash); CHECK(e.state().cards.at(23).zone == Zone::Ash);
    CHECK(e.state().players[1].deck == std::vector<CardId>{22,24,20,21});
}

TEST_CASE("v262 command offers three real routes with private hand selection", "[alpha-v262][choice]") {
    for (const auto mode : {ChoiceMode::SpellFirst, ChoiceMode::SpellSecond, ChoiceMode::SpellThird}) {
        auto s = feedbackState(); feedbackCard(s, 10, "v260-word-0015", 0, Zone::Hand);
        feedbackCard(s, 20, "v260-analytic-0001", 1, Zone::Deck);
        feedbackCard(s, 21, "v260-analytic-0001", 1, Zone::Analysis, SpellState::Ready);
        feedbackCard(s, 22, "v260-analytic-0001", 1, Zone::Hand);
        auto e = test::ObservedEngine::scenario(feedbackCatalog(), s); feedbackSend(e, 0, PreloadWord{10, true, 1, 0, {}, 0, 0, Rules::freeRing(e.state(), e.catalog(), 1)}); feedbackPass(e);
        REQUIRE(e.state().decision); CHECK(e.state().decision->options.size() == 3);
        feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue::mode(mode)}); feedbackPass(e);
        CardId chosen = 20;
        if (mode != ChoiceMode::SpellFirst) {
            REQUIRE(e.state().decision); chosen = mode == ChoiceMode::SpellSecond ? 21 : 22;
            CHECK(e.state().decision->player == (chosen == 22 ? 1 : 0));
            feedbackSend(e, e.state().decision->player, Choose{e.state().decision->id, ChoiceValue::card(chosen)}); feedbackPass(e);
        }
        CHECK(e.state().cards.at(chosen).zone == Zone::Ash);
    }
}

TEST_CASE("v262 blur selects all damage types and halves only the chosen type", "[alpha-v262][choice]") {
    auto cat = feedbackCatalog(); auto s = feedbackState(); s.phase = Phase::Cast;
    feedbackCard(s, 10, "v260-analytic-0026", 0, Zone::Casting, SpellState::Ready);
    auto e = test::ObservedEngine::scenario(cat, s); feedbackSend(e, 0, ActivateSpell{10}); feedbackPass(e);
    REQUIRE(e.state().decision); CHECK(e.state().decision->options.size() == 13);
    feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue::damage(DamageType::Piercing)}); feedbackPass(e);
    CHECK(Rules::damage(e.state(),cat,0,{EffectKind::Damage,7,DamageType::Piercing}) == 3);
    CHECK(Rules::damage(e.state(),cat,0,{EffectKind::Damage,7,DamageType::Fire}) == 7);
}

TEST_CASE("v262 color spray includes radiant and protects its owner until turn end", "[alpha-v262][choice]") {
    const auto cat = feedbackCatalog(); auto s = feedbackState();
    feedbackCard(s, 10, "v260-word-0011", 0, Zone::Hand);
    auto e = test::ObservedEngine::scenario(cat,s); feedbackSend(e, 0, PreloadWord{10, true, 1, playerTarget(1), {}, 0, 0, Rules::freeRing(e.state(), e.catalog(), 1)}); feedbackPass(e);
    REQUIRE(e.state().decision); CHECK(e.state().decision->options.size() == 7);
    feedbackSend(e, 0, Choose{e.state().decision->id, ChoiceValue::damage(DamageType::Radiant)}); feedbackPass(e);
    CHECK(e.state().players[1].life == 35); CHECK_FALSE(spell_rules::playerTargetAllowed(e.state(),cat,1,0));
    auto end = test::mutation(e.state()); end.phase = Phase::End; spell_rules::expire(end,cat,true); CHECK(spell_rules::playerTargetAllowed(end,cat,1,0));
}

TEST_CASE("v262 fear filters protected player targets but allows self targets", "[alpha-v262][targets]") {
    auto s = feedbackState(); s.active = 1; s.phase = Phase::Cast;
    feedbackCard(s,10,"v260-analytic-0027",0,Zone::Analysis,SpellState::Active);
    feedbackCard(s,20,"v260-analytic-0001",1,Zone::Casting,SpellState::Ready);
    auto e = test::ObservedEngine::scenario(feedbackCatalog(),s);
    CHECK_FALSE(e.submit(1,ActivateSpell{20,playerTarget(0)}).accepted);
    for(const auto &a : e.viewFor(1).actions) if(const auto *cast = std::get_if<ActivateSpell>(&a.command)) CHECK(cast->target != playerTarget(0));
    feedbackSend(e,1,ActivateSpell{20,playerTarget(1)}); feedbackPass(e); CHECK(e.state().players[1].life < 40);
}

TEST_CASE("v262 shatter and catapult pay their distinct costs atomically", "[alpha-v262][costs]") {
    for (bool shatter : {false,true}) {
        auto s = feedbackState(); s.phase = Phase::Cast;
        feedbackCard(s,10,shatter ? "v260-analytic-0036" : "v260-analytic-0038",0,Zone::Casting,SpellState::Ready);
        feedbackCard(s,11,shatter ? "v260-formation-0001" : "v260-analytic-0001",0,shatter ? Zone::SideDeck : Zone::Analysis,shatter ? SpellState::None : SpellState::Ready);
        auto e = test::ObservedEngine::scenario(feedbackCatalog(),s); const auto before=e.digest();
        CHECK_FALSE(e.submit(0,ActivateSpell{10,shatter ? CardId{2} : playerTarget(1)}).accepted); CHECK(e.digest() == before);
        feedbackSend(e,0,ActivateSpell{10,shatter ? CardId{2} : playerTarget(1),{},11}); feedbackPass(e);
        CHECK(e.state().cards.at(11).zone == Zone::Ash);
        if(shatter) CHECK(e.state().cards.at(2).zone == Zone::Ash); else CHECK(e.state().players[1].life == 30);
    }
}

TEST_CASE("v262 mage hand transfers and returns the spell and its attachments", "[alpha-v262][control]") {
    auto s=feedbackState(); feedbackCard(s,10,"v260-word-0004",0,Zone::Hand);
    feedbackCard(s,20,"v260-analytic-0001",1,Zone::Analysis,SpellState::Ready);
    auto &seal=feedbackCard(s,21,"v260-seal-0001",1,Zone::Attached); seal.host=20;
    auto cat=feedbackCatalog(); auto e=test::ObservedEngine::scenario(cat,s);
    feedbackSend(e,0,PreloadWord{10, true, 1, 20, {}, 0, 0, Rules::freeRing(e.state(), e.catalog(), 1)}); feedbackPass(e); REQUIRE(e.state().decision);
    feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::card(1)}); feedbackPass(e);
    CHECK(controllerOf(e.state().cards.at(20)) == 0); CHECK(e.state().cards.at(20).host == 1); CHECK(controllerOf(e.state().cards.at(21)) == 0);
    auto next=test::mutation(e.state()); next.phase=Phase::Prepare; ++next.players[0].ownTurn; spell_rules::expire(next,cat,true);
    CHECK(controllerOf(next.cards.at(20)) == 1); CHECK(next.cards.at(20).host == 2); CHECK(controllerOf(next.cards.at(21)) == 1);
}

TEST_CASE("Mage Hand feedback identifies the moved spell and host for each viewer", "[hotfix-v08]") {
    for (const bool hidden : {false, true}) {
        auto state = feedbackState();
        state.phase = Phase::Cast;
        state.cards.at(1).definition = "v260-formation-0003"; // 安卡十字 has one ring.
        feedbackCard(state, 10, "v260-word-0004", 0, Zone::Casting, SpellState::Ready);
        auto &target = feedbackCard(state, 20, "v260-analytic-0001", 1, Zone::Analysis,
                                    hidden ? SpellState::None : SpellState::Ready);
        target.faceDown = target.wasAmbushed = hidden;
        auto engine = test::ObservedEngine::scenario(feedbackCatalog(), state);
        feedbackSend(engine, 0, ActivateSpell{10, 20});
        feedbackPass(engine);
        REQUIRE(engine.state().decision);
        feedbackSend(engine, 0, Choose{engine.state().decision->id, ChoiceValue::card(1)});
        for (PlayerId viewer : {0, 1}) {
            CAPTURE(hidden, viewer);
            const auto &events = engine.events();
            const auto effect = std::find_if(events.begin(), events.end(), [&](const GameEvent &event) {
                return event.kind == "effect" && event.card == 10 && event.effectType == EffectKind::BorrowAnalysis &&
                    (event.audience == -1 || event.audience == viewer);
            });
            REQUIRE(effect != events.end());
            CHECK(effect->chainIndex == 1);
            CHECK(effect->text.find("安卡十字") != std::string::npos);
            CHECK(effect->text.find("第1环") != std::string::npos);
            CHECK((effect->text.find("火球术") != std::string::npos) == (!hidden || viewer == 0));
            if (hidden && viewer == 1) CHECK(effect->text.find("盖伏法术") != std::string::npos);
        }
    }
}

TEST_CASE("v262 cloud mist locks one or two selected spells until it leaves", "[alpha-v262][targets]") {
    const auto cat=feedbackCatalog(); auto s=feedbackState(); s.phase=Phase::Cast;
    feedbackCard(s,10,"v260-analytic-0011",0,Zone::Casting,SpellState::Ready);
    feedbackCard(s,20,"v260-analytic-0001",0,Zone::Analysis,SpellState::Ready);
    feedbackCard(s,21,"v260-analytic-0001",1,Zone::Analysis,SpellState::Ready);
    auto e=test::ObservedEngine::scenario(cat,s); feedbackSend(e,0,ActivateSpell{10}); feedbackPass(e);
    REQUIRE(e.state().decision); feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::card(20)});
    REQUIRE(e.state().decision); feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::card(21)}); feedbackPass(e);
    auto next=test::mutation(e.state()); next.phase=Phase::Main; next.phaseGate=next.nextDecision++; next.decision.reset(); next.chain.reset();
    auto blocked=test::ObservedEngine::scenario(cat,next); CHECK_FALSE(blocked.submit(0,PrepareCast{20}).accepted);
    const auto projected=e.viewFor(0); auto card=std::find_if(projected.cards.begin(),projected.cards.end(),[](const auto &c){return c.instance.id==20;});
    REQUIRE(card!=projected.cards.end()); CHECK(card->statuses == std::vector<std::string>{"【云雾术】效果生效中"});
    StateMaintenance::leave(next,10,&cat); auto free=test::ObservedEngine::scenario(cat,next); feedbackSend(free,0,PrepareCast{20});
}

TEST_CASE("v262 lesser restoration heals its target and clears at most six own load", "[alpha-v262][resources]") {
    auto s=feedbackState(); s.phase=Phase::Cast; s.players[1].life=20;
    feedbackCard(s,10,"v260-analytic-0016",0,Zone::Casting,SpellState::Ready);
    s.temporary.push_back({s.nextLoad++,0,0,4,0,true}); auto cat=feedbackCatalog(); auto e=test::ObservedEngine::scenario(cat,s);
    feedbackSend(e,0,ActivateSpell{10,playerTarget(1)}); feedbackPass(e);
    int cleared=0;
    while(e.state().decision && e.state().decision->kind==DecisionKind::ClearAnyLoad) { REQUIRE(cleared++ < 7); feedbackSend(e,0,Choose{e.state().decision->id,e.state().decision->options.front()}); feedbackPass(e); }
    CHECK(e.state().players[1].life == 22); CHECK(Rules::load(e.state(),0)==0);
}

TEST_CASE("v262 earthquake waits for the owner's main phase", "[alpha-v262][phase-trigger]") {
    const auto cat=feedbackCatalog(); auto s=feedbackState(); s.phase=Phase::Cast;
    feedbackCard(s,10,"v260-analytic-0035",0,Zone::Casting,SpellState::Ready);
    feedbackCard(s,20,"v260-analytic-0001",1,Zone::Analysis).faceDown=true;
    auto e=test::ObservedEngine::scenario(cat,s); feedbackSend(e,0,ActivateSpell{10}); feedbackPass(e); CHECK(e.state().cards.at(20).zone==Zone::Analysis);
    REQUIRE(e.state().decision); REQUIRE(e.state().decision->kind==DecisionKind::ReturnSpell);
    feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::card(1)}); feedbackPass(e);
    CHECK(e.state().cards.at(20).zone==Zone::Analysis);
    for(int n=0;n<10 && !(e.state().phase==Phase::Main && e.state().active==0);++n) {
        feedbackSend(e,e.state().active,AdvancePhase{e.state().phaseGate}); feedbackPass(e);
    }
    REQUIRE(e.state().phase==Phase::Main); REQUIRE(e.state().active==0); CHECK(e.state().cards.at(20).zone==Zone::Ash);
}

TEST_CASE("v262 mind spike reveals ambush information without flipping it", "[alpha-v262][inspection]") {
    auto s=feedbackState(); s.phase=Phase::Cast;
    feedbackCard(s,10,"v260-analytic-0034",0,Zone::Casting,SpellState::Ready);
    feedbackCard(s,20,"v260-analytic-0001",1,Zone::Analysis).faceDown=true;
    auto e=test::ObservedEngine::scenario(feedbackCatalog(),s); feedbackSend(e,0,ActivateSpell{10,playerTarget(1)}); feedbackPass(e);
    REQUIRE(e.state().decision); feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::mode(ChoiceMode::Confirm)}); feedbackPass(e);
    CHECK(e.state().cards.at(20).faceDown); const auto view=e.viewFor(0);
    const auto card=std::find_if(view.cards.begin(),view.cards.end(),[](const auto &c){return c.instance.id==20;}); REQUIRE(card!=view.cards.end()); CHECK_FALSE(card->hidden);
}

TEST_CASE("v262 calamity offers all three outcomes and temporary load clears on opponent end", "[alpha-v262][timers]") {
    const auto cat=feedbackCatalog();
    for(const auto mode:{ChoiceMode::SpellFirst,ChoiceMode::SpellSecond,ChoiceMode::SpellThird}) {
        auto s=feedbackState(); s.phase=Phase::Cast;
        feedbackCard(s,10,"v260-analytic-0033",0,Zone::Analysis,SpellState::Active).remaining=1;
        auto e=test::ObservedEngine::scenario(cat,s); feedbackSend(e,0,AdvancePhase{e.state().phaseGate}); feedbackPass(e); REQUIRE(e.state().decision);
        feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::mode(mode)}); feedbackPass(e);
        CHECK(e.state().cards.at(10).zone==Zone::Ash);
        if(mode==ChoiceMode::SpellFirst) CHECK(e.state().players[1].life==38);
        else if(mode==ChoiceMode::SpellThird) CHECK(e.state().players[1].mana==19);
        else {
            CHECK(Rules::load(e.state(),1)==2);
            auto end=test::mutation(e.state()); end.active=1; ++end.players[1].ownTurn; end.phase=Phase::End; end.phaseStep=PhaseStep::Body; end.phaseGate=end.nextDecision++;
            auto other=test::ObservedEngine::scenario(cat,end); feedbackSend(other,1,AdvancePhase{other.state().phaseGate}); feedbackPass(other); CHECK(Rules::load(other.state(),1)==0);
        }
    }
}

TEST_CASE("v262 haste inserts the owner turn and expires after that end phase", "[alpha-v262][haste]") {
    const auto cat=feedbackCatalog(); auto s=feedbackState(); s.phase=Phase::End;
    Trigger t; t.owner=0; EffectResolver::apply(s,cat,t,{EffectKind::ExtraTurn,1}); EffectResolver::apply(s,cat,t,{EffectKind::DoubleLoad,2});
    auto e=test::ObservedEngine::scenario(cat,s); feedbackSend(e,0,AdvancePhase{e.state().phaseGate}); feedbackPass(e);
    CHECK(e.state().active==0); CHECK(e.state().globalTurn==6); CHECK(spell_rules::loadMultiplier(e.state(),0)==2);
    auto end=test::mutation(e.state()); end.phase=Phase::End; spell_rules::expire(end,cat,true); CHECK(spell_rules::loadMultiplier(end,0)==1);
    end.phaseStep=PhaseStep::Body; end.phaseGate=end.nextDecision++;
    auto next=test::ObservedEngine::scenario(cat,end);
    feedbackSend(next,0,AdvancePhase{next.state().phaseGate}); feedbackPass(next);
    CHECK(next.state().active==1); CHECK(next.state().extraTurns.empty());
}


TEST_CASE("v262 blessing load and mana options resolve before the last countdown", "[alpha-v262][timers]") {
    for(const auto mode:{ChoiceMode::SpellSecond,ChoiceMode::SpellThird}) {
        auto s=feedbackState(); s.phase=Phase::Cast; s.players[0].mana=10;
        feedbackCard(s,10,"v260-analytic-0032",0,Zone::Analysis,SpellState::Active).remaining=1;
        s.temporary.push_back({s.nextLoad++,0,0,3,0,true}); auto cat=feedbackCatalog(); auto e=test::ObservedEngine::scenario(cat,s);
        feedbackSend(e,0,AdvancePhase{e.state().phaseGate}); feedbackPass(e); REQUIRE(e.state().decision);
        const auto labels=e.viewFor(0).actions; CHECK(labels.front().label.find("恢复2点")!=std::string::npos);
        feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::mode(mode)}); feedbackPass(e);
        if(mode==ChoiceMode::SpellSecond) {
            for(int n=0;n<2;++n) { REQUIRE(e.state().decision); feedbackSend(e,0,Choose{e.state().decision->id,e.state().decision->options.front()}); feedbackPass(e); }
            CHECK(Rules::load(e.state(),0)==1);
        } else CHECK(e.state().players[0].mana==11);
        CHECK(e.state().cards.at(10).zone==Zone::Ash);
    }
}

TEST_CASE("v262 fragile curse changes actual damage and expires at turn end", "[alpha-v262][damage-watchers]") {
    const auto cat=feedbackCatalog(); auto s=feedbackState(); Trigger t; t.owner=0; t.targetPlayer=1;
    EffectResolver::apply(s,cat,t,{EffectKind::DamageTakenBonus,3}); EffectResolver::apply(s,cat,t,{EffectKind::Damage,4,DamageType::Fire});
    CHECK(s.players[1].life==33); s.phase=Phase::End; spell_rules::expire(s,cat,true);
    EffectResolver::apply(s,cat,t,{EffectKind::Damage,4,DamageType::Fire}); CHECK(s.players[1].life==29);
}

TEST_CASE("v262 analysis time changes exclude active spells", "[alpha-v262][targets]") {
    const auto cat=feedbackCatalog();
    for(CardId selected:{10,11}) {
        auto s=feedbackState(); s.phase=Phase::Cast;
        feedbackCard(s,10,"v260-analytic-0001",1,Zone::Analysis,SpellState::Analyzing);
        feedbackCard(s,11,"v260-analytic-0001",1,Zone::Analysis,SpellState::Ready);
        feedbackCard(s,12,"v260-analytic-0032",1,Zone::Analysis,SpellState::Active).remaining=3;
        feedbackCard(s,20,"v260-analytic-0002",0,Zone::Casting,SpellState::Ready);
        auto e=test::ObservedEngine::scenario(cat,s); feedbackSend(e,0,ActivateSpell{20,playerTarget(1)}); feedbackPass(e);
        REQUIRE(e.state().decision); CHECK(e.state().decision->options==std::vector<ChoiceValue>{ChoiceValue::card(10),ChoiceValue::card(11)});
        feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::card(selected)}); feedbackPass(e);
        CHECK(e.state().cards.at(selected).analysisProgress==-1); CHECK(e.state().cards.at(12).analysisProgress==0); CHECK(e.state().cards.at(12).remaining==3);
    }
}

TEST_CASE("v262 public modifiers survive network projection with decimal wide identifiers", "[alpha-v262][network]") {
    const auto cat=feedbackCatalog(); auto s=feedbackState(); s.globalTurn=9007199254740993ULL;
    Trigger t; t.owner=0; EffectResolver::apply(s,cat,t,{EffectKind::DoubleLoad,2}); EffectResolver::apply(s,cat,t,{EffectKind::ExtraTurn,1});
    auto e=test::ObservedEngine::scenario(cat,s); auto projected=e.viewFor(0); const auto wire=net::encodeView(projected);
    REQUIRE(wire.at("modifiers").size()==1); CHECK(wire["modifiers"][0]["expiresTurn"]=="9007199254740994");
    const auto restored=net::decodeView(wire,cat); CHECK(restored.modifiers.at(0).expiresTurn==9007199254740994ULL); CHECK(restored.extraTurns==std::deque<PlayerId>{0});
    auto changed=test::mutation(s); changed.modifiers[0].amount=3; CHECK(GameEngine::scenario(cat,changed).digest()!=e.digest());
}

TEST_CASE("v262 death ward enforces the existing single copy restriction", "[alpha-v262][death-ward]") {
    auto s=feedbackState(); s.phase=Phase::Cast;
    feedbackCard(s,10,"v260-analytic-0023",0,Zone::Analysis,SpellState::Active);
    feedbackCard(s,11,"v260-analytic-0023",0,Zone::Casting,SpellState::Ready);
    auto e=test::ObservedEngine::scenario(feedbackCatalog(),s); const auto before=e.digest(); CHECK_FALSE(e.submit(0,ActivateSpell{11}).accepted); CHECK(e.digest()==before);
}

TEST_CASE("v262 death ward uniqueness follows control and rejects setup without payment", "[alpha-v262][death-ward]") {
    auto s=feedbackState();
    auto &borrowed=feedbackCard(s,10,"v260-analytic-0023",1,Zone::Analysis,SpellState::Active);
    borrowed.controller=0; borrowed.host=1;
    feedbackCard(s,11,"v260-analytic-0023",0,Zone::Hand);
    auto e=test::ObservedEngine::scenario(feedbackCatalog(),s); const auto before=e.digest();
    CHECK_FALSE(e.submit(0,StartAnalysis{11, 1, 0, {}, Rules::freeRing(e.state(), e.catalog(), 1)}).accepted);
    CHECK(e.digest()==before); CHECK(e.state().players[0].mana==20);
}

TEST_CASE("v262 filtering derives timer and delayed piercing damage tags", "[alpha-v262][content]") {
    const auto cat=feedbackCatalog(); const auto tags=app::cardTags(cat.at("v260-talisman-0012"));
    CHECK(std::find(tags.begin(),tags.end(),"计时")!=tags.end()); CHECK(std::find(tags.begin(),tags.end(),damageName(DamageType::Piercing))!=tags.end());
    const auto delayed=app::cardTags(cat.at("v260-analytic-0014")); CHECK(std::find(delayed.begin(),delayed.end(),damageName(DamageType::Poison))!=delayed.end());
}


TEST_CASE("v262 every feedback card supports a real legal cast with exactly one payment", "[alpha-v262][card-matrix]") {
    const auto cat=feedbackCatalog();
    const std::vector<std::string> definitions={
        "v260-analytic-0004", // 魔法飞弹
        "v260-analytic-0009", // 阳炎爆
        "v260-analytic-0011", // 云雾术
        "v260-analytic-0014", // 臭云术
        "v260-analytic-0015", // 油腻术
        "v260-analytic-0016", // 次级复原
        "v260-analytic-0017", // 吸收元素
        "v260-analytic-0022", // 致病射线
        "v260-analytic-0023", // 防死结界
        "v260-analytic-0026", // 朦胧术
        "v260-analytic-0027", // 恐惧术
        "v260-analytic-0028", // 精神监狱
        "v260-analytic-0030", // 昏昏欲睡
        "v260-analytic-0031", // 疯狂冠冕
        "v260-analytic-0032", // 祝福术
        "v260-analytic-0033", // 灾祸术
        "v260-analytic-0034", // 心灵尖刺
        "v260-analytic-0035", // 地震术
        "v260-analytic-0036", // 碎环术
        "v260-analytic-0038", // 弹射术
        "v260-analytic-0039", // 加速术
        "v260-word-0004", // 法师之手
        "v260-word-0009", // 扰神曲
        "v260-word-0010", // 降咒
        "v260-word-0011", // 七彩喷射
        "v260-word-0014", // 脆弱诅咒
        "v260-word-0015", // 命令术
        "v260-word-0017", // 克敌机先
        "v260-word-0019", // 异界探知
        "v260-talisman-0007", // 死佑护甲
        "v260-talisman-0008", // 步履维艰
        "v260-talisman-0011", // 侦测善恶
        "v260-talisman-0012", // 荆棘丛生
    };
    for(const auto &id:definitions) {
        CAPTURE(id,cat.at(id).name); const auto &d=cat.at(id); auto s=feedbackState();
        const bool word=d.type==CardType::Word; s.phase=word ? Phase::Main : Phase::Cast;
        feedbackCard(s,10,id.c_str(),0,word ? Zone::Hand : Zone::Casting,word ? SpellState::None : SpellState::Ready);
        feedbackCard(s,11,"v260-analytic-0001",0,Zone::Hand);
        feedbackCard(s,12,"v260-analytic-0001",0,Zone::Deck);
        feedbackCard(s,20,"v260-analytic-0001",1,Zone::Analysis,SpellState::Ready);
        feedbackCard(s,21,"v260-analytic-0001",0,Zone::Analysis,SpellState::Ready);
        feedbackCard(s,22,"v260-formation-0001",0,Zone::SideDeck);
        for(CardId card:{30,31,32,33}) feedbackCard(s,card,"v260-analytic-0001",1,Zone::Deck);
        CardId target=0;
        if(d.target==TargetKind::Player)target=playerTarget(1);
        if(d.target==TargetKind::EnemyFormation || d.target==TargetKind::EmptyEnemyFormation) {
            target=2; StateMaintenance::leave(s,20,&cat);
        }
        if(d.target==TargetKind::EnemyAnalysisSpell || d.target==TargetKind::EnemyAmbush)target=20;
        if(d.target==TargetKind::EnemyAmbush) {s.cards.at(20).faceDown=true;s.cards.at(20).spell=SpellState::None;}
        const int activation=Rules::castCost(s,cat,10);
        const int fee=activation+(word ? d.cost : 0);
        const auto costs=card_rules::costOptions(s,cat,0,10); const CardId discard=costs.empty() ? 0 : costs.front();
        auto e=test::ObservedEngine::scenario(cat,s);
        const Command command=word ? Command(PreloadWord{10, true, 1, target, {}, discard, 0, Rules::freeRing(e.state(), e.catalog(), 1)}) : Command(ActivateSpell{10,target,{},discard});
        bool listed=false; e.visitLegalActions(0,[&](const LegalAction &a){if(a.source==10 && commandKind(a.command)==commandKind(command))listed=true;return true;}); CHECK(listed);
        feedbackSend(e,0,command);
        for(int n=0;n<80 && e.state().decision;++n) {
            const auto choice=*e.state().decision;
            if(choice.kind==DecisionKind::Response) feedbackSend(e,choice.player,PassResponse{choice.id});
            else feedbackSend(e,choice.player,Choose{choice.id,choice.options.empty() ? ChoiceValue{} : choice.options.front()});
        }
        CHECK_FALSE(e.state().decision); CHECK(e.state().players[0].mana==20-fee);
        REQUIRE(e.state().cards.at(10).payments.size()==1); CHECK(e.state().cards.at(10).payments.front().paid==activation);
        CHECK((e.state().cards.at(10).spell==SpellState::Active || e.state().cards.at(10).spell==SpellState::Resolved));
    }
}

TEST_CASE("v262 borrowed spell goes to ash when the return side has no legal ring", "[alpha-v262][control]") {
    auto s=feedbackState(); feedbackCard(s,10,"v260-word-0004",0,Zone::Hand);
    feedbackCard(s,20,"v260-analytic-0001",1,Zone::Analysis,SpellState::Ready);
    auto cat=feedbackCatalog(); auto e=test::ObservedEngine::scenario(cat,s); feedbackSend(e,0,PreloadWord{10, true, 1, 20, {}, 0, 0, Rules::freeRing(e.state(), e.catalog(), 1)}); feedbackPass(e);
    REQUIRE(e.state().decision); feedbackSend(e,0,Choose{e.state().decision->id,ChoiceValue::card(1)}); feedbackPass(e);
    auto next=test::mutation(e.state()); StateMaintenance::leave(next,2,&cat); next.phase=Phase::Prepare;++next.players[0].ownTurn;
    spell_rules::expire(next,cat,true); CHECK(next.cards.at(20).zone==Zone::Ash); CHECK(controllerOf(next.cards.at(20))==1);
}

TEST_CASE("v262 grease overflow lets the affected controller choose which spell leaves", "[alpha-v262][modifiers]") {
    const auto cat=feedbackCatalog(); auto s=feedbackState(); s.phase=Phase::Cast;
    feedbackCard(s,10,"v260-analytic-0015",0,Zone::Casting,SpellState::Ready);
    const int rings=Rules::rings(s,cat,2);
    for(int n=0;n<rings;++n)feedbackCard(s,20+n,"v260-analytic-0001",1,Zone::Analysis,SpellState::Ready);
    auto e=test::ObservedEngine::scenario(cat,s); feedbackSend(e,0,ActivateSpell{10,2}); feedbackPass(e);
    REQUIRE(e.state().decision); CHECK(e.state().decision->kind==DecisionKind::Overflow); CHECK(e.state().decision->player==1);
    feedbackSend(e,1,Choose{e.state().decision->id,ChoiceValue::card(20)}); CHECK(e.state().cards.at(20).zone==Zone::Ash);
    const auto view=e.viewFor(1); auto formation=std::find_if(view.cards.begin(),view.cards.end(),[](const auto &c){return c.instance.id==2;}); REQUIRE(formation!=view.cards.end());
    CHECK(formation->effectiveRings==rings/2); CHECK(formation->statuses==std::vector<std::string>{"【油腻术】效果生效中"});
}

TEST_CASE("v262 retaliation chains finish through the existing event queue", "[alpha-v262][damage-watchers]") {
    auto s=feedbackState(); s.phase=Phase::Cast;
    for(int p=0;p<2;++p)feedbackCard(s,10+p,"v260-analytic-0031",p,Zone::Analysis,SpellState::Active);
    feedbackCard(s,20,"v260-analytic-0001",0,Zone::Casting,SpellState::Ready);
    auto e=test::ObservedEngine::scenario(feedbackCatalog(),s); feedbackSend(e,0,ActivateSpell{20,playerTarget(1)});
    for(int n=0;n<160 && e.state().decision;++n) {
        const auto d=*e.state().decision;
        if(d.kind==DecisionKind::Response)feedbackSend(e,d.player,PassResponse{d.id});
        else feedbackSend(e,d.player,Choose{d.id,d.options.front()});
    }
    CHECK(e.state().result!=-1); CHECK(e.state().players[0].life>=0); CHECK(e.state().players[1].life>=0); CHECK(e.state().queue.items.empty());
}


TEST_CASE("v262 counterspell does not count a word's single payment twice", "[alpha-v262][response]") {
    const auto cat=feedbackCatalog(); auto s=feedbackState();
    feedbackCard(s,10,"v260-talisman-0004",1,Zone::Casting,SpellState::Ready);
    feedbackCard(s,20,"v260-word-0001",0,Zone::Hand);
    const int paid=cat.at("v260-word-0001").cost+cat.at("v260-word-0001").castCost; auto e=test::ObservedEngine::scenario(cat,s);
    feedbackSend(e,0,PreloadWord{20, true, 1, playerTarget(1), {}, 0, 0, Rules::freeRing(e.state(), e.catalog(), 1)});
    REQUIRE(e.state().decision); feedbackSend(e,1,Respond{e.state().decision->id,10,"respond",0,e.state().chain->links.front().id}); feedbackPass(e);
    CHECK(e.state().players[1].mana==20-paid); CHECK(e.state().cards.at(20).zone==Zone::Ash); CHECK(e.state().players[1].life==40);
}

namespace {
void feedbackWindow(MutationContext &s, PlayerId priority) {
    ChainLink root; root.id = 1; root.kind = LinkKind::Phase; root.item.owner = s.active;
    ChainState chain; chain.id = 1; chain.priority = priority; chain.window = ResponseWindow::Action;
    chain.links.push_back(root); s.chain = chain; s.nextLink = 2;
    s.decision = PendingDecision{1, priority, DecisionKind::Response, {}, true};
}
CardView feedbackView(const GameEngine &e, PlayerId viewer, CardId id) {
    const auto view = e.viewFor(viewer);
    const auto card = std::find_if(view.cards.begin(), view.cards.end(),
        [id](const CardView &entry) { return entry.instance.id == id; });
    REQUIRE(card != view.cards.end());
    return *card;
}
}

TEST_CASE("hand word responses require the owner's turn at every speed", "[hotfix-rules]") {
    for (const int speed : {2, 3}) for (const bool ownTurn : {false, true})
    for (const bool ambush : {false, true}) {
        CAPTURE(speed, ownTurn, ambush);
        const auto cat = feedbackCatalog();
        const char *definition = speed == 3 ? "v260-word-0009" : "v260-word-0018";
        REQUIRE(cat.at(definition).speed == speed);
        auto s = feedbackState(); s.active = ownTurn ? 0 : 1; feedbackWindow(s, 0);
        auto &word = feedbackCard(s, 10, definition, 0, ambush ? Zone::Analysis : Zone::Hand);
        word.faceDown = word.wasAmbushed = ambush; word.ambushedTurn = 1;
        auto e = test::ObservedEngine::scenario(cat, s);
        const Command cmd = ambush ? Command{FlipAmbush{10, 0, {}, 1}}
            : Command{PreloadWord{10, true, 1, 0, {}, 0, 1, 0}};
        const auto actions = e.viewFor(0).actions;
        CHECK(std::any_of(actions.begin(), actions.end(), [&](const LegalAction &action) {
            return encodeCommand(action.command) == encodeCommand(cmd);
        }) == (ownTurn || ambush));
        ActionIntent source; source.source = 10;
        const auto throughNetwork = net::decodeOptions(net::encodeOptions(e.queryActions(0, source)));
        const auto op = std::to_string(static_cast<int>(commandKind(cmd)));
        CHECK(std::any_of(throughNetwork.options.begin(), throughNetwork.options.end(), [&](const ActionOption &option) {
            return option.value == op;
        }) == (ownTurn || ambush));
        const auto before = e.digest();
        CHECK(e.submit(0, cmd).accepted == (ownTurn || ambush));
        if (!ownTurn && !ambush) {
            CHECK(e.digest() == before);
            for (const GameState *state : {static_cast<const GameState *>(&s), &e.state()}) {
                CHECK(state->cards.at(10).zone == Zone::Hand);
                CHECK(state->players[0].mana == 20);
                CHECK(Rules::load(*state, 0) == 0);
                REQUIRE(state->chain);
                CHECK(state->chain->links.size() == 1);
            }
        }
    }
}

TEST_CASE("Sunburst keeps its actual setting payment until it is set again", "[hotfix-rules]") {
    auto cat = feedbackCatalog();
    auto s = feedbackState();
    feedbackCard(s, 10, "v260-analytic-0009", 0, Zone::Hand);
    feedbackCard(s, 11, "v260-analytic-0009", 0, Zone::Hand);
    for (CardId id : {20, 21, 22}) feedbackCard(s, id, "v260-analytic-0001", 0, Zone::Ash);
    auto e = test::ObservedEngine::scenario(cat, s);
    feedbackSend(e, 0, StartAnalysis{10, 1, 0, {}, 0}); feedbackPass(e);
    REQUIRE(e.state().cards.at(10).settingPaid == 17);
    s = test::mutation(e.state());
    feedbackCard(s, 23, "v260-analytic-0001", 0, Zone::Ash);
    e = test::ObservedEngine::scenario(cat, s);
    for (PlayerId viewer : {0, 1}) CHECK(feedbackView(e, viewer, 10).effectiveSettingCost == 17);
    CHECK(feedbackView(e, 0, 11).effectiveSettingCost == 16);
    s.cards.at(23).zone = Zone::Hand; s.cards.at(22).zone = Zone::Hand;
    s.cards.at(10).spell = SpellState::Ready;
    e = test::ObservedEngine::scenario(cat, s);
    feedbackSend(e, 0, PrepareCast{10}); feedbackPass(e);
    CHECK(feedbackView(e, 0, 10).effectiveSettingCost == 17);
    s = test::mutation(e.state());
    card_rules::returnCard(s, cat, 10, true);
    s.players[0].mana = 20;
    e = test::ObservedEngine::scenario(cat, s);
    CHECK(feedbackView(e, 0, 10).effectiveSettingCost == 18);
    feedbackSend(e, 0, SetAmbush{10, 1, 0}); feedbackPass(e);
    CHECK(feedbackView(e, 1, 10).hidden);
    CHECK(feedbackView(e, 1, 10).effectiveSettingCost == 0);
    s = test::mutation(e.state()); ++s.globalTurn;
    e = test::ObservedEngine::scenario(cat, s);
    feedbackSend(e, 0, FlipAmbush{10}); feedbackPass(e);
    CHECK(e.state().cards.at(10).settingPaid == 18);
    CHECK(e.state().players[0].mana == 2);
}

TEST_CASE("word setting and activation payments honor each modifier and cost reference", "[hotfix-rules]") {
    for (const bool ambush : {false, true}) for (const bool counter : {false, true})
    for (const bool discount : {false, true}) {
        CAPTURE(ambush, counter, discount);
        auto cat = feedbackCatalog(); auto s = feedbackState();
        auto &def = cat.cards.at("v260-word-0009"); def.castCost = 1;
        if (discount) { cat.cards.at("v260-formation-0017").baseAnalysisDiscount = true;
            s.cards.at(1).base = true; def.school = "evocation"; }
        auto &word = feedbackCard(s, 10, "v260-word-0009", 0, ambush ? Zone::Analysis : Zone::Hand);
        word.faceDown = word.wasAmbushed = ambush; word.ambushedTurn = 1;
        if (ambush) {
            s.active = 1; feedbackWindow(s, 0);
        }
        if (counter) feedbackCard(s, 20, "v260-talisman-0004", 1, Zone::Casting, SpellState::Ready);
        auto e = test::ObservedEngine::scenario(cat, s);
        if (ambush) feedbackSend(e, 0, FlipAmbush{10, 0, {}, 1});
        else feedbackSend(e, 0, PreloadWord{10, true, 1, 0, {}, 0, 0, 0});
        const int setting = discount ? 1 : 2;
        CHECK(e.state().players[0].mana == 20 - setting - 1);
        CHECK(e.state().cards.at(10).settingPaid == setting);
        REQUIRE(e.state().cards.at(10).payments.size() == 1);
        CHECK(e.state().cards.at(10).payments.front().paid == 1);
        const auto view = feedbackView(e, 0, 10);
        CHECK(view.effectiveSettingCost == setting);
        CHECK(view.effectiveCastCost == 1);
        if (counter) {
            feedbackSend(e, 1, Respond{e.state().decision->id, 20, "respond", 0, e.state().chain->links.back().id});
            feedbackPass(e);
            CHECK(e.state().players[1].mana == 20 - setting - 1);
            CHECK(e.state().cards.at(10).zone == Zone::Ash);
        } else {
            s = test::mutation(e.state());
            feedbackCard(s, 30, "v260-seal-0002", 0, Zone::Attached).host = 10;
            e = test::ObservedEngine::scenario(cat, s);
            feedbackPass(e);
            CHECK(e.state().players[0].mana == 19);
        }
    }
}

TEST_CASE("selected host quote and zero setting payment survive a lost discount", "[hotfix-rules]") {
    auto cat = feedbackCatalog(); auto s = feedbackState();
    cat.cards.at("v260-formation-0017").baseAnalysisDiscount = true; s.cards.at(1).base = true;
    cat.cards.at("v260-analytic-0001").cost = 1;
    feedbackCard(s, 10, "v260-analytic-0001", 0, Zone::Hand);
    auto e = test::ObservedEngine::scenario(cat, s);
    REQUIRE(feedbackView(e, 0, 10).effectiveSettingCost == 1);
    ActionIntent intent;
    intent.selections = {{ActionField::Operation, std::to_string(static_cast<int>(CommandKind::StartAnalysis))},
                         {ActionField::Source, "10"}, {ActionField::Host, "1"}, {ActionField::Ring, "0"}};
    const auto options = e.queryActions(0, intent);
    REQUIRE(options.complete);
    CHECK(net::encodeOptions(options).value("costQuote", std::string{}).find("设置费用：0") != std::string::npos);
    feedbackSend(e, 0, *options.complete); feedbackPass(e);
    REQUIRE(e.state().players[0].mana == 20);
    s = test::mutation(e.state()); s.cards.at(1).base = false;
    e = test::ObservedEngine::scenario(cat, s);
    CHECK(feedbackView(e, 0, 10).effectiveSettingCost == 0);
}

TEST_CASE("confirmation quotes match formation recipes and rune attachment payments", "[hotfix-rules][action-quote]") {
    auto cat = feedbackCatalog();
    auto s = feedbackState();
    ActionIntent intent;
    std::string expected;
    int mana = 0, life = 0;
    SECTION("formation includes both recipe currencies alongside selected material") {
        s.cards.at(1).definition = "v260-formation-0009";
        auto &def = cat.cards.at("v260-formation-0010");
        def.recipe.push_back({RecipeCostKind::Life, 3});
        feedbackCard(s, 10, "v260-formation-0010", 0, Zone::SideDeck);
        intent.selections = {{ActionField::Operation, std::to_string(static_cast<int>(CommandKind::SetFormation))},
                             {ActionField::Source, "10"}, {ActionField::Replacement, "0"}, {ActionField::Material, "1"}};
        mana = 2; life = 3; expected = "设置费用：2 魔素，3 生命";
    }
    SECTION("attaching Arcane Ring quotes its printed mana cost") {
        feedbackCard(s, 10, "v260-seal-0002", 0, Zone::Hand);
        intent.selections = {{ActionField::Operation, std::to_string(static_cast<int>(CommandKind::AttachSeal))},
                             {ActionField::Source, "10"}, {ActionField::Host, "1"}};
        mana = 2; expected = "附着费用：2 魔素";
    }
    auto e = test::ObservedEngine::scenario(cat, s);
    const auto options = e.queryActions(0, intent);
    REQUIRE(options.complete);
    CHECK(options.costQuote == expected);
    feedbackSend(e, 0, *options.complete);
    CHECK(e.state().players[0].mana == s.players[0].mana - mana);
    CHECK(e.state().players[0].life == s.players[0].life - life);
}

TEST_CASE("all released concentration sources can be maintained or declined before income", "[hotfix-rules]") {
    const auto cat = feedbackCatalog();
    for (const auto &[id, fee] : std::vector<std::pair<const char *, int>>{
        {"v260-talisman-0005", 2}, {"v260-analytic-0013", 1}, {"v260-analytic-0008", 2},
        {"v260-analytic-0014", 1}, {"v260-analytic-0017", 1}, {"v260-analytic-0027", 4},
        {"v260-analytic-0028", 6}, {"v260-analytic-0031", 2}}) {
        for (const int mana : {fee - 1, fee}) for (const bool maintain : {false, true}) {
            if (maintain && mana < fee) continue;
            CAPTURE(id, fee, mana, maintain);
            auto s = feedbackState(); s.phase = Phase::Draw; s.players[0].mana = mana;
            auto &card = feedbackCard(s, 10, id, 0, Zone::Analysis, SpellState::Active);
            card.concentrationCost = fee; card.castLoad = 2;
            auto e = test::ObservedEngine::scenario(cat, s);
            feedbackSend(e, 0, AdvancePhase{e.state().phaseGate}); feedbackPass(e);
            REQUIRE(e.state().decision); REQUIRE(e.state().decision->kind == DecisionKind::Concentration);
            CHECK(e.state().decision->options.empty() == (mana < fee));
            if (mana < fee)
                CHECK(bridge::viewDto(e.viewFor(0)).at("decision").value("upkeepUnavailable", std::string{}) ==
                      "准备收入前的可用资源不足");
            CHECK(e.state().players[0].mana == mana);
            const int enemyLife = e.state().players[1].life;
            auto token = e.state().decision->id;
            feedbackSend(e, 0, Choose{token, maintain ? ChoiceValue::card(10) : ChoiceValue{}});
            CHECK_FALSE(e.submit(0, Choose{token, ChoiceValue::card(10)}).accepted);
            feedbackPass(e);
            CHECK(e.state().cards.at(10).zone == (maintain ? Zone::Analysis : Zone::Ash));
            CHECK(Rules::load(e.state(), 0) == (maintain ? 2 : 0));
            if (maintain) CHECK(e.state().cards.at(10).payments.back().paid == fee);
            if (std::string(id) == "v260-analytic-0013") CHECK(e.state().players[1].life == enemyLife - (maintain ? 2 : 0));
            if (std::string(id) == "v260-talisman-0005")
                CHECK(Rules::damage(e.state(), cat, 0, {EffectKind::Damage, 6, DamageType::Fire}) == (maintain ? 3 : 6));
            CHECK_FALSE(e.state().decision);
        }
    }
}
