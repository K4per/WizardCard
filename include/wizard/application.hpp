#pragma once
#include "wizard/content.hpp"
#include "wizard/decks.hpp"
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
    ConfirmDeckDiscard
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
void atomicWriteJson(const std::filesystem::path &, const Json &);
struct DeckPreset {
    std::string id, name;
    PlayerDeck deck;
    std::string description;
};
std::vector<DeckPreset> loadPresets(const Content &, const std::filesystem::path &assets);

enum class MatchMode { Hotseat, Ai, Tutorial };
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
    Application(Content, std::filesystem::path userDirectory, std::vector<DeckPreset> = {});
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
        return bool(match_);
    }
    bool paused() const {
        return page_ != Page::Match || !match_;
    }
    bool quitRequested() const {
        return quit_;
    }
    std::uint64_t generation() const {
        return generation_;
    }
    std::uint64_t revision() const {
        return revision_;
    }
    const MatchSession &match() const;
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
    MatchConfig configuration_;
    Page page_{Page::Menu}, settingsReturn_{Page::Menu}, confirmationReturn_{Page::Menu};
    std::string notice_;
    std::uint64_t generation_{};
    bool quit_{}, leaveQuits_{};
    SessionOptions options_;
    Tutorial tutorial_;
    std::uint64_t revision_{};
};
} // namespace wizard::app
