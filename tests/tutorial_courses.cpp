#include "wizard/lesson_script.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <set>

using namespace wizard;
using namespace wizard::app;

namespace {
const auto root = std::filesystem::path(WIZARD_SOURCE_DIR);
const auto &tutorialText() {
    static const auto value = TextCatalog::load(root / "godot/data/text");
    return value;
}
const auto &content() {
    static const auto value = loadContent(root / "assets/release-v260");
    return value;
}
const LessonDefinition &course(const char *id) {
    static const auto catalog = LessonCatalog::load(root / "godot/data/tutorial/lessons", content().catalog,
                                                    tutorialText());
    INFO(id);
    const auto *found = catalog.find(id);
    REQUIRE(found);
    return *found;
}

const char *basic = "v260-formation-0009", *standard = "v260-formation-0010",
           *reinforced = "v260-formation-0011", *compound = "v260-formation-0012",
           *amplified = "v260-formation-0013", *high = "v260-formation-0016",
           *perfect = "v260-formation-0017";
const char *bolt = "v260-analytic-0006", *fire = "v260-analytic-0001", *spike = "v260-analytic-0034",
           *spark = "v260-word-0001", *shield = "v260-talisman-0003", *seal = "v260-seal-0003",
           *grease = "v260-analytic-0015", *absorb = "v260-analytic-0017", *filler = "v260-word-0021";

// Human actions go through the same lesson filter as the application. Only the
// course AI may choose opponent commands, including opening and chain responses.
struct Play {
    GameEngine engine;
    LessonScript script;
    std::vector<int> sparkTurns;
    std::set<std::string> shown;
    std::string textPrefix;
    bool skipCastWhenAdvancing{};

    Json snapshot() const { return script.snapshot(engine); }

    void recordText() {
        const auto state = snapshot();
        const auto &highlights = state.at("highlights");
        if (std::find(highlights.begin(), highlights.end(), "ui:end_phase") != highlights.end())
            CHECK(engine.validate(0, AdvancePhase{engine.state().phaseGate}).empty());
        shown.insert(state.at("hintKey").get<std::string>());
        if (state.at("params").contains("note"))
            shown.insert(state.at("params").at("note").at("key").get<std::string>());
        if (state.contains("recapHint")) shown.insert(state.at("recapHint").get<std::string>());
        if (state.contains("actionHints"))
            for (const auto &key : state.at("actionHints")) shown.insert(key.get<std::string>());
    }
    void expectText(std::initializer_list<int> numbers) const {
        for (auto number : numbers) {
            const auto key = textPrefix + (number < 10 ? "0" : "") + std::to_string(number);
            INFO(key);
            CHECK(shown.count(key) == 1);
        }
    }

