#include "wizard/replay.hpp"
#include "wizard/ai.hpp"
#include "wizard/core/card_rules.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;
namespace {
CardCatalog catalog254() {
    auto cat = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets").catalog;
    cat.mechanicsRevision = 8;
    return cat;
}
GameState state254(Phase phase = Phase::Main) {
    GameState s; s.phase = phase; s.phaseStep = PhaseStep::Body;
    s.phaseGate = 1; s.globalTurn = 5; s.rng = 149;
    for (auto &p : s.players) { p.ownTurn = 2; p.mana = 20; }
    return s;
}
CardInstance &put254(GameState &s, CardId id, const char *definition, int owner, Zone zone,
                     CardId host = 0, SpellState spell = SpellState::None) {
    CardInstance c; c.id = id; c.definition = definition; c.owner = owner;
    c.zone = zone; c.host = host; c.spell = spell;
    if (zone == Zone::Analysis) c.fieldOrder = s.nextFieldOrder++;
    if (host) { c.sourceFormation = host; c.ringIndex = 0; c.sourceRingIndex = 0; }
    s.cards[id] = c; return s.cards.at(id);
}
void send254(GameEngine &e, int p, const Command &cmd) {
    const auto result = e.submit(p, cmd); INFO(result.error); REQUIRE(result.accepted);
}
void pass254(GameEngine &e) {
    for (int i = 0; e.state().decision && e.state().decision->kind == DecisionKind::Response && i < 50; ++i) {
        const auto d = *e.state().decision;
        send254(e, d.player, PassResponse{d.id});
    }
}
void settle254(GameEngine &e) {
    for (int i = 0; e.state().decision && i < 100; ++i) {
        const auto d = *e.state().decision;
        if (d.kind == DecisionKind::Response) send254(e, d.player, PassResponse{d.id});
        else send254(e, d.player, Choose{d.id, d.options.empty() ? 0 : d.options.front()});
    }
    REQUIRE_FALSE(e.state().decision);
}
void deck254(GameState &s, CardId id, const char *definition, int owner = 0) {
    put254(s, id, definition, owner, Zone::Deck); s.players[owner].deck.push_back(id);
}
}

TEST_CASE("Alpha v2.5.4 analytic setup and preparation defer all targets until activation", "[alpha-v254][existing][targets]") {
    for (const auto *definition : {"arcana-chariot", "arcana-death", "magic-missile"}) {
        CAPTURE(definition);
        auto cat = catalog254(); auto s = state254();
        put254(s, 1, "sun", 0, Zone::Analysis);
        put254(s, 10, definition, 0, Zone::Hand);
        auto e = GameEngine::scenario(cat, s);
        const auto initial = e.digest();
        REQUIRE_FALSE(e.submit(0, StartAnalysis{10, 1, playerTarget(1)}).accepted);
        REQUIRE(e.digest() == initial);
        send254(e, 0, StartAnalysis{10, 1}); settle254(e);
        s = e.state(); s.cards.at(10).spell = SpellState::Ready;
        s.cards.at(10).analysisProgress = cat.at(definition).analysisTurns;
        e = GameEngine::scenario(cat, s);
        const auto before = e.digest();
        REQUIRE_FALSE(e.submit(0, PrepareCast{10, playerTarget(1)}).accepted);
        REQUIRE(e.digest() == before);
        const auto view = e.viewFor(0);
        REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &action) {
            const auto *p = std::get_if<PrepareCast>(&action.command);
            return p && p->card == 10 && !p->target && p->targets.empty();
        }));
        send254(e, 0, PrepareCast{10}); settle254(e);
        REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
        REQUIRE(e.state().cards.at(10).targetCard == 0);
        s = e.state(); s.phase = Phase::Cast; s.phaseStep = PhaseStep::Body; s.phaseGate = 2;
        e = GameEngine::scenario(cat, s);
        const auto unchosen = e.digest();
        REQUIRE_FALSE(e.submit(0, ActivateSpell{10}).accepted);
        REQUIRE(e.digest() == unchosen);
        send254(e, 0, ActivateSpell{10, playerTarget(1)}); settle254(e);
        REQUIRE(e.state().cards.at(10).payments.size() == 1);
        REQUIRE(e.state().players[1].life < startingLife);
    }
}

TEST_CASE("Alpha v2.5.4 Unravel legal targets exclude hidden spells and formations with runes", "[alpha-v254][existing][ai]") {
    auto cat = catalog254(); auto s = state254(Phase::Cast);
    put254(s, 1, "sun", 0, Zone::Analysis);
    put254(s, 2, "small-hexagram", 1, Zone::Analysis);
    put254(s, 3, "small-hexagram", 1, Zone::Analysis);
    put254(s, 4, "small-hexagram", 1, Zone::Analysis);
    auto &hidden = put254(s, 20, "fireball", 1, Zone::Analysis, 2);
    hidden.faceDown = hidden.wasAmbushed = true; hidden.ambushedTurn = 1;
    put254(s, 21, "ring", 1, Zone::Attached, 3);
    auto &spell = put254(s, 10, "unravel", 0, Zone::Casting, 0, SpellState::Ready);
    spell.sourceFormation = 1;
    put254(s, 30, "small-hexagram", 0, Zone::SideDeck);
    auto e = GameEngine::scenario(cat, s);
    for (CardId bad : {2u, 3u, 20u}) {
        const auto before = e.digest();
        REQUIRE_FALSE(e.submit(0, ActivateSpell{10, bad, {bad}}).accepted);
        REQUIRE(e.digest() == before);
    }
    auto view = e.viewFor(0);
    for (const auto &a : view.actions)
        if (const auto *activate = std::get_if<ActivateSpell>(&a.command); activate && activate->card == 10)
            REQUIRE(activate->target == 4);
    for (auto difficulty : {ai::Difficulty::Easy, ai::Difficulty::Normal, ai::Difficulty::Hard}) {
        const auto choice = ai::choose(view, difficulty); REQUIRE(choice);
        if (const auto *activate = std::get_if<ActivateSpell>(&choice->command)) REQUIRE(activate->target == 4);
        const auto trial = e.submit(0, choice->command); INFO(trial.error); REQUIRE(trial.accepted);
        e = GameEngine::scenario(cat, s);
    }
}

