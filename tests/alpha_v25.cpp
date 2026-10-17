#include "wizard/core/engine.hpp"
#include "wizard/command_codec.hpp"
#include "wizard/decks.hpp"
#include "wizard/ai.hpp"
#include "wizard/replay.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <chrono>

using namespace wizard;
namespace {
struct Fixture {
    CardCatalog catalog;
    MatchConfig config;
    Fixture() {
        catalog.alphaV2 = catalog.advancedRules = true;
        catalog.mechanicsRevision = 3; catalog.baseLoadCapacity = 2;
        catalog.rulesVersion = catalog.cardSetVersion = "2.5.0";
        CardDefinition formation; formation.id = "formation"; formation.name = "一级阵法";
        formation.type = CardType::Formation; formation.level = 1; formation.body = 1;
        formation.rings = 3; formation.capacity = 8; formation.income = 2; formation.maxRank = 1;
        formation.baseEligible = false;
        for (int i = 0; i < 4; ++i) {
            auto card = formation; card.id += std::to_string(i); card.name += std::to_string(i);
            catalog.cards.emplace(card.id, card);
            for (int n = 0; n < (i == 3 ? 1 : 3); ++n)
                for (auto &deck : config.players) deck.sideDeck.push_back(card.id);
        }
        CardDefinition spell; spell.id = "spell"; spell.name = "解析法术";
        spell.type = CardType::Analytic; spell.rank = 1; spell.analysisTurns = 1;
        spell.target = TargetKind::Opponent; spell.effects = {{EffectKind::Damage, 1}};
        for (int i = 0; i < 14; ++i) {
            auto card = spell; card.id += std::to_string(i); card.name += std::to_string(i);
            catalog.cards.emplace(card.id, card);
            for (int n = 0; n < (i == 13 ? 1 : 3); ++n)
                for (auto &deck : config.players) deck.cards.push_back(card.id);
        }
        for (auto &deck : config.players) deck.baseFormation = "unused-legacy-field";
    }
    GameState state(Phase phase = Phase::Main) const {
        GameState s; s.phase = phase; s.phaseStep = PhaseStep::Body;
        s.phaseGate = 1; s.nextDecision = 2; s.rng = 42;
        for (int p = 0; p < 2; ++p) {
            s.players[p].mana = 12; s.players[p].ownTurn = 2;
            CardInstance c; c.id = p + 1; c.owner = p;
            c.definition = "formation0"; c.zone = Zone::Analysis;
            s.cards.emplace(c.id, c);
        }
        return s;
    }
};
void advance(GameEngine &e) {
    const auto r = e.submit(e.state().active, AdvancePhase{e.state().phaseGate});
    INFO(r.error); REQUIRE(r.accepted);
}
CardId initial(const GameView &view) {
    for (const auto &action : view.actions)
        if (const auto *choice = std::get_if<SelectInitialFormation>(&action.command)) return choice->card;
    return 0;
}
}
TEST_CASE("Alpha v2.5 main phase can enter casting or skip directly to end with cleanup", "[alpha-v25][phase-choice]") {
    Fixture f;
    f.catalog.cards.at("formation0").onCast = {{EffectKind::Heal, 4}};
    f.catalog.cards.at("formation0").onEnd = {{EffectKind::Heal, 2}};
    auto state = f.state(); state.players[0].life = 30;
    CardInstance prepared; prepared.id = 10; prepared.definition = "spell0";
    prepared.zone = Zone::Casting; prepared.spell = SpellState::Ready;
    state.cards.emplace(prepared.id, prepared);
    auto e = GameEngine::scenario(f.catalog, state);
    const auto actions = e.viewFor(0).actions;
    REQUIRE(std::count_if(actions.begin(), actions.end(), [](const LegalAction &action) {
        return std::holds_alternative<AdvancePhase>(action.command);
    }) == 2);
    const auto gate = e.state().phaseGate;
    const auto skipped = AdvancePhase{gate, true};
    REQUIRE(std::get<AdvancePhase>(decodeCommand(encodeCommand(skipped))).skipCast);
    REQUIRE(e.submit(0, skipped).accepted);
    REQUIRE(e.state().phase == Phase::Draw);
    REQUIRE(e.state().active == 1);
    REQUIRE(e.state().players[0].life == 32);
    REQUIRE_FALSE(e.state().skipCastPhase);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(1, AdvancePhase{e.state().phaseGate, true}).accepted);
    REQUIRE(e.digest() == before);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(std::none_of(e.state().events.begin(), e.state().events.end(), [](const GameEvent &event) {
        return event.kind == "phase_enter" && event.text == "进入施法阶段";
    }));
    e = GameEngine::scenario(f.catalog, state);
    advance(e);
    REQUIRE(e.state().phase == Phase::Cast);
    REQUIRE(e.state().players[0].life == 34);
}