    explicit Play(const char *id)
        : engine(course(id).scenario.start(content().catalog)), script(course(id)),
          textPrefix("tutorial.l" + std::string(id).substr(7, 2) + ".s") {
        recordText();
    }
    int mana() const {
        return engine.state().players[0].mana;
    }
    int life() const {
        return engine.state().players[0].life;
    }
    int load() const {
        return Rules::load(engine.state(), 0);
    }
    int turn() const {
        return engine.state().players[0].ownTurn;
    }
    CardId card(Zone zone, const char *def, int seat = 0) const {
        for (const auto &[id, c] : engine.state().cards)
            if (c.owner == seat && c.zone == zone && c.definition == def)
                return id;
        FAIL("Missing card " << def << " in zone " << static_cast<int>(zone));
        return 0;
    }
    void send(int seat, const Command &c) {
        auto before = engine.viewFor(seat);
        CAPTURE(snapshot(), seat, before.globalTurn, static_cast<int>(before.phase), c.index());
        if (seat == 0)
            REQUIRE(script.allows(before, c));
        if (seat == 1 && std::holds_alternative<PreloadWord>(c))
            sparkTurns.push_back(before.players[1].ownTurn);
        const auto result = engine.submit(seat, c);
        INFO(result.error);
        REQUIRE(result.accepted);
        script.observe(seat, c, before, engine.viewFor(seat), result.events);
        recordText();
    }
    void reading() {
        for (int i = 0; i < 64 && script.canContinue(engine.viewFor(0)); ++i) {
            REQUIRE(script.continueLesson(engine.viewFor(0)));
            recordText();
        }
    }
    // Advance the ordinary phase gate, AI, or pending mandatory choice. Never
    // synthesize spell damage, player resources, or the result of a decision.
    void tick() {
        reading();
        const auto &s = engine.state();
        REQUIRE(s.result == -1);
        const auto seat = s.decision ? s.decision->player : s.active;
        CAPTURE(snapshot(), seat, s.globalTurn, static_cast<int>(s.phase));
        if (seat == 1) {
            const auto choice = script.opponent(
                engine.viewFor(1), [&](const auto &visit) { engine.visitLegalActions(1, visit); });
            REQUIRE(choice);
            send(1, choice->command);
        } else if (s.decision && s.decision->kind == DecisionKind::Response) {
            send(0, PassResponse{s.decision->id});
        } else if (s.decision && s.decision->kind != DecisionKind::CastOrder) {
            const auto &d = *s.decision;
            auto option = d.mayPass ? ChoiceValue{} : d.options.front();
            if (d.kind == DecisionKind::Discard)
                for (const auto &o : d.options)
                    if (o.kind == ChoiceKind::Card &&
                        s.cards.at(static_cast<CardId>(o.value)).definition == filler) {
                        option = o;
                        break;
                    }
            send(0, Choose{d.id, option});
        } else {
            const auto lesson = snapshot();
            const auto highlights = lesson.at("highlights");
            INFO(lesson.dump());
            CHECK(std::find(highlights.begin(), highlights.end(), "ui:end_phase") != highlights.end());
            const auto hint = tutorialText().render(decodeTextRef({{"key", lesson.at("hintKey")},
                                                                  {"args", lesson.at("params")}}));
            CHECK(hint.find("【切换阶段】") != std::string::npos);
            send(0, AdvancePhase{s.phaseGate, skipCastWhenAdvancing && s.phase == Phase::Main});
        }
    }
    template <class Predicate> void until(Predicate condition) {
        for (int i = 0; i < 140; ++i) {
            reading();
            if (condition())
                return;
            tick();
        }
        FAIL("Unreached state at " << snapshot().dump());
    }
    void phase(int ownTurn, Phase want) {
        until([&] {
            const auto &s = engine.state();
            return s.active == 0 && turn() == ownTurn && s.phase == want && !s.opening &&
                   (!s.decision || s.decision->kind == DecisionKind::CastOrder);
        });
    }
    void settled() {
        until([&] {
            const auto &s = engine.state();
            return s.result != -1 ||
                   (script.complete() || !s.decision || s.decision->kind == DecisionKind::CastOrder);
        });
    }
    void action(const Command &c) {
        reading();
        send(0, c);
        settled();
    }
    void place(const char *def, const char *host) {
        const auto h = card(Zone::Analysis, host);
        action(StartAnalysis{
            card(Zone::Hand, def), h, 0, {}, Rules::freeRing(engine.state(), engine.catalog(), h)});
    }
    void ambush(const char *def, const char *host) {
        const auto h = card(Zone::Analysis, host);
        action(SetAmbush{card(Zone::Hand, def), h, Rules::freeRing(engine.state(), engine.catalog(), h)});
    }
    void prepare(const char *def) {
        action(PrepareCast{card(Zone::Analysis, def)});
    }
    void activate(const char *def) {
        for (const auto &[id, c] : engine.state().cards) {
            if (c.owner == 0 && c.zone == Zone::Casting && c.spell == SpellState::Ready &&
                c.definition == def) {
                action(ActivateSpell{id, playerTarget(1)});
                return;
            }
        }
        FAIL("No prepared spell " << def);
    }
    void finish(bool win) {
        until([&] { return engine.state().result != -1; });
        CHECK(engine.state().result == (win ? 0 : 1));
        CHECK(script.complete() == win);
    }
};
} // namespace

