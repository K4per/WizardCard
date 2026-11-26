#include "wizard/bridge.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <algorithm>

using namespace wizard;

TEST_CASE("editor lesson runs all twenty three-name choices through the real deck library", "[tutorial-courses][editor]") {
    const auto source = std::filesystem::path(WIZARD_SOURCE_DIR);
    const auto content = loadContent(source / "assets/release-v260");
    const auto directory = std::filesystem::temp_directory_path() /
        ("wizard-editor-course-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto courses = app::LessonCatalog::load(content.dataDirectory / "tutorial/lessons", content.catalog, *content.textCatalog);
    REQUIRE(courses.lessons().size() == 7);
    REQUIRE(courses.editors().size() == 1);
    const auto &editor = courses.editors().front();
    CHECK(courses.find(editor.at("id").get<std::string>()) == nullptr);
    const auto candidates = editor.at("candidates").get<std::vector<std::string>>();
    REQUIRE(candidates.size() == 6);
    app::DeckLibrary library(content.catalog, directory);
    auto draft = library.create("course");
    draft.format = 3;
    draft.cards = editor.at("cards").get<decltype(draft.cards)>();
    draft.sideCards = editor.at("sideCards").get<decltype(draft.sideCards)>();
    REQUIRE(draft.size() == 37);
    REQUIRE(draft.sideSize() == 10);
    // The selectable preset list folds this published starter into the identical default deck.
    const auto presets = readJson(source / "assets/release-v260/presets.json");
    const auto starter = std::find_if(presets.begin(), presets.end(), [](const Json &p) {
        return p.at("id") == "v260-starter";
    });
    REQUIRE(starter != presets.end());
    std::map<std::string, int> main, side;
    for (const auto &row : starter->at("cards")) main[row.at("id").get<std::string>()] += row.at("count").get<int>();
    for (const auto &row : starter->at("sideCards")) side[row.at("id").get<std::string>()] += row.at("count").get<int>();
    for (auto id : {"v260-analytic-0001", "v260-analytic-0002", "v260-analytic-0006"}) --main.at(id);
    REQUIRE(draft.cards == main);
    REQUIRE(draft.sideCards == side);

    std::string error;
    REQUIRE(library.save(draft, error));
    CHECK_FALSE(app::deckProblems(content.catalog, draft).empty());
    int combinations = 0;
    for (std::size_t a = 0; a < candidates.size(); ++a)
        for (std::size_t b = a + 1; b < candidates.size(); ++b)
            for (std::size_t c = b + 1; c < candidates.size(); ++c) {
                auto complete = draft;
                for (const auto index : {a, b, c}) ++complete.cards[candidates[index]];
                REQUIRE(app::deckProblems(content.catalog, complete).empty());
                REQUIRE(library.save(complete, error));
                app::DeckLibrary persisted(content.catalog, directory);
                REQUIRE(persisted.find(complete.id));
                CHECK(persisted.find(complete.id)->cards == complete.cards);
                auto emptySide = complete;
                emptySide.sideCards.clear();
                REQUIRE(library.save(emptySide, error));
                CHECK_FALSE(app::deckProblems(content.catalog, emptySide).empty());
                ++combinations;
            }
    CHECK(combinations == 20);
    std::filesystem::remove_all(directory);
}

TEST_CASE("editor lessons reject invalid text, candidates and competing course orders", "[tutorial-courses][editor]") {
    const auto source = std::filesystem::path(WIZARD_SOURCE_DIR);
    const auto content = loadContent(source / "assets/release-v260");
    const auto directory = std::filesystem::temp_directory_path() /
        ("wizard-editor-validation-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    const auto file = "lesson-08-deckbuilding.json";
    const auto base = readJson(content.dataDirectory / "tutorial/lessons" / file);
    const auto load = [&] {
        return app::LessonCatalog::load(directory, content.catalog, *content.textCatalog, [](const auto &) {});
    };
    for (const auto &field : {"candidate", "rank", "text", "count", "mixed"}) {
        auto bad = base;
        if (field == std::string("candidate")) bad["candidates"][0] = "v260-formation-0009";
        if (field == std::string("rank")) bad["cardRevision"] = 12;
        if (field == std::string("text")) bad["steps"]["open"]["hintKey"] = "tutorial.missing.entry";
        if (field == std::string("count")) bad["cards"]["v260-analytic-0001"] = 4;
        if (field == std::string("mixed")) bad["scenario"] = Json::object();
        writeAtomicJson(directory / file, bad);
        CHECK(load().editors().empty());
    }
    writeAtomicJson(directory / file, base);
    auto battle = readJson(content.dataDirectory / "tutorial/lessons/lesson-01-first-match.json");
    battle["order"] = base.at("order");
    writeAtomicJson(directory / "lesson-01-first-match.json", battle);
    const auto duplicated = load();
    CHECK(duplicated.editors().empty());
    CHECK(duplicated.lessons().empty());
    std::filesystem::remove_all(directory);
}
