#include "wizard/sound.hpp"
#include "wizard/content.hpp"
#include <algorithm>
#include <catch2/catch_test_macros.hpp>
using namespace wizard;
using namespace wizard::ui;
namespace {
GameView view() {
    GameView v;
    v.viewer = v.active = 0;
    v.result = -1;
    for (auto &p : v.players) {
        p.life = 20;
        p.deckCount = 25;
        p.handCount = 5;
    }
    return v;
}
bool has(const std::vector<SoundId> &sounds, SoundId id) {
    return std::find(sounds.begin(), sounds.end(), id) != sounds.end();
}
} // namespace
TEST_CASE("sound consumption is structural incremental and private-view safe") {
    auto a = view(), b = a;
    GameEvent damage{"effect", "任意本地化文本", -1, 7, 5};
    damage.effectType = EffectKind::Damage;
    damage.actualAmount = 5;
    b.events.push_back(damage);
    REQUIRE(matchSounds(a, b, true) == std::vector<SoundId>{SoundId::Damage});
    REQUIRE(matchSounds(b, b, true).empty());
    b.events.back().audience = 1;
    REQUIRE(matchSounds(a, b, true).empty());
    b.events.back().audience = -1;
    b.events.back().actualAmount = 0;
    REQUIRE(matchSounds(a, b, true).empty());
    b.viewer = 1;
    REQUIRE(matchSounds(a, b, true).empty());
}
TEST_CASE("release success and actual impacts do not infer from cancellation or payment prose") {
    auto a = view(), b = a;
    b.events = {{"pay", "释放法术并造成伤害", -1, 7}, {"link_skipped", "释放成功", -1, 7}};
    REQUIRE(matchSounds(a, b, true).empty());
    b.events.push_back({"link_resolved", "not localized", -1, 7});
    b.events.back().spellReleased = true;
    REQUIRE(matchSounds(a, b, true) == std::vector<SoundId>{SoundId::SpellRelease});
}
TEST_CASE("sound merging preserves damage and healing within the same net-zero transition") {
    auto a = view(), b = a;
    for (auto kind : {EffectKind::Damage, EffectKind::Damage, EffectKind::Heal}) {
        GameEvent event{"effect", "", -1, 7, 3};
        event.effectType = kind;
        event.actualAmount = 3;
        b.events.push_back(event);
    }
    REQUIRE(matchSounds(a, b, true) == std::vector<SoundId>{SoundId::Damage, SoundId::Heal});
}
TEST_CASE("only human response decisions sound once and terminal outcome is exclusive") {
    auto a = view(), b = a;
    b.decision = PendingDecision{42, 0, DecisionKind::Response, {7}, true};
    REQUIRE(has(matchSounds(a, b, true), SoundId::ResponseOpen));
    REQUIRE(matchSounds(a, b, false).empty());
    REQUIRE(matchSounds(b, b, true).empty());
    for (int result = 0; result < 3; ++result) {
        b.result = result;
        const auto expected = result == 0   ? SoundId::Victory
                              : result == 1 ? SoundId::Defeat
                                            : SoundId::DrawResult;
        REQUIRE(matchSounds(a, b, true) == std::vector<SoundId>{expected});
        REQUIRE(matchSounds(b, b, true).empty());
    }
}
TEST_CASE("draw discard and readiness distinguish real card transitions from played actions") {
    auto a = view(), b = a;
    CardView card;
    card.instance.id = 7;
    card.instance.zone = Zone::Hand;
    a.cards = {card};
    card.instance.zone = Zone::Ash;
    b.cards = {card};
    b.players[0].handCount--;
    REQUIRE(has(matchSounds(a, b, true), SoundId::DiscardCard));
    b.events.push_back({"command", "", -1, 7});
    REQUIRE_FALSE(has(matchSounds(a, b, true), SoundId::DiscardCard));
    b.players[1].deckCount -= 2;
    b.players[1].handCount += 2;
    REQUIRE(has(matchSounds(a, b, true), SoundId::DrawCard));
    a.cards[0].instance.zone = b.cards[0].instance.zone = Zone::Analysis;
    a.cards[0].instance.spell = SpellState::Analyzing;
    b.cards[0].instance.spell = SpellState::Ready;
    REQUIRE(has(matchSounds(a, b, true), SoundId::AnalysisReady));
}
TEST_CASE("real effect metadata captures capped healing and actual draw independently of text") {
    auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets");
    GameState s;
    Trigger t;
    t.owner = 0;
    s.players[0].life = maximumLife - 1;
    EffectResolver::apply(s, content.catalog, t, {EffectKind::Heal, 6});
    REQUIRE(s.events.back().effectType == EffectKind::Heal);
    REQUIRE(s.events.back().actualAmount == 1);
    REQUIRE(s.events.back().affectedPlayer == 0);
    EffectResolver::apply(s, content.catalog, t, {EffectKind::Heal, 6});
    REQUIRE(s.events.back().actualAmount == 0);
    EffectResolver::apply(s, content.catalog, t, {EffectKind::Draw, 2});
    REQUIRE(s.events.back().actualAmount == 0);
    REQUIRE(s.players[0].drawFailed);
}