TEST_CASE("first match opens with real gestures, explorable hand and two scheduled Sparks",
          "[tutorial-courses]") {
    Play p("lesson-01-first-match");
    p.reading();
    p.send(0, SubmitOpeningGesture{1, 1});
    p.until([&] { return p.engine.state().openingStage == OpeningStage::Formation; });
    CHECK(p.engine.state().first == 0);
    CHECK(p.engine.viewFor(0).players[0].handCount == 5);
    REQUIRE(p.card(Zone::Hand, bolt));
    p.action(SelectInitialFormation{p.card(Zone::SideDeck, basic)});
    p.phase(1, Phase::Main);
    CHECK(p.mana() == 1);
    p.phase(2, Phase::Main);
    p.action(SetFormation{p.card(Zone::SideDeck, standard), 0, {p.card(Zone::Analysis, basic)}});
    p.phase(3, Phase::Main);
    CHECK(p.life() == 2);
    CHECK(p.sparkTurns == std::vector<int>{2});
    SECTION("face-up Bolt wins on the learner's fourth turn") {
        p.place(bolt, standard);
        p.phase(4, Phase::Main);
        p.prepare(bolt);
        CHECK(p.snapshot().at("step") == "enter-cast");
        CHECK(p.snapshot().at("highlights") == Json::array({"ui:end_phase"}));
        p.phase(4, Phase::Cast);
        CHECK(p.snapshot().at("hintKey") == "tutorial.l01.s14");
        p.activate(bolt);
        p.finish(true);
        p.expectText({1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 13, 14});
    }
    SECTION("abandoning the Bolt leaves the second Spark lethal") {
        p.place(bolt, standard);
        p.action(Abandon{p.card(Zone::Analysis, bolt)});
        p.finish(false);
        CHECK(p.sparkTurns == std::vector<int>{2, 4});
    }
    SECTION("a concealed Bolt never progresses on its own") {
        p.ambush(bolt, standard);
        p.expectText({10});
        p.phase(4, Phase::Main);
        const auto spell = p.card(Zone::Analysis, bolt);
        CHECK(p.engine.state().cards.at(spell).analysisProgress == 0);
        p.action(FlipAmbush{spell});
        CHECK(p.engine.state().cards.at(spell).analysisProgress == 0);
        p.finish(false);
    }
}

TEST_CASE("face-up and ambush routes finish on different turns and both survive", "[tutorial-courses]") {
    for (bool hidden : {false, true}) {
        CAPTURE(hidden);
        Play p("lesson-02-set-ambush");
        p.reading();
        const auto options = p.snapshot().at("actionHints");
        CHECK(options == Json{{"StartAnalysis", "tutorial.l02.s02"}, {"SetAmbush", "tutorial.l02.s02.ambush"}});
        const auto spell = p.card(Zone::Hand, bolt);
        if (hidden) {
            p.ambush(bolt, reinforced);
            CHECK(p.mana() == 4);
            p.phase(2, Phase::Prepare);
            CHECK(p.engine.state().cards.at(spell).analysisProgress == 0);
            p.phase(3, Phase::Main);
            CHECK(p.life() == 5);
            const auto balance = p.mana();
            p.action(FlipAmbush{spell});
            CHECK(p.mana() == balance - 2);
            CHECK(p.engine.state().cards.at(spell).analysisProgress == 0);
        } else {
            p.place(bolt, reinforced);
            CHECK(p.mana() == 2);
        }
        int last = hidden ? 4 : 2;
        p.phase(last, Phase::Main);
        REQUIRE(p.engine.state().cards.at(spell).analysisProgress == 1);
        p.prepare(bolt);
        p.phase(last, Phase::Cast);
        p.activate(bolt);
        p.finish(true);
        if (hidden)
            p.expectText({1, 2, 4, 5, 6, 7});
        else
            p.expectText({1, 2, 3, 7});
        CHECK(p.life() == (hidden ? 2 : 8));
        CHECK(p.mana() == (hidden ? 10 : 4));
        CHECK(p.sparkTurns == (hidden ? std::vector<int>{2, 3} : std::vector<int>{}));
    }
}

