#include "wizard/application.hpp"
#include "wizard/command_codec.hpp"
#include "wizard/core/arcana.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;
namespace {
CardCatalog catalog() {
    auto result = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
    result.mechanicsRevision = 5;
    return result;
}
GameState stateAt(Phase phase = Phase::Main) {
    GameState state;
    state.phase = phase;
    state.phaseStep = PhaseStep::Body;
    state.phaseGate = 1;
    state.globalTurn = 2;
    state.rng = 17;
    for (auto &player : state.players) { player.mana = 20; player.ownTurn = 2; }
    return state;
}
CardInstance &put(GameState &state, CardId id, const std::string &definition, PlayerId owner, Zone zone,
                  CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance card;
    card.id = id; card.definition = definition; card.owner = owner; card.zone = zone;
    card.host = host; card.sourceFormation = host; card.spell = spell;
    if (zone == Zone::Analysis) card.fieldOrder = state.nextFieldOrder++;
    state.cards.emplace(id, card);
    return state.cards.at(id);
}
void send(GameEngine &engine, PlayerId player, const Command &command) {
    const auto result = engine.submit(player, command);
    INFO(result.error);
    REQUIRE(result.accepted);
}
void settle(GameEngine &engine) {
    for (int n = 0; n < 32 && engine.state().decision; ++n) {
        const auto decision = *engine.state().decision;
        if (decision.kind == DecisionKind::Response) send(engine, decision.player, PassResponse{decision.id});
        else if (decision.kind == DecisionKind::TriggerOrder)
            send(engine, decision.player, Choose{decision.id, decision.options.front()});
        else return;
    }
}
bool hasFlip(const GameView &view, CardId card) {
    return std::any_of(view.actions.begin(), view.actions.end(), [&](const LegalAction &action) {
        const auto *flip = std::get_if<FlipAmbush>(&action.command);
        return flip && flip->card == card;
    });
}
}

TEST_CASE("Alpha v2.5.2 formations and spells accept several paid seals with individual removal", "[alpha-v252][seals]") {
    auto cat = catalog();
    auto state = stateAt();
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "hexagram", 1, Zone::Analysis);
    put(state, 10, "conduit", 0, Zone::Hand);
    put(state, 11, "conduit", 0, Zone::Hand);
    put(state, 12, "uplift", 0, Zone::Hand);
    put(state, 13, "uplift", 0, Zone::Hand);
    put(state, 20, "fireball", 0, Zone::Analysis, 1, SpellState::Ready).ringIndex = 0;
    auto engine = GameEngine::scenario(cat, state);
    const int mana = engine.state().players[0].mana;
    send(engine, 0, AttachSeal{10, 1}); settle(engine);
    const auto view = engine.viewFor(0);
    REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *attach = std::get_if<AttachSeal>(&action.command);
        return attach && attach->card == 11 && attach->host == 1;
    }));
    send(engine, 0, AttachSeal{11, 1}); settle(engine);
    send(engine, 0, AttachSeal{12, 20}); settle(engine);
    send(engine, 0, AttachSeal{13, 20}); settle(engine);
    REQUIRE(engine.state().players[0].mana == mana - cat.at("conduit").cost * 2 - cat.at("uplift").cost * 2);
    REQUIRE(Rules::castCost(engine.state(), cat, 20) == cat.at("fireball").castCost + cat.at("uplift").castCostBonus * 2);
    send(engine, 0, RemoveSeal{12}); settle(engine);
    REQUIRE(engine.state().cards.at(12).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(13).zone == Zone::Attached);
    REQUIRE(engine.state().cards.at(13).host == 20);
    send(engine, 0, Abandon{20}); settle(engine);
    REQUIRE(engine.state().cards.at(13).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(13).host == 0);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Attached);
    REQUIRE(engine.state().cards.at(11).zone == Zone::Attached);
}

TEST_CASE("Alpha v2.5.2 removal is offered and shares the formation quota while cleaning hosted cards", "[alpha-v252][formations]") {
    auto cat = catalog();
    auto state = stateAt();
    put(state, 1, "hexagram", 0, Zone::Analysis).base = true;
    put(state, 2, "hexagram", 1, Zone::Analysis);
    put(state, 10, "fireball", 0, Zone::Analysis, 1, SpellState::Ready).ringIndex = 0;
    put(state, 11, "conduit", 0, Zone::Attached, 1);
    put(state, 12, "uplift", 0, Zone::Attached, 10);
    put(state, 13, "hexagram", 0, Zone::SideDeck);
    auto engine = GameEngine::scenario(cat, state);
    const auto view = engine.viewFor(0);
    REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *remove = std::get_if<RemoveFormation>(&action.command);
        return remove && remove->card == 1 && action.label.find("移除阵法") != std::string::npos;
    }));
    const int mana = engine.state().players[0].mana;
    send(engine, 0, decodeCommand(encodeCommand(RemoveFormation{1}))); settle(engine);
    for (CardId id : {1u, 10u, 11u, 12u}) {
        REQUIRE(engine.state().cards.at(id).zone == Zone::Ash);
        REQUIRE(engine.state().cards.at(id).host == 0);
    }
    REQUIRE(engine.state().players[0].mana == mana);
    REQUIRE(engine.state().players[0].formations == 1);
    REQUIRE_FALSE(engine.submit(0, SetFormation{13}).accepted);
    REQUIRE_FALSE(engine.submit(1, RemoveFormation{2}).accepted);
}

