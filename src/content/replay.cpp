#include "wizard/replay.hpp"
#include "json_integer.hpp"
#include <limits>
#include <type_traits>
namespace wizard {
using detail::integer;
namespace {
MatchConfig sharedConfig(const Content &c, std::uint32_t seed) {
    MatchConfig config;
    config.seed = seed;
    for (auto &p : config.players) {
        p.cards = c.deck;
        p.sideDeck = c.sideDeck;
    }
    return config;
}
}
MatchSession::MatchSession(Content c, std::uint32_t seed) : MatchSession(c, sharedConfig(c, seed)) {}
MatchSession::MatchSession(Content c, MatchConfig config)
    : content_(std::move(c)), config_(std::move(config)), engine_(content_.catalog, config_) {
    appendEvents(engine_.takeInitialEvents());
}
void MatchSession::changed() {
    ++revision_;
    for (auto &cached : viewCache_) cached.reset();
}
void MatchSession::appendEvents(const std::vector<GameEvent> &batch) {
    for (const auto &event : batch) {
        const auto index = events_.size();
        events_.push_back(event);
        for (PlayerId p = 0; p < 2; ++p)
            if (event.audience == -1 || event.audience == p) visibleEvents_[p].push_back(index);
    }
}
void MatchSession::truncateEvents(std::size_t count) {
    events_.resize(count);
    for (auto &indices : visibleEvents_)
        indices.erase(std::lower_bound(indices.begin(), indices.end(), count), indices.end());
    ++eventEpoch_;
}
EventBatch MatchSession::eventsSince(PlayerId viewer, std::uint64_t epoch, std::uint64_t cursor) const {
    if (viewer < 0 || viewer > 1) throw std::invalid_argument("invalid event viewer");
    const auto &indices = visibleEvents_[viewer];
    const auto end = static_cast<std::uint64_t>(indices.size());
    const auto oldest = end > 512 ? end - 512 : 0;
    const bool reset = epoch != eventEpoch_ || cursor > end || cursor < oldest;
    const auto first = reset ? oldest : cursor;
    EventBatch batch{eventEpoch_, first, end, reset, {}};
    for (auto n = first; n < end; ++n) batch.events.push_back(events_[indices[static_cast<std::size_t>(n)]]);
    return batch;
}
void MatchSession::handleFault(PlayerId actor, const Command &command, const CommandResult &result) {
    if (result.errorCode != "execution_budget") return;
    technicalPaused_ = true;
    clockFrozen_ = true;
    clock_.running = false;
    clock_.runningPlayers = {};
    clock_.runningPlayer = -1;
    ++clock_.sequence;
    changed();
    diagnostic_ = {{"format", 1}, {"code", result.errorCode}, {"message", result.error},
                   {"actor", actor}, {"triggerCommand", encodeCommand(command)},
                   {"recording", recording()}, {"state", engine_.canonicalState()}};
}
void MatchSession::saveDiagnostic(const std::filesystem::path &path) const {
    if (!technicalPaused_) throw std::runtime_error("no technical diagnostic");
    writeAtomicJson(path, diagnostic_);
}
CommandResult MatchSession::submit(PlayerId p, const Command &c) {
    tick();
    if (technicalPaused_) return {false, "对局因技术故障暂停，请导出诊断后退出", {}, {}, "technical_paused"};
    if (const auto *choice = std::get_if<Choose>(&c); choice && choice->option.kind == ChoiceKind::Legacy)
        return {false, "选择必须包含明确类型", {}, {}, "invalid_choice_type"};
    auto result = engine_.submit(p, c);
    if (result.accepted) {
        commands_.push_back({p, c, engine_.digest()});
        appendEvents(result.events);
        changed();
        syncClock();
        captureTurn();
    }
    else handleFault(p, c, result);
    return result;
}
Json MatchSession::recording() const {
    Json players = Json::array();
    for (const auto &p : config_.players) {
        Json player = {{"cards", p.cards}, {"sideDeck", p.sideDeck}};
        players.push_back(std::move(player));
    }
    Json j = {{"format", 5}, {"contentSchema", 1}, {"commandSchema", 1}, {"digestSchema", 2}, {"mechanicsRevision", 9},
              {"programVersion", programVersion},
              {"rulesVersion", content_.catalog.rulesVersion},
              {"cardSetVersion", content_.catalog.cardSetVersion},
              {"contentHash", content_.catalog.contentHash},
              {"seed", config_.seed},
              {"players", players},
              {"commands", Json::array()},
              {"finalDigest", engine_.digest()}};
    for (const auto &c : commands_) {
        Json row = {{"actor", c.actor}, {"digest", c.digest}};
        if (c.debug) row["debug"] = encodeDebugOperation(*c.debug);
        else row["command"] = encodeCommand(c.command);
        j["commands"].push_back(std::move(row));
    }
    j["containsDebugOperations"] = engine_.state().debugUsed;
    j["technicalPaused"] = technicalPaused_;
    const auto &s = engine_.state();
    j["statistics"] = {{"turns", {s.players[0].ownTurn, s.players[1].ownTurn}},
                       {"result", s.result},
                       {"firstPlayer", s.first},
                       {"commandCount", commands_.size()}};
    j["events"] = Json::array();
    for (const auto &e : events_)
        j["events"].push_back({{"kind", e.kind},
                               {"text", e.text},
                               {"audience", e.audience},
                               {"card", e.card},
                               {"amount", e.amount},
                               {"effectType", e.effectType ? Json(static_cast<int>(*e.effectType)) : Json()},
                               {"actualAmount", e.actualAmount},
                               {"affectedPlayer", e.affectedPlayer},
                               {"spellReleased", e.spellReleased}});
    return j;
}
void MatchSession::save(const std::filesystem::path &path) const {
    writeAtomicJson(path, recording());
    if (technicalPaused_) {
        auto diagnosticPath = path;
        diagnosticPath += ".diagnostic.json";
        saveDiagnostic(diagnosticPath);
    }
}
GameEngine replay(const Content &c, const Json &j,
                  const std::function<void(std::size_t, const GameEngine&)> &observer,
                  const std::function<void(std::size_t, const std::vector<GameEvent>&)> &eventObserver) {
    const int format = j.at("format").get<int>();
    const bool legacy = format == 4;
    if ((!legacy && format != 5) ||
        (legacy && j.at("programVersion") != "2.5.5-hotfix.1") ||
        (!legacy && (j.at("programVersion") != programVersion || j.at("contentSchema") != 1 || j.at("commandSchema") != 1 || j.at("digestSchema") != 2 ||
                     j.at("mechanicsRevision") != 9)) ||
        j.at("rulesVersion") != "2.5.5" || c.catalog.rulesVersion != "2.5.5" ||
        j.at("cardSetVersion") != c.catalog.cardSetVersion || j.at("contentHash") != c.catalog.contentHash)
        throw std::runtime_error("incompatible replay version or content hash");
    MatchConfig config;
    config.seed =
        static_cast<std::uint32_t>(integer(j.at("seed"), std::numeric_limits<std::uint32_t>::max()));
    if (!j.at("players").is_array() || j.at("players").size() != 2)
        throw std::runtime_error("replay requires two player decks");
    for (int p = 0; p < 2; ++p) {
        config.players[p].cards = j.at("players").at(p).at("cards").get<std::vector<std::string>>();
        config.players[p].sideDeck = j.at("players").at(p).at("sideDeck").get<std::vector<std::string>>();
    }
    GameEngine engine(c.catalog, config);
    if (!j.at("commands").is_array())
        throw std::runtime_error("invalid replay commands");
    int n = 0;
    const auto initialEvents = engine.takeInitialEvents();
    if (eventObserver) eventObserver(0, initialEvents);
    if (observer) observer(0, engine);
    for (const auto &row : j.at("commands")) {
        if (row.contains("debug") && !j.value("containsDebugOperations", false))
            throw std::runtime_error("debug replay operation missing debug marker");
        const auto result = applyRecordedCommand(engine, row, legacy);
        if (!result.accepted || (legacy ? engine.legacyDigest() : engine.digest()) != row.at("digest").get<std::string>()) {
            throw std::runtime_error("replay diverged at command " + std::to_string(n));
        }
        ++n;
        if (eventObserver) eventObserver(static_cast<std::size_t>(n), result.events);
        if (observer) observer(static_cast<std::size_t>(n), engine);
    }
    if ((legacy ? engine.legacyDigest() : engine.digest()) != j.at("finalDigest").get<std::string>())
        throw std::runtime_error("final digest mismatch");
    return engine;
}
} // namespace wizard
