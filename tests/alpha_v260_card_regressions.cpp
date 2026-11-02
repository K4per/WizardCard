#include "support/observed_engine.hpp"
#include "wizard/bridge.hpp"
#include "wizard/core/action_query.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

using namespace wizard;

namespace {
CardCatalog releaseCatalog() {
    return loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets" / "release-v260").catalog;
}

CardInstance &put(MutationContext &state, CardId id, const char *definition, Zone zone, PlayerId owner = 0) {
    CardInstance card;
    card.id = id; card.definition = definition; card.zone = zone; card.owner = owner;
    if (zone == Zone::Analysis) card.fieldOrder = state.nextFieldOrder++;
    state.cards[id] = card;
    if (zone == Zone::Deck) state.players[owner].deck.push_back(id);
    return state.cards.at(id);
}

MutationContext mainState() {
    MutationContext state;
    state.phase = Phase::Main; state.phaseStep = PhaseStep::Body; state.flow = Flow::Main;
    state.phaseGate = 1; state.globalTurn = 5; state.rng = 73;
    for (auto &player : state.players) { player.mana = 20; player.ownTurn = 2; }
    put(state, 1, "v260-formation-0017", Zone::Analysis);
    put(state, 2, "v260-formation-0017", Zone::Analysis, 1);
    return state;
}

void send(test::ObservedEngine &engine, PlayerId player, const Command &command) {
    const auto result = engine.submit(player, command);
    INFO(result.error);
    REQUIRE(result.accepted);
}

void passResponses(test::ObservedEngine &engine) {
    for (int n = 0; n < 20 && engine.state().decision &&
         engine.state().decision->kind == DecisionKind::Response; ++n) {
        const auto decision = *engine.state().decision;
        send(engine, decision.player, PassResponse{decision.id});
    }
    REQUIRE_FALSE((engine.state().decision && engine.state().decision->kind == DecisionKind::Response));
}
}

TEST_CASE("Alpha v2.6.0 rune details display both attachment effects", "[alpha-v260][card-regression]") {
    const auto catalog = releaseCatalog();
    const std::vector<std::array<std::string, 3>> runes{
        {"v260-seal-0001", "魔素收入+1。", "无效果。"},
        {"v260-seal-0002", "环位+2。", "发动时，获得其实际设置费用的魔素。"},
        {"v260-seal-0003", "阵法等级+1，环位+1，魔素收入-1。", "发动费用+1，额外造成5点力场伤害。"}
    };
    for (const auto &rune : runes) {
        INFO(rune[0]);
        const auto text = bridge::cardDefinitionDto(catalog.at(rune[0])).at("text").get<std::string>();
        CHECK(text.find("附着于阵法") != std::string::npos);
        CHECK(text.find(rune[1]) != std::string::npos);
        CHECK(text.find("附着于法术") != std::string::npos);
        CHECK(text.find(rune[2]) != std::string::npos);
    }
}