TEST_CASE("all seven formation levels consume their actual recipes over eight turns", "[tutorial-courses]") {
    Play p("lesson-03-formations");
    p.skipCastWhenAdvancing = GENERATE(false, true);
    CAPTURE(p.skipCastWhenAdvancing);
    std::vector<int> balances;
    auto upgrade = [&](const char *name, std::vector<CardId> materials) {
        const auto highlights = p.snapshot().at("highlights");
        CHECK(std::find(highlights.begin(), highlights.end(), std::string("card:side_deck.") + name) != highlights.end());
        CHECK(std::find(highlights.begin(), highlights.end(), "ui:end_phase") == highlights.end());
        // The recipe accepts selected materials independently of selection order.
        std::reverse(materials.begin(), materials.end());
        p.action(SetFormation{p.card(Zone::SideDeck, name), 0, materials});
        for (auto id : materials)
            CHECK(p.engine.state().cards.at(id).zone != Zone::Analysis);
    };
    p.phase(1, Phase::Main);
    CHECK(p.mana() == 2);
    upgrade(standard, {p.card(Zone::Analysis, basic)});
    balances.push_back(p.mana());
    CHECK_FALSE(
        p.engine.validate(0, SetFormation{p.card(Zone::SideDeck, basic), 0, {p.card(Zone::Hand, filler)}})
            .empty());
    p.phase(2, Phase::Main);
    upgrade(basic, {p.card(Zone::Hand, filler)});
    balances.push_back(p.mana());
    p.phase(3, Phase::Main);
    p.ambush(filler, standard);
    upgrade(reinforced, {p.card(Zone::Analysis, standard), p.card(Zone::Analysis, filler)});
    balances.push_back(p.mana());
    p.phase(4, Phase::Main);
    upgrade(compound, {p.card(Zone::Analysis, basic), p.card(Zone::Analysis, reinforced)});
    balances.push_back(p.mana());
    p.phase(5, Phase::Main);
    p.place(grease, compound);
    balances.push_back(p.mana());
    p.phase(6, Phase::Main);
    upgrade(amplified, {p.card(Zone::Analysis, compound), p.card(Zone::Analysis, grease)});
    p.place(absorb, amplified);
    balances.push_back(p.mana());
    p.phase(7, Phase::Main);
    upgrade(high, {p.card(Zone::Analysis, amplified), p.card(Zone::Analysis, absorb)});
    const auto first = p.skipCastWhenAdvancing ? grease : absorb;
    const auto second = p.skipCastWhenAdvancing ? absorb : grease;
    p.place(first, high);
    CHECK(p.snapshot().at("highlights") == Json::array({std::string("card:hand.") + second}));
    p.place(second, high);
    balances.push_back(p.mana());
    p.phase(8, Phase::Main);
    upgrade(perfect,
            {p.card(Zone::Analysis, high), p.card(Zone::Analysis, absorb), p.card(Zone::Analysis, grease)});
    balances.push_back(p.mana());
    CHECK(balances == std::vector<int>{0, 2, 5, 9, 10, 9, 4, 4});
    CHECK(p.engine.state().result == -1);
    CHECK(p.script.complete());
    REQUIRE(p.card(Zone::Analysis, perfect));
    p.expectText({1, 2, 3, 4, 5, 6, 7, 8});
}

