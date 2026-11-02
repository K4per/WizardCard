#include "engine_internal.hpp"
#include "wizard/core/card_rules.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard {
using namespace detail;
std::string GameEngine::prepare(PlayerId actor, const PrepareCast &cmd, bool deferred) {
    if (const auto error = validatePreparation(actor, cmd, deferred); !error.empty()) return error;
    auto &c = state_.cards.at(cmd.card);
    const auto &def = catalog().at(c.definition);
    c.targetCard = cmd.target; c.targets = cmd.targets;
    ChainLink root; root.kind = LinkKind::Preparation; root.item = snapshot(c, def);
    root.item.target = TargetKind::None; root.item.effects.clear(); root.item.requiresSource = true; root.speed = def.speed;
    if (deferred) state_.deferredPreparations.push_back(std::move(root));
    else startChain(ResponseWindow::Prepare, std::move(root), 1 - state_.active);
    return {};
}

std::vector<PrepareCast> GameEngine::preparationsFor(PlayerId actor, DecisionId id) const {
    std::vector<PrepareCast> out;
    for (const auto &[cid, c] : state_.cards)
        if (controllerOf(c) == actor && c.zone == Zone::Analysis && c.spell == SpellState::Ready) {
            const auto &def = catalog().at(c.definition);
            std::vector<CardId> discards{0};
            if (def.extraDiscard)
                discards = inZone(state_, actor, Zone::Hand);
            const std::vector<std::vector<CardId>> targetSets{{}};
            for (const auto &targets : targetSets)
                for (auto discard : discards) {
                    PrepareCast cmd{cid, targets.empty() ? 0 : targets.front(), discard, targets, id};
                    if (validatePreparation(actor, cmd, true).empty())
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
    if (std::holds_alternative<Timeout>(command)) {
        s.result = 1 - actor; s.opening = false;
        s.queue.items.clear(); s.deferredPreparations.clear(); s.decision.reset();
        s.effect.reset(); s.chain.reset(); s.phaseGate = 0;
        s.events.push_back({"timeout", "玩家 " + std::to_string(actor + 1) + " 思考时间耗尽", -1});
        return {};
    }
    if (std::holds_alternative<Surrender>(command)) {
        s.opening = false;
        s.players[actor].surrendered = true;
        StateMaintenance::check(s, catalog());
        return {};
    }
    if (s.opening) {
        const auto *choice = std::get_if<SelectInitialFormation>(&command);
        if (!choice) return "opening_required: 请先确认初始阵法";
        if (s.initialFormations[actor]) return "initial_confirmed: 初始阵法已确认";
        const auto found = s.cards.find(choice->card);
        if (found == s.cards.end() || controllerOf(found->second) != actor || found->second.zone != Zone::SideDeck)
            return "invalid_initial: 初始阵法必须来自自己的副卡组";
        const auto &definition = catalog().at(found->second.definition);
        if (definition.type != CardType::Formation || definition.level != 1)
            return "invalid_initial: 初始阵法必须为等级1的阵法卡";
        s.initialFormations[actor] = choice->card;
        if (s.initialFormations[0] && s.initialFormations[1]) finishOpening();
        else s.active = s.initialFormations[0] ? 1 : 0;
        return {};
    }
    if (std::holds_alternative<SelectInitialFormation>(command)) return "opening_finished: 开局选择已经结束";
    if (s.decision) {
        const auto d = *s.decision;
        if (actor != d.player)
            return "decision_mismatch: 请完成当前玩家的选择";
        if (d.kind == DecisionKind::Response) {
            if ((std::holds_alternative<PrepareCast>(command) ||
                                          std::holds_alternative<FlipAmbush>(command) ||
                                          std::holds_alternative<PreloadWord>(command)))
                return responseSetupV2(actor, command);
            Command selected = command;
            if (auto old = std::get_if<Choose>(&selected)) {
                if (old->decision != d.id)
                    return "stale_decision: 选择已过期";
                if (!old->option)
                    selected = PassResponse{d.id};
                else {
                    auto candidates = responsesFor(actor, d.id);
                    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                                    [&](const Respond &r) { return r.card != old->option.value; }),
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
            if (r->ability == "__activate_spell")
                return activateSpellV252(actor, r->card, r->target, r->targets, r->discard, false, r->decision);
            auto &c = s.cards.at(r->card);
            const auto &def = catalog().at(c.definition);
            const auto a = *ability(def, r->ability);
            const bool hand = c.zone == Zone::Hand;
            const bool ambush = c.faceDown;
            const int paid = hasCapability(def, Capability::CounterSpell) ? detail::linkedSpellCost(s, r->link)
                : Rules::castCost(s, catalog(), c.id) + (ambush && def.type == CardType::Analytic ? def.cost : 0);
            const int bound = paid;
            card_rules::pay(s, actor, paid, &catalog());
            StateMaintenance::check(s, catalog());
            if (s.result != -1) return {};
            if (r->discard)
                StateMaintenance::leave(s, r->discard, &catalog());
            if (a.burden)
                s.temporary.push_back(
                    {s.nextLoad++, actor, c.id, a.burden, s.players[actor].ownTurn + (actor != s.active)});
            if ((hand || ambush) && def.type == CardType::Word)
                c.settingPaid = paid;
            c.castLoad += bound;
            ResourceRules::increasedLoad(s, catalog(), actor, bound + a.burden);
            if (s.result != -1) return {};
            c.sourceRingIndex = c.ringIndex >= 0 ? c.ringIndex : c.sourceRingIndex;
            c.sourceFormation = c.host ? c.host : c.sourceFormation;
            c.zone = Zone::Casting;
            c.faceDown = false;
            c.host = 0;
            c.ringIndex = -1;
            c.spell = SpellState::Pending;
            c.targetCard = r->target;
            c.targets = targetCards(r->target, r->targets);
            c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
            c.payments.push_back({s.players[actor].ownTurn, paid, bound, false});
            ChainLink link;
            link.id = s.nextLink++;
            link.kind = LinkKind::Spell;
            link.speed = def.speed;
            link.castCost = hasCapability(def, Capability::CounterSpell)
                ? paid : Rules::castCost(s, catalog(), c.id);
            link.ability = a.id;
            link.paymentIndex = static_cast<int>(c.payments.size()) - 1;
            link.paid = paid;
            link.addedLoad = bound + a.burden;
            link.discarded = r->discard;
            link.item.owner = actor;
            link.item.source = c.id;
            link.item.targetCard = r->target;
            link.item.targetCards = targetCards(r->target, r->targets);
            link.item.targetPlayer = a.target == TargetKind::Player && r->target
                ? static_cast<PlayerId>(r->target - playerTarget(0)) : a.target == TargetKind::Opponent ? 1 - actor : actor;
            link.item.effects = a.effects.empty() ? def.effects : a.effects;
            if (hasCapability(def, Capability::WindStrike)) {
                const auto target = std::find_if(s.chain->links.begin(), s.chain->links.end(),
                    [&](const ChainLink &candidate) { return candidate.id == r->link; });
                if (target != s.chain->links.end() && target->item.source &&
                    s.cards.count(target->item.source)) {
                    const auto rank = catalog().at(s.cards.at(target->item.source).definition).rank;
                    for (auto &effect : link.item.effects)
                        if (effect.kind == EffectKind::Damage) {
                            effect.amount = rank * 2;
                            effect.recipient = EffectRecipient::Opponent;
                        }
                    if (rank >= def.rank)
                        link.item.effects.erase(std::remove_if(link.item.effects.begin(), link.item.effects.end(),
                            [](const Effect &effect) { return effect.kind == EffectKind::DestroyEnemyAnalysis; }),
                            link.item.effects.end());
                    link.item.invalidateEffectsKey();
                    link.item.targetCard = target->item.source;
                }
            }
            for (const auto &[sid, seal] : s.cards)
                if (seal.zone == Zone::Attached && seal.host == c.id &&
                    catalog().at(seal.definition).damageBonus) {
                    const auto &sealDefinition = catalog().at(seal.definition);
                    link.item.effects.push_back({EffectKind::Damage, catalog().at(seal.definition).damageBonus,
                        hasCapability(sealDefinition, Capability::ThunderDamageBonus)
                            ? DamageType::Thunder : DamageType::Force, EffectRecipient::Opponent});
                }
            link.item.target = a.target;
            link.item.targetLink = r->link;
            link.item.requiresSource = a.requiresSource;
            {
                castingTraitsV2(c.id);
                if (s.result != -1) return {};
            s.chain->hadActualLinks = true;
            }
            s.chain->links.push_back(std::move(link));
            s.chain->responded.push_back(c.id);
            s.chain->passes = 0;
            s.chain->priority = 1 - actor;
            s.decision.reset();
            s.events.push_back({"pay", "响应支付 " + std::to_string(paid) + " 魔素", -1, c.id, paid});
            s.events.push_back(
                {"link_declared", "响应链节 #" + std::to_string(s.chain->links.back().id), -1, c.id});
            StateMaintenance::check(s, catalog());
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
        const auto chosen = choose->option;
        const auto opt = static_cast<CardId>(chosen.value);
        if (d.kind == DecisionKind::EffectPrepare && opt)
            return "decision_mismatch: 请指定完整的准备施法操作";
        if (!(!chosen && d.mayPass) && std::find(d.options.begin(), d.options.end(), chosen) == d.options.end())
            return "invalid_option: 无效选择";
        if ((d.kind == DecisionKind::EffectChoice || d.kind == DecisionKind::DamageChoice)) {
            if (!chooseContentEffectV2(actor, d.kind, chosen))
                return "invalid_option: 选择已失效";
            s.decision.reset();
            s.events.push_back({"choice", "玩家 " + std::to_string(actor + 1) + " 完成选择"});
            return {};
        }
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
                                  [&](const Trigger &t) { return t.id == chosen.value; });
            auto t = *i;
            s.queue.items.erase(i);
            startTrigger(std::move(t));
            break;
        }
        case DecisionKind::Discard:
            StateMaintenance::leave(s, opt, &catalog());
            StateMaintenance::check(s, catalog());
            break;
        case DecisionKind::Overflow:
            StateMaintenance::leave(s, opt, &catalog());
            break;
        case DecisionKind::ClearLoad:
            if (!opt) {
                ++s.effect->cursor;
                s.effect->resume<RepeatedChoiceContinuation>().remaining = -1;
            } else {
                auto i = std::find_if(s.temporary.begin(), s.temporary.end(),
                                      [&](const TemporaryLoad &t) { return t.id == opt; });
                --i->amount;
                --s.effect->resume<RepeatedChoiceContinuation>().remaining;
                s.events.push_back({"load", "移除1点临时荷载"});
            }
            break;
        case DecisionKind::ClearAnyLoad:
            if (!opt) { ++s.effect->cursor; s.effect->resume<RepeatedChoiceContinuation>().remaining = -1; }
            else {
                if (chosen.kind == ChoiceKind::Load) {
                    const auto id = opt;
                    auto load = std::find_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &t) { return t.id == id; });
                    --load->amount;
                } else {
                    auto &card = s.cards.at(opt);
                    if (card.castLoad) --card.castLoad;
                    else --card.analysisLoad;
                }
                --s.effect->resume<RepeatedChoiceContinuation>().remaining;
                s.events.push_back({"load", "清除1点荷载"});
            }
            break;
        case DecisionKind::StormSearch: {
            auto &deck = s.players[actor].deck;
            deck.erase(std::find(deck.begin(), deck.end(), opt));
            s.cards.at(opt).zone = Zone::Hand;
            s.events.push_back({"search", "检索一张风暴法术加入手牌", actor, opt});
            for (std::size_t size = deck.size(); size > 1; --size)
                std::swap(deck[size - 1], deck[bounded(s, static_cast<std::uint32_t>(size))]);
            break;
        }
        case DecisionKind::StormHeartChoice:
            if (chosen == ChoiceValue::mode(ChoiceMode::HeartDraw)) {
                ResourceRules::drawOne(s, catalog(), actor);
            } else {
                std::vector<CardId> options;
                for (const auto &[id, card] : s.cards)
                    if (card.owner == actor && card.zone == Zone::Ash && detail::stormTags(catalog().at(card.definition).tags))
                        options.push_back(id);
                if (!options.empty()) decision(actor, DecisionKind::StormHeartRecover, std::move(options));
            }
            break;
        case DecisionKind::StormHeartRecover: {
            auto &deck = s.players[actor].deck;
            card_rules::returnCard(s, catalog(), opt, false);
            for (std::size_t size = deck.size(); size > 1; --size)
                std::swap(deck[size - 1], deck[bounded(s, static_cast<std::uint32_t>(size))]);
            s.events.push_back({"recover", "将一张风暴卡放回牌库并洗切", actor, opt});
            break;
        }
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
                StateMaintenance::leave(s, opt, &catalog());
            ++s.effect->cursor;
            break;
        case DecisionKind::EffectDiscard:
            StateMaintenance::leave(s, opt, &catalog());
            ++s.effect->cursor;
            break;
        case DecisionKind::EffectPrepare:
            ++s.effect->cursor; // explicit decline, no declaration
            break;
        case DecisionKind::Concentration: {
            CardId source = 0;
            for (const auto &[id, c] : s.cards)
                if (c.owner == actor && (c.zone == Zone::Casting || (c.zone == Zone::Analysis)) && c.spell == SpellState::Active &&
                    catalog().at(c.definition).concentration)
                    source = s.maintenanceCard();
            if (!source) break;
            auto &c = s.cards.at(source);
            if (opt) {
                const int paid = Rules::concentrationUpkeep(s, catalog(), source);
                card_rules::pay(s, actor, paid, &catalog());
                c.castLoad += paid;
                c.payments.push_back({s.players[actor].ownTurn, paid, paid, false});
                ResourceRules::increasedLoad(s, catalog(), actor, paid);
                if (s.result != -1) return {};
                s.events.push_back({"concentration", "维持专注：支付 " + std::to_string(paid) + " 魔素及荷载",
                                    -1, source, paid});
                const auto &definition = catalog().at(c.definition);
                if (!definition.onCast.empty()) {
                    auto trigger = snapshot(c, definition);
                    trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
                    trigger.effects = definition.onCast;
                    s.queue.append(std::move(trigger));
                }
            } else {
                StateMaintenance::leave(s, source, &catalog());
                StateMaintenance::check(s, catalog());
            }
            s.maintenance = std::monostate{};
            break;
        }
        case DecisionKind::ReturnSpell: {
            const CardId source = s.maintenanceCard();
            if (!source || !s.cards.count(source)) return "invalid_return";
            auto &card = s.cards.at(source);
            if (opt) {
                if (!s.cards.count(opt) || s.cards.at(opt).owner != actor ||
                    s.cards.at(opt).zone != Zone::Analysis ||
                    catalog().at(s.cards.at(opt).definition).type != CardType::Formation ||
                    catalog().at(card.definition).rank > Rules::maxRank(s, catalog(), opt) ||
                    Rules::freeRing(s, catalog(), opt) < 0) return "invalid_return";
                if (hasCapability(catalog().at(card.definition), Capability::RequiresStormHeartSetup) &&
                    !hasCapability(catalog().at(s.cards.at(opt).definition), Capability::StormHeart))
                    return "invalid_return";
                if (!Rules::canHost(s, catalog(), source, opt)) return "invalid_return";
                const int ring = choose->ringIndex < 0 ? Rules::freeRing(s, catalog(), opt) : choose->ringIndex;
                if (ring < 0 || ring >= Rules::rings(s, catalog(), opt)) return "invalid_return";
                for (const auto &[id, other] : s.cards)
                    if (id != source && other.zone == Zone::Analysis && other.host == opt && other.ringIndex == ring)
                        return "invalid_return";
                card.zone = Zone::Analysis; card.host = opt; card.sourceFormation = opt;
                card.ringIndex = ring; card.sourceRingIndex = ring;
                card.fieldOrder = s.nextFieldOrder++;
                s.events.push_back({"return", "持续法术回位", -1, source});
            } else StateMaintenance::leave(s, source, &catalog());
            s.maintenance = std::monostate{};
            break;
        }
        case DecisionKind::Response:
            break;
        }
        return {};
    }
    return executeV2(actor, command);
}
} // namespace wizard
