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
CommandResult applyRecordedCommand(GameEngine &engine, const Json &row) {
    if (!row.at("actor").is_number_integer() || row.at("actor").get<int>() < 0 || row.at("actor").get<int>() > 1)
        throw std::invalid_argument("无效回放玩家");
    if (row.contains("debug")) {
        if (row.contains("command")) throw std::invalid_argument("回放行不能同时包含常规和调试操作");
        return engine.debug(decodeDebugOperation(row.at("debug")));
    }
    const auto command = decodeCommand(row.at("command"));
    const auto actor = row.at("actor").get<int>();
    return std::holds_alternative<Timeout>(command) ? engine.timeout(actor) : engine.submit(actor, command);
}
void MatchSession::captureTurn() {
    const auto &s = engine_.state();
    if (s.opening || s.result != -1) return;
    if (turnCheckpoints_.empty() || turnCheckpoints_.back().engine.state().globalTurn != s.globalTurn)
        turnCheckpoints_.push_back({engine_, commands_.size(), clock_, replenishedTurn_});
}
bool MatchSession::canRewindPreviousTurn() const {
    return debugEnabled_ && std::any_of(turnCheckpoints_.begin(), turnCheckpoints_.end(), [&](const TurnCheckpoint &c) {
        return c.engine.state().globalTurn < engine_.state().globalTurn;
    });
}
CommandResult MatchSession::debug(const DebugOperation &op) {
    if (!debugEnabled_) return {false, "调试控制台仅限本地 AI 单人对局", {}, {}, "debug_unavailable"};
    if (op.operation == "rewind_previous_turn") {
        if (!canRewindPreviousTurn()) return {false, "没有可用的上一回合开始快照", {}, {}, "no_previous_turn"};
        const auto at = std::find_if(turnCheckpoints_.rbegin(), turnCheckpoints_.rend(), [&](const TurnCheckpoint &c) {
            return c.engine.state().globalTurn < engine_.state().globalTurn;
        });
        // Validate the operation before altering any live state.
        auto restored = at->engine;
        auto result = restored.debug(op);
        if (!result.accepted) return result;
        const auto position = at->position;
        clock_ = at->clock; replenishedTurn_ = at->replenishedTurn;
        const auto turn = at->engine.state().globalTurn;
        engine_ = std::move(restored);
        commands_.resize(position);
        turnCheckpoints_.erase(std::remove_if(turnCheckpoints_.begin(), turnCheckpoints_.end(),
            [&](const TurnCheckpoint &c) { return c.engine.state().globalTurn > turn; }), turnCheckpoints_.end());
        commands_.push_back({op.player, Advance{}, engine_.digest(), op});
        syncClock();
        return result;
    }
    const auto result = engine_.debug(op);
    if (result.accepted) {
        commands_.push_back({op.player, Advance{}, engine_.digest(), op});
        syncClock();
    }
    return result;
}
}
