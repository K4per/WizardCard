#pragma once
#include "wizard/application.hpp"
#include "wizard/interaction.hpp"
#include <optional>
#include <memory>

namespace wizard::bridge {
// Engine-independent whitelist DTOs. uint64 values NEVER cross the scripting boundary as numbers.
Json commandDto(const Command &);
Json viewDto(const GameView &);
Json cardDefinitionDto(const CardDefinition &);

class LocalSession {
  public:
    explicit LocalSession(RoomFactory roomFactory = {}) : roomFactory_(std::move(roomFactory)) {}
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
    Json createDraft(const std::string &preset, int format = 1) const;
    Json validateDraft(const Json &) const;
    Json saveDraft(const Json &);
    Json eraseDraft(const std::string &);
    Json query(const Json &) const;
    Json applySettings(const Json &);
    Json leaveMatch(PlayerId);
    Json interact(PlayerId, const Json &, const std::string &generation, const std::string &revision);
    Json networkRequest(const Json &request);
    Json pollNetwork();
    // Explicit OFFLINE verification tools, not part of the player snapshot or network protocol.
    Json readReplayPlan(const std::filesystem::path &path) const;
    Json verifyReplay(const std::filesystem::path &path) const;
    Json replayFrame(const std::filesystem::path &path, std::size_t step, PlayerId viewer) const;
    std::uint64_t generation() const { return generation_; }

  private:
    struct Pending { PlayerId viewer; Command command; std::uint64_t generation, revision; };
    std::optional<Content> content_;
    RoomFactory roomFactory_;
    std::unique_ptr<app::Application> application_;
    std::optional<Pending> pending_;
    std::uint64_t generation_{};
    ui::Interaction interaction_;
    std::uint64_t interactionGeneration_{}, interactionRevision_{};
    PlayerId interactionViewer_{-1};
    // Replay browsing keeps one validated engine and advances it between frames.
    // Without this cache every next/previous click rebuilt the whole recording.
    mutable std::filesystem::path replayCachePath_;
    mutable Json replayCacheRecord_;
    mutable std::unique_ptr<GameEngine> replayCacheEngine_;
    mutable std::filesystem::file_time_type replayCacheWriteTime_{};
    mutable std::uintmax_t replayCacheFileSize_{};
    mutable std::size_t replayCacheStep_{};
    mutable bool replayCacheValid_{};
    mutable std::map<std::size_t, GameEngine> replayCheckpoints_;
    void prepareReplay(const std::filesystem::path &) const;
    bool visibleTo(PlayerId viewer) const;
    bool current(const std::string &generation, const std::string &revision) const;
    std::string actionId(PlayerId viewer, std::size_t index) const;
    GameView view(PlayerId viewer) const;
    Json accepted(const CommandResult &, const GameView &before);
};
} // namespace wizard::bridge