TEST_CASE("Alpha v2.5.2 unconfigured Storm Vortex flips on the immediately following opponent turn", "[alpha-v252][ambush]") {
    const auto cat = catalog();
    for (PlayerId owner : {0, 1}) {
        auto state = stateAt();
        state.active = 1 - owner;
        put(state, 1, "storm-crest", owner, Zone::Analysis);
        put(state, 2, "hexagram", 1 - owner, Zone::Analysis);
        auto &hidden = put(state, 10, "storm-vortex", owner, Zone::Analysis, 1);
        hidden.faceDown = hidden.wasAmbushed = true;
        hidden.ambushedTurn = 1;
        hidden.ringIndex = 0;
        put(state, 20, "fireball", 1 - owner, Zone::Analysis, 2, SpellState::Ready).ringIndex = 0;
        put(state, 30, "spark", 1 - owner, Zone::Hand);
        auto engine = GameEngine::scenario(cat, state);
        send(engine, 1 - owner, SetAmbush{30, 2});
        REQUIRE(engine.state().decision);
        REQUIRE(engine.state().decision->player == owner);
        REQUIRE(hasFlip(engine.viewFor(owner), 10));
        const auto token = engine.state().decision->id;
        const int mana = engine.state().players[owner].mana;
        send(engine, owner, FlipAmbush{10, 0, {}, token});
        const auto digest = engine.digest();
        REQUIRE_FALSE(engine.submit(owner, FlipAmbush{10, 0, {}, token}).accepted);
        REQUIRE(engine.digest() == digest);
        REQUIRE(engine.state().players[owner].mana == mana - cat.at("storm-vortex").cost);
        settle(engine);
        REQUIRE(engine.state().decision);
        REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
        send(engine, owner, Choose{engine.state().decision->id, 20}); settle(engine);
        REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    }
}

TEST_CASE("Alpha v2.5.2 generic ambush activation keeps wait speed priority cost and exclusive conditions", "[alpha-v252][ambush]") {
    const auto base = catalog();
    for (int invalid = 0; invalid < 7; ++invalid) {
        auto cat = base;
        auto state = stateAt(invalid == 1 ? Phase::Draw : Phase::Main);
        state.active = 1;
        put(state, 1, "storm-crest", 0, Zone::Analysis);
        put(state, 2, "hexagram", 1, Zone::Analysis);
        auto &hidden = put(state, 10, "storm-vortex", 0, Zone::Analysis, 1);
        hidden.faceDown = hidden.wasAmbushed = true; hidden.ambushedTurn = invalid == 0 ? 2 : 1;
        hidden.ringIndex = 0;
        if (invalid == 1) cat.cards.at("storm-vortex").speed = 2;
        if (invalid == 2) state.players[0].mana = 0;
        if (invalid == 3) cat.cards.at("storm-vortex").responseOnly = true;
        if (invalid == 4) state.cards.at(1).definition = "hexagram";
        ChainLink phase; phase.id = 1; phase.kind = LinkKind::Phase; phase.item.owner = 1;
        phase.speed = invalid == 5 ? 4 : 0;
        ChainState chain; chain.id = 1; chain.initiator = 1; chain.priority = invalid == 6 ? 1 : 0;
        chain.window = ResponseWindow::Action; chain.links.push_back(phase);
        state.chain = chain;
        state.decision = PendingDecision{1, 0, DecisionKind::Response, {10}, true};
        state.nextLink = 2;
        auto engine = GameEngine::scenario(cat, state);
        const auto unchanged = engine.digest();
        const auto result = engine.submit(0, FlipAmbush{10, 0, {}, 1});
        REQUIRE_FALSE(result.accepted);
        if (invalid != 6) REQUIRE(result.error.find("priority") == std::string::npos);
        REQUIRE(engine.digest() == unchanged);
        REQUIRE_FALSE(hasFlip(engine.viewFor(0), 10));
    }
}


namespace {
void settleChoices(GameEngine &engine) {
    for (int n = 0; n < 96 && engine.state().decision && engine.state().result == -1; ++n) {
        const auto choice = *engine.state().decision;
        if (choice.kind == DecisionKind::Response) send(engine, choice.player, PassResponse{choice.id});
        else if (choice.mayPass && choice.options.empty()) send(engine, choice.player, Choose{choice.id, 0});
        else if (!choice.options.empty()) send(engine, choice.player, Choose{choice.id, choice.options.front()});
        else FAIL("Unresolvable decision in Arcana regression");
    }
}
CardCatalog arcanaCatalog() {
    auto cat = catalog();
    // A generous test host isolates card effects from unrelated host-rank/overload rules.
    auto &host = cat.cards.at("hexagram");
    host.capacity = 50; host.rings = 8; host.maxRank = 8; host.level = 8;
    return cat;
}
void deckCard(GameState &state, CardId id, const std::string &definition, PlayerId owner = 0) {
    put(state, id, definition, owner, Zone::Deck);
    state.players[owner].deck.push_back(id);
}
}