TEST_CASE("Alpha v2.5 ambush responses open on opponent action windows", "[alpha-v25][ambush]") {
    Fixture f;
    auto ambush = f.catalog.cards.at("spell0");
    ambush.id = "ambush";
    ambush.name = "盖伏响应";
    ambush.type = CardType::Word;
    ambush.speed = 3;
    ambush.responseOnly = false;
    ambush.target = TargetKind::Self;
    ResponseAbility ability;
    ability.id = "respond";
    ability.fromAmbush = true;
    ability.windows = {ResponseWindow::Action};
    ability.effects = {{EffectKind::GainMana, 1}};
    ambush.responses = {ability};
    f.catalog.cards.emplace(ambush.id, ambush);
    auto state = f.state(Phase::Main);
    state.active = 1;
    state.globalTurn = 2;
    state.players[0].ownTurn = 2;
    CardInstance hidden;
    hidden.id = 20;
    hidden.owner = 0;
    hidden.definition = ambush.id;
    hidden.zone = Zone::Analysis;
    hidden.host = 1;
    hidden.faceDown = hidden.wasAmbushed = true;
    hidden.ambushedTurn = 1;
    state.cards.emplace(hidden.id, hidden);
    CardInstance pending;
    pending.id = 30;
    pending.owner = 1;
    pending.definition = "spell0";
    pending.zone = Zone::Hand;
    state.cards.emplace(pending.id, pending);
    auto e = GameEngine::scenario(f.catalog, state);
    REQUIRE(e.submit(1, StartAnalysis{30, 2}).accepted);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->player == 0);
    REQUIRE(std::any_of(e.viewFor(0).actions.begin(), e.viewFor(0).actions.end(), [](const LegalAction &action) {
        const auto *respond = std::get_if<Respond>(&action.command);
        return respond && respond->card == 20;
    }));

    auto aiState = state;
    aiState.active = 0;
    aiState.cards.at(20).owner = 1;
    aiState.cards.at(20).host = 2;
    aiState.cards.at(30).owner = 0;
    auto ai = GameEngine::scenario(f.catalog, aiState);
    REQUIRE(ai.submit(0, StartAnalysis{30, 1}).accepted);
    REQUIRE(ai.state().decision);
    REQUIRE(ai.state().decision->player == 1);
    const auto selection = ai::choose(ai.viewFor(1), ai::Difficulty::Normal);
    REQUIRE(selection);
    REQUIRE(std::holds_alternative<Respond>(selection->command));
    REQUIRE(ai.submit(1, selection->command).accepted);
}

TEST_CASE("Alpha v2.5 AI can answer an opponent cast with a facedown counter", "[alpha-v25][ambush]") {
    Fixture f;
    auto counter = f.catalog.cards.at("spell0");
    counter.id = "counter";
    counter.name = "盖伏反制";
    counter.type = CardType::Word;
    counter.speed = 4;
    counter.responseOnly = true;
    ResponseAbility ability;
    ability.id = "counter";
    ability.fromAmbush = true;
    ability.windows = {ResponseWindow::Cast};
    ability.eventOwner = EventOwner::Opponent;
    ability.target = TargetKind::EnemySpellLink;
    ability.effects = {{EffectKind::CounterSpell, 0}};
    counter.responses = {ability};
    f.catalog.cards.emplace(counter.id, counter);
    auto state = f.state(Phase::Cast);
    state.active = 0;
    state.globalTurn = 2;
    state.players[1].ownTurn = 2;
    CardInstance hidden;
    hidden.id = 20;
    hidden.owner = 1;
    hidden.definition = counter.id;
    hidden.zone = Zone::Analysis;
    hidden.host = 2;
    hidden.faceDown = hidden.wasAmbushed = true;
    hidden.ambushedTurn = 1;
    state.cards.emplace(hidden.id, hidden);
    CardInstance pending;
    pending.id = 30;
    pending.owner = 0;
    pending.definition = "spell0";
    pending.zone = Zone::Casting;
    pending.spell = SpellState::Ready;
    state.cards.emplace(pending.id, pending);
    auto e = GameEngine::scenario(f.catalog, state);
    REQUIRE(e.submit(0, ActivateSpell{30}).accepted);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->player == 1);
    const auto selection = ai::choose(e.viewFor(1), ai::Difficulty::Normal);
    REQUIRE(selection);
    REQUIRE(std::holds_alternative<Respond>(selection->command));
    REQUIRE(e.submit(1, selection->command).accepted);
}

