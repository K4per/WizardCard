#include "engine_internal.hpp"
#include "wizard/core/card_rules.hpp"
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
    if ((!need.definition.empty() && card.definition != need.definition) ||
        (!need.definitions.empty() &&
         std::find(need.definitions.begin(), need.definitions.end(), card.definition) == need.definitions.end()))
        return false;
    if ((!need.school.empty() && definition.school != need.school) ||
        (!need.schools.empty() &&
         std::find(need.schools.begin(), need.schools.end(), definition.school) == need.schools.end()))
        return false;
    if (need.readyOnly && card.spell != SpellState::Ready && card.spell != SpellState::Active) return false;
    if (need.fireDamage) {
        if (!card_rules::isFireSpell(definition)) return false;
    }
    (void)catalog;
    return true;
}
bool recipeMatches(const GameState &state, const CardCatalog &catalog, PlayerId player,
                   const CardDefinition &formation, const std::vector<CardId> &materials) {
    std::vector<const RecipeCost *> slots;
    for (const auto &need : formation.recipe)
        if (need.kind != RecipeCostKind::Mana && need.kind != RecipeCostKind::Life)
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
            if (hasCapability(formation, Capability::Moon) && slots[index]->definition.empty() &&
                !hasCapability(definition, Capability::MoonMaterialFormation)) continue;
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
    if (catalog().at(s.cards.at(spell).definition).type != CardType::Analytic) return;
    const auto owner = s.cards.at(spell).owner;
    const auto batch = s.nextBatch++;
    for (const auto &[id, formation] : s.cards) {
        if (formation.owner != owner || formation.zone != Zone::Analysis) continue;
        const auto &def = catalog().at(formation.definition);
        if (def.type == CardType::Formation &&
            hasCapability(def, Capability::ClearLoadOnAnalysis)) {
            auto trigger = snapshot(formation, def); trigger.id = s.nextTrigger++; trigger.batch = batch;
            trigger.effects = {{EffectKind::ClearAnyLoad, 1}}; s.queue.append(std::move(trigger));
        }
    }
}
void GameEngine::settingCompletedV253(CardId spell, CardId host) {
    auto &s = state_;
    if (s.result != -1 || !s.cards.count(host)) return;

    const auto &formation = s.cards.at(host);
    if (!hasCapability(catalog().at(formation.definition), Capability::SiqueCampfire) || formation.zone != Zone::Analysis ||
        !card_rules::isFireSpell(catalog().at(s.cards.at(spell).definition))) return;
    auto trigger = snapshot(formation, catalog().at(formation.definition));
    trigger.id = s.nextTrigger++;
    trigger.batch = s.nextBatch++;
    trigger.effects = {{EffectKind::Damage, 1, DamageType::Fire, EffectRecipient::Opponent}};
    s.queue.append(std::move(trigger));
    s.events.push_back({"campfire_setting", "味契·篝火：成功设置火焰伤害法术，触发1点火焰伤害", -1, host, 1});
}
void GameEngine::quickAnalysisV2(CardId spell, CardId host) {
    auto &s = state_;
    auto &card = s.cards.at(spell);
    auto &formation = s.cards.at(host);
    const auto &def = catalog().at(card.definition);
    const auto &hostDefinition = catalog().at(formation.definition);
    const int advance = hasCapability(hostDefinition, Capability::QuickHostAnalysis2) ? 2 :
        hasCapability(hostDefinition, Capability::QuickHostAnalysis1) ? 1 : 0;
    if (advance) {
        card_rules::advanceAnalysis(s, catalog(), spell, advance);
        s.temporary.push_back({s.nextLoad++, card.owner, host, advance, s.players[card.owner].ownTurn + 1, true});
        ResourceRules::increasedLoad(s, catalog(), card.owner, advance);
        return;
    }
    if (!catalog().at(formation.definition).quickRankOne || def.rank > 1 ||
        card.spell != SpellState::Analyzing || formation.quickAnalysisTurn == s.players[card.owner].ownTurn)
        return;
    card_rules::advanceAnalysis(s, catalog(), spell, def.analysisTurns - card.analysisProgress);
    formation.quickAnalysisTurn = s.players[card.owner].ownTurn;
    s.events.push_back({"analysis_ready", "阵法立即完成低位阶解析（每回合一次）", -1, spell});

}
void GameEngine::castingTraitsV2(CardId spell) {
    auto &s = state_;
    if (s.result != -1) return;
    const auto &card = s.cards.at(spell);
    {
        const auto batch = s.nextBatch++;
        for (const auto &[id, active] : s.cards) {
            const auto &d = catalog().at(active.definition);
            if (active.owner != card.owner && active.spell == SpellState::Active &&
                (active.zone == Zone::Analysis || active.zone == Zone::Casting) &&
                hasCapability(d, Capability::OpponentSpellLoad4)) {
                auto trigger = snapshot(active, d); trigger.id = s.nextTrigger++; trigger.batch = batch;
                trigger.effects = {{EffectKind::AddTemporary, 4, DamageType::Force, EffectRecipient::Opponent}};
                s.queue.append(std::move(trigger));
            }
        }
    }
    const auto host = s.cards.find(card.sourceFormation);
    if (host == s.cards.end() || host->second.zone != Zone::Analysis) return;
    if (hasCapability(catalog().at(host->second.definition), Capability::HostCastLifeForTemporary)) {
        if (!s.players[card.owner].debugInfiniteLife) --s.players[card.owner].life;
        s.events.push_back({"life_loss", "逆十字：失去1点生命", -1, spell, 1});
        StateMaintenance::check(s, catalog());
        if (s.result == -1) {
            ResourceRules::grantTemporaryLife(s, card.owner, spell, 2, s.players[card.owner].ownTurn + 1);
        }
    }
}
std::string GameEngine::activateSpellV252(PlayerId actor, CardId id, CardId target,
                                            const std::vector<CardId> &targets, CardId discard,
                                            bool flip, DecisionId token) {
    if (const auto error = validateActivation(actor, id, target, targets, discard, flip, token); !error.empty()) return error;
    auto &s = state_;
    const bool responding = token != 0;
    auto &card = s.cards.at(id);
    const auto &def = catalog().at(card.definition);
    const bool detach = card_rules::detachCost(def);
    const auto hasTrait = [&](Capability capability) { return hasCapability(def, capability); };
    CardId stormCostFormation = 0;
    if (std::any_of(def.effects.begin(), def.effects.end(), [](const Effect &effect) { return effect.kind == EffectKind::StormCyclone; }))
        for (const auto &[formationId, formation] : s.cards)
            if (formation.owner == actor && formation.zone == Zone::Analysis && formation.stormPoints >= 5 &&
                catalog().at(formation.definition).type == CardType::Formation && detail::stormTags(catalog().at(formation.definition).tags)) {
                stormCostFormation = formationId; break;
            }
    const int fee = Rules::castCost(s, catalog(), id);
    card_rules::pay(s, actor, fee, &catalog());
    StateMaintenance::check(s, catalog());
    if (s.result != -1) return {};
    card.castLoad += fee;
    ResourceRules::increasedLoad(s, catalog(), actor, fee);
    if (s.result != -1) return {};
    if (detach) for (auto seal : targets) StateMaintenance::leave(s, seal, &catalog());
    if (stormCostFormation) s.cards.at(stormCostFormation).stormPoints -= 5;
    if (flip) { card.settingPaid = fee; card.faceDown = false; card.zone = Zone::Casting;
                card.host = 0; card.ringIndex = -1; }
    card.spell = SpellState::Pending;
    card.targetCard = target;
    card.targets = detach ? std::vector<CardId>{} : targetCards(target, targets);
    if (hasTrait(Capability::DiscardSpellSearch)) {
        if (discard) { StateMaintenance::leave(s, discard, &catalog()); card.targets = {discard}; }
    } else card_rules::payExtraCost(s, catalog(), actor, id, discard);
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
        if (seal.zone == Zone::Attached && seal.host == id && catalog().at(seal.definition).damageBonus) {
            const auto &sealDefinition = catalog().at(seal.definition);
            link.item.effects.push_back({EffectKind::Damage, sealDefinition.damageBonus,
                hasCapability(sealDefinition, Capability::ThunderDamageBonus) ? DamageType::Thunder : DamageType::Force,
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
    const auto *word = std::get_if<PreloadWord>(&command);
    const auto id = flip ? flip->card : prepare ? prepare->card : word->card;
    const auto token = flip ? flip->decision : prepare ? prepare->decision : word->decision;
    if (token != s.decision->id) return "stale_decision";
    const auto found = s.cards.find(id);
    if (found == s.cards.end() || controllerOf(found->second) != actor) return "invalid_card";
    auto &card = found->second; const auto &def = catalog().at(card.definition);
    const bool analytic = def.type == CardType::Analytic;
    const bool spell = spellType(def.type);
    const bool handWord = word && word->release &&
                          card.zone == Zone::Hand && def.type == CardType::Word;
    if (!spell || (!handWord && card.zone != Zone::Analysis)) return "response_source";
    if (handWord && def.speed == 2 && actor != s.active)
        return "spell_speed: 手牌二速言灵只能在自己的回合响应";
    if (!spellSpeedAllowed(s, def)) return "spell_speed: 咒文速度不允许当前时点";
    if (handWord) {
        const auto hasTrait = [&](Capability capability) {
            return hasCapability(def, capability);
        };
        const auto host = s.cards.find(word->formation);
        if (def.responseOnly || s.players[actor].blockedActionTurn == s.players[actor].ownTurn ||
            !card_rules::activationAllowed(s, catalog(), actor, def) ||
            (hasCapability(def, Capability::SiqueAromaLure) &&
             !card_rules::hasFormation(s, catalog(), actor, Capability::SiqueCauldron)) ||
            !card_rules::activationError(s, catalog(), actor, id, word->target).empty())
            return "response_condition: 言灵法术当前不可发动";
        if ((card_rules::hasFormation(s, catalog(), 1 - actor, Capability::Moon)) ||
            (hasTrait(Capability::RequiresStormFormation) && !hasStormFormation(s, catalog(), actor)) ||
            (hasTrait(Capability::RequiresStormSetup) && !hasStormFormation(s, catalog(), actor)))
            return "setup_condition: 不满足阵法或设置条件";
        if (host == s.cards.end() || host->second.owner != actor ||
            !Rules::canHost(s, catalog(), id, word->formation) ||
            Rules::freeRing(s, catalog(), word->formation) < 0 ||
            static_cast<int>(inZone(s, actor, Zone::Casting).size()) >=
                Rules::castingCapacity(s, catalog(), actor))
            return "host_restriction: 没有可用的合法阵法环位";
        const bool detachCost = card_rules::detachCost(def) != 0;
        if (detachCost ? word->target != 0 || !card_rules::validDetach(s, catalog(), actor, def, word->targets)
                       : !validTargets(s, def.target, def.targetCount, actor, word->target, word->targets))
            return "target: 目标或发动材料不合法";
        const bool searchCost = hasTrait(Capability::DiscardSpellSearch);
        const auto discard = s.cards.find(word->discard);
        if (searchCost) {
            if (!word->discard || word->discard == id || discard == s.cards.end() || discard->second.owner != actor ||
                discard->second.zone != Zone::Hand || !spellType(catalog().at(discard->second.definition).type))
                return "extra_cost: 需要弃置一张法术手牌";
            const auto &school = catalog().at(discard->second.definition).school;
            if (std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                    const auto &candidate = entry.second;
                    const auto &candidateDefinition = catalog().at(candidate.definition);
                    return candidate.owner == actor && candidate.zone == Zone::Deck &&
                        spellType(candidateDefinition.type) && candidateDefinition.school == school;
                })) return "extra_cost: 没有可检索的同派系法术";
        } else if (!card_rules::validCost(s, catalog(), actor, id, word->discard))
            return "extra_cost: 发动费用或材料不合法";
        const int fee = Rules::castCost(s, catalog(), id);
        if (!card_rules::canPay(s, actor, fee, &catalog())) return "cost: 魔素不足";
        card_rules::pay(s, actor, fee, &catalog());
        card.analysisLoad += fee;
        ResourceRules::increasedLoad(s, catalog(), actor, fee);
        if (s.result != -1) return {};
        if (detachCost)
            for (auto seal : word->targets) StateMaintenance::leave(s, seal, &catalog());
        card.sourceFormation = word->formation;
        card.sourceRingIndex = Rules::freeRing(s, catalog(), word->formation);
        card.fieldOrder = s.nextFieldOrder++;
        card.settingPaid = fee;
        card.zone = Zone::Casting;
        card.host = 0;
        card.ringIndex = -1;
        card.spell = SpellState::Pending;
        card.targetCard = word->target;
        card.targets = detachCost ? std::vector<CardId>{} : word->targets;
        if (searchCost) {
            StateMaintenance::leave(s, word->discard, &catalog());
            card.targets = {word->discard};
        } else card_rules::payExtraCost(s, catalog(), actor, id, word->discard);
        card.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        card.payments.push_back({s.players[actor].ownTurn, fee, fee, false});
        settingCompletedV253(id, word->formation);
        castingTraitsV2(id);
        if (s.result != -1) return {};
        ChainLink link;
        link.id = s.nextLink++;
        link.kind = LinkKind::Spell;
        link.speed = def.speed;
        link.castCost = fee;
        link.paymentIndex = static_cast<int>(card.payments.size()) - 1;
        link.paid = fee;
        link.addedLoad = fee;
        link.item = snapshot(card, def);
        link.item.requiresSource = true;
        for (const auto &[sealId, seal] : s.cards)
            if (seal.zone == Zone::Attached && seal.host == id && catalog().at(seal.definition).damageBonus) {
                const auto &sealDefinition = catalog().at(seal.definition);
                link.item.effects.push_back({EffectKind::Damage, sealDefinition.damageBonus,
                    hasCapability(sealDefinition, Capability::ThunderDamageBonus) ? DamageType::Thunder : DamageType::Force,
                    EffectRecipient::Opponent});
            }
        s.chain->links.push_back(std::move(link));
        s.chain->hadActualLinks = true;
        s.chain->window = ResponseWindow::Cast;
        s.chain->priority = 1 - actor;
        s.chain->passes = 0;
        s.events.push_back({"word_response", "手牌言灵法术设置、准备并发动", -1, id, fee});
        s.decision.reset();
        return {};
    }

    if (flip) {
        if (!card.faceDown || card.ambushedTurn >= s.globalTurn ||
            !Rules::canHost(s, catalog(), id, card.host)) return "ambush_wait";
        if (!analytic) return activateSpellV252(actor, id, flip->target, flip->targets, flip->discard,
                                                true, token);
        if ((flip->target || !flip->targets.empty() || flip->discard))
            return "target: 解析法术在发动时选择目标";
        if (card_rules::hasFormation(s, catalog(), 1 - actor, Capability::Moon)) return "moon_setup_restriction";
        const int fee = Rules::analysisCost(s, catalog(), id, card.host);
        if (!card_rules::canPay(s, actor, fee, &catalog())) return "cost";
        card_rules::pay(s, actor, fee, &catalog()); card.analysisLoad += fee;
        ResourceRules::increasedLoad(s, catalog(), actor, fee);
        if (s.result != -1) return {};
        card.faceDown = false; card.settingPaid = fee; card.analysisProgress = 0;
        if (hasCapability(def, Capability::GreatStorm)) {
            card.remaining = s.cards.at(card.host).stormPoints;
            s.cards.at(card.host).stormPoints = 0;
        }
        card.analysisStarted = s.players[actor].ownTurn;
        card.spell = def.analysisTurns ? SpellState::Analyzing : SpellState::Ready;
        if (card.spell == SpellState::Ready) analysisCompletedV2(id);
        quickAnalysisV2(id, card.host);
        s.chain->window = ResponseWindow::Action;
    } else {
        if (!analytic) return "not_ready";
        const bool targetsValid = !prepare->target && prepare->targets.empty();
        if (card.faceDown || card.spell != SpellState::Ready || card.canceledTurn == s.players[actor].ownTurn ||
            prepare->discard || !targetsValid)
            return "not_ready";
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog(), actor))
            return "casting_capacity";
        for (const auto &link : s.chain->links)
            if (link.item.source == id) return "response_used";
        card.targetCard = prepare->target; card.targets = prepare->targets;
        ChainLink link; link.id = s.nextLink++; link.kind = LinkKind::Preparation; link.speed = def.speed;
        link.item = snapshot(card, def); link.item.effects.clear(); link.item.requiresSource = true;
        link.item.target = TargetKind::None;
        s.chain->links.push_back(std::move(link)); s.chain->hadActualLinks = true;
        s.chain->window = ResponseWindow::Prepare;
    }
    s.events.push_back({flip ? "flip" : "prepare_declared", flip ? "反转解析法术" : "宣告准备施法", -1, id});
    s.chain->priority = 1 - actor; s.chain->passes = 0;
    s.decision.reset();
    return {};
}
void GameEngine::visitResponseSetups(PlayerId actor, DecisionId token, const std::function<void(const LegalAction &)> &visitor, const ActionIntent *intent) const {
    auto add = [&](std::string name, CardId id, Command command) {
        if (detail::matchesIntent(command, intent) && validateResponseSetup(actor, command).empty())
            visitor({std::move(name), id, 0, std::move(command)});
    };
    for (const auto &[id, card] : state_.cards) {
        if (!detail::intentAllows(intent, ActionField::Source, id) || card.owner != actor || card.zone != Zone::Analysis) continue;
        const auto &definition = catalog().at(card.definition);
        if (card.faceDown && spellType(definition.type)) {
            const auto targetSets = definition.type == CardType::Analytic ? detail::Combinations({}, 0)
                : detail::activationSets(state_, catalog(), actor, definition, intent);
            std::vector<CardId> discards = card_rules::costOptions(state_, catalog(), actor, id);
            if (hasCapability(definition, Capability::DiscardSpellSearch)) {
                discards.clear();
                for (const auto &[discard, candidate] : state_.cards)
                    if (discard != id && candidate.owner == actor && candidate.zone == Zone::Hand &&
                        spellType(catalog().at(candidate.definition).type)) discards.push_back(discard);
            }
            for (const auto &targets : targetSets) for (auto discard : discards) {
                const auto target = card_rules::detachCost(definition) || targets.empty() ? 0 : targets.front();
                add("反转 " + definition.name, id, FlipAmbush{id, target, targets, token, discard});
            }
        }
        else if (card.spell == SpellState::Ready) {
            const std::vector<std::vector<CardId>> targetSets{{}};
            for (const auto &targets : targetSets)
                add("准备施法 " + definition.name, id,
                    PrepareCast{id, targets.empty() ? 0 : targets.front(), 0, targets, token});
        }
    }
    {
        for (const auto &[id, card] : state_.cards) {
            if (!detail::intentAllows(intent, ActionField::Source, id) || card.owner != actor || card.zone != Zone::Hand) continue;
            const auto &definition = catalog().at(card.definition);
            if (definition.type != CardType::Word || definition.responseOnly) continue;
            const auto targetSets = detail::activationSets(state_, catalog(), actor, definition, intent);
            std::vector<CardId> discards{0};
            if (hasCapability(definition, Capability::DiscardSpellSearch)) {
                discards.clear();
                for (const auto &[discard, candidate] : state_.cards)
                    if (discard != id && candidate.owner == actor && candidate.zone == Zone::Hand &&
                        spellType(catalog().at(candidate.definition).type)) discards.push_back(discard);
            } else discards = card_rules::costOptions(state_, catalog(), actor, id);
            for (const auto &[host, formation] : state_.cards) {
                if (!detail::intentAllows(intent, ActionField::Host, host) || formation.owner != actor || formation.zone != Zone::Analysis ||
                    catalog().at(formation.definition).type != CardType::Formation) continue;
                for (const auto &targets : targetSets) for (auto discard : discards) {
                    const auto target = card_rules::detachCost(definition) || targets.empty() ? 0 : targets.front();
                    add("手牌发动言灵 " + definition.name + " → " + catalog().at(formation.definition).name,
                        id, PreloadWord{id, true, host, target, targets, discard, token});
                }
            }
        }
    }
}
std::vector<LegalAction> GameEngine::responseSetupsV2(PlayerId actor, DecisionId token) const {
    std::vector<LegalAction> out;
    visitResponseSetups(actor, token, [&](const LegalAction &action) { out.push_back(action); });
    return out;
}
std::string GameEngine::executeV2(PlayerId actor, const Command &command) {
    if (const auto error = validateAction(actor, command); !error.empty()) return error;
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
    if (it == s.cards.end() || controllerOf(it->second) != actor) return "invalid_card";
    auto &c = it->second;
    const auto &def = catalog().at(c.definition);
    auto &pl = s.players[actor];
    if (def.type != CardType::Formation &&
        (std::holds_alternative<StartAnalysis>(command) || std::holds_alternative<SetAmbush>(command) ||
         std::holds_alternative<FlipAmbush>(command) || std::holds_alternative<PreloadWord>(command)) &&
        hasCapability(def, Capability::Unique))
        for (const auto &[id, other] : s.cards)
            if (id != cardId && other.owner == actor && other.definition == c.definition &&
                (other.zone == Zone::Analysis || other.zone == Zone::Casting))
                return "unique: 己方场上已存在同名唯一卡牌";
    const auto activationAllowed = [&] {
        return card_rules::activationAllowed(s, catalog(), actor, def) &&
            (!hasCapability(def, Capability::SiqueAromaLure) ||
             card_rules::hasFormation(s, catalog(), actor, Capability::SiqueCauldron)) &&
            card_rules::activationError(s, catalog(), actor, cardId, 0).empty();
    };
    const auto activationTargets = [&](CardId target, const std::vector<CardId> &ids) {
        return card_rules::detachCost(def) ? target == 0 && card_rules::validDetach(s, catalog(), actor, def, ids)
            : validTargets(s, def.target, def.targetCount, actor, target, ids);
    };
    const auto detach = [&](const std::vector<CardId> &ids) {
        if (card_rules::detachCost(def)) for (auto id : ids) StateMaintenance::leave(s, id, &catalog());
    };
    const bool requiresStormFormation = hasCapability(def, Capability::RequiresStormFormation);
    const bool requiresStormForSetup = hasCapability(def, Capability::RequiresStormSetup);
    const bool requiresStormHeartSetup = hasCapability(def, Capability::RequiresStormHeartSetup);
    auto ownedFormation = [&](CardId id) {
        auto host = s.cards.find(id);
        return host != s.cards.end() && host->second.owner == actor && host->second.zone == Zone::Analysis &&
            catalog().at(host->second.definition).type == CardType::Formation;
    };
    auto pay = [&](int mana, int load) {
        if (!card_rules::canPay(s, actor, mana, &catalog())) return false;
        card_rules::pay(s, actor, mana, &catalog());
        StateMaintenance::check(s, catalog());
        if (s.result != -1) return true;
        if (load > 0) { c.analysisLoad += load; ResourceRules::increasedLoad(s, catalog(), actor, load); }
        s.events.push_back({"pay", "支付魔素", -1, cardId, mana});
        return true;
    };
    auto ring = [&](CardId host, bool faceDown = false) {
        return (!requiresStormHeartSetup ||
            (s.cards.count(host) && hasCapability(catalog().at(s.cards.at(host).definition), Capability::StormHeart))) &&
            Rules::canHost(s, catalog(), cardId, host, faceDown) &&
            Rules::freeRing(s, catalog(), host) >= 0;
    };
    auto place = [&](CardId host) {
        c.zone = Zone::Analysis; c.host = host; c.sourceFormation = host;
        c.ringIndex = Rules::freeRing(s, catalog(), host);
        c.sourceRingIndex = c.ringIndex;
        c.wasAmbushed = false;
        c.fieldOrder = s.nextFieldOrder++;
    };
    auto phaseSpeed = [&] {
        return spellSpeedAllowed(s, def);
    };
    auto setupSpeed = [&] {
        if (card_rules::hasFormation(s, catalog(), 1 - actor, Capability::Moon)) return false;
        return spellSpeedAllowed(s, def, true);
    };
    const bool discardSpellSearch = hasCapability(def, Capability::DiscardSpellSearch);
    auto validSpellCost = [&](CardId material) {
        if (!discardSpellSearch) return card_rules::validCost(s, catalog(), actor, cardId, material);
        const auto found = s.cards.find(material);
        if (!material || material == cardId || found == s.cards.end() || found->second.owner != actor ||
            found->second.zone != Zone::Hand || !spellType(catalog().at(found->second.definition).type)) return false;
        const auto &school = catalog().at(found->second.definition).school;
        return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &item) {
            return item.second.owner == actor && item.second.zone == Zone::Deck &&
                spellType(catalog().at(item.second.definition).type) && catalog().at(item.second.definition).school == school;
        });
    };
    auto validRecovery = [&](CardId target) {
        if (def.target != TargetKind::EnemyAshCard) return true;
        const auto selected = s.cards.find(target);
        if (selected == s.cards.end() || catalog().at(selected->second.definition).type != CardType::Analytic ||
            catalog().at(selected->second.definition).rank > 5) return false;
        return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &item) {
            const auto &host = item.second;
            return host.owner == actor && host.zone == Zone::Analysis && !host.faceDown &&
                catalog().at(host.definition).type == CardType::Formation &&
                catalog().at(selected->second.definition).rank <= Rules::maxRank(s, catalog(), item.first) &&
                Rules::freeRing(s, catalog(), item.first) >= 0;
        });
    };
    auto paySpellCost = [&](CardId material) {
        if (discardSpellSearch) {
            if (material) { StateMaintenance::leave(s, material, &catalog()); c.targets = {material}; }
        } else card_rules::payExtraCost(s, catalog(), actor, cardId, material);
    };
    if (const auto *cmd = std::get_if<ActivateAbility>(&command)) {
        if (auto result = sevenAbility(actor, *cmd)) return *result;
        if (!activationAllowed()) return "sique_activation_restriction";
        if (s.phase != Phase::Main || s.phaseStep != PhaseStep::Body || actor != s.active || s.chain || s.effect)
            return "ability_unavailable";
        const auto hasTrait = [&](Capability capability) {
            return hasCapability(def, capability);
        };
        if (cmd->ability == "storm_search" || cmd->ability == "storm_heart_choice") {
            const bool isSearch = cmd->ability == "storm_search" && hasCapability(def, Capability::StormCrest) &&
                hasTrait(Capability::ActivateStormSearch);
            const bool isHeart = cmd->ability == "storm_heart_choice" && hasCapability(def, Capability::StormHeart) &&
                hasTrait(Capability::ActivateStormHeartChoice);
            if (!(isSearch || isHeart) || c.zone != Zone::Analysis || def.type != CardType::Formation ||
                c.stormPoints < 2 || c.stormAbilityTurn == pl.ownTurn)
                return "ability_unavailable";
            std::vector<ChoiceValue> options;
            if (isSearch) {
                for (auto id : pl.deck) {
                    const auto &candidate = s.cards.at(id);
                    const auto &candidateDefinition = catalog().at(candidate.definition);
                    const auto &tags = candidateDefinition.tags;
                    const bool spell = candidateDefinition.type == CardType::Analytic ||
                        candidateDefinition.type == CardType::Word || candidateDefinition.type == CardType::Talisman;
                    if (candidate.zone == Zone::Deck && spell &&
                        detail::stormTags(tags))
                        options.push_back(ChoiceValue::card(id));
                }
                if (options.empty()) return "ability_target";
            } else {
                options.push_back(ChoiceValue::mode(ChoiceMode::HeartDraw));
                const bool recoverable = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                    const auto &candidate = entry.second;
                    const auto &candidateDefinition = catalog().at(candidate.definition);
                    return candidate.owner == actor && candidate.zone == Zone::Ash &&

                        detail::stormTags(candidateDefinition.tags);
                });
                if (recoverable) options.push_back(ChoiceValue::mode(ChoiceMode::HeartRecover));
            }
            c.stormPoints -= 2;
            c.stormAbilityTurn = pl.ownTurn;
            typedDecision(actor, isSearch ? DecisionKind::StormSearch : DecisionKind::StormHeartChoice,
                          std::move(options));
            return {};
        }
        if (cmd->ability != "storm_annihilation_destroy" || cmd->target == 0 || c.zone != Zone::Attached ||
            def.type != CardType::Seal || !hasTrait(Capability::ActivateStormCrestDestroy))
            return "ability_unavailable";
        const auto host = s.cards.find(c.host);
        if (host == s.cards.end() || host->second.owner != actor || host->second.zone != Zone::Analysis ||
            catalog().at(host->second.definition).type != CardType::Formation || host->second.stormPoints < 2 ||
            !detail::stormTags(catalog().at(host->second.definition).tags))
            return "ability_cost";
        const auto target = s.cards.find(cmd->target);
        if (target == s.cards.end() || target->second.owner == actor || target->second.faceDown ||
            target->second.zone != Zone::Analysis || target->second.spell != SpellState::None ||
            catalog().at(target->second.definition).type != CardType::Formation ||
            Rules::occupied(s, cmd->target) != 0)
            return "ability_target";
        const auto &targetDefinition = catalog().at(target->second.definition);
        if (targetDefinition.opponentDestroyProtected ||
            hasCapability(targetDefinition, Capability::EffectImmune) ||
            hasCapability(targetDefinition, Capability::EffectIndestructible))
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
        if (hasCapability(def, Capability::SevenStarsEffectOnly))
            return "effect_only_formation";
        int lifeFee = 0;
        for (const auto &part : def.recipe) if (part.kind == RecipeCostKind::Life) lifeFee += part.amount;
        if (!card_rules::canPayLife(s, actor, lifeFee)) return "life_cost";
        if (hasCapability(def, Capability::SiqueCauldron) &&
            !card_rules::hasFormation(s, catalog(), actor, Capability::SiqueCampfire))
            return "sique_requires_campfire";
        if (s.phase != Phase::Main || c.zone != Zone::SideDeck || def.type != CardType::Formation || pl.formations)
            return "formation_limit";
        if (cmd->replace && (!ownedFormation(cmd->replace) || cmd->replace == cardId)) return "invalid_replacement";
        if (!recipeMatches(s, catalog(), actor, def, cmd->materials)) return "recipe_materials";
        if (hasCapability(def, Capability::MaxTwo)) {
            int count = 0;
            for (const auto &[id, other] : s.cards)
                if (controllerOf(other) == actor && other.zone == Zone::Analysis && other.definition == c.definition &&
                    id != cmd->replace && std::find(cmd->materials.begin(), cmd->materials.end(), id) == cmd->materials.end()) ++count;
            if (count >= 2) return "formation_copy_limit: 此阵法最多存在两张";
        }
        if (hasCapability(def, Capability::Unique))
            for (const auto &[id, other] : s.cards)
                if (controllerOf(other) == actor && other.zone == Zone::Analysis && other.definition == c.definition &&
                    id != cmd->replace && std::find(cmd->materials.begin(), cmd->materials.end(), id) == cmd->materials.end())
                    return "unique_formation";
        if (std::find(cmd->materials.begin(), cmd->materials.end(), cmd->replace) != cmd->materials.end())
            return "recipe_materials";
        int fee = def.cost;
        for (const auto &part : def.recipe)
            if (part.kind == RecipeCostKind::Mana) fee += part.amount;
        if (!card_rules::canPay(s, actor, fee, &catalog())) return "cost: 魔素不足";
        int body = usedBody(s, catalog(), actor);
        if (cmd->replace) body -= catalog().at(s.cards.at(cmd->replace).definition).body;
        for (auto id : cmd->materials)
            if (catalog().at(s.cards.at(id).definition).type == CardType::Formation)
                body -= catalog().at(s.cards.at(id).definition).body;
        if (body + def.body > 5) return "formation_capacity";
        if (hasCapability(def, Capability::StormHeartUpgrade)) {
            const auto crest = std::find_if(cmd->materials.begin(), cmd->materials.end(), [&](CardId material) {
                return hasCapability(catalog().at(s.cards.at(material).definition), Capability::StormCrest);
            });
            if (crest == cmd->materials.end() || s.cards.at(*crest).stormPoints < 10)
                return "storm_points";
        }
        card_rules::pay(s, actor, fee, &catalog());
        StateMaintenance::check(s, catalog());
        if (s.result != -1) return {};
        for (auto id : cmd->materials) StateMaintenance::leave(s, id, &catalog());
        if (cmd->replace) StateMaintenance::leave(s, cmd->replace, &catalog());
        c.zone = Zone::Analysis; c.fieldOrder = s.nextFieldOrder++;
        c.settingPaid = fee; ++pl.formations;
        {
            card_rules::payLife(s, catalog(), actor, lifeFee, cardId);
            if (hasCapability(def, Capability::BloodPool) || hasCapability(def, Capability::BloodHeart))
                card_rules::changeLifeLimit(s, catalog(), actor, -5);
            card_rules::queueFormationSetup(s, catalog(), cardId);
        }
        s.events.push_back({"formation_set", "设置阵法", -1, cardId});
        if (hasCapability(def, Capability::ReturnFormationFromAsh)) {
            auto trigger = snapshot(c, def); trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
            trigger.effects = {{EffectKind::RecoverFormation, 1}}; s.queue.append(std::move(trigger));
        }
        if (hasCapability(def, Capability::SearchGreatStormOnSetup)) {
            const auto found = std::find_if(pl.deck.begin(), pl.deck.end(), [&](CardId candidate) {
                return s.cards.at(candidate).zone == Zone::Deck &&
                    hasCapability(catalog().at(s.cards.at(candidate).definition), Capability::GreatStorm);
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

        if (s.phase != Phase::Main || c.zone != Zone::Analysis || def.type != CardType::Formation || pl.formations)
            return "invalid_removal";
        StateMaintenance::leave(s, cardId, &catalog()); ++pl.formations; actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<StartAnalysis>(&command)) {
        if ((cmd->target || !cmd->targets.empty()))
            return "target: 解析法术在发动时选择目标";
        if (def.type == CardType::Analytic && greatStormLocksAnalysis(s, catalog(), actor))
            return "great_storm_lock: 大风暴术阻止设置其他解析法术";
        if (requiresStormForSetup && !hasStormFormation(s, catalog(), actor))
            return "storm_formation: 需要己方风暴阵法";
        if (!setupSpeed() || c.zone != Zone::Hand || def.type != CardType::Analytic || !ring(cmd->formation))
            return "ring_or_rank";
        int fee = Rules::analysisCost(s, catalog(), cardId, cmd->formation);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        place(cmd->formation); c.settingPaid = fee; c.analysisProgress = 0;
        if (hasCapability(def, Capability::GreatStorm)) {
            c.remaining = s.cards.at(cmd->formation).stormPoints;
            s.cards.at(cmd->formation).stormPoints = 0;
        }
        c.spell = def.analysisTurns <= 0 ? SpellState::Ready : SpellState::Analyzing;
        c.analysisStarted = pl.ownTurn; c.targetCard = cmd->target; c.targets = cmd->targets;
        if (c.spell == SpellState::Ready) analysisCompletedV2(cardId);
        quickAnalysisV2(cardId, cmd->formation);
        settingCompletedV253(cardId, cmd->formation);
        {
            ChainLink setup;
            setup.kind = LinkKind::Setup;
            setup.item = snapshot(c, def);
            setup.item.effects.clear();
            startChain(ResponseWindow::Prepare, std::move(setup), 1 - actor);
        }
        return {};
    }
    if (const auto *cmd = std::get_if<SetAmbush>(&command)) {
        if (def.type == CardType::Analytic && greatStormLocksAnalysis(s, catalog(), actor))
            return "great_storm_lock: 大风暴术阻止设置其他解析法术";
        if (requiresStormForSetup && !hasStormFormation(s, catalog(), actor))
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
        if (def.type != CardType::Analytic)
            return activateSpellV252(actor, cardId, cmd->target, cmd->targets, cmd->discard, true);
        if (def.type != CardType::Analytic && !activationAllowed()) return "sique_activation_restriction";
        if ((requiresStormFormation || requiresStormForSetup) && !hasStormFormation(s, catalog(), actor))
            return "storm_formation: 需要己方风暴阵法";
        if (cmd->decision || !c.faceDown || c.zone != Zone::Analysis || c.ambushedTurn >= s.globalTurn ||
            (actor != s.active) || !setupSpeed()) return "ambush_wait";
        if (!Rules::canHost(s, catalog(), cardId, c.host) ||
            (requiresStormHeartSetup && !hasCapability(catalog().at(s.cards.at(c.host).definition), Capability::StormHeart)))
            return "host_restriction";
        if (def.type == CardType::Analytic && def.speed == 1 && s.phase != Phase::Main)
            return "spell_speed";
        if (def.type == CardType::Analytic) {
            if ((cmd->target || !cmd->targets.empty() || cmd->discard))
                return "target: 解析法术在发动时选择目标";
            const int fee = Rules::analysisCost(s, catalog(), cardId, c.host);
            if (!pay(fee, fee)) return "cost: 魔素不足";
            if (s.result != -1) return {};
            c.faceDown = false; c.settingPaid = fee;
            if (hasCapability(def, Capability::GreatStorm)) {
                c.remaining = s.cards.at(c.host).stormPoints;
                s.cards.at(c.host).stormPoints = 0;
            }
            c.analysisProgress = 0; c.analysisStarted = pl.ownTurn;
            c.spell = def.analysisTurns <= 0 ? SpellState::Ready : SpellState::Analyzing;
            if (c.spell == SpellState::Ready) analysisCompletedV2(cardId);
            quickAnalysisV2(cardId, c.host);
            actionTimingV2(); return {};
        }
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog(), actor))
            return "casting_capacity";
        if (!activationAllowed() || def.responseOnly || !activationTargets(cmd->target, cmd->targets) || !validSpellCost(cmd->discard) || !validRecovery(cmd->target))
            return "target_or_response_window";
        const int fee = Rules::castCost(s, catalog(), cardId);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        detach(cmd->targets);
        c.settingPaid = fee; c.faceDown = false; c.zone = Zone::Casting; c.host = 0; c.ringIndex = -1;
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = card_rules::detachCost(def) ? std::vector<CardId>{} : cmd->targets;
        paySpellCost(cmd->discard);
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<PreloadWord>(&command)) {
        if (card_rules::hasFormation(s, catalog(), 1 - actor, Capability::Moon)) return "moon_setup_restriction";
        if (!activationAllowed()) return "sique_activation_restriction";
        if (requiresStormFormation && !hasStormFormation(s, catalog(), actor))
            return "storm_formation: 需要己方风暴阵法";
        if (pl.blockedActionTurn == pl.ownTurn) return "blocked_incantation";
        if (!setupSpeed() || c.zone != Zone::Hand || def.type != CardType::Word || def.responseOnly ||
            !ring(cmd->formation) ||
            static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog(), actor))
            return "word_setup";
        if (!activationTargets(cmd->target, cmd->targets) || !validSpellCost(cmd->discard)) return "target_or_cost";
        const int fee = Rules::castCost(s, catalog(), cardId);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        detach(cmd->targets);
        place(cmd->formation); c.settingPaid = fee;
        settingCompletedV253(cardId, cmd->formation);
        c.spell = SpellState::Ready;
        c.zone = Zone::Casting; c.host = 0; c.ringIndex = -1;
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = card_rules::detachCost(def) ? std::vector<CardId>{} : cmd->targets;
        paySpellCost(cmd->discard);
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        analysisCompletedV2(cardId); castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<PrepareCast>(&command)) {
        {
            if (cmd->target || !cmd->targets.empty()) return "target: 解析法术在发动时选择目标";
        }
        return prepare(actor, *cmd, false);
    }
    if (const auto *cmd = std::get_if<ActivateSpell>(&command))
        return activateSpellV252(actor, cardId, cmd->target, cmd->targets, cmd->discard, false);
    if (const auto *cmd = std::get_if<AttachSeal>(&command)) {
        if (!card_rules::activationAllowed(s, catalog(), actor, def))
            return "sique_attachment_restriction: 魔力坩埚存在时只能附着味契卡";
        auto host = s.cards.find(cmd->host);
        if (s.phase != Phase::Main || c.zone != Zone::Hand || def.type != CardType::Seal ||
            host == s.cards.end() || host->second.owner != actor || host->second.faceDown ||
            (host->second.zone != Zone::Analysis && host->second.zone != Zone::Casting)) return "seal_host";

        const auto &hostDefinition = catalog().at(host->second.definition);
        if (hostDefinition.type != CardType::Formation && !spellType(hostDefinition.type))
            return "seal_host";
        if (hasCapability(def, Capability::SiqueCauldronOnly) &&
            !hasCapability(hostDefinition, Capability::SiqueCauldron)) return "seal_host";
        const auto hasTrait = [&](Capability capability) {
            return hasCapability(def, capability);
        };
        const bool stormFormation = hostDefinition.type == CardType::Formation &&
            detail::stormTags(hostDefinition.tags);
        if (hasTrait(Capability::StormFormationHostRestriction) &&
            !detail::stormTags(hostDefinition.tags))
            return "seal_host: 湮灭之纹章只能附着于恒风卡";
        if ((hasTrait(Capability::StormFormationHostOnly) && !stormFormation) ||
            (hasTrait(Capability::StormFormationHostRestriction) &&
             hostDefinition.type == CardType::Formation && !stormFormation) ||
            (hasTrait(Capability::StormFormationHostRestriction) &&
             hostDefinition.type != CardType::Formation && hostDefinition.type != CardType::Analytic &&
             hostDefinition.type != CardType::Word && hostDefinition.type != CardType::Talisman))
            return "seal_host";
        if (hasCapability(def, Capability::SiqueRequiresCauldron) &&
            !card_rules::hasFormation(s, catalog(), actor, Capability::SiqueCauldron)) return "sique_requires_cauldron";
        const int lifeCost = card_rules::attachmentLifeCost(def);
        if (lifeCost && !card_rules::canPayLife(s, actor, lifeCost)) return "life_cost";
        if (!pay(def.cost, 0)) return "cost: 魔素不足";
        if (lifeCost) card_rules::payLife(s, catalog(), actor, lifeCost, cardId);
        c.zone = Zone::Attached; c.host = cmd->host;
        card_rules::normalizeLife(s, catalog(), actor);
        card_rules::attached(s, catalog(), cardId);
        actionTimingV2(); return {};
    }
    if (std::holds_alternative<RemoveSeal>(command)) return "seal_limit";
    if (const auto *cmd = std::get_if<Abandon>(&command)) {
        if (!spellType(def.type) || (c.zone != Zone::Analysis && c.zone != Zone::Casting) ||
            (c.zone == Zone::Casting ? s.phase != Phase::Cast : s.phase != Phase::Main)) return "invalid_abandon";
        StateMaintenance::leave(s, cmd->card, &catalog());
        actionTimingV2(); return {};
    }
    return "unsupported_v2_command";
}
}
