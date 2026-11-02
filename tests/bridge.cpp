#include "wizard/bridge.hpp"
#include "wizard/network.hpp"
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <limits>
using namespace wizard;
namespace {
struct Fixture {
    const std::filesystem::path assets{std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"};
    const std::filesystem::path directory{std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "v15" /
        ("bridge-test-" + net::token())};
    bridge::LocalSession bridge;
    Fixture() {
        REQUIRE(bridge.initialize(assets, directory).at("ok") == true);
        const auto started = bridge.start(Json{{"seed", 42}});
        INFO(started.dump());
        REQUIRE(started.at("ok") == true);
    }
    ~Fixture() { std::error_code ignored; std::filesystem::remove_all(directory, ignored); }
    Json actingView() {
        auto v = bridge.snapshot(0).at("data");
        return bridge.snapshot(v.at("actingPlayer").get<int>()).at("data");
    }
    Json completeIntent(const Json &v, int operation = -1) {
        Json intent{{"source", "0"}, {"selections", Json::array()}};
        if (operation >= 0) intent["selections"].push_back({{"field", static_cast<int>(ActionField::Operation)}, {"value", std::to_string(operation)}});
        for (int depth = 0; depth < 32; ++depth) {
            const auto response = bridge.queryActions(v.at("viewer"), intent, v.at("generation"), v.at("revision"));
            INFO(response.dump()); REQUIRE(response.at("ok") == true);
            const auto &query = response.at("data");
            if (!query.at("complete").is_null()) return intent;
            REQUIRE_FALSE(query.at("options").empty());
            auto option = query.at("options").front();
            if (query.at("field") == static_cast<int>(ActionField::Operation))
                for (const auto &candidate : query.at("options")) if (candidate.at("value") != std::to_string(static_cast<int>(CommandKind::Surrender))) { option = candidate; break; }
            intent["selections"].push_back({{"field", query.at("field")}, {"value", option.at("value")}});
        }
        FAIL("query did not complete"); return intent;
    }
    Json select(const Json &v, int operation = -1) {
        return bridge.selectIntent(v.at("viewer"), completeIntent(v, operation), v.at("generation"), v.at("revision"));
    }
    Json confirm(const Json &v) { return bridge.confirmAction(v.at("generation"), v.at("revision")); }
};
}
TEST_CASE("bridge IDs preserve the complete unsigned 64 bit range without floating point") {
    const auto maximum = std::numeric_limits<std::uint64_t>::max();
    auto dto = bridge::commandDto(Respond{maximum, 4, "counter", 0, maximum, 0});
    REQUIRE(dto.at("decision") == "18446744073709551615");
    REQUIRE(dto.at("link") == "18446744073709551615");
    GameView v; v.phaseGate = maximum; v.decision = PendingDecision{maximum, 0, DecisionKind::Response};
    v.chain = ChainState{}; v.chain->id = maximum;
    const auto view = bridge::viewDto(v);
    REQUIRE(view.at("phaseGate") == dto.at("decision"));
    REQUIRE(view.at("decision").at("id") == dto.at("decision"));
    REQUIRE(view.at("chain").at("id") == dto.at("decision"));
}
TEST_CASE("bridge selection does not pay and duplicate confirmation cannot submit twice") {
    Fixture f; const auto before = f.actingView();
    REQUIRE(f.select(before).at("ok") == true);
    REQUIRE(f.actingView() == before);
    REQUIRE(f.confirm(before).at("ok") == true);
    const auto after = f.actingView();
    REQUIRE(after.at("revision") != before.at("revision"));
    REQUIRE(f.confirm(before).at("errorCode") == "stale_action");
    REQUIRE(f.actingView() == after);
}
TEST_CASE("bridge presentation batches contain only filtered references and never repeat on reads") {
    Fixture f;
    std::size_t cuesSeen{};
    for (int step = 0; step < 24; ++step) {
        const auto before = f.actingView();
        if (!before.at("actionSummary").at("actionable").get<bool>()) break;
        REQUIRE(f.select(before).at("ok") == true);
        const auto response = f.confirm(before);
        REQUIRE(response.at("ok") == true);
        const auto &data = response.at("data");
        const auto after = f.bridge.snapshot(before.at("viewer")).at("data");
        REQUIRE(data.at("generation") == after.at("generation"));
        REQUIRE(data.at("revision") == after.at("revision"));
        REQUIRE(data.at("cues").is_array());
        REQUIRE_FALSE(after.contains("cues"));
        auto visible = [&](const Json &id) {
            if (id == "0") return true;
            for (const auto *view : {&before, &after})
                for (const auto &card : view->at("cards"))
                    if (card.at("id") == id) return true;
            return false;
        };
        std::size_t sequence{};
        for (const auto &cue : data.at("cues")) {
            ++cuesSeen;
            REQUIRE(cue.at("sequence") == std::to_string(++sequence));
            for (const auto *field : {"source", "host", "target"}) {
                REQUIRE(cue.at(field).is_string());
                REQUIRE(visible(cue.at(field)));
            }
            REQUIRE_FALSE(cue.contains("card"));
            REQUIRE_FALSE(cue.contains("definition"));
            REQUIRE(cue.at("duration").get<double>() > 0);
        }
        REQUIRE(f.bridge.snapshot(before.at("viewer")).at("data") == after);
        REQUIRE(f.confirm(before).at("ok") == false);
    }
    REQUIRE(cuesSeen > 0);
}
TEST_CASE("bridge rejects invalid actors actions generations and malformed start options atomically") {
    Fixture f; const auto v = f.actingView();
    REQUIRE(f.bridge.snapshot(-1).at("ok") == false);
    REQUIRE(f.bridge.snapshot(2).at("ok") == false);
    REQUIRE(f.bridge.selectIntent(v.at("viewer"), Json{{"selections", {{{"field", 99}, {"value", "made-up"}}}}}, v.at("generation"), v.at("revision")).at("ok") == false);
    REQUIRE(f.bridge.selectIntent(v.at("viewer"), Json::object(), "0", v.at("revision")).at("ok") == false);
    REQUIRE(f.bridge.start(Json{{"seed", -1}}).at("ok") == false);
    REQUIRE(f.bridge.start(Json{{"players", Json::array()}}).at("ok") == false);
    REQUIRE(f.actingView() == v);
}
TEST_CASE("bridge pause preserves a selection but stops confirmations and AI") {
    Fixture f; const auto v = f.actingView();
    REQUIRE(f.select(v).at("ok") == true);
    REQUIRE(f.bridge.pause(true).at("ok") == true);
    REQUIRE(f.confirm(v).at("errorCode") == "match_paused");
    REQUIRE(f.bridge.stepAi(v.at("generation"), v.at("revision")).at("ok") == false);
    REQUIRE(f.bridge.pause(false).at("ok") == true);
    REQUIRE(f.confirm(v).at("ok") == true);
}
TEST_CASE("bridge release reinitialize and restart invalidate old UI handles") {
    Fixture f; const auto old = f.actingView();
    REQUIRE(f.select(old).at("ok") == true);
    REQUIRE(f.bridge.release().at("ok") == true);
    REQUIRE(f.confirm(old).at("ok") == false);
    REQUIRE(f.bridge.initialize(f.assets, f.directory).at("ok") == true);
    REQUIRE(f.bridge.start(Json::object()).at("ok") == true);
    REQUIRE(f.actingView().at("generation") != old.at("generation"));
    REQUIRE(f.confirm(old).at("ok") == false);
    const auto v = f.actingView();
    REQUIRE(f.select(v, static_cast<int>(CommandKind::Surrender)).at("ok") == true); REQUIRE(f.confirm(v).at("ok") == true);
    REQUIRE(f.bridge.restart(43).at("ok") == true);
    REQUIRE(f.actingView().at("generation") != v.at("generation"));
}
TEST_CASE("bridge whitelist excludes private cards hidden definitions seed and writable state") {
    Fixture f;
    for (int viewer = 0; viewer < 2; ++viewer) {
        const auto v = f.bridge.snapshot(viewer).at("data");
        REQUIRE_FALSE(v.contains("seed")); REQUIRE_FALSE(v.contains("rng")); REQUIRE_FALSE(v.contains("state"));
        for (const auto &c : v.at("cards")) {
            REQUIRE_FALSE(c.at("zone") == static_cast<int>(Zone::Deck));
            if (c.at("zone") == static_cast<int>(Zone::Hand)) REQUIRE(c.at("owner") == viewer);
        }
    }
    GameView view; CardView hidden; hidden.hidden = true; hidden.definition.name = "must not leak";
    hidden.instance.id = 42;
    hidden.instance.host = 7;
    hidden.instance.ringIndex = 3;
    hidden.instance.fieldOrder = 9;
    view.cards.push_back(hidden);
    const auto hiddenDto = bridge::viewDto(view).at("cards").at(0);
    REQUIRE_FALSE(hiddenDto.contains("definition"));
    REQUIRE(hiddenDto.at("host") == "7");
    REQUIRE(hiddenDto.at("ringIndex") == 3);
    REQUIRE(hiddenDto.at("fieldOrder") == "9");
    REQUIRE(bridge::viewDto(view).dump().find("must not leak") == std::string::npos);
}
TEST_CASE("dynamic setting and casting fees survive local bridge and LAN projections", "[preview2][costs]") {
    GameView view; CardView card;
    card.instance.id = 42;
    card.instance.definition = "sunburst";
    card.definition.id = "sunburst";
    card.effectiveSettingCost = 17;
    card.effectiveCastCost = 2;
    view.cards.push_back(card);
    const auto dto = bridge::viewDto(view).at("cards").at(0);
    REQUIRE(dto.at("effectiveSettingCost") == 17);
    REQUIRE(dto.at("effectiveCastCost") == 2);
    CardCatalog catalog; catalog.cards.emplace(card.definition.id, card.definition);
    const auto decoded = net::decodeView(net::encodeView(view), catalog);
    REQUIRE(decoded.cards.at(0).effectiveSettingCost == 17);
    REQUIRE(decoded.cards.at(0).effectiveCastCost == 2);
    view.cards.at(0).hidden = true;
    const auto hidden = bridge::viewDto(view).at("cards").at(0);
    REQUIRE_FALSE(hidden.contains("effectiveSettingCost"));
    REQUIRE_FALSE(hidden.contains("effectiveCastCost"));
}
TEST_CASE("storm point counts survive network projection and appear in visible bridge snapshots", "[storm][projection]") {
    GameView view;
    CardView storm;
    storm.instance.id = 42;
    storm.instance.definition = "storm-crest";
    storm.instance.stormPoints = 6;
    storm.instance.stormReleaseProgress = 2;
    storm.definition.id = "storm-crest";
    storm.definition.name = "风暴纹章";
    storm.definition.type = CardType::Formation;
    storm.definition.tags = {"风暴"};
    view.cards.push_back(storm);

    CardCatalog catalog;
    catalog.cards.emplace(storm.definition.id, storm.definition);
    const auto decoded = net::decodeView(net::encodeView(view), catalog);
    REQUIRE(decoded.cards.at(0).instance.stormPoints == 6);
    REQUIRE(decoded.cards.at(0).instance.stormReleaseProgress == 2);
    const auto visible = bridge::viewDto(decoded).at("cards").at(0);
    REQUIRE(visible.at("stormPoints") == 6);

    view.cards.at(0).hidden = true;
    const auto hidden = bridge::viewDto(view).at("cards").at(0);
    REQUIRE_FALSE(hidden.contains("stormPoints"));
}
TEST_CASE("shown concealed card remains inspectable and facedown after LAN transport", "[preview2][privacy]") {
    GameView view; CardView card;
    card.instance.id = 42;
    card.instance.definition = "fireball";
    card.instance.faceDown = true;
    card.instance.publiclyRevealed = true;
    card.definition.id = "fireball";
    card.definition.name = "火球术";
    card.definition.text = "对对方玩家造成10点火焰伤害。";
    view.cards.push_back(card);
    CardCatalog catalog; catalog.cards.emplace(card.definition.id, card.definition);
    const auto decoded = net::decodeView(net::encodeView(view), catalog);
    REQUIRE(decoded.cards.at(0).instance.publiclyRevealed);
    REQUIRE(decoded.cards.at(0).instance.faceDown);
    REQUIRE_FALSE(decoded.cards.at(0).hidden);
    const auto dto = bridge::viewDto(decoded).at("cards").at(0);
    REQUIRE(dto.at("faceDown") == true);
    REQUIRE(dto.at("definition").at("name") == "火球术");
    REQUIRE(dto.at("definition").at("text") == card.definition.text);
}
TEST_CASE("bridge AI exposes only human projection and rejects stale plans") {
    Fixture f; REQUIRE(f.bridge.release().at("ok") == true);
    REQUIRE(f.bridge.initialize(f.assets, f.directory).at("ok") == true);
    REQUIRE(f.bridge.start(Json{{"mode", "ai"}}).at("ok") == true);
    REQUIRE(f.bridge.snapshot(1).at("ok") == false);
    const auto v = f.bridge.snapshot(0).at("data");
    REQUIRE(f.bridge.stepAi("0", v.at("revision")).at("errorCode") == "stale_ai");
}
TEST_CASE("bridge retains accepted state after replay save failure") {
    Fixture f; const auto v = f.actingView();
    const auto saved = f.bridge.saveReplay();
    const std::filesystem::path file = std::filesystem::u8path(saved.at("data").at("path").get<std::string>());
    std::filesystem::remove(file);
    std::filesystem::create_directory(file); // Atomic replacement must fail, without filesystem permissions tricks.
    REQUIRE(f.select(v).at("ok") == true);
    const auto result = f.confirm(v);
    REQUIRE(result.at("ok") == true); REQUIRE(result.at("data").at("replaySaved") == false);
    REQUIRE(f.actingView().at("revision") != v.at("revision"));
    std::filesystem::remove(file);
    REQUIRE(f.bridge.saveReplay().at("ok") == true);
}
TEST_CASE("local replay autosave batches full history writes and flushes each thirty second command") {
    Fixture f;
    const auto path = std::filesystem::u8path(
        f.bridge.saveReplay().at("data").at("path").get<std::string>());
    REQUIRE(readJson(path).at("commands").size() == 0);
    for (int command = 1; command <= 32; ++command) {
        const auto view = f.actingView();
        REQUIRE(f.select(view).at("ok") == true);
        const auto result = f.confirm(view);
        REQUIRE(result.at("ok") == true);
        REQUIRE_FALSE(result.at("data").at("pending").get<bool>());
        if (command == 1) CHECK(readJson(path).at("commands").size() == 1);
        if (command > 1 && command < 32) CHECK(readJson(path).at("commands").size() == 1);
        if (command == 32) CHECK(readJson(path).at("commands").size() == 32);
    }
}
TEST_CASE("bridge current replay compatibility gates reject unsupported rules") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    MatchSession session(content, 42);
    REQUIRE(session.submit(0, Surrender{}).accepted);
    const auto record = session.recording();
    REQUIRE(replay(content, record).digest() == session.engine().digest());
    for (const auto *key : {"rulesVersion", "cardSetVersion", "contentHash", "finalDigest"}) {
        auto bad = record; bad[key] = "unknown"; REQUIRE_THROWS(replay(content, bad));
    }
    auto bad = record; bad["commands"][0]["digest"] = "tampered"; REQUIRE_THROWS(replay(content, bad));
    bad = record; bad["format"] = 1; REQUIRE_THROWS(replay(content, bad));
}

