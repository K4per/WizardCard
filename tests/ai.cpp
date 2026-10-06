#include "wizard/application.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <chrono>
#include <set>
using namespace wizard;
using namespace wizard::app;
namespace {
Content content() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
}
CardView shown(CardId id, const char *name, int owner = 0, Zone zone = Zone::Hand) {
    CardView c;
    c.instance.id = id;
    c.instance.definition = name;
    c.instance.owner = owner;
    c.instance.zone = zone;
    c.definition = content().catalog.at(name);
    return c;
}
GameView view() {
    GameView v;
    v.result = -1;
    v.viewer = v.active = 0;
    v.phase = Phase::Main;
    for (auto &p : v.players) {
        p.life = 20;
        p.mana = 10;
        p.capacity = 20;
        p.deckCount = 20;
        p.handCount = 5;
    }
    return v;
}
bool same(const Command &a, const Command &b) {
    return encodeCommand(a) == encodeCommand(b);
}
struct Fixture {
    Content data = content();
    std::filesystem::path directory =
        std::filesystem::path(WIZARD_SOURCE_DIR) / "build" / "ai-tests" /
        std::to_string(std::chrono::steady_clock::now().time_since_epoch().count());
    ~Fixture() {
        std::error_code e;
        std::filesystem::remove_all(directory, e);
    }
    Application app() {
        return Application(data, directory,
                           loadPresets(data, std::filesystem::path(WIZARD_SOURCE_DIR) / "assets"));
    }
};
Command student(const Application &a) {
    auto v = a.match().engine().viewFor(0);
    const auto *t = a.tutorial();
    if (t) {
        std::vector<LegalAction> available;
        for (const auto &action : v.actions)
            if (t->allows(v, action.command))
                available.push_back(action);
        v.actions = std::move(available);
    }
    const auto choice = ai::choose(v, ai::Difficulty::Normal);
    REQUIRE(choice);
    return choice->command;
}
} // namespace
TEST_CASE("uncommon rarity uses the same Chinese label in details and filters") {
    REQUIRE(rarityLabel("uncommon") == "罕见");
    REQUIRE(rarityLabel("common") == "普通");
    REQUIRE(rarityLabel("rare") == "稀有");
    REQUIRE(rarityLabel("epic") == "史诗");
    REQUIRE(rarityLabel("legendary") == "传说");
    REQUIRE(rarityLabel("future") == "future");
}
TEST_CASE("normal AI takes immediate lethal damage instead of a future analysis") {
    auto v = view();
    v.players[1].life = 2;
    v.cards = {shown(1, "fireball"), shown(2, "spark")};
    v.actions = {{"解析", 1, 3, StartAnalysis{1, 3}},
                 {"立即释放", 2, 0, PreloadWord{2,true}},
                 {"结束", 0, 0, AdvancePhase{9}}};
    const auto selected = ai::choose(v, ai::Difficulty::Normal);
    REQUIRE(selected);
    REQUIRE(std::holds_alternative<PreloadWord>(selected->command));
}
TEST_CASE("hard AI orders a cheap preparation before a valuable spell against a public counter") {
    auto v = view();
    auto fire = shown(1, "fireball", 0, Zone::Analysis);
    fire.instance.spell = SpellState::Ready;
    auto ward = shown(2, "ward", 0, Zone::Analysis);
    ward.instance.spell = SpellState::Ready;
    auto counter = shown(3, "barbs", 1, Zone::Words);
    v.cards = {fire, ward, counter};
    v.actions = {
        {"火球", 1, 0, PrepareCast{1}}, {"结界", 2, 0, PrepareCast{2}}, {"结束", 0, 0, AdvancePhase{9}}};
    REQUIRE(std::get<PrepareCast>(ai::choose(v, ai::Difficulty::Normal)->command).card == 1);
    REQUIRE(std::get<PrepareCast>(ai::choose(v, ai::Difficulty::Hard)->command).card == 2);
    REQUIRE(std::get<PrepareCast>(ai::choose(v, ai::Difficulty::Easy)->command).card == 1);
    v.cards.pop_back();
    REQUIRE(std::get<PrepareCast>(ai::choose(v, ai::Difficulty::Hard)->command).card == 1);
}
TEST_CASE("AI skips fatal self load and drawing beyond the remaining deck") {
    auto v = view();
    v.players[0].deckCount = 1;
    v.players[0].load = v.players[0].capacity = 8;
    v.cards = {shown(1, "recall"), shown(2, "mend", 0, Zone::Analysis)};
    v.actions = {
        {"抽牌", 1, 0, PlayAction{1}}, {"治疗", 2, 0, PrepareCast{2}}, {"结束", 0, 0, AdvancePhase{3}}};
    for (const auto d : {ai::Difficulty::Easy, ai::Difficulty::Normal, ai::Difficulty::Hard})
        REQUIRE(std::holds_alternative<AdvancePhase>(ai::choose(v, d)->command));
}
TEST_CASE("AI discards inexpensive cards and can stop optional self destruction") {
    auto v = view();
    v.cards = {shown(1, "fireball"), shown(2, "spark")};
    v.decision = PendingDecision{7, 0, DecisionKind::Discard, {1, 2}, false};
    v.actions = {{"弃置", 1, 0, Choose{7, 1}}, {"弃置", 2, 0, Choose{7, 2}}};
    REQUIRE(std::get<Choose>(ai::choose(v, ai::Difficulty::Normal)->command).option == 2);
    v.decision = PendingDecision{8, 0, DecisionKind::DestroyOwnFormation, {3}, true};
    v.cards.push_back(shown(3, "reservoir", 0, Zone::Analysis));
    v.actions = {{"销毁", 3, 0, Choose{8, 3}}, {"跳过", 0, 0, Choose{8, 0}}};
    REQUIRE(std::get<Choose>(ai::choose(v, ai::Difficulty::Normal)->command).option == 0);
}
TEST_CASE("AI clears available temporary load and declines unsafe concentration") {
    auto v = view();
    v.decision = PendingDecision{5, 0, DecisionKind::ClearLoad, {12, 13}, true};
    v.temporary = {{12, 0, 1, 1, 2, false}, {13, 0, 2, 3, 2, false}, {14, 0, 3, 5, 0, true}};
    v.actions = {{"清理", 0, 0, Choose{5, 12}}, {"清理", 0, 0, Choose{5, 13}}, {"停止", 0, 0, Choose{5, 0}}};
    REQUIRE(std::get<Choose>(ai::choose(v, ai::Difficulty::Hard)->command).option == 13);
    auto ward = shown(1, "ward", 0, Zone::Casting);
    ward.instance.concentrationCost = 1;
    v.cards = {ward};
    v.players[0].load = v.players[0].capacity;
    v.decision = PendingDecision{6, 0, DecisionKind::Concentration, {1}, true};
    v.actions = {{"维持", 1, 0, Choose{6, 1}}, {"销毁", 0, 0, Choose{6, 0}}};
    REQUIRE(std::get<Choose>(ai::choose(v, ai::Difficulty::Hard)->command).option == 0);
}
TEST_CASE("hard AI can prioritize a mana trigger before damage without hidden queue access") {
    auto v = view();
    v.decision = PendingDecision{3, 0, DecisionKind::TriggerOrder, {11, 12}, false};
    Trigger damage;
    damage.id = 11;
    damage.owner = 0;
    damage.target = TargetKind::Opponent;
    damage.effects = {{EffectKind::Damage, 4}};
    Trigger mana;
    mana.id = 12;
    mana.owner = 0;
    mana.effects = {{EffectKind::GainMana, 2}};
    v.triggers = {damage, mana};
    v.actions = {{"伤害", 0, 0, Choose{3, 11}}, {"魔素", 0, 0, Choose{3, 12}}};
    REQUIRE(std::get<Choose>(ai::choose(v, ai::Difficulty::Normal)->command).option == 11);
    REQUIRE(std::get<Choose>(ai::choose(v, ai::Difficulty::Hard)->command).option == 12);
}
TEST_CASE("AI counters a live enemy link and passes once that link is already cancelled") {
    auto v = view();
    auto word = shown(2, "barbs", 0, Zone::Words);
    v.cards = {word};
    v.decision = PendingDecision{3, 0, DecisionKind::Response, {2}, true};
    ChainState chain;
    chain.window = ResponseWindow::Prepare;
    chain.priority = 0;
    ChainLink root;
    root.id = 9;
    root.kind = LinkKind::Preparation;
    root.item.owner = 1;
    root.item.target = TargetKind::Opponent;
    root.item.effects = {{EffectKind::Damage, 10}};
    chain.links = {root};
    v.chain = chain;
    v.actions = {{"反制", 2, 0, Respond{3, 2, "cancel_preparation", 0, 9}}, {"放弃", 0, 0, PassResponse{3}}};
    REQUIRE(std::holds_alternative<Respond>(ai::choose(v, ai::Difficulty::Normal)->command));
    v.chain->links[0].canceled = true;
    REQUIRE(std::holds_alternative<PassResponse>(ai::choose(v, ai::Difficulty::Normal)->command));
}
TEST_CASE("AI budgets are finite preserve a pass and return no action on terminal or empty views") {
    auto v = view();
    v.cards = {shown(1, "reservoir", 0, Zone::Analysis)};
    v.actions = {{"拆除", 1, 0, RemoveFormation{1}}, {"结束", 0, 0, AdvancePhase{3}}};
    const auto selected = ai::choose(v, ai::Difficulty::Hard, 1);
    REQUIRE(selected);
    REQUIRE(selected->evaluated == 1);
    REQUIRE(std::holds_alternative<AdvancePhase>(selected->command));
    REQUIRE_FALSE(ai::choose(v, ai::Difficulty::Hard, 0));
    v.result = 0;
    REQUIRE_FALSE(ai::choose(v, ai::Difficulty::Normal));
    v.result = -1;
    v.actions.clear();
    REQUIRE_FALSE(ai::choose(v, ai::Difficulty::Normal));
}
TEST_CASE(
    "every forced decision has a deterministic projected choice including effect preparation and overflow") {
    for (int kind = 0; kind < 10; ++kind) {
        auto v = view();
        v.cards = {shown(1, "fireball", 0, Zone::Analysis)};
        v.decision = PendingDecision{9, 0, static_cast<DecisionKind>(kind), {1}, false};
        v.actions = {{"选择", 1, 0, Choose{9, 1}}};
        if (v.decision->kind == DecisionKind::EffectPrepare)
            v.actions = {{"准备", 1, 0, PrepareCast{1, 0, 0, {}, 9}}};
        for (auto d : {ai::Difficulty::Easy, ai::Difficulty::Normal, ai::Difficulty::Hard}) {
            const auto first = ai::choose(v, d), second = ai::choose(v, d);
            REQUIRE(first);
            REQUIRE(second);
            REQUIRE(same(first->command, second->command));
            REQUIRE(same(first->command, v.actions.front().command));
        }
    }
}
TEST_CASE("AI choices cannot inspect a different enemy hand or shuffled deck with the same projection") {
    auto c = content();
    MatchConfig config;
    for (auto &p : config.players)
        p.cards = c.deck;
    GameEngine e(c.catalog, config);
    while (e.state().phase != Phase::Main) {
        const auto &state = e.state();
        const int actor = state.decision ? state.decision->player : state.active;
        auto action = ai::choose(e.viewFor(actor), ai::Difficulty::Normal);
        REQUIRE(action);
        REQUIRE(e.submit(actor, action->command).accepted);
    }
    auto s = e.state();
    const int actor = s.decision ? s.decision->player : s.active;
    for (auto &[id, card] : s.cards)
        if (card.owner != actor && (card.zone == Zone::Hand || card.zone == Zone::Deck))
            card.definition = "mend";
    auto other = GameEngine::scenario(c.catalog, s);
    REQUIRE(e.digest() != other.digest());
    auto v = e.viewFor(actor), w = other.viewFor(actor);
    REQUIRE(
        std::any_of(v.actions.begin(), v.actions.end(), [](const LegalAction &a) { return a.source != 0; }));
    for (auto d : {ai::Difficulty::Easy, ai::Difficulty::Normal, ai::Difficulty::Hard}) {
        auto a = ai::choose(v, d), b = ai::choose(w, d);
        REQUIRE(a);
        REQUIRE(b);
        REQUIRE(same(a->command, b->command));
        REQUIRE(a->score == b->score);
    }
}
TEST_CASE("trigger choices expose only the deciding player's visible source snapshots") {
    auto data = content();
    MatchConfig config;
    for (auto &p : config.players)
        p.cards = data.deck;
    GameEngine engine(data.catalog, config);
    auto state = engine.state();
    CardId privateSource = 0;
    for (const auto &[id, c] : state.cards)
        if (c.owner == 1 && c.zone == Zone::Hand)
            privateSource = id;
    REQUIRE(privateSource);
    state.queue.items.clear();
    Trigger publicTrigger;
    publicTrigger.id = 900;
    publicTrigger.owner = 0;
    publicTrigger.source = 1;
    publicTrigger.effects = {{EffectKind::GainMana, 2}};
    Trigger otherTrigger;
    otherTrigger.id = 901;
    otherTrigger.owner = 1;
    otherTrigger.source = privateSource;
    otherTrigger.effects = {{EffectKind::Draw, 1}};
    state.queue.append(publicTrigger);
    state.queue.append(otherTrigger);
    state.decision = PendingDecision{999, 0, DecisionKind::TriggerOrder, {900}, false};
    auto scenario = GameEngine::scenario(data.catalog, state);
    auto v = scenario.viewFor(0);
    REQUIRE(v.triggers.size() == 1);
    REQUIRE(v.triggers[0].id == 900);
    REQUIRE(scenario.viewFor(1).triggers.empty());
}
TEST_CASE("AI can pay a preloaded response discard and counter an opposing counter through the normal core") {
    auto data = content();
    auto spell = data.catalog.cards.at("spark");
    spell.id = "ai-test-counter";
    spell.speed = 3; spell.immediate = false; spell.rank = 1;
    ResponseAbility ability;
    ability.id = "teaching-test-counter";
    ability.fromWords = true;
    ability.preloadedCost = 1;
    ability.extraDiscard = 1;
    ability.target = TargetKind::PendingLink;
    ability.windows = {ResponseWindow::Prepare};
    ability.effects = {{EffectKind::NegateLink, 0}};
    spell.responses = {ability};
    data.catalog.cards.emplace(spell.id, spell);
    MatchConfig config;
    for (auto &p : config.players)
        p.cards = data.deck;
    GameEngine initial(data.catalog, config);
    auto state = initial.state();
    state.phase = Phase::Main;
    state.phaseStep = PhaseStep::Body;
    state.flow = Flow::Main;
    state.active = 0;
    for (auto &p : state.players) {
        p.mana = 10;
        p.ownTurn = 2;
    }
    CardId fire = 0, word = 0, counter = 0, discard = 0;
    for (auto &[id, c] : state.cards) {
        if (c.owner == 0 && c.zone == Zone::Hand && !fire) {
            c.definition = "fireball";
            c.zone = Zone::Analysis;
            c.spell = SpellState::Ready;
            c.host = c.sourceFormation = 1;
            c.analysisLoad = 3;
            fire = id;
        } else if (c.owner == 0 && c.zone == Zone::Hand && !counter) {
            c.definition = "ai-test-counter";
            c.zone = Zone::Words;
            counter = id;
        } else if (c.owner == 0 && c.zone == Zone::Hand && !discard) {
            c.definition = "reservoir";
            discard = id;
        }
        if (c.owner == 1 && c.zone == Zone::Hand && !word) {
            c.definition = "barbs";
            c.zone = Zone::Words;
            c.analysisLoad = 5;
            word = id;
        }
    }
    REQUIRE(fire);
    REQUIRE(counter);
    REQUIRE(discard);
    REQUIRE(word);
    auto engine = GameEngine::scenario(data.catalog, state);
    REQUIRE(engine.submit(0, PrepareCast{fire}).accepted);
    auto first = ai::choose(engine.viewFor(1), ai::Difficulty::Hard);
    REQUIRE(first);
    REQUIRE(std::holds_alternative<Respond>(first->command));
    REQUIRE(engine.submit(1, first->command).accepted);
    auto second = ai::choose(engine.viewFor(0), ai::Difficulty::Hard);
    REQUIRE(second);
    REQUIRE(std::holds_alternative<Respond>(second->command));
    auto response = std::get<Respond>(second->command);
    REQUIRE(response.card == counter);
    REQUIRE(response.discard != counter);
    REQUIRE(response.discard != 0);
    REQUIRE(engine.submit(0, response).accepted);
    for (int n = 0; engine.state().chain; ++n) {
        REQUIRE(n < 20);
        const auto &s = engine.state();
        auto actor = s.decision ? s.decision->player : s.active;
        auto action = ai::choose(engine.viewFor(actor), ai::Difficulty::Hard);
        REQUIRE(action);
        REQUIRE(engine.submit(actor, action->command).accepted);
    }
    REQUIRE(engine.state().cards.at(fire).zone == Zone::Casting);
    REQUIRE(engine.state().cards.at(response.discard).zone == Zone::Ash);
    REQUIRE(engine.state().cards.at(counter).zone == Zone::Ash);
}
TEST_CASE("all difficulties finish fixed seed games with representative decks and deterministic replay") {
    auto c = content();
    auto presets = loadPresets(c, std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    std::set<DecisionKind> encountered;
    for (int d = 0; d < 3; ++d)
        for (auto seed : {1u, 39u, 42u}) {
            MatchConfig config;
            config.seed = seed;
            config.players[0] = presets[1].deck;
            config.players[1] = presets[static_cast<std::size_t>(d + 1)].deck;
            MatchSession session(c, config);
            for (int n = 0; session.engine().state().result == -1; ++n) {
                REQUIRE(n < 1500);
                const auto &s = session.engine().state();
                const int actor = s.decision ? s.decision->player : s.active;
                if (s.decision)
                    encountered.insert(s.decision->kind);
                const auto selected = ai::choose(session.engine().viewFor(actor),
                                                 static_cast<ai::Difficulty>(actor == 1 ? d : 1));
                REQUIRE(selected);
                auto result = session.submit(actor, selected->command);
                INFO(result.error);
                REQUIRE(result.accepted);
                REQUIRE(Rules::invariants(session.engine().state(), c.catalog).empty());
            }
            REQUIRE(replay(c, session.recording()).digest() == session.engine().digest());
        }
    REQUIRE(encountered.count(DecisionKind::Response));
    REQUIRE(encountered.count(DecisionKind::CastOrder));
    REQUIRE(encountered.count(DecisionKind::Concentration));
}
TEST_CASE("pending AI plans pause and reject duplicate revised or previous match submissions") {
    Fixture f;
    auto a = f.app();
    MatchConfig config;
    config.seed = 39;
    for (auto &p : config.players)
        p.cards = f.data.deck;
    std::string error;
    REQUIRE(a.start(config, error, {MatchMode::Ai, ai::Difficulty::Hard}));
    for (int n = 0; !a.aiTurn(); ++n) {
        REQUIRE(n < 100);
        REQUIRE(a.submit(0, student(a), a.generation()).accepted);
    }
    auto plan = a.planAi();
    REQUIRE(plan);
    auto digest = a.match().engine().digest();
    a.openSettings();
    REQUIRE_FALSE(a.planAi());
    REQUIRE(a.commitAi(*plan).errorCode == "match_paused");
    REQUIRE(a.match().engine().digest() == digest);
    a.closeSettings();
    REQUIRE(a.commitAi(*plan).accepted);
    REQUIRE(a.commitAi(*plan).errorCode == "stale_ai");
    a.requestLeave();
    REQUIRE(a.confirmLeave(0, error));
    REQUIRE(a.start(config, error, {MatchMode::Ai, ai::Difficulty::Easy}));
    REQUIRE(a.commitAi(*plan).errorCode == "stale_ai");
}
TEST_CASE(
    "fixed tutorial uses normal shuffle rejects wrong moves and teaches real cancellation and release") {
    Fixture f;
    auto a = f.app();
    std::string error;
    REQUIRE(a.startTutorial(error));
    REQUIRE(a.configuration().seed == 39);
    REQUIRE(a.match().engine().state().first == 1);
    const auto opening = a.match().engine().digest();
    a.requestLeave();
    REQUIRE(a.confirmLeave(0, error));
    REQUIRE(a.startTutorial(error));
    REQUIRE(a.match().engine().digest() == opening);
    std::set<Lesson> visited;
    bool triedWrong = false, paused = false;
    for (int n = 0; a.tutorial()->lesson != Lesson::Complete; ++n) {
        INFO(static_cast<int>(a.tutorial()->lesson));
        REQUIRE(n < 300);
        REQUIRE(a.match().engine().state().result == -1);
        auto v = a.match().engine().viewFor(0);
        visited.insert(a.tutorial()->lesson);
        if (a.tutorial()->canContinue(v)) {
            a.continueTutorial();
            continue;
        }
        if (a.aiTurn()) {
            auto plan = a.planAi();
            REQUIRE(plan);
            REQUIRE(a.commitAi(*plan).accepted);
            continue;
        }
        if (a.tutorial()->lesson == Lesson::Rune && !triedWrong) {
            for (const auto &action : v.actions)
                if (auto cmd = std::get_if<AttachSeal>(&action.command))
                    if (!a.tutorial()->allows(v, action.command)) {
                        auto before = a.match().engine().digest();
                        REQUIRE(a.submit(0, *cmd, a.generation()).errorCode == "tutorial_hint");
                        REQUIRE(a.match().engine().digest() == before);
                        triedWrong = true;
                        break;
                    }
        }
        if (!paused) {
            auto before = a.match().engine().digest();
            auto step = a.tutorial()->lesson;
            a.openSettings();
            a.continueTutorial();
            REQUIRE(a.match().engine().digest() == before);
            REQUIRE(a.tutorial()->lesson == step);
            a.closeSettings();
            paused = true;
        }
        auto result = a.submit(0, student(a), a.generation());
        INFO(result.error);
        REQUIRE(result.accepted);
    }
    REQUIRE(triedWrong);
    REQUIRE(visited.size() == 11);
    REQUIRE(a.tutorial()->countered);
    REQUIRE(a.match().engine().state().players[1].life == startingLife - 10);
    REQUIRE(replay(f.data, a.match().recording()).digest() == a.match().engine().digest());
    for (int n = 0; a.match().engine().state().result == -1; ++n) {
        REQUIRE(n < 1500);
        if (a.aiTurn()) {
            auto plan = a.planAi();
            REQUIRE(plan);
            REQUIRE(a.commitAi(*plan).accepted);
        } else
            REQUIRE(a.submit(0, student(a), a.generation()).accepted);
    }
    REQUIRE(replay(f.data, a.match().recording()).digest() == a.match().engine().digest());
    REQUIRE(a.restart(12345, error));
    REQUIRE(a.mode() == MatchMode::Tutorial);
    REQUIRE(a.configuration().seed == 39);
    REQUIRE(a.match().engine().digest() == opening);
}
