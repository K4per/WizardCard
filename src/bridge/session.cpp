#include "wizard/bridge.hpp"
#include "wizard/sound.hpp"
#include "wizard/presentation.hpp"
#include <algorithm>
#include <limits>

namespace wizard::bridge {
namespace {
constexpr std::size_t replayAutosaveInterval = 32;
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
        match_.application = std::move(next);
        decks_.content = std::move(content);
        decks_.libraryCache.reset();
        replay_.valid = false;
        replay_.browsing = false;
        replay_.engine.reset();
        replay_.checkpoints.clear();
        match_.pending.reset();
        match_.interactionViewer = -1;
        replaySaveHealthy_ = true;
        ++match_.generation;
        Json presets = Json::array();
        for (const auto &preset : match_.application->playableDecks())
            presets.push_back({{"id", preset.id}, {"name", preset.name}, {"description", preset.description}});
        return ok({{"programVersion", programVersion}, {"rulesVersion", decks_.content->catalog.rulesVersion},
                   {"cardSetVersion", decks_.content->catalog.cardSetVersion},
                   {"contentHash", decks_.content->catalog.contentHash}, {"presets", presets},
                   {"settings", app::encodeSettings(match_.application->settings())},
                   {"notice", match_.application->notice()}, {"generation", std::to_string(match_.generation)}});
    } catch (const std::exception &e) { return fail("initialize_failed", e.what()); }
}
Json LocalSession::start(const Json &options) {
    if (!match_.application) return fail("not_initialized", "请先加载内容");
    try {
        const auto mode = options.value("mode", std::string("hotseat"));
        const auto difficulty = options.value("difficulty", 1);
        if (difficulty < 0 || difficulty > 2) return fail("invalid_options", "无效AI难度");
        if (mode != "hotseat" && mode != "ai" && mode != "tutorial")
            return fail("invalid_options", "无效对局模式");
        std::string error;
        bool started{};
        if (mode == "tutorial") started = match_.application->startTutorial(error);
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
                    config.players[p].sideDeck = players.at(p).at("sideDeck").get<std::vector<std::string>>();
                }
            } else {
                const auto presets = match_.application->playableDecks();
                for (int p = 0; p < 2; ++p) {
                    const auto id = options.value(p == 0 ? "playerDeck" : "opponentDeck", std::string("default"));
                    const auto found = std::find_if(presets.begin(), presets.end(),
                        [&](const app::DeckPreset &preset) { return preset.id == id; });
                    if (found == presets.end()) return fail("invalid_deck", "未找到合法卡组");
                    config.players[p] = found->deck;
                }
            }
            started = match_.application->start(config, error,
                {mode == "ai" ? app::MatchMode::Ai : app::MatchMode::Hotseat,
                 static_cast<ai::Difficulty>(difficulty)});
        }
        if (!started) return fail("start_failed", error);
        replay_.browsing = false;
        match_.pending.reset(); replaySaveHealthy_ = true; ++match_.generation;
        match_.interactionViewer = -1;
        return ok({{"generation", std::to_string(match_.generation)},
                   {"revision", std::to_string(match_.application->revision())}});
    } catch (const std::exception &e) { return fail("invalid_options", e.what()); }
}
bool LocalSession::visibleTo(PlayerId viewer) const {
    return match_.application && match_.application->hasMatch() && viewer >= 0 && viewer <= 1 &&
        (match_.application->mode() == app::MatchMode::Hotseat ||
         (match_.application->mode() == app::MatchMode::Network && match_.application->network() &&
          match_.application->network()->started() && match_.application->network()->view().viewer == viewer) ||
         (viewer == 0 && match_.application->mode() != app::MatchMode::Network));
}
bool LocalSession::current(const std::string &generation, const std::string &revision) const {
    return match_.application && match_.application->hasMatch() && generation == std::to_string(match_.generation) &&
           revision == std::to_string(match_.application->revision());
}
GameView LocalSession::view(PlayerId viewer) const { return match_.application->viewFor(viewer); }
Json LocalSession::snapshot(PlayerId viewer) const {
    if (!visibleTo(viewer)) return fail("invalid_viewer", "没有此查看者的对局视图");
    const auto v = view(viewer);
    auto data = viewDto(v);
    data["generation"] = std::to_string(match_.generation);
    data["revision"] = std::to_string(match_.application->revision());
    data["paused"] = match_.application->paused();
    data["aiTurn"] = match_.application->aiTurn();
    data["actingPlayer"] = match_.application->actingPlayer();
    data["automaticCommand"] = nullptr;
    const bool debugAvailable = match_.application->debugAvailable() && !replay_.browsing;
    Json debugPlayers = Json::array();
    for (const auto &p : v.players) debugPlayers.push_back({{"infiniteLife", p.infiniteLife},
        {"infiniteMana", p.infiniteMana}, {"infiniteLoad", p.infiniteLoad}});
    const bool canRewind = debugAvailable && match_.application->match().canRewindPreviousTurn();
    data["debug"] = {{"available", debugAvailable}, {"players", debugPlayers}, {"canRewind", canRewind},
        {"reason", !debugAvailable ? "调试控制台仅限本地 AI 单人对局；回放、教学、联机及观战不可用" :
            canRewind ? "" : "没有可用的上一回合开始快照"}};
    data["debug"]["used"] = match_.application->debugAvailable() && match_.application->match().engine().state().debugUsed;
    const auto automatic = ui::automaticAdvance(v, match_.application->settings().automaticPhases &&
        match_.application->mode() != app::MatchMode::Tutorial && match_.application->mode() != app::MatchMode::Network);
    if (automatic) data["automaticCommand"] = commandDto(Command(*automatic));
    data["technicalPaused"] = match_.application->technicalPaused();
    if (const auto *tutorial = match_.application->tutorial())
        data["tutorial"] = {{"lesson", static_cast<int>(tutorial->lesson)}, {"title", tutorial->title()},
                            {"hint", tutorial->hint(v)}, {"canContinue", tutorial->canContinue(v)},
                            {"complete", tutorial->lesson == app::Lesson::Complete}};
    else data["tutorial"] = nullptr;
    return ok(std::move(data));
}
namespace {
std::uint64_t numericId(const std::string &text) {
    if (text.empty() || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::invalid_argument("ID需要精确十进制字符串");
    std::size_t used{}; const auto value = std::stoull(text, &used);
    if (used != text.size()) throw std::invalid_argument("ID无效");
    return value;
}
ActionIntent actionIntent(const Json &data) {
    ActionIntent intent;
    intent.offset = data.value("offset", std::size_t{});
    intent.limit = data.value("limit", std::size_t{64});
    const auto source = numericId(data.value("source", std::string("0")));
    if (source > std::numeric_limits<CardId>::max()) throw std::invalid_argument("卡牌实例ID超出范围");
    intent.source = static_cast<CardId>(source);
    for (const auto &entry : data.value("selections", Json::array())) {
        const auto field = entry.at("field").get<int>();
        if (field < 0 || field >= static_cast<int>(ActionField::Confirm)) throw std::invalid_argument("选择字段无效");
        intent.selections.push_back({static_cast<ActionField>(field), entry.at("value").get<std::string>()});
    }
    return intent;
}
Command commandFromDto(Json data) {
    for (auto &[key, value] : data.items()) {
        if (key == "type" || key == "ability" || key == "release" || key == "skipCast" || key == "ringIndex") continue;
        if (key == "option" && value.is_object()) { numericId(value.at("value").get<std::string>()); }
        else if (value.is_array()) for (auto &id : value) id = numericId(id.get<std::string>());
        else value = numericId(value.get<std::string>());
    }
    return decodeCommand(data);
}
}
Json LocalSession::queryActions(PlayerId viewer, const Json &intent, const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_query", "操作查询已过期");
    if (!visibleTo(viewer)) return fail("invalid_viewer", "无权查询该查看者操作");
    try {
        const auto options = match_.application->queryActions(viewer, match_.application->generation(), match_.application->revision(), actionIntent(intent));
        if (!options.error.empty()) return fail("invalid_query", options.error);
        Json rows = Json::array();
        for (const auto &option : options.options) rows.push_back({{"value", option.value}, {"label", option.label},
            {"card", std::to_string(option.card)}, {"player", option.player}});
        const auto v = view(viewer);
        return ok({{"schema", schemaVersion}, {"generation", generation}, {"revision", revision}, {"viewer", viewer},
            {"decisionId", v.decision ? std::to_string(v.decision->id) : v.opening ? "opening" : "0"},
            {"field", static_cast<int>(options.field)}, {"options", rows}, {"hasMore", options.hasMore},
            {"pending", options.pending}, {"complete", options.complete ? commandDto(*options.complete) : Json()}});
    } catch (const std::exception &e) { return fail("invalid_query", e.what()); }
}
Json LocalSession::selectIntent(PlayerId viewer, const Json &intent, const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_action", "操作已过期");
    if (!visibleTo(viewer)) return fail("invalid_viewer", "无权提交该查看者操作");
    if (match_.application->paused()) return fail("match_paused", "对局已暂停");
    try {
        const auto options = match_.application->queryActions(viewer, match_.application->generation(), match_.application->revision(), actionIntent(intent));
        if (options.pending) return ok({{"pending", true}});
        if (!options.complete) return fail("incomplete_action", options.error.empty() ? "请完成当前选择" : options.error);
        match_.pending = Pending{viewer, *options.complete, match_.generation, match_.application->revision()};
        return ok({{"command", commandDto(*options.complete)}});
    } catch (const std::exception &e) { return fail("invalid_action", e.what()); }
}
Json LocalSession::submitCommand(PlayerId viewer, const Json &command, const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_action", "操作已过期");
    if (!visibleTo(viewer)) return fail("invalid_viewer", "无权提交该查看者操作");
    if (match_.application->paused()) return fail("match_paused", "对局已暂停");
    try {
        const auto before = view(viewer);
        return accepted(match_.application->submit(viewer, commandFromDto(command), match_.application->generation()), before);
    } catch (const std::exception &e) { return fail("invalid_action", e.what()); }
}
Json LocalSession::eventsSince(PlayerId viewer, const std::string &epoch, const std::string &cursor) const {
    if (!visibleTo(viewer)) return fail("invalid_viewer", "没有此查看者的事件");
    try {
        const auto batch = match_.application->eventsSince(viewer, numericId(epoch), numericId(cursor));
        Json events = Json::array();
        for (const auto &event : batch.events) events.push_back(eventDto(event));
        return ok({{"schema", schemaVersion}, {"generation", std::to_string(match_.generation)}, {"viewer", viewer},
            {"epoch", std::to_string(batch.epoch)}, {"base", std::to_string(batch.base)},
            {"next", std::to_string(batch.next)}, {"reset", batch.reset}, {"events", events}});
    } catch (const std::exception &e) { return fail("invalid_cursor", e.what()); }
}
Json LocalSession::pollMatch(PlayerId viewer, const std::string &generation, const std::string &revision) {
    if (!match_.application || !match_.application->hasMatch()) return ok({{"schema", schemaVersion}, {"active", false}});
    match_.application->tick();
    if (!visibleTo(viewer)) return fail("invalid_viewer", "没有此查看者的对局视图");
    Json data{{"schema", schemaVersion}, {"active", true}, {"generation", std::to_string(match_.generation)},
        {"revision", std::to_string(match_.application->revision())}, {"clock", clockDto(match_.application->clock())},
        {"technicalPaused", match_.application->technicalPaused()}, {"paused", match_.application->paused()}};
    if (!current(generation, revision)) data["snapshot"] = snapshot(viewer).at("data");
    return ok(std::move(data));
}
Json LocalSession::saveDiagnostic() const {
    if (!match_.application || !match_.application->hasMatch()) return fail("no_match", "没有对局");
    std::string error;
    if (!match_.application->saveDiagnostic(error)) return fail("diagnostic_save_failed", error);
    return ok();
}
Json LocalSession::accepted(const CommandResult &result, const GameView &before) {
    if (!result.accepted) return fail(result.errorCode.empty() ? "rejected" : result.errorCode, result.error);
    match_.pending.reset();
    if (result.pending) {
        return ok({{"generation", std::to_string(match_.generation)},
                   {"revision", std::to_string(match_.application->revision())},
                   {"pending", true}, {"replaySaved", false}, {"saveError", ""},
                   {"sounds", Json::array()}, {"cues", Json::array()}});
    }
    if (match_.application->mode() == app::MatchMode::Network)
        match_.application->tickNetwork();
    std::string error;
    bool saved = true;
    const bool terminal = match_.application->technicalPaused() ||
        (match_.application->mode() != app::MatchMode::Network &&
         match_.application->match().engine().state().result != -1);
    const bool batchedAutosave = match_.application->mode() == app::MatchMode::Network ||
        !replaySaveHealthy_ || terminal || match_.application->match().commandCount() == 1 ||
        match_.application->match().commandCount() % replayAutosaveInterval == 0;
    if (batchedAutosave) {
        saved = match_.application->saveReplay(error);
        replaySaveHealthy_ = saved;
    }
    Json sounds = Json::array();
    const auto after = view(before.viewer);
    std::vector<GameEvent> visibleEvents;
    for (const auto &event : result.events) if (event.audience < 0 || event.audience == before.viewer) visibleEvents.push_back(event);
    for (const auto cue : ui::matchSounds(before, after, visibleEvents, !match_.application->aiTurn()))
        sounds.push_back({{"file", ui::soundFile(cue)}, {"gain", ui::soundGain(cue)},
                          {"priority", ui::soundPriority(cue)}, {"cooldownSeconds", ui::soundCooldown(cue)}});
    ui::MatchPresentation presentation;
    presentation.observe(before, after, visibleEvents, match_.application->settings().reducedMotion);
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
    return ok({{"generation", std::to_string(match_.generation)},
               {"revision", std::to_string(match_.application->revision())},
               {"pending", false}, {"replaySaved", saved}, {"saveError", error},
               {"sounds", sounds}, {"cues", cues}});
}
Json LocalSession::confirmAction(const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_action", "确认已过期");
    if (!match_.pending) return fail("no_selection", "请先选择操作");
    if (match_.pending->generation != match_.generation || match_.pending->revision != match_.application->revision()) {
        match_.pending.reset(); return fail("stale_action", "选择已过期");
    }
    if (match_.application->paused()) return fail("match_paused", "对局已暂停");
    const auto selected = *match_.pending;
    const auto before = view(selected.viewer);
    return accepted(match_.application->submit(selected.viewer, selected.command, match_.application->generation()), before);
}
Json LocalSession::cancelAction() { match_.pending.reset(); match_.interaction.cancel(); return ok(); }
Json LocalSession::debugCommand(const Json &request) {
    if (!match_.application || !match_.application->debugAvailable() || replay_.browsing)
        return fail("debug_unavailable", "调试控制台仅限本地 AI 单人对局；回放、教学、联机及观战不可用");
    try {
        const auto operation = decodeDebugOperation(request);
        const auto result = match_.application->debug(operation);
        if (!result.accepted) return fail(result.errorCode, result.error);
        match_.pending.reset(); match_.interaction.cancel();
        std::string saveError;
        const bool saved = match_.application->saveReplay(saveError);
        replaySaveHealthy_ = saved;
        return ok({{"message", result.events.empty() ? "调试操作已执行" : result.events.front().text},
            {"parameters", encodeDebugOperation(operation)}, {"containsDebugOperations", true},
            {"generation", std::to_string(match_.generation)}, {"revision", std::to_string(match_.application->revision())},
            {"replaySaved", saved}, {"saveError", saveError}});
    } catch (const std::exception &e) { return fail("invalid_debug_operation", e.what()); }
}
Json LocalSession::pause(bool paused) {
    if (!match_.application || !match_.application->hasMatch()) return fail("no_match", "没有对局");
    if (paused) match_.application->openSettings(); else match_.application->closeSettings();
    return ok();
}
Json LocalSession::stepAi(const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_ai", "AI操作已过期");
    const auto plan = match_.application->planAi();
    if (!plan) return fail("ai_unavailable", "当前不能提交AI操作");
    const auto before = view(0);
    return accepted(match_.application->commitAi(*plan), before);
}
Json LocalSession::continueTutorial(const std::string &generation, const std::string &revision) {
    if (!current(generation, revision)) return fail("stale_action", "教学操作已过期");
    if (match_.application->paused()) return fail("match_paused", "对局已暂停");
    const auto *tutorial = match_.application->tutorial();
    if (!tutorial || !tutorial->canContinue(view(0))) return fail("tutorial_unavailable", "当前没有教学讲解");
    match_.application->continueTutorial(); match_.pending.reset(); return ok();
}
Json LocalSession::saveReplay() const {
    if (!match_.application || !match_.application->hasMatch()) return fail("no_match", "没有对局");
    if (match_.application->mode() == app::MatchMode::Network && match_.application->network()->view().result == -1)
        return fail("replay_unavailable", "完整联机复盘在对局结束后开放");
    std::string error;
    if (!match_.application->saveReplay(error)) {
        replaySaveHealthy_ = false;
        return fail("save_failed", error);
    }
    replaySaveHealthy_ = true;
    return ok({{"path", match_.application->replayPath().u8string()}});
}
Json LocalSession::restart(std::uint32_t seed) {
    if (!match_.application) return fail("not_initialized", "请先加载内容");
    std::string error;
    if (!match_.application->restart(seed, error)) return fail("restart_failed", error);
    replay_.browsing = false;
    replaySaveHealthy_ = true;
    ++match_.generation; match_.pending.reset(); return ok();
}
Json LocalSession::release() {
    match_.pending.reset(); match_.interaction.cancel(); match_.interactionViewer = -1;
    match_.application.reset(); decks_.content.reset(); decks_.libraryCache.reset();
    replay_ = ReplayBrowser{}; replaySaveHealthy_ = true; ++match_.generation; return ok();
}
Json LocalSession::readReplayPlan(const std::filesystem::path &path) const {
    if (!decks_.content) return fail("not_initialized", "请先加载内容");
    if (match_.application->network() && match_.application->network()->started() && match_.application->network()->view().result == -1)
        return fail("replay_unavailable", "联机对局进行中不能打开完整复盘");
    try {
        prepareReplay(path);
        replay_.browsing = true;
        const auto &j = replay_.record;
        Json commands = Json::array();
        for (const auto &row : j.at("commands"))
            if (row.contains("debug")) commands.push_back(row);
            else commands.push_back({{"actor", row.at("actor")},
                {"command", commandDto(j.value("format", 5) == 4 ? decodeLegacyCommand(row.at("command")) : decodeCommand(row.at("command")))}, {"digest", row.at("digest")}});
        return ok({{"schema", schemaVersion}, {"format", j.at("format")}, {"readOnly", j.value("format", 5) == 4},
                   {"seed", j.at("seed")}, {"players", j.at("players")},
                   {"commands", commands}, {"finalDigest", j.at("finalDigest")}});
    } catch (const std::exception &e) { return fail("invalid_replay", e.what()); }
}
Json LocalSession::verifyReplay(const std::filesystem::path &path) const {
    if (!decks_.content) return fail("not_initialized", "请先加载内容");
    if (match_.application->network() && match_.application->network()->started() && match_.application->network()->view().result == -1)
        return fail("replay_unavailable", "联机对局进行中不能打开完整复盘");
    try {
        prepareReplay(path);
        const auto &j = replay_.record;
        Json digests = Json::array();
        for (const auto &row : j.at("commands")) digests.push_back(row.at("digest"));
        return ok({{"digest", j.at("finalDigest")}, {"commandCount", j.at("commands").size()},
                   {"digests", digests}});
    } catch (const std::exception &e) { return fail("invalid_replay", e.what()); }
}
void LocalSession::prepareReplay(const std::filesystem::path &path) const {
    const auto writeTime = std::filesystem::last_write_time(path);
    const auto fileSize = std::filesystem::file_size(path);
    if (replay_.valid && replay_.path == path && replay_.writeTime == writeTime &&
        replay_.fileSize == fileSize) return;
    replay_.valid = false;
    auto record = readJson(path);
    const auto count = record.at("commands").size();
    // Replay browsing has a fixed memory bound independent of recording length.
    const auto stride = std::max<std::size_t>(8, (count + 14) / 15);
    std::map<std::size_t, GameEngine> checkpoints;
    std::map<std::size_t, std::array<std::vector<GameEvent>, 2>> eventCheckpoints;
    std::array<std::vector<GameEvent>, 2> visibleEvents;
    auto finalEngine = replay(*decks_.content, record, [&](std::size_t step, const GameEngine &engine) {
        if (step % stride == 0) {
            checkpoints.emplace(step, engine);
            eventCheckpoints.emplace(step, visibleEvents);
        }
    }, [&](std::size_t, const std::vector<GameEvent> &batch) {
        for (const auto &event : batch) for (PlayerId viewer = 0; viewer < 2; ++viewer)
            if (event.audience == -1 || event.audience == viewer) {
                auto &history = visibleEvents[viewer];
                history.push_back(event);
                if (history.size() > 512) history.erase(history.begin());
            }
    });
    record.erase("events");
    replay_.record = std::move(record);
    replay_.checkpoints = std::move(checkpoints);
    replay_.eventCheckpoints = std::move(eventCheckpoints);
    replay_.visibleEvents = std::move(visibleEvents);
    replay_.engine = std::make_unique<GameEngine>(std::move(finalEngine));
    replay_.step = count;
    replay_.path = path;
    replay_.writeTime = writeTime;
    replay_.fileSize = fileSize;
    replay_.valid = true;
}
Json LocalSession::replayFrame(const std::filesystem::path &path, std::size_t step, PlayerId viewer) const {
    if (!decks_.content) return fail("not_initialized", "请先加载内容");
    if (match_.application->network() && match_.application->network()->started() && match_.application->network()->view().result == -1)
        return fail("replay_unavailable", "联机对局进行中不能打开完整复盘");
    if (viewer != 0 && viewer != 1) return fail("invalid_viewer", "复盘查看者无效");
    try {
        prepareReplay(path);
        const auto count = replay_.record.at("commands").size();
        if (step > count) return fail("invalid_step", "复盘位置超出范围");
        auto checkpoint = std::prev(replay_.checkpoints.upper_bound(step));
        if (step < replay_.step || checkpoint->first > replay_.step) {
            replay_.engine = std::make_unique<GameEngine>(checkpoint->second);
            replay_.step = checkpoint->first;
            replay_.visibleEvents = replay_.eventCheckpoints.at(checkpoint->first);
        }
        for (std::size_t i = replay_.step; i < step; ++i) {
            const auto &row = replay_.record.at("commands").at(i);
            const auto result = applyRecordedCommand(*replay_.engine, row, replay_.record.value("format", 5) == 4);
            if (!result.accepted)
                throw std::runtime_error("复盘指令未能重放");
            for (const auto &event : result.events) for (PlayerId viewer = 0; viewer < 2; ++viewer)
                if (event.audience == -1 || event.audience == viewer) {
                    auto &history = replay_.visibleEvents[viewer];
                    history.push_back(event);
                    if (history.size() > 512) history.erase(history.begin());
                }
        }
        replay_.step = step;
        auto data = viewDto(replay_.engine->viewFor(viewer));
        replay_.browsing = true;
        data["debug"] = {{"available", false}, {"canRewind", false}, {"reason", "回放中不能调用调试操作"}};
        data["debug"]["players"] = Json::array();
        for (const auto &player : data.at("players"))
            data["debug"]["players"].push_back({{"infiniteLife", player.at("infiniteLife")},
                {"infiniteMana", player.at("infiniteMana")}, {"infiniteLoad", player.at("infiniteLoad")}});
        data["debug"]["used"] = replay_.engine->state().debugUsed;
        data["step"] = step;
        data["commandCount"] = count;
        data["digest"] = replay_.engine->digest();
        // Event history belongs to the replay browser, never its engine checkpoints.
        data["eventLog"] = Json::array();
        for (const auto &event : replay_.visibleEvents[viewer]) data["eventLog"].push_back(eventDto(event));
        return ok(std::move(data));
    } catch (const std::exception &e) { return fail("invalid_replay", e.what()); }
}
} // namespace wizard::bridge
