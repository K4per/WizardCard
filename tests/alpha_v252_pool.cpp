#include "wizard/application.hpp"
#include "wizard/core/card_rules.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <set>

using namespace wizard;
namespace {
GameState poolState(Phase phase = Phase::Cast) {
    GameState s;
    s.phase = phase; s.phaseStep = PhaseStep::Body; s.phaseGate = 1;
    s.globalTurn = 5; s.rng = 73;
    for (auto &p : s.players) { p.mana = 20; p.ownTurn = 2; }
    return s;
}
CardInstance &poolCard(GameState &s, CardId id, const std::string &definition, PlayerId owner,
                       Zone zone, CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance c;
    c.id = id; c.definition = definition; c.owner = owner; c.zone = zone; c.spell = spell;
    c.host = zone == Zone::Analysis || zone == Zone::Attached ? host : 0;
    c.sourceFormation = host;
    if (zone == Zone::Analysis) c.fieldOrder = s.nextFieldOrder++;
    s.cards.emplace(id, c);
    if (zone == Zone::Deck) s.players[owner].deck.push_back(id);
    return s.cards.at(id);
}
void poolSend(GameEngine &e, PlayerId p, const Command &c) {
    const auto result = e.submit(p, c);
    INFO(result.error);
    REQUIRE(result.accepted);
}
void poolReject(GameEngine &e, PlayerId p, const Command &c) {
    const auto before = e.digest();
    const auto result = e.submit(p, c);
    REQUIRE_FALSE(result.accepted);
    REQUIRE(e.digest() == before);
}
void poolSettle(GameEngine &e) {
    for (int n = 0; n < 256 && e.state().decision && e.state().result == -1; ++n) {
        const auto d = *e.state().decision;
        if (d.kind == DecisionKind::Response) poolSend(e, d.player, PassResponse{d.id});
        else if (!d.options.empty()) poolSend(e, d.player, Choose{d.id, d.options.front()});
        else if (d.mayPass) poolSend(e, d.player, Choose{d.id, 0});
        else FAIL("An authoritative pool decision has no legal option");
    }
    REQUIRE((!e.state().decision || e.state().result != -1));
}
bool poolTrait(const CardDefinition &d, const std::string &name) {
    return std::find(d.traits.begin(), d.traits.end(), name) != d.traits.end();
}
std::vector<CardId> poolMaterials(GameState &s, const CardDefinition &d) {
    std::vector<CardId> materials;
    CardId id = 20;
    int spellIndex = 0;
    for (const auto &recipe : d.recipe) {
        if (recipe.kind == RecipeCostKind::Mana) continue;
        for (int n = 0; n < recipe.amount; ++n, ++id) {
            std::string material = recipe.definition;
            Zone zone = Zone::Hand;
            SpellState spell = SpellState::None;
            if (recipe.kind == RecipeCostKind::SacrificeFormation) {
                if (material.empty()) material = "hexagram";
                zone = Zone::Analysis;
            } else if (recipe.kind == RecipeCostKind::SacrificeSpell) {
                material = recipe.school == "necromancy" ? "false-life" :
                    recipe.distinctSchools && spellIndex++ ? "false-life" : "fireball";
                zone = Zone::Casting; spell = SpellState::Ready;
            } else if (material.empty()) material = "spark";
            auto &card = poolCard(s, id, material, 0, zone, 0, spell);
            if (material == "storm-crest") card.stormPoints = 10;
            materials.push_back(id);
        }
    }
    if (d.id == "sique-cauldron") poolCard(s, 1, "sique-campfire", 0, Zone::Analysis);
    if (d.id == "storm-heart") poolCard(s, 90, "great-storm", 0, Zone::Deck);
    return materials;
}
}

