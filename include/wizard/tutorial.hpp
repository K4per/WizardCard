#pragma once
#include "wizard/ai.hpp"
#include "wizard/replay.hpp"

namespace wizard::app {
enum class Lesson {
    Intro,
    Resources,
    Formation,
    Rune,
    Analysis,
    Waiting,
    Preparation,
    Response,
    Retry,
    Release,
    Review,
    Complete
};
MatchConfig tutorialConfiguration(const Content &);
struct Tutorial {
    Lesson lesson{Lesson::Intro};
    CardId spell{};
    bool countered{};
    std::string title() const;
    std::string hint(const GameView &) const;
    bool canContinue(const GameView &) const;
    void continueLesson(const GameView &);
    bool allows(const GameView &, const Command &) const;
    void observe(PlayerId, const Command &, const GameView &after, const std::vector<GameEvent> &);
    std::optional<ai::Selection> opponent(const GameView &) const;
};
} // namespace wizard::app
