#include "engine_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard {
using namespace detail;
std::string GameEngine::prepare(PlayerId actor, const PrepareCast &cmd, bool deferred) {
    auto &s = state_;
    auto i = s.cards.find(cmd.card);
    if (i == s.cards.end() || i->second.owner != actor)
        return "invalid_card: 无效法术";
    auto &c = i->second;
    const auto &def = catalog_.at(c.definition);
    auto &pl = s.players[actor];
    if (def.type != CardType::Analytic || c.zone != Zone::Analysis || c.spell != SpellState::Ready ||
        c.canceledTurn == pl.ownTurn)
        return "not_ready: 法术未就绪或本回合已被取消";
    if (!deferred && cmd.decision)
        return "stale_decision: 效果选择已过期";
    for (const auto &pending : s.deferredPreparations)
        if (pending.item.source == cmd.card)
            return "not_ready: 法术已经宣告准备";
    if (s.chain)
        for (const auto &link : s.chain->links)
            if (link.kind == LinkKind::Preparation && link.item.source == cmd.card)
                return "not_ready: 法术已经宣告准备";
    if (!validTargets(s, def.target, def.targetCount, actor, cmd.target, cmd.targets))
        return "target: 无效目标";
    auto discard = s.cards.find(cmd.discard);
    if ((def.extraDiscard && (discard == s.cards.end() || discard->second.owner != actor ||
                              discard->second.zone != Zone::Hand)) ||
        (!def.extraDiscard && cmd.discard))
        return "extra_cost: 额外成本不合法";
    const int castFee = Rules::castCost(s, catalog_, c.id);
    if (pl.mana < castFee || Rules::load(s, actor) + castFee > Rules::capacity(s, catalog_, actor))
        return "cost: 魔素或荷载不足";
    if (cmd.discard)
        StateMaintenance::leave(s, cmd.discard, &catalog_);
    pl.mana -= castFee;
    s.events.push_back({"pay", "准备支付 " + std::to_string(castFee) + " 魔素", -1, c.id, castFee});
    c.castLoad += castFee;
    c.payments.push_back({pl.ownTurn, castFee, castFee, false});
    c.targetCard = cmd.target;
    c.targets = targetCards(cmd.target, cmd.targets);
    c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
    ChainLink root;
    root.kind = LinkKind::Preparation;
    root.speed = catalog_.advancedRules ? def.speed : 0;
    root.castCost = castFee;
    root.item = snapshot(c, def);
    root.item.effects.clear();
    root.item.requiresSource = true;
    root.paymentIndex = static_cast<int>(c.payments.size()) - 1;
    root.preparationLoad = castFee;
    root.paid = castFee;
    root.addedLoad = castFee;
    root.discarded = cmd.discard;
    if (deferred) {
        s.deferredPreparations.push_back(std::move(root));
        s.events.push_back({"prepare_deferred", "效果宣告准备；当前连锁结束后开放响应", -1, c.id});
    } else
        startChain(ResponseWindow::Prepare, std::move(root), 1 - actor);
    s.phaseGate = 0;
    return {};
}
std::vector<PrepareCast> GameEngine::preparationsFor(PlayerId actor, DecisionId id) const {
    std::vector<PrepareCast> out;
    for (const auto &[cid, c] : state_.cards)
        if (c.owner == actor && c.zone == Zone::Analysis && c.spell == SpellState::Ready) {
            const auto &def = catalog_.at(c.definition);
            std::vector<CardId> discards{0};
            if (def.extraDiscard)
                discards = inZone(state_, actor, Zone::Hand);
            for (const auto &targets : sets(state_, def.target, def.targetCount, actor))
                for (auto discard : discards) {
                    PrepareCast cmd{cid, targets.empty() ? 0 : targets.front(), discard, targets, id};
                    auto trial = *this;
                    if (trial.prepare(actor, cmd, true).empty())
                        out.push_back(std::move(cmd));
                }
        }
    return out;
}
std::string GameEngine::execute(PlayerId actor, const Command &command) {
    auto &s = state_;
    if (actor < 0 || actor > 1)
        return "invalid_player: 无效玩家";
    if (s.result != -1)
        return "game_over: 对局已经结束";
    if (std::holds_alternative<Surrender>(command)) {
        s.players[actor].surrendered = true;
        StateMaintenance::check(s, catalog_);
        return {};
    }
    if (s.decision) {
        const auto d = *s.decision;
        if (actor != d.player)
            return "decision_mismatch: 请完成当前玩家的选择";
        if (d.kind == DecisionKind::Response) {
            Command selected = command;
            if (auto old = std::get_if<Choose>(&selected)) {
                if (old->decision != d.id)
                    return "stale_decision: 选择已过期";
                if (!old->option)
                    selected = PassResponse{d.id};
                else {
                    auto candidates = responsesFor(actor, d.id);
                    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                                    [&](const Respond &r) { return r.card != old->option; }),
                                     candidates.end());
                    if (candidates.size() != 1)
                        return "ambiguous_response: 请指定响应能力与目标";
                    selected = candidates.front();
                }
            }
            if (auto pass = std::get_if<PassResponse>(&selected)) {
                if (pass->decision != d.id)
                    return "stale_decision: 选择已过期";
                s.decision.reset();
                passResponse();
                return {};
            }
            auto r = std::get_if<Respond>(&selected);
            if (!r || r->decision != d.id)
                return "decision_mismatch: 请提交当前响应";
            auto error = validateResponse(actor, *r);
            if (!error.empty())
                return error;
            auto &c = s.cards.at(r->card);
            const auto &def = catalog_.at(c.definition);
            const auto a = *ability(def, r->ability);
            const bool hand = c.zone == Zone::Hand;
            const bool analytic = c.zone == Zone::Analysis && !c.faceDown;
            const bool ambush = c.faceDown;
            const int paid =
                analytic ? Rules::castCost(s, catalog_, c.id)
                : ambush ? def.cost
                : hand   ? a.handCost
                         : a.preloadedCost +
                             (def.type == CardType::Word ? Rules::castCost(s, catalog_, c.id) - def.cost : 0);
            const int bound = analytic || def.type == CardType::Word ? paid : 0;
            s.players[actor].mana -= paid;
            if (r->discard)
                StateMaintenance::leave(s, r->discard, &catalog_);
            if (a.burden)
                s.temporary.push_back(
                    {s.nextLoad++, actor, c.id, a.burden, s.players[actor].ownTurn + (actor != s.active)});
            if ((hand || ambush) && def.type == CardType::Word)
                c.settingPaid = paid;
            c.castLoad += bound;
            c.zone = analytic ? Zone::Casting : Zone::Resolving;
            c.faceDown = false;
            c.host = 0;
            c.spell = analytic ? SpellState::Pending : SpellState::None;
            c.targetCard = r->target;
            c.targets = targetCards(r->target, r->targets);
            c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
            c.payments.push_back({s.players[actor].ownTurn, paid, bound, false});
            ChainLink link;
            link.id = s.nextLink++;
            link.kind = analytic ? LinkKind::Spell : LinkKind::Response;
            link.speed = catalog_.advancedRules ? def.speed : 0;
            link.castCost = Rules::castCost(s, catalog_, c.id);
            link.ability = a.id;
            link.paymentIndex = static_cast<int>(c.payments.size()) - 1;
            link.paid = paid;
            link.addedLoad = bound + a.burden;
            link.discarded = r->discard;
            link.item.owner = actor;
            link.item.source = c.id;
            link.item.targetCard = r->target;
            link.item.targetCards = targetCards(r->target, r->targets);
            link.item.targetPlayer = a.target == TargetKind::Opponent ? 1 - actor : actor;
            link.item.effects = a.effects;
            for (const auto &[sid, seal] : s.cards)
                if (seal.zone == Zone::Attached && seal.host == c.id &&
                    catalog_.at(seal.definition).damageBonus)
                    link.item.effects.push_back({EffectKind::Damage, catalog_.at(seal.definition).damageBonus,
                                                 DamageType::Force, EffectRecipient::Opponent});
            link.item.target = a.target;
            link.item.targetLink = r->link;
            link.item.requiresSource = a.requiresSource;
            s.chain->links.push_back(std::move(link));
            s.chain->responded.push_back(c.id);
            s.chain->passes = 0;
            s.chain->priority = 1 - actor;
            s.decision.reset();
            s.events.push_back({"pay", "响应支付 " + std::to_string(paid) + " 魔素", -1, c.id, paid});
            s.events.push_back(
                {"link_declared", "响应链节 #" + std::to_string(s.chain->links.back().id), -1, c.id});
            StateMaintenance::check(s, catalog_);
            return {};
        }
        if (d.kind == DecisionKind::EffectPrepare && std::holds_alternative<PrepareCast>(command)) {
            const auto &cmd = std::get<PrepareCast>(command);
            if (cmd.decision != d.id)
                return "stale_decision: 效果选择已过期";
            auto error = prepare(actor, cmd, true);
            if (!error.empty())
                return error;
            s.decision.reset();
            ++s.effect->cursor;
            return {};
        }
        auto choose = std::get_if<Choose>(&command);
        if (!choose || choose->decision != d.id)
            return "decision_mismatch: 请完成当前选择";
        auto opt = choose->option;
        if (d.kind == DecisionKind::EffectPrepare && opt)
            return "decision_mismatch: 请指定完整的准备施法操作";
        if (!(opt == 0 && d.mayPass) && std::find(d.options.begin(), d.options.end(), opt) == d.options.end())
            return "invalid_option: 无效选择";
        s.decision.reset();
        s.events.push_back({"choice", "玩家 " + std::to_string(actor + 1) + " 完成选择"});
        switch (d.kind) {
        case DecisionKind::CastOrder:
            if (opt)
                beginEffect(opt, AfterEffect::Spell);
            else {
                s.phaseStep = PhaseStep::EndWindow;
                s.phaseGate = 0;
                s.events.push_back({"cast_pass", "本阶段不再释放；待释放法术及已支付荷载保留"});
            }
            break;
        case DecisionKind::TriggerOrder: {
            auto i = std::find_if(s.queue.items.begin(), s.queue.items.end(),
                                  [&](const Trigger &t) { return t.id == opt; });
            auto t = *i;
            s.queue.items.erase(i);
            startTrigger(std::move(t));
            break;
        }
        case DecisionKind::Discard:
            StateMaintenance::leave(s, opt, &catalog_);
            StateMaintenance::check(s, catalog_);
            break;
        case DecisionKind::Overflow:
            StateMaintenance::leave(s, opt, &catalog_);
            break;
        case DecisionKind::ClearLoad:
            if (!opt) {
                ++s.effect->cursor;
                s.effect->clearRemaining = -1;
            } else {
                auto i = std::find_if(s.temporary.begin(), s.temporary.end(),
                                      [&](const TemporaryLoad &t) { return t.id == opt; });
                --i->amount;
                --s.effect->clearRemaining;
                s.events.push_back({"load", "移除1点临时荷载"});
            }
            break;
        case DecisionKind::SearchDeck: {
            auto &deck = s.players[actor].deck;
            deck.erase(std::find(deck.begin(), deck.end(), opt));
            s.cards.at(opt).zone = Zone::Hand;
            s.events.push_back({"search", "检索一张行动卡加入手牌", actor, opt});
            for (std::size_t i = deck.size(); i > 1; --i)
                std::swap(deck[i - 1], deck[bounded(s, static_cast<std::uint32_t>(i))]);
            ++s.effect->cursor;
            break;
        }
        case DecisionKind::DestroyAmbush:
        case DecisionKind::DestroyOwnFormation:
            if (opt)
                StateMaintenance::leave(s, opt, &catalog_);
            ++s.effect->cursor;
            break;
        case DecisionKind::EffectDiscard:
            StateMaintenance::leave(s, opt, &catalog_);
            ++s.effect->cursor;
            break;
        case DecisionKind::EffectPrepare:
            ++s.effect->cursor; // explicit decline, no declaration
            break;
        case DecisionKind::Concentration: {
            CardId source = 0;
            for (const auto &[id, c] : s.cards)
                if (c.owner == actor && c.zone == Zone::Casting && c.spell == SpellState::Active &&
                    catalog_.at(c.definition).concentration)
                    source = id;
            auto &c = s.cards.at(source);
            if (opt) {
                const int paid = c.concentrationCost;
                s.players[actor].mana -= paid;
                c.castLoad += paid;
                c.payments.push_back({s.players[actor].ownTurn, paid, paid, false});
                s.events.push_back({"concentration", "维持专注：支付 " + std::to_string(paid) + " 魔素及荷载",
                                    -1, source, paid});
            } else {
                StateMaintenance::leave(s, source, &catalog_);
                StateMaintenance::check(s, catalog_);
            }
            break;
        }
        case DecisionKind::Response:
            break;
        }
        return {};
    }
    if (std::holds_alternative<Choose>(command) || std::holds_alternative<Respond>(command) ||
        std::holds_alternative<PassResponse>(command))
        return "stale_decision: 选择已过期";
    if (std::holds_alternative<AdvancePhase>(command) || std::holds_alternative<Advance>(command)) {
        if (std::holds_alternative<Advance>(command) && s.phase != Phase::Main)
            return "wrong_phase: 旧推进命令仅适用于主要阶段";
        auto gate = std::get_if<AdvancePhase>(&command);
        if (actor != s.active || s.phaseStep != PhaseStep::Body || !s.phaseGate ||
            (gate && gate->gate != s.phaseGate) || s.chain || s.effect || !s.queue.items.empty())
            return "phase_gate: 当前阶段不能结束";
        s.phaseGate = 0;
        s.phaseStep = PhaseStep::EndWindow;
        return {};
    }
    if (const auto *cmd = std::get_if<ActivateSpell>(&command)) {
        if (!catalog_.advancedRules || actor != s.active || s.phaseStep != PhaseStep::Body || s.chain ||
            s.effect || !s.queue.items.empty())
            return "wrong_phase: 当前不能主动释放";
        auto it = s.cards.find(cmd->card);
        if (it == s.cards.end() || it->second.owner != actor)
            return "invalid_card";
        auto &c = it->second;
        const auto &d = catalog_.at(c.definition);
        if (c.faceDown || d.responseOnly || d.extraDiscard ||
            (d.type == CardType::Analytic && c.canceledTurn == s.players[actor].ownTurn) ||
            (d.speed <= 2 && s.phase != Phase::Main && s.phase != Phase::Cast) ||
            !((d.type == CardType::Word && c.zone == Zone::Words) ||
              (d.type == CardType::Analytic && c.zone == Zone::Analysis && c.spell == SpellState::Ready &&
               s.phase == Phase::Cast)))
            return "not_ready: 法术不能在此时释放";
        if (!validTargets(s, d.target, d.targetCount, actor, cmd->target, cmd->targets))
            return "target: 无效目标";
        const int fee = Rules::castCost(s, catalog_, c.id) - (d.type == CardType::Word ? d.cost : 0);
        if (s.players[actor].mana < fee || Rules::load(s, actor) + fee > Rules::capacity(s, catalog_, actor))
            return "cost: 魔素或荷载不足";
        s.players[actor].mana -= fee;
        c.castLoad += fee;
        c.payments.push_back({s.players[actor].ownTurn, fee, fee, false});
        s.events.push_back({"pay", "释放支付 " + std::to_string(fee) + " 魔素", -1, c.id, fee});
        c.zone = Zone::Casting;
        c.spell = SpellState::Pending;
        c.host = 0;
        c.targetCard = cmd->target;
        c.targets = targetCards(cmd->target, cmd->targets);
        c.targetPlayer = d.target == TargetKind::Opponent ? 1 - actor : actor;
        beginEffect(c.id, AfterEffect::Spell);
        return {};
    }
    if (const auto *cmd = std::get_if<FlipAmbush>(&command)) {
        if (!catalog_.advancedRules || actor != s.active || s.phase != Phase::Main ||
            s.phaseStep != PhaseStep::Body || s.chain || s.effect || !s.queue.items.empty())
            return "wrong_phase: 当前不能反转";
        auto it = s.cards.find(cmd->card);
        if (it == s.cards.end() || it->second.owner != actor || !it->second.faceDown ||
            it->second.ambushedTurn >= s.globalTurn)
            return "ambush_turn: 埋伏卡须等到下一个全局回合才能反转";
        auto &c = it->second;
        const auto &d = catalog_.at(c.definition);
        const auto host = c.host;
        if (d.responseOnly)
            return "response_window: 该卡须在合法响应时点反转";
        c.faceDown = false;
        c.zone = Zone::Hand;
        c.host = 0;
        s.events.push_back({"reveal", "反转 " + d.name, -1, c.id});
        if (d.type == CardType::Analytic)
            return execute(actor, StartAnalysis{c.id, host});
        if (d.type == CardType::Action)
            return execute(actor, PlayAction{c.id, cmd->target, cmd->targets});
        auto error = execute(actor, PreloadWord{c.id, d.immediate});
        if (!error.empty() || d.immediate)
            return error;
        return execute(actor, ActivateSpell{c.id, cmd->target, cmd->targets});
    }
    if (actor != s.active || s.phase != Phase::Main || s.phaseStep != PhaseStep::Body || s.chain ||
        s.effect || !s.queue.items.empty())
        return "wrong_phase: 当前不能执行主要阶段操作";
    auto &pl = s.players[actor];
    auto owned = [&](CardId id, Zone z) {
        auto i = s.cards.find(id);
        return i != s.cards.end() && i->second.owner == actor && i->second.zone == z;
    };
    auto cost = [&](int mana, int load) {
        return pl.mana >= mana && Rules::load(s, actor) + load <= Rules::capacity(s, catalog_, actor);
    };
    auto pay = [&](CardId id, int mana) {
        pl.mana -= mana;
        s.events.push_back({"pay", "支付 " + std::to_string(mana) + " 魔素", -1, id, mana});
    };
    auto error = std::visit(
        [&](const auto &cmd) -> std::string {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, Advance> || std::is_same_v<T, AdvancePhase> ||
                          std::is_same_v<T, Choose> || std::is_same_v<T, PassResponse> ||
                          std::is_same_v<T, Respond> || std::is_same_v<T, Surrender> ||
                          std::is_same_v<T, FlipAmbush> || std::is_same_v<T, ActivateSpell>)
                return "invalid_command";
            else {
                auto i = s.cards.find(cmd.card);
                if (i == s.cards.end() || i->second.owner != actor)
                    return "invalid_card: 不是自己的卡牌";
                auto &c = i->second;
                const auto &def = catalog_.at(c.definition);
                if constexpr (std::is_same_v<T, SetAmbush>) {
                    if (!catalog_.advancedRules || !owned(cmd.card, Zone::Hand) ||
                        (def.type != CardType::Analytic && def.type != CardType::Word &&
                         def.type != CardType::Action))
                        return "invalid_ambush: 只有法术或行动卡可埋伏";
                    if (!cost(1, 0))
                        return "cost: 埋伏需要1魔素";
                    if (def.type == CardType::Analytic) {
                        if (!owned(cmd.formation, Zone::Analysis) ||
                            catalog_.at(s.cards.at(cmd.formation).definition).type != CardType::Formation ||
                            def.rank > Rules::maxRank(s, catalog_, cmd.formation) ||
                            Rules::occupied(s, cmd.formation) >= Rules::rings(s, catalog_, cmd.formation))
                            return "ring_or_rank";
                        c.zone = Zone::Analysis;
                        c.host = cmd.formation;
                        c.sourceFormation = cmd.formation;
                    } else {
                        if (cmd.formation || inZone(s, actor, Zone::Words).size() >= 3)
                            return "word_limit";
                        c.zone = Zone::Words;
                    }
                    pay(c.id, 1);
                    c.faceDown = true;
                    c.ambushedTurn = s.globalTurn;
                    c.spell = SpellState::None;
                    s.events.push_back({"ambush", "埋伏一张卡", -1, c.id});
                    return {};
                } else if constexpr (std::is_same_v<T, SetFormation>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Formation || pl.formations)
                        return "formation_limit: 阵法操作不可用";
                    if (cmd.replace) {
                        if (!owned(cmd.replace, Zone::Analysis) || s.cards.at(cmd.replace).base ||
                            catalog_.at(s.cards.at(cmd.replace).definition).type != CardType::Formation ||
                            pl.mana < 2)
                            return "invalid_replacement: 无法替换";
                        StateMaintenance::leave(s, cmd.replace, &catalog_);
                        pay(cmd.card, 2);
                    }
                    if (catalog_.advancedRules) {
                        if (pl.mana < def.cost)
                            return "cost: 阵法设置魔素不足";
                        pay(c.id, def.cost);
                        c.settingPaid = def.cost;
                    }
                    c.zone = Zone::Analysis;
                    ++pl.formations;
                    if (usedBody(s, catalog_, actor) > 5 ||
                        Rules::load(s, actor) > Rules::capacity(s, catalog_, actor))
                        return "capacity: 空间或荷载不合法";
                } else if constexpr (std::is_same_v<T, RemoveFormation>) {
                    if (!owned(cmd.card, Zone::Analysis) || c.base || def.type != CardType::Formation ||
                        pl.formations || pl.mana < 2)
                        return "invalid_removal: 无法拆除阵法";
                    StateMaintenance::leave(s, cmd.card, &catalog_);
                    pay(cmd.card, 2);
                    ++pl.formations;
                    if (Rules::load(s, actor) > Rules::capacity(s, catalog_, actor))
                        return "overload: 拆除后超载";
                } else if constexpr (std::is_same_v<T, StartAnalysis>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Analytic ||
                        !owned(cmd.formation, Zone::Analysis))
                        return "invalid_analysis: 无效解析对象";
                    const auto &host = catalog_.at(s.cards.at(cmd.formation).definition);
                    if (host.type != CardType::Formation ||
                        def.rank > Rules::maxRank(s, catalog_, cmd.formation) ||
                        Rules::occupied(s, cmd.formation) >= Rules::rings(s, catalog_, cmd.formation))
                        return "ring_or_rank: 环位或位阶不允许";
                    const int fee = Rules::analysisCost(s, catalog_, c.id, cmd.formation);
                    if (!cost(fee, fee))
                        return "cost: 魔素或荷载不足";
                    pay(cmd.card, fee);
                    c.analysisLoad = fee;
                    c.settingPaid = fee;
                    c.castLoad = 0;
                    c.payments.clear();
                    c.targets.clear();
                    c.zone = Zone::Analysis;
                    c.spell = def.analysisTurns == 0 ? SpellState::Ready : SpellState::Analyzing;
                    auto &formation = s.cards.at(cmd.formation);
                    if (host.quickRankOne && def.rank <= 1 && c.spell == SpellState::Analyzing &&
                        formation.quickAnalysisTurn != pl.ownTurn) {
                        c.spell = SpellState::Ready;
                        formation.quickAnalysisTurn = pl.ownTurn;
                        s.events.push_back(
                            {"analysis_ready", "阵法立即完成低位阶解析（每回合一次）", -1, c.id});
                    }
                    c.host = cmd.formation;
                    c.sourceFormation = cmd.formation;
                    c.analysisStarted = pl.ownTurn;
                    c.canceledTurn = -1;
                } else if constexpr (std::is_same_v<T, PrepareCast>) {
                    auto error = prepare(actor, cmd, false);
                    if (!error.empty())
                        return error;
                } else if constexpr (std::is_same_v<T, PlayAction>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Action ||
                        pl.blockedActionTurn == pl.ownTurn)
                        return "invalid_action: 行动不可用";
                    if (!validTargets(s, def.target, def.targetCount, actor, cmd.target, cmd.targets))
                        return "target: 无效目标";
                    if (!cost(def.cost, def.burden))
                        return "cost: 魔素或荷载不足";
                    pay(cmd.card, def.cost);
                    ++pl.actionsPlayed;
                    c.zone = Zone::Action;
                    c.targetCard = cmd.target;
                    c.targets = targetCards(cmd.target, cmd.targets);
                    c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
                    if (def.burden)
                        s.temporary.push_back({s.nextLoad++, actor, c.id, def.burden, pl.ownTurn});
                    beginEffect(c.id, AfterEffect::Action);
                } else if constexpr (std::is_same_v<T, PreloadWord>) {
                    if (cmd.release && !def.immediate)
                        return "word_release: 只有零阶言灵可设置并释放";
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Word ||
                        (!(def.immediate && (!catalog_.advancedRules || cmd.release)) &&
                         inZone(s, actor, Zone::Words).size() >= 3))
                        return "word_limit: 无法预置言灵";
                    if (!cost(def.cost, def.cost))
                        return "cost: 魔素或荷载不足";
                    pay(cmd.card, def.cost);
                    c.zone = Zone::Words;
                    c.analysisLoad = def.cost;
                    c.settingPaid = def.cost;
                    if (def.immediate && (!catalog_.advancedRules || cmd.release)) {
                        c.zone = Zone::Casting;
                        c.spell = SpellState::Pending;
                        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
                        beginEffect(c.id, AfterEffect::Spell);
                    }
                } else if constexpr (std::is_same_v<T, AttachSeal>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Seal ||
                        !(owned(cmd.host, Zone::Analysis) || owned(cmd.host, Zone::Words)) ||
                        seals(s, cmd.host) || s.cards.at(cmd.host).faceDown)
                        return "seal_limit: 宿主不合法或已有符文";
                    if (!cost(def.cost, 0))
                        return "cost: 魔素不足";
                    pay(cmd.card, def.cost);
                    c.zone = Zone::Attached;
                    c.host = cmd.host;
                } else if constexpr (std::is_same_v<T, RemoveSeal>) {
                    if (!owned(cmd.card, Zone::Attached) || pl.sealRemovals)
                        return "seal_limit: 无法主动拆除符文";
                    StateMaintenance::leave(s, cmd.card, &catalog_);
                    ++pl.sealRemovals;
                    if (hasOverflow(s, catalog_) ||
                        Rules::load(s, actor) > Rules::capacity(s, catalog_, actor))
                        return "capacity: 请先放弃多余法术";
                } else if constexpr (std::is_same_v<T, Abandon>) {
                    if ((c.zone != Zone::Analysis && c.zone != Zone::Casting && c.zone != Zone::Words) ||
                        (def.type != CardType::Analytic && def.type != CardType::Word &&
                         !(c.faceDown && def.type == CardType::Action)))
                        return "invalid_abandon: 无法放弃该卡";
                    StateMaintenance::leave(s, cmd.card, &catalog_);
                }
                s.events.push_back(
                    {"command", "玩家 " + std::to_string(actor + 1) + " 操作 " + def.name, -1, cmd.card});
                return {};
            }
        },
        command);
    if (error.empty()) {
        s.phaseGate = 0;
        StateMaintenance::check(s, catalog_);
    }
    return error;
}
} // namespace wizard
