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
inline constexpr int schemaVersion = 2;
Json eventDto(const GameEvent &);
Json clockDto(const MatchClockView &);

class LocalSession {
  public:
    explicit LocalSession(RoomFactory roomFactory = {}) : roomFactory_(std::move(roomFactory)) {}
    Json initialize(const std::filesystem::path &assets, const std::filesystem::path &userDirectory);
    Json start(const Json &options);
    Json snapshot(PlayerId viewer) const;
    Json queryActions(PlayerId viewer, const Json &intent, const std::string &generation, const std::string &revision);
    Json selectIntent(PlayerId viewer, const Json &intent, const std::string &generation, const std::string &revision);
    Json submitCommand(PlayerId viewer, const Json &command, const std::string &generation, const std::string &revision);
    Json eventsSince(PlayerId viewer, const std::string &epoch, const std::string &cursor) const;
    Json pollMatch(PlayerId viewer, const std::string &generation, const std::string &revision);
    Json saveDiagnostic() const;
    Json confirmAction(const std::string &generation, const std::string &revision);
    Json cancelAction();
    Json debugCommand(const Json &);
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
    std::uint64_t generation() const { return match_.generation; }

  private:
    struct Pending { PlayerId viewer; Command command; std::uint64_t generation, revision; };
    struct DeckWorkspace {
        std::optional<Content> content;
        mutable std::optional<Json> libraryCache;
    } decks_;
    struct MatchWorkspace {
        std::unique_ptr<app::Application> application;
        std::optional<Pending> pending;
        std::uint64_t generation{};
        ui::Interaction interaction;
        std::uint64_t interactionGeneration{}, interactionRevision{};
        PlayerId interactionViewer{-1};
    } match_;
    struct ReplayBrowser {
        std::filesystem::path path;
        Json record;
        std::unique_ptr<GameEngine> engine;
        std::filesystem::file_time_type writeTime{};
        std::uintmax_t fileSize{};
        std::size_t step{};
        bool valid{}, browsing{};
        std::map<std::size_t, GameEngine> checkpoints;
        std::map<std::size_t, std::array<std::vector<GameEvent>, 2>> eventCheckpoints;
        std::array<std::vector<GameEvent>, 2> visibleEvents;
    };
    mutable ReplayBrowser replay_;
    // Full replay writes are deliberately batched. A failed write keeps the
    // session dirty so each following command retries until persistence works.
    mutable bool replaySaveHealthy_{true};
    RoomFactory roomFactory_;
    void prepareReplay(const std::filesystem::path &) const;
    bool visibleTo(PlayerId viewer) const;
    bool current(const std::string &generation, const std::string &revision) const;
    GameView view(PlayerId viewer) const;
    Json accepted(const CommandResult &, const GameView &before);
};
} // namespace wizard::bridge
