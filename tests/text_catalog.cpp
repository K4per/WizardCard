#include "wizard/bridge.hpp"
#include "wizard/network.hpp"
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

TEST_CASE("native term keys render editable snapshot captions without changing legacy names", "[text-catalog]") {
    TextDirectory directory;
    const auto key = phaseTermKey(Phase::Cast);
    const auto production = loadContent(source / "assets/release-v260");
    REQUIRE(production.renderText({key, {}}) == phaseName(Phase::Cast));
    CHECK(production.renderText({schoolTermKey("evocation"), {}}) == schoolName("evocation"));
    CHECK(production.renderText({damageTermKey(DamageType::Piercing), {}}) == damageName(DamageType::Piercing));
    CHECK(production.renderText({zoneTermKey(Zone::Ash), {}}) == zoneName(Zone::Ash));
    CHECK(production.renderText({windowTermKey(ResponseWindow::Damage), {}}) == windowName(ResponseWindow::Damage));
    CHECK(production.renderText({linkTermKey(LinkKind::Setup), {}}) == linkName(LinkKind::Setup));
    CHECK(schoolTermKey("unknown").empty());

    auto terms = readJson(source / "godot/data/text/zh_CN/terms.json");
    terms[key] = "Edited caption";
    writeJson(directory.path / "zh_CN/terms.json", terms);
    Content edited;
    edited.textCatalog = std::make_shared<const TextCatalog>(TextCatalog::load(directory.path));
    GameView view;
    view.phase = Phase::Cast;
    const auto snapshot = bridge::viewDto(view, &edited);
    CHECK(snapshot.at("phaseNameKey") == key);
    CHECK(snapshot.at("phaseName") == "Edited caption");
    CHECK(bridge::viewDto(view).at("phaseName") == "施法");
}

TEST_CASE("edited term files keep every digest of the published 79-command network replay", "[text-catalog][replay]") {
    TextDirectory directory;
    auto terms = readJson(source / "godot/data/text/zh_CN/terms.json");
    for (const auto &group : {"terms.phase.", "terms.link.", "terms.window.", "terms.zone."}) {
        for (auto &[key, name] : terms.items())
            if (key.compare(0, std::string_view(group).size(), group) == 0) name = "Edited " + key;
    }
    writeJson(directory.path / "zh_CN/terms.json", terms);
    const auto record = readJson(source / "docs/reports/2.6.4-hotfix.1/network-replay.json");
    auto content = loadContent(source / "assets/release-v260");
    content.textCatalog = std::make_shared<const TextCatalog>(TextCatalog::load(directory.path));
    REQUIRE(content.catalog.contentHash == record.at("contentHash"));
    REQUIRE(record.at("commands").size() == 79);
    bool checkedOpenChain = false;
    const auto completed = replay(content, record, [&](std::size_t step, const GameEngine &engine) {
        if (step) REQUIRE(engine.digest() == record.at("commands").at(step - 1).at("digest"));
        if (checkedOpenChain || !engine.state().chain || engine.state().chain->links.empty()) return;
        const auto snapshot = bridge::viewDto(engine.viewFor(0), &content);
        const auto &chain = snapshot.at("chain");
        CHECK(snapshot.at("phaseName") == "Edited " + snapshot.at("phaseNameKey").get<std::string>());
        CHECK(chain.at("windowName") == "Edited " + chain.at("windowNameKey").get<std::string>());
        const auto &link = chain.at("links").front();
        CHECK(link.at("kindName") == "Edited " + link.at("kindNameKey").get<std::string>());
        const auto &card = snapshot.at("cards").front();
        CHECK(card.at("zoneName") == "Edited " + card.at("zoneNameKey").get<std::string>());

        // The historical public description lives on the chain, but never contributes to its state hash.
        auto state = engine.state();
        REQUIRE_FALSE(state.chain->links.front().publicAction.empty());
        state.chain->links.front().publicAction = "A different public description";
        const auto changed = GameEngine::scenario(engine.catalog(), std::move(state));
        CHECK(changed.canonicalState() == engine.canonicalState());
        CHECK(changed.digest() == engine.digest());
        checkedOpenChain = true;
    });
    REQUIRE(checkedOpenChain);
    CHECK(completed.digest() == "8e34753e7e6b6e28");
}

TEST_CASE("optional presentation references round trip alongside legacy event and option strings", "[text-catalog][network]") {
    const TextRef ref{"ui.test.label", {{"mana", 5}}};
    TextDirectory fixture;
    const auto catalog = TextCatalog::load(fixture.path);
    const auto rendered = catalog.render(ref);

    LegalAction action{rendered, 0, 0, Advance{}, ref};
    ActionOptions options;
    options.options.push_back({"1", action.label, 0, -1, action.labelRef});
    options.costQuote = rendered;
    options.costQuoteRef = ref;
    options.error = rendered;
    options.errorRef = ref;
    const auto encoded = net::encodeOptions(options);
    REQUIRE(encoded.at("options").front().at("label") == rendered);
    REQUIRE(encoded.at("options").front().at("labelRef") == encodeTextRef(ref));
    const auto decoded = net::decodeOptions(encoded);
    REQUIRE(decoded.options.front().labelRef);
    REQUIRE(decoded.costQuoteRef);
    REQUIRE(decoded.errorRef);
    CHECK(catalog.render(*decoded.options.front().labelRef, "en") == "Mana 5");
    CHECK(catalog.render(*decoded.costQuoteRef, "en") == "Mana 5");
    CHECK(catalog.render(*decoded.errorRef, "en") == "Mana 5");

    auto old = encoded;
    old["options"].front().erase("labelRef");
    old.erase("costQuoteRef");
    old.erase("errorRef");
    CHECK(net::encodeOptions(net::decodeOptions(old)) == old);
    auto malformed = encoded;
    malformed["options"].front()["labelRef"] = Json{{"unexpected", "field"}};
    CHECK_THROWS(net::decodeOptions(malformed));

    GameEvent event{"phase_enter", rendered};
    event.textRef = ref;
    EventBatch batch{1, 0, 1, true, {event}};
    auto wire = net::encodeEvents(batch);
    const auto roundTrip = net::decodeEvents(wire);
    REQUIRE(roundTrip.events.front().textRef);
    CHECK(catalog.render(*roundTrip.events.front().textRef, "en") == "Mana 5");
    CHECK(bridge::eventDto(roundTrip.events.front()).at("textRef") == encodeTextRef(ref));
    CHECK(bridge::eventDto(roundTrip.events.front()).at("text") == rendered);
    wire["events"].front().erase("textRef");
    CHECK(net::encodeEvents(net::decodeEvents(wire)) == wire);
}
