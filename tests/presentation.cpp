#include "wizard/presentation.hpp"
#include "wizard/application.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
#include <limits>
using namespace wizard;
using namespace wizard::ui;
namespace {
GameView baseView() {
    GameView v;
    v.viewer = 0;
    v.active = 0;
    v.result = -1;
    for (auto &p : v.players) {
        p.life = 20;
        p.mana = 5;
        p.capacity = 8;
        p.deckCount = 25;
    }
    return v;
}
CardView card(CardId id, PlayerId owner, Zone zone) {
    CardView c;
    c.instance.id = id;
    c.instance.owner = owner;
    c.instance.zone = zone;
    c.definition.name = "可见法术";
    c.definition.analysisTurns = 2;
    return c;
}
bool has(const MatchPresentation &p, CueKind kind) {
    return std::any_of(p.cues().begin(), p.cues().end(), [&](const MatchCue &c) { return c.kind == kind; });
}
} // namespace
TEST_CASE("presentation never copies an opponent hand or deck and uses anonymous draw counts") {
    auto before = baseView(), after = before;
    after.cards = {card(901, 1, Zone::Hand), card(902, 1, Zone::Deck)};
    after.cards[0].definition.name = "隐藏卡名";
    after.players[1].deckCount -= 2;
    after.players[1].handCount += 2;
    after.events.push_back({"draw", "隐藏抽牌文字", 1, 901});
    MatchPresentation p;
    p.observe(before, after);
    REQUIRE(p.cues().size() == 1);
    REQUIRE(p.cues()[0].kind == CueKind::Draw);
    REQUIRE(p.cues()[0].amount == 2);
    REQUIRE(p.cues()[0].source == 0);
    REQUIRE_FALSE(p.cues()[0].card);
    REQUIRE(p.cues()[0].text == "对手抽牌");
}
TEST_CASE("presentation distinguishes discard from an action used before moving to ash") {
    auto before = baseView(), after = before;
    before.cards = {card(1, 0, Zone::Hand), card(2, 0, Zone::Hand)};
    after.cards = {card(1, 0, Zone::Ash), card(2, 0, Zone::Ash)};
    after.events.push_back({"command", "使用行动", -1, 1});
    MatchPresentation p;
    p.observe(before, after);
    REQUIRE(p.cues().size() == 2);
    REQUIRE(p.cues()[0].used);
    REQUIRE_FALSE(p.cues()[1].used);
    REQUIRE(p.cues()[0].from == Zone::Hand);
    REQUIRE(p.cues()[0].to == Zone::Ash);
}
TEST_CASE("ready preparation response cancellation and release reflect submitted public transitions") {
    auto before = baseView(), after = before;
    before.cards = {card(4, 0, Zone::Analysis)};
    before.cards[0].instance.spell = SpellState::Analyzing;
    before.chain = ChainState{};
    ChainLink root;
    root.item.source = 4;
    before.chain->links.push_back(root);
    after.cards = before.cards;
    after.cards[0].instance.spell = SpellState::Ready;
    after.events = {{"pay", "准备支付 1 魔素", -1, 4},
                    {"link_declared", "响应链节 #2", -1, 4},
                    {"link_skipped", "取消或失效", -1, 4}};
    MatchPresentation p;
    p.observe(before, after);
    REQUIRE(has(p, CueKind::Ready));
    REQUIRE(has(p, CueKind::Prepare));
    REQUIRE(has(p, CueKind::Response));
    REQUIRE(has(p, CueKind::Cancel));
    for (const auto &cue : p.cues())
        if (cue.kind == CueKind::Response)
            REQUIRE(cue.target == 4);
    auto cast = after;
    cast.chain = ChainState{};
    ChainLink link;
    link.kind = LinkKind::Spell;
    link.item.source = 4;
    cast.chain->links.push_back(link);
    auto released = cast;
    released.chain.reset();
    released.cards[0].instance.zone = Zone::Ash;
    released.events.push_back({"link_resolved", "结算链节 #3", -1, 4});
    p.clear();
    p.observe(cast, released);
    REQUIRE(has(p, CueKind::Release));
    cast.chain.reset();
    cast.cards[0].instance.zone = Zone::Casting;
    cast.cards[0].instance.spell = SpellState::Pending;
    p.clear();
    p.observe(cast, released);
    REQUIRE(has(p, CueKind::Release)); // A whole release may resolve inside a single submit.
}
TEST_CASE("resource feedback uses actual projected deltas without inventing damage from event text") {
    auto before = baseView(), after = before;
    after.events.push_back({"effect", "造成 999 点伤害", -1, 0, 999});
    MatchPresentation p;
    p.observe(before, after);
    REQUIRE_FALSE(has(p, CueKind::Damage));
    after.players[1].life -= 10;
    after.players[0].mana -= 3;
    after.players[0].load += 3;
    p.observe(before, after);
    REQUIRE(has(p, CueKind::Damage));
    REQUIRE(has(p, CueKind::Mana));
    REQUIRE(has(p, CueKind::Load));
    for (const auto &c : p.cues())
        if (c.kind == CueKind::Damage)
            REQUIRE(c.amount == -10);
}
TEST_CASE("handoff and new match histories clear private animation snapshots") {
    auto before = baseView(), after = before;
    after.cards = {card(42, 0, Zone::Hand)};
    MatchPresentation p;
    p.observe(before, after);
    REQUIRE(p.busy());
    auto enemy = after;
    enemy.viewer = 1;
    p.observe(after, enemy);
    REQUIRE_FALSE(p.busy());
    p.observe(before, after);
    REQUIRE(p.busy());
    after.events.push_back({"draw", "抽牌", 0, 42});
    p.observe(after, before);
    REQUIRE_FALSE(p.busy());
}
TEST_CASE("animation clocks are bounded deterministic and expire without changing match inputs") {
    auto before = baseView(), after = before;
    after.players[0].mana += 2;
    after.phase = Phase::Main;
    MatchPresentation a, b;
    a.observe(before, after);
    b.observe(before, after);
    a.update(.2f);
    b.update(.1f);
    b.update(.1f);
    REQUIRE(a.cues().size() == b.cues().size());
    REQUIRE(a.cues()[0].progress() == b.cues()[0].progress());
    a.update(std::numeric_limits<float>::quiet_NaN());
    REQUIRE(a.cues()[0].age == b.cues()[0].age);
    a.update(-1);
    REQUIRE(a.cues()[0].age == b.cues()[0].age);
    for (int n = 0; n < 100; ++n)
        a.observe(before, after);
    REQUIRE(a.cues().size() <= 48);
    a.update(3);
    REQUIRE_FALSE(a.busy());
    REQUIRE(after.players[0].mana == 7);
    REQUIRE(after.cards.empty());
}
TEST_CASE("reduced motion preserves numeric and phase feedback without card flights") {
    auto before = baseView(), after = before;
    before.cards = {card(1, 0, Zone::Hand)};
    after.cards = {card(1, 0, Zone::Analysis)};
    after.phase = Phase::Main;
    after.players[0].mana -= 3;
    MatchPresentation p;
    p.observe(before, after, true);
    REQUIRE_FALSE(has(p, CueKind::Move));
    REQUIRE(has(p, CueKind::Mana));
    REQUIRE(has(p, CueKind::Phase));
    p.update(.3f);
    REQUIRE_FALSE(p.busy());
    auto settings = app::encodeSettings(app::Settings{});
    settings.erase("reducedMotion");
    REQUIRE_FALSE(app::decodeSettings(settings).reducedMotion);
    settings["reducedMotion"] = true;
    REQUIRE(app::decodeSettings(settings).reducedMotion);
    settings["reducedMotion"] = "yes";
    REQUIRE_THROWS(app::decodeSettings(settings));
}
