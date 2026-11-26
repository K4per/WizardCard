#include "wizard/lesson_script.hpp"
#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

using namespace wizard;
using namespace wizard::app;

namespace {
const auto source = std::filesystem::path(WIZARD_SOURCE_DIR);

const Content &content() {
    static const auto value = loadContent(source / "assets/release-v260");
    return value;
}

struct Courses {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("wizard-lessons-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    std::filesystem::path lessons = path / "lessons";
    std::filesystem::path text = path / "text";

    Courses() {
        std::filesystem::create_directories(lessons);
        writeJson(text / "manifest.json", {{"defaultLocale", "zh_CN"}, {"locales", {"zh_CN"}},
            {"files", {"tutorial.json"}}, {"fallbacks", Json::object()}});
        auto messages = readJson(source / "godot/data/text/zh_CN/tutorial.json");
        messages.update({
            {"tutorial.test.title", "标题"}, {"tutorial.test.intro", "选择 {card}"},
            {"tutorial.test.play", "行动"}, {"tutorial.test.blocked", "请按提示操作"}
        });
        writeJson(text / "zh_CN/tutorial.json", messages);
    }
    ~Courses() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
    LessonCatalog load(const TextCatalog::Diagnostic &diagnostic = {}) const {
        return LessonCatalog::load(lessons, content().catalog, TextCatalog::load(text), diagnostic);
    }
    void write(const Json &lesson) const {
        writeJson(lessons / (lesson.at("id").get<std::string>() + ".json"), lesson);
    }
};

Json step(const std::string &id) {
    return {{"id", id}, {"hint", "tutorial.test.play"}, {"continue", true}};
}

Json lesson(const std::string &id, int order, Json steps = Json::array({step("intro")})) {
    const Json player{{"mana", 20}, {"formation", "v260-formation-0016"},
        {"deck", {"v260-analytic-0001"}}, {"hand", {"v260-word-0001", "v260-talisman-0003"}}};
    return {{"id", id}, {"order", order}, {"titleKey", "tutorial.test.title"}, {"cardRevision", 13},
        {"scenario", {{"phase", "Main"}, {"players", {player, player}}}}, {"steps", std::move(steps)}};
}

ai::CandidateSource actions(const GameEngine &engine, PlayerId player) {
    return [&engine, player](const auto &visitor) { engine.visitLegalActions(player, visitor); };
}

void submit(LessonScript &script, GameEngine &engine, PlayerId player, const Command &command) {
    const auto before = engine.viewFor(player);
    const auto result = engine.submit(player, command);
    REQUIRE(result.accepted);
    script.observe(player, command, before, engine.viewFor(player), result.events);
}
}

TEST_CASE("lesson discovery sorts valid courses and isolates bad definitions", "[lesson-script]") {
    Courses courses;
    courses.write(lesson("aardvark", 8));
    courses.write(lesson("zebra", 2));
    auto revision = lesson("old", 3);
    revision["cardRevision"] = 12;
    courses.write(revision);
    auto missingText = lesson("no-text", 4);
    missingText["steps"][0]["hint"] = "tutorial.test.missing";
    courses.write(missingText);
    auto unknownCommand = lesson("bad-command", 5);
    unknownCommand["steps"][0]["allow"] = {"NeverACommand"};
    courses.write(unknownCommand);
    auto missingCard = lesson("no-card", 6);
    missingCard["steps"][0]["allow"] = {{{"command", "SetAmbush"}, {"card", "missing-card"}}};
    courses.write(missingCard);
    auto wrongTarget = lesson("bad-highlight", 7);
    wrongTarget["steps"][0]["highlight"] = {"unknown:learner.hand"};
    courses.write(wrongTarget);
    courses.write(lesson("duplicate-step", 9, Json::array({step("intro"), step("intro")})));
    courses.write(lesson("duplicate-first", 10));
    courses.write(lesson("duplicate-second", 10));
    auto wrongId = lesson("wrong-file", 11);
    writeJson(courses.lessons / "mismatch.json", wrongId);
    auto wrongParameter = lesson("bad-parameter", 12);
    wrongParameter["steps"][0]["hint"] = "tutorial.test.intro";
    courses.write(wrongParameter);

    std::vector<std::string> diagnostics;
    const auto catalog = courses.load([&](const std::string &message) { diagnostics.push_back(message); });
    REQUIRE(catalog.lessons().size() == 2);
    CHECK(catalog.lessons()[0].scenario.id() == "zebra");
    CHECK(catalog.lessons()[1].scenario.id() == "aardvark");
    REQUIRE(catalog.find("zebra"));
    CHECK(catalog.find("zebra")->titleKey == "tutorial.test.title");
    CHECK_FALSE(catalog.find("mismatch"));
    CHECK(diagnostics.size() == 10);
}

TEST_CASE("lesson branches on an accepted learner action in the requested turn", "[lesson-script]") {
    Courses courses;
    const Json command{{"command", "PreloadWord"}, {"card", "v260-word-0001"}, {"target", "opponent"}};
    Json play{{"id", "play"}, {"hint", "tutorial.test.play"}, {"blockedHint", "tutorial.test.blocked"},
        {"allow", Json::array({command})}, {"until", Json::array({
            Json{{"command", "PassResponse"}, {"actor", "opponent"}, {"turn", 2}, {"next", "wrong"}},
            Json{{"command", command}, {"active", "learner"}, {"turn", 1}, {"phase", "Main"}, {"next", "end"}}
        })}};
    Json intro{{"id", "intro"}, {"hint", "tutorial.test.intro"}, {"continue", true},
        {"params", {{"card", {{"card", "v260-word-0001"}}}}}, {"hintAnchor", "bottom"},
        {"highlight", {"zone:opponent.casting", "card:hand.v260-word-0001", "ui:phase"}}};
    courses.write(lesson("branch", 1, Json::array({intro, play, step("wrong"), step("end")})));
    const auto catalog = courses.load();
    const auto *definition = catalog.find("branch");
    REQUIRE(definition);
    auto engine = definition->scenario.start(content().catalog);
    LessonScript script(*definition);
    auto view = engine.viewFor(0);
    CHECK(script.snapshot() == Json{{"lessonId", "branch"}, {"step", "intro"},
        {"hintKey", "tutorial.test.intro"}, {"params", intro["params"]}, {"hintAnchor", "bottom"},
        {"highlights", intro["highlight"]}, {"blockedHint", ""}, {"canContinue", true}, {"complete", false}});
    CHECK(script.canContinue(view));
    REQUIRE(script.continueLesson(view));
    CHECK_FALSE(script.continueLesson(view));
    CHECK(script.blockedHint() == "tutorial.test.blocked");

    std::optional<Command> selected;
    actions(engine, 0)([&](const auto &candidate) {
        if (std::holds_alternative<PreloadWord>(candidate.command) && script.allows(view, candidate.command)) {
            selected = candidate.command;
            return false;
        }
        return true;
    });
    REQUIRE(selected);
    auto word = std::get<PreloadWord>(*selected);
    CHECK(word.target == playerTarget(1));
    word.target = playerTarget(0);
    CHECK_FALSE(script.allows(view, word));
    CHECK_FALSE(script.allows(view, SetAmbush{}));
    CHECK(script.allows(view, AdvancePhase{}));
    CHECK(script.allows(view, Surrender{}));
    auto later = view;
    later.globalTurn = 2;
    script.observe(0, *selected, later, later, {});
    CHECK(script.snapshot()["step"] == "play");
    script.observe(0, PassResponse{}, later, later, {});
    CHECK(script.snapshot()["step"] == "play");
    submit(script, engine, 0, *selected);
    CHECK(script.snapshot()["step"] == "end");
    REQUIRE(script.continueLesson(engine.viewFor(0)));
    CHECK(script.complete());
    CHECK_FALSE(script.snapshot()["canContinue"].get<bool>());
}

TEST_CASE("lesson opponent chooses the real legal losing gesture and observes the opening winner", "[lesson-script]") {
    Courses courses;
    const Json wait{{"id", "opening"}, {"hint", "tutorial.test.play"},
        {"allow", {"SubmitOpeningGesture"}},
        {"until", {{"event", "opening_winner"}, {"actor", "opponent"}}}};
    auto document = lesson("opening", 1, Json::array({wait}));
    const Json player{{"deck", content().deck}, {"sideDeck", content().sideDeck}};
    document["scenario"] = {{"opening", true}, {"players", {player, player}}};
    document["openingOpponent"] = "learnerWins";
    courses.write(document);

    const auto catalog = courses.load();
    const auto *definition = catalog.find("opening");
    REQUIRE(definition);
    for (int gesture = 0; gesture != 3; ++gesture) {
        auto engine = definition->scenario.start(content().catalog);
        LessonScript script(*definition);
        CHECK_FALSE(script.opponent(engine.viewFor(1), actions(engine, 1)));
        submit(script, engine, 0, SubmitOpeningGesture{1, gesture});
        CHECK_FALSE(script.complete());
        const auto selection = script.opponent(engine.viewFor(1), actions(engine, 1));
        REQUIRE(selection);
        CHECK(std::get<SubmitOpeningGesture>(selection->command).gesture == (gesture + 2) % 3);
        submit(script, engine, 1, selection->command);
        CHECK(engine.state().first == 0);
        CHECK(script.complete());
    }
}

TEST_CASE("lesson completion waits for the declared event and leaves forced decisions legal", "[lesson-script]") {
    Courses courses;
    const Json choose{{"command", "Choose"}, {"option", {{"card", "v260-word-0001"}}}};
    auto document = lesson("choices", 1, Json::array({
        Json{{"id", "choose"}, {"hint", "tutorial.test.play"}, {"allow", Json::array({choose})},
            {"until", {{"command", choose}}}}
    }));
    document["complete"] = {{"event", "ChainResolved"}, {"actor", "learner"}};
    courses.write(document);
    const auto catalog = courses.load();
    const auto *definition = catalog.find("choices");
    REQUIRE(definition);
    const auto engine = definition->scenario.start(content().catalog);
    auto view = engine.viewFor(0);
    const auto card = std::find_if(view.cards.begin(), view.cards.end(), [](const auto &item) {
        return item.instance.zone == Zone::Hand && item.definition.id == "v260-word-0001";
    });
    REQUIRE(card != view.cards.end());
    view.decision = PendingDecision{14, 0, DecisionKind::Discard, {ChoiceValue::card(card->instance.id)}, false};
    const Choose other{14, ChoiceValue::mode(ChoiceMode::Confirm)};
    const Choose selected{14, ChoiceValue::card(card->instance.id)};
    LessonScript script(*definition);
    CHECK(script.allows(view, other));
    CHECK_FALSE(script.allows(view, Choose{15, other.option}));
    script.observe(0, other, view, view, {});
    CHECK_FALSE(script.complete());
    script.observe(0, selected, view, view, {});
    CHECK_FALSE(script.complete());
    script.observe(1, PassResponse{}, view, view, {{"chain_closed", ""}});
    CHECK_FALSE(script.complete());
    script.observe(0, PassResponse{}, view, view, {{"chain_closed", ""}});
    CHECK(script.complete());
}
TEST_CASE("scripted opponent selects a matching legal source and opens the learner's response", "[lesson-script]") {
    Courses courses;
    const Json word{{"command", "PreloadWord"}, {"card", "v260-word-0001"}, {"target", "learner"}};
    const Json waiting{{"id", "waiting"}, {"hint", "tutorial.test.play"},
        {"until", {{"event", "ResponseOpened"}, {"command", word}, {"actor", "opponent"}, {"turn", 2}}}};
    auto document = lesson("response", 1, Json::array({waiting, step("respond")}));
    document["scenario"]["activePlayer"] = 1;
    document["scenario"]["turn"] = 2;
    document["scenario"]["players"][0]["faceDown"] = {{{"card", "v260-talisman-0003"}}};
    document["opponent"] = {{{"step", "waiting"}, {"do", word}, {"turn", 2}}};
    courses.write(document);

    const auto catalog = courses.load();
    const auto *definition = catalog.find("response");
    REQUIRE(definition);
    auto engine = definition->scenario.start(content().catalog);
    LessonScript script(*definition);
    const auto action = script.opponent(engine.viewFor(1), actions(engine, 1));
    REQUIRE(action);
    REQUIRE(std::holds_alternative<PreloadWord>(action->command));
    CHECK(std::get<PreloadWord>(action->command).target == playerTarget(0));
    submit(script, engine, 1, action->command);
    REQUIRE(engine.viewFor(0).decision);
    CHECK(engine.viewFor(0).decision->kind == DecisionKind::Response);
    CHECK_FALSE(engine.viewFor(1).decision);
    CHECK(script.snapshot()["step"] == "respond");
}

TEST_CASE("local bridge plays a catalog lesson and verifies its course-only replay", "[lesson-script][bridge]") {
    Courses fixture;
    const auto assets = fixture.path / "assets";
    const auto data = source / "assets" / "release-v260";
    std::filesystem::create_directories(assets);
    for (const auto *file : {"cards.json", "deck.json", "side-deck.json", "presets.json"})
        std::filesystem::copy_file(data / file, assets / file);
    std::filesystem::copy(fixture.text, assets / "text", std::filesystem::copy_options::recursive);
    fixture.lessons = assets / "tutorial" / "lessons";
    std::filesystem::create_directories(fixture.lessons);
    const Json word{{"command", "PreloadWord"}, {"card", "v260-word-0001"}, {"target", "opponent"}};
    auto course = lesson("installed", 1, Json::array({
        step("intro"),
        Json{{"id", "play"}, {"hint", "tutorial.test.play"}, {"blockedHint", "tutorial.test.blocked"},
             {"allow", Json::array({word})}, {"until", {{"command", word}}}}
    }));
    course["scenario"]["players"][0]["load"] = 20;
    fixture.write(course);
    bridge::LocalSession session;
    const auto storage = fixture.path / "profile";
    const auto loaded = session.initialize(assets, storage);
    INFO(loaded.dump());
    REQUIRE(loaded.at("ok") == true);
    REQUIRE(loaded.at("data").at("lessons").at(0).at("id") == "installed");
    const Json options{{"mode", "tutorial"}, {"lessonId", "installed"}};
    for (const auto *field : {"players", "seed", "chapter", "scenario", "directory"}) {
        auto forged = options;
        forged[field] = 1;
        CHECK(session.start(forged).at("ok") == false);
    }
    CHECK(session.start({{"mode", "hotseat"}, {"lessonId", "installed"}}).at("ok") == false);
    CHECK(session.start({{"mode", "tutorial"}, {"lessonId", "../installed"}}).at("ok") == false);
    REQUIRE(session.start(options).at("ok") == true);
    CHECK(session.start(options).at("ok") == false);
    auto view = session.snapshot(0).at("data");
    REQUIRE(view.at("tutorial").at("step") == "intro");
    REQUIRE(session.continueTutorial(view.at("generation"), view.at("revision")).at("ok") == true);
    view = session.snapshot(0).at("data");
    const auto rejection = session.submitCommand(0, bridge::commandDto(SetAmbush{2,1,0}),
        view.at("generation"), view.at("revision"));
    REQUIRE(rejection.at("errorCode") == "tutorial_blocked");
    CHECK(rejection.at("blockedHint") == "tutorial.test.blocked");
    // Select the legal word and target from this course's opening position.
    const auto catalog = fixture.load();
    const auto *definition = catalog.find("installed");
    REQUIRE(definition);
    const auto initial = definition->scenario.start(content().catalog);
    std::optional<Command> cast;
    initial.visitLegalActions(0, [&](const auto &action) {
        const auto *command = std::get_if<PreloadWord>(&action.command);
        if (command && command->target == playerTarget(1)) cast = action.command;
        return !cast;
    });
    REQUIRE(cast);
    const auto &command = std::get<PreloadWord>(*cast);
    const auto entry = [](ActionField field, auto value) {
        return Json{{"field", static_cast<int>(field)}, {"value", std::to_string(value)}};
    };
    Json intent{{"source", std::to_string(command.card)}, {"selections", Json::array({
        entry(ActionField::Operation, static_cast<int>(CommandKind::PreloadWord)),
        entry(ActionField::Source, command.card), entry(ActionField::Host, command.formation),
        entry(ActionField::Ring, command.ringIndex)})}};
    const auto quote = session.queryActions(0, intent, view.at("generation"), view.at("revision"));
    REQUIRE(quote.at("ok") == true);
    REQUIRE(quote.at("data").at("field") == static_cast<int>(ActionField::Target));
    const int damage = quote.at("data").at("overloadPreview");
    REQUIRE(damage > 0);
    intent["selections"].push_back(entry(ActionField::Target, command.target));
    const auto confirmation = session.queryActions(0, intent, view.at("generation"), view.at("revision"));
    REQUIRE(confirmation.at("ok") == true);
    CHECK(confirmation.at("data").at("overloadPreview") == damage);
    CHECK_FALSE(confirmation.at("data").at("costQuote").get<std::string>().empty());
    CHECK(session.snapshot(0).at("data") == view);
    auto result = session.submitCommand(0, bridge::commandDto(*cast), view.at("generation"), view.at("revision"));
    INFO(result.dump()); REQUIRE(result.at("ok") == true);
    const auto after = session.snapshot(0).at("data");
    CHECK(view.at("players")[0].at("life").get<int>() - after.at("players")[0].at("life").get<int>() == damage);
    CHECK(after.at("tutorial").at("complete") == true);
    CHECK_FALSE(std::filesystem::exists(storage / "replays"));
    const auto written = session.saveReplay("lesson");
    REQUIRE(written.at("ok") == true);
    const auto file = std::filesystem::u8path(written.at("data").at("path").get<std::string>());
    auto plan = session.readReplayPlan(file);
    REQUIRE(plan.at("ok") == true);
    CHECK(plan.at("data").at("lessonId") == "installed");
    CHECK_FALSE(plan.at("data").contains("players"));
    CHECK(session.replayFrame(file, 1, 0).at("ok") == true);
    REQUIRE(session.restart(7).at("ok") == true);
    CHECK(session.snapshot(0).at("data").at("tutorial").at("step") == "intro");
    course["order"] = 2;
    fixture.write(course);
    REQUIRE(session.initialize(assets, storage).at("ok") == true);
    const auto changed = session.verifyReplay(file);
    CHECK(changed.at("ok") == false);
    CHECK(changed.at("error") == content().renderText({"tutorial.replay.changed", {}}));
}
