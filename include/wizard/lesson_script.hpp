#pragma once
#include "wizard/ai.hpp"
#include "wizard/text_catalog.hpp"
#include "wizard/tutorial_scenario.hpp"
#include <memory>

namespace wizard::app {
namespace detail {
struct LessonProgram;
}

class LessonDefinition {
public:
    TutorialScenario scenario;
    int order;
    std::string titleKey;

private:
    friend class LessonCatalog;
    friend class LessonScript;
    LessonDefinition(TutorialScenario, int, std::string, std::shared_ptr<const detail::LessonProgram>);
    std::shared_ptr<const detail::LessonProgram> program_;
};

class LessonCatalog {
public:
    static LessonCatalog load(const std::filesystem::path &directory, const CardCatalog &, const TextCatalog &,
                              const TextCatalog::Diagnostic &diagnostic = {});
    const std::vector<LessonDefinition> &lessons() const { return lessons_; }
    const std::vector<Json> &editors() const { return editors_; }
    const LessonDefinition *find(std::string_view id) const;

private:
    std::vector<LessonDefinition> lessons_;
    std::vector<Json> editors_;
};

class LessonScript {
public:
    explicit LessonScript(const LessonDefinition &);

    // The application applies this filter to engine-validated actions.
    bool allows(const GameView &before, const Command &) const;
    // The application calls this only for an accepted engine result.
    void observe(PlayerId actor, const Command &, const GameView &before, const GameView &after,
                 const std::vector<GameEvent> &);
    bool canContinue(const GameView &) const;
    bool continueLesson(const GameView &);
    std::optional<ai::Selection> opponent(const GameView &, const ai::CandidateSource &) const;
    Json snapshot(const GameEngine &) const;
    bool complete() const { return complete_; }
    const std::string &blockedHint() const;

private:
    void advance(std::size_t next);
    bool hasContinue() const;

    std::string lessonId_;
    std::shared_ptr<const detail::LessonProgram> program_;
    std::size_t step_{};
    std::vector<bool> opponentUsed_;
    std::optional<SubmitOpeningGesture> learnerGesture_;
    bool awaitingCompletion_{};
    bool terminal_{};
    bool complete_{};
};
} // namespace wizard::app