TEST_CASE("spell choice permits slow losses and a free shield followed by a winning Spark",
          "[tutorial-courses]") {
    Play p("lesson-04-spells");
    bool win = false;
    auto playSpark = [&] {
        const auto host = p.card(Zone::Analysis, standard);
        p.action(PreloadWord{p.card(Zone::Hand, spark),
                             true,
                             host,
                             playerTarget(1),
                             {},
                             0,
                             0,
                             Rules::freeRing(p.engine.state(), p.engine.catalog(), host)});
    };
    SECTION("word wins immediately") {
        win = true;
        playSpark();
    }
    SECTION("the analytic is too slow") {
        p.place(bolt, standard);
    }
    SECTION("shield alone with a passed response loses") {
        p.ambush(shield, standard);
        CHECK(p.mana() == 2);
    }
    SECTION("concealing the shield leaves enough mana to win") {
        p.ambush(shield, standard);
        CHECK(p.mana() == 2);
        win = true;
        playSpark();
    }
    p.finish(win);
    p.expectText({1, 2, 3, 4, 5});
    if (win) p.expectText({6, 7});
    CHECK(p.sparkTurns.size() == (win ? 0 : 1));
}

TEST_CASE("attaching the rune raises the host rank and lowers next turn's income", "[tutorial-courses]") {
    Play p("lesson-05-runes");
    auto host = p.card(Zone::Analysis, standard), rune = p.card(Zone::Hand, seal);
    const auto fireball = p.card(Zone::Hand, fire);
    REQUIRE_FALSE(p.engine.validate(0, StartAnalysis{fireball, host, 0, {}, 0}).empty());
    p.action(AttachSeal{rune, host});
    CHECK(p.mana() == 3);
    CHECK(p.engine.state().cards.at(rune).host == host);
    CHECK(Rules::maxRank(p.engine.state(), p.engine.catalog(), host) == 3);
    CHECK(Rules::rings(p.engine.state(), p.engine.catalog(), host) == 3);
    p.place(fire, standard);
    CHECK(p.mana() == 0);
    CHECK(p.load() == 3);
    p.phase(2, Phase::Main);
    CHECK(p.mana() == 1);
    p.prepare(fire);
    p.phase(2, Phase::Cast);
    p.activate(fire);
    p.finish(true);
    CHECK(p.mana() == 0);
    CHECK(p.load() == 4);
    p.expectText({1, 2, 3, 4, 5, 6});
}

