#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
#include <limits>
using namespace wizard;
namespace {
struct Fixture {
    const std::filesystem::path assets{std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"};
    const std::filesystem::path directory{std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "v15" /
        ("bridge-test-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()))};
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
    Json select(const Json &v, std::size_t i = 0) {
        return bridge.selectAction(v.at("viewer"), v.at("actions").at(i).at("id"), v.at("generation"), v.at("revision"));
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
        if (before.at("actions").empty()) break;
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
    REQUIRE(f.bridge.selectAction(v.at("viewer"), "made-up", v.at("generation"), v.at("revision")).at("ok") == false);
    REQUIRE(f.bridge.selectAction(v.at("viewer"), v.at("actions").at(0).at("id"), "0", v.at("revision")).at("ok") == false);
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
    std::size_t surrender{};
    for (; surrender < v.at("actions").size(); ++surrender)
        if (v.at("actions").at(surrender).at("command").at("type") == 11) break;
    REQUIRE(f.select(v, surrender).at("ok") == true); REQUIRE(f.confirm(v).at("ok") == true);
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
    view.cards.push_back(hidden);
    REQUIRE_FALSE(bridge::viewDto(view).at("cards").at(0).contains("definition"));
    REQUIRE(bridge::viewDto(view).dump().find("must not leak") == std::string::npos);
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
TEST_CASE("Alpha program whitelist retains every content and digest compatibility gate") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    const auto legacy = readJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "tests/fixtures/alpha-v1-replay.json");
    REQUIRE(legacy.at("programVersion") == "1.0.0-alpha");
    REQUIRE(replay(content, legacy).digest() == legacy.at("finalDigest"));
    MatchSession session(content, 42);
    REQUIRE(session.submit(session.engine().state().active, Surrender{}).accepted);
    auto record = session.recording();
    for (const auto *version : {"1.0.0-alpha", "1.1.0-alpha"}) {
        record["programVersion"] = version;
        REQUIRE(replay(content, record).digest() == session.engine().digest());
    }
    for (const auto *key : {"programVersion", "rulesVersion", "cardSetVersion", "contentHash", "finalDigest"}) {
        auto bad = record; bad[key] = "unknown"; REQUIRE_THROWS(replay(content, bad));
    }
    auto bad = record; bad["commands"][0]["digest"] = "tampered"; REQUIRE_THROWS(replay(content, bad));
    bad = record; bad["format"] = 1; REQUIRE_THROWS(replay(content, bad));
}

TEST_CASE("bridge local library preserves Chinese names and unknown IDs through atomic drafts") {
    Fixture f;
    auto draft = f.bridge.createDraft("default").at("data");
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

TEST_CASE("bridge interaction reuses target and confirmation without mutation or truncated IDs") {
    Fixture f;
    auto v = f.actingView();
    for (int n = 0; n < 30 && v.at("phase") != static_cast<int>(Phase::Main); ++n) {
        REQUIRE(f.select(v).at("ok") == true); REQUIRE(f.confirm(v).at("ok") == true);
        v = f.actingView();
    }
    REQUIRE(v.at("phase") == static_cast<int>(Phase::Main));
    auto request = [&](Json command) { return f.bridge.interact(v.at("viewer"), command, v.at("generation"), v.at("revision")); };
    REQUIRE(request(Json{{"operation", "select"}, {"card", "4294967297"}}).at("ok") == false);
    REQUIRE(request(Json{{"operation", "select"}, {"card", "1x"}}).at("ok") == false);
    Json selected;
    for (const auto &action : v.at("actions")) {
        if (action.at("source") == "0") continue;
        const auto result = request(Json{{"operation", "select"}, {"card", action.at("source")}});
        if (result.at("ok") == true && !result.at("data").at("groups").empty()) { selected = result.at("data"); break; }
    }
    REQUIRE_FALSE(selected.is_null());
    selected = request(Json{{"operation", "activate"}, {"group", 0}}).at("data");
    for (int n = 0; n < 4 && selected.at("pending").is_null(); ++n) {
        REQUIRE_FALSE(selected.at("candidates").empty());
        selected = request(Json{{"operation", "pick"}, {"card", selected.at("candidates").at(0)}}).at("data");
    }
    REQUIRE_FALSE(selected.at("pending").is_null());
    REQUIRE(f.actingView() == v);
    REQUIRE(f.bridge.pause(true).at("ok") == true);
    REQUIRE(request(Json{{"operation", "activate"}, {"group", 0}}).at("ok") == false);
    REQUIRE(f.bridge.pause(false).at("ok") == true);
    REQUIRE(f.confirm(v).at("ok") == true);
    REQUIRE(f.confirm(v).at("ok") == false);
    REQUIRE(request(Json{{"operation", "select"}, {"card", "1"}}).at("ok") == false);
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