TEST_CASE("Alpha v2.5.2 every nonconditional Arcana source reaches normal authoritative resolution", "[alpha-v252][arcana]") {
    const auto cat = arcanaCatalog();
    const std::vector<std::string> cards{
        "arcana-fool", "arcana-magician", "arcana-priestess", "arcana-empress", "arcana-emperor",
        "arcana-hierophant", "arcana-lovers", "arcana-chariot", "arcana-strength", "arcana-hermit",
        "arcana-justice", "arcana-hanged-man", "arcana-death", "arcana-temperance", "arcana-tower", "arcana-world",
        "earthquake"};
    for (const auto &definition : cards) {
        INFO(definition);
        auto state = stateAt(Phase::Cast);
        put(state, 1, "hexagram", 0, Zone::Analysis);
        put(state, 2, "hexagram", 1, Zone::Analysis);
        put(state, 10, definition, 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
        CardId cost = 0;
        CardId target = cat.at(definition).target == TargetKind::Player ? playerTarget(1) : 0;
        if (definition == "arcana-tower") {
            state.cards.at(1).definition = "reservoir";
            state.cards.at(2).definition = "reservoir";
            target = 1;
        }
        if (definition == "arcana-fool")
            for (CardId id = 11; id < 16; ++id) put(state, id, "spark", 0, Zone::Hand);
        if (definition == "arcana-priestess") {
            put(state, 11, "uplift", 0, Zone::Hand); cost = 11;
        }
        if (definition == "arcana-emperor") {
            put(state, 11, "spark", 0, Zone::Analysis, 1, SpellState::Ready).ringIndex = 0; cost = 11;
        }
        if (definition == "arcana-hermit" || definition == "arcana-justice" || definition == "arcana-lovers") {
            put(state, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Ready).ringIndex = 0;
            put(state, 21, "spark", 1, Zone::Analysis, 2, SpellState::Ready).ringIndex = 1;
        }
        deckCard(state, 30, "arcana-chariot");
        deckCard(state, 31, "spark");
        deckCard(state, 32, "uplift");
        deckCard(state, 33, "arcana-hierophant");
        put(state, 40, "fireball", 0, Zone::Ash);
        put(state, 41, "hexagram", 0, Zone::Ash);
        deckCard(state, 50, "spark", 1);
        put(state, 51, "fireball", 1, Zone::Ash);
        auto engine = GameEngine::scenario(cat, state);
        const auto command = ActivateSpell{10, target, target ? std::vector<CardId>{target} : std::vector<CardId>{}, cost};
        auto invalidTarget = GameEngine::scenario(cat, state);
        REQUIRE_FALSE(invalidTarget.submit(0, ActivateSpell{10, 2, {2}, cost}).accepted);
        auto noPriorityState = state;
        noPriorityState.active = 1;
        auto noPriority = GameEngine::scenario(cat, noPriorityState);
        REQUIRE_FALSE(noPriority.submit(0, command).accepted);
        if (Rules::castCost(state, cat, 10) > 0) {
            auto unpaidState = state;
            unpaidState.players[0].mana = 0;
            auto unpaid = GameEngine::scenario(cat, unpaidState);
            const auto unchanged = unpaid.digest();
            REQUIRE_FALSE(unpaid.submit(0, command).accepted);
            REQUIRE(unpaid.digest() == unchanged);
        }
        send(engine, 0, command);
        settleChoices(engine);
        REQUIRE(engine.state().cards.at(10).spell != SpellState::Ready);
        REQUIRE_FALSE(engine.submit(0, command).accepted);
        REQUIRE(Rules::invariants(engine.state(), cat).empty());
        if (definition == "arcana-fool") {
            REQUIRE(std::count_if(engine.state().cards.begin(), engine.state().cards.end(), [](const auto &entry) {
                return entry.second.owner == 0 && entry.second.zone == Zone::Hand;
            }) == 5);
        } else if (definition == "arcana-magician") {
            REQUIRE(std::count_if(engine.state().cards.begin(), engine.state().cards.end(), [](const auto &entry) {
                return entry.second.zone == Zone::Exile;
            }) == 2);
        } else if (definition == "arcana-priestess") {
            REQUIRE(engine.state().cards.at(11).zone == Zone::Ash);
            REQUIRE(engine.state().cards.at(32).zone == Zone::Hand);
        } else if (definition == "arcana-emperor") {
            REQUIRE(engine.state().cards.at(11).zone == Zone::Exile);
            REQUIRE(engine.state().cards.at(30).zone == Zone::Hand);
        } else if (definition == "arcana-chariot") REQUIRE(engine.state().players[1].life == 20);
        else if (definition == "arcana-hermit") REQUIRE(engine.state().cards.at(20).faceDown);
        else if (definition == "arcana-justice") {
            REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
            REQUIRE(engine.state().cards.at(21).zone == Zone::Ash);
        } else if (definition == "arcana-hanged-man") {
            REQUIRE(engine.state().players[0].life == 20);
            REQUIRE(engine.state().cards.at(40).zone == Zone::Deck);
            REQUIRE(engine.state().cards.at(41).zone == Zone::SideDeck);
            REQUIRE(engine.state().cards.at(30).zone == Zone::Ash);
        } else if (definition == "arcana-death") REQUIRE(engine.state().players[1].life == 0);
        else if (definition == "arcana-tower") {
            REQUIRE(engine.state().cards.at(1).zone == Zone::Ash);
            REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
        } else if (definition == "arcana-world") {
            for (CardId id : {30u, 31u, 32u, 33u}) REQUIRE(engine.state().cards.at(id).zone == Zone::Hand);
        }
    }
}

TEST_CASE("Alpha v2.5.2 Arcana costs and conditions reject before payment or zone mutation", "[alpha-v252][arcana]") {
    const auto cat = arcanaCatalog();
    for (const std::string definition : {"arcana-fool", "arcana-magician", "arcana-priestess", "arcana-emperor", "arcana-hanged-man", "arcana-tower"}) {
        INFO(definition);
        auto state = stateAt(Phase::Cast);
        put(state, 1, "hexagram", 0, Zone::Analysis);
        put(state, 2, "hexagram", 1, Zone::Analysis);
        put(state, 10, definition, 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
        if (definition == "arcana-hanged-man") state.players[0].life = 1;
        auto engine = GameEngine::scenario(cat, state);
        const auto digest = engine.digest();
        REQUIRE_FALSE(engine.submit(0, ActivateSpell{10}).accepted);
        REQUIRE(engine.digest() == digest);
    }
}

TEST_CASE("Alpha v2.5.2 Arcana analysis hooks count events and preserve exile distinction", "[alpha-v252][arcana]") {
    const auto cat = arcanaCatalog();
    auto state = stateAt();
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "hexagram", 1, Zone::Analysis);
    put(state, 10, "arcana-chariot", 0, Zone::Analysis, 1, SpellState::Analyzing).ringIndex = 0;
    put(state, 11, "arcana-death", 0, Zone::Analysis, 1, SpellState::Analyzing).ringIndex = 1;
    put(state, 12, "arcana-devil", 0, Zone::Analysis, 1, SpellState::Analyzing).ringIndex = 2;
    put(state, 13, "fireball", 0, Zone::Analysis, 1, SpellState::Analyzing).ringIndex = 3;
    put(state, 20, "spark", 1, Zone::Hand);
    put(state, 21, "spark", 1, Zone::Hand);
    arcana::advance(state, cat, 13, 5);
    REQUIRE(state.cards.at(10).analysisProgress == 1);
    StateMaintenance::leave(state, 20, &cat);
    REQUIRE(state.cards.at(11).analysisProgress == 1);
    REQUIRE(state.cards.at(10).analysisProgress == 2);
    arcana::erase(state, cat, 21);
    REQUIRE(state.cards.at(21).zone == Zone::Exile);
    REQUIRE(state.cards.at(11).analysisProgress == 1);
    Trigger damage; damage.owner = 1; damage.targetPlayer = 0;
    EffectResolver::apply(state, cat, damage, {EffectKind::Damage, 1, DamageType::Fire});
    REQUIRE(state.cards.at(12).analysisProgress == 1);
    REQUIRE(state.cards.at(10).analysisProgress == 3);
}

TEST_CASE("Alpha v2.5.2 generic ready spells can join chains without response configuration on either turn", "[alpha-v252][ambush]") {
    auto cat = arcanaCatalog();
    cat.cards.at("spark").speed = 3;
    for (PlayerId owner : {0, 1}) {
        auto state = stateAt(); state.active = 1 - owner;
        put(state, 1, "hexagram", owner, Zone::Analysis);
        put(state, 2, "hexagram", 1 - owner, Zone::Analysis);
        put(state, 10, "spark", owner, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
        put(state, 20, "spark", 1 - owner, Zone::Hand);
        auto engine = GameEngine::scenario(cat, state);
        send(engine, 1 - owner, SetAmbush{20, 2});
        REQUIRE(engine.state().decision);
        const auto view = engine.viewFor(owner);
        const auto action = std::find_if(view.actions.begin(), view.actions.end(), [&](const LegalAction &candidate) {
            const auto *response = std::get_if<Respond>(&candidate.command);
            return response && response->card == 10 && response->ability == "__activate_spell" &&
                response->target == playerTarget(1 - owner);
        });
        REQUIRE(action != view.actions.end());
        send(engine, owner, action->command);
        settleChoices(engine);
        REQUIRE(engine.state().players[1 - owner].life == 40 - cat.at("spark").effects.front().amount);
        REQUIRE(engine.state().cards.at(10).payments.size() == 1);
    }
}

TEST_CASE("Alpha v2.5.2 Wind Strike responds only to the opponent setting an analytic spell", "[alpha-v252][ambush]") {
    auto cat = arcanaCatalog();
    auto state = stateAt();
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "storm-crest", 1, Zone::Analysis).stormPoints = 5;
    auto &hidden = put(state, 10, "wind-strike", 1, Zone::Analysis, 2);
    hidden.faceDown = hidden.wasAmbushed = true; hidden.ambushedTurn = 1; hidden.ringIndex = 0;
    put(state, 20, "fireball", 0, Zone::Hand);
    auto engine = GameEngine::scenario(cat, state);
    REQUIRE_FALSE(hasFlip(engine.viewFor(1), 10));
    send(engine, 0, StartAnalysis{20, 1});
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->player == 1);
    const auto view = engine.viewFor(1);
    const auto response = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *r = std::get_if<Respond>(&action.command); return r && r->card == 10;
    });
    REQUIRE(response != view.actions.end());
    auto slowCatalog = cat;
    slowCatalog.cards.at("wind-strike").speed = 1;
    auto slow = GameEngine::scenario(slowCatalog, engine.state());
    REQUIRE_FALSE(slow.submit(1, response->command).accepted);
    auto wrongEventState = engine.state();
    wrongEventState.chain->window = ResponseWindow::Action;
    auto wrongEvent = GameEngine::scenario(cat, wrongEventState);
    REQUIRE_FALSE(wrongEvent.submit(1, response->command).accepted);
    send(engine, 1, response->command); settleChoices(engine);
    REQUIRE(engine.state().players[0].life == 40 - cat.at("fireball").rank * 2);
}


