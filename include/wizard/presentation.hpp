#pragma once
#include "wizard/core.hpp"
#include <cstdint>

namespace wizard::ui {
enum class CueKind { Move, Draw, Ready, Prepare, Release, Response, Cancel, Damage, Heal, Mana, Load, Phase };
struct MatchCue {
    std::uint64_t id{};
    CueKind kind{};
    CardId source{}, host{}, target{};
    PlayerId player{};
    Zone from{Zone::Hand}, to{Zone::Hand};
    std::optional<CardView> card;
    std::string text;
    int amount{};
    bool used{};
    float age{}, duration{.75f};
    float progress() const;
    bool visible() const;
};
// Presentation only: two views of the same viewer, no engine or unfiltered command events.
class MatchPresentation {
  public:
    void observe(const GameView &before, const GameView &after, bool reducedMotion = false);
    void update(float seconds);
    void clear();
    void sampleLatest(float seconds); // Deterministic capture of an actual submitted transition.
    bool busy() const;
    const std::vector<MatchCue> &cues() const {
        return cues_;
    }

  private:
    std::vector<MatchCue> cues_;
    std::uint64_t next_{1}, latest_{};
};
} // namespace wizard::ui