TEST_CASE("bridge local library preserves Chinese names and unknown IDs through atomic drafts") {
    Fixture f;
    auto draft = f.bridge.createDraft("default", 3).at("data");
    draft["name"] = "中文草稿 · 修复";
    draft["cards"]["removed-card-id"] = 1;
    REQUIRE_FALSE(f.bridge.validateDraft(draft).at("data").at("problems").empty());
    REQUIRE(f.bridge.saveDraft(draft).at("ok") == true);
    REQUIRE(f.bridge.query(Json{{"type", 1}, {"name", "火球"}}).at("data").at("ids") == Json::array({"fireball"}));
    REQUIRE(f.bridge.query(Json{{"type", 8}}).at("ok") == false);
    REQUIRE(f.bridge.initialize(f.assets, f.directory).at("ok") == true);
    const auto saved = f.bridge.library().at("data").at("drafts").at(0);
    REQUIRE(saved.at("name") == draft.at("name"));
    REQUIRE(saved.at("cards").at("removed-card-id") == 1);
    REQUIRE_FALSE(saved.at("problems").empty());
    const auto path = f.directory / "decks.json";
    std::filesystem::remove(path); std::filesystem::create_directory(path);
    auto changed = draft; changed["name"] = "不能覆盖";
    REQUIRE(f.bridge.saveDraft(changed).at("ok") == false);
    REQUIRE(f.bridge.library().at("data").at("drafts").at(0) == saved);
}