TEST_CASE("Alpha v2.5.2 Devil changes an opponent selected target and charges once", "[alpha-v252][arcana]") {
    const auto cat = arcanaCatalog();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "hexagram", 1, Zone::Analysis);
    put(state, 10, "spark", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
    put(state, 20, "arcana-devil", 1, Zone::Casting, 0, SpellState::Ready).sourceFormation = 2;
    auto engine = GameEngine::scenario(cat, state);
    send(engine, 0, ActivateSpell{10, playerTarget(1), {playerTarget(1)}});
    REQUIRE(engine.state().decision);
    const auto view = engine.viewFor(1);
    const auto response = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *r = std::get_if<Respond>(&action.command);
        return r && r->card == 20 && r->ability == "retarget_spell";
    });
    REQUIRE(response != view.actions.end());
    send(engine, 1, response->command); settleChoices(engine);
    REQUIRE(engine.state().players[0].life == 40 - cat.at("spark").effects.front().amount);
    REQUIRE(engine.state().players[1].life == 40);
    REQUIRE(engine.state().players[1].mana == 14);
    REQUIRE(Rules::load(engine.state(), 1) == 12);
    REQUIRE(std::any_of(engine.state().temporary.begin(), engine.state().temporary.end(), [](const TemporaryLoad &load) {
        return load.owner == 1 && load.amount == 6 && load.independent;
    }));
    REQUIRE(engine.state().cards.at(20).payments.size() == 1);
}

TEST_CASE("Alpha v2.5.2 Lovers departure forces the opponent to choose one analytic-zone spell", "[alpha-v252][arcana]") {
    const auto cat = arcanaCatalog();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "hexagram", 1, Zone::Analysis);
    put(state, 10, "arcana-lovers", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
    put(state, 20, "fireball", 1, Zone::Analysis, 2, SpellState::Ready).ringIndex = 0;
    auto engine = GameEngine::scenario(cat, state);
    send(engine, 0, ActivateSpell{10}); settleChoices(engine);
    send(engine, 0, Abandon{10});
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->player == 1);
    settleChoices(engine);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(10).remaining == 0);
}

TEST_CASE("Alpha v2.5.2 Hierophant absorbs one event Strength doubles damage and Temperance refunds one per payment", "[alpha-v252][arcana]") {
    const auto cat = arcanaCatalog();
    auto state = stateAt();
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "hexagram", 1, Zone::Analysis);
    put(state, 10, "arcana-hierophant", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
    put(state, 20, "arcana-strength", 1, Zone::Analysis, 2, SpellState::Active).ringIndex = 0;
    put(state, 21, "arcana-temperance", 0, Zone::Analysis, 1, SpellState::Active).ringIndex = 0;
    auto engine = GameEngine::scenario(cat, state);
    send(engine, 0, ActivateSpell{10}); settleChoices(engine);
    REQUIRE(engine.state().players[0].nextDamageImmune);
    REQUIRE(engine.state().players[0].mana == 15);
    auto damaged = engine.state();
    Trigger trigger; trigger.owner = 1; trigger.targetPlayer = 0;
    EffectResolver::apply(damaged, cat, trigger, {EffectKind::Damage, 5});
    REQUIRE(damaged.players[0].life == 40);
    REQUIRE_FALSE(damaged.players[0].nextDamageImmune);
    EffectResolver::apply(damaged, cat, trigger, {EffectKind::Damage, 5});
    REQUIRE(damaged.players[0].life == 30);
    send(engine, 0, AdvancePhase{engine.state().phaseGate, true}); settleChoices(engine);
    REQUIRE(engine.state().globalTurn == state.globalTurn + 1);
    REQUIRE_FALSE(engine.state().players[0].nextDamageImmune);
}

