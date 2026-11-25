#include "wizard/content.hpp"
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <fstream>

using namespace wizard;

namespace {
const auto source = std::filesystem::path(WIZARD_SOURCE_DIR);

struct TextDirectory {
    std::filesystem::path path = std::filesystem::temp_directory_path() /
        ("wizard-text-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

    TextDirectory() {
        writeJson(path / "manifest.json", {
            {"defaultLocale", "zh_CN"}, {"locales", {"zh_CN", "en"}},
            {"files", {"ui.json", "terms.json"}}, {"fallbacks", Json::object()}
        });
        writeJson(path / "zh_CN/ui.json", {{"ui.test.label", "魔素 {mana}"}});
        writeJson(path / "zh_CN/terms.json", Json::object());
        writeJson(path / "en/ui.json", {{"ui.test.label", "Mana {mana}"}});
        writeJson(path / "en/terms.json", Json::object());
    }

    ~TextDirectory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};
} // namespace

TEST_CASE("text catalog renders the shared native and Godot examples", "[text-catalog]") {
    const auto path = source / "godot/tests/fixtures/text_catalog";
    const auto catalog = TextCatalog::load(path);
    const auto fixture = readJson(path / "cases.json");
    const auto cardName = [&](const std::string &id) {
        return fixture.at("cards").value(id, std::string{});
    };
    for (const auto &row : fixture.at("cases")) {
        INFO(row.at("name").get<std::string>());
        const auto textRef = Json{{"key", row.at("key")}, {"args", row.at("args")}};
        CHECK(encodeTextRef(decodeTextRef(textRef)) == textRef);
        CHECK(catalog.render(decodeTextRef(textRef), row.at("locale").get<std::string>(), cardName,
                             [](const std::string &) {}) == row.at("expected").get<std::string>());
    }
    CHECK(encodeTextRef(decodeTextRef({{"key", "ui.test.default"}})) ==
          Json{{"key", "ui.test.default"}, {"args", Json::object()}});
}

TEST_CASE("text catalog rejects invalid manifests and entries", "[text-catalog]") {
    TextDirectory directory;
    const auto manifestPath = directory.path / "manifest.json";
    const auto sourcePath = directory.path / "en/ui.json";
    const auto manifest = readJson(manifestPath);

    SECTION("manifest paths stay within a locale") {
        auto bad = manifest;
        bad["files"][0] = "../ui.json";
        writeJson(manifestPath, bad);
    }
    SECTION("fallback chains cannot cycle") {
        auto bad = manifest;
        bad["fallbacks"]["en"] = "en";
        writeJson(manifestPath, bad);
    }
    SECTION("fallback targets must be declared") {
        auto bad = manifest;
        bad["fallbacks"]["en"] = "es";
        writeJson(manifestPath, bad);
    }
    SECTION("malformed template") {
        writeJson(sourcePath, {{"ui.test.label", "Mana {mana"}});
    }
    SECTION("locale must use the same parameters as the default") {
        writeJson(sourcePath, {{"ui.test.label", "Mana {amount}"}});
    }
    SECTION("locale must not introduce keys without default text") {
        writeJson(sourcePath, {{"ui.test.other", "Mana {mana}"}});
    }
    SECTION("text key must have a namespace and lowercase segments") {
        writeJson(sourcePath, {{"ui.Test.label", "Mana {mana}"}});
    }
    SECTION("entries must be strings") {
        writeJson(sourcePath, {{"ui.test.label", 2}});
    }
    SECTION("inline key references must have default text") {
        writeJson(sourcePath, {{"ui.test.label", "{mana} {phase:terms.phase.missing}"}});
    }
    SECTION("duplicates within one JSON document") {
        std::ofstream(sourcePath) << R"({"ui.test.label":"{mana}","ui.test.label":"{mana}"})";
    }
    SECTION("duplicates across catalog files") {
        writeJson(directory.path / "en/terms.json", {{"ui.test.label", "Mana {mana}"}});
    }
    REQUIRE_THROWS(TextCatalog::load(directory.path));
}

TEST_CASE("text catalog reports unresolved references and argument names", "[text-catalog]") {
    TextDirectory directory;
    const auto catalog = TextCatalog::load(directory.path);
    std::vector<std::string> messages;
    const auto diagnostic = [&](const std::string &message) { messages.push_back(message); };

    CHECK(catalog.render({"ui.test.unknown", {}}, "en", {}, diagnostic) == "ui.test.unknown");
    REQUIRE(messages.size() == 1);
    CHECK(messages.back().find("ui.test.unknown") != std::string::npos);

    messages.clear();
    CHECK(catalog.render({"ui.test.label", {{"manna", 4}}}, "en", {}, diagnostic) == "Mana {mana}");
    REQUIRE(messages.size() == 2);
    CHECK(messages[0].find("manna") != std::string::npos);
    CHECK(messages[1].find("mana") != std::string::npos);

    messages.clear();
    CHECK(catalog.render({"ui.test.label", {{"mana", CardTextRef{"unknown-card"}}}}, "en", {}, diagnostic) ==
          "Mana unknown-card");
    REQUIRE(messages.size() == 1);
    CHECK(messages.back().find("unknown-card") != std::string::npos);
}

TEST_CASE("text catalog stops recursive inline references", "[text-catalog]") {
    TextDirectory directory;
    writeJson(directory.path / "zh_CN/terms.json", {
        {"terms.test.first", "{term:terms.test.second}"},
        {"terms.test.second", "{term:terms.test.first}"}
    });
    const auto catalog = TextCatalog::load(directory.path);
    std::vector<std::string> messages;
    const auto diagnostic = [&](const std::string &message) { messages.push_back(message); };
    CHECK(catalog.render({"terms.test.first", {}}, "zh_CN", {}, diagnostic) == "terms.test.first");
    REQUIRE(messages.size() == 1);
    CHECK(messages.back().find("terms.test.first") != std::string::npos);
}

TEST_CASE("native content reads editable catalog from the asset tree", "[text-catalog]") {
    TextDirectory directory;
    const auto assets = directory.path / "assets";
    const auto deck = assets / "release-v260";
    std::filesystem::rename(directory.path / "manifest.json", directory.path / "text-manifest.json");
    std::filesystem::create_directories(assets / "text");
    std::filesystem::rename(directory.path / "text-manifest.json", assets / "text/manifest.json");
    std::filesystem::rename(directory.path / "en", assets / "text/en");
    std::filesystem::rename(directory.path / "zh_CN", assets / "text/zh_CN");
    std::filesystem::create_directories(deck);
    const auto production = source / "assets/release-v260";
    for (const auto *file : {"cards.json", "deck.json", "side-deck.json"})
        std::filesystem::copy_file(production / file, deck / file);

    auto content = loadContent(deck);
    REQUIRE(content.textCatalog);
    CHECK(content.renderText({"ui.test.label", {{"mana", CardTextRef{"v260-word-0001"}}}}) == "魔素 火花术");
    CHECK(content.catalog.contentHash == loadContent(production).catalog.contentHash);

    writeJson(assets / "text/zh_CN/ui.json", {{"ui.test.label", "用掉 {mana} 魔素"}});
    content = loadContent(deck);
    CHECK(content.renderText({"ui.test.label", {{"mana", 3}}}) == "用掉 3 魔素");

    writeJson(assets / "text/zh_CN/ui.json", {{"ui.test.label", false}});
    REQUIRE_THROWS(loadContent(deck));
}