TEST_CASE("Alpha v2.5.2 every formal formation executes its printed recipe and rejects invalid timing", "[alpha-v252][pool]") {
    const auto cat = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
    int covered = 0;
    for (const auto &[id, d] : cat.cards) {
        if (d.type != CardType::Formation) continue;
        INFO(id);
        {
            ++covered;
            auto s = poolState(Phase::Main);
            poolCard(s, 10, id, 0, Zone::SideDeck);
            const auto materials = poolMaterials(s, d);
            const SetFormation command{10, 0, materials};
            poolCard(s, 200, "reservoir", 1, Zone::Analysis);
            auto badTarget = GameEngine::scenario(cat, s);
            poolReject(badTarget, 0, SetFormation{10, 200, materials});
            int fee = d.cost;
            for (const auto &recipe : d.recipe)
                if (recipe.kind == RecipeCostKind::Mana) fee += recipe.amount;
            if (fee > 0) {
                auto unpaidState = s; unpaidState.players[0].mana = 0;
                auto unpaid = GameEngine::scenario(cat, unpaidState);
                poolReject(unpaid, 0, command);
            }
            auto wrongPhase = s; wrongPhase.phase = Phase::Cast;
            auto wrong = GameEngine::scenario(cat, wrongPhase);
            poolReject(wrong, 0, command);
            if (!materials.empty()) {
                auto missing = GameEngine::scenario(cat, s);
                auto invalid = command; invalid.materials.pop_back();
                poolReject(missing, 0, invalid);
            }
            auto e = GameEngine::scenario(cat, s);
            poolSend(e, 0, command); poolSettle(e);
            REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
            REQUIRE(e.state().players[0].formations == 1);
            for (auto material : materials) REQUIRE(e.state().cards.at(material).zone == Zone::Ash);
            REQUIRE(e.state().players[0].mana == 20 - fee);
            poolReject(e, 0, command);
            REQUIRE(Rules::invariants(e.state(), cat).empty());
        }
    }
    REQUIRE(covered == 18);
}

TEST_CASE("Alpha v2.5.2 every formal seal attaches through authority with paid cost and valid host", "[alpha-v252][pool]") {
    const auto cat = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
    int covered = 0;
    for (const auto &[id, d] : cat.cards) {
        if (d.type != CardType::Seal) continue;
        INFO(id);
        {
            ++covered;
            auto s = poolState(Phase::Main);
            const bool sique = poolTrait(d, "sique_cauldron_only");
            const bool storm = poolTrait(d, "storm_formation_host_only") ||
                poolTrait(d, "storm_formation_host_restriction");
            poolCard(s, 1, sique ? "sique-cauldron" : storm ? "storm-crest" : "hexagram", 0, Zone::Analysis);
            poolCard(s, 2, "hexagram", 1, Zone::Analysis);
            poolCard(s, 10, id, 0, Zone::Hand);
            poolCard(s, 40, "sique-sugar", 0, Zone::Deck);
            poolCard(s, 41, "sique-roast", 0, Zone::Deck);
            auto e = GameEngine::scenario(cat, s);
            poolReject(e, 0, AttachSeal{10, 2});
            auto unpaidState = s; unpaidState.players[0].mana = 0;
            auto unpaid = GameEngine::scenario(cat, unpaidState);
            poolReject(unpaid, 0, AttachSeal{10, 1});
            poolSend(e, 0, AttachSeal{10, 1}); poolSettle(e);
            REQUIRE(e.state().cards.at(10).zone == Zone::Attached);
            REQUIRE(e.state().cards.at(10).host == 1);
            REQUIRE(e.state().players[0].mana == 20 - d.cost);
            poolReject(e, 0, AttachSeal{10, 1});
            REQUIRE(Rules::invariants(e.state(), cat).empty());
        }
    }
    REQUIRE(covered == 10);
}

