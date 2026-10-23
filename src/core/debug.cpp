#include "wizard/core/engine.hpp"
#include <algorithm>
#include <limits>
namespace wizard {
CommandResult GameEngine::debug(const DebugOperation &op) {
    auto rejected = [&](const std::string &reason) -> CommandResult {
        return {false, reason, {}, state_.decision, "invalid_debug_operation"};
    };
    if (op.player < 0 || op.player > 1) return rejected("玩家必须为 1 或 2");
    if (state_.opening) return rejected("请先完成初始阵法选择");
    if (state_.result != -1 && op.operation != "rewind_previous_turn") return rejected("对局已结束，请回到上一回合");
    const bool numeric = op.operation == "gain_mana" || op.operation == "lose_mana" ||
        op.operation == "gain_life" || op.operation == "lose_life" ||
        op.operation == "gain_temporary_life" || op.operation == "lose_temporary_life" ||
        op.operation == "gain_load" || op.operation == "clear_load";
    if (numeric && (op.amount <= 0 || op.amount > 1000000)) return rejected("数量必须为 1 至 1000000 的整数");
    GameEngine next = *this;
    auto &s = next.state_;
    auto &p = s.players[op.player];
    CardId changedCard{};
    std::string text;
    if (op.operation == "add_card") {
        const CardDefinition *definition = nullptr;
        const auto found = catalog_.cards.find(op.cardId);
        if (found != catalog_.cards.end()) definition = &found->second;
        else for (const auto &[id, d] : catalog_.cards) if (d.name == op.cardId) {
            if (definition) return rejected("卡名不唯一，请使用卡牌 ID");
            definition = &d;
        }
        if (!definition) return rejected("未找到卡牌 ID 或名称：" + op.cardId);
        const auto hand = std::count_if(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
            return entry.second.owner == op.player && entry.second.zone == Zone::Hand;
        });
        if (hand >= 8) return rejected("手牌已达到上限 8，未加入卡牌");
        if (!s.cards.empty() && s.cards.rbegin()->first >= playerTarget(0) - 1)
            return rejected("卡牌实例 ID 已耗尽");
        CardInstance card;
        card.id = s.cards.empty() ? 1 : s.cards.rbegin()->first + 1;
        card.owner = op.player; card.zone = Zone::Hand; card.definition = definition->id;
        changedCard = card.id; s.cards.emplace(card.id, card);
        text = "从虚空加入手牌：" + definition->name + " #" + std::to_string(card.id) + "（不计抽牌）";
    } else if (op.operation == "gain_mana" || op.operation == "lose_mana") {
        const int before = p.mana;
        p.mana = op.operation == "gain_mana" ? std::min(catalog_.usesAlphaV25() ? maximumMana : 12, p.mana + op.amount) :
            std::max(0, p.mana - op.amount);
        text = "魔素 " + std::to_string(before) + " → " + std::to_string(p.mana);
    } else if (op.operation == "gain_life" || op.operation == "lose_life") {
        const int before = p.life;
        p.life = op.operation == "gain_life" ? std::min(maximumLife, p.life + op.amount) : std::max(0, p.life - op.amount);
        text = "普通生命 " + std::to_string(before) + " → " + std::to_string(p.life);
    } else if (op.operation == "gain_temporary_life" || op.operation == "lose_temporary_life") {
        if (op.operation == "gain_temporary_life") {
            if (p.temporaryLife > 1000000 - op.amount) return rejected("临时生命总量不能超过 1000000");
            ResourceRules::grantTemporaryLife(s, op.player, 0, op.amount, p.ownTurn + 1);
        } else ResourceRules::absorbTemporaryLife(s, op.player, op.amount);
        text = "临时生命调整为 " + std::to_string(p.temporaryLife);
    } else if (op.operation == "gain_load" || op.operation == "clear_load") {
        const bool bound = op.loadKind == "bound", independent = op.loadKind == "independent";
        if (!bound && !independent && op.loadKind != "temporary") return rejected("荷载类型必须为 bound、independent 或 temporary");
        if (bound) {
            const auto source = s.cards.find(op.sourceId);
            if (!op.sourceId || source == s.cards.end() || source->second.owner != op.player ||
                (source->second.zone != Zone::Analysis && source->second.zone != Zone::Casting &&
                 source->second.zone != Zone::Attached && source->second.zone != Zone::Words))
                return rejected("绑定荷载需要该玩家场上有效的来源卡牌实例 ID");
        } else if (op.sourceId) return rejected("独立和临时荷载不接受来源 ID");
        if (op.operation == "gain_load") {
            if (Rules::load(s, op.player) > 1000000 - op.amount) return rejected("荷载总量不能超过 1000000");
            s.temporary.push_back({s.nextLoad++, op.player, bound ? op.sourceId : 0, op.amount,
                bound || independent ? 0 : p.ownTurn + 1, independent, bound});
        } else {
            int remaining = op.amount;
            // Bound analysis/cast costs belong to their card as well as explicit source-bound entries.
            if (bound) {
                auto &card = s.cards.at(op.sourceId);
                const int analysis = std::min(remaining, card.analysisLoad);
                card.analysisLoad -= analysis; remaining -= analysis;
                const int cast = std::min(remaining, card.castLoad);
                card.castLoad -= cast; remaining -= cast;
            }
            for (auto &load : s.temporary) {
                if (load.owner != op.player || load.sourceBound != bound || load.independent != independent ||
                    (bound && load.source != op.sourceId)) continue;
                const int cleared = std::min(remaining, load.amount);
                load.amount -= cleared; remaining -= cleared;
            }
            s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                [](const TemporaryLoad &load) { return load.amount == 0; }), s.temporary.end());
        }
        text = "调整" + std::string(bound ? "绑定" : independent ? "独立" : "临时") +
            "荷载，当前总荷载 " + std::to_string(Rules::load(s, op.player));
    } else if (op.operation == "set_infinite_life" || op.operation == "set_infinite_mana" || op.operation == "set_infinite_load") {
        if (op.operation == "set_infinite_life") p.debugInfiniteLife = op.enabled;
        if (op.operation == "set_infinite_mana") p.debugInfiniteMana = op.enabled;
        if (op.operation == "set_infinite_load") p.debugInfiniteLoad = op.enabled;
        text = std::string(op.enabled ? "开启" : "关闭") +
            (op.operation == "set_infinite_life" ? "无限生命" : op.operation == "set_infinite_mana" ? "无限魔素" : "无限荷载上限");
    } else if (op.operation == "rewind_previous_turn") {
        // The authority session restores its validated checkpoint and truncates the recording first.
        text = "回到上一回合开始，随机数状态与回放位置已恢复";
    } else return rejected("未知调试操作：" + op.operation);
    const auto start = s.events.size();
    s.debugUsed = true;
    s.events.push_back({"debug", "[调试] 玩家 " + std::to_string(op.player + 1) + "：" + text +
        "；命令=" + op.operation + "；数量=" + std::to_string(op.amount), -1, changedCard, op.amount});
    if (op.operation == "gain_load") ResourceRules::increasedLoad(s, catalog_, op.player, op.amount);
    // Direct console changes are literal; infinite switches only alter rule judgments.
    // Disabling capacity immunity resumes the ordinary current-state overload check immediately.
    if (op.operation == "set_infinite_load" && !op.enabled) {
        const int excess = std::max(0, Rules::load(s, op.player) - Rules::capacity(s, catalog_, op.player));
        if (excess) ResourceRules::damage(s, op.player, excess * 2, "overload", &catalog_);
        StateMaintenance::check(s, catalog_, true);
    } else StateMaintenance::check(s, catalog_);
    const auto errors = Rules::invariants(s, catalog_);
    if (!errors.empty()) return rejected("调试操作破坏规则状态：" + errors.front());
    std::vector<GameEvent> events(s.events.begin() + static_cast<std::ptrdiff_t>(start), s.events.end());
    *this = std::move(next);
    return {true, {}, std::move(events), state_.decision, {}};
}
}
