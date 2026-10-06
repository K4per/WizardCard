#include "wizard/content.hpp"
#include "wizard/interaction.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>

using namespace wizard;
namespace {
struct UpdatedCards {
    Content content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    GameState state;
    UpdatedCards() {
        state.phase = Phase::Main;
        state.phaseStep = PhaseStep::Body;
        state.flow = Flow::Main;
        state.nextLoad = 1000;
        state.rng = 7;
        for (int p = 0; p < 2; ++p) {
            state.players[p].mana = 10;
            state.players[p].life = 20;
            state.players[p].ownTurn = 2;
            auto &base = card(static_cast<CardId>(p + 1), "balance", p, Zone::Analysis);
            base.base = true;
            for (CardId id = static_cast<CardId>(100 + 30 * p); id < static_cast<CardId>(120 + 30 * p);
                 ++id) {
                card(id, "spark", p, Zone::Deck);
                state.players[p].deck.push_back(id);
            }
        }
    }
    CardInstance &card(CardId id, const std::string &def, int p = 0, Zone zone = Zone::Hand) {
        CardInstance c;
        c.id = id;
        c.definition = def;
        c.owner = p;
        c.zone = zone;
        state.cards[id] = c;
        return state.cards.at(id);
    }
    CardInstance &ready(CardId id, const std::string &def = "fireball", int p = 0) {
        auto &c = card(id, def, p, Zone::Analysis);
        c.spell = SpellState::Ready;
        c.host = static_cast<CardId>(p + 1);
        c.sourceFormation = c.host;
        c.analysisLoad = c.settingPaid = content.catalog.at(def).cost;
        return c;
    }
    CardInstance &active(CardId id, int p = 0) {
        auto &c = ready(id, "ward", p);
        c.zone = Zone::Casting;
        c.host = 0;
        c.spell = SpellState::Active;
        c.castLoad = 1;
        c.concentrationCost = 1;
        c.targetPlayer = 1 - p;
        c.payments.push_back({1, 1, 1, false});
        return c;
    }
    GameEngine engine() {
        return GameEngine::scenario(content.catalog, state);
    }
};
void submit(GameEngine &e, int p, const Command &cmd) {
    auto r = e.submit(p, cmd);
    INFO(r.error);
    REQUIRE(r.accepted);
}
void choice(GameEngine &e, CardId option) {
    REQUIRE(e.state().decision);
    auto d = *e.state().decision;
    submit(e, d.player, Choose{d.id, option});
}
void advance(GameEngine &e) {
    REQUIRE(e.state().phaseGate);
    submit(e, e.state().active, AdvancePhase{e.state().phaseGate});
}
void reach(GameEngine &e, int player, Phase phase) {
    for (int n = 0; n < 25 && !(e.state().active == player && e.state().phase == phase); ++n) {
        REQUIRE(e.state().result == -1);
        REQUIRE_FALSE(e.state().decision);
        advance(e);
    }
    REQUIRE(e.state().active == player);
    REQUIRE(e.state().phase == phase);
}
bool hasOption(const GameEngine &e, CardId id) {
    const auto &opts = e.state().decision->options;
    return std::find(opts.begin(), opts.end(), id) != opts.end();
}
} // namespace