TEST_CASE("Alpha v2.5.4 refire cannot draw without a successful recycle", "[alpha-v254][existing][then]") {
    auto cat = catalog254(); auto s = state254();
    put254(s, 1, "sun", 0, Zone::Analysis);
    put254(s, 10, "sique-refire", 0, Zone::Hand);
    deck254(s, 20, "spark");
    auto e = GameEngine::scenario(cat, s);
    send254(e, 0, PreloadWord{10, true, 1}); pass254(e);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(20).zone == Zone::Deck);
    s = state254(); put254(s, 1, "sun", 0, Zone::Analysis);
    put254(s, 10, "sique-refire", 0, Zone::Hand);
    put254(s, 11, "sique-sugar", 0, Zone::Ash); deck254(s, 20, "spark");
    e = GameEngine::scenario(cat, s);
    send254(e, 0, PreloadWord{10, true, 1}); pass254(e);
    REQUIRE(e.state().decision); REQUIRE_FALSE(e.state().decision->mayPass);
    const auto rejected = e.digest();
    REQUIRE_FALSE(e.submit(0, Choose{e.state().decision->id, 0}).accepted); REQUIRE(e.digest() == rejected);
    send254(e, 0, Choose{e.state().decision->id, 11});
    pass254(e); REQUIRE(e.state().decision); REQUIRE(e.state().decision->mayPass);
    send254(e, 0, Choose{e.state().decision->id, 0}); pass254(e);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().players[0].deck.size() == 2);
}

TEST_CASE("Alpha v2.5.4 Fool and following words use actual casting zones", "[alpha-v254][existing][fool]") {
    auto cat = catalog254(); auto s = state254();
    put254(s, 1, "sun", 0, Zone::Analysis);
    put254(s, 10, "arcana-fool", 0, Zone::Hand);
    for (CardId id = 20; id < 25; ++id) put254(s, id, "arcana-fool", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    send254(e, 0, PreloadWord{10, true, 1}); settle254(e);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Resolved);
    const auto next = std::find_if(e.state().cards.begin(), e.state().cards.end(), [](const auto &entry) {
        return entry.second.zone == Zone::Hand && entry.second.definition == "arcana-fool";
    });
    REQUIRE(next != e.state().cards.end());
    send254(e, 0, PreloadWord{next->first, true, 1}); settle254(e);
}

TEST_CASE("Alpha v2.5.4 Death advances only for cards entering its owners ash", "[alpha-v254][existing][death]") {
    auto cat = catalog254(); auto s = state254();
    put254(s, 1, "sun", 0, Zone::Analysis); put254(s, 2, "sun", 1, Zone::Analysis);
    auto &a = put254(s, 10, "arcana-death", 0, Zone::Analysis, 1, SpellState::Analyzing);
    a.analysisProgress = 0;
    put254(s, 11, "arcana-death", 1, Zone::Analysis, 2, SpellState::Analyzing);
    put254(s, 20, "spark", 0, Zone::Hand);
    StateMaintenance::leave(s, 20, &cat);
    REQUIRE(s.cards.at(10).analysisProgress == 1);
    REQUIRE(s.cards.at(11).analysisProgress == 0);
}

TEST_CASE("Alpha v2.5.4 Storm Guidance allows declining its dependent followup", "[alpha-v254][existing][then]") {
    auto cat = catalog254(); auto s = state254();
    put254(s, 1, "storm-heart", 0, Zone::Analysis);
    put254(s, 10, "storm-guidance", 0, Zone::Hand);
    put254(s, 20, "small-hexagram", 1, Zone::Analysis);
    auto e = GameEngine::scenario(cat, s);
    send254(e, 0, PreloadWord{10, true, 1}); pass254(e);
    REQUIRE(e.state().decision); send254(e, 0, Choose{e.state().decision->id, 20});
    REQUIRE(e.state().cards.at(20).zone == Zone::Ash);
    REQUIRE(e.state().decision); REQUIRE(e.state().decision->mayPass);
    send254(e, 0, Choose{e.state().decision->id, 0}); pass254(e);
    REQUIRE(e.state().cards.at(1).stormPoints == 1);
}

TEST_CASE("Alpha v2.5.4 Great Storm removes and snapshots counters when set", "[alpha-v254][existing][storm]") {
    auto cat = catalog254(); auto s = state254();
    put254(s, 1, "storm-heart", 0, Zone::Analysis).stormPoints = 3;
    put254(s, 10, "great-storm", 0, Zone::Hand);
    auto e = GameEngine::scenario(cat, s);
    send254(e, 0, StartAnalysis{10, 1}); settle254(e);
    REQUIRE(e.state().cards.at(1).stormPoints == 0);
    REQUIRE(e.state().cards.at(10).remaining == 3);
}
