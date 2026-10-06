#include "wizard/application.hpp"
#include <atomic>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>
using namespace wizard;
using namespace wizard::app;
namespace {
struct AppFixture {
    Content content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    std::filesystem::path directory;
    AppFixture() {
        static std::atomic<unsigned> serial{};
        directory = std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "application-tests" /
                    (std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + "-" +
                     std::to_string(++serial));
        std::filesystem::create_directories(directory);
    }
    ~AppFixture() {
        std::error_code error;
        std::filesystem::remove_all(directory, error);
    }
    Application app() {
        return Application(content, directory,
                           loadPresets(content, std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"));
    }
    MatchConfig config() {
        MatchConfig c;
        for (auto &p : c.players)
            p.cards = content.deck;
        return c;
    }
};
void start(Application &a, const MatchConfig &c) {
    std::string error;
    INFO(error);
    REQUIRE(a.start(c, error));
}
} // namespace
TEST_CASE("settings serialize strictly and multiply master and sound volume") {
    Settings s;
    s.masterVolume = 50;
    s.soundVolume = 20;
    s.width = 1920;
    s.height = 1080;
    s.automaticPhases = false;
    REQUIRE(s.effectiveSoundVolume() == 10.f);
    REQUIRE(encodeSettings(decodeSettings(encodeSettings(s))) == encodeSettings(s));
    for (const auto *field : {"masterVolume", "soundVolume", "windowMode", "width", "height", "format"}) {
        auto j = encodeSettings(s);
        j[field] = -1;
        REQUIRE_THROWS(decodeSettings(j));
        j = encodeSettings(s);
        j[field] = 1.5;
        REQUIRE_THROWS(decodeSettings(j));
    }
    auto j = encodeSettings(s);
    j["automaticPhases"] = 1;
    REQUIRE_THROWS(decodeSettings(j));
    j = encodeSettings(s);
    j["masterVolume"] = 101;
    REQUIRE_THROWS(decodeSettings(j));
}
TEST_CASE("menu starts without constructing a match and settings survive recreation") {
    AppFixture f;
    auto a = f.app();
    REQUIRE(a.page() == Page::Menu);
    REQUIRE_FALSE(a.hasMatch());
    REQUIRE_THROWS(a.match());
    REQUIRE(a.paused());
    a.openSettings();
    REQUIRE(a.page() == Page::Settings);
    Settings s;
    s.masterVolume = 0;
    s.soundVolume = 35;
    s.windowMode = WindowMode::Borderless;
    s.width = 1280;
    s.height = 800;
    s.automaticPhases = false;
    std::string error;
    REQUIRE(a.applySettings(s, error));
    a.closeSettings();
    REQUIRE(a.page() == Page::Menu);
    auto b = f.app();
    REQUIRE(encodeSettings(b.settings()) == encodeSettings(s));
    REQUIRE(b.notice().empty());
    REQUIRE_FALSE(std::filesystem::exists(f.directory / "replays"));
}
TEST_CASE("invalid settings preserve original file until explicit successful replacement") {
    AppFixture f;
    std::ofstream(f.directory / "settings.json") << "broken";
    auto a = f.app();
    REQUIRE_FALSE(a.notice().empty());
    REQUIRE(a.settings().masterVolume == 80);
    std::ifstream original(f.directory / "settings.json");
    std::string text;
    original >> text;
    REQUIRE(text == "broken");
    original.close();
    Settings s;
    s.masterVolume = 40;
    std::string error;
    REQUIRE(a.applySettings(s, error));
    REQUIRE(f.app().settings().masterVolume == 40);
}
TEST_CASE("failed atomic settings replacement keeps committed values and temporary files are cleaned") {
    AppFixture f;
    auto a = f.app();
    std::string error;
    Settings original;
    REQUIRE(a.applySettings(original, error));
    // A directory cannot be replaced by the atomic file operation.
    std::filesystem::remove(f.directory / "settings.json");
    std::filesystem::create_directory(f.directory / "settings.json");
    Settings draft;
    draft.masterVolume = 1;
    REQUIRE_FALSE(a.applySettings(draft, error));
    REQUIRE(a.settings().masterVolume == 80);
    REQUIRE_FALSE(error.empty());
    for (const auto &p : std::filesystem::directory_iterator(f.directory))
        REQUIRE(p.path().filename() == "settings.json");
}
TEST_CASE("setup uses independent legal presets and records both actual configurations") {
    AppFixture f;
    auto a = f.app();
    REQUIRE(a.presets().size() >= 4);
    for (const auto &preset : a.presets()) {
        REQUIRE(Rules::deckErrors(f.content.catalog, preset.deck).empty());
        REQUIRE_FALSE(preset.description.empty());
    }
    auto config = f.config();
    config.players[1] = a.presets()[1].deck;
    REQUIRE(config.players[0].cards != config.players[1].cards);
    a.navigate(Page::HotseatSetup);
    start(a, config);
    REQUIRE(a.page() == Page::Match);
    REQUIRE_FALSE(a.paused());
    auto j = readJson(a.replayPath());
    REQUIRE(j["players"][0]["cards"] != j["players"][1]["cards"]);
    REQUIRE(replay(f.content, j).digest() == a.match().engine().digest());
    auto digest = a.match().engine().digest();
    std::string error;
    REQUIRE_FALSE(a.start(config, error));
    REQUIRE(a.match().engine().digest() == digest);
}
TEST_CASE("illegal match configuration or unavailable replay directory never creates a session") {
    AppFixture f;
    auto a = f.app();
    a.navigate(Page::HotseatSetup);
    auto c = f.config();
    c.players[0].cards.pop_back();
    std::string error;
    REQUIRE_FALSE(a.start(c, error));
    REQUIRE_FALSE(a.hasMatch());
    REQUIRE(a.page() == Page::HotseatSetup);
    std::ofstream(f.directory / "replays") << "blocked";
    REQUIRE_FALSE(a.start(f.config(), error));
    REQUIRE_FALSE(a.hasMatch());
}
TEST_CASE("match settings pause all submissions and retain decision gate and digest") {
    AppFixture f;
    auto a = f.app();
    start(a, f.config());
    const auto digest = a.match().engine().digest();
    const auto actor = a.match().engine().state().active;
    const auto gate = a.match().engine().state().phaseGate;
    a.openSettings();
    REQUIRE(a.paused());
    REQUIRE(a.page() == Page::Settings);
    REQUIRE_FALSE(a.submit(actor, AdvancePhase{gate}, a.generation()).accepted);
    REQUIRE_FALSE(a.submit(actor, Surrender{}, a.generation()).accepted);
    REQUIRE(a.match().engine().digest() == digest);
    a.closeSettings();
    REQUIRE_FALSE(a.paused());
    REQUIRE(a.submit(actor, AdvancePhase{gate}, a.generation()).accepted);
}
TEST_CASE("return confirmation cancel preserves paused settings and active match") {
    AppFixture f;
    auto a = f.app();
    start(a, f.config());
    auto digest = a.match().engine().digest();
    a.openSettings();
    a.requestLeave();
    REQUIRE(a.page() == Page::ConfirmLeave);
    a.cancelConfirmation();
    REQUIRE(a.page() == Page::Settings);
    REQUIRE(a.hasMatch());
    REQUIRE(a.match().engine().digest() == digest);
    a.closeSettings();
    REQUIRE(a.page() == Page::Match);
}
TEST_CASE("return to menu completes and saves a replay then invalidates old match actions") {
    AppFixture f;
    auto a = f.app();
    start(a, f.config());
    const auto actor = a.match().engine().state().active;
    const auto epoch = a.generation();
    const auto path = a.replayPath();
    a.requestLeave();
    std::string error;
    REQUIRE(a.confirmLeave(actor, error));
    REQUIRE(a.page() == Page::Menu);
    REQUIRE_FALSE(a.hasMatch());
    auto ended = replay(f.content, readJson(path));
    REQUIRE(ended.state().result == 1 - actor);
    start(a, f.config());
    REQUIRE(a.generation() > epoch);
    auto digest = a.match().engine().digest();
    auto result = a.submit(actor, Surrender{}, epoch);
    REQUIRE_FALSE(result.accepted);
    REQUIRE(result.errorCode == "stale_match");
    REQUIRE(a.match().engine().digest() == digest);
}
TEST_CASE("failed leave save preserves the live session and confirmation can be canceled") {
    AppFixture f;
    auto a = f.app();
    start(a, f.config());
    const auto digest = a.match().engine().digest();
    auto actor = a.match().engine().state().active;
    std::filesystem::remove(a.replayPath());
    std::filesystem::create_directory(a.replayPath());
    a.requestLeave(true);
    std::string error;
    REQUIRE_FALSE(a.confirmLeave(actor, error));
    REQUIRE(a.hasMatch());
    REQUIRE(a.match().engine().digest() == digest);
    REQUIRE_FALSE(a.quitRequested());
    REQUIRE(a.page() == Page::ConfirmLeave);
    a.cancelConfirmation();
    REQUIRE(a.page() == Page::Match);
}
TEST_CASE("menu quit is immediate while in-match quit requires saved confirmation") {
    AppFixture f;
    auto a = f.app();
    a.requestLeave(true);
    REQUIRE(a.quitRequested());
    auto b = f.app();
    start(b, f.config());
    b.requestLeave(true);
    REQUIRE_FALSE(b.quitRequested());
    std::string error;
    REQUIRE(b.confirmLeave(b.match().engine().state().active, error));
    REQUIRE(b.quitRequested());
    REQUIRE_FALSE(b.hasMatch());
}
TEST_CASE("surrender through settings yields a result and restart preserves both chosen decks") {
    AppFixture f;
    auto a = f.app();
    auto config = f.config();
    config.players[1] = a.presets()[1].deck;
    start(a, config);
    a.openSettings();
    a.requestSurrender();
    REQUIRE(a.page() == Page::ConfirmSurrender);
    a.cancelConfirmation();
    REQUIRE(a.page() == Page::Settings);
    a.requestSurrender();
    std::string error;
    auto actor = a.match().engine().state().active;
    REQUIRE(a.confirmSurrender(actor, error));
    REQUIRE(a.page() == Page::Match);
    REQUIRE(a.match().engine().state().result == 1 - actor);
    REQUIRE(replay(f.content, readJson(a.replayPath())).digest() == a.match().engine().digest());
    const auto epoch = a.generation();
    REQUIRE(a.restart(91, error));
    REQUIRE(a.configuration().seed == 91);
    REQUIRE(a.configuration().players[1].cards == config.players[1].cards);
    REQUIRE(a.generation() > epoch);
}
TEST_CASE("saving while paused retains exact rule state and menu navigation cannot abandon a match") {
    AppFixture f;
    auto a = f.app();
    start(a, f.config());
    a.openSettings();
    auto digest = a.match().engine().digest();
    a.navigate(Page::Menu);
    REQUIRE(a.page() == Page::Settings);
    std::string error;
    REQUIRE(a.saveReplay(error));
    REQUIRE(a.match().engine().digest() == digest);
    REQUIRE(replay(f.content, readJson(a.replayPath())).digest() == digest);
}
TEST_CASE("surrender save failure leaves the live session unmodified") {
    AppFixture f;
    auto a = f.app();
    start(a, f.config());
    a.openSettings();
    const auto digest = a.match().engine().digest();
    std::filesystem::remove(a.replayPath());
    std::filesystem::create_directory(a.replayPath());
    a.requestSurrender();
    std::string error;
    REQUIRE_FALSE(a.confirmSurrender(a.match().engine().state().active, error));
    REQUIRE(a.page() == Page::ConfirmSurrender);
    REQUIRE(a.match().engine().digest() == digest);
    a.cancelConfirmation();
    REQUIRE(a.page() == Page::Settings);
}
TEST_CASE("restart save failure retains result configuration and generation") {
    AppFixture f;
    auto a = f.app();
    start(a, f.config());
    REQUIRE(a.submit(a.match().engine().state().active, Surrender{}, a.generation()).accepted);
    const auto digest = a.match().engine().digest();
    const auto epoch = a.generation();
    std::filesystem::rename(f.directory / "replays", f.directory / "saved-replays");
    std::ofstream(f.directory / "replays") << "blocked";
    std::string error;
    REQUIRE_FALSE(a.restart(99, error));
    REQUIRE(a.match().engine().digest() == digest);
    REQUIRE(a.configuration().seed == 42);
    REQUIRE(a.generation() == epoch);
    REQUIRE(a.page() == Page::Match);
}