TEST_CASE("Alpha v2.5.2 Earthquake removes both players concealed spells on its main-phase trigger", "[alpha-v252][arcana]") {
    const auto cat = arcanaCatalog();
    auto state = stateAt(Phase::Prepare);
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "hexagram", 1, Zone::Analysis);
    auto &earthquake = put(state, 10, "earthquake", 0, Zone::Analysis, 1, SpellState::Active);
    earthquake.ringIndex = 0; earthquake.remaining = 2;
    for (PlayerId owner : {0, 1}) {
        auto &hidden = put(state, 20 + owner, "spark", owner, Zone::Analysis, owner + 1);
        hidden.ringIndex = owner ? 0 : 1;
        hidden.faceDown = hidden.wasAmbushed = true; hidden.ambushedTurn = state.globalTurn;
    }
    auto engine = GameEngine::scenario(cat, state);
    send(engine, 0, AdvancePhase{1}); settleChoices(engine);
    REQUIRE(engine.state().phase == Phase::Main);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(21).zone == Zone::Ash);
}

TEST_CASE("Alpha v2.5.2 Unravel completes setup analysis targetless preparation and side deck resolution", "[alpha-v252][unravel]") {
    const auto cat = arcanaCatalog();
    auto state = stateAt();
    put(state, 1, "hexagram", 0, Zone::Analysis);
    put(state, 2, "reservoir", 1, Zone::Analysis);
    put(state, 10, "unravel", 0, Zone::Hand);
    put(state, 20, "small-hexagram", 0, Zone::SideDeck);
    for (CardId id = 30; id < 38; ++id) deckCard(state, id, "spark", id % 2);
    auto engine = GameEngine::scenario(cat, state);
    send(engine, 0, StartAnalysis{10, 1}); settleChoices(engine);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Analyzing);
    for (int step = 0; step < 16 && !(engine.state().active == 0 && engine.state().phase == Phase::Main &&
             engine.state().cards.at(10).spell == SpellState::Ready); ++step) {
        send(engine, engine.state().active, AdvancePhase{engine.state().phaseGate}); settleChoices(engine);
    }
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Ready);
    const auto view = engine.viewFor(0);
    REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &a) {
        const auto *prepare = std::get_if<PrepareCast>(&a.command);
        return prepare && prepare->card == 10 && !prepare->target && prepare->targets.empty();
    }));
    send(engine, 0, PrepareCast{10}); settleChoices(engine);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Casting);
    send(engine, 0, AdvancePhase{engine.state().phaseGate}); settleChoices(engine);
    send(engine, 0, ActivateSpell{10, 2, {2}}); settleChoices(engine);
    REQUIRE(engine.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Resolved);
}

TEST_CASE("Alpha v2.5.2 Unravel prepares without an enemy target and rejects fabricated preparation targets", "[alpha-v252][unravel]") {
    const auto cat = catalog();
    auto state = stateAt();
    put(state, 1, "sun", 0, Zone::Analysis);
    put(state, 10, "unravel", 0, Zone::Analysis, 1, SpellState::Ready).ringIndex = 0;
    put(state, 20, "small-hexagram", 0, Zone::SideDeck);
    auto engine = GameEngine::scenario(cat, state);
    for (const auto &command : {PrepareCast{10, 1, 0, {1}}, PrepareCast{10, 0, 0, {1}}}) {
        const auto before = engine.digest();
        REQUIRE_FALSE(engine.submit(0, command).accepted);
        REQUIRE(engine.digest() == before);
    }
    const auto view = engine.viewFor(0);
    REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *prepare = std::get_if<PrepareCast>(&action.command);
        return prepare && prepare->card == 10 && !prepare->target && prepare->targets.empty();
    }));
    send(engine, 0, PrepareCast{10});
    settleChoices(engine);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Casting);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Ready);
    REQUIRE(engine.state().players[0].mana == 20);
    send(engine, 0, AdvancePhase{engine.state().phaseGate}); settleChoices(engine);
    const auto before = engine.digest();
    REQUIRE_FALSE(engine.submit(0, ActivateSpell{10}).accepted);
    REQUIRE(engine.digest() == before);
    REQUIRE(engine.state().cards.at(20).zone == Zone::SideDeck);
    REQUIRE(engine.state().cards.at(10).payments.empty());
}