TEST_CASE("Alpha v2.6.0 divination discards then selects one of the top three cards", "[alpha-v260][card-regression]") {
    const auto catalog = releaseCatalog();
    auto state = mainState();
    put(state, 10, "v260-word-0020", Zone::Hand);
    put(state, 11, "v260-word-0001", Zone::Hand);
    for (CardId id = 20; id < 24; ++id) put(state, id, "v260-analytic-0001", Zone::Deck);
    auto engine = test::ObservedEngine::scenario(catalog, state);

    const auto actions = engine.viewFor(0).actions;
    const auto action = std::find_if(actions.begin(), actions.end(), [](const LegalAction &candidate) {
        const auto *word = std::get_if<PreloadWord>(&candidate.command);
        return word && word->card == 10;
    });
    REQUIRE(action != actions.end());
    CHECK(std::get<PreloadWord>(action->command).discard == 11);
    send(engine, 0, action->command);
    passResponses(engine);
    CHECK(engine.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(engine.state().decision);
    const auto decision = *engine.state().decision;
    CHECK(decision.player == 0);
    REQUIRE(decision.options.size() == 3);
    for (const CardId id : {21u, 22u, 23u})
        CHECK(std::find(decision.options.begin(), decision.options.end(), ChoiceValue::card(id)) != decision.options.end());
    for (const PlayerId viewer : {0, 1}) {
        const auto view = engine.viewFor(viewer);
        for (const CardId id : {20u, 21u, 22u, 23u})
            CHECK(std::any_of(view.cards.begin(), view.cards.end(), [&](const CardView &card) {
                return card.instance.id == id;
            }) == (viewer == 0 && id != 20));
    }
    send(engine, 0, Choose{decision.id, ChoiceValue::card(22)});
    passResponses(engine);
    CHECK(engine.state().cards.at(22).zone == Zone::Hand);
    REQUIRE(engine.state().players[0].deck.size() == 3);
    CHECK(engine.state().players[0].deck.back() == 20);
    CHECK(engine.state().players[0].deck == std::vector<CardId>{21, 23, 20});
    CHECK(engine.state().cards.at(10).spell == SpellState::Resolved);
    CHECK_FALSE(engine.state().decision);
}

TEST_CASE("Alpha v2.6.0 circuit overload selects analysis and allows passing later responses", "[alpha-v260][card-regression]") {
    const auto catalog = releaseCatalog();
    auto state = mainState();
    put(state, 10, "v260-word-0005", Zone::Hand);
    put(state, 11, "v260-word-0001", Zone::Hand);
    auto &target = put(state, 20, "v260-analytic-0001", Zone::Analysis);
    target.host = target.sourceFormation = 1;
    target.ringIndex = target.sourceRingIndex = 0;
    target.spell = SpellState::Analyzing; target.settingPaid = 4;
    auto engine = test::ObservedEngine::scenario(catalog, state);

    ActionIntent intent;
    intent.selections = {
        {ActionField::Operation, std::to_string(static_cast<unsigned>(CommandKind::PreloadWord))},
        {ActionField::Source, "10"}, {ActionField::Host, "1"}
    };
    const auto query = engine.queryActions(0, intent);
    REQUIRE(query.field == ActionField::Target);
    REQUIRE(query.options.size() == 1);
    CHECK(query.options.front().card == 20);

    const auto actions = engine.viewFor(0).actions;
    const auto action = std::find_if(actions.begin(), actions.end(), [](const LegalAction &candidate) {
        const auto *word = std::get_if<PreloadWord>(&candidate.command);
        return word && word->card == 10 && word->target == 20;
    });
    REQUIRE(action != actions.end());
    send(engine, 0, action->command);
    passResponses(engine);
    CHECK(engine.state().cards.at(20).spell == SpellState::Ready);
    CHECK(engine.state().cards.at(20).analysisProgress == catalog.at("v260-analytic-0001").analysisTurns);
    CHECK(std::any_of(engine.state().temporary.begin(), engine.state().temporary.end(), [](const TemporaryLoad &load) {
        return load.owner == 0 && load.source == 10 && load.amount == 4;
    }));
    CHECK(engine.state().cards.at(10).spell == SpellState::Resolved);
    REQUIRE_FALSE(engine.state().decision);
    send(engine, 0, AdvancePhase{engine.state().phaseGate});
    passResponses(engine);
    CHECK(engine.state().phase == Phase::Cast);
}

TEST_CASE("Alpha v2.6.0 circuit overload rejects a missing target atomically", "[alpha-v260][card-regression]") {
    auto state = mainState();
    put(state, 10, "v260-word-0005", Zone::Hand);
    put(state, 11, "v260-word-0001", Zone::Hand);
    auto engine = test::ObservedEngine::scenario(releaseCatalog(), state);
    const auto before = engine.digest();
    const auto result = engine.submit(0, PreloadWord{10, true, 1});
    CHECK_FALSE(result.accepted);
    if (result.accepted) passResponses(engine);
    CHECK(engine.digest() == before);
}

TEST_CASE("Alpha v2.6.0 divination requires a separate discard and handles short decks", "[alpha-v260][card-regression]") {
    const auto catalog = releaseCatalog();
    for (int count = 0; count <= 2; ++count) {
        CAPTURE(count);
        auto state = mainState();
        put(state, 10, "v260-word-0020", Zone::Hand);
        for (int n = 0; n < count; ++n) put(state, 20 + n, "v260-analytic-0001", Zone::Deck);
        auto engine = test::ObservedEngine::scenario(catalog, state);
        const auto before = engine.digest();
        CHECK_FALSE(engine.submit(0, PreloadWord{10, true, 1}).accepted);
        CHECK_FALSE(engine.submit(0, PreloadWord{10, true, 1, 0, {}, 10}).accepted);
        CHECK(engine.digest() == before);

        put(state, 11, "v260-seal-0001", Zone::Hand);
        engine = test::ObservedEngine::scenario(catalog, state);
        send(engine, 0, PreloadWord{10, true, 1, 0, {}, 11});
        passResponses(engine);
        if (count) {
            REQUIRE(engine.state().decision);
            CHECK(engine.state().decision->options.size() == static_cast<std::size_t>(count));
            const auto decision = *engine.state().decision;
            send(engine, 0, Choose{decision.id, ChoiceValue::card(20)});
            passResponses(engine);
            CHECK(engine.state().cards.at(20).zone == Zone::Hand);
        }
        CHECK_FALSE(engine.state().decision);
        CHECK(engine.state().players[0].exhaustion == 0);
        CHECK(engine.state().cards.at(11).zone == Zone::Ash);
        CHECK(engine.state().players[0].deck.size() == static_cast<std::size_t>(std::max(0, count - 1)));
    }
}

TEST_CASE("Alpha v2.6.0 circuit overload loses its target without blocking response passes", "[alpha-v260][card-regression]") {
    auto state = mainState();
    put(state, 10, "v260-word-0005", Zone::Hand);
    put(state, 11, "v260-word-0001", Zone::Hand);
    auto &target = put(state, 20, "v260-analytic-0001", Zone::Analysis);
    target.host = 1; target.spell = SpellState::Analyzing; target.settingPaid = 3;
    auto engine = test::ObservedEngine::scenario(releaseCatalog(), state);
    send(engine, 0, PreloadWord{10, true, 1, 20});
    state = test::mutation(engine.state());
    StateMaintenance::leave(state, 20, &engine.catalog());
    engine = test::ObservedEngine::scenario(releaseCatalog(), state);
    passResponses(engine);
    CHECK(engine.state().temporary.empty());
    REQUIRE_FALSE(engine.state().decision);
    send(engine, 0, AdvancePhase{engine.state().phaseGate});
    passResponses(engine);
    CHECK(engine.state().phase == Phase::Cast);
}