TEST_CASE("Alpha v2.5.2 every ordinary formal spell resolves its source and meaningful effect choices", "[alpha-v252][pool]") {
    const auto cat = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
    const std::set<std::string> responseOnly{
        "barbs", "counter-spell", "wind-strike", "sique-boiling-surge", "arcana-devil"};
    std::set<std::string> covered;
    for (const auto &[id, d] : cat.cards) {
        if (d.type == CardType::Formation || d.type == CardType::Seal || responseOnly.count(id)) continue;
        INFO(id);
        {
            covered.insert(id);
            auto s = poolState();
            const bool sique = id.rfind("sique-", 0) == 0;
            const bool storm = poolTrait(d, "requires_storm_formation") ||
                poolTrait(d, "requires_storm_setup") || poolTrait(d, "requires_storm_heart_setup");
            poolCard(s, 1, sique ? "sique-cauldron" : storm ? "storm-heart" : "sun", 0, Zone::Analysis).stormPoints = 10;
            if (sique) poolCard(s, 3, "sique-campfire", 0, Zone::Analysis);
            poolCard(s, 2, id == "unravel" || id == "arcana-tower" ? "reservoir" : "elemental-ring", 1, Zone::Analysis);
            poolCard(s, 10, id, 0, Zone::Casting, id == "sique-roast" ? 3 : 1, SpellState::Ready);
            s.players[0].life = 30;
            const bool healing = !d.effects.empty() && d.effects.front().kind == EffectKind::Heal;
            CardId target = d.target == TargetKind::Player ? playerTarget(healing ? 0 : 1) : 0;
            CardId cost = 0;
            std::vector<CardId> targets;
            if (id != "arcana-fool") {
                poolCard(s, 11, "fireball", 0, Zone::Hand);
                poolCard(s, 12, "spark", 0, Zone::Hand);
            } else for (CardId card = 11; card < 16; ++card) poolCard(s, card, "spark", 0, Zone::Hand);
            if (id == "true-strike") cost = 11;
            if (id == "arcana-priestess") { s.cards.at(11).definition = "uplift"; cost = 11; }
            if (id == "arcana-emperor") {
                auto &donor = s.cards.at(11);
                donor.definition = "spark"; donor.zone = Zone::Analysis; donor.host = 1;
                donor.sourceFormation = 1; donor.ringIndex = 0; donor.spell = SpellState::Ready;
                cost = 11;
            }
            if (id == "arcana-tower") {
                s.cards.at(1).definition = "reservoir";
                target = 1;
            }
            if (id == "unravel") {
                target = 2;
                poolCard(s, 90, "small-hexagram", 0, Zone::SideDeck);
            }
            if (id == "funny-box") { poolCard(s, 20, "fireball", 1, Zone::Ash); target = 20; }
            else if (id != "unravel") {
                auto &enemy = poolCard(s, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Analyzing);
                enemy.ringIndex = 0; enemy.analysisProgress = 1;
                auto &hidden = poolCard(s, 21, "spark", 1, Zone::Analysis, 2);
                hidden.faceDown = hidden.wasAmbushed = true; hidden.ambushedTurn = 1; hidden.ringIndex = 1;
            }
            if (id == "instant-circuit-overload") {
                auto &analyzing = poolCard(s, 13, "arcana-chariot", 0, Zone::Analysis, 1, SpellState::Analyzing);
                analyzing.ringIndex = 1; target = 13;
            }
            for (CardId card = 40; card < 60; ++card)
                poolCard(s, card, card == 40 ? "arcana-chariot" : card == 41 ? "uplift" :
                    card == 42 ? "wind-strike" : card == 43 ? "sique-sugar" :
                    card == 44 ? "sique-salt" : "fireball", 0, Zone::Deck);
            for (CardId card = 60; card < 70; ++card) poolCard(s, card, "spark", 1, Zone::Deck);
            poolCard(s, 80, "sique-sugar", 0, Zone::Ash);
            poolCard(s, 81, "sique-roast", 0, Zone::Ash);
            poolCard(s, 82, "fireball", 0, Zone::Ash);
            poolCard(s, 83, "fireball", 0, Zone::Ash);
            poolCard(s, 84, "fireball", 0, Zone::Ash);
            poolCard(s, 85, "sique-sugar", 1, Zone::Ash);
            poolCard(s, 86, "fireball", 1, Zone::Ash);
            for (int n = 0; n < card_rules::detachCost(d); ++n) {
                const CardId seal = static_cast<CardId>(30 + n);
                poolCard(s, seal, n == 0 ? "sique-vinegar" : n == 1 ? "sique-sugar" : "sique-chili", 0, Zone::Attached, 1);
                targets.push_back(seal);
            }
            if (target) targets = {target};
            const ActivateSpell command{10, target, targets, cost};
            auto e = GameEngine::scenario(cat, s);
            // Every case rejects a malformed target before payment or effect mutation.
            poolReject(e, 0, ActivateSpell{10, 999999, {999999}, cost});
            const int fee = Rules::castCost(s, cat, 10);
            if (fee > 0) {
                auto unpaidState = s; unpaidState.players[0].mana = 0;
                auto unpaid = GameEngine::scenario(cat, unpaidState);
                poolReject(unpaid, 0, command);
            }
            poolSend(e, 0, command); poolSettle(e);
            REQUIRE(e.state().cards.at(10).spell != SpellState::Ready);
            REQUIRE(e.state().cards.at(10).payments.size() == 1);
            REQUIRE(e.state().cards.at(10).payments.front().paid == fee);
            if (!d.effects.empty() && d.effects.front().kind == EffectKind::Damage)
                REQUIRE(e.state().players[1].life == startingLife - d.effects.front().amount -
                    (id == "sique-blowfire" ? 4 : 0));
            if (healing) REQUIRE(e.state().players[0].life == 30 + d.effects.front().amount);
            if (id == "scorching-ray") {
                REQUIRE(e.state().players[1].life == startingLife - 5);
                REQUIRE(e.state().players[0].life == 20); // Later ray choices select the first player.
            }
            if (id == "chromatic-orb") REQUIRE(e.state().players[1].life == startingLife - 8);
            if (id == "false-life") REQUIRE(e.state().players[0].temporaryLife == 6);
            if (id == "aid") REQUIRE(e.state().players[0].temporaryLife == 5);
            if (e.state().result == -1) {
                REQUIRE_FALSE(e.state().effect);
                poolReject(e, 0, command);
            }
            for (int n = 0; n < card_rules::detachCost(d); ++n)
                REQUIRE(e.state().cards.at(static_cast<CardId>(30 + n)).zone == Zone::Ash);
            if (id == "instant-circuit-overload") REQUIRE(e.state().cards.at(13).analysisProgress == 2);
            if (id == "sique-pancakes") REQUIRE(e.state().players[0].siqueDamageCap == 4);
            if (id == "sique-divine-stew") REQUIRE(e.state().players[0].siqueLifePaymentTurn == s.globalTurn);
            if (id == "arcana-hierophant") REQUIRE(e.state().players[0].nextDamageImmune);
            if (id == "arcana-emperor") REQUIRE(e.state().cards.at(11).zone == Zone::Exile);
            if (id == "arcana-priestess" || id == "true-strike") REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
            if (id == "unravel") {
                REQUIRE(e.state().cards.at(2).zone == Zone::Ash);
                REQUIRE(e.state().cards.at(90).zone == Zone::Ash);
            }
            if (id == "arcana-tower") {
                REQUIRE(e.state().cards.at(1).zone == Zone::Ash);
                REQUIRE(e.state().cards.at(2).zone == Zone::Ash);
            }
            REQUIRE(Rules::invariants(e.state(), cat).empty());
        }
    }
    REQUIRE(covered.size() == 66);
    REQUIRE(covered.size() + responseOnly.size() + 18 + 10 == cat.cards.size());
}