TEST_CASE("Alpha v2.5.2 Unravel rejects occupied enemy formations before paying", "[alpha-v252][unravel]") {
    const auto cat = catalog();
    for (bool concealed : {false, true}) {
        INFO(concealed);
        auto state = stateAt(Phase::Cast);
        put(state, 1, "sun", 0, Zone::Analysis);
        put(state, 2, "elemental-ring", 1, Zone::Analysis);
        put(state, 10, "unravel", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
        auto &occupant = put(state, 11, "fireball", 1, Zone::Analysis, 2, SpellState::Ready);
        occupant.ringIndex = 0;
        occupant.faceDown = occupant.wasAmbushed = concealed;
        occupant.ambushedTurn = concealed ? 1 : 0;
        if (concealed) occupant.spell = SpellState::None;
        put(state, 20, "small-hexagram", 0, Zone::SideDeck);
        auto engine = GameEngine::scenario(cat, state);
        const auto before = engine.digest();
        REQUIRE_FALSE(engine.submit(0, ActivateSpell{10, 2, {2}}).accepted);
        REQUIRE(engine.digest() == before);
        REQUIRE(engine.state().cards.at(20).zone == Zone::SideDeck);
        REQUIRE(engine.state().cards.at(10).payments.empty());
    }
}

TEST_CASE("Alpha v2.5.2 Unravel may activate with an empty side deck but cannot destroy", "[alpha-v252][unravel]") {
    const auto cat = catalog();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "sun", 0, Zone::Analysis);
    put(state, 2, "reservoir", 1, Zone::Analysis);
    put(state, 10, "unravel", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
    auto engine = GameEngine::scenario(cat, state);
    const int fee = Rules::castCost(state, cat, 10);
    REQUIRE(fee == 1);
    send(engine, 0, ActivateSpell{10, 2, {2}}); settleChoices(engine);
    REQUIRE(engine.state().cards.at(2).zone == Zone::Analysis);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Resolved);
    REQUIRE(engine.state().players[0].mana == 20 - fee);
    REQUIRE(engine.state().cards.at(10).payments.size() == 1);
    REQUIRE(engine.state().cards.at(10).payments.front().paid == fee);
    REQUIRE_FALSE(engine.state().cards.at(10).payments.front().canceled);
    REQUIRE(std::any_of(engine.state().events.begin(), engine.state().events.end(), [](const GameEvent &event) {
        return event.kind == "effect_incomplete" && event.card == 10;
    }));
}

TEST_CASE("Alpha v2.5.2 Unravel loses an invalidated chain target without a refund or side deck discard", "[alpha-v252][unravel]") {
    const auto cat = catalog();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "sun", 0, Zone::Analysis);
    put(state, 2, "reservoir", 1, Zone::Analysis);
    put(state, 3, "storm-crest", 1, Zone::Analysis).stormPoints = 5;
    put(state, 10, "unravel", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
    put(state, 20, "storm-guidance", 1, Zone::Casting, 0, SpellState::Ready).sourceFormation = 3;
    put(state, 30, "small-hexagram", 0, Zone::SideDeck);
    auto engine = GameEngine::scenario(cat, state);
    const int fee = Rules::castCost(state, cat, 10);
    send(engine, 0, ActivateSpell{10, 2, {2}});
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->player == 1);
    const auto view = engine.viewFor(1);
    const auto response = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
        const auto *r = std::get_if<Respond>(&action.command);
        return r && r->card == 20 && r->ability == "__activate_spell";
    });
    REQUIRE(response != view.actions.end());
    send(engine, 1, response->command); settle(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    REQUIRE(std::find(engine.state().decision->options.begin(), engine.state().decision->options.end(), 2) !=
            engine.state().decision->options.end());
    send(engine, 1, Choose{engine.state().decision->id, 2}); settleChoices(engine);
    REQUIRE(engine.state().cards.at(2).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(30).zone == Zone::SideDeck);
    REQUIRE(engine.state().players[0].mana == 20 - fee);
    REQUIRE(engine.state().cards.at(10).payments.size() == 1);
    REQUIRE(engine.state().cards.at(10).payments.front().paid == fee);
    REQUIRE(engine.state().cards.at(10).payments.front().canceled);
    REQUIRE(std::none_of(engine.state().events.begin(), engine.state().events.end(), [](const GameEvent &event) {
        return event.kind == "effect_incomplete" && event.card == 10;
    }));
}

TEST_CASE("Alpha v2.5.2 unique Strength rejects same owner setup duplicates independently for each player", "[alpha-v252][arcana]") {
    const auto cat = catalog();
    for (PlayerId owner : {0, 1}) for (Zone existingZone : {Zone::Analysis, Zone::Casting}) {
        INFO(owner); INFO(static_cast<int>(existingZone));
        auto state = stateAt(); state.active = owner;
        put(state, 1, "sun", owner, Zone::Analysis);
        put(state, 2, "sun", 1 - owner, Zone::Analysis);
        auto &existing = put(state, 10, "arcana-strength", owner, existingZone,
                             existingZone == Zone::Analysis ? 1 : 0, SpellState::Active);
        existing.sourceFormation = 1;
        if (existingZone == Zone::Analysis) existing.ringIndex = 0;
        put(state, 11, "arcana-strength", owner, Zone::Hand);
        auto engine = GameEngine::scenario(cat, state);
        for (const Command &command : {Command{StartAnalysis{11, 1}}, Command{SetAmbush{11, 1}}}) {
            const auto before = engine.digest();
            const auto result = engine.submit(owner, command);
            REQUIRE_FALSE(result.accepted);
            REQUIRE(result.error.find("unique") != std::string::npos);
            REQUIRE(engine.digest() == before);
        }
        auto hiddenState = state;
        auto &hidden = hiddenState.cards.at(11);
        hidden.zone = Zone::Analysis; hidden.host = hidden.sourceFormation = 1;
        hidden.ringIndex = 1; hidden.faceDown = hidden.wasAmbushed = true; hidden.ambushedTurn = 1;
        auto hiddenEngine = GameEngine::scenario(cat, hiddenState);
        const auto before = hiddenEngine.digest();
        const auto result = hiddenEngine.submit(owner, FlipAmbush{11});
        REQUIRE_FALSE(result.accepted);
        REQUIRE(result.error.find("unique") != std::string::npos);
        REQUIRE(hiddenEngine.digest() == before);
    }
    for (PlayerId owner : {0, 1}) for (bool ambush : {false, true}) {
        INFO(owner); INFO(ambush);
        auto state = stateAt(); state.active = owner;
        put(state, 1, "sun", owner, Zone::Analysis);
        put(state, 2, "sun", 1 - owner, Zone::Analysis);
        put(state, 10, "arcana-strength", 1 - owner, Zone::Analysis, 2, SpellState::Active).ringIndex = 0;
        put(state, 11, "arcana-strength", owner, Zone::Hand);
        auto engine = GameEngine::scenario(cat, state);
        if (ambush) {
            send(engine, owner, SetAmbush{11, 1}); settleChoices(engine);
            auto next = engine.state(); ++next.globalTurn;
            engine = GameEngine::scenario(cat, next);
            send(engine, owner, FlipAmbush{11}); settleChoices(engine);
        } else {
            send(engine, owner, StartAnalysis{11, 1}); settleChoices(engine);
        }
        REQUIRE(engine.state().cards.at(10).owner == 1 - owner);
        REQUIRE(engine.state().cards.at(10).spell == SpellState::Active);
        REQUIRE(engine.state().cards.at(11).owner == owner);
        REQUIRE(engine.state().cards.at(11).zone == Zone::Analysis);
        REQUIRE(engine.state().cards.at(11).spell == SpellState::Analyzing);
        REQUIRE(engine.state().players[owner].mana == 12);
    }
}

