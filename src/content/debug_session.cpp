#include "wizard/replay.hpp"
#include <algorithm>
#include <limits>
#include <stdexcept>
namespace wizard {
Json encodeDebugOperation(const DebugOperation &op) {
    return {{"operation", op.operation}, {"player", op.player}, {"amount", op.amount},
        {"cardId", op.cardId}, {"loadKind", op.loadKind}, {"sourceId", std::to_string(op.sourceId)}, {"enabled", op.enabled}};
}
DebugOperation decodeDebugOperation(const Json &j) {
    if (!j.is_object()) throw std::invalid_argument("调试参数必须为对象");
    for (const auto &[key, value] : j.items()) {
        (void)value;
        if (key != "operation" && key != "player" && key != "amount" && key != "cardId" &&
            key != "loadKind" && key != "sourceId" && key != "enabled")
            throw std::invalid_argument("未知调试参数：" + key);
    }
    DebugOperation op;
    op.operation = j.at("operation").get<std::string>();
    auto integer = [&](const char *key, int fallback) {
        if (!j.contains(key)) return fallback;
        if (!j.at(key).is_number_integer()) throw std::invalid_argument(std::string(key) + " 必须为整数");
        const auto value = j.at(key).get<std::int64_t>();
        if (value < 0 || value > 1000000) throw std::invalid_argument(std::string(key) + " 超出范围");
        return static_cast<int>(value);
    };
    op.player = integer("player", 0); op.amount = integer("amount", 0);
    op.cardId = j.value("cardId", std::string{}); op.loadKind = j.value("loadKind", std::string{});
    if (j.contains("enabled")) {
        if (!j.at("enabled").is_boolean()) throw std::invalid_argument("enabled 必须为布尔值");
        op.enabled = j.at("enabled").get<bool>();
    }
    if (j.contains("sourceId")) {
        if (!j.at("sourceId").is_string()) throw std::invalid_argument("sourceId 必须为字符串实例 ID");
        const auto source = j.at("sourceId").get<std::string>();
        if (!source.empty()) {
            if (source.size() > 10 || !std::all_of(source.begin(), source.end(), [](char c) { return c >= '0' && c <= '9'; }))
                throw std::invalid_argument("sourceId 必须为有效的数字实例 ID");
            const auto number = std::stoull(source);
            if (number >= playerTarget(0)) throw std::invalid_argument("来源实例 ID 超出范围");
            op.sourceId = static_cast<CardId>(number);
        }
    }
    return op;
}
CommandResult applyRecordedCommand(GameEngine &engine, const Json &row, bool legacy) {
    if (!row.at("actor").is_number_integer() || row.at("actor").get<int>() < 0 || row.at("actor").get<int>() > 1)
        throw std::invalid_argument("无效回放玩家");
    if (row.contains("debug")) {
        if (row.contains("command")) throw std::invalid_argument("回放行不能同时包含常规和调试操作");
        return engine.debug(decodeDebugOperation(row.at("debug")));
    }
    const auto command = legacy ? decodeLegacyCommand(row.at("command")) : decodeCommand(row.at("command"));
    const auto actor = row.at("actor").get<int>();
    return std::holds_alternative<Timeout>(command) ? engine.timeout(actor) : engine.submit(actor, command);
}
void MatchSession::enableDebug(bool enabled) {
    if (debugEnabled_ == enabled) return;
    debugEnabled_ = enabled;
    turnCheckpoints_.clear();
    turnMarkers_.clear();
    if (enabled) captureTurn();
}
void MatchSession::captureTurn() {
    const auto &s = engine_.state();
    if (!debugEnabled_ || s.opening || s.result != -1 || technicalPaused_) return;
    if (!turnMarkers_.empty() && turnMarkers_.back().turn == s.globalTurn) return;
    turnMarkers_.push_back({s.globalTurn, commands_.size(), events_.size(), clock_, replenishedTurn_});
    turnCheckpoints_.push_back({engine_, commands_.size(), clock_, replenishedTurn_, events_.size()});
    if (turnCheckpoints_.size() > 16) turnCheckpoints_.erase(turnCheckpoints_.begin());
}
bool MatchSession::canRewindPreviousTurn() const {
    return debugEnabled_ && !technicalPaused_ && std::any_of(turnMarkers_.begin(), turnMarkers_.end(), [&](const TurnMarker &c) {
        return c.turn < engine_.state().globalTurn;
    });
}
CommandResult MatchSession::debug(const DebugOperation &op) {
    if (!debugEnabled_) return {false, "调试控制台仅限本地 AI 单人对局", {}, {}, "debug_unavailable"};
    if (technicalPaused_) return {false, "技术暂停期间不能修改对局", {}, {}, "technical_paused"};
    if (op.operation == "rewind_previous_turn") {
        if (!canRewindPreviousTurn()) return {false, "没有可用的上一回合开始快照", {}, {}, "no_previous_turn"};
        const auto marker = *std::find_if(turnMarkers_.rbegin(), turnMarkers_.rend(), [&](const TurnMarker &c) {
            return c.turn < engine_.state().globalTurn;
        });
        auto cached = std::find_if(turnCheckpoints_.begin(), turnCheckpoints_.end(), [&](const TurnCheckpoint &c) {
            return c.position == marker.position;
        });
        auto restored = cached != turnCheckpoints_.end() ? cached->engine : GameEngine(content_.catalog, config_);
        if (cached == turnCheckpoints_.end()) {
            restored.takeInitialEvents();
            for (std::size_t i = 0; i < marker.position; ++i) {
                const auto &row = commands_[i];
                const auto result = row.debug ? restored.debug(*row.debug) :
                    std::holds_alternative<Timeout>(row.command) ? restored.timeout(row.actor) : restored.submit(row.actor, row.command);
                if (!result.accepted || restored.digest() != row.digest)
                    return {false, "历史检查点重建失败", {}, {}, "checkpoint_diverged"};
            }
        }
        auto result = restored.debug(op);
        if (!result.accepted) return result;
        clock_ = marker.clock; replenishedTurn_ = marker.replenishedTurn;
        engine_ = std::move(restored);
        commands_.resize(marker.position);
        truncateEvents(marker.eventCount);
        turnCheckpoints_.erase(std::remove_if(turnCheckpoints_.begin(), turnCheckpoints_.end(),
            [&](const TurnCheckpoint &c) { return c.engine.state().globalTurn > marker.turn; }), turnCheckpoints_.end());
        turnMarkers_.erase(std::remove_if(turnMarkers_.begin(), turnMarkers_.end(),
            [&](const TurnMarker &c) { return c.turn > marker.turn; }), turnMarkers_.end());
        commands_.push_back({op.player, Advance{}, engine_.digest(), op});
        appendEvents(result.events);
        changed();
        syncClock();
        return result;
    }
    const auto result = engine_.debug(op);
    if (result.accepted) {
        commands_.push_back({op.player, Advance{}, engine_.digest(), op});
        appendEvents(result.events);
        changed();
        syncClock();
    }
    return result;
}
}