TEST_CASE("bridge incremental queries are read only and bound to exact contexts") {
    Fixture f; const auto v = f.actingView();
    const auto initial = f.bridge.eventsSince(v.at("viewer"), "0", "0").at("data");
    const auto intent = f.completeIntent(v);
    const auto query = f.bridge.queryActions(v.at("viewer"), intent, v.at("generation"), v.at("revision"));
    REQUIRE(query.at("ok") == true);
    const auto &queryData = query.at("data");
    const std::string decisionId = v.at("decision").is_object()
        ? v.at("decision").at("id").get<std::string>()
        : v.value("opening", false) ? "opening" : "0";
    REQUIRE(queryData.at("schema") == bridge::schemaVersion);
    REQUIRE(queryData.at("generation") == v.at("generation"));
    REQUIRE(queryData.at("revision") == v.at("revision"));
    REQUIRE(queryData.at("viewer") == v.at("viewer"));
    REQUIRE(queryData.at("decisionId") == decisionId);
    REQUIRE(f.actingView() == v);
    REQUIRE_FALSE(v.contains("actions")); REQUIRE_FALSE(v.contains("events"));
    REQUIRE(v.at("schema") == bridge::schemaVersion);
    REQUIRE(f.bridge.selectIntent(v.at("viewer"), intent, v.at("generation"), v.at("revision")).at("ok") == true);
    REQUIRE(f.bridge.cancelAction().at("ok") == true);
    REQUIRE(f.confirm(v).at("ok") == false);
    REQUIRE(f.actingView() == v);
    REQUIRE(f.bridge.eventsSince(v.at("viewer"), initial.at("epoch"), initial.at("next")).at("data").at("events").empty());
    REQUIRE(f.select(v).at("ok") == true); REQUIRE(f.confirm(v).at("ok") == true);
    REQUIRE(f.bridge.queryActions(v.at("viewer"), intent, v.at("generation"), v.at("revision")).at("ok") == false);
    const auto now = f.bridge.snapshot(v.at("viewer")).at("data");
    const auto idle = f.bridge.pollMatch(v.at("viewer"), now.at("generation"), now.at("revision")).at("data");
    REQUIRE_FALSE(idle.contains("snapshot")); REQUIRE(idle.contains("clock"));
}

