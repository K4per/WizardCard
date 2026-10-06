#include "wizard/sound.hpp"
#include <algorithm>
#include <array>

namespace wizard::ui {
const char *soundFile(SoundId id) {
    return std::array<const char *, 18>{
        "ui_click.mp3",       "card_select.mp3", "ui_cancel.mp3",     "ui_reject.mp3",  "card_place.mp3",
        "confirm.mp3",        "card_draw.mp3",   "card_discard.mp3",  "turn_ready.mp3", "phase_change.mp3",
        "analysis_ready.mp3", "response.mp3",    "spell_release.mp3", "damage.mp3",     "heal.mp3",
        "victory.mp3",        "defeat.mp3",      "confirm.mp3"}
        .at(static_cast<std::size_t>(id));
}
float soundGain(SoundId id) {
    if (id == SoundId::Click || id == SoundId::Select || id == SoundId::Place)
        return .45f;
    if (id == SoundId::PhaseChange || id == SoundId::Confirm || id == SoundId::DrawCard)
        return .55f;
    return .8f;
}
int soundPriority(SoundId id) {
    if (id == SoundId::Victory || id == SoundId::Defeat || id == SoundId::DrawResult)
        return 3;
    if (id == SoundId::Damage || id == SoundId::Heal || id == SoundId::SpellRelease ||
        id == SoundId::ResponseOpen || id == SoundId::TurnReady)
        return 2;
    if (id == SoundId::Click || id == SoundId::Select || id == SoundId::Place || id == SoundId::Confirm)
        return 0;
    return 1;
}
float soundCooldown(SoundId id) {
    if (id == SoundId::PhaseChange)
        return 1.1f;
    if (id == SoundId::TurnReady)
        return .5f;
    return .09f;
}
std::vector<SoundId> matchSounds(const GameView &before, const GameView &after, bool humanDecision) {
    std::vector<SoundId> out;
    if (before.viewer != after.viewer || after.events.size() < before.events.size())
        return out;
    auto add = [&](SoundId id) {
        if (std::find(out.begin(), out.end(), id) == out.end())
            out.push_back(id);
    };
    // A release precedes impact sounds even when the whole link completes in one submit.
    for (std::size_t n = before.events.size(); n < after.events.size(); ++n) {
        const auto &e = after.events[n];
        if ((e.audience < 0 || e.audience == after.viewer) && e.spellReleased)
            add(SoundId::SpellRelease);
    }
    for (std::size_t n = before.events.size(); n < after.events.size(); ++n) {
        const auto &e = after.events[n];
        if (e.audience >= 0 && e.audience != after.viewer)
            continue;
        if (e.effectType == EffectKind::Damage && e.actualAmount > 0)
            add(SoundId::Damage);
        if (e.effectType == EffectKind::Heal && e.actualAmount > 0)
            add(SoundId::Heal);
        if (e.effectType == EffectKind::Draw && e.actualAmount > 0)
            add(SoundId::DrawCard);
    }
    for (int p = 0; p < 2; ++p)
        if (after.players[p].deckCount < before.players[p].deckCount &&
            after.players[p].handCount > before.players[p].handCount)
            add(SoundId::DrawCard);
    for (const auto &c : after.cards) {
        if (c.instance.zone == Zone::Hand || c.instance.zone == Zone::Deck)
            continue;
        const auto old = std::find_if(before.cards.begin(), before.cards.end(),
                                      [&](const CardView &x) { return x.instance.id == c.instance.id; });
        if (old == before.cards.end()) {
            if (c.instance.owner != after.viewer && c.instance.zone == Zone::Ash &&
                after.players[c.instance.owner].handCount < before.players[c.instance.owner].handCount &&
                std::none_of(after.events.begin() + static_cast<std::ptrdiff_t>(before.events.size()),
                             after.events.end(), [&](const GameEvent &e) {
                                 return e.kind == "command" && e.card == c.instance.id;
                             }))
                add(SoundId::DiscardCard);
            continue;
        }
        if (old->instance.spell == SpellState::Analyzing && c.instance.spell == SpellState::Ready)
            add(SoundId::AnalysisReady);
        if (old->instance.zone == Zone::Hand && c.instance.zone == Zone::Ash) {
            const bool used =
                std::any_of(after.events.begin() + static_cast<std::ptrdiff_t>(before.events.size()),
                            after.events.end(), [&](const GameEvent &e) {
                                return e.kind == "command" && e.card == c.instance.id &&
                                       (e.audience < 0 || e.audience == after.viewer);
                            });
            if (!used)
                add(SoundId::DiscardCard);
        }
    }
    if (after.phase != before.phase || after.active != before.active)
        add(SoundId::PhaseChange);
    if (humanDecision && after.decision && after.decision->kind == DecisionKind::Response &&
        (!before.decision || before.decision->id != after.decision->id))
        add(SoundId::ResponseOpen);
    if (humanDecision && before.active != after.active && after.active == after.viewer)
        add(SoundId::TurnReady);
    if (before.result == -1 && after.result != -1) {
        // Results replace other feedback in the same batch; play exactly one result cue.
        out.clear();
        add(after.result == 2              ? SoundId::DrawResult
            : after.result == after.viewer ? SoundId::Victory
                                           : SoundId::Defeat);
    }
    return out;
}
} // namespace wizard::ui
