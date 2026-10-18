#include "wizard/bridge.hpp"
#include "wizard/sound.hpp"
#include "wizard/presentation.hpp"
#include <algorithm>
#include <limits>

namespace wizard::bridge {
namespace {
Json ok(Json data = Json::object()) { return {{"ok", true}, {"data", std::move(data)}}; }
Json fail(const std::string &code, const std::string &error) {
    return {{"ok", false}, {"errorCode", code}, {"error", error}};
}
}
Json LocalSession::initialize(const std::filesystem::path &assets,
                              const std::filesystem::path &userDirectory) {
    try {
        auto content = loadContent(assets);
        auto next = std::make_unique<app::Application>(content, userDirectory,
                                                      app::loadPresets(content, assets), roomFactory_);
        application_ = std::move(next);
        content_ = std::move(content);
        pending_.reset();
        interactionViewer_ = -1;
        ++generation_;
        Json presets = Json::array();
        for (const auto &preset : application_->playableDecks())
            presets.push_back({{"id", preset.id}, {"name", preset.name}, {"description", preset.description}});
        return ok({{"programVersion", programVersion}, {"rulesVersion", content_->catalog.rulesVersion},
                   {"cardSetVersion", content_->catalog.cardSetVersion},
                   {"contentHash", content_->catalog.contentHash}, {"presets", presets},
                   {"settings", app::encodeSettings(application_->settings())},
                   {"notice", application_->notice()}, {"generation", std::to_string(generation_)}});
    } catch (const std::exception &e) { return fail("initialize_failed", e.what()); }
}
Json LocalSession::start(const Json &options) {
    if (!application_) return fail("not_initialized", "请先加载内容");
    try {
        const auto mode = options.value("mode", std::string("hotseat"));
        const auto difficulty = options.value("difficulty", 1);
        if (difficulty < 0 || difficulty > 2) return fail("invalid_options", "无效AI难度");
        if (mode != "hotseat" && mode != "ai" && mode != "tutorial")
            return fail("invalid_options", "无效对局模式");
        std::string error;
        bool started{};
        if (mode == "tutorial") started = application_->startTutorial(error);
        else {
            MatchConfig config;
            if (options.contains("seed")) {
                const auto &seed = options.at("seed");
                if (!seed.is_number_integer() || seed.get<std::int64_t>() < 0 ||
                    seed.get<std::uint64_t>() > std::numeric_limits<std::uint32_t>::max())
                    return fail("invalid_options", "种子超出范围");
                config.seed = seed.get<std::uint32_t>();
            }
            if (options.contains("players")) {
                const auto &players = options.at("players");
                if (!players.is_array() || players.size() != 2)
                    return fail("invalid_options", "需要双方牌表");
                for (int p = 0; p < 2; ++p) {
                    config.players[p].baseFormation = players.at(p).value("baseFormation", std::string{});
                    config.players[p].cards = players.at(p).at("cards").get<std::vector<std::string>>();
                    if (content_->catalog.usesAlphaV2())
                        config.players[p].sideDeck = players.at(p).at("sideDeck").get<std::vector<std::string>>();
                }
            } else {
                const auto presets = application_->playableDecks();
                for (int p = 0; p < 2; ++p) {
                    const auto id = options.value(p == 0 ? "playerDeck" : "opponentDeck", std::string("default"));
                    const auto found = std::find_if(presets.begin(), presets.end(),
                        [&](const app::DeckPreset &preset) { return preset.id == id; });
                    if (found == presets.end()) return fail("invalid_deck", "未找到合法卡组");
                    config.players[p] = found->deck;
                }
            }
            started = application_->start(config, error,
                {mode == "ai" ? app::MatchMode::Ai : app::MatchMode::Hotseat,
                 static_cast<ai::Difficulty>(difficulty)});
        }
        if (!started) return fail("start_failed", error);
        pending_.reset(); ++generation_;
        interactionViewer_ = -1;
        return ok({{"generation", std::to_string(generation_)},
                   {"revision", std::to_string(application_->revision())}});
    } catch (const std::exception &e) { return fail("invalid_options", e.what()); }
}
bool LocalSession::visibleTo(PlayerId viewer) const {
    return application_ && application_->hasMatch() && viewer >= 0 && viewer <= 1 &&
        (application_->mode() == app::MatchMode::Hotseat ||
         (application_->mode() == app::MatchMode::Network && application_->network() &&
          application_->network()->started() && application_->network()->view().viewer == viewer) ||
         (viewer == 0 && application_->mode() != app::MatchMode::Network));
}
bool LocalSession::current(const std::string &generation, const std::string &revision) const {
    return application_ && application_->hasMatch() && generation == std::to_string(generation_) &&
           revision == std::to_string(application_->revision());
}
std::string LocalSession::actionId(PlayerId viewer, std::size_t index) const {
    return std::to_string(generation_) + ":" + std::to_string(application_->revision()) + ":" +
           std::to_string(viewer) + ":" + std::to_string(index);
}
GameView LocalSession::view(PlayerId viewer) const {
    auto v = application_->viewFor(viewer);
    if (const auto *tutorial = application_->tutorial())
        v.actions.erase(std::remove_if(v.actions.begin(), v.actions.end(),
            [&](const LegalAction &a) { return !tutorial->allows(v, a.command); }), v.actions.end());
    return v;
}
Json LocalSession::snapshot(PlayerId viewer) const {
    if (!visibleTo(viewer)) return fail("invalid_viewer", "没有此查看者的对局视图");
    const auto v = view(viewer);
    auto data = viewDto(v);
    data["generation"] = std::to_string(generation_);
    data["revision"] = std::to_string(application_->revision());
    data["paused"] = application_->paused();
    data["aiTurn"] = application_->aiTurn();
    data["actingPlayer"] = application_->actingPlayer();
    data["actions"] = Json::array();
    data["automaticAction"] = nullptr;
    const auto automatic = ui::automaticAdvance(v, application_->settings().automaticPhases &&
        application_->mode() != app::MatchMode::Tutorial && application_->mode() != app::MatchMode::Network);
    for (std::size_t i = 0; i < v.actions.size(); ++i) {
        const auto &a = v.actions[i];
        data["actions"].push_back({{"id", actionId(viewer, i)}, {"label", a.label},
                                  {"source", std::to_string(a.source)}, {"target", std::to_string(a.target)},
                                  {"command", commandDto(a.command)}});
        if (automatic && encodeCommand(a.command) == encodeCommand(Command(*automatic)))
            data["automaticAction"] = actionId(viewer, i);
    }
    if (const auto *tutorial = application_->tutorial())
        data["tutorial"] = {{"lesson", static_cast<int>(tutorial->lesson)}, {"title", tutorial->title()},
                            {"hint", tutorial->hint(v)}, {"canContinue", tutorial->canContinue(v)},
                            {"complete", tutorial->lesson == app::Lesson::Complete}};
    else data["tutorial"] = nullptr;
    return ok(std::move(data));
}
Json LocalSession::selectAction(PlayerId viewer, const std::string &id,
                                const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_action", "操作已过期");
    if (!visibleTo(viewer)) return fail("invalid_viewer", "无权提交该查看者操作");
    if (application_->paused()) return fail("match_paused", "对局已暂停");
    const auto v = view(viewer);
    for (std::size_t i = 0; i < v.actions.size(); ++i)
        if (actionId(viewer, i) == id) {
            pending_ = Pending{viewer, v.actions[i].command, generation_, application_->revision()};
            return ok({{"label", v.actions[i].label}, {"command", commandDto(v.actions[i].command)}});
        }
    return fail("invalid_action", "动作不在当前合法列表中");
}
Json LocalSession::accepted(const CommandResult &result, const GameView &before) {
    if (!result.accepted) return fail(result.errorCode.empty() ? "rejected" : result.errorCode, result.error);
    pending_.reset();
    if (result.pending) {
        return ok({{"generation", std::to_string(generation_)},
                   {"revision", std::to_string(application_->revision())},
                   {"pending", true}, {"replaySaved", false}, {"saveError", ""},
                   {"sounds", Json::array()}, {"cues", Json::array()}});
    }
    if (application_->mode() == app::MatchMode::Network)
        application_->tickNetwork();
    std::string error;
    const bool saved = application_->saveReplay(error);
    Json sounds = Json::array();
    const auto after = view(before.viewer);
    for (const auto cue : ui::matchSounds(before, after, !application_->aiTurn()))
        sounds.push_back({{"file", ui::soundFile(cue)}, {"gain", ui::soundGain(cue)},
                          {"priority", ui::soundPriority(cue)}, {"cooldownSeconds", ui::soundCooldown(cue)}});
    ui::MatchPresentation presentation;
    presentation.observe(before, after, application_->settings().reducedMotion);
    Json cues = Json::array();
    for (const auto &cue : presentation.cues()) {
        // References originate exclusively from the same viewer's filtered GameViews.
        // No card definitions or hidden engine state cross this presentation boundary.
        cues.push_back({{"sequence", std::to_string(cue.id)}, {"kind", static_cast<int>(cue.kind)},
                       {"source", std::to_string(cue.source)}, {"host", std::to_string(cue.host)},
                       {"target", std::to_string(cue.target)}, {"player", cue.player},
                       {"from", static_cast<int>(cue.from)}, {"to", static_cast<int>(cue.to)},
                       {"amount", cue.amount}, {"text", cue.card && cue.card->hidden ? "隐藏卡牌" : cue.text},
                       {"duration", cue.duration}});
    }
    return ok({{"generation", std::to_string(generation_)},
               {"revision", std::to_string(application_->revision())},
               {"pending", false}, {"replaySaved", saved}, {"saveError", error},
               {"sounds", sounds}, {"cues", cues}});
}
Json LocalSession::confirmAction(const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_action", "确认已过期");
    if (!pending_) return fail("no_selection", "请先选择操作");
    if (pending_->generation != generation_ || pending_->revision != application_->revision()) {
        pending_.reset(); return fail("stale_action", "选择已过期");
    }
    if (application_->paused()) return fail("match_paused", "对局已暂停");
    const auto selected = *pending_;
    const auto before = view(selected.viewer);
    return accepted(application_->submit(selected.viewer, selected.command, application_->generation()), before);
}
Json LocalSession::cancelAction() { pending_.reset(); interaction_.cancel(); return ok(); }
Json LocalSession::pause(bool paused) {
    if (!application_ || !application_->hasMatch()) return fail("no_match", "没有对局");
    if (paused) application_->openSettings(); else application_->closeSettings();
    return ok();
}
Json LocalSession::stepAi(const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_ai", "AI操作已过期");
    const auto plan = application_->planAi();
    if (!plan) return fail("ai_unavailable", "当前不能提交AI操作");
    const auto before = view(0);
    return accepted(application_->commitAi(*plan), before);
}
Json LocalSession::continueTutorial(const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_action", "教学操作已过期");
    if (application_->paused()) return fail("match_paused", "对局已暂停");
    const auto *tutorial = application_->tutorial();
    if (!tutorial || !tutorial->canContinue(view(0))) return fail("tutorial_unavailable", "当前没有教学讲解");
    application_->continueTutorial(); pending_.reset(); return ok();
}
Json LocalSession::saveReplay() const {
    if (!application_ || !application_->hasMatch()) return fail("no_match", "没有对局");
    if (application_->mode() == app::MatchMode::Network && application_->network()->view().result == -1)
        return fail("replay_unavailable", "完整联机复盘在对局结束后开放");
    std::string error;
    if (!application_->saveReplay(error)) return fail("save_failed", error);
    return ok({{"path", application_->replayPath().u8string()}});
}
Json LocalSession::restart(std::uint32_t seed) {
    if (!application_) return fail("not_initialized", "请先加载内容");
    std::string error;
    if (!application_->restart(seed, error)) return fail("restart_failed", error);
    ++generation_; pending_.reset(); return ok();
}
Json LocalSession::release() {
    pending_.reset(); interaction_.cancel(); interactionViewer_ = -1;
    application_.reset(); content_.reset(); ++generation_; return ok();
}
Json LocalSession::readReplayPlan(const std::filesystem::path &path) const {
    if (!content_) return fail("not_initialized", "请先加载内容");
    if (application_->network() && application_->network()->started() && application_->network()->view().result == -1)
        return fail("replay_unavailable", "联机对局进行中不能打开完整复盘");
    try {
        const auto j = readJson(path);
        (void)replay(*content_, j); // Validate before exporting any test sequence.
        Json commands = Json::array();
        for (const auto &row : j.at("commands"))
            commands.push_back({{"actor", row.at("actor")},
                                {"command", commandDto(decodeCommand(row.at("command")))},
                                {"digest", row.at("digest")}});
        return ok({{"seed", j.at("seed")}, {"players", j.at("players")},
                   {"commands", commands}, {"finalDigest", j.at("finalDigest")}});
    } catch (const std::exception &e) { return fail("invalid_replay", e.what()); }
}
Json LocalSession::verifyReplay(const std::filesystem::path &path) const {
    if (!content_) return fail("not_initialized", "请先加载内容");
    if (application_->network() && application_->network()->started() && application_->network()->view().result == -1)
        return fail("replay_unavailable", "联机对局进行中不能打开完整复盘");
    try {
        const auto j = readJson(path);
        const auto engine = replay(*content_, j);
        Json digests = Json::array();
        for (const auto &row : j.at("commands")) digests.push_back(row.at("digest"));
        return ok({{"digest", engine.digest()}, {"commandCount", j.at("commands").size()},
                   {"digests", digests}});
    } catch (const std::exception &e) { return fail("invalid_replay", e.what()); }
}
Json LocalSession::replayFrame(const std::filesystem::path &path, std::size_t step, PlayerId viewer) const {
    if (!content_) return fail("not_initialized", "请先加载内容");
    if (application_->network() && application_->network()->started() && application_->network()->view().result == -1)
        return fail("replay_unavailable", "联机对局进行中不能打开完整复盘");
    if (viewer != 0 && viewer != 1) return fail("invalid_viewer", "复盘查看者无效");
    try {
        const auto writeTime = std::filesystem::last_write_time(path);
        const auto fileSize = std::filesystem::file_size(path);
        if (!replayCacheValid_ || replayCachePath_ != path ||
            replayCacheWriteTime_ != writeTime || replayCacheFileSize_ != fileSize) {
            replayCacheRecord_ = readJson(path);
            (void)replay(*content_, replayCacheRecord_); // Validate once, including the unshown suffix.
            replayCacheConfig_ = {};
            replayCacheConfig_.seed = replayCacheRecord_.at("seed").get<std::uint32_t>();
            for (int p = 0; p < 2; ++p) {
                const auto &player = replayCacheRecord_.at("players").at(p);
                replayCacheConfig_.players[p].baseFormation = player.value("baseFormation", std::string{});
                replayCacheConfig_.players[p].cards = player.at("cards").get<std::vector<std::string>>();
                if (content_->catalog.usesAlphaV2())
                    replayCacheConfig_.players[p].sideDeck = player.at("sideDeck").get<std::vector<std::string>>();
            }
            replayCacheEngine_ = std::make_unique<GameEngine>(content_->catalog, replayCacheConfig_);
            replayCachePath_ = path;
            replayCacheWriteTime_ = writeTime;
            replayCacheFileSize_ = fileSize;
            replayCacheStep_ = 0;
            replayCacheValid_ = true;
        }
        const auto count = replayCacheRecord_.at("commands").size();
        if (step > count) return fail("invalid_step", "复盘位置超出范围");
        if (step < replayCacheStep_) {
            replayCacheEngine_ = std::make_unique<GameEngine>(content_->catalog, replayCacheConfig_);
            replayCacheStep_ = 0;
        }
        for (std::size_t i = replayCacheStep_; i < step; ++i) {
            const auto &row = replayCacheRecord_.at("commands").at(i);
            const auto command = decodeCommand(row.at("command"));
            const auto result = std::holds_alternative<Timeout>(command) ? replayCacheEngine_->timeout(row.at("actor")) :
                replayCacheEngine_->submit(row.at("actor"), command);
            if (!result.accepted)
                throw std::runtime_error("复盘指令未能重放");
        }
        replayCacheStep_ = step;
        auto data = viewDto(replayCacheEngine_->viewFor(viewer));
        data["step"] = step;
        data["commandCount"] = count;
        data["digest"] = replayCacheEngine_->digest();
        return ok(std::move(data));
    } catch (const std::exception &e) { return fail("invalid_replay", e.what()); }
}
} // namespace wizard::bridge