TEST_CASE("load lesson admits every four and five spell order and all shortfall choices",
          "[tutorial-courses]") {
    auto def = [](char c) { return c == 'S' ? spike : c == 'F' ? fire : bolt; };
    std::set<std::string> allRoutes;
    for (const std::string selection : {"BFFS", "BBFFS", "BBS", "BBF", "BFF", "BFS", "FFS"}) {
        CAPTURE(selection);
        Play base("lesson-06-load");
        for (auto c : selection)
            base.place(def(c), amplified);
        if (selection == "BFFS")
            CHECK(base.load() == 11);
        if (selection == "BBFFS")
            CHECK(base.load() == 13);
        const auto first = base.card(Zone::Analysis, def(selection[0]));
        CHECK_FALSE(base.engine.validate(0, PrepareCast{first}).empty());
        base.phase(2, Phase::Draw);
        CHECK(base.engine.state().cards.at(first).analysisProgress == 0);
        base.phase(2, Phase::Prepare);
        CHECK(base.engine.state().cards.at(first).analysisProgress == 1);
        base.phase(2, Phase::Main);
        for (auto c : selection)
            base.prepare(def(c));
        base.phase(2, Phase::Cast);
        std::string order = selection;
        int clean = 0, early = 0, late = 0;
        do {
            Play p = base;
            CAPTURE(order);
            int casts = 0;
            for (char c : order) {
                p.activate(def(c));
                ++casts;
                if (p.engine.state().result != -1)
                    break;
            }
            allRoutes.insert(p.shown.begin(), p.shown.end());
            if (selection.size() == 3) {
                int damage = 0;
                for (char c : selection)
                    damage += c == 'S' ? 10 : c == 'F' ? 7 : 5;
                CHECK(p.engine.state().players[1].life == 29 - damage);
                CHECK_FALSE(p.script.complete());
                if (selection == "BBS") {
                    p.phase(3, Phase::Main);
                    p.place(fire, amplified);
                    p.place(fire, amplified);
                    p.send(0, AdvancePhase{p.engine.state().phaseGate, true});
                    p.phase(4, Phase::Main);
                    p.prepare(fire);
                    p.prepare(fire);
                    p.phase(4, Phase::Cast);
                    p.activate(fire);
                    p.activate(fire);
                    p.finish(true);
                }
                break;
            }
            p.finish(true);
            CHECK(p.snapshot().at("hintKey") ==
                  (selection.size() == 4 ? "tutorial.l06.s08" : "tutorial.l06.s10"));
            CHECK(p.snapshot().at("recapHint") == "tutorial.l06.s09");
            if (selection.size() == 4) {
                CHECK(p.load() == 17);
                CHECK(p.life() == 10);
                ++clean;
            } else if (casts == 4) {
                CHECK(p.load() == 19);
                CHECK(p.life() == 6);
                ++early;
            } else {
                CHECK(p.load() == 20);
                CHECK(p.life() == 4);
                ++late;
            }
        } while (std::next_permutation(order.begin(), order.end()));
        if (selection.size() == 4)
            CHECK(clean == 12);
        if (selection.size() == 5) {
            CHECK(early == 12);
            CHECK(late == 18);
        }
    }
    for (int i = 1; i <= 10; ++i) {
        const auto key = std::string("tutorial.l06.s") + (i < 10 ? "0" : "") + std::to_string(i);
        INFO(key);
        CHECK(allRoutes.count(key) == 1);
    }
}

TEST_CASE("the real response window accepts a chained shield and permits a lethal pass",
          "[tutorial-courses]") {
    Play p("lesson-07-chain");
    CHECK(p.mana() == 4);
    p.until([&] {
        auto d = p.engine.viewFor(0).decision;
        return d && d->kind == DecisionKind::Response && p.snapshot().at("step") == "respond";
    });
    CHECK(p.snapshot().at("hintSurface") == "response");
    const auto shieldId = p.card(Zone::Analysis, shield);
    REQUIRE(p.engine.state().cards.at(shieldId).faceDown);
    REQUIRE(p.engine.state().cards.at(shieldId).host == p.card(Zone::Analysis, compound));
    SECTION("decline the shield") {
        p.send(0, PassResponse{p.engine.state().decision->id});
        p.finish(false);
        CHECK(p.snapshot().at("hintKey") == "tutorial.l07.s07");
        p.expectText({1, 2, 3, 7, 9});
    }
    SECTION("chain shield then counter with Bolt") {
        std::optional<Command> flip;
        p.engine.visitLegalActions(0, [&](const auto &a) {
            if (auto c = std::get_if<FlipAmbush>(&a.command); c && c->card == shieldId)
                flip = *c;
            return !flip;
        });
        REQUIRE(flip);
        p.action(*flip);
        CHECK(p.life() == 1);
        CHECK(p.mana() == 1);
        p.phase(2, Phase::Main);
        CHECK(p.mana() == 5);
        p.place(bolt, compound);
        CHECK(p.mana() == 3);
        p.phase(3, Phase::Main);
        CHECK(p.mana() == 7);
        p.prepare(bolt);
        p.phase(3, Phase::Cast);
        p.activate(bolt);
        p.finish(true);
        CHECK(p.mana() == 6);
        CHECK(p.snapshot().at("hintKey") == "tutorial.l07.s08");
        p.expectText({1, 2, 3, 4, 5, 6, 8, 9});
    }
}