TEST_CASE("Alpha v2.5.2 each conditional formal response joins its actual declared event", "[alpha-v252][pool]") {
    const auto cat = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
    for (const std::string id : {"barbs", "counter-spell", "wind-strike", "sique-boiling-surge", "arcana-devil"}) {
        INFO(id);
        {
            auto s = poolState(id == "wind-strike" || id == "barbs" ? Phase::Main : Phase::Cast);
            poolCard(s, 1, id == "wind-strike" ? "storm-heart" : id == "sique-boiling-surge" ? "sique-cauldron" : "sun", 1, Zone::Analysis);
            poolCard(s, 2, "hexagram", 0, Zone::Analysis);
            if (id == "sique-boiling-surge") poolCard(s, 3, "sique-campfire", 1, Zone::Analysis);
            const bool casting = id == "arcana-devil" || id == "sique-boiling-surge";
            auto &source = poolCard(s, 10, id, 1, casting ? Zone::Casting : Zone::Analysis, 1,
                casting ? SpellState::Ready : SpellState::None);
            if (!casting) {
                source.faceDown = source.wasAmbushed = true;
                source.ambushedTurn = 1; source.ringIndex = 0;
            }
            auto outside = GameEngine::scenario(cat, s);
            poolReject(outside, 1, Respond{1, 10, cat.at(id).responses.front().id, 0, 1});
            auto e = GameEngine::scenario(cat, s);
            // Open a real chain by an opponent action; do not manufacture a pending link.
            auto eventState = s;
            if (id == "wind-strike") poolCard(eventState, 20, "fireball", 0, Zone::Hand);
            else if (id == "barbs") {
                auto &spell = poolCard(eventState, 20, "fireball", 0, Zone::Analysis, 2, SpellState::Ready);
                spell.ringIndex = 0;
            } else poolCard(eventState, 20, id == "sique-boiling-surge" ? "unravel" : "spark", 0, Zone::Casting, 2, SpellState::Ready);
            e = GameEngine::scenario(cat, eventState);
            if (id == "wind-strike") poolSend(e, 0, StartAnalysis{20, 2});
            else if (id == "barbs") poolSend(e, 0, PrepareCast{20, playerTarget(1), 0, {playerTarget(1)}});
            else poolSend(e, 0, ActivateSpell{20, id == "sique-boiling-surge" ? 1 : playerTarget(1),
                {id == "sique-boiling-surge" ? 1 : playerTarget(1)}});
            REQUIRE(e.state().decision);
            REQUIRE(e.state().decision->kind == DecisionKind::Response);
            const auto view = e.viewFor(1);
            auto action = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &candidate) {
                const auto *r = std::get_if<Respond>(&candidate.command);
                return r && r->card == 10;
            });
            REQUIRE(action != view.actions.end());
            auto invalid = std::get<Respond>(action->command); invalid.link = 999999;
            poolReject(e, 1, invalid);
            const auto saved = action->command;
            poolSend(e, 1, saved); poolSettle(e);
            REQUIRE(e.state().cards.at(10).payments.size() == 1);
            if (id == "arcana-devil") {
                REQUIRE(e.state().players[0].life == startingLife - 3);
                REQUIRE(e.state().players[1].life == startingLife);
            } else if (id == "wind-strike") REQUIRE(e.state().players[0].life == startingLife - 6);
            else if (id == "sique-boiling-surge") {
                REQUIRE(e.state().cards.at(1).zone == Zone::Analysis);
                REQUIRE(e.state().cards.at(20).zone == Zone::Ash);
                REQUIRE(e.state().players[0].life == startingLife - 10);
            } else if (id == "counter-spell") REQUIRE(e.state().cards.at(20).zone == Zone::Ash);
            else REQUIRE(e.state().cards.at(20).zone != Zone::Casting);
            REQUIRE(Rules::invariants(e.state(), cat).empty());
        }
    }
}
