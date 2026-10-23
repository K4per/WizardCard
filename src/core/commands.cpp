#include "engine_internal.hpp"
#include "wizard/core/sique.hpp"
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
    if (catalog_.usesAlphaV2()) {
        const bool preparationLocked = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
            const auto &cloudMist = entry.second;
            return (cloudMist.zone == Zone::Analysis || cloudMist.zone == Zone::Casting) &&
                cloudMist.spell == SpellState::Active && cloudMist.remaining > 0 &&
                catalog_.at(cloudMist.definition).id == "cloud-mist" &&
                std::find(cloudMist.targets.begin(), cloudMist.targets.end(), cmd.card) != cloudMist.targets.end();
        });
        if (preparationLocked)
            return "preparation_locked: 云雾术持续期间不能准备此解析法术";
        if (def.type != CardType::Analytic || c.faceDown || cmd.discard || c.zone != Zone::Analysis || c.spell != SpellState::Ready ||
            c.canceledTurn == pl.ownTurn)
            return "not_ready: 法术未解析完成";
        if (catalog_.usesAlphaV253() && !Rules::canHost(s, catalog_, cmd.card, c.host))
            return "host_restriction: 阵法等级或宿主条件不满足准备施法";
        // Shatter Ring chooses its empty formation when it is released. The
        // preparation announcement only opens the response window and must not
        // capture a target that has no effect yet.
        if (catalog_.usesAlphaV252() && def.id == "unravel") {
            if (cmd.target || !cmd.targets.empty()) return "target: 碎环术在发动时选择目标";
        } else if (!validTargets(s, def.target, def.targetCount, actor, cmd.target, cmd.targets)) {
            return "target: 无效目标";
        }
        if (!deferred && ((actor != s.active && (!c.wasAmbushed || c.ambushedTurn >= s.globalTurn || def.speed < 2)) ||
            (def.speed == 1 && s.phase != Phase::Main) ||
            (def.speed == 2 && s.phase != Phase::Main && s.phase != Phase::Cast)))
            return "spell_speed: 此时不能准备";
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "casting_capacity: 施法区没有空位";
        if (!deferred && cmd.decision) return "stale_decision";
        c.targetCard = cmd.target; c.targets = cmd.targets;
        for (const auto &pending : s.deferredPreparations)
            if (pending.item.source == cmd.card) return "not_ready";
        if (s.chain)
            for (const auto &link : s.chain->links)
                if (link.kind == LinkKind::Preparation && link.item.source == cmd.card) return "not_ready";
        ChainLink root; root.kind = LinkKind::Preparation; root.item = snapshot(c, def);
        if (catalog_.usesAlphaV252() && def.id == "unravel") root.item.target = TargetKind::None;
        root.item.effects.clear(); root.item.requiresSource = true; root.speed = def.speed;
        if (deferred) s.deferredPreparations.push_back(std::move(root));
        else startChain(ResponseWindow::Prepare, std::move(root), 1 - s.active);
        return {};
    }
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
    if (!ResourceRules::canPayMana(s, actor, castFee) || (!pl.debugInfiniteLoad && Rules::load(s, actor) + castFee > Rules::capacity(s, catalog_, actor)))
        return "cost: 魔素或荷载不足";
    if (cmd.discard)
        StateMaintenance::leave(s, cmd.discard, &catalog_);
    ResourceRules::payMana(s, actor, castFee);
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
            const auto targetSets = catalog_.usesAlphaV252() && def.id == "unravel"
                ? std::vector<std::vector<CardId>>{{}} : sets(state_, def.target, def.targetCount, actor);
            for (const auto &targets : targetSets)
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
        StateMaintenance::check(s, catalog_);
        return {};
    }
    if (s.opening) {
        const auto *choice = std::get_if<SelectInitialFormation>(&command);
        if (!choice) return "opening_required: 请先确认初始阵法";
        if (s.initialFormations[actor]) return "initial_confirmed: 初始阵法已确认";
        const auto found = s.cards.find(choice->card);
        if (found == s.cards.end() || found->second.owner != actor || found->second.zone != Zone::SideDeck)
            return "invalid_initial: 初始阵法必须来自自己的副卡组";
        const auto &definition = catalog_.at(found->second.definition);
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
            if (catalog_.usesAlphaV2() && (std::holds_alternative<PrepareCast>(command) ||
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
            if (catalog_.usesAlphaV252() && r->ability == "__activate_spell")
                return activateSpellV252(actor, r->card, r->target, r->targets, r->discard, false, r->decision);
            auto &c = s.cards.at(r->card);
            const auto &def = catalog_.at(c.definition);
            const auto a = *ability(def, r->ability);
            const bool hand = c.zone == Zone::Hand;
            const bool analytic = c.zone == Zone::Analysis && !c.faceDown;
            const bool ambush = c.faceDown;
            const int paid = catalog_.usesAlphaV2()
                ? Rules::castCost(s, catalog_, c.id) +
                      (ambush && def.type == CardType::Analytic ? def.cost : 0)
                : analytic ? Rules::castCost(s, catalog_, c.id)
                : ambush ? def.cost
                : hand   ? a.handCost
                         : a.preloadedCost +
                             (def.type == CardType::Word ? Rules::castCost(s, catalog_, c.id) - def.cost : 0);
            const int bound = catalog_.usesAlphaV2() ? paid : analytic || def.type == CardType::Word ? paid : 0;
            sique::pay(s, actor, paid, &catalog_);
            StateMaintenance::check(s, catalog_);
            if (s.result != -1) return {};
            if (r->discard)
                StateMaintenance::leave(s, r->discard, &catalog_);
            if (a.burden)
                s.temporary.push_back(
                    {s.nextLoad++, actor, c.id, a.burden, s.players[actor].ownTurn + (actor != s.active)});
            if ((hand || ambush) && def.type == CardType::Word)
                c.settingPaid = paid;
            c.castLoad += bound;
            ResourceRules::increasedLoad(s, catalog_, actor, bound + a.burden);
            if (s.result != -1) return {};
            c.sourceRingIndex = c.ringIndex >= 0 ? c.ringIndex : c.sourceRingIndex;
            c.sourceFormation = c.host ? c.host : c.sourceFormation;
            c.zone = catalog_.usesAlphaV2() || analytic ? Zone::Casting : Zone::Resolving;
            c.faceDown = false;
            c.host = 0;
            if (catalog_.usesAlphaV2()) c.ringIndex = -1;
            c.spell = catalog_.usesAlphaV2() || analytic ? SpellState::Pending : SpellState::None;
            c.targetCard = r->target;
            c.targets = targetCards(r->target, r->targets);
            c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
            c.payments.push_back({s.players[actor].ownTurn, paid, bound, false});
            ChainLink link;
            link.id = s.nextLink++;
            link.kind = catalog_.usesAlphaV2() || analytic ? LinkKind::Spell : LinkKind::Response;
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
            link.item.targetPlayer = a.target == TargetKind::Player && r->target
                ? static_cast<PlayerId>(r->target - playerTarget(0)) : a.target == TargetKind::Opponent ? 1 - actor : actor;
            link.item.effects = a.effects.empty() && catalog_.usesAlphaV2() ? def.effects : a.effects;
            if (catalog_.usesAlphaV2() && def.id == "wind-strike") {
                const auto target = std::find_if(s.chain->links.begin(), s.chain->links.end(),
                    [&](const ChainLink &candidate) { return candidate.id == r->link; });
                if (target != s.chain->links.end() && target->item.source &&
                    s.cards.count(target->item.source)) {
                    const auto rank = catalog_.at(s.cards.at(target->item.source).definition).rank;
                    for (auto &effect : link.item.effects)
                        if (effect.kind == EffectKind::Damage) {
                            effect.amount = rank * 2;
                            effect.recipient = EffectRecipient::Opponent;
                        }
                    if (rank >= def.rank)
                        link.item.effects.erase(std::remove_if(link.item.effects.begin(), link.item.effects.end(),
                            [](const Effect &effect) { return effect.kind == EffectKind::DestroyEnemyAnalysis; }),
                            link.item.effects.end());
                    link.item.targetCard = target->item.source;
                }
            }
            for (const auto &[sid, seal] : s.cards)
                if (seal.zone == Zone::Attached && seal.host == c.id &&
                    catalog_.at(seal.definition).damageBonus) {
                    const auto &sealDefinition = catalog_.at(seal.definition);
                    link.item.effects.push_back({EffectKind::Damage, catalog_.at(seal.definition).damageBonus,
                        std::find(sealDefinition.traits.begin(), sealDefinition.traits.end(),
                                  "thunder_damage_bonus") != sealDefinition.traits.end()
                            ? DamageType::Thunder : DamageType::Force, EffectRecipient::Opponent});
                }
            link.item.target = a.target;
            link.item.targetLink = r->link;
            link.item.requiresSource = a.requiresSource;
            if (catalog_.usesAlphaV2()) {
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
        if (catalog_.usesAlphaV2() && (d.kind == DecisionKind::EffectChoice || d.kind == DecisionKind::DamageChoice)) {
            chooseContentEffectV2(actor, d.kind, opt);
            return {};
        }
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
        case DecisionKind::ClearAnyLoad:
            if (!opt) { ++s.effect->cursor; s.effect->clearRemaining = -1; }
            else {
                if (opt & 0x80000000u) {
                    const auto id = opt & 0x7fffffffu;
                    auto load = std::find_if(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &t) { return t.id == id; });
                    --load->amount;
                } else {
                    auto &card = s.cards.at(opt);
                    if (card.castLoad) --card.castLoad;
                    else --card.analysisLoad;
                }
                --s.effect->clearRemaining;
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
            if (opt == 1) {
                ResourceRules::drawOne(s, catalog_, actor);
            } else {
                std::vector<CardId> options;
                for (const auto &[id, card] : s.cards)
                    if (card.owner == actor && card.zone == Zone::Ash &&
                        catalog_.at(card.definition).type != CardType::Formation &&
                        std::find(catalog_.at(card.definition).tags.begin(),
                                  catalog_.at(card.definition).tags.end(), "风暴") !=
                            catalog_.at(card.definition).tags.end())
                        options.push_back(id);
                if (!options.empty()) decision(actor, DecisionKind::StormHeartRecover, std::move(options));
            }
            break;
        case DecisionKind::StormHeartRecover: {
            auto &deck = s.players[actor].deck;
            auto &card = s.cards.at(opt);
            card.zone = Zone::Deck;
            deck.push_back(opt);
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
                if (c.owner == actor && (c.zone == Zone::Casting || (catalog_.usesAlphaV2() && c.zone == Zone::Analysis)) && c.spell == SpellState::Active &&
                    catalog_.at(c.definition).concentration)
                    source = catalog_.usesAlphaV2() ? s.maintenanceSource : id;
            if (!source) break;
            auto &c = s.cards.at(source);
            if (opt) {
                const int paid = Rules::concentrationUpkeep(s, catalog_, source);
                sique::pay(s, actor, paid, &catalog_);
                c.castLoad += paid;
                c.payments.push_back({s.players[actor].ownTurn, paid, paid, false});
                ResourceRules::increasedLoad(s, catalog_, actor, paid);
                if (s.result != -1) return {};
                s.events.push_back({"concentration", "维持专注：支付 " + std::to_string(paid) + " 魔素及荷载",
                                    -1, source, paid});
                const auto &definition = catalog_.at(c.definition);
                if (catalog_.usesAlphaV25() && !definition.onCast.empty()) {
                    auto trigger = snapshot(c, definition);
                    trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
                    trigger.effects = definition.onCast;
                    s.queue.append(std::move(trigger));
                }
            } else {
                StateMaintenance::leave(s, source, &catalog_);
                StateMaintenance::check(s, catalog_);
            }
            break;
        }
        case DecisionKind::ReturnSpell: {
            const CardId source = s.maintenanceSource;
            if (!source || !s.cards.count(source)) return "invalid_return";
            auto &card = s.cards.at(source);
            if (opt) {
                if (!s.cards.count(opt) || s.cards.at(opt).owner != actor ||
                    s.cards.at(opt).zone != Zone::Analysis ||
                    catalog_.at(s.cards.at(opt).definition).type != CardType::Formation ||
                    catalog_.at(card.definition).rank > Rules::maxRank(s, catalog_, opt) ||
                    Rules::freeRing(s, catalog_, opt) < 0) return "invalid_return";
                if (std::find(catalog_.at(card.definition).traits.begin(),
                              catalog_.at(card.definition).traits.end(), "requires_storm_heart_setup") !=
                        catalog_.at(card.definition).traits.end() &&
                    catalog_.at(s.cards.at(opt).definition).id != "storm-heart")
                    return "invalid_return";
                if (!Rules::canHost(s, catalog_, source, opt)) return "invalid_return";
                const int ring = choose->ringIndex < 0 ? Rules::freeRing(s, catalog_, opt) : choose->ringIndex;
                if (ring < 0 || ring >= Rules::rings(s, catalog_, opt)) return "invalid_return";
                for (const auto &[id, other] : s.cards)
                    if (id != source && other.zone == Zone::Analysis && other.host == opt && other.ringIndex == ring)
                        return "invalid_return";
                card.zone = Zone::Analysis; card.host = opt; card.sourceFormation = opt;
                card.ringIndex = ring; card.sourceRingIndex = ring;
                card.fieldOrder = s.nextFieldOrder++;
                s.events.push_back({"return", "持续法术回位", -1, source});
            } else StateMaintenance::leave(s, source, &catalog_);
            s.maintenanceSource = 0;
            break;
        }
        case DecisionKind::Response:
            break;
        }
        return {};
    }
    if (catalog_.usesAlphaV2())
        return executeV2(actor, command);
    if (std::holds_alternative<Choose>(command) || std::holds_alternative<Respond>(command) ||
        std::holds_alternative<PassResponse>(command))
        return "stale_decision: 选择已过期";
    if (std::holds_alternative<AdvancePhase>(command) || std::holds_alternative<Advance>(command)) {
        if (std::holds_alternative<Advance>(command) && s.phase != Phase::Main)
            return "wrong_phase: 旧推进命令仅适用于主要阶段";
        auto gate = std::get_if<AdvancePhase>(&command);
        if (gate && gate->skipCast) return "unsupported_phase: 当前规则不能跳过施法阶段";
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
        if (!ResourceRules::canPayMana(s, actor, fee) || (!s.players[actor].debugInfiniteLoad && Rules::load(s, actor) + fee > Rules::capacity(s, catalog_, actor)))
            return "cost: 魔素或荷载不足";
        ResourceRules::payMana(s, actor, fee);
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
        return ResourceRules::canPayMana(s, actor, mana) && (pl.debugInfiniteLoad || Rules::load(s, actor) + load <= Rules::capacity(s, catalog_, actor));
    };
    auto pay = [&](CardId id, int mana) {
        ResourceRules::payMana(s, actor, mana);
        s.events.push_back({"pay", "支付 " + std::to_string(mana) + " 魔素", -1, id, mana});
    };
    auto error = std::visit(
        [&](const auto &cmd) -> std::string {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, Advance> || std::is_same_v<T, AdvancePhase> ||
                          std::is_same_v<T, Choose> || std::is_same_v<T, PassResponse> ||
                          std::is_same_v<T, Respond> || std::is_same_v<T, Surrender> ||
                          std::is_same_v<T, FlipAmbush> || std::is_same_v<T, ActivateSpell> ||
                          std::is_same_v<T, ActivateAbility> || std::is_same_v<T, Timeout> ||
                          std::is_same_v<T, SelectInitialFormation>)
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
                            !ResourceRules::canPayMana(s, actor, 2))
                            return "invalid_replacement: 无法替换";
                        StateMaintenance::leave(s, cmd.replace, &catalog_);
                        pay(cmd.card, 2);
                    }
                    if (catalog_.advancedRules) {
                        if (!ResourceRules::canPayMana(s, actor, def.cost))
                            return "cost: 阵法设置魔素不足";
                        pay(c.id, def.cost);
                        c.settingPaid = def.cost;
                    }
                    c.zone = Zone::Analysis;
                    ++pl.formations;
                    if (usedBody(s, catalog_, actor) > 5 ||
                        (!pl.debugInfiniteLoad && Rules::load(s, actor) > Rules::capacity(s, catalog_, actor)))
                        return "capacity: 空间或荷载不合法";
                } else if constexpr (std::is_same_v<T, RemoveFormation>) {
                    if (!owned(cmd.card, Zone::Analysis) || c.base || def.type != CardType::Formation ||
                        pl.formations || !ResourceRules::canPayMana(s, actor, 2))
                        return "invalid_removal: 无法拆除阵法";
                    StateMaintenance::leave(s, cmd.card, &catalog_);
                    pay(cmd.card, 2);
                    ++pl.formations;
                    if ((!pl.debugInfiniteLoad && Rules::load(s, actor) > Rules::capacity(s, catalog_, actor)))
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
                        s.cards.at(cmd.host).faceDown)
                        return "seal_host: 宿主不合法";
                    if (!catalog_.usesAlphaV252() && seals(s, cmd.host))
                        return "seal_limit: 宿主已有符文";
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
                        (!pl.debugInfiniteLoad && Rules::load(s, actor) > Rules::capacity(s, catalog_, actor)))
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
#include "v2_commands.inc"
