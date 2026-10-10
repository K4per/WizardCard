#pragma once
#include "wizard/core/view.hpp"

namespace wizard::ui {
enum class SoundId {
    Click,
    Select,
    Cancel,
    Reject,
    Place,
    Confirm,
    DrawCard,
    DiscardCard,
    TurnReady,
    PhaseChange,
    AnalysisReady,
    ResponseOpen,
    SpellRelease,
    Damage,
    Heal,
    Victory,
    Defeat,
    DrawResult
};
const char *soundFile(SoundId);
float soundGain(SoundId);
int soundPriority(SoundId);
float soundCooldown(SoundId);
// Views must share a viewer. Only newly appended public/own events are consumed.
// humanDecision is false while the AI controls the current decision.
std::vector<SoundId> matchSounds(const GameView &before, const GameView &after, bool humanDecision);
} // namespace wizard::ui
