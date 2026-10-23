#include "wizard/replay.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
using namespace wizard;
namespace {
CardCatalog rules253() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog; }
GameState main253() {
    GameState s; s.phase = Phase::Main; s.phaseStep = PhaseStep::Body;
    s.phaseGate = 2; s.globalTurn = 5; s.rng = 123;
    for (auto &p : s.players) { p.mana = 20; p.ownTurn = 2; }
    return s;
}
CardInstance &put253(GameState &s, CardId id, const std::string &definition, int player, Zone zone) {
    CardInstance c; c.id = id; c.definition = definition; c.owner = player; c.zone = zone;
    if (zone == Zone::Analysis) c.fieldOrder = s.nextFieldOrder++;
    s.cards[id] = c;
    return s.cards.at(id);
}
void settle253(GameEngine &e) {
    for (int n = 0; e.state().decision && n < 40; ++n) {
        const auto d = *e.state().decision;
        Command c = d.kind == DecisionKind::Response ? Command(PassResponse{d.id}) :
            Command(Choose{d.id, d.options.empty() ? 0 : d.options.front()});
        const auto result = e.submit(d.player, c);
        INFO(result.error); REQUIRE(result.accepted);
    }
    REQUIRE_FALSE(e.state().decision);
}
void send253(GameEngine &e, int p, Command c) {
    const auto result = e.submit(p, c); INFO(result.error); REQUIRE(result.accepted);
}
void window253(GameState &s, int speed = 0, int priority = 0) {
    ChainLink root; root.id = 1; root.kind = LinkKind::Phase; root.speed = speed;
    ChainState chain; chain.id = 1; chain.priority = priority;
    chain.window = ResponseWindow::Action; chain.links.push_back(root);
    s.chain = chain; s.decision = PendingDecision{1, 0, DecisionKind::Response, {}, true};
    s.nextLink = 2;
}
}
TEST_CASE("Alpha v2.5.3 opening mana is zero for both players before initial formation selection", "[alpha-v253]") {
    const auto c = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    MatchConfig config;
    for (auto &p : config.players) { p.cards = c.deck; p.sideDeck = c.sideDeck; }
    GameEngine e(c.catalog, config);
    REQUIRE(e.state().opening);
    REQUIRE(e.state().players[0].mana == 0);
    REQUIRE(e.state().players[1].mana == 0);
    REQUIRE(maximumMana == 20);
}
TEST_CASE("Alpha v2.5.3 high rank ambush bypasses host rank but cannot flip prepare or set face up", "[alpha-v253]") {
    auto cat = rules253(); auto s = main253();
    put253(s, 1, "small-hexagram", 0, Zone::Analysis);
    put253(s, 10, "sunburst", 0, Zone::Hand);
    REQUIRE(cat.at("sunburst").rank > Rules::maxRank(s, cat, 1));
    auto e = GameEngine::scenario(cat, s);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, StartAnalysis{10, 1}).accepted); REQUIRE(e.digest() == before);
    const auto view = e.viewFor(0);
    REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &a) {
        const auto *ambush = std::get_if<SetAmbush>(&a.command); return ambush && ambush->card == 10;
    }));
    send253(e, 0, SetAmbush{10, 1}); settle253(e);
    s = e.state(); ++s.globalTurn;
    e = GameEngine::scenario(cat, s);
    const auto hidden = e.digest();
    REQUIRE_FALSE(e.submit(0, FlipAmbush{10}).accepted);
    REQUIRE_FALSE(e.submit(0, PrepareCast{10}).accepted);
    REQUIRE(e.digest() == hidden);
    send253(e, 0, Abandon{10}); settle253(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("Alpha v2.5.3 hand word responses share legal generation priority speed cost and codec", "[alpha-v253]") {
    for (int invalid = -1; invalid < 6; ++invalid) {
        auto cat = rules253(); auto s = main253();
        put253(s, 1, "hexagram", 0, Zone::Analysis);
        put253(s, 2, "sun", 1, Zone::Analysis);
        put253(s, 10, "disrupt", 0, Zone::Hand);
        // A legal ambush keeps the chain open at declaration for cost checks.
        auto &counter = put253(s, 20, "counter-spell", 1, Zone::Analysis);
        counter.faceDown = true; counter.host = 2; counter.ringIndex = 0;
        counter.wasAmbushed = true; counter.ambushedTurn = 1;
        if (invalid == 0) s.phase = Phase::Draw;
        if (invalid == 1) s.players[0].mana = 0;
        if (invalid == 2) cat.cards.at("disrupt").speed = 1;
        if (invalid == 3) cat.cards.at("disrupt").responseOnly = true;
        window253(s, invalid == 4 ? 4 : 0, invalid == 5 ? 1 : 0);
        auto e = GameEngine::scenario(cat, s);
        const Command command = PreloadWord{10, true, 1, 0, {}, 0, 1};
        REQUIRE(encodeCommand(decodeCommand(encodeCommand(command))) == encodeCommand(command));
        const auto view = e.viewFor(0);
        const bool offered = std::any_of(view.actions.begin(), view.actions.end(), [&](const LegalAction &a) {
            return encodeCommand(a.command) == encodeCommand(command);
        });
        const auto before = e.digest();
        const auto result = e.submit(0, command);
        CAPTURE(invalid, result.error);
        REQUIRE(offered == (invalid == -1));
        REQUIRE(result.accepted == (invalid == -1));
        if (invalid != -1) REQUIRE(e.digest() == before);
        else {
            REQUIRE(e.state().decision);
            REQUIRE(e.state().decision->kind == DecisionKind::Response);
            REQUIRE(e.state().decision->player == 1);
            REQUIRE(e.state().chain);
            REQUIRE(e.state().chain->mode == ChainMode::Building);
            REQUIRE(Rules::load(e.state(), 0) == 2);
            REQUIRE(e.state().cards.at(10).analysisLoad == 2);
            REQUIRE(e.state().cards.at(10).castLoad == 0);
            REQUIRE(e.state().temporary.empty());
            REQUIRE(e.state().players[0].mana == 18);
            REQUIRE(e.state().cards.at(10).payments.size() == 1);
            REQUIRE(e.state().cards.at(10).payments.front().paid == 2);
            REQUIRE(e.state().chain->links.size() == 2);
            REQUIRE(e.state().chain->links.back().item.source == 10);
            REQUIRE(e.state().chain->links.back().paid == 2);
            settle253(e);
            REQUIRE(e.state().players[0].mana == 18);
            REQUIRE(e.state().cards.at(10).payments.size() == 1);
            REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
            REQUIRE(e.state().cards.at(10).spell == SpellState::Resolved);
            // Disrupt adds one temporary load to its owner and three to the
            // opponent once it resolves; its two paid load remain on the card.
            REQUIRE(Rules::load(e.state(), 0) == 3);
            REQUIRE(Rules::load(e.state(), 1) == 3);
            REQUIRE(std::count_if(e.state().temporary.begin(), e.state().temporary.end(), [](const TemporaryLoad &load) {
                return load.source == 10 && load.owner == 0 && load.amount == 1;
            }) == 1);
            REQUIRE(std::count_if(e.state().temporary.begin(), e.state().temporary.end(), [](const TemporaryLoad &load) {
                return load.source == 10 && load.owner == 1 && load.amount == 3;
            }) == 1);
            REQUIRE(e.state().players[1].life == startingLife);
        }
    }
}
TEST_CASE("Alpha v2.5.3 campfire triggers once after successful setting without adding spell damage", "[alpha-v253]") {
    auto cat = rules253(); auto s = main253();
    put253(s, 1, "sique-campfire", 0, Zone::Analysis);
    put253(s, 2, "hexagram", 1, Zone::Analysis);
    put253(s, 10, "fireball", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    send253(e, 0, StartAnalysis{10, 1}); settle253(e);
    REQUIRE(e.state().players[1].life == startingLife - 1);
    const auto same = e.digest();
    REQUIRE_FALSE(e.submit(0, StartAnalysis{10, 1}).accepted); REQUIRE(e.digest() == same);
    auto resolved = e.state();
    const Trigger trigger{0, 0, 0, 10, 0, 1};
    EffectResolver::apply(resolved, cat, trigger, {EffectKind::Damage, 4, DamageType::Fire});
    REQUIRE(resolved.players[1].life == startingLife - 5);
    REQUIRE(std::count_if(resolved.events.begin(), resolved.events.end(), [](const GameEvent &event) {
        return event.kind == "campfire_setting";
    }) == 1);
}
TEST_CASE("Alpha v2.5.3 campfire setting trigger excludes ambush and flip actions", "[alpha-v253]") {
    auto cat = rules253(); auto s = main253();
    put253(s, 1, "sique-campfire", 0, Zone::Analysis);
    put253(s, 10, "fireball", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    send253(e, 0, SetAmbush{10, 1}); settle253(e);
    REQUIRE(e.state().players[1].life == startingLife);
    s = e.state(); ++s.globalTurn;
    e = GameEngine::scenario(cat, s);
    send253(e, 0, FlipAmbush{10}); settle253(e);
    REQUIRE(e.state().players[1].life == startingLife);
}
TEST_CASE("Alpha v2.5.3 cauldron rejects other runes and annihilation requires a Storm spell host", "[alpha-v253]") {
    auto cat = rules253(); auto s = main253();
    put253(s, 1, "sique-cauldron", 0, Zone::Analysis);
    put253(s, 10, "ring", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s); auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, AttachSeal{10, 1}).accepted); REQUIRE(e.digest() == before);
    s.cards.clear();
    put253(s, 1, "fireball", 0, Zone::Casting).spell = SpellState::Ready;
    put253(s, 10, "storm-annihilation-crest", 0, Zone::Hand);
    e = GameEngine::scenario(cat, s); before = e.digest();
    REQUIRE_FALSE(e.submit(0, AttachSeal{10, 1}).accepted); REQUIRE(e.digest() == before);
    s.cards.at(1).definition = "cutting-wind";
    e = GameEngine::scenario(cat, s);
    send253(e, 0, AttachSeal{10, 1}); settle253(e);
    REQUIRE(e.state().cards.at(10).host == 1);
}
TEST_CASE("Alpha v2.5.3 elemental ring adds one per evocation damage event only", "[alpha-v253]") {
    const auto cat = rules253();
    for (const auto &definition : {"fireball", "ray-of-frost", "sique-backfire"}) {
        auto s = main253();
        put253(s, 1, "elemental-ring", 0, Zone::Analysis);
        auto &spell = put253(s, 10, definition, 0, Zone::Casting);
        spell.sourceFormation = 1;
        Trigger t; t.source = 10; t.owner = 0; t.targetPlayer = 1;
        const bool evocation = cat.at(definition).school == "evocation";
        for (int n = 0; n < 3; ++n)
            EffectResolver::apply(s, cat, t, {EffectKind::Damage, 2, DamageType::Fire});
        REQUIRE(s.players[1].life == startingLife - 3 * (evocation ? 3 : 2));
    }
}
TEST_CASE("Alpha v2.5.3 Storm Crest confirmed counter thresholds determine all resources", "[alpha-v253]") {
    const auto cat = rules253(); auto s = main253();
    auto &crest = put253(s, 1, "storm-crest", 0, Zone::Analysis);
    REQUIRE(crest.stormPoints == 0);
    for (int points : {0, 2, 3, 4, 5, 8}) {
        crest.stormPoints = points;
        REQUIRE(Rules::level(s, cat, 1) == (points >= 5 ? 3 : points >= 3 ? 2 : 1));
        REQUIRE(Rules::rings(s, cat, 1) == Rules::level(s, cat, 1));
        REQUIRE(Rules::capacity(s, cat, 0) == cat.baseLoadCapacity + (points >= 5 ? 10 : points >= 3 ? 8 : 5));
        REQUIRE(Rules::income(s, cat, 1) == (points >= 5 ? 2 : 1));
    }
}
TEST_CASE("Alpha v2.5.3 cauldron allows non Sique analysis and preparation but rejects activation", "[alpha-v253]") {
    auto cat = rules253(); auto s = main253();
    put253(s, 1, "sique-cauldron", 0, Zone::Analysis);
    put253(s, 2, "hexagram", 0, Zone::Analysis);
    put253(s, 10, "fireball", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    send253(e, 0, StartAnalysis{10, 2}); settle253(e);
    s = e.state(); s.cards.at(10).spell = SpellState::Ready;
    s.cards.at(10).analysisProgress = cat.at("fireball").analysisTurns;
    e = GameEngine::scenario(cat, s);
    send253(e, 0, PrepareCast{10, playerTarget(1)}); settle253(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    s = e.state(); s.phase = Phase::Cast;
    e = GameEngine::scenario(cat, s);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, ActivateSpell{10, playerTarget(1)}).accepted);
    REQUIRE(e.digest() == before);
}
