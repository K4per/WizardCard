#pragma once
#include "wizard/application.hpp"
#include "wizard/interaction.hpp"
#include <optional>

namespace wizard::bridge {
// Engine-independent whitelist DTOs. uint64 values NEVER cross the scripting boundary as numbers.
Json commandDto(const Command &);
Json viewDto(const GameView &);
Json cardDefinitionDto(const CardDefinition &);

class LocalSession {
  public:
    Json initialize(const std::filesystem::path &assets, const std::filesystem::path &userDirectory);
    Json start(const Json &options);
    Json snapshot(PlayerId viewer) const;
    Json selectAction(PlayerId viewer, const std::string &actionId,
                      const std::string &generation, const std::string &revision);
    Json confirmAction(const std::string &generation, const std::string &revision);
    Json cancelAction();
    Json pause(bool paused);
    Json stepAi(const std::string &generation, const std::string &revision);
    Json continueTutorial(const std::string &generation, const std::string &revision);
    Json saveReplay() const;
    Json restart(std::uint32_t seed);
    Json release();
    Json library() const;
    Json createDraft(const std::string &preset) const;
    Json validateDraft(const Json &) const;
    Json saveDraft(const Json &);
    Json eraseDraft(const std::string &);
    Json query(const Json &) const;
    Json applySettings(const Json &);
    Json leaveMatch(PlayerId);
    Json interact(PlayerId, const Json &, const std::string &generation, const std::string &revision);
    // Explicit OFFLINE verification tools, not part of the player snapshot or network protocol.
    Json readReplayPlan(const std::filesystem::path &path) const;
    Json verifyReplay(const std::filesystem::path &path) const;
    std::uint64_t generation() const { return generation_; }

  private:
    struct Pending { PlayerId viewer; Command command; std::uint64_t generation, revision; };
    std::optional<Content> content_;
    std::unique_ptr<app::Application> application_;
    std::optional<Pending> pending_;
    std::uint64_t generation_{};
    ui::Interaction interaction_;
    std::uint64_t interactionGeneration_{}, interactionRevision_{};
    PlayerId interactionViewer_{-1};
    bool visibleTo(PlayerId viewer) const;
    bool current(const std::string &generation, const std::string &revision) const;
    std::string actionId(PlayerId viewer, std::size_t index) const;
    GameView view(PlayerId viewer) const;
    Json accepted(const CommandResult &, const GameView &before);
};
} // namespace wizard::bridge