TEST_CASE("updated card identities artwork mappings and printed values stay aligned") {
    UpdatedCards f;
    const auto &cat = f.content.catalog;
    REQUIRE(cat.cards.size() == 30);
    REQUIRE(cat.at("spark").type == CardType::Word);
    REQUIRE(cat.at("spark").rank == 0);
    REQUIRE(cat.at("spark").immediate);
    REQUIRE(cat.at("unravel").castCost == 0);
    REQUIRE(cat.at("unravel").extraDiscard == 0);
    REQUIRE(cat.at("conduit").type == CardType::Seal);
    REQUIRE(cat.at("conduit").incomeBonus == 2);
    REQUIRE(cat.at("barbs").rarity == "epic");
    REQUIRE(cat.at("barbs").cost == 5);
    REQUIRE(cat.at("reservoir").capacity == 5);
    REQUIRE(cat.at("reservoir").rings == 0);
    REQUIRE(cat.at("balance").baseEligible);
    REQUIRE_FALSE(cat.at("reservoir").baseEligible);
    auto art = readJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/art/runtime.json");
    for (const auto &[id, d] : cat.cards)
        REQUIRE(art["cards"].contains(id));
}
TEST_CASE("rank zero word releases immediately without using a preloaded slot") {
    UpdatedCards f;
    f.card(10, "spark");
    for (CardId id = 11; id <= 13; ++id)
        f.card(id, "barbs", 0, Zone::Words);
    auto e = f.engine();
    submit(e, 0, PreloadWord{10, true});
    REQUIRE(e.state().players[0].mana == 9);
    REQUIRE(e.state().players[1].life == 18);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(Rules::load(e.state(), 0) == 0);
    REQUIRE(e.state().phase == Phase::Main);
    REQUIRE(e.state().cards.at(11).zone == Zone::Words);
    const auto capacity = Rules::capacity(f.state, f.content.catalog, 0);
    f.state.temporary.push_back({1, 0, 0, capacity, 3});
    e = f.engine();
    auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, PreloadWord{10, true}).accepted);
    REQUIRE(e.digest() == before);
}
TEST_CASE("instant word still has a cast response chain and cancellation keeps its paid cost") {
    UpdatedCards f;
    f.card(10, "spark");
    auto d = f.content.catalog.at("clarity");
    d.id = d.name = "counter";
    d.effects.clear();
    d.type = CardType::Word;
    d.speed = 2;
    d.cost = 1;
    d.rank = 1;
    ResponseAbility a;
    a.id = "cancel";
    a.fromWords = true;
    a.preloadedCost = 1;
    a.windows = {ResponseWindow::Cast};
    a.target = TargetKind::PendingLink;
    a.effects = {{EffectKind::NegateLink, 0}};
    d.responses = {a};
    f.content.catalog.cards[d.id] = d;
    f.card(11, "counter", 1, Zone::Words);
    auto e = f.engine();
    submit(e, 0, PreloadWord{10, true});
    REQUIRE(e.state().decision);
    REQUIRE(e.state().decision->kind == DecisionKind::Response);
    REQUIRE(Rules::load(e.state(), 0) == 1);
    auto v = e.viewFor(1);
    auto r = std::find_if(v.actions.begin(), v.actions.end(),
                          [](const LegalAction &a) { return std::holds_alternative<Respond>(a.command); });
    REQUIRE(r != v.actions.end());
    submit(e, 1, r->command);
    REQUIRE(e.state().players[0].mana == 9);
    REQUIRE(e.state().players[1].life == 20);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("fireball resolves ten damage then optional empty own formation choice atomically") {
    UpdatedCards f;
    f.ready(10);
    f.card(11, "reservoir", 0, Zone::Analysis);
    f.card(12, "reservoir", 1, Zone::Analysis);
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().players[1].life == 10);
    REQUIRE(e.state().decision->kind == DecisionKind::DestroyOwnFormation);
    REQUIRE(hasOption(e, 11));
    REQUIRE_FALSE(hasOption(e, 1));
    REQUIRE_FALSE(hasOption(e, 12));
    REQUIRE(e.state().chain->mode == ChainMode::Resolving);
    auto before = e.digest();
    REQUIRE_FALSE(e.submit(0, Choose{e.state().decision->id, 1}).accepted);
    REQUIRE(e.digest() == before);
    choice(e, 11);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().result == -1);
}
TEST_CASE("fireball optional removal may be declined and victory waits for full link cleanup") {
    UpdatedCards f;
    f.ready(10);
    f.card(11, "reservoir", 0, Zone::Analysis);
    f.state.players[1].life = 10;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().result == -1);
    REQUIRE(e.state().players[1].life == 0);
    choice(e, 0);
    REQUIRE(e.state().result == 0);
    REQUIRE(e.state().cards.at(11).zone == Zone::Analysis);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE_FALSE(e.state().effect);
}
TEST_CASE("life stitch produces independent load that survives spell cleanup and end phases") {
    UpdatedCards f;
    f.ready(10, "mend");
    f.state.players[0].life = maximumLife - 2;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().players[0].life == maximumLife);
    REQUIRE(Rules::load(e.state(), 0) == 2);
    REQUIRE(e.state().temporary.front().independent);
    reach(e, 1, Phase::Main);
    REQUIRE(Rules::load(e.state(), 0) == 2);
    reach(e, 0, Phase::Main);
    REQUIRE(Rules::load(e.state(), 0) == 2);
    auto state = e.state();
    Trigger clear;
    clear.owner = 0;
    EffectResolver::apply(state, f.content.catalog, clear, {EffectKind::ClearIndependent, 0});
    REQUIRE(Rules::load(state, 0) == 0);
}
TEST_CASE("shatter ring discards only a formation hand card during resolution") {
    UpdatedCards f;
    f.ready(10, "unravel");
    f.card(11, "balance", 1, Zone::Analysis);
    f.card(12, "balance");
    f.card(13, "spark");
    f.card(14, "reservoir", 0, Zone::Analysis);
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 11, 0});
    REQUIRE(e.state().cards.at(12).zone == Zone::Hand);
    REQUIRE(e.state().players[0].mana == 10);
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().decision->kind == DecisionKind::EffectDiscard);
    REQUIRE(hasOption(e, 12));
    REQUIRE_FALSE(hasOption(e, 13));
    REQUIRE_FALSE(hasOption(e, 14));
    choice(e, 12);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).zone == Zone::Ash);
}
TEST_CASE("shatter ring cannot destroy without its discard or an available target") {
    UpdatedCards f;
    f.ready(10, "unravel");
    f.card(11, "balance", 1, Zone::Analysis);
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 11, 0});
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().cards.at(11).zone == Zone::Analysis);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    f.card(12, "balance");
    e = f.engine();
    submit(e, 0, PrepareCast{10, 11, 0});
    auto state = e.state();
    StateMaintenance::leave(state, 11, &f.content.catalog);
    e = GameEngine::scenario(f.content.catalog, state);
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(12).zone == Zone::Hand);
}
TEST_CASE("arc formation prevents opponent destruction but permits owner destruction and gives no rings") {
    UpdatedCards f;
    f.card(10, "reservoir", 1, Zone::Analysis);
    Trigger t;
    t.owner = 0;
    t.targetCard = 10;
    REQUIRE(Rules::targetValid(f.state, TargetKind::EmptyEnemyFormation, 0, 10));
    EffectResolver::apply(f.state, f.content.catalog, t, {EffectKind::Destroy, 0});
    REQUIRE(f.state.cards.at(10).zone == Zone::Analysis);
    REQUIRE(Rules::rings(f.state, f.content.catalog, 10) == 0);
    t.owner = 1;
    EffectResolver::apply(f.state, f.content.catalog, t, {EffectKind::Destroy, 0});
    REQUIRE(f.state.cards.at(10).zone == Zone::Ash);
    f.card(11, "reservoir", 0, Zone::Analysis);
    f.card(12, "mend");
    auto e = f.engine();
    REQUIRE_FALSE(e.submit(0, StartAnalysis{12, 11}).accepted);
    auto deck = PlayerDeck{};
    deck.cards = f.content.deck;
    deck.baseFormation = "reservoir";
    REQUIRE_FALSE(Rules::deckErrors(f.content.catalog, deck).empty());
}
TEST_CASE("inverted triangle income applies only when attached to a formation") {
    UpdatedCards f;
    f.card(10, "conduit");
    f.state.players[0].mana = 1;
    auto e = f.engine();
    submit(e, 0, AttachSeal{10, 1});
    REQUIRE(e.state().players[0].mana == 0);
    reach(e, 1, Phase::Main);
    reach(e, 0, Phase::Prepare);
    REQUIRE(e.state().players[0].mana == 4);
    f.card(11, "mend");
    e = f.engine();
    submit(e, 0, StartAnalysis{11, 1});
    auto s = e.state();
    s.players[0].mana = 1;
    e = GameEngine::scenario(f.content.catalog, s);
    submit(e, 0, AttachSeal{10, 11});
    reach(e, 1, Phase::Main);
    reach(e, 0, Phase::Prepare);
    REQUIRE(e.state().players[0].mana == 2);
}
TEST_CASE("ring refunds actual setting payment rather than preparation payment once") {
    UpdatedCards f;
    f.ready(10);
    f.card(11, "ring", 0, Zone::Attached).host = 10;
    f.state.players[0].mana = 4;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().players[0].mana == 6);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    f.state.cards.at(10).settingPaid = 2;
    e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().players[0].mana == 5);
}
TEST_CASE("ring can attach to preloaded words and refunds their setting payment on successful response") {
    UpdatedCards f;
    f.ready(10);
    f.card(11, "barbs", 1, Zone::Words).analysisLoad = 5;
    f.state.cards.at(11).settingPaid = 5;
    f.card(12, "ring", 1, Zone::Attached).host = 11;
    f.state.players[1].mana = 2;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 11);
    REQUIRE(e.state().players[1].mana == 7);
    REQUIRE(e.state().cards.at(12).zone == Zone::Ash);
    f.state.active = 1;
    f.card(12, "ring", 1, Zone::Hand);
    e = f.engine();
    submit(e, 1, AttachSeal{12, 11});
    REQUIRE(e.state().cards.at(12).host == 11);
}
TEST_CASE("recall draws before its new load and checks overload after its full effect") {
    UpdatedCards f;
    f.card(10, "recall");
    const auto capacity = Rules::capacity(f.state, f.content.catalog, 0);
    f.state.temporary.push_back({1, 0, 0, capacity, 3});
    auto e = f.engine();
    submit(e, 0, PlayAction{10, 0});
    REQUIRE(e.state().players[0].deck.size() == 18);
    REQUIRE(e.state().players[0].mana == 9);
    REQUIRE(Rules::load(e.state(), 0) == capacity + 1);
    REQUIRE(e.state().result == 1);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    f.state.temporary.clear();
    e = f.engine();
    submit(e, 0, PlayAction{10, 0});
    REQUIRE(Rules::load(e.state(), 0) == 1);
    reach(e, 1, Phase::Main);
    REQUIRE(Rules::load(e.state(), 0) == 0);
}
TEST_CASE(
    "still mind discards at resolution and clears every temporary source but no independent or bound load") {
    UpdatedCards f;
    f.card(10, "clarity");
    f.card(11, "spark");
    f.ready(12, "mend");
    f.state.temporary = {{1, 0, 0, 2, 3}, {2, 0, 0, 3, 3}, {3, 0, 0, 2, 0, true}, {4, 1, 0, 4, 3}};
    auto e = f.engine();
    submit(e, 0, PlayAction{10, 0});
    REQUIRE(e.state().decision->kind == DecisionKind::EffectDiscard);
    REQUIRE(Rules::load(e.state(), 0) == 8);
    choice(e, 11);
    REQUIRE(Rules::load(e.state(), 0) == 3);
    REQUIRE(Rules::load(e.state(), 1) == 4);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("still mind without another hand card cannot perform its dependent load removal") {
    UpdatedCards f;
    f.card(10, "clarity");
    f.state.temporary.push_back({1, 0, 0, 4, 3});
    auto e = f.engine();
    submit(e, 0, PlayAction{10, 0});
    REQUIRE(Rules::load(e.state(), 0) == 4);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
}
TEST_CASE("disruption gives four temporary load and expires at the target next end") {
    UpdatedCards f;
    f.card(10, "disrupt");
    auto e = f.engine();
    submit(e, 0, PlayAction{10, 0});
    REQUIRE(Rules::load(e.state(), 1) == 4);
    REQUIRE(e.state().temporary.front().expiryTurn == 3);
    reach(e, 1, Phase::Main);
    REQUIRE(Rules::load(e.state(), 1) == 4);
    reach(e, 0, Phase::Main);
    REQUIRE(Rules::load(e.state(), 1) == 0);
}
TEST_CASE("concentration persists through end and requires payment before prepare income") {
    UpdatedCards f;
    f.ready(10, "ward");
    f.state.players[0].mana = 4;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    reach(e, 0, Phase::Cast);
    choice(e, 10);
    REQUIRE(e.state().cards.at(10).spell == SpellState::Active);
    REQUIRE(e.state().players[1].life == 18);
    reach(e, 1, Phase::Main);
    REQUIRE(e.state().cards.at(10).zone == Zone::Casting);
    reach(e, 0, Phase::Prepare);
    REQUIRE(e.state().decision->kind == DecisionKind::Concentration);
    REQUIRE(e.state().players[0].mana == 3);
    REQUIRE(Rules::load(e.state(), 0) == 3);
    choice(e, 10);
    REQUIRE(e.state().players[0].mana == 4);
    REQUIRE(Rules::load(e.state(), 0) == 4);
    REQUIRE(e.state().players[1].life == 18);
    reach(e, 0, Phase::Cast);
    REQUIRE(e.state().players[1].life == 17);
    reach(e, 1, Phase::Main);
    reach(e, 0, Phase::Prepare);
    choice(e, 0);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(Rules::load(e.state(), 0) == 0);
}
TEST_CASE("concentration cannot maintain with insufficient pre-income mana or losing load") {
    for (bool full : {false, true}) {
        UpdatedCards f;
        f.active(10);
        f.state.phase = Phase::Draw;
        f.state.phaseStep = PhaseStep::Body;
        if (full)
            f.state.temporary.push_back({1, 0, 0, 5 + f.content.catalog.baseLoadCapacity, 3});
        else
            f.state.players[0].mana = 0;
        auto e = f.engine();
        advance(e);
        REQUIRE(e.state().decision->kind == DecisionKind::Concentration);
        REQUIRE(e.state().decision->options.empty());
        auto before = e.digest();
        REQUIRE_FALSE(e.submit(0, Choose{e.state().decision->id, 10}).accepted);
        REQUIRE(e.digest() == before);
        choice(e, 0);
        REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
        REQUIRE(e.state().result == -1);
        if (!full)
            REQUIRE(e.state().players[0].mana == 2);
    }
}
TEST_CASE("concentration accepts exactly capacity and removes all its accumulated load on destruction") {
    UpdatedCards f;
    auto &c = f.active(10);
    c.castLoad = 4 + f.content.catalog.baseLoadCapacity;
    f.state.temporary.push_back({1, 0, 10, 1, 0, true});
    f.state.phase = Phase::Draw;
    auto e = f.engine();
    advance(e);
    REQUIRE(hasOption(e, 10));
    choice(e, 10);
    REQUIRE(Rules::load(e.state(), 0) == Rules::capacity(e.state(), f.content.catalog, 0));
    reach(e, 0, Phase::Main);
    submit(e, 0, Abandon{10});
    REQUIRE(Rules::load(e.state(), 0) == 0);
}
TEST_CASE("new successful concentration replaces old spell and its attached seal") {
    UpdatedCards f;
    f.active(10);
    f.card(11, "ring", 0, Zone::Attached).host = 10;
    f.ready(12, "ward");
    auto e = f.engine();
    submit(e, 0, PrepareCast{12, 0, 0});
    reach(e, 0, Phase::Cast);
    REQUIRE(e.state().players[1].life == 19);
    choice(e, 12);
    REQUIRE(e.state().cards.at(10).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).spell == SpellState::Active);
    REQUIRE(e.state().players[1].life == 17);
    REQUIRE(Rules::load(e.state(), 0) == 3);
}
TEST_CASE("barbs grants optional paid preparation out of turn after the current chain") {
    UpdatedCards f;
    f.ready(10, "fireball");
    f.ready(11, "mend", 1);
    auto &b = f.card(12, "barbs", 1, Zone::Words);
    b.analysisLoad = b.settingPaid = 5;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 12);
    REQUIRE(e.state().decision->kind == DecisionKind::EffectPrepare);
    REQUIRE(e.state().decision->player == 1);
    auto view = e.viewFor(1);
    auto a = std::find_if(view.actions.begin(), view.actions.end(), [](const LegalAction &a) {
        return std::holds_alternative<PrepareCast>(a.command);
    });
    REQUIRE(a != view.actions.end());
    const auto cmd = std::get<PrepareCast>(a->command);
    REQUIRE(cmd.card == 11);
    REQUIRE(cmd.decision == e.state().decision->id);
    REQUIRE(encodeCommand(decodeCommand(encodeCommand(cmd))) == encodeCommand(cmd));
    auto before = e.digest();
    auto stale = cmd;
    ++stale.decision;
    REQUIRE_FALSE(e.submit(1, stale).accepted);
    REQUIRE(e.digest() == before);
    REQUIRE_FALSE(e.submit(1, Choose{cmd.decision, 11}).accepted);
    REQUIRE(e.digest() == before);
    submit(e, 1, cmd);
    REQUIRE(e.state().cards.at(10).zone == Zone::Analysis);
    REQUIRE(e.state().cards.at(10).canceledTurn == 2);
    REQUIRE(e.state().cards.at(11).zone == Zone::Casting);
    REQUIRE(e.state().players[1].mana == 9);
    REQUIRE(e.state().cards.at(12).zone == Zone::Ash);
    REQUIRE(e.state().deferredPreparations.empty());
    int closed = -1, opened = -1;
    for (std::size_t n = 0; n < e.state().events.size(); ++n) {
        if (e.state().events[n].kind == "chain_closed" && closed < 0)
            closed = static_cast<int>(n);
        if (e.state().events[n].kind == "chain_open" && closed >= 0) {
            opened = static_cast<int>(n);
            break;
        }
    }
    REQUIRE(opened > closed);
    REQUIRE(e.state().phase == Phase::Main);
    REQUIRE(e.state().active == 0);
    reach(e, 1, Phase::Cast);
    choice(e, 11);
    REQUIRE(e.state().players[1].life == 26);
}
TEST_CASE("barbs optional preparation may be declined and obeys readiness and costs") {
    UpdatedCards f;
    f.ready(10);
    f.ready(11, "mend", 1);
    f.card(12, "barbs", 1, Zone::Words).analysisLoad = 5;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 12);
    choice(e, 0);
    REQUIRE(e.state().cards.at(11).zone == Zone::Analysis);
    REQUIRE(e.state().players[1].mana == 10);
    f.state.players[1].mana = 0;
    e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 12);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(11).zone == Zone::Analysis);
    f.state.players[1].mana = 10;
    f.state.cards.at(11).spell = SpellState::Analyzing;
    e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 12);
    REQUIRE_FALSE(e.state().decision);
    REQUIRE(e.state().cards.at(11).spell == SpellState::Analyzing);
}
TEST_CASE("barbs granted preparation opens an independent response window and can itself be canceled") {
    UpdatedCards f;
    f.ready(10, "mend");
    f.ready(11, "mend", 1);
    f.card(12, "barbs", 1, Zone::Words).analysisLoad = 5;
    f.card(13, "barbs", 0, Zone::Words).analysisLoad = 5;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 12);
    auto v = e.viewFor(1);
    auto i = std::find_if(v.actions.begin(), v.actions.end(), [](const LegalAction &a) {
        return std::holds_alternative<PrepareCast>(a.command);
    });
    REQUIRE(i != v.actions.end());
    submit(e, 1, i->command);
    REQUIRE(e.state().decision->kind == DecisionKind::Response);
    REQUIRE(e.state().decision->player == 0);
    REQUIRE(e.state().chain->initiator == 1);
    REQUIRE(e.state().chain->links.size() == 1);
    choice(e, 13);
    REQUIRE(e.state().cards.at(11).zone == Zone::Analysis);
    REQUIRE(e.state().cards.at(11).castLoad == 0);
    REQUIRE(e.state().players[1].mana == 9);
    REQUIRE(e.state().cards.at(11).canceledTurn == 2);
    REQUIRE(e.state().cards.at(13).zone == Zone::Ash);
    REQUIRE(e.state().deferredPreparations.empty());
}
TEST_CASE("word concentration uses its actual setting cost and replaces analytic concentration") {
    UpdatedCards f;
    auto word = f.content.catalog.at("barbs");
    word.id = word.name = "focus_word";
    word.cost = 2;
    word.concentration = true;
    word.target = TargetKind::Opponent;
    word.responses[0].effects = {{EffectKind::CancelPreparation, 0}};
    word.onCast = {{EffectKind::Damage, 1}};
    f.content.catalog.cards[word.id] = word;
    f.ready(10, "mend");
    f.active(11, 1);
    auto &c = f.card(12, word.id, 1, Zone::Words);
    c.analysisLoad = c.settingPaid = 2;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 12);
    REQUIRE(e.state().cards.at(11).zone == Zone::Ash);
    REQUIRE(e.state().cards.at(12).spell == SpellState::Active);
    REQUIRE(e.state().cards.at(12).concentrationCost == 2);
    reach(e, 1, Phase::Prepare);
    choice(e, 12);
    REQUIRE(e.state().cards.at(12).castLoad == 2);
    REQUIRE(e.state().players[1].mana == 10);
    reach(e, 1, Phase::Cast);
    REQUIRE(e.state().players[0].life == 19);
}
TEST_CASE("updated content rejects invalid immediate concentration and attachment declarations") {
    const auto source = readJson(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets/cards.json");
    auto index = [&](const char *id) {
        for (std::size_t n = 0; n < source["cards"].size(); ++n)
            if (source["cards"][n]["id"] == id)
                return n;
        return source["cards"].size();
    };
    auto bad = source;
    bad["cards"][index("spark")]["rank"] = 1;
    REQUIRE_THROWS(parseCatalog(bad));
    bad = source;
    bad["cards"][index("ward")]["duration"] = 2;
    REQUIRE_THROWS(parseCatalog(bad));
    bad = source;
    bad["cards"][index("ward")]["onPrepare"] = Json::array({{{"kind", "damage"}, {"amount", 1}}});
    REQUIRE_THROWS(parseCatalog(bad));
    bad = source;
    bad["cards"][index("fireball")]["incomeBonus"] = 1;
    REQUIRE_THROWS(parseCatalog(bad));
    bad = source;
    bad["cards"][index("ring")]["refundCast"] = 1;
    REQUIRE_THROWS(parseCatalog(bad));
}
TEST_CASE("new resolution choices and preparation commands retain exact interface decision identities") {
    UpdatedCards f;
    f.ready(10);
    f.ready(11, "unravel", 1);
    f.card(13, "balance", 0, Zone::Analysis);
    f.card(12, "barbs", 1, Zone::Words).analysisLoad = 5;
    auto e = f.engine();
    submit(e, 0, PrepareCast{10, 0, 0});
    choice(e, 12);
    auto v = e.viewFor(1);
    ui::Interaction interaction;
    interaction.update(v);
    REQUIRE(interaction.select(11));
    REQUIRE_FALSE(interaction.groups().empty());
    REQUIRE(interaction.activate(0));
    REQUIRE(interaction.step() == ui::Step::Target);
    REQUIRE(interaction.pick(13));
    REQUIRE(interaction.step() == ui::Step::Confirm);
    auto offered = *interaction.pending();
    REQUIRE(std::get<PrepareCast>(offered.command).decision == v.decision->id);
    auto next = v;
    ++next.decision->id;
    for (auto &a : next.actions)
        if (auto c = std::get_if<PrepareCast>(&a.command))
            ++c->decision;
    interaction.update(next);
    interaction.offer(offered);
    REQUIRE_FALSE(interaction.pending());
    submit(e, 1, offered.command);
    REQUIRE(e.state().cards.at(11).zone == Zone::Casting);
}
