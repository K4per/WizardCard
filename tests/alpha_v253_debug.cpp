#include "support/observed_engine.hpp"
#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <chrono>
using namespace wizard;
namespace {
Content debugContent() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"); }
MatchConfig debugConfig(const Content &c) {
    MatchConfig config; config.seed = 123456;
    for (auto &p : config.players) { p.cards = c.deck; p.sideDeck = c.sideDeck; }
    return config;
}
test::ObservedEngine debugEngine() {
    auto c = debugContent(); MutationContext s;
    s.phase = Phase::Main; s.phaseStep = PhaseStep::Body; s.flow = Flow::Main;
    s.phaseGate = 1; s.nextDecision = 2; s.globalTurn = 3; s.rng = 456;
    for (auto &p : s.players) { p.ownTurn = 2; p.life = 20; }
    for (int p = 0; p < 2; ++p) {
        CardInstance formation; formation.id = static_cast<CardId>(p + 1); formation.owner = p;
        formation.definition = "hexagram"; formation.zone = Zone::Analysis; formation.fieldOrder = s.nextFieldOrder++;
        s.cards.emplace(formation.id, formation);
    }
    return test::ObservedEngine::scenario(c.catalog, s);
}
DebugOperation operation(const std::string &name, int player = 0, int amount = 0) {
    DebugOperation op; op.operation = name; op.player = player; op.amount = amount; return op;
}
void debugSend(test::ObservedEngine &e, const DebugOperation &op) {
    const auto result = e.debug(op); INFO(result.error); REQUIRE(result.accepted);
}
void goToTurn(MatchSession &session, std::uint64_t turn) {
    for (int n = 0; n < 500 && (session.engine().state().opening || session.engine().state().globalTurn < turn); ++n) {
        const auto &s = test::mutation(session.engine().state());
        const int player = s.opening ? (s.initialFormations[0] ? 1 : 0) : s.decision ? s.decision->player : s.active;
        const auto v = test::view(session.engine(),player);
        auto preferred = std::find_if(v.actions.begin(), v.actions.end(), [&](const LegalAction &a) {
            if (s.opening) return std::holds_alternative<SelectInitialFormation>(a.command);
            if (s.decision) return s.decision->kind == DecisionKind::Response ?
                std::holds_alternative<PassResponse>(a.command) : std::holds_alternative<Choose>(a.command);
            return std::holds_alternative<AdvancePhase>(a.command) || std::holds_alternative<Advance>(a.command);
        });
        REQUIRE(preferred != v.actions.end());
        const auto result = session.submit(player, preferred->command); INFO(result.error); REQUIRE(result.accepted);
    }
    REQUIRE(session.engine().state().globalTurn == turn);
    REQUIRE_FALSE(session.engine().state().opening);
}
struct DebugDirectory {
    std::filesystem::path path = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "debug-tests" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    ~DebugDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
};
}
TEST_CASE("Alpha v2.5.3 debug adds cards without drawing and rejects invalid or full hands atomically", "[alpha-v253][debug]") {
    auto e = debugEngine();
    auto add = operation("add_card"); add.cardId = "fireball";
    const auto deck = e.state().players[0].deck;
    debugSend(e, add);
    REQUIRE(e.state().cards.size() == 3);
    REQUIRE(e.state().players[0].deck == deck);
    REQUIRE(e.state().cards.at(3).definition == "fireball");
    REQUIRE(e.events().back().kind == "debug");
    REQUIRE(e.state().debugUsed);
    add.cardId = e.catalog().at("fireball").name;
    for (int i = 1; i < 8; ++i) debugSend(e, add);
    const auto digest = e.digest(); const auto events = e.events().size();
    REQUIRE_FALSE(e.debug(add).accepted);
    add.cardId = "missing"; REQUIRE_FALSE(e.debug(add).accepted);
    add.player = 2; REQUIRE_FALSE(e.debug(add).accepted);
    REQUIRE(e.digest() == digest); REQUIRE(e.events().size() == events);
    REQUIRE(std::none_of(e.events().begin(), e.events().end(), [](const GameEvent &event) { return event.kind == "draw"; }));
}
TEST_CASE("Alpha v2.5.3 infinite resources are independent and literal debug changes retain actual values", "[alpha-v253][debug]") {
    auto e = debugEngine();
    auto life = operation("set_infinite_life"); life.enabled = true; debugSend(e, life);
    auto mana = operation("set_infinite_mana"); mana.enabled = true; debugSend(e, mana);
    auto s = test::mutation(e.state());
    REQUIRE(ResourceRules::canPayMana(s, 0, 1000)); ResourceRules::payMana(s, 0, 1000);
    REQUIRE(s.players[0].mana == 0);
    Trigger t; t.owner = 1; t.targetPlayer = 0;
    EffectResolver::apply(s, e.catalog(), t, {EffectKind::Damage, 100});
    ResourceRules::damage(s, 0, 100, "exhaustion", &e.catalog());
    REQUIRE(s.players[0].life == 20);
    debugSend(e, operation("gain_mana", 0, 8)); debugSend(e, operation("lose_mana", 0, 3));
    REQUIRE(e.state().players[0].mana == 5);
    debugSend(e, operation("gain_temporary_life", 0, 7));
    debugSend(e, operation("lose_temporary_life", 0, 2));
    REQUIRE(e.state().players[0].temporaryLife == 5);
    debugSend(e, operation("lose_life", 0, 100));
    REQUIRE(e.state().players[0].life == 0); REQUIRE(e.state().players[0].temporaryLife == 5);
    REQUIRE(e.state().result == -1);
    life.enabled = false; debugSend(e, life);
    REQUIRE(e.state().result == 1);
    REQUIRE(e.state().players[0].debugInfiniteMana);
}
TEST_CASE("Alpha v2.5.3 infinite mana bypasses authoritative cost gates without negative resources", "[alpha-v253][debug]") {
    auto e = debugEngine(); auto add = operation("add_card"); add.cardId = "fireball"; debugSend(e, add);
    REQUIRE_FALSE(e.submit(0, StartAnalysis{3, 1}).accepted);
    auto toggle = operation("set_infinite_mana"); toggle.enabled = true; debugSend(e, toggle);
    const auto result = e.submit(0, StartAnalysis{3, 1}); INFO(result.error); REQUIRE(result.accepted);
    REQUIRE(e.state().players[0].mana == 0);
    REQUIRE(e.state().cards.at(3).analysisLoad > 0);
    REQUIRE(Rules::invariants(e.state(), e.catalog()).empty());
}
TEST_CASE("Alpha v2.5.3 debug load categories validate source and preserve provenance", "[alpha-v253][debug]") {
    auto e = debugEngine(); auto toggle = operation("set_infinite_load"); toggle.enabled = true; debugSend(e, toggle);
    auto bound = operation("gain_load", 0, 6); bound.loadKind = "bound"; bound.sourceId = 2;
    const auto before = e.digest(); REQUIRE_FALSE(e.debug(bound).accepted); REQUIRE(e.digest() == before);
    bound.sourceId = 1; debugSend(e, bound);
    auto independent = operation("gain_load", 0, 7); independent.loadKind = "independent"; debugSend(e, independent);
    auto temporary = operation("gain_load", 0, 8); temporary.loadKind = "temporary"; debugSend(e, temporary);
    REQUIRE(Rules::load(e.state(), 0) == 21); REQUIRE(e.state().players[0].life == 20);
    auto clear = operation("clear_load", 0, 3); clear.loadKind = "temporary"; debugSend(e, clear);
    REQUIRE(Rules::load(e.state(), 0) == 18);
    auto s = test::mutation(e.state()); StateMaintenance::leave(s, 1, &e.catalog());
    REQUIRE(Rules::load(s, 0) == 12);
    REQUIRE(std::none_of(s.temporary.begin(), s.temporary.end(), [](const TemporaryLoad &l) { return l.sourceBound && l.source == 1; }));
    toggle.enabled = false; debugSend(e, toggle);
    REQUIRE_FALSE(e.state().players[0].debugInfiniteLoad);
    REQUIRE(e.state().players[0].life < 20);
}
TEST_CASE("Alpha v2.5.3 debug rewind restores complete turn state RNG and replay prefix", "[alpha-v253][debug]") {
    const auto content = debugContent(); MatchSession session(content, debugConfig(content));
    REQUIRE_FALSE(session.debug(operation("gain_mana", 0, 1)).accepted);
    session.enableDebug(true); REQUIRE_FALSE(session.canRewindPreviousTurn());
    goToTurn(session, 1); const auto firstTurn = session.engine();
    auto grant = operation("gain_mana", 0, 20); REQUIRE(session.debug(grant).accepted);
    auto add = operation("add_card"); add.cardId = "fireball"; REQUIRE(session.debug(add).accepted);
    goToTurn(session, 2);
    REQUIRE(session.canRewindPreviousTurn());
    auto expected = firstTurn; const auto rewind = operation("rewind_previous_turn");
    REQUIRE(expected.debug(rewind).accepted); REQUIRE(session.debug(rewind).accepted);
    REQUIRE(session.engine().digest() == expected.digest());
    REQUIRE(session.engine().state().rng == firstTurn.state().rng);
    REQUIRE(session.engine().state().players[0].deck == firstTurn.state().players[0].deck);
    REQUIRE(session.engine().state().players[1].deck == firstTurn.state().players[1].deck);
    REQUIRE_FALSE(session.canRewindPreviousTurn());
    const auto record = session.recording();
    REQUIRE(record.at("containsDebugOperations") == true);
    REQUIRE(record.at("commands").back().contains("debug"));
    REQUIRE(replay(content, record).digest() == session.engine().digest());
    const auto afterRewind = session.engine().digest();
    REQUIRE_FALSE(session.debug(rewind).accepted); REQUIRE(session.engine().digest() == afterRewind);
    goToTurn(session, 2);
    REQUIRE(replay(content, session.recording()).digest() == session.engine().digest());
}
TEST_CASE("Alpha v2.5.3 debug authority rejects hotseat tutorial network mode and replay browsing", "[alpha-v253][debug][bridge]") {
    const auto content = debugContent(); DebugDirectory directory;
    app::Application application(content, directory.path);
    std::string error;
    REQUIRE(application.start(debugConfig(content), error, {app::MatchMode::Hotseat}));
    REQUIRE_FALSE(application.debug(operation("gain_mana", 0, 2)).accepted);
    app::Application tutorialApplication(content, directory.path / "tutorial");
    REQUIRE(tutorialApplication.startTutorial(error));
    REQUIRE_FALSE(tutorialApplication.debug(operation("gain_mana", 0, 2)).accepted);
    REQUIRE_FALSE(application.start(debugConfig(content), error, {app::MatchMode::Network}));
    bridge::LocalSession bridge;
    REQUIRE(bridge.initialize(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets", directory.path / "bridge").at("ok") == true);
    REQUIRE(bridge.start({{"mode", "ai"}}).at("ok") == true);
    const auto snap = bridge.snapshot(0).at("data"); REQUIRE(snap.at("debug").at("available") == true);
    REQUIRE_FALSE(bridge.debugCommand({{"operation", "gain_mana"}, {"amount", 1.5}}).at("ok").get<bool>());
    REQUIRE_FALSE(bridge.debugCommand({{"operation", "gain_mana"}, {"amount", -1}}).at("ok").get<bool>());
    REQUIRE_FALSE(bridge.debugCommand({{"operation", "gain_load"}, {"amount", 1}, {"loadKind", "bound"}, {"sourceId", "1junk"}}).at("ok").get<bool>());
    // A verified replay view is read only even when a previous AI session remains allocated.
    MatchSession session(content, debugConfig(content)); session.enableDebug(true); goToTurn(session, 1);
    REQUIRE(session.debug(operation("gain_mana", 0, 1)).accepted);
    auto toggle = operation("set_infinite_life"); toggle.enabled = true;
    REQUIRE(session.debug(toggle).accepted);
    const auto enabledStep = session.recording().at("commands").size();
    toggle.enabled = false; REQUIRE(session.debug(toggle).accepted);
    const auto path = directory.path / "debug-recording.json"; std::filesystem::create_directories(directory.path); session.save(path);
    REQUIRE(bridge.replayFrame(path, session.recording().at("commands").size(), 0).at("ok") == true);
    REQUIRE(bridge.replayFrame(path, enabledStep, 0).at("data").at("debug").at("players").at(0).at("infiniteLife") == true);
    REQUIRE(bridge.replayFrame(path, session.recording().at("commands").size(), 0).at("data").at("debug").at("players").at(0).at("infiniteLife") == false);
    REQUIRE(bridge.replayFrame(path, 0, 0).at("data").at("debug").at("players").at(0).at("infiniteLife") == false);
    REQUIRE_FALSE(bridge.debugCommand({{"operation", "gain_mana"}, {"amount", 1}}).at("ok").get<bool>());
}
