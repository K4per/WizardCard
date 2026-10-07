#include "wizard/application.hpp"
#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <fstream>
#include <set>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#endif

namespace wizard::app {
namespace {
int integer(const Json &j, const char *key, int low, int high) {
    const auto &v = j.at(key);
    if (!v.is_number_integer() || v.get<std::int64_t>() < low || v.get<std::int64_t>() > high)
        throw std::runtime_error(std::string("设置字段无效：") + key);
    return v.get<int>();
}
#ifndef _WIN32
std::filesystem::path environmentPath(const char *key) {
    const auto *value = std::getenv(key);
    return value && *value ? std::filesystem::u8path(value) : std::filesystem::path{};
}
#endif
} // namespace
float Settings::effectiveSoundVolume() const {
    return static_cast<float>(masterVolume * soundVolume) / 100.f;
}
Json encodeSettings(const Settings &s) {
    return {{"format", 1},
            {"masterVolume", s.masterVolume},
            {"soundVolume", s.soundVolume},
            {"windowMode", static_cast<int>(s.windowMode)},
            {"width", s.width},
            {"height", s.height},
            {"automaticPhases", s.automaticPhases},
            {"reducedMotion", s.reducedMotion}};
}
Settings decodeSettings(const Json &j) {
    if (integer(j, "format", 1, 1) != 1)
        throw std::runtime_error("设置版本不兼容");
    Settings s;
    s.masterVolume = integer(j, "masterVolume", 0, 100);
    s.soundVolume = integer(j, "soundVolume", 0, 100);
    s.windowMode = static_cast<WindowMode>(integer(j, "windowMode", 0, 2));
    s.width = static_cast<unsigned>(integer(j, "width", 960, 3840));
    s.height = static_cast<unsigned>(integer(j, "height", 600, 2160));
    if (!j.at("automaticPhases").is_boolean())
        throw std::runtime_error("阶段自动通过设置无效");
    s.automaticPhases = j.at("automaticPhases").get<bool>();
    if (j.contains("reducedMotion")) {
        if (!j.at("reducedMotion").is_boolean())
            throw std::runtime_error("动画设置无效");
        s.reducedMotion = j.at("reducedMotion").get<bool>();
    }
    return s;
}
std::filesystem::path userDataDirectory() {
#ifdef _WIN32
    wchar_t path[32768]{};
    auto length = GetEnvironmentVariableW(L"LOCALAPPDATA", path, 32768);
    if (length && length < 32768)
        return std::filesystem::path(path) / "WizardCard";
    length = GetEnvironmentVariableW(L"USERPROFILE", path, 32768);
    if (length && length < 32768)
        return std::filesystem::path(path) / "AppData" / "Local" / "WizardCard";
#else
    auto path = environmentPath("XDG_DATA_HOME");
    if (!path.empty())
        return path / "WizardCard";
    path = environmentPath("HOME");
    if (!path.empty())
        return path / ".local" / "share" / "WizardCard";
#endif
    throw std::runtime_error("无法定位用户数据目录，请使用 --user-data 指定目录");
}

std::vector<DeckPreset> loadPresets(const Content &content, const std::filesystem::path &assets) {
    std::vector<DeckPreset> out{
        {"default",
         "示范卡组",
         {"balance", content.deck},
         "覆盖解析、言灵和荷载的综合入门卡组。先建立荷载空间，解析火球术，再选择施法顺序。"}};
    if (!std::filesystem::exists(assets / "presets.json"))
        return out;
    const auto j = readJson(assets / "presets.json");
    if (!j.is_array())
        throw std::runtime_error("预设卡组格式错误");
    std::set<std::string> ids{"default"};
    for (const auto &row : j) {
        DeckPreset p;
        p.id = row.at("id").get<std::string>();
        p.name = row.at("name").get<std::string>();
        p.description = row.value("description", std::string{});
        if (p.id.empty() || p.name.empty() || !ids.insert(p.id).second)
            throw std::runtime_error("预设卡组ID或名称无效");
        p.deck.baseFormation = row.at("baseFormation").get<std::string>();
        p.deck.cards = parseDeck(row.at("cards"), content.catalog, "presets.json/" + p.id);
        auto errors = Rules::deckErrors(content.catalog, p.deck);
        if (!errors.empty())
            throw std::runtime_error(errors.front());
        out.push_back(std::move(p));
    }
    return out;
}
Application::Application(Content content, std::filesystem::path directory, std::vector<DeckPreset> presets)
    : content_(std::move(content)), directory_(std::move(directory)), presets_(std::move(presets)),
      decks_(content_.catalog, directory_) {
    if (presets_.empty())
        presets_.push_back({"default", "示范卡组", {"balance", content_.deck}});
    for (const auto &preset : presets_) {
        auto errors = Rules::deckErrors(content_.catalog, preset.deck);
        if (!errors.empty())
            throw std::invalid_argument(errors.front());
    }
    for (auto &player : configuration_.players)
        player = presets_.front().deck;
    try {
        if (std::filesystem::exists(directory_ / "settings.json"))
            settings_ = decodeSettings(readJson(directory_ / "settings.json"));
    } catch (const std::exception &) {
        notice_ = "设置文件无法读取，已使用默认设置；应用设置后可重新保存。";
    }
    if (!decks_.notice().empty())
        notice_ += (notice_.empty() ? "" : " ") + decks_.notice();
}
std::vector<DeckPreset> Application::playableDecks() const {
    auto choices = presets_;
    for (const auto &draft : decks_.drafts())
        if (decks_.problems(draft).empty())
            choices.push_back({"user/" + draft.id, draft.name + "（自建）", draft.playerDeck()});
    return choices;
}
const MatchSession &Application::match() const {
    if (!match_)
        throw std::logic_error("没有进行中的对局");
    return *match_;
}
void Application::navigate(Page page) {
    if (hasMatch())
        return;
    if (page == Page::Menu || page == Page::HotseatSetup || page == Page::AiSetup || page == Page::Decks ||
        page == Page::DeckEditor || page == Page::ConfirmDeckDelete || page == Page::ConfirmDeckDiscard ||
        page == Page::NetworkSetup || page == Page::NetworkRoom)
        page_ = page;
}
void Application::openSettings() {
    if (page_ != Page::Menu && page_ != Page::Match)
        return;
    settingsReturn_ = page_;
    page_ = Page::Settings;
}
void Application::closeSettings() {
    if (page_ == Page::Settings)
        page_ = settingsReturn_;
}
bool Application::applySettings(const Settings &s, std::string &error) {
    try {
        auto validated = decodeSettings(encodeSettings(s));
        atomicWriteJson(directory_ / "settings.json", encodeSettings(validated));
        settings_ = validated;
        notice_.clear();
        return true;
    } catch (const std::exception &e) {
        error = std::string("设置未保存：") + e.what();
        return false;
    }
}
bool Application::start(const MatchConfig &config, std::string &error, SessionOptions options) {
    if (match_ || network_) {
        error = "请先结束当前对局";
        return false;
    }
    try {
        (void)ai::difficultyLabel(options.difficulty);
        if (options.mode != MatchMode::Hotseat && options.mode != MatchMode::Ai &&
            options.mode != MatchMode::Tutorial)
            throw std::invalid_argument("无效对局模式");
        auto next = std::make_unique<MatchSession>(content_, config);
        auto path = directory_ / "replays" /
                    ("match-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) +
                     "-" + std::to_string(generation_ + 1) + ".json");
        atomicWriteJson(path, next->recording());
        match_ = std::move(next);
        configuration_ = config;
        replayPath_ = std::move(path);
        ++generation_;
        ++revision_;
        options_ = options;
        tutorial_ = {};
        page_ = Page::Match;
        return true;
    } catch (const std::exception &e) {
        error = std::string("无法开始对局：") + e.what();
        return false;
    }
}
bool Application::startTutorial(std::string &error) {
    try {
        return start(tutorialConfiguration(content_), error, {MatchMode::Tutorial, ai::Difficulty::Easy});
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool Application::restart(std::uint32_t seed, std::string &error) {
    if (!match_ || match_->engine().state().result == -1) {
        error = "当前对局尚未结束";
        return false;
    }
    auto config = configuration_;
    config.seed = mode() == MatchMode::Tutorial ? tutorialConfiguration(content_).seed : seed;
    const auto options = options_;
    auto previous = std::move(match_);
    if (start(config, error, options))
        return true;
    match_ = std::move(previous);
    return false;
}
CommandResult Application::submit(PlayerId actor, const Command &command, std::uint64_t generation) {
    if (generation != generation_)
        return {false, "对局操作已过期", {}, {}, "stale_match"};
    if (paused())
        return {false, "对局已暂停", {}, {}, "match_paused"};
    if (network_) {
        if (actor != network_->view().viewer)
            return {false, "无权操作对方席位", {}, {}, "wrong_player"};
        return network_->submit(command);
    }
    if (mode() == MatchMode::Tutorial && actor == 0 &&
        !tutorial_.allows(match_->engine().viewFor(0), command))
        return {false, tutorial_.hint(match_->engine().viewFor(0)), {}, {}, "tutorial_hint"};
    auto result = match_->submit(actor, command);
    if (result.accepted) {
        ++revision_;
        if (mode() == MatchMode::Tutorial)
            tutorial_.observe(actor, command, match_->engine().viewFor(0), result.events);
    }
    return result;
}
bool Application::aiTurn() const {
    if (paused() || mode() == MatchMode::Hotseat || mode() == MatchMode::Network ||
        match_->engine().state().result != -1)
        return false;
    const auto &s = match_->engine().state();
    if ((s.decision ? s.decision->player : s.active) != 1)
        return false;
    if (mode() == MatchMode::Tutorial && tutorial_.canContinue(match_->engine().viewFor(0)))
        return false;
    return true;
}
std::optional<AiPlan> Application::planAi() const {
    if (!aiTurn())
        return {};
    const auto view = match_->engine().viewFor(1);
    auto selection =
        mode() == MatchMode::Tutorial ? tutorial_.opponent(view) : ai::choose(view, options_.difficulty);
    if (!selection)
        return {};
    return AiPlan{generation_, revision_, *selection};
}
CommandResult Application::commitAi(const AiPlan &plan) {
    if (plan.generation != generation_ || plan.revision != revision_)
        return {false, "AI操作已过期", {}, {}, "stale_ai"};
    if (paused())
        return {false, "对局已暂停", {}, {}, "match_paused"};
    if (!aiTurn())
        return {false, "当前不是AI决策", {}, {}, "ai_unavailable"};
    return submit(1, plan.selection.command, plan.generation);
}
void Application::continueTutorial() {
    if (paused() || mode() != MatchMode::Tutorial)
        return;
    const auto view = match_->engine().viewFor(0);
    if (!tutorial_.canContinue(view))
        return;
    tutorial_.continueLesson(view);
    ++revision_;
}
bool Application::saveReplay(std::string &error) const {
    if (network_)
        return network_->save(error);
    if (!match_) {
        error = "没有可保存的对局";
        return false;
    }
    try {
        atomicWriteJson(replayPath_, match_->recording());
        return true;
    } catch (const std::exception &e) {
        error = std::string("复盘未保存：") + e.what();
        return false;
    }
}
void Application::requestLeave(bool quit) {
    if (network_) {
        if (page_ == Page::ConfirmLeave || page_ == Page::ConfirmSurrender)
            return;
        confirmationReturn_ = page_;
        leaveQuits_ = quit;
        page_ = Page::ConfirmLeave;
        return;
    }
    if (!match_) {
        if (quit)
            quit_ = true;
        else
            page_ = Page::Menu;
        return;
    }
    if (page_ == Page::ConfirmLeave || page_ == Page::ConfirmSurrender)
        return;
    confirmationReturn_ = page_;
    leaveQuits_ = quit;
    page_ = Page::ConfirmLeave;
}
void Application::cancelConfirmation() {
    if (page_ == Page::ConfirmLeave || page_ == Page::ConfirmSurrender)
        page_ = confirmationReturn_;
}
bool Application::confirmLeave(PlayerId actor, std::string &error) {
    if (network_)
        return leaveNetwork(error);
    if (page_ != Page::ConfirmLeave || !match_) {
        error = "离开确认已失效";
        return false;
    }
    try {
        auto finished = *match_;
        if (finished.engine().state().result == -1) {
            auto r = finished.submit(actor, Surrender{});
            if (!r.accepted)
                throw std::runtime_error(r.error);
        }
        atomicWriteJson(replayPath_, finished.recording());
        match_.reset();
        options_ = {};
        tutorial_ = {};
        ++generation_;
        page_ = Page::Menu;
        quit_ = leaveQuits_;
        return true;
    } catch (const std::exception &e) {
        error = std::string("对局仍保留，无法保存记录：") + e.what();
        return false;
    }
}
void Application::requestSurrender() {
    if (network_) {
        if (hasMatch() && network_->view().result == -1) {
            confirmationReturn_ = page_;
            page_ = Page::ConfirmSurrender;
        }
        return;
    }
    if (!match_ || match_->engine().state().result != -1 || (page_ != Page::Settings && page_ != Page::Match))
        return;
    confirmationReturn_ = page_;
    page_ = Page::ConfirmSurrender;
}
bool Application::confirmSurrender(PlayerId actor, std::string &error) {
    if (network_) {
        auto r = network_->submit(Surrender{});
        error = r.error;
        if (r.accepted)
            page_ = Page::Match;
        return r.accepted;
    }
    if (page_ != Page::ConfirmSurrender || !match_) {
        error = "投降确认已失效";
        return false;
    }
    try {
        auto finished = std::make_unique<MatchSession>(*match_);
        auto r = finished->submit(actor, Surrender{});
        if (!r.accepted)
            throw std::runtime_error(r.error);
        atomicWriteJson(replayPath_, finished->recording());
        match_ = std::move(finished);
        page_ = Page::Match;
        return true;
    } catch (const std::exception &e) {
        error = std::string("投降未完成：") + e.what();
        return false;
    }
}
GameView Application::viewFor(PlayerId player) const {
    if (network_)
        return network_->view();
    return match().engine().viewFor(player);
}
PlayerId Application::actingPlayer() const {
    if (network_)
        return network_->view().viewer;
    const auto &s = match().engine().state();
    return s.decision ? s.decision->player : s.active;
}
bool Application::startNetwork(bool host, const std::string &address, unsigned short port,
                               const PlayerDeck &d, std::string &error) {
    if (hasMatch() || network_) {
        error = "请先离开当前房间或对局";
        return false;
    }
    try {
        auto path = directory_ / "replays" / ("network-" + net::token() + ".json");
        auto peer = std::make_unique<net::Peer>(content_, path);
        if (host)
            peer->host(port, d);
        else
            peer->join(address, port);
        network_ = std::move(peer);
        replayPath_ = path;
        options_.mode = MatchMode::Network;
        ++generation_;
        page_ = Page::NetworkRoom;
        revision_ = 0;
        return true;
    } catch (const std::exception &e) {
        error = e.what();
        return false;
    }
}
bool Application::tickNetwork() {
    if (!network_)
        return false;
    network_->tick();
    if (!network_->closing() && !network_->ended())
        leavePending_ = false;
    if (network_->closing())
        notice_ = "等待投降确认和复盘保存…";
    else
        notice_ = network_->notice();
    if (network_->ended() && leavePending_) {
        network_.reset();
        options_ = {};
        ++generation_;
        page_ = Page::Menu;
        quit_ = leaveQuits_;
        leavePending_ = false;
        return false;
    }
    if (!network_->started())
        return false;
    bool reset = network_->takeReset();
    networkReset_ = reset;
    if (page_ == Page::NetworkRoom)
        page_ = Page::Match;
    auto r = network_->revision();
    bool changed = r != revision_ || reset;
    revision_ = r;
    return changed;
}
bool Application::leaveNetwork(std::string &error, bool force) {
    if (!network_)
        return true;
    bool done = network_->leave(error, force);
    if (done) {
        network_.reset();
        options_ = {};
        ++generation_;
        page_ = Page::Menu;
        quit_ = leaveQuits_;
        leavePending_ = false;
    } else if (network_->closing())
        leavePending_ = true;
    return done;
}
} // namespace wizard::app