TEST_CASE("bridge local settings and leave preserve match on persistence failure") {
    Fixture f;
    auto settings = f.bridge.library().at("data").at("settings");
    settings["masterVolume"] = 25;
    REQUIRE(f.bridge.applySettings(settings).at("ok") == true);
    std::filesystem::remove(f.directory / "settings.json");
    std::filesystem::create_directory(f.directory / "settings.json");
    settings["masterVolume"] = 90;
    REQUIRE(f.bridge.applySettings(settings).at("ok") == false);
    REQUIRE(f.bridge.library().at("data").at("settings").at("masterVolume") == 25);
    const auto v = f.actingView();
    auto record = f.bridge.saveReplay().at("data").at("path").get<std::string>();
    const auto path = std::filesystem::u8path(record);
    std::filesystem::remove(path); std::filesystem::create_directory(path);
    REQUIRE(f.bridge.leaveMatch(v.at("viewer")).at("ok") == false);
    REQUIRE(f.actingView() == v);
    std::filesystem::remove(path);
    REQUIRE(f.bridge.leaveMatch(v.at("viewer")).at("ok") == true);
    REQUIRE(f.bridge.snapshot(0).at("ok") == false);
    REQUIRE(f.confirm(v).at("ok") == false);
}

TEST_CASE("bridge replay frames rewind without changing the live match and validate the unseen suffix") {
    Fixture f;
    const auto initial = f.bridge.snapshot(0).at("data");
    auto v = f.actingView();
    REQUIRE(f.select(v).at("ok") == true);
    REQUIRE(f.confirm(v).at("ok") == true);
    const auto live = f.bridge.snapshot(0).at("data");
    const auto saved = f.bridge.saveReplay();
    const auto path = std::filesystem::u8path(saved.at("data").at("path").get<std::string>());
    auto record = readJson(path);
    const auto frame = f.bridge.replayFrame(path, 0, 0);
    REQUIRE(frame.at("ok") == true);
    REQUIRE(frame.at("data").at("cards") == initial.at("cards"));
    REQUIRE(frame.at("data").at("players") == initial.at("players"));
    REQUIRE(f.bridge.replayFrame(path, 1, 0).at("data").at("digest") == record.at("finalDigest"));
    REQUIRE(f.bridge.snapshot(0).at("data") == live);
    REQUIRE_FALSE(f.bridge.replayFrame(path, 2, 0).at("ok").get<bool>());
    REQUIRE_FALSE(f.bridge.replayFrame(path, 0, 2).at("ok").get<bool>());
    record["commands"][0]["digest"] = "tampered";
    writeJson(path, record);
    REQUIRE_FALSE(f.bridge.replayFrame(path, 0, 0).at("ok").get<bool>());
    REQUIRE(f.bridge.snapshot(0).at("data") == live);
}

