#pragma once
#include "wizard/replay.hpp"
#include "wizard/decks.hpp"
#include "wizard/room.hpp"
#include "wizard/tutorial.hpp"
#include <memory>

namespace wizard::app {
enum class Page {
    Menu,
    HotseatSetup,
    AiSetup,
    Decks,
    Settings,
    Match,
    ConfirmLeave,
    ConfirmSurrender,
    DeckEditor,
    ConfirmDeckDelete,
    ConfirmDeckDiscard,
    NetworkSetup,
    NetworkRoom
};
enum class WindowMode { Windowed, Borderless, Fullscreen };
struct Settings {
    int masterVolume{80}, soundVolume{80};
    WindowMode windowMode{WindowMode::Windowed};
    unsigned width{1600}, height{1000};
    bool automaticPhases{true};
    bool reducedMotion{};
    float effectiveSoundVolume() const;
};
Json encodeSettings(const Settings &);
Settings decodeSettings(const Json &);
std::filesystem::path userDataDirectory();
inline void atomicWriteJson(const std::filesystem::path &p, const Json &j) {
    writeAtomicJson(p, j);
}
struct DeckPreset {
    std::string id, name;
    PlayerDeck deck;
    std::string description;
};
std::vector<DeckPreset> loadPresets(const Content &, const std::filesystem::path &assets);

enum class MatchMode { Hotseat, Ai, Tutorial, Network };
struct SessionOptions {
    MatchMode mode{MatchMode::Hotseat};
    ai::Difficulty difficulty{ai::Difficulty::Normal};
};
struct AiPlan {
    std::uint64_t generation{}, revision{};
    ai::Selection selection;
};
// No window dependencies. AI receives only a projection; settings pause all submissions.
class Application {
  public:
    Application(Content, std::filesystem::path userDirectory, std::vector<DeckPreset> = {}, RoomFactory = {});
    Page page() const {
        return page_;
    }
    const Settings &settings() const {
        return settings_;
    }
    const std::string &notice() const {
        return notice_;
    }
    const std::filesystem::path &userDirectory() const {
        return directory_;
    }
    const std::filesystem::path &replayPath() const {
        return replayPath_;
    }
    const std::vector<DeckPreset> &presets() const {
        return presets_;
    }
    DeckLibrary &decks() {
        return decks_;
    }
    const DeckLibrary &decks() const {
        return decks_;
    }
    std::vector<DeckPreset> playableDecks() const;
    bool hasMatch() const {
        return bool(match_) || (network_ && network_->started());
    }
    bool paused() const {
        return page_ != Page::Match || !hasMatch() || (network_ && network_->blocked());
    }
    bool quitRequested() const {
        return quit_;
    }
    std::uint64_t generation() const {
        return generation_;
    }
    std::uint64_t revision() const { return revision_; }
    // Compatibility accessor for legacy offline tools; presentation uses viewFor.
    const MatchSession &match() const;
    GameView viewFor(PlayerId) const;
    PlayerId actingPlayer() const;
    RoomSession *network() {
        return network_.get();
    }
    const RoomSession *network() const {
        return network_.get();
    }
    bool startNetwork(bool host, const std::string &address, unsigned short port, const PlayerDeck &,
                      std::string &);
    bool tickNetwork();
    bool networkReset() const {
        return networkReset_;
    }
    bool leaveNetwork(std::string &, bool force = false);
    const MatchConfig &configuration() const {
        return configuration_;
    }
    void navigate(Page);
    void openSettings();
    void closeSettings();
    bool applySettings(const Settings &, std::string &error);
    bool start(const MatchConfig &, std::string &error, SessionOptions = {});
    bool startTutorial(std::string &error);
    bool restart(std::uint32_t seed, std::string &error);
    CommandResult submit(PlayerId, const Command &, std::uint64_t generation);
    MatchMode mode() const {
        return options_.mode;
    }
    ai::Difficulty difficulty() const {
        return options_.difficulty;
    }
    bool aiTurn() const;
    std::optional<AiPlan> planAi() const;
    CommandResult commitAi(const AiPlan &);
    const Tutorial *tutorial() const {
        return mode() == MatchMode::Tutorial && hasMatch() ? &tutorial_ : nullptr;
    }
    void continueTutorial();
    bool saveReplay(std::string &error) const;
    void requestLeave(bool quit = false);
    void cancelConfirmation();
    bool confirmLeave(PlayerId, std::string &error);
    void requestSurrender();
    bool confirmSurrender(PlayerId, std::string &error);

  private:
    Content content_;
    std::filesystem::path directory_, replayPath_;
    Settings settings_;
    std::vector<DeckPreset> presets_;
    DeckLibrary decks_;
    std::unique_ptr<MatchSession> match_;
    std::unique_ptr<RoomSession> network_;
    RoomFactory roomFactory_;
    MatchConfig configuration_;
    Page page_{Page::Menu}, settingsReturn_{Page::Menu}, confirmationReturn_{Page::Menu};
    std::string notice_;
    std::uint64_t generation_{};
    bool quit_{}, leaveQuits_{}, leavePending_{}, networkReset_{};
    SessionOptions options_;
    Tutorial tutorial_;
    std::uint64_t revision_{};
};
} // namespace wizard::app