TEST_CASE("Alpha v2.5.2 all five conditional responses reject unpaid fees without consuming the source", "[alpha-v252][response-cost]") {
    const auto cat = catalog();
    for (const std::string id : {"barbs", "counter-spell", "wind-strike", "sique-boiling-surge", "arcana-devil"}) {
        INFO(id);
        auto state = stateAt(id == "wind-strike" || id == "barbs" ? Phase::Main : Phase::Cast);
        put(state, 1, id == "wind-strike" ? "storm-heart" : id == "sique-boiling-surge" ? "sique-cauldron" : "sun", 1, Zone::Analysis);
        put(state, 2, "sun", 0, Zone::Analysis);
        if (id == "sique-boiling-surge") put(state, 3, "sique-campfire", 1, Zone::Analysis);
        const bool casting = id == "arcana-devil" || id == "sique-boiling-surge";
        auto &source = put(state, 10, id, 1, casting ? Zone::Casting : Zone::Analysis,
                           casting ? 0 : 1, casting ? SpellState::Ready : SpellState::None);
        source.sourceFormation = 1;
        if (!casting) {
            source.faceDown = source.wasAmbushed = true; source.ambushedTurn = 1; source.ringIndex = 0;
        }
        if (id == "wind-strike") put(state, 20, "fireball", 0, Zone::Hand);
        else if (id == "barbs") put(state, 20, "fireball", 0, Zone::Analysis, 2, SpellState::Ready).ringIndex = 0;
        else put(state, 20, id == "sique-boiling-surge" ? "unravel" : "spark", 0,
                 Zone::Casting, 0, SpellState::Ready).sourceFormation = 2;
        auto engine = GameEngine::scenario(cat, state);
        if (id == "wind-strike") send(engine, 0, StartAnalysis{20, 2});
        else if (id == "barbs") send(engine, 0, PrepareCast{20, playerTarget(1), 0, {playerTarget(1)}});
        else {
            const CardId target = id == "sique-boiling-surge" ? 1 : playerTarget(1);
            send(engine, 0, ActivateSpell{20, target, {target}});
        }
        REQUIRE(engine.state().decision);
        REQUIRE(engine.state().decision->player == 1);
        const auto view = engine.viewFor(1);
        const auto response = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
            const auto *r = std::get_if<Respond>(&action.command); return r && r->card == 10;
        });
        REQUIRE(response != view.actions.end());
        const Command command = response->command;
        auto unpaidState = engine.state(); unpaidState.players[1].mana = 0;
        auto unpaid = GameEngine::scenario(cat, unpaidState);
        const auto before = unpaid.digest();
        const auto result = unpaid.submit(1, command);
        REQUIRE_FALSE(result.accepted);
        REQUIRE(result.error.find("cost") != std::string::npos);
        REQUIRE(unpaid.digest() == before);
        REQUIRE(unpaid.state().cards.at(10).payments.empty());
        REQUIRE(unpaid.state().cards.at(10).faceDown == !casting);
        REQUIRE(Rules::castCost(engine.state(), cat, 10) > 0);
        send(engine, 1, command); settleChoices(engine);
        REQUIRE(engine.state().cards.at(10).payments.size() == 1);
    }
}

TEST_CASE("Alpha v2.5.2 Sun clears only enemy concealed spells at its owners end start", "[alpha-v252][sun]") {
    const auto cat = arcanaCatalog();
    for (PlayerId active : {0, 1}) {
        INFO(active);
        auto state = stateAt(Phase::Cast); state.active = active;
        put(state, 1, "sun", 0, Zone::Analysis);
        put(state, 2, "hexagram", 1, Zone::Analysis);
        for (CardId id : {10u, 20u, 21u}) {
            const PlayerId owner = id == 10 ? 0 : 1;
            auto &hidden = put(state, id, "spark", owner, Zone::Analysis, owner + 1);
            hidden.ringIndex = id == 21 ? 1 : 0;
            hidden.faceDown = hidden.wasAmbushed = true; hidden.ambushedTurn = 1;
        }
        put(state, 22, "fireball", 1, Zone::Analysis, 2, SpellState::Ready).ringIndex = 2;
        auto engine = GameEngine::scenario(cat, state);
        send(engine, active, AdvancePhase{engine.state().phaseGate}); settleChoices(engine);
        REQUIRE(engine.state().phase == Phase::End);
        REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
        REQUIRE(engine.state().cards.at(22).zone == Zone::Analysis);
        for (CardId id : {20u, 21u})
            REQUIRE(engine.state().cards.at(id).zone == (active == 0 ? Zone::Ash : Zone::Analysis));
    }
}

TEST_CASE("Alpha v2.5.2 Sun is excluded from another cards field removal effect", "[alpha-v252][sun]") {
    const auto cat = catalog();
    auto state = stateAt(Phase::Cast);
    put(state, 1, "storm-crest", 0, Zone::Analysis).stormPoints = 5;
    put(state, 2, "sun", 1, Zone::Analysis);
    put(state, 10, "storm-guidance", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
    auto engine = GameEngine::scenario(cat, state);
    send(engine, 0, ActivateSpell{10}); settle(engine);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::EffectChoice);
    const auto &options = engine.state().decision->options;
    REQUIRE(std::find(options.begin(), options.end(), 2) == options.end());
    REQUIRE(std::find(options.begin(), options.end(), 1) != options.end());
    send(engine, 0, Choose{engine.state().decision->id, 1}); settleChoices(engine);
    REQUIRE(engine.state().cards.at(1).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(2).zone == Zone::Analysis);
}