TEST_CASE("Alpha v2.5 action windows let AI use a facedown shield", "[alpha-v25][ambush]") {
    Fixture f;
    auto shield = f.catalog.cards.at("spell0");
    shield.id = "shield";
    shield.name = "盖伏护盾";
    shield.type = CardType::Word;
    shield.cost = 1;
    shield.speed = 2;
    shield.target = TargetKind::Self;
    shield.responses = {};
    ResponseAbility response;
    response.id = "shield";
    response.fromAmbush = true;
    response.windows = {ResponseWindow::Action};
    response.target = TargetKind::Self;
    response.effects = {{EffectKind::ShieldForTurn, 5}};
    shield.responses = {response};
    f.catalog.cards.emplace(shield.id, shield);
    auto action = f.catalog.cards.at("spell0");
    action.id = "action";
    action.name = "对手行动";
    action.type = CardType::Action;
    action.cost = 1;
    action.effects = {{EffectKind::GainMana, 1}};
    action.responses.clear();
    f.catalog.cards.emplace(action.id, action);
    auto state = f.state(Phase::Main);
    state.active = 1;
    state.globalTurn = 2;
    state.players[0].ownTurn = 2;
    CardInstance hidden;
    hidden.id = 20;
    hidden.owner = 0;
    hidden.definition = shield.id;
    hidden.zone = Zone::Analysis;
    hidden.host = 1;
    hidden.faceDown = hidden.wasAmbushed = true;
    hidden.ambushedTurn = 1;
    state.cards.emplace(hidden.id, hidden);
    CardInstance pending;
    pending.id = 30;
    pending.owner = 1;
    pending.definition = action.id;
    pending.zone = Zone::Hand;
    state.cards.emplace(pending.id, pending);
    auto engine = GameEngine::scenario(f.catalog, state);
    REQUIRE(engine.submit(1, PlayAction{30}).accepted);
    REQUIRE(engine.state().decision);
    REQUIRE(engine.state().decision->player == 0);
    const auto view = engine.viewFor(0);
    REQUIRE(std::any_of(view.actions.begin(), view.actions.end(), [](const LegalAction &legal) {
        const auto *respond = std::get_if<Respond>(&legal.command);
        return respond && respond->card == 20;
    }));
    const auto selection = ai::choose(view, ai::Difficulty::Normal);
    REQUIRE(selection);
    REQUIRE(std::holds_alternative<Respond>(selection->command));
    REQUIRE(engine.submit(0, selection->command).accepted);
    for (int n = 0; engine.state().decision && n < 8; ++n)
        REQUIRE(engine.submit(engine.state().decision->player,
                              PassResponse{engine.state().decision->id})
                    .accepted);
    REQUIRE(engine.state().cards.at(30).zone == Zone::Ash);
}