TEST_CASE("v2.5 command DTO and wire codec retain opening recipe materials targets and exact decisions") {
    const auto decision = std::numeric_limits<std::uint64_t>::max();
    const std::vector<Command> commands = {
        SetFormation{42, 8, {9, 10}}, StartAnalysis{2, 3, 4, {4, 5}},
        PreloadWord{2, true, 3, 4, {4, 5}}, ActivateSpell{2, 4, {4, 5}},
        PrepareCast{2, 4, 6, {4, 5}, decision}, FlipAmbush{2, 4, {4, 5}, decision},
        Choose{decision, 7, 2},
        Respond{decision, 2, "counter", 4, decision, 6, {4, 5}},
        PassResponse{decision}, AdvancePhase{decision}, AdvancePhase{decision, true}, SelectInitialFormation{42}, Timeout{}};
    for (const auto &command : commands) {
        const auto encoded = encodeCommand(command);
        REQUIRE(encodeCommand(decodeCommand(encoded)) == encoded);
        const auto dto = bridge::commandDto(command);
        for (const auto &[key, value] : dto.items()) {
            if (key == "type" || key == "release" || key == "ability" || key == "ringIndex" || key == "skipCast") continue;
            if (value.is_array()) for (const auto &id : value) REQUIRE(id.is_string());
            else if (key == "option") { REQUIRE(value.at("kind").is_number_integer()); REQUIRE(value.at("value").is_string()); }
            else REQUIRE(value.is_string());
        }
    }
}