TEST_CASE("Alpha v2.5.2 Messy Wave adds four temporary load per opponent spell activation", "[alpha-v252][messy-wave]") {
    const auto cat = catalog();
    for (const std::string definition : {"spark", "fireball", "shield"}) for (PlayerId owner : {0, 1}) {
        INFO(definition); INFO(owner);
        auto state = stateAt(Phase::Cast); state.active = owner;
        put(state, 1, "sun", owner, Zone::Analysis);
        put(state, 2, "sun", 1 - owner, Zone::Analysis);
        auto &wave = put(state, 10, "messy-wave", 1, Zone::Analysis, owner == 1 ? 1 : 2, SpellState::Active);
        wave.ringIndex = 0; wave.remaining = 3;
        put(state, 20, definition, owner, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
        auto engine = GameEngine::scenario(cat, state);
        const CardId target = cat.at(definition).target == TargetKind::Player ? playerTarget(1 - owner) : 0;
        send(engine, owner, ActivateSpell{20, target, target ? std::vector<CardId>{target} : std::vector<CardId>{}});
        settleChoices(engine);
        const auto count = std::count_if(engine.state().temporary.begin(), engine.state().temporary.end(), [&](const TemporaryLoad &load) {
            return load.owner == owner && load.source == 10 && load.amount == 4 && !load.independent;
        });
        REQUIRE(count == (owner == 0 ? 1 : 0));
        REQUIRE(engine.state().cards.at(10).remaining == 3);
        REQUIRE(engine.state().cards.at(20).payments.size() == 1);
    }
}

TEST_CASE("Alpha v2.5.2 Snow Rain Storm surcharges setting only and doubles actual focus upkeep", "[alpha-v252][snow-rain]") {
    const auto cat = catalog();
    auto state = stateAt();
    put(state, 1, "sun", 0, Zone::Analysis);
    put(state, 2, "sun", 1, Zone::Analysis);
    put(state, 10, "snow-rain-storm", 0, Zone::Casting, 0, SpellState::Ready).sourceFormation = 1;
    auto &focus = put(state, 11, "protective-flame", 1, Zone::Analysis, 2, SpellState::Active);
    focus.ringIndex = 0; focus.concentrationCost = 2;
    put(state, 12, "messy-wave", 1, Zone::Hand);
    put(state, 13, "fireball", 1, Zone::Hand);
    deckCard(state, 30, "spark", 1);
    auto engine = GameEngine::scenario(cat, state);
    send(engine, 0, ActivateSpell{10}); settleChoices(engine);
    REQUIRE(engine.state().cards.at(10).spell == SpellState::Active);
    REQUIRE(engine.state().cards.at(10).concentrationCost == 2);
    REQUIRE(Rules::analysisCost(engine.state(), cat, 12, 2) == cat.at("messy-wave").cost + 2);
    REQUIRE(Rules::analysisCost(engine.state(), cat, 13, 2) == cat.at("fireball").cost);
    REQUIRE(Rules::castCost(engine.state(), cat, 12) == cat.at("messy-wave").castCost);
    for (int n = 0; n < 24 && !(engine.state().decision && engine.state().decision->kind == DecisionKind::Concentration); ++n) {
        if (engine.state().decision) {
            const auto d = *engine.state().decision;
            REQUIRE(d.kind == DecisionKind::Response);
            send(engine, d.player, PassResponse{d.id});
        } else send(engine, engine.state().active, AdvancePhase{engine.state().phaseGate});
    }
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->kind == DecisionKind::Concentration);
    REQUIRE(engine.state().decision->player == 1);
    REQUIRE(Rules::concentrationUpkeep(engine.state(), cat, 11) == 4);
    const int before = engine.state().players[1].mana;
    send(engine, 1, Choose{engine.state().decision->id, 11}); settleChoices(engine);
    REQUIRE(engine.state().players[1].mana == before - 4);
    REQUIRE(engine.state().cards.at(11).payments.back().paid == 4);
    for (int n = 0; n < 8 && engine.state().phase != Phase::Main; ++n) {
        send(engine, 1, AdvancePhase{engine.state().phaseGate}); settleChoices(engine);
    }
    REQUIRE(engine.state().phase == Phase::Main);
    const int settingBefore = engine.state().players[1].mana;
    send(engine, 1, StartAnalysis{12, 2}); settleChoices(engine);
    REQUIRE(engine.state().players[1].mana == settingBefore - cat.at("messy-wave").cost - 2);
    REQUIRE(engine.state().cards.at(12).settingPaid == cat.at("messy-wave").cost + 2);
}

TEST_CASE("Alpha v2.5.2 Woodpile allows two own copies and excludes a replaced copy from that limit", "[alpha-v252][woodpile]") {
    const auto cat = catalog();
    for (PlayerId owner : {0, 1}) {
        INFO(owner);
        auto state = stateAt(); state.active = owner;
        put(state, 1, "sique-woodpile", owner, Zone::Analysis);
        put(state, 2, "sique-woodpile", 1 - owner, Zone::Analysis);
        put(state, 3, "sique-woodpile", 1 - owner, Zone::Analysis);
        put(state, 10, "sique-woodpile", owner, Zone::SideDeck);
        put(state, 11, "sique-woodpile", owner, Zone::SideDeck);
        auto engine = GameEngine::scenario(cat, state);
        send(engine, owner, SetFormation{10}); settleChoices(engine);
        REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
        REQUIRE(engine.state().players[owner].mana == 17);
        auto next = engine.state(); next.players[owner].formations = 0;
        engine = GameEngine::scenario(cat, next);
        const auto before = engine.digest();
        const auto result = engine.submit(owner, SetFormation{11});
        REQUIRE_FALSE(result.accepted);
        REQUIRE(result.error.find("formation_copy_limit") != std::string::npos);
        REQUIRE(engine.digest() == before);
        send(engine, owner, SetFormation{11, 1}); settleChoices(engine);
        REQUIRE(engine.state().cards.at(1).zone == Zone::Ash);
        REQUIRE(engine.state().cards.at(10).zone == Zone::Analysis);
        REQUIRE(engine.state().cards.at(11).zone == Zone::Analysis);
        REQUIRE(engine.state().players[owner].mana == 14);
    }
}