TEST_CASE("Alpha v2.5 skip casting preserves main phase end response before entering end", "[alpha-v25][phase-choice]") {
    Fixture f;
    auto &spell = f.catalog.cards.at("spell0");
    spell.type = CardType::Word; spell.speed = 3;
    ResponseAbility ability; ability.id = "end-response"; ability.fromAmbush = true;
    ability.handCost = 0; ability.windows = {ResponseWindow::PhaseEnd};
    ability.phases = {Phase::Main}; ability.effects = {{EffectKind::Heal, 1}};
    spell.responses.push_back(ability);
    auto state = f.state();
    CardInstance response; response.id = 10; response.owner = 1;
    response.definition = "spell0"; response.zone = Zone::Analysis; response.host = 2;
    response.faceDown = response.wasAmbushed = true; response.ambushedTurn = 0;
    state.cards.emplace(response.id, response);
    auto e = GameEngine::scenario(f.catalog, state);
    REQUIRE(e.submit(0, AdvancePhase{e.state().phaseGate, true}).accepted);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->kind == DecisionKind::Response);
    REQUIRE(e.state().phase == Phase::Main);
    REQUIRE(e.state().skipCastPhase);
    auto replayed = GameEngine::scenario(f.catalog, state);
    REQUIRE(replayed.submit(0, decodeCommand(encodeCommand(AdvancePhase{state.phaseGate, true}))).accepted);
    REQUIRE(replayed.digest() == e.digest());
    REQUIRE(e.submit(1, PassResponse{e.state().decision->id}).accepted);
    REQUIRE(e.state().phase == Phase::Draw);
    REQUIRE(e.state().active == 1);
    REQUIRE_FALSE(e.state().skipCastPhase);
}
TEST_CASE("Alpha v2.5 skipped casting completes the turn after mandatory end cleanup choices", "[alpha-v25][phase-choice]") {
    Fixture f; auto state = f.state();
    for (CardId id = 10; id < 19; ++id) {
        CardInstance hand; hand.id = id; hand.definition = "spell0"; hand.zone = Zone::Hand;
        state.cards.emplace(id, hand);
    }
    auto e = GameEngine::scenario(f.catalog, state);
    REQUIRE(e.submit(0, AdvancePhase{e.state().phaseGate, true}).accepted);
    REQUIRE(e.state().phase == Phase::End);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->kind == DecisionKind::Discard);
    REQUIRE(e.state().skipCastPhase);
    REQUIRE(e.submit(0, Choose{e.state().decision->id, 10}).accepted);
    REQUIRE(e.state().active == 1);
    REQUIRE(e.state().phase == Phase::Draw);
    REQUIRE_FALSE(e.state().skipCastPhase);
}
TEST_CASE("Alpha v2.5 initial formations are privately confirmed before shuffle and simultaneous reveal", "[alpha-v25]") {
    Fixture f; GameEngine e(f.catalog, f.config);
    REQUIRE(Rules::deckErrors(f.catalog, f.config.players[0]).empty());
    const auto a = e.viewFor(0), b = e.viewFor(1);
    REQUIRE(a.opening); REQUIRE(a.players[0].handCount == 0);
    REQUIRE(a.globalTurn == 0);
    REQUIRE(a.players[0].deckCount == 40); REQUIRE(a.players[1].sideDeckCount == 10);
    REQUIRE(initial(a) != 0); REQUIRE(initial(b) != 0);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, SelectInitialFormation{initial(b)}).accepted);
    REQUIRE_FALSE(e.submit(0, AdvancePhase{}).accepted);
    REQUIRE(e.digest() == before);
    REQUIRE(e.submit(0, SelectInitialFormation{initial(a)}).accepted);
    const auto privateView = e.viewFor(0), otherView = e.viewFor(1);
    REQUIRE(privateView.initialFormation == initial(a));
    REQUIRE(otherView.initialFormation == 0); REQUIRE(otherView.initialFormationConfirmed[0]);
    REQUIRE(otherView.players[0].sideDeckCount == 10);
    REQUIRE(std::none_of(otherView.cards.begin(), otherView.cards.end(), [](const CardView &c) { return c.instance.owner == 0; }));
    REQUIRE(privateView.events.empty()); REQUIRE(e.state().rng == f.config.seed);
    REQUIRE(initial(privateView) == 0); REQUIRE_FALSE(e.submit(0, SelectInitialFormation{initial(a)}).accepted);
    REQUIRE(e.submit(1, SelectInitialFormation{initial(b)}).accepted);
    const auto started = e.viewFor(0);
    REQUIRE_FALSE(started.opening); REQUIRE(started.players[0].handCount == 5);
    REQUIRE(started.globalTurn == 1);
    REQUIRE(started.players[1].handCount == 5); REQUIRE(started.players[0].sideDeckCount == 9);
    REQUIRE(started.players[1].sideDeckCount == 9); REQUIRE(started.players[0].deckCount == 35);
    REQUIRE(e.state().cards.at(initial(a)).base); REQUIRE(e.state().cards.at(initial(b)).base);
    REQUIRE(e.state().players[0].formations == 0); REQUIRE(e.state().players[1].formations == 0);
    REQUIRE(std::count_if(started.events.begin(), started.events.end(), [](const GameEvent &event) { return event.kind == "initial_formation"; }) == 2);
    GameEngine replayed(f.catalog, f.config);
    REQUIRE(replayed.submit(0, decodeCommand(encodeCommand(SelectInitialFormation{initial(a)}))).accepted);
    REQUIRE(replayed.submit(1, SelectInitialFormation{initial(b)}).accepted);
    REQUIRE(replayed.digest() == e.digest());
}
TEST_CASE("Alpha v2.5 rejects non-level-one opening entities and allows either confirmation order", "[alpha-v25]") {
    Fixture f; f.catalog.cards.at("formation1").level = 2;
    GameEngine e(f.catalog, f.config);
    const auto view = e.viewFor(0);
    auto invalid = std::find_if(view.cards.begin(), view.cards.end(), [](const CardView &card) {
        return card.definition.id == "formation1";
    });
    REQUIRE(invalid != view.cards.end());
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, SelectInitialFormation{invalid->instance.id}).accepted); REQUIRE(e.digest() == before);
    REQUIRE(e.submit(1, SelectInitialFormation{initial(e.viewFor(1))}).accepted);
    REQUIRE(e.state().active == 0); REQUIRE(e.viewFor(0).initialFormation == 0);
    REQUIRE(e.submit(0, SelectInitialFormation{initial(e.viewFor(0))}).accepted);
    REQUIRE_FALSE(e.state().opening);
    const auto started = e.viewFor(0);
    REQUIRE(std::none_of(started.actions.begin(), started.actions.end(), [](const LegalAction &a) {
        return std::holds_alternative<SelectInitialFormation>(a.command);
    }));
}
TEST_CASE("Alpha v2.5 timeouts use an authority path and terminate pending work deterministically", "[alpha-v25]") {
    Fixture f; GameEngine e(f.catalog, f.config);
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, Timeout{}).accepted); REQUIRE(e.digest() == before);
    REQUIRE(e.timeout(0).accepted); REQUIRE(e.state().result == 1); REQUIRE_FALSE(e.state().opening);
    REQUIRE(e.state().events.back().kind == "timeout");
    GameEngine replayed(f.catalog, f.config);
    REQUIRE(std::holds_alternative<Timeout>(decodeCommand(encodeCommand(Timeout{}))));
    REQUIRE(replayed.timeout(0).accepted); REQUIRE(replayed.digest() == e.digest());
    REQUIRE_FALSE(e.timeout(1).accepted);
}
TEST_CASE("Alpha v2.5 replay records opening and timeout events in format four and rejects old versions", "[alpha-v25]") {
    Fixture f; Content content; content.catalog = f.catalog;
    MatchSession session(content, f.config);
    auto now = MatchSession::Clock::time_point{};
    session.enableClock(false, [&] { return now; });
    REQUIRE(session.submit(0, SelectInitialFormation{initial(session.viewFor(0))}).accepted);
    REQUIRE(session.submit(1, SelectInitialFormation{initial(session.viewFor(1))}).accepted);
    now += std::chrono::minutes(6); session.tick();
    REQUIRE(session.engine().state().result != -1);
    const auto record = session.recording();
    REQUIRE(record.at("format") == 4);
    REQUIRE_FALSE(record.at("players").at(0).contains("baseFormation"));
    REQUIRE(record.at("commands").back().at("command").at("type") == 19);
    REQUIRE(replay(content, record).digest() == session.engine().digest());
    auto old = record; old["format"] = 3; REQUIRE_THROWS(replay(content, old));
    old = record; old["programVersion"] = "2.0.0-rc.1"; REQUIRE_THROWS(replay(content, old));
    old = record; old["rulesVersion"] = "2.0.0-rc.1"; REQUIRE_THROWS(replay(content, old));
}
TEST_CASE("Alpha v2.5 formation setting text derives from validated recipes and resolves later card names", "[alpha-v25]") {
    auto json = readCatalogJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    json["mechanicsRevision"] = 3;
    auto formation = std::find_if(json["cards"].begin(), json["cards"].end(), [](const Json &card) { return card.at("id") == "small-hexagram"; });
    REQUIRE(formation != json["cards"].end());
    (*formation)["baseEligible"] = false;
    (*formation)["recipe"] = Json::array({{{"kind", "mana"}, {"amount", 2}},
        {{"kind", "sacrifice_formation"}, {"amount", 1}, {"definition", "hexagram"}},
        {{"kind", "sacrifice_spell"}, {"amount", 2}, {"readyOnly", true}, {"distinctSchools", true}, {"fireDamage", true}}});
    (*formation)["summonCondition"] = "不应作为权威条件显示的旧文本";
    const auto catalog = parseCatalog(json);
    const auto &conditions = catalog.at("small-hexagram").settingConditions;
    REQUIRE(conditions.find("支付2魔素") != std::string::npos);
    REQUIRE(conditions.find("均衡之六芒星") != std::string::npos);
    REQUIRE(conditions.find("已解析完成或持续生效") != std::string::npos);
    REQUIRE(conditions.find("学派互不相同") != std::string::npos);
    REQUIRE(conditions.find("火焰伤害") != std::string::npos);
    REQUIRE(conditions.find("开局可从副卡组免费选择") != std::string::npos);
    REQUIRE(conditions.find("不应作为权威") == std::string::npos);
    (*formation)["recipe"][1]["definition"] = "unknown-material";
    REQUIRE_THROWS(parseCatalog(json));
}
TEST_CASE("Alpha v2.5 construction requires a level one formation without a saved choice or base eligibility", "[alpha-v25]") {
    Fixture f; app::DeckDraft draft; draft.format = 3; draft.id = "draft"; draft.name = "卡组";
    draft.baseFormation.clear();
    for (const auto &id : f.config.players[0].cards) ++draft.cards[id];
    for (const auto &id : f.config.players[0].sideDeck) ++draft.sideCards[id];
    REQUIRE(app::constructionProblems(f.catalog, draft).empty());
    const auto encoded = app::encodeDraft(draft);
    REQUIRE_FALSE(encoded.contains("baseFormation"));
    REQUIRE(app::decodeDraft(encoded).baseFormation.empty());
    for (auto &[id, card] : f.catalog.cards) if (card.type == CardType::Formation) card.level = 2;
    REQUIRE_FALSE(Rules::deckErrors(f.catalog, f.config.players[0]).empty());
    REQUIRE_FALSE(app::constructionProblems(f.catalog, draft).empty());
}
TEST_CASE("storm point counts affect canonical match state and remain in public card views", "[alpha-v25][storm]") {
    Fixture f;
    auto withStormPoints = f.state();
    withStormPoints.cards.at(1).stormPoints = 5;
    const auto charged = GameEngine::scenario(f.catalog, withStormPoints);
    const auto empty = GameEngine::scenario(f.catalog, f.state());
    auto withReleaseProgress = f.state();
    withReleaseProgress.cards.at(1).stormReleaseProgress = 1;
    const auto releasing = GameEngine::scenario(f.catalog, withReleaseProgress);

    REQUIRE(charged.digest() != empty.digest());
    REQUIRE(releasing.digest() != empty.digest());
    const auto view = charged.viewFor(0);
    const auto card = std::find_if(view.cards.begin(), view.cards.end(), [](const CardView &item) {
        return item.instance.id == 1;
    });
    REQUIRE(card != view.cards.end());
    REQUIRE(card->instance.stormPoints == 5);
    REQUIRE(card->instance.stormReleaseProgress == 0);
    const auto releaseView = releasing.viewFor(0);
    const auto releaseCard = std::find_if(releaseView.cards.begin(), releaseView.cards.end(), [](const CardView &item) {
        return item.instance.id == 1;
    });
    REQUIRE(releaseCard != releaseView.cards.end());
    REQUIRE(releaseCard->instance.stormReleaseProgress == 1);
}
TEST_CASE("Alpha v2.5 draw clears temporary resources before income and exhaustion only for the turn player", "[alpha-v25]") {
    Fixture f; auto s = f.state(Phase::End);
    s.players[0].temporaryLife = s.players[1].temporaryLife = 8;
    s.temporary = {{1, 1, 0, 4, 0}, {2, 0, 0, 3, 0}, {3, 1, 0, 1, 0, true}, {4, 1, 2, 1, 0, false, true}};
    s.nextLoad = 5;
    auto e = GameEngine::scenario(f.catalog, s); advance(e);
    REQUIRE(e.state().phase == Phase::Draw); REQUIRE(e.state().active == 1);
    REQUIRE(e.state().players[1].temporaryLife == 0); REQUIRE(e.state().players[1].life == 39);
    REQUIRE(e.state().players[0].temporaryLife == 8); REQUIRE(Rules::load(e.state(), 1) == 2);
    REQUIRE(Rules::load(e.state(), 0) == 3);
    const auto &events = e.state().events;
    const auto load = std::find_if(events.begin(), events.end(), [](const GameEvent &v) { return v.kind == "temporary_load_cleared"; });
    const auto life = std::find_if(events.begin(), events.end(), [](const GameEvent &v) { return v.kind == "temporary_life_cleared"; });
    const auto income = std::find_if(events.begin(), events.end(), [](const GameEvent &v) { return v.kind == "income"; });
    REQUIRE(load < life); REQUIRE(life < income);
    auto continuation = e.state(); continuation.players[1].temporaryLife = 5;
    continuation.temporary.push_back({5, 1, 0, 2, 0}); continuation.nextLoad = 6;
    e = GameEngine::scenario(f.catalog, continuation); advance(e);
    REQUIRE(e.state().phase == Phase::Prepare); REQUIRE(e.state().players[1].temporaryLife == 5);
    REQUIRE(Rules::load(e.state(), 1) == 4);
}
TEST_CASE("Alpha v2.5 timed focus and perpetual spells activate in casting and maintain without a new activation", "[alpha-v25]") {
    for (int trait = 0; trait < 3; ++trait) {
        Fixture f; auto &d = f.catalog.cards.at("spell0");
        d.duration = trait == 0 ? 2 : 0; d.concentration = trait == 1; d.perpetual = trait == 2;
        d.concentrationUpkeep = 1;
        auto s = f.state(); CardInstance c; c.id = 10; c.definition = "spell0";
        c.zone = Zone::Analysis; c.host = 1; c.spell = SpellState::Ready; s.cards.emplace(c.id, c);
        auto e = GameEngine::scenario(f.catalog, s);
        REQUIRE_FALSE(e.submit(0, ActivateSpell{10}).accepted);
        const auto before = e.viewFor(0);
        REQUIRE(std::none_of(before.actions.begin(), before.actions.end(), [](const LegalAction &a) { return std::holds_alternative<ActivateSpell>(a.command); }));
        REQUIRE(e.submit(0, PrepareCast{10}).accepted); REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
        advance(e); REQUIRE(e.submit(0, ActivateSpell{10}).accepted);
        REQUIRE(e.state().cards.at(10).zone == Zone::Casting); REQUIRE(e.state().cards.at(10).spell == SpellState::Active);
        advance(e); REQUIRE(e.state().decision); REQUIRE(e.state().decision->kind == DecisionKind::ReturnSpell);
        REQUIRE(e.submit(0, Choose{e.state().decision->id, 1}).accepted);
        REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
        advance(e);
        if (trait == 0) REQUIRE(e.state().cards.at(10).remaining == 1);
        if (trait == 1) {
            for (int n = 0; n < 6 && !e.state().decision; ++n) advance(e);
            REQUIRE(e.state().decision); REQUIRE(e.state().decision->kind == DecisionKind::Concentration);
            const auto mana = e.state().players[0].mana;
            REQUIRE(e.submit(0, Choose{e.state().decision->id, 10}).accepted);
            REQUIRE(e.state().cards.at(10).zone == Zone::Analysis); REQUIRE_FALSE(e.state().chain);
            REQUIRE(e.state().players[0].mana == mana - 1);
        }
    }
}
TEST_CASE("Alpha v2.5 continuous spells go to ash when no legal return ring remains", "[alpha-v25]") {
    Fixture f;
    auto &definition = f.catalog.cards.at("spell0");
    definition.perpetual = true;
    auto s = f.state();
    CardInstance active; active.id = 10; active.owner = 0; active.definition = "spell0";
    active.zone = Zone::Casting; active.spell = SpellState::Active;
    active.sourceFormation = 1; active.sourceRingIndex = 0;
    s.cards.emplace(active.id, active);
    for (int ring = 0; ring < 3; ++ring) {
        CardInstance occupant; occupant.id = 20 + ring; occupant.owner = 0; occupant.definition = "spell1";
        occupant.zone = Zone::Analysis; occupant.spell = SpellState::Active;
        occupant.host = 1; occupant.sourceFormation = 1; occupant.ringIndex = ring; occupant.sourceRingIndex = ring;
        s.cards.emplace(occupant.id, occupant);
    }
    s.returnRequested = true;
    auto e = GameEngine::scenario(f.catalog, s);
    advance(e);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().maintenanceSource == 0);
    REQUIRE_FALSE(e.state().returnRequested);
}
TEST_CASE("Alpha v2.5 continuous spells can return to another legal formation and reject invalid choices atomically", "[alpha-v25]") {
    Fixture f;
    auto &definition = f.catalog.cards.at("spell0");
    definition.perpetual = true;
    auto s = f.state();
    s.cards.at(1).zone = Zone::Ash;
    CardInstance replacement; replacement.id = 3; replacement.owner = 0; replacement.definition = "formation1";
    replacement.zone = Zone::Analysis;
    s.cards.emplace(replacement.id, replacement);
    CardInstance active; active.id = 10; active.owner = 0; active.definition = "spell0";
    active.zone = Zone::Casting; active.spell = SpellState::Active;
    active.sourceFormation = 1; active.sourceRingIndex = 0;
    s.cards.emplace(active.id, active);
    s.returnRequested = true;
    auto e = GameEngine::scenario(f.catalog, s);
    advance(e);
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->kind == DecisionKind::ReturnSpell);
    const auto decision = e.state().decision->id;
    REQUIRE(std::find(e.state().decision->options.begin(), e.state().decision->options.end(), 3) !=
            e.state().decision->options.end());
    const auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, Choose{decision, 3, 99}).accepted);
    REQUIRE(e.digest() == before);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    REQUIRE(e.submit(0, Choose{decision, 3, 0}).accepted);
    REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(e.state().cards.at(10).host == 3);
}
TEST_CASE("Alpha v2.5 side-deck content has no saved initial choice and validates its level-one candidate", "[alpha-v25]") {
    struct TemporaryDirectory {
        std::filesystem::path path = std::filesystem::temp_directory_path() /
            ("wizard-v25-side-" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
        TemporaryDirectory() { std::filesystem::create_directories(path); }
        ~TemporaryDirectory() { std::error_code ec; std::filesystem::remove_all(path, ec); }
    } temporary;
    const auto assets = std::filesystem::path(WIZARD_SOURCE_DIR) / "assets";
    auto json = readCatalogJson(assets); json["mechanicsRevision"] = 3;
    auto side = readJson(assets / "side-deck.json"); side.erase("initialFormation");
    writeJson(temporary.path / "cards.json", json);
    writeJson(temporary.path / "deck.json", readJson(assets / "deck.json"));
    writeJson(temporary.path / "side-deck.json", side);
    const auto content = loadContent(temporary.path);
    REQUIRE(content.initialFormation.empty()); REQUIRE(content.sideDeck.size() == 10);
    for (auto &card : json["cards"])
        if (card.at("type") == "formation" && card.at("level") == 1) { card["level"] = 2; card["baseEligible"] = false; }
    writeJson(temporary.path / "cards.json", json);
    REQUIRE_THROWS(loadContent(temporary.path));
}
