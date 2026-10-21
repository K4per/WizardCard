#include "engine_internal.hpp"
#include "wizard/core/sique.hpp"
#include "wizard/core/arcana.hpp"
#include <algorithm>
#include <functional>
#include <set>
namespace wizard {
using namespace detail;
namespace {
bool spellType(CardType type) {
    return type == CardType::Analytic || type == CardType::Word || type == CardType::Talisman;
}
bool materialMatches(const CardDefinition &definition, const RecipeCost &need, const CardInstance &card,
                     const CardCatalog &catalog, PlayerId owner) {
    if (card.owner != owner || card.faceDown ||
        (card.zone != Zone::Analysis && card.zone != Zone::Casting && card.zone != Zone::Hand)) return false;
    if (need.kind == RecipeCostKind::SacrificeFormation &&
        (definition.type != CardType::Formation || card.zone != Zone::Analysis)) return false;
    if (need.kind == RecipeCostKind::SacrificeSpell &&
        (!spellType(definition.type) || card.zone == Zone::Hand)) return false;
    if (need.kind == RecipeCostKind::DiscardHand && card.zone != Zone::Hand) return false;
    if (!need.definition.empty() && card.definition != need.definition) return false;
    if (!need.school.empty() && definition.school != need.school) return false;
    if (need.readyOnly && card.spell != SpellState::Ready && card.spell != SpellState::Active) return false;
    if (need.fireDamage) {
        if (!sique::fireSpell(definition)) return false;
    }
    (void)catalog;
    return true;
}
bool recipeMatches(const GameState &state, const CardCatalog &catalog, PlayerId player,
                   const CardDefinition &formation, const std::vector<CardId> &materials) {
    std::vector<const RecipeCost *> slots;
    for (const auto &need : formation.recipe)
        if (need.kind != RecipeCostKind::Mana)
            for (int i = 0; i < need.amount; ++i) slots.push_back(&need);
    if (materials.size() != slots.size()) return false;
    std::set<CardId> distinct(materials.begin(), materials.end());
    if (distinct.size() != materials.size()) return false;
    std::vector<bool> taken(materials.size());
    std::function<bool(std::size_t)> search = [&](std::size_t index) {
        if (index == slots.size()) return true;
        for (std::size_t i = 0; i < materials.size(); ++i) {
            const auto it = state.cards.find(materials[i]);
            if (taken[i] || it == state.cards.end()) continue;
            const auto &card = it->second;
            const auto &definition = catalog.at(card.definition);
            if (!materialMatches(definition, *slots[index], card, catalog, player)) continue;
            if (catalog.usesAlphaV252() && formation.id == "moon" && slots[index]->definition.empty() &&
                (definition.body != 2 || (definition.id != "hexagram" && definition.id != "pentagram-of-life" &&
                                           definition.id != "octagram"))) continue;
            if (slots[index]->distinctSchools) {
                bool repeated = false;
                for (std::size_t j = 0; j < materials.size(); ++j)
                    if (taken[j] && catalog.at(state.cards.at(materials[j]).definition).school == definition.school)
                        repeated = true;
                if (repeated) continue;
            }
            taken[i] = true;
            if (search(index + 1)) return true;
            taken[i] = false;
        }
        return false;
    };
    return search(0);
}
}
void GameEngine::actionTimingV2(ResponseWindow window) {
    if (state_.chain || state_.effect || state_.decision || state_.result != -1) return;
    ChainLink root;
    root.kind = LinkKind::Phase;
    root.item.owner = state_.active;
    startChain(window, std::move(root), 1 - state_.active);
}
void GameEngine::analysisCompletedV2(CardId spell) {
    auto &s = state_;
    if (catalog_.usesAlphaV25() && catalog_.at(s.cards.at(spell).definition).type != CardType::Analytic) return;
    const auto owner = s.cards.at(spell).owner;
    const auto batch = s.nextBatch++;
    for (const auto &[id, formation] : s.cards) {
        if (formation.owner != owner || formation.zone != Zone::Analysis) continue;
        const auto &def = catalog_.at(formation.definition);
        if (def.type == CardType::Formation &&
            std::find(def.traits.begin(), def.traits.end(), "clear_load_on_analysis") != def.traits.end()) {
            auto trigger = snapshot(formation, def); trigger.id = s.nextTrigger++; trigger.batch = batch;
            trigger.effects = {{EffectKind::ClearAnyLoad, 1}}; s.queue.append(std::move(trigger));
        }
    }
}
void GameEngine::quickAnalysisV2(CardId spell, CardId host) {
    auto &s = state_;
    auto &card = s.cards.at(spell);
    auto &formation = s.cards.at(host);
    const auto &def = catalog_.at(card.definition);
    const auto &traits = catalog_.at(formation.definition).traits;
    const int advance = std::find(traits.begin(), traits.end(), "quick_host_analysis_2") != traits.end() ? 2 :
        std::find(traits.begin(), traits.end(), "quick_host_analysis_1") != traits.end() ? 1 : 0;
    if (catalog_.usesAlphaV252() && advance) {
        arcana::advance(s, catalog_, spell, advance);
        s.temporary.push_back({s.nextLoad++, card.owner, host, advance, s.players[card.owner].ownTurn + 1, true});
        ResourceRules::increasedLoad(s, catalog_, card.owner, advance);
        return;
    }
    if (!catalog_.at(formation.definition).quickRankOne || def.rank > 1 ||
        card.spell != SpellState::Analyzing || formation.quickAnalysisTurn == s.players[card.owner].ownTurn)
        return;
    if (catalog_.usesAlphaV252()) arcana::advance(s, catalog_, spell, def.analysisTurns - card.analysisProgress);
    else { card.analysisProgress = def.analysisTurns; card.spell = SpellState::Ready; }
    formation.quickAnalysisTurn = s.players[card.owner].ownTurn;
    s.events.push_back({"analysis_ready", "阵法立即完成低位阶解析（每回合一次）", -1, spell});
    if (!catalog_.usesAlphaV252()) analysisCompletedV2(spell);
}
void GameEngine::castingTraitsV2(CardId spell) {
    auto &s = state_;
    if (s.result != -1) return;
    const auto &card = s.cards.at(spell);
    if (catalog_.usesAlphaV252()) {
        const auto batch = s.nextBatch++;
        for (const auto &[id, active] : s.cards) {
            const auto &d = catalog_.at(active.definition);
            if (active.owner != card.owner && active.spell == SpellState::Active &&
                (active.zone == Zone::Analysis || active.zone == Zone::Casting) &&
                std::find(d.traits.begin(), d.traits.end(), "opponent_spell_load_4") != d.traits.end()) {
                auto trigger = snapshot(active, d); trigger.id = s.nextTrigger++; trigger.batch = batch;
                trigger.effects = {{EffectKind::AddTemporary, 4, DamageType::Force, EffectRecipient::Opponent}};
                s.queue.append(std::move(trigger));
            }
        }
    }
    const auto host = s.cards.find(card.sourceFormation);
    if (host == s.cards.end() || host->second.zone != Zone::Analysis) return;
    const auto &traits = catalog_.at(host->second.definition).traits;
    if (std::find(traits.begin(), traits.end(), "host_cast_life_for_temporary") != traits.end()) {
        --s.players[card.owner].life;
        s.events.push_back({"life_loss", "逆十字：失去1点生命", -1, spell, 1});
        StateMaintenance::check(s, catalog_);
        if (s.result == -1) {
            if (catalog_.usesAlphaV252())
                ResourceRules::grantTemporaryLife(s, card.owner, spell, 2, s.players[card.owner].ownTurn + 1);
            else s.players[card.owner].temporaryLife = std::max(2, s.players[card.owner].temporaryLife);
        }
    }
}
std::string GameEngine::activateSpellV252(PlayerId actor, CardId id, CardId target,
                                            const std::vector<CardId> &targets, CardId discard,
                                            bool flip, DecisionId token) {
    auto &s = state_;
    const bool responding = token != 0;
    if (responding) {
        if (!s.chain || s.chain->mode != ChainMode::Building || s.chain->priority != actor)
            return "priority: 当前没有响应优先权";
        if (s.decision && s.decision->id != token) return "stale_decision";
    } else if (s.chain || s.decision || s.effect || actor != s.active || s.phaseStep != PhaseStep::Body)
        return "priority: 当前没有行动权";
    const auto found = s.cards.find(id);
    if (found == s.cards.end() || found->second.owner != actor) return "invalid_card";
    auto &card = found->second;
    const auto &def = catalog_.at(card.definition);
    if (!spellType(def.type)) return "invalid_spell";
    if (auto error = arcana::activationError(s, catalog_, actor, id, target); !error.empty()) return error;
    if (!spellSpeedAllowed(s, def, flip)) return "spell_speed: 咒文速度不允许当前时点发动";
    if (def.responseOnly || (def.effects.empty() && !def.responses.empty()))
        return "response_condition: 仅可在卡牌明示的事件发生时发动";
    if (!sique::activationAllowed(s, catalog_, actor, def) ||
        (def.id == "sique-aroma-lure" && !sique::hasFormation(s, actor, "sique-cauldron")))
        return "sique_activation_restriction";
    const auto hasTrait = [&](const char *name) {
        return std::find(def.traits.begin(), def.traits.end(), name) != def.traits.end();
    };
    if ((hasTrait("requires_storm_formation") || (flip && hasTrait("requires_storm_setup"))) &&
        !hasStormFormation(s, catalog_, actor)) return "storm_formation: 需要己方风暴阵法";
    if (flip) {
        if (def.type == CardType::Analytic) return "not_ready: 解析法术须先完成解析并准备施法";
        if (!card.faceDown || card.zone != Zone::Analysis || card.ambushedTurn >= s.globalTurn)
            return "ambush_wait: 盖伏等待期限尚未满足";
        if (!Rules::canHost(s, catalog_, id, card.host) ||
            (hasTrait("requires_storm_heart_setup") &&
             catalog_.at(s.cards.at(card.host).definition).id != "storm-heart")) return "host_restriction";
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "casting_capacity";
    } else if (card.faceDown || card.zone != Zone::Casting || card.spell != SpellState::Ready)
        return "not_ready: 法术须在施法区准备完成";
    const bool detach = sique::detachCost(def);
    if (detach ? target != 0 || !sique::validDetach(s, catalog_, actor, def, targets)
               : !validTargets(s, def.target, def.targetCount, actor, target, targets)) return "target";
    if (hasTrait("discard_spell_search")) {
        const auto material = s.cards.find(discard);
        if (!discard || discard == id || material == s.cards.end() || material->second.owner != actor ||
            material->second.zone != Zone::Hand || !spellType(catalog_.at(material->second.definition).type))
            return "extra_cost";
        const auto &school = catalog_.at(material->second.definition).school;
        if (std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                return entry.second.owner == actor && entry.second.zone == Zone::Deck &&
                    spellType(catalog_.at(entry.second.definition).type) &&
                    catalog_.at(entry.second.definition).school == school;
            })) return "extra_cost";
    } else if (!arcana::validCost(s, catalog_, actor, id, discard)) return "extra_cost";
    if (def.target == TargetKind::EnemyAshCard) {
        const auto recovered = s.cards.find(target);
        if (recovered == s.cards.end() || catalog_.at(recovered->second.definition).type != CardType::Analytic ||
            catalog_.at(recovered->second.definition).rank > 5 ||
            std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                const auto &host = entry.second;
                return host.owner == actor && host.zone == Zone::Analysis && !host.faceDown &&
                    catalog_.at(host.definition).type == CardType::Formation &&
                    catalog_.at(recovered->second.definition).rank <= Rules::maxRank(s, catalog_, entry.first) &&
                    Rules::freeRing(s, catalog_, entry.first) >= 0;
            })) return "target";
    }
    CardId stormCostFormation = 0;
    if (std::any_of(def.effects.begin(), def.effects.end(), [](const Effect &effect) {
            return effect.kind == EffectKind::StormCyclone;
        })) {
        for (const auto &[formationId, formation] : s.cards) {
            const auto &formationDefinition = catalog_.at(formation.definition);
            if (formation.owner == actor && formation.zone == Zone::Analysis && formation.stormPoints >= 5 &&
                formationDefinition.type == CardType::Formation &&
                std::find(formationDefinition.tags.begin(), formationDefinition.tags.end(), "风暴") !=
                    formationDefinition.tags.end()) { stormCostFormation = formationId; break; }
        }
        if (!stormCostFormation) return "storm_points: 风暴点数不足";
    }
    const int fee = Rules::castCost(s, catalog_, id);
    if (!sique::canPay(s, actor, fee)) return "cost: 魔素不足";
    sique::pay(s, actor, fee);
    StateMaintenance::check(s, catalog_);
    if (s.result != -1) return {};
    card.castLoad += fee;
    ResourceRules::increasedLoad(s, catalog_, actor, fee);
    if (s.result != -1) return {};
    if (detach) for (auto seal : targets) StateMaintenance::leave(s, seal, &catalog_);
    if (stormCostFormation) s.cards.at(stormCostFormation).stormPoints -= 5;
    if (flip) { card.settingPaid = fee; card.faceDown = false; card.zone = Zone::Casting;
                card.host = 0; card.ringIndex = -1; }
    card.spell = SpellState::Pending;
    card.targetCard = target;
    card.targets = detach ? std::vector<CardId>{} : targetCards(target, targets);
    if (hasTrait("discard_spell_search")) {
        if (discard) { StateMaintenance::leave(s, discard, &catalog_); card.targets = {discard}; }
    } else arcana::payCost(s, catalog_, actor, id, discard);
    card.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
    card.payments.push_back({s.players[actor].ownTurn, fee, fee, false});
    castingTraitsV2(id);
    if (s.result != -1) return {};
    if (!responding) { beginEffect(id, AfterEffect::Spell); return {}; }
    ChainLink link;
    link.id = s.nextLink++;
    link.kind = LinkKind::Spell;
    link.speed = def.speed;
    link.castCost = fee;
    link.paymentIndex = static_cast<int>(card.payments.size()) - 1;
    link.paid = fee;
    link.item = snapshot(card, def);
    link.item.requiresSource = true;
    for (const auto &[sealId, seal] : s.cards)
        if (seal.zone == Zone::Attached && seal.host == id && catalog_.at(seal.definition).damageBonus) {
            const auto &sealDefinition = catalog_.at(seal.definition);
            link.item.effects.push_back({EffectKind::Damage, sealDefinition.damageBonus,
                std::find(sealDefinition.traits.begin(), sealDefinition.traits.end(), "thunder_damage_bonus") !=
                    sealDefinition.traits.end() ? DamageType::Thunder : DamageType::Force,
                EffectRecipient::Opponent});
        }
    s.chain->links.push_back(std::move(link));
    s.chain->hadActualLinks = true;
    s.chain->window = ResponseWindow::Cast;
    s.events.push_back({flip ? "flip" : "cast", flip ? "反转并发动法术" : "发动法术", -1, id});
    s.chain->priority = 1 - actor;
    s.chain->passes = 0;
    s.decision.reset();
    return {};
}
std::string GameEngine::responseSetupV2(PlayerId actor, const Command &command) {
    auto &s = state_;
    if (!s.decision || !s.chain || s.chain->mode != ChainMode::Building || s.chain->priority != actor)
        return "priority";
    const auto *flip = std::get_if<FlipAmbush>(&command);
    const auto *prepare = std::get_if<PrepareCast>(&command);
    const auto id = flip ? flip->card : prepare->card;
    const auto token = flip ? flip->decision : prepare->decision;
    if (token != s.decision->id) return "stale_decision";
    const auto found = s.cards.find(id);
    if (found == s.cards.end() || found->second.owner != actor) return "invalid_card";
    auto &card = found->second; const auto &def = catalog_.at(card.definition);
    const bool analytic = def.type == CardType::Analytic;
    const bool spell = spellType(def.type);
    if (!catalog_.usesAlphaV252() && !analytic) return "response_source";
    if (!spell || card.zone != Zone::Analysis) return "response_source";
    if (!spellSpeedAllowed(s, def)) return "spell_speed: 咒文速度不允许当前时点";
    if (!catalog_.usesAlphaV252() && actor != s.active &&
        (!card.wasAmbushed || card.ambushedTurn >= s.globalTurn)) return "response_source";
    if (flip) {
        if (!card.faceDown || card.ambushedTurn >= s.globalTurn ||
            !Rules::canHost(s, catalog_, id, card.host)) return "ambush_wait";
        if (!analytic) return activateSpellV252(actor, id, flip->target, flip->targets, flip->discard,
                                                true, token);
        if (catalog_.usesAlphaV252() && sique::hasFormation(s, 1 - actor, "moon")) return "moon_setup_restriction";
        const int fee = Rules::analysisCost(s, catalog_, id, card.host);
        if (!sique::canPay(s, actor, fee)) return "cost";
        sique::pay(s, actor, fee); card.analysisLoad += fee;
        ResourceRules::increasedLoad(s, catalog_, actor, fee);
        if (s.result != -1) return {};
        card.faceDown = false; card.settingPaid = fee; card.analysisProgress = 0;
        card.analysisStarted = s.players[actor].ownTurn;
        card.spell = def.analysisTurns ? SpellState::Analyzing : SpellState::Ready;
        if (card.spell == SpellState::Ready) analysisCompletedV2(id);
        quickAnalysisV2(id, card.host);
        s.chain->window = ResponseWindow::Action;
    } else {
        if (!analytic) return "not_ready";
        const bool targetsValid = catalog_.usesAlphaV252() && def.id == "unravel"
            ? !prepare->target && prepare->targets.empty()
            : validTargets(s, def.target, def.targetCount, actor, prepare->target, prepare->targets);
        if (card.faceDown || card.spell != SpellState::Ready || card.canceledTurn == s.players[actor].ownTurn ||
            prepare->discard || !targetsValid)
            return "not_ready";
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "casting_capacity";
        for (const auto &link : s.chain->links)
            if (link.item.source == id) return "response_used";
        card.targetCard = prepare->target; card.targets = prepare->targets;
        ChainLink link; link.id = s.nextLink++; link.kind = LinkKind::Preparation; link.speed = def.speed;
        link.item = snapshot(card, def); link.item.effects.clear(); link.item.requiresSource = true;
        if (catalog_.usesAlphaV252() && def.id == "unravel") link.item.target = TargetKind::None;
        s.chain->links.push_back(std::move(link)); s.chain->hadActualLinks = true;
        s.chain->window = ResponseWindow::Prepare;
    }
    s.events.push_back({flip ? "flip" : "prepare_declared", flip ? "反转解析法术" : "宣告准备施法", -1, id});
    s.chain->priority = catalog_.usesAlphaV252() ? 1 - actor : 1 - s.active; s.chain->passes = 0;
    s.decision.reset();
    return {};
}
std::vector<LegalAction> GameEngine::responseSetupsV2(PlayerId actor, DecisionId token) const {
    std::vector<LegalAction> out;
    auto add = [&](std::string name, CardId id, Command command) {
        auto trial = *this;
        trial.state_.decision = PendingDecision{token, actor, DecisionKind::Response, {}, true};
        if (trial.responseSetupV2(actor, command).empty()) out.push_back({std::move(name), id, 0, std::move(command)});
    };
    for (const auto &[id, card] : state_.cards) {
        if (card.owner != actor || card.zone != Zone::Analysis) continue;
        const auto &definition = catalog_.at(card.definition);
        if (card.faceDown && spellType(definition.type) && (catalog_.usesAlphaV252() || definition.type == CardType::Analytic)) {
            const auto targetSets = definition.type == CardType::Analytic ? std::vector<std::vector<CardId>>{{}}
                : sique::detachCost(definition) ? sique::detachOptions(state_, catalog_, actor, definition)
                : sets(state_, definition.target, definition.targetCount, actor);
            std::vector<CardId> discards = arcana::costOptions(state_, catalog_, actor, id);
            if (std::find(definition.traits.begin(), definition.traits.end(), "discard_spell_search") !=
                definition.traits.end()) {
                discards.clear();
                for (const auto &[discard, candidate] : state_.cards)
                    if (discard != id && candidate.owner == actor && candidate.zone == Zone::Hand &&
                        spellType(catalog_.at(candidate.definition).type)) discards.push_back(discard);
            }
            for (const auto &targets : targetSets) for (auto discard : discards) {
                const auto target = sique::detachCost(definition) || targets.empty() ? 0 : targets.front();
                add("反转 " + definition.name, id, FlipAmbush{id, target, targets, token, discard});
            }
        }
        else if (card.spell == SpellState::Ready) {
            const auto targetSets = catalog_.usesAlphaV252() && definition.id == "unravel"
                ? std::vector<std::vector<CardId>>{{}} : sets(state_, definition.target, definition.targetCount, actor);
            for (const auto &targets : targetSets)
                add("准备施法 " + definition.name, id,
                    PrepareCast{id, targets.empty() ? 0 : targets.front(), 0, targets, token});
        }
    }
    return out;
}
std::string GameEngine::executeV2(PlayerId actor, const Command &command) {
    auto &s = state_;
    if (std::holds_alternative<Choose>(command) || std::holds_alternative<Respond>(command) ||
        std::holds_alternative<PassResponse>(command)) return "stale_decision";
    if (s.decision || s.chain || s.effect || !s.queue.items.empty()) return "pending: 先处理待决事项";
    if (actor != s.active || s.phaseStep != PhaseStep::Body) return "priority: 当前没有行动权";
    if (const auto *advance = std::get_if<AdvancePhase>(&command)) {
        if (!s.phaseGate || advance->gate != s.phaseGate) return "phase_gate: 无效阶段令牌";
        if (advance->skipCast && s.phase != Phase::Main) return "wrong_phase: 只能在主要阶段跳过施法";
        s.skipCastPhase = advance->skipCast;
        s.phaseGate = 0;
        s.phaseStep = PhaseStep::EndWindow;
        if (s.phase == Phase::Cast) s.returnRequested = true;
        return {};
    }
    if (std::holds_alternative<Advance>(command)) {
        if (s.phase != Phase::Main || !s.phaseGate) return "phase_gate";
        s.phaseGate = 0; s.phaseStep = PhaseStep::EndWindow; return {};
    }
    auto cardId = std::visit([](const auto &item) -> CardId {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, Choose> || std::is_same_v<T, Advance> ||
                      std::is_same_v<T, AdvancePhase> || std::is_same_v<T, PassResponse> || std::is_same_v<T, Surrender> ||
                      std::is_same_v<T, Timeout>) return 0;
        else return item.card;
    }, command);
    auto it = s.cards.find(cardId);
    if (it == s.cards.end() || it->second.owner != actor) return "invalid_card";
    auto &c = it->second;
    const auto &def = catalog_.at(c.definition);
    auto &pl = s.players[actor];
    if (catalog_.usesAlphaV252() && def.type != CardType::Formation &&
        (std::holds_alternative<StartAnalysis>(command) || std::holds_alternative<SetAmbush>(command) ||
         std::holds_alternative<FlipAmbush>(command) || std::holds_alternative<PreloadWord>(command)) &&
        std::find(def.traits.begin(), def.traits.end(), "unique") != def.traits.end())
        for (const auto &[id, other] : s.cards)
            if (id != cardId && other.owner == actor && other.definition == c.definition &&
                (other.zone == Zone::Analysis || other.zone == Zone::Casting))
                return "unique: 己方场上已存在同名唯一卡牌";
    const auto activationAllowed = [&] {
        return sique::activationAllowed(s, catalog_, actor, def) &&
            (def.id != "sique-aroma-lure" || sique::hasFormation(s, actor, "sique-cauldron")) &&
            arcana::activationError(s, catalog_, actor, cardId, 0).empty();
    };
    const auto activationTargets = [&](CardId target, const std::vector<CardId> &ids) {
        return sique::detachCost(def) ? target == 0 && sique::validDetach(s, catalog_, actor, def, ids)
            : validTargets(s, def.target, def.targetCount, actor, target, ids);
    };
    const auto detach = [&](const std::vector<CardId> &ids) {
        if (sique::detachCost(def)) for (auto id : ids) StateMaintenance::leave(s, id, &catalog_);
    };
    const bool requiresStormFormation = std::find(def.traits.begin(), def.traits.end(),
        "requires_storm_formation") != def.traits.end();
    const bool requiresStormForSetup = std::find(def.traits.begin(), def.traits.end(),
        "requires_storm_setup") != def.traits.end();
    const bool requiresStormHeartSetup = std::find(def.traits.begin(), def.traits.end(),
        "requires_storm_heart_setup") != def.traits.end();
    auto ownedFormation = [&](CardId id) {
        auto host = s.cards.find(id);
        return host != s.cards.end() && host->second.owner == actor && host->second.zone == Zone::Analysis &&
            catalog_.at(host->second.definition).type == CardType::Formation;
    };
    auto pay = [&](int mana, int load) {
        if (!sique::canPay(s, actor, mana)) return false;
        sique::pay(s, actor, mana);
        StateMaintenance::check(s, catalog_);
        if (s.result != -1) return true;
        if (load > 0) { c.analysisLoad += load; ResourceRules::increasedLoad(s, catalog_, actor, load); }
        s.events.push_back({"pay", "支付魔素", -1, cardId, mana});
        return true;
    };
    auto ring = [&](CardId host, bool faceDown = false) {
        return (!requiresStormHeartSetup ||
                (s.cards.count(host) && catalog_.at(s.cards.at(host).definition).id == "storm-heart")) &&
            Rules::canHost(s, catalog_, cardId, host, faceDown) &&
            Rules::freeRing(s, catalog_, host) >= 0;
    };
    auto place = [&](CardId host) {
        c.zone = Zone::Analysis; c.host = host; c.sourceFormation = host;
        c.ringIndex = Rules::freeRing(s, catalog_, host);
        c.sourceRingIndex = c.ringIndex;
        c.wasAmbushed = false;
        c.fieldOrder = s.nextFieldOrder++;
    };
    auto phaseSpeed = [&] {
        if (catalog_.usesAlphaV252()) return spellSpeedAllowed(s, def);
        return (def.speed >= 3 || (def.speed >= 2 && (s.phase == Phase::Main || s.phase == Phase::Cast)) ||
                (def.speed == 1 && s.phase == Phase::Cast));
    };
    auto setupSpeed = [&] {
        if (catalog_.usesAlphaV252() && sique::hasFormation(s, 1 - actor, "moon")) return false;
        if (catalog_.usesAlphaV252()) return spellSpeedAllowed(s, def, true);
        return def.speed >= 3 || (def.speed >= 2 && (s.phase == Phase::Main || s.phase == Phase::Cast)) ||
               (def.speed == 1 && s.phase == Phase::Main);
    };
    const bool discardSpellSearch = std::find(def.traits.begin(), def.traits.end(), "discard_spell_search") != def.traits.end();
    auto validSpellCost = [&](CardId material) {
        if (!discardSpellSearch) return arcana::validCost(s, catalog_, actor, cardId, material);
        const auto found = s.cards.find(material);
        if (!material || material == cardId || found == s.cards.end() || found->second.owner != actor ||
            found->second.zone != Zone::Hand || !spellType(catalog_.at(found->second.definition).type)) return false;
        const auto &school = catalog_.at(found->second.definition).school;
        return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &item) {
            return item.second.owner == actor && item.second.zone == Zone::Deck &&
                spellType(catalog_.at(item.second.definition).type) && catalog_.at(item.second.definition).school == school;
        });
    };
    auto validRecovery = [&](CardId target) {
        if (def.target != TargetKind::EnemyAshCard) return true;
        const auto selected = s.cards.find(target);
        if (selected == s.cards.end() || catalog_.at(selected->second.definition).type != CardType::Analytic ||
            catalog_.at(selected->second.definition).rank > 5) return false;
        return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &item) {
            const auto &host = item.second;
            return host.owner == actor && host.zone == Zone::Analysis && !host.faceDown &&
                catalog_.at(host.definition).type == CardType::Formation &&
                catalog_.at(selected->second.definition).rank <= Rules::maxRank(s, catalog_, item.first) &&
                Rules::freeRing(s, catalog_, item.first) >= 0;
        });
    };
    auto paySpellCost = [&](CardId material) {
        if (discardSpellSearch) {
            if (material) { StateMaintenance::leave(s, material, &catalog_); c.targets = {material}; }
        } else arcana::payCost(s, catalog_, actor, cardId, material);
    };
    if (const auto *cmd = std::get_if<PlayAction>(&command)) {
        if (!activationAllowed()) return "sique_activation_restriction";
        if (c.zone != Zone::Hand || def.type != CardType::Action ||
            pl.blockedActionTurn == pl.ownTurn)
            return "invalid_action: 行动不可用";
        if (!validTargets(s, def.target, def.targetCount, actor, cmd->target, cmd->targets))
            return "target: 无效目标";
        if (!sique::canPay(s, actor, def.cost) || Rules::load(s, actor) + def.burden > Rules::capacity(s, catalog_, actor))
            return "cost: 魔素或荷载不足";
        sique::pay(s, actor, def.cost);
        StateMaintenance::check(s, catalog_);
        if (s.result != -1) return {};
        ++pl.actionsPlayed;
        c.targetCard = cmd->target;
        c.targets = targetCards(cmd->target, cmd->targets);
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        if (def.burden)
            s.temporary.push_back({s.nextLoad++, actor, c.id, def.burden, pl.ownTurn});
        beginEffect(c.id, AfterEffect::Action);
        return {};
    }
    if (const auto *cmd = std::get_if<ActivateAbility>(&command)) {
        if (!activationAllowed()) return "sique_activation_restriction";
        if (s.phase != Phase::Main || s.phaseStep != PhaseStep::Body || actor != s.active || s.chain || s.effect)
            return "ability_unavailable";
        const auto hasTrait = [&](const char *name) {
            return std::find(def.traits.begin(), def.traits.end(), name) != def.traits.end();
        };
        if (cmd->ability == "storm_search" || cmd->ability == "storm_heart_choice") {
            const bool isSearch = cmd->ability == "storm_search" && c.definition == "storm-crest" &&
                hasTrait("activate_storm_search");
            const bool isHeart = cmd->ability == "storm_heart_choice" && c.definition == "storm-heart" &&
                hasTrait("activate_storm_heart_choice");
            if (!(isSearch || isHeart) || c.zone != Zone::Analysis || def.type != CardType::Formation ||
                c.stormPoints < 2 || c.stormAbilityTurn == pl.ownTurn)
                return "ability_unavailable";
            std::vector<CardId> options;
            if (isSearch) {
                for (auto id : pl.deck) {
                    const auto &candidate = s.cards.at(id);
                    const auto &candidateDefinition = catalog_.at(candidate.definition);
                    const auto &tags = candidateDefinition.tags;
                    const bool spell = candidateDefinition.type == CardType::Analytic ||
                        candidateDefinition.type == CardType::Word || candidateDefinition.type == CardType::Talisman;
                    if (candidate.zone == Zone::Deck && spell &&
                        std::find(tags.begin(), tags.end(), "风暴") != tags.end())
                        options.push_back(id);
                }
                if (options.empty()) return "ability_target";
            } else {
                options.push_back(1); // Draw one card.
                const bool recoverable = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                    const auto &candidate = entry.second;
                    const auto &candidateDefinition = catalog_.at(candidate.definition);
                    return candidate.owner == actor && candidate.zone == Zone::Ash &&
                        candidateDefinition.type != CardType::Formation &&
                        std::find(candidateDefinition.tags.begin(), candidateDefinition.tags.end(), "风暴") !=
                            candidateDefinition.tags.end();
                });
                if (recoverable) options.push_back(2); // Recover a Storm card from Ash.
            }
            c.stormPoints -= 2;
            c.stormAbilityTurn = pl.ownTurn;
            decision(actor, isSearch ? DecisionKind::StormSearch : DecisionKind::StormHeartChoice,
                     std::move(options));
            return {};
        }
        if (cmd->ability != "storm_annihilation_destroy" || cmd->target == 0 || c.zone != Zone::Attached ||
            def.type != CardType::Seal || !hasTrait("activate_storm_crest_destroy"))
            return "ability_unavailable";
        const auto host = s.cards.find(c.host);
        if (host == s.cards.end() || host->second.owner != actor || host->second.zone != Zone::Analysis ||
            catalog_.at(host->second.definition).type != CardType::Formation || host->second.stormPoints < 2 ||
            std::find(catalog_.at(host->second.definition).tags.begin(),
                      catalog_.at(host->second.definition).tags.end(), "风暴") ==
                catalog_.at(host->second.definition).tags.end())
            return "ability_cost";
        const auto target = s.cards.find(cmd->target);
        if (target == s.cards.end() || target->second.owner == actor || target->second.faceDown ||
            target->second.zone != Zone::Analysis || target->second.spell != SpellState::None ||
            catalog_.at(target->second.definition).type != CardType::Formation ||
            Rules::occupied(s, cmd->target) != 0)
            return "ability_target";
        const auto &targetDefinition = catalog_.at(target->second.definition);
        if (targetDefinition.opponentDestroyProtected ||
            std::find(targetDefinition.traits.begin(), targetDefinition.traits.end(), "effect_immune") !=
                targetDefinition.traits.end() ||
            std::find(targetDefinition.traits.begin(), targetDefinition.traits.end(), "effect_indestructible") !=
                targetDefinition.traits.end())
            return "ability_target";
        host->second.stormPoints -= 2;
        auto trigger = snapshot(c, def);
        trigger.id = s.nextTrigger++;
        trigger.batch = s.nextBatch++;
        trigger.targetCard = cmd->target;
        trigger.targetPlayer = 1 - actor;
        trigger.target = TargetKind::EnemyCard;
        trigger.effects = {{EffectKind::Destroy, 1}};
        ChainLink root;
        root.kind = LinkKind::Trigger;
        root.item = std::move(trigger);
        startChain(ResponseWindow::Action, std::move(root), 1 - actor);
        return {};
    }
    if (const auto *cmd = std::get_if<SetFormation>(&command)) {
        if (def.id == "sique-cauldron" && !sique::hasFormation(s, actor, "sique-campfire"))
            return "sique_requires_campfire";
        if (s.phase != Phase::Main || c.zone != Zone::SideDeck || def.type != CardType::Formation || pl.formations)
            return "formation_limit";
        if (cmd->replace && (!ownedFormation(cmd->replace) || cmd->replace == cardId)) return "invalid_replacement";
        if (!recipeMatches(s, catalog_, actor, def, cmd->materials)) return "recipe_materials";
        if (catalog_.usesAlphaV252() && std::find(def.traits.begin(), def.traits.end(), "max_two") != def.traits.end()) {
            int count = 0;
            for (const auto &[id, other] : s.cards)
                if (other.owner == actor && other.zone == Zone::Analysis && other.definition == c.definition &&
                    id != cmd->replace && std::find(cmd->materials.begin(), cmd->materials.end(), id) == cmd->materials.end()) ++count;
            if (count >= 2) return "formation_copy_limit: 此阵法最多存在两张";
        }
        if (std::find(def.traits.begin(), def.traits.end(), "unique") != def.traits.end())
            for (const auto &[id, other] : s.cards)
                if (other.owner == actor && other.zone == Zone::Analysis && other.definition == c.definition &&
                    id != cmd->replace && std::find(cmd->materials.begin(), cmd->materials.end(), id) == cmd->materials.end())
                    return "unique_formation";
        if (std::find(cmd->materials.begin(), cmd->materials.end(), cmd->replace) != cmd->materials.end())
            return "recipe_materials";
        int fee = def.cost;
        for (const auto &part : def.recipe)
            if (part.kind == RecipeCostKind::Mana) fee += part.amount;
        if (!sique::canPay(s, actor, fee)) return "cost: 魔素不足";
        int body = usedBody(s, catalog_, actor);
        if (cmd->replace) body -= catalog_.at(s.cards.at(cmd->replace).definition).body;
        for (auto id : cmd->materials)
            if (catalog_.at(s.cards.at(id).definition).type == CardType::Formation)
                body -= catalog_.at(s.cards.at(id).definition).body;
        if (body + def.body > 5) return "formation_capacity";
        if (std::find(def.traits.begin(), def.traits.end(), "storm_heart_upgrade") != def.traits.end()) {
            const auto crest = std::find_if(cmd->materials.begin(), cmd->materials.end(), [&](CardId material) {
                return s.cards.at(material).definition == "storm-crest";
            });
            if (crest == cmd->materials.end() || s.cards.at(*crest).stormPoints < 10)
                return "storm_points";
        }
        sique::pay(s, actor, fee);
        StateMaintenance::check(s, catalog_);
        if (s.result != -1) return {};
        for (auto id : cmd->materials) StateMaintenance::leave(s, id, &catalog_);
        if (cmd->replace) StateMaintenance::leave(s, cmd->replace, &catalog_);
        c.zone = Zone::Analysis; c.fieldOrder = s.nextFieldOrder++;
        c.settingPaid = fee; ++pl.formations;
        s.events.push_back({"formation_set", "设置阵法", -1, cardId});
        if (std::find(def.traits.begin(), def.traits.end(), "return_formation_from_ash") != def.traits.end()) {
            auto trigger = snapshot(c, def); trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
            trigger.effects = {{EffectKind::RecoverFormation, 1}}; s.queue.append(std::move(trigger));
        }
        if (std::find(def.traits.begin(), def.traits.end(), "search_great_storm_on_setup") != def.traits.end()) {
            const auto found = std::find_if(pl.deck.begin(), pl.deck.end(), [&](CardId candidate) {
                return s.cards.at(candidate).zone == Zone::Deck && s.cards.at(candidate).definition == "great-storm";
            });
            if (found != pl.deck.end()) {
                const auto searched = *found;
                pl.deck.erase(found);
                s.cards.at(searched).zone = Zone::Hand;
                for (std::size_t size = pl.deck.size(); size > 1; --size)
                    std::swap(pl.deck[size - 1], pl.deck[bounded(s, static_cast<std::uint32_t>(size))]);
                s.events.push_back({"search", "检索大风暴术加入手牌", actor, searched});
            }
        }
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<RemoveFormation>(&command)) {
        if (!catalog_.usesAlphaV252()) return "invalid_removal";
        if (s.phase != Phase::Main || c.zone != Zone::Analysis || def.type != CardType::Formation || pl.formations)
            return "invalid_removal";
        StateMaintenance::leave(s, cardId, &catalog_); ++pl.formations; actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<StartAnalysis>(&command)) {
        if (def.type == CardType::Analytic && greatStormLocksAnalysis(s, catalog_, actor))
            return "great_storm_lock: 大风暴术阻止设置其他解析法术";
        if (requiresStormForSetup && !hasStormFormation(s, catalog_, actor))
            return "storm_formation: 需要己方风暴阵法";
        if (!setupSpeed() || c.zone != Zone::Hand || def.type != CardType::Analytic || !ring(cmd->formation))
            return "ring_or_rank";
        int fee = Rules::analysisCost(s, catalog_, cardId, cmd->formation);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        place(cmd->formation); c.settingPaid = fee; c.analysisProgress = 0;
        c.spell = def.analysisTurns <= 0 ? SpellState::Ready : SpellState::Analyzing;
        c.analysisStarted = pl.ownTurn; c.targetCard = cmd->target; c.targets = cmd->targets;
        if (c.spell == SpellState::Ready) analysisCompletedV2(cardId);
        quickAnalysisV2(cardId, cmd->formation);
        if (catalog_.usesAlphaV252()) {
            ChainLink setup;
            setup.kind = LinkKind::Setup;
            setup.item = snapshot(c, def);
            setup.item.effects.clear();
            startChain(ResponseWindow::Prepare, std::move(setup), 1 - actor);
        } else actionTimingV2();
        return {};
    }
    if (const auto *cmd = std::get_if<SetAmbush>(&command)) {
        if (def.type == CardType::Analytic && greatStormLocksAnalysis(s, catalog_, actor))
            return "great_storm_lock: 大风暴术阻止设置其他解析法术";
        if (requiresStormForSetup && !hasStormFormation(s, catalog_, actor))
            return "storm_formation: 需要己方风暴阵法";
        if (s.phase != Phase::Main || c.zone != Zone::Hand || !spellType(def.type) || !ring(cmd->formation, true))
            return "invalid_ambush";
        place(cmd->formation); c.faceDown = true; c.wasAmbushed = true;
        c.publiclyRevealed = false;
        c.ambushedTurn = s.globalTurn; c.spell = SpellState::None;
        s.events.push_back({"ambush", "盖伏法术", -1, cardId});
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<FlipAmbush>(&command)) {
        if (catalog_.usesAlphaV252() && def.type != CardType::Analytic)
            return activateSpellV252(actor, cardId, cmd->target, cmd->targets, cmd->discard, true);
        if (def.type != CardType::Analytic && !activationAllowed()) return "sique_activation_restriction";
        if ((requiresStormFormation || requiresStormForSetup) && !hasStormFormation(s, catalog_, actor))
            return "storm_formation: 需要己方风暴阵法";
        if (cmd->decision || !c.faceDown || c.zone != Zone::Analysis || c.ambushedTurn >= s.globalTurn ||
            (actor != s.active) || !setupSpeed()) return "ambush_wait";
        if (!Rules::canHost(s, catalog_, cardId, c.host) ||
            (requiresStormHeartSetup && catalog_.at(s.cards.at(c.host).definition).id != "storm-heart"))
            return "host_restriction";
        if (def.type == CardType::Analytic && def.speed == 1 && s.phase != Phase::Main)
            return "spell_speed";
        if (def.type == CardType::Analytic) {
            const int fee = Rules::analysisCost(s, catalog_, cardId, c.host);
            if (!pay(fee, fee)) return "cost: 魔素不足";
            if (s.result != -1) return {};
            c.faceDown = false; c.settingPaid = fee;
            c.analysisProgress = 0; c.analysisStarted = pl.ownTurn;
            c.spell = def.analysisTurns <= 0 ? SpellState::Ready : SpellState::Analyzing;
            if (c.spell == SpellState::Ready) analysisCompletedV2(cardId);
            quickAnalysisV2(cardId, c.host);
            actionTimingV2(); return {};
        }
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "casting_capacity";
        if (!activationAllowed() || def.responseOnly || !activationTargets(cmd->target, cmd->targets) || !validSpellCost(cmd->discard) || !validRecovery(cmd->target))
            return "target_or_response_window";
        const int fee = Rules::castCost(s, catalog_, cardId);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        detach(cmd->targets);
        c.settingPaid = fee; c.faceDown = false; c.zone = Zone::Casting; c.host = 0; c.ringIndex = -1;
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = sique::detachCost(def) ? std::vector<CardId>{} : cmd->targets;
        paySpellCost(cmd->discard);
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<PreloadWord>(&command)) {
        if (catalog_.usesAlphaV252() && sique::hasFormation(s, 1 - actor, "moon")) return "moon_setup_restriction";
        if (!activationAllowed()) return "sique_activation_restriction";
        if (requiresStormFormation && !hasStormFormation(s, catalog_, actor))
            return "storm_formation: 需要己方风暴阵法";
        if (pl.blockedActionTurn == pl.ownTurn) return "blocked_incantation";
        if (!setupSpeed() || c.zone != Zone::Hand || def.type != CardType::Word || def.responseOnly ||
            !ring(cmd->formation) ||
            static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "word_setup";
        if (!activationTargets(cmd->target, cmd->targets) || !validSpellCost(cmd->discard)) return "target_or_cost";
        const int fee = Rules::castCost(s, catalog_, cardId);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        detach(cmd->targets);
        place(cmd->formation); c.settingPaid = fee;
        c.spell = SpellState::Ready;
        c.zone = Zone::Casting; c.host = 0; c.ringIndex = -1;
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = sique::detachCost(def) ? std::vector<CardId>{} : cmd->targets;
        paySpellCost(cmd->discard);
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        analysisCompletedV2(cardId); castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<PrepareCast>(&command)) {
        if (catalog_.usesAlphaV252() && c.definition == "unravel") {
            if (cmd->target || !cmd->targets.empty()) return "target: 碎环术在发动时选择目标";
        } else if (!validTargets(s, def.target, def.targetCount, actor, cmd->target, cmd->targets)) {
            return "target";
        }
        return prepare(actor, *cmd, false);
    }
    if (const auto *cmd = std::get_if<ActivateSpell>(&command)) {
        if (catalog_.usesAlphaV252())
            return activateSpellV252(actor, cardId, cmd->target, cmd->targets, cmd->discard, false);
        if (!activationAllowed()) return "sique_activation_restriction";
        if (requiresStormFormation && !hasStormFormation(s, catalog_, actor))
            return "storm_formation: 需要己方风暴阵法";
        if (c.zone != Zone::Casting || c.spell != SpellState::Ready || !phaseSpeed() ||
            !activationTargets(cmd->target, cmd->targets) || !validSpellCost(cmd->discard) || !validRecovery(cmd->target))
            return "not_ready";
        CardId stormCostFormation = 0;
        if (std::any_of(def.effects.begin(), def.effects.end(), [](const Effect &effect) {
                return effect.kind == EffectKind::StormCyclone;
            })) {
            for (const auto &[formationId, formation] : s.cards) {
                const auto &formationDefinition = catalog_.at(formation.definition);
                if (formation.owner == actor && formation.zone == Zone::Analysis &&
                    formationDefinition.type == CardType::Formation && formation.stormPoints >= 5 &&
                    std::find(formationDefinition.tags.begin(), formationDefinition.tags.end(), "风暴") !=
                        formationDefinition.tags.end()) {
                    stormCostFormation = formationId;
                    break;
                }
            }
            if (!stormCostFormation) return "storm_points: 风暴点数不足";
        }
        const int fee = Rules::castCost(s, catalog_, cardId);
        if (!sique::canPay(s, actor, fee)) return "cost: 魔素不足";
        sique::pay(s, actor, fee); c.castLoad += fee; ResourceRules::increasedLoad(s, catalog_, actor, fee);
        if (s.result != -1) return {};
        detach(cmd->targets);
        if (stormCostFormation) s.cards.at(stormCostFormation).stormPoints -= 5;
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = sique::detachCost(def) ? std::vector<CardId>{} : cmd->targets;
        paySpellCost(cmd->discard);
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<AttachSeal>(&command)) {
        auto host = s.cards.find(cmd->host);
        if (s.phase != Phase::Main || c.zone != Zone::Hand || def.type != CardType::Seal ||
            host == s.cards.end() || host->second.owner != actor || host->second.faceDown ||
            (host->second.zone != Zone::Analysis && host->second.zone != Zone::Casting)) return "seal_host";
        if (!catalog_.usesAlphaV252() && seals(s, cmd->host) && host->second.definition != "sique-cauldron")
            return "seal_host";
        const auto &hostDefinition = catalog_.at(host->second.definition);
        if (hostDefinition.type != CardType::Formation && !spellType(hostDefinition.type))
            return "seal_host";
        if (std::find(def.traits.begin(), def.traits.end(), "sique_cauldron_only") != def.traits.end() &&
            hostDefinition.id != "sique-cauldron") return "seal_host";
        const auto hasTrait = [&](const char *name) {
            return std::find(def.traits.begin(), def.traits.end(), name) != def.traits.end();
        };
        const bool stormFormation = hostDefinition.type == CardType::Formation &&
            std::find(hostDefinition.tags.begin(), hostDefinition.tags.end(), "风暴") != hostDefinition.tags.end();
        if ((hasTrait("storm_formation_host_only") && !stormFormation) ||
            (hasTrait("storm_formation_host_restriction") &&
             hostDefinition.type == CardType::Formation && !stormFormation) ||
            (hasTrait("storm_formation_host_restriction") &&
             hostDefinition.type != CardType::Formation && hostDefinition.type != CardType::Analytic &&
             hostDefinition.type != CardType::Word && hostDefinition.type != CardType::Talisman))
            return "seal_host";
        if (std::find(def.traits.begin(), def.traits.end(), "sique_requires_cauldron") != def.traits.end() &&
            !sique::hasFormation(s, actor, "sique-cauldron")) return "sique_requires_cauldron";
        if (!pay(def.cost, 0)) return "cost: 魔素不足";
        c.zone = Zone::Attached; c.host = cmd->host;
        sique::attached(s, catalog_, cardId);
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<RemoveSeal>(&command)) {
        if (s.phase != Phase::Main || c.zone != Zone::Attached || pl.sealRemovals) return "seal_limit";
        StateMaintenance::leave(s, cmd->card, &catalog_); ++pl.sealRemovals;
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<Abandon>(&command)) {
        if (!spellType(def.type) || (c.zone != Zone::Analysis && c.zone != Zone::Casting) ||
            (c.zone == Zone::Casting ? s.phase != Phase::Cast : s.phase != Phase::Main)) return "invalid_abandon";
        StateMaintenance::leave(s, cmd->card, &catalog_);
        actionTimingV2(); return {};
    }
    return "unsupported_v2_command";
}
}
