#include "wizard/network.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
using namespace wizard;
namespace {
Content currentContent() { return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"); }
void nextTurn(MatchSession &session, std::uint64_t target) {
    for (int steps = 0; steps < 1000; ++steps) {
        const auto &state = session.engine().state();
        if (!state.opening && state.globalTurn >= target) return;
        const int player = state.opening ? (state.initialFormations[0] ? 1 : 0) :
            state.decision ? state.decision->player : state.active;
        const auto view = session.viewFor(player);
        std::optional<Command> selected = view.actionSummary.safeAdvance;
        ActionIntent intent;
        if (!selected) {
            const auto kind = state.opening ? CommandKind::SelectInitialFormation : CommandKind::Choose;
            intent.selections.push_back({ActionField::Operation, std::to_string(static_cast<unsigned>(kind))});
            for (int depth = 0; !selected && depth < 32; ++depth) {
                const auto options = session.queryActions(player, 1, session.revision(), intent);
                REQUIRE(options.error.empty());
                if (options.complete) { selected = options.complete; break; }
                REQUIRE_FALSE(options.options.empty());
                intent.selections.push_back({options.field, options.options.front().value});
            }
        }
        REQUIRE(selected);
        const auto result = session.submit(player, *selected);
        INFO(result.error); REQUIRE(result.accepted);
    }
    FAIL("turn advance did not terminate");
}
}
TEST_CASE("session history is outside engine copies and event cursors are viewer local", "[architecture][session]") {
    const auto content = currentContent(); MatchSession session(content, 41);
    nextTurn(session, 1);
    CHECK(session.debugCheckpointCount() == 0);
    const auto before = session.engine().digest();
    for (int player = 0; player < 2; ++player) {
        const auto events = session.eventsSince(player, 0, 0);
        REQUIRE(events.reset);
        REQUIRE(std::all_of(events.events.begin(), events.events.end(), [&](const GameEvent &event) {
            return event.audience == -1 || event.audience == player;
        }));
        CHECK(session.eventsSince(player, events.epoch, events.next).events.empty());
        CHECK(session.eventsSince(player, events.epoch, events.next + 1).reset);
        const auto recording = session.recording();
        const auto visible = std::count_if(recording.at("events").begin(), recording.at("events").end(), [&](const Json &event) {
            return event.at("audience") == -1 || event.at("audience") == player;
        });
        CHECK(events.next == static_cast<std::uint64_t>(visible));
        session.viewFor(player); session.viewFor(player);
    }
    CHECK(session.engine().digest() == before);
    CHECK_THROWS(session.eventsSince(2, 0, 0));
}
TEST_CASE("opponent hidden deck identities do not affect visible projection query or event cursor", "[architecture][session][privacy]") {
    const auto content = currentContent(); MatchConfig first; first.seed = 41;
    for (auto &player : first.players) { player.cards = content.deck; player.sideDeck = content.sideDeck; }
    auto second = first;
    std::rotate(second.players[1].cards.begin(), second.players[1].cards.begin() + 1, second.players[1].cards.end());
    MatchSession a(content, first), b(content, second);
    nextTurn(a, 1); nextTurn(b, 1);
    CHECK(a.engine().digest() != b.engine().digest());
    CHECK(net::encodeView(a.viewFor(0)) == net::encodeView(b.viewFor(0)));
    CHECK(net::encodeEvents(a.eventsSince(0, 0, 0)) == net::encodeEvents(b.eventsSince(0, 0, 0)));
    CHECK(net::encodeOptions(a.queryActions(0, 1, a.revision(), {})) == net::encodeOptions(b.queryActions(0, 1, b.revision(), {})));
}
TEST_CASE("debug checkpoints are bounded and rewind revisions never repeat", "[architecture][session]") {
    const auto content = currentContent(); MatchSession session(content, 57);
    session.enableDebug(true);
    nextTurn(session, 1);
    for (int player = 0; player < 2; ++player) {
        DebugOperation infinite; infinite.operation = "set_infinite_life"; infinite.player = player; infinite.enabled = true;
        REQUIRE(session.debug(infinite).accepted);
    }
    nextTurn(session, 19);
    CHECK(session.debugCheckpointCount() == 16);
    const auto epoch = session.eventsSince(0, 0, 0).epoch;
    auto revision = session.revision();
    for (int n = 0; n < 18; ++n) {
        DebugOperation rewind; rewind.operation = "rewind_previous_turn";
        REQUIRE(session.debug(rewind).accepted);
        REQUIRE(session.revision() > revision);
        revision = session.revision();
    }
    CHECK(session.engine().state().globalTurn == 1);
    CHECK_FALSE(session.canRewindPreviousTurn());
    CHECK(session.eventsSince(0, epoch, 0).reset);
    CHECK(replay(content, session.recording()).digest() == session.engine().digest());
    session.enableDebug(false);
    CHECK(session.debugCheckpointCount() == 0);
}
TEST_CASE("current recordings use versioned format five and still import current format four", "[architecture][replay]") {
    const auto content = currentContent(); MatchSession session(content, 41);
    nextTurn(session, 2);
    auto record = session.recording();
    REQUIRE(record.at("format") == 5);
    REQUIRE(record.at("contentSchema") == 1);
    REQUIRE(record.at("commandSchema") == 1);
    REQUIRE(record.at("digestSchema") == 2);
    std::vector<GameEvent> replayEvents;
    CHECK(replay(content, record, {}, [&](std::size_t, const std::vector<GameEvent> &batch) {
        replayEvents.insert(replayEvents.end(), batch.begin(), batch.end());
    }).digest() == session.engine().digest());
    REQUIRE(replayEvents.size() == record.at("events").size());
    for (std::size_t index = 0; index < replayEvents.size(); ++index) {
        CHECK(replayEvents[index].kind == record.at("events").at(index).at("kind"));
        CHECK(replayEvents[index].text == record.at("events").at(index).at("text"));
    }
    auto legacy = record; legacy["format"] = 4; legacy["programVersion"] = "2.5.5-hotfix.1";
    MatchConfig config; config.seed = 41;
    for (auto &player : config.players) { player.cards = content.deck; player.sideDeck = content.sideDeck; }
    GameEngine engine(content.catalog, config);
    for (auto &row : legacy["commands"]) {
        REQUIRE(applyRecordedCommand(engine, row).accepted);
        row["digest"] = engine.legacyDigest();
        if (row.at("command").at("type") == static_cast<unsigned>(CommandKind::Choose)) {
            const auto choice = decodeChoice(row.at("command").at("option"));
            REQUIRE((choice.kind == ChoiceKind::Pass || choice.kind == ChoiceKind::Card ||
                     choice.kind == ChoiceKind::Payment || choice.kind == ChoiceKind::Trigger));
            row["command"]["option"] = choice.value;
        }
    }
    legacy["finalDigest"] = engine.legacyDigest();
    CHECK(replay(content, legacy).digest() == session.engine().digest());
    auto bad = record; bad["digestSchema"] = 999; CHECK_THROWS(replay(content, bad));
    bad = legacy; bad["rulesVersion"] = "2.5.4"; CHECK_THROWS(replay(content, bad));
}
TEST_CASE("protocol four separates state clocks events and read only queries", "[architecture][network]") {
    using namespace wizard::net;
    const auto content = currentContent(); HostSession host(content); GuestSession guest(content);
    REQUIRE(hello(content).at("protocol") == 4);
    auto oldHello = hello(content); oldHello["protocol"] = 3; CHECK_THROWS(host.receive(oldHello));
    guest.receive(host.receive(hello(content)));
    host.ready(); guest.receive(host.receive({{"type", "Ready"}, {"session", host.session()}}));
    const auto snapshot = host.snapshot(1); guest.receive(snapshot);
    CHECK(snapshot.at("type") == "StateSnapshot");
    CHECK_FALSE(snapshot.at("view").contains("actions"));
    CHECK_FALSE(snapshot.at("view").contains("events"));
    const auto clock = host.clockUpdate(); guest.receive(clock);
    CHECK_FALSE(clock.contains("view"));
    const auto revision = host.revision();
    ActionIntent intent;
    const auto query = guest.query(1, guest.revision(), intent);
    guest.receive(host.receive(query));
    const auto result = guest.queryResult(1, guest.revision(), intent);
    REQUIRE(result); CHECK_FALSE(result->pending); CHECK(result->error.empty());
    CHECK_FALSE(result->options.empty()); CHECK(host.revision() == revision);
    guest.receive(host.eventUpdate(1, 0, 0));
    const auto events = guest.eventsSince(0, 0);
    CHECK(guest.eventsSince(events.epoch, events.next).events.empty());
    CHECK(encodeOptions(decodeOptions(encodeOptions(*result))) == encodeOptions(*result));
}
TEST_CASE("pre refactor current format four golden traces import without an old rules executor", "[legacy][architecture][replay]") {
    const auto content = currentContent();
    for (const auto seed : {42, 71, 314}) {
        const auto path = std::filesystem::path(WIZARD_SOURCE_DIR) / "tests" / "fixtures" /
            ("current-format4-seed-" + std::to_string(seed) + ".json");
        const auto record = readJson(path);
        std::size_t verified = 0;
        INFO("seed=" << seed);
        const auto engine = replay(content, record, [&](std::size_t step, const GameEngine &) { verified = step; });
        CHECK(verified == record.at("commands").size());
        CHECK(engine.legacyDigest() == record.at("finalDigest"));
    }
}
TEST_CASE("query responses are isolated from command sequences and discarded after newer intent or state", "[architecture][network]") {
    using namespace wizard::net;
    const auto content = currentContent(); HostSession host(content); GuestSession guest(content);
    guest.receive(host.receive(hello(content))); host.ready();
    guest.receive(host.receive({{"type", "Ready"}, {"session", host.session()}}));
    guest.receive(host.snapshot(1));
    ActionIntent first; const auto oldRequest = guest.query(7, guest.revision(), first);
    const auto oldReply = host.receive(oldRequest);
    auto second = first; second.source = guest.view().cards.front().instance.id;
    const auto currentRequest = guest.query(7, guest.revision(), second);
    guest.receive(oldReply);
    REQUIRE(guest.queryResult(7, guest.revision(), second)->pending);
    guest.receive(host.receive(currentRequest));
    CHECK_FALSE(guest.queryResult(7, guest.revision(), second)->pending);
    CHECK(encodeIntent(decodeIntent(encodeIntent(second))) == encodeIntent(second));
    const auto command = guest.command(Surrender{});
    CHECK(command.at("sequence") == "1");
    guest.receive(host.receive(command)); guest.receive(host.snapshot(1));
    guest.receive(oldReply);
    CHECK_FALSE(guest.queryResult(7, guest.revision(), first));
}
TEST_CASE("typed choice transport rejects legacy numeric choices and preserves wide link identifiers", "[architecture][network]") {
    using namespace wizard::net;
    const auto link = (std::uint64_t{1} << 63) + 51;
    const Command command = Choose{4, {ChoiceKind::Link, link}};
    const auto wire = wireCommand(command);
    CHECK(wire.at("option").at("value") == std::to_string(link));
    CHECK(encodeCommand(unwireCommand(wire)) == encodeCommand(command));
    auto legacy = wire; legacy["option"] = 1;
    CHECK_THROWS(unwireCommand(legacy));
    legacy["option"] = {{"kind", static_cast<unsigned>(ChoiceKind::Legacy)}, {"value", "1"}};
    CHECK_THROWS(unwireCommand(legacy));
    CHECK_THROWS(decodeChoice(Json{{"kind", static_cast<unsigned>(ChoiceKind::Player)}, {"value", "2"}}));
    CHECK_THROWS(decodeChoice(Json{{"kind", static_cast<unsigned>(ChoiceKind::Card)}, {"value", "4294967296"}}));
}
TEST_CASE("execution faults roll back commands freeze clocks and retain local diagnostic evidence", "[architecture][session][fault]") {
    auto content = currentContent();
    content.catalog.cards.at("small-hexagram").onDraw.assign(10001, Effect{EffectKind::GainMana, 0});
    MatchSession session(content, 42);
    auto now = MatchSession::Clock::time_point{};
    session.enableClock(false, [&] { return now; });
    bool faulted = false;
    for (int step = 0; step < 16 && !faulted; ++step) {
        const auto &state = session.engine().state();
        const auto actor = state.opening ? (state.initialFormations[0] ? 1 : 0) :
            state.decision ? state.decision->player : state.active;
        const auto view = session.viewFor(actor);
        std::optional<Command> command = view.actionSummary.safeAdvance;
        if (state.opening) {
            const auto card = std::find_if(view.cards.begin(), view.cards.end(), [&](const CardView &entry) {
                return entry.instance.owner == actor && entry.instance.definition == "small-hexagram" && entry.instance.zone == Zone::SideDeck;
            });
            REQUIRE(card != view.cards.end()); command = SelectInitialFormation{card->instance.id};
        }
        REQUIRE(command);
        const auto digest = session.engine().digest();
        const auto commands = session.recording().at("commands");
        const auto events = session.eventsSince(0, 0, 0);
        const auto revision = session.revision();
        const auto result = session.submit(actor, *command);
        if (result.accepted) continue;
        REQUIRE(result.errorCode == "execution_budget");
        CHECK(result.events.empty()); CHECK(session.engine().digest() == digest);
        CHECK(session.recording().at("commands") == commands);
        CHECK(session.eventsSince(0, events.epoch, events.next).events.empty());
        CHECK(session.revision() > revision); CHECK(session.technicalPaused());
        CHECK(session.queryActions(actor, 1, revision, {}).error == "stale_query");
        const auto remaining = session.clockView().remainingMs;
        session.freezeClock(false); now += std::chrono::hours(1); session.tick();
        CHECK(session.clockView().remainingMs == remaining); CHECK_FALSE(session.clockView().running);
        CHECK_FALSE(session.submit(actor, Surrender{}).accepted);
        const auto evidence = session.diagnostic();
        REQUIRE(evidence.at("recording").at("commands") == commands);
        const auto directory = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "session-tests" / net::token();
        writeAtomicJson(directory / "blocked", Json{{"file", true}});
        CHECK_THROWS(session.saveDiagnostic(directory / "blocked" / "diagnostic.json"));
        CHECK(session.diagnostic() == evidence);
        session.save(directory / "fault.json");
        CHECK(readJson(directory / "fault.json.diagnostic.json") == evidence);
        CHECK(replay(content, readJson(directory / "fault.json")).digest() == digest);
        faulted = true;
    }
    REQUIRE(faulted);
}
TEST_CASE("authority execution faults pause both peers without transmitting hidden diagnostic state", "[architecture][network][fault]") {
    using namespace wizard::net;
    auto content = currentContent();
    content.catalog.cards.at("small-hexagram").onDraw.assign(10001, Effect{EffectKind::GainMana, 0});
    const auto nonFaultFormation = std::find_if(content.sideDeck.begin(), content.sideDeck.end(), [](const std::string &id) {
        return id != "small-hexagram";
    });
    REQUIRE(nonFaultFormation != content.sideDeck.end());
    std::iter_swap(content.sideDeck.begin(), nonFaultFormation);
    HostSession host(content); GuestSession guest(content);
    guest.receive(host.receive(hello(content))); host.ready();
    guest.receive(host.receive({{"type", "Ready"}, {"session", host.session()}}));
    guest.receive(host.snapshot(1));
    for (int step = 0; step < 16 && !host.technicalPaused(); ++step) {
        const auto hv = host.view(); const auto gv = guest.view();
        const auto actor = hv.opening ? (!hv.initialFormationConfirmed[0] ? 0 : 1) :
            hv.decision ? 0 : gv.decision ? 1 : hv.active;
        const auto &view = actor == 0 ? hv : gv;
        auto command = view.actionSummary.safeAdvance;
        if (view.opening) {
            const auto card = std::find_if(view.cards.begin(), view.cards.end(), [&](const CardView &entry) {
                return entry.instance.owner == actor && entry.instance.definition == "small-hexagram" && entry.instance.zone == Zone::SideDeck;
            });
            REQUIRE(card != view.cards.end()); command = SelectInitialFormation{card->instance.id};
        }
        REQUIRE(command);
        if (actor == 0) host.submit(*command);
        else guest.receive(host.receive(guest.command(*command)));
        guest.receive(host.snapshot(1));
    }
    REQUIRE(host.technicalPaused()); REQUIRE(guest.technicalPaused());
    const auto snapshot = host.snapshot(1);
    CHECK_FALSE(snapshot.contains("diagnostic")); CHECK_FALSE(snapshot.contains("recording"));
    CHECK_FALSE(snapshot.at("view").contains("state")); CHECK_FALSE(snapshot.at("view").contains("digest"));
    REQUIRE(host.diagnostic().contains("recording"));
    CHECK_THROWS(host.receive({{"type", "Replay"}, {"session", host.session()}}));
    CHECK_THROWS(guest.command(Surrender{}));
    host.freeze(false); guest.receive(host.receive(guest.resume()));
    CHECK(host.technicalPaused()); CHECK(guest.technicalPaused());
    CHECK_FALSE(host.clock().running); CHECK_FALSE(guest.view().clock.running);
}
