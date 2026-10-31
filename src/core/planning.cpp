#include "engine_internal.hpp"
#include "wizard/core/card_rules.hpp"
#include <set>
#include <type_traits>
namespace wizard {
using namespace detail;
namespace {
bool spellType(CardType type) { return type == CardType::Analytic || type == CardType::Word || type == CardType::Talisman; }
bool affordable(const GameState &s, const CardCatalog &cat, PlayerId p, int fee) {
    return !card_rules::paymentChoices(s, cat, p, fee).empty();
}
}
std::string GameEngine::validateActivation(PlayerId actor, CardId id, CardId target,
    const std::vector<CardId> &targets, CardId discard, bool flip, DecisionId token) const {
    const auto &s = state_;
    const bool responding = token != 0;
    if (responding) {
        if (!s.chain || s.chain->mode != ChainMode::Building || s.chain->priority != actor)
            return "priority: 当前没有响应优先权";
        if (s.decision && s.decision->id != token) return "stale_decision";
    } else if (s.chain || s.decision || s.effect || actor != s.active || s.phaseStep != PhaseStep::Body)
        return "priority: 当前没有行动权";
    const auto found = s.cards.find(id);
    if (found == s.cards.end() || controllerOf(found->second) != actor) return "invalid_card";
    const auto &card = found->second;
    const auto &def = catalog().at(card.definition);
    if (!spellType(def.type)) return "invalid_spell";
    if (auto error = card_rules::activationError(s, catalog(), actor, id, target); !error.empty()) return error;
    if (!spellSpeedAllowed(s, def, flip)) return "spell_speed: 咒文速度不允许当前时点发动";
    if (def.responseOnly || (def.effects.empty() && !def.responses.empty()))
        return "response_condition: 仅可在卡牌明示的事件发生时发动";
    if (!card_rules::activationAllowed(s, catalog(), actor, def) ||
        (hasCapability(def, Capability::SiqueAromaLure) &&
         !card_rules::hasFormation(s, catalog(), actor, Capability::SiqueCauldron)))
        return "sique_activation_restriction";
    const auto hasTrait = [&](Capability capability) {
        return hasCapability(def, capability);
    };
    if ((hasTrait(Capability::RequiresStormFormation) || (flip && hasTrait(Capability::RequiresStormSetup))) &&
        !hasStormFormation(s, catalog(), actor)) return "storm_formation: 需要己方风暴阵法";
    if (flip) {
        if (def.type == CardType::Analytic) return "not_ready: 解析法术须先完成解析并准备施法";
        if (!card.faceDown || card.zone != Zone::Analysis || card.ambushedTurn >= s.globalTurn)
            return "ambush_wait: 盖伏等待期限尚未满足";
        if (!Rules::canHost(s, catalog(), id, card.host) ||
            (hasTrait(Capability::RequiresStormHeartSetup) &&
             !hasCapability(catalog().at(s.cards.at(card.host).definition), Capability::StormHeart))) return "host_restriction";
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog(), actor))
            return "casting_capacity";
    } else if (card.faceDown || card.zone != Zone::Casting || card.spell != SpellState::Ready)
        return "not_ready: 法术须在施法区准备完成";
    const bool detach = card_rules::detachCost(def);
    if (detach ? target != 0 || !card_rules::validDetach(s, catalog(), actor, def, targets)
               : !validTargets(s, def.target, def.targetCount, actor, target, targets)) return "target";
    if (hasTrait(Capability::DiscardSpellSearch)) {
        const auto material = s.cards.find(discard);
        if (!discard || discard == id || material == s.cards.end() || material->second.owner != actor ||
            material->second.zone != Zone::Hand || !spellType(catalog().at(material->second.definition).type))
            return "extra_cost";
        const auto &school = catalog().at(material->second.definition).school;
        if (std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                return entry.second.owner == actor && entry.second.zone == Zone::Deck &&
                    spellType(catalog().at(entry.second.definition).type) &&
                    catalog().at(entry.second.definition).school == school;
            })) return "extra_cost";
    } else if (!card_rules::validCost(s, catalog(), actor, id, discard)) return "extra_cost";
    if (def.target == TargetKind::EnemyAshCard) {
        const auto recovered = s.cards.find(target);
        if (recovered == s.cards.end() || catalog().at(recovered->second.definition).type != CardType::Analytic ||
            catalog().at(recovered->second.definition).rank > 5 ||
            std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                const auto &host = entry.second;
                return host.owner == actor && host.zone == Zone::Analysis && !host.faceDown &&
                    catalog().at(host.definition).type == CardType::Formation &&
                    catalog().at(recovered->second.definition).rank <= Rules::maxRank(s, catalog(), entry.first) &&
                    Rules::freeRing(s, catalog(), entry.first) >= 0;
            })) return "target";
    }
    CardId stormCostFormation = 0;
    if (std::any_of(def.effects.begin(), def.effects.end(), [](const Effect &effect) {
            return effect.kind == EffectKind::StormCyclone;
        })) {
        for (const auto &[formationId, formation] : s.cards) {
            const auto &formationDefinition = catalog().at(formation.definition);
            if (formation.owner == actor && formation.zone == Zone::Analysis && formation.stormPoints >= 5 &&
                formationDefinition.type == CardType::Formation &&
                detail::stormTags(formationDefinition.tags)) { stormCostFormation = formationId; break; }
        }
        if (!stormCostFormation) return "storm_points: 风暴点数不足";
    }
    const int fee = Rules::castCost(s, catalog(), id);
    if (!affordable(s, catalog(), actor, fee)) return "cost: 魔素不足";
    return {};
}
std::string GameEngine::validatePreparation(PlayerId actor, const PrepareCast &cmd, bool deferred) const {
    const auto &s = state_;
    const auto i = s.cards.find(cmd.card);
    if (i == s.cards.end() || controllerOf(i->second) != actor) return "invalid_card";
    const auto &c = i->second; const auto &def = catalog().at(c.definition);
    const auto &pl = s.players[actor];
    for (const auto &[id, cloud] : s.cards)
        if ((cloud.zone == Zone::Analysis || cloud.zone == Zone::Casting) && cloud.spell == SpellState::Active && cloud.remaining > 0 &&
            hasCapability(catalog().at(cloud.definition), Capability::CloudMist) &&
            std::find(cloud.targets.begin(), cloud.targets.end(), cmd.card) != cloud.targets.end()) return "preparation_locked";
    if (def.type != CardType::Analytic || c.faceDown || cmd.discard || c.zone != Zone::Analysis || c.spell != SpellState::Ready || c.canceledTurn == pl.ownTurn) return "not_ready";
    if (!Rules::canHost(s, catalog(), cmd.card, c.host)) return "host_restriction";
    if (cmd.target || !cmd.targets.empty()) return "target";
    if (!deferred && ((actor != s.active && (!c.wasAmbushed || c.ambushedTurn >= s.globalTurn || def.speed < 2)) ||
        (def.speed == 1 && s.phase != Phase::Main) || (def.speed == 2 && s.phase != Phase::Main && s.phase != Phase::Cast))) return "spell_speed";
    if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog(), actor)) return "casting_capacity";
    if (!deferred && cmd.decision) return "stale_decision";
    for (const auto &pending : s.deferredPreparations) if (pending.item.source == cmd.card) return "not_ready";
    if (s.chain) for (const auto &link : s.chain->links) if (link.kind == LinkKind::Preparation && link.item.source == cmd.card) return "not_ready";
    return {};
}
std::string GameEngine::validateResponseSetup(PlayerId actor, const Command &command) const {
    const auto &s = state_;
    if (!s.chain || s.chain->mode != ChainMode::Building || s.chain->priority != actor)
        return "priority";
    const auto *flip = std::get_if<FlipAmbush>(&command);
    const auto *prepare = std::get_if<PrepareCast>(&command);
    const auto *word = std::get_if<PreloadWord>(&command);
    const auto id = flip ? flip->card : prepare ? prepare->card : word->card;
    const auto token = flip ? flip->decision : prepare ? prepare->decision : word->decision;
    if (s.decision && token != s.decision->id) return "stale_decision";
    const auto found = s.cards.find(id);
    if (found == s.cards.end() || controllerOf(found->second) != actor) return "invalid_card";
    const auto &card = found->second; const auto &def = catalog().at(card.definition);
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
        if (!affordable(s, catalog(), actor, fee)) return "cost: 魔素不足";
        return {};
    }

    if (flip) {
        if (!card.faceDown || card.ambushedTurn >= s.globalTurn ||
            !Rules::canHost(s, catalog(), id, card.host)) return "ambush_wait";
        if (!analytic) return validateActivation(actor, id, flip->target, flip->targets, flip->discard,
                                                true, token);
        if ((flip->target || !flip->targets.empty() || flip->discard))
            return "target: 解析法术在发动时选择目标";
        if (card_rules::hasFormation(s, catalog(), 1 - actor, Capability::Moon)) return "moon_setup_restriction";
        const int fee = Rules::analysisCost(s, catalog(), id, card.host);
        if (!affordable(s, catalog(), actor, fee)) return "cost";
        return {};
    }
        if (!analytic) return "not_ready";
        const bool targetsValid = !prepare->target && prepare->targets.empty();
        if (card.faceDown || card.spell != SpellState::Ready || card.canceledTurn == s.players[actor].ownTurn ||
            prepare->discard || !targetsValid)
            return "not_ready";
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog(), actor))
            return "casting_capacity";
        for (const auto &link : s.chain->links)
            if (link.item.source == id) return "response_used";
    return {};
}

namespace {
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
std::string GameEngine::validateAction(PlayerId actor, const Command &command) const {
    const auto &s = state_;
    if (std::holds_alternative<Choose>(command) || std::holds_alternative<Respond>(command) ||
        std::holds_alternative<PassResponse>(command)) return "stale_decision";
    if (s.decision || s.chain || s.effect || !s.queue.items.empty()) return "pending: 先处理待决事项";
    if (actor != s.active || s.phaseStep != PhaseStep::Body) return "priority: 当前没有行动权";
    if (const auto *advance = std::get_if<AdvancePhase>(&command)) {
        if (!s.phaseGate || advance->gate != s.phaseGate) return "phase_gate";
        if (advance->skipCast && s.phase != Phase::Main) return "wrong_phase";
        return {};
    }
    if (std::holds_alternative<Advance>(command)) return s.phase == Phase::Main && s.phaseGate ? "" : "phase_gate";
    auto cardId = std::visit([](const auto &item) -> CardId {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, Choose> || std::is_same_v<T, Advance> ||
                      std::is_same_v<T, AdvancePhase> || std::is_same_v<T, PassResponse> || std::is_same_v<T, Surrender> ||
                      std::is_same_v<T, Timeout>) return 0;
        else return item.card;
    }, command);
    auto it = s.cards.find(cardId);
    if (it == s.cards.end() || controllerOf(it->second) != actor) return "invalid_card";
    const auto &c = it->second;
    const auto &def = catalog().at(c.definition);
    const auto &pl = s.players[actor];
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
    const bool requiresStormFormation = hasCapability(def, Capability::RequiresStormFormation);
    const bool requiresStormForSetup = hasCapability(def, Capability::RequiresStormSetup);
    const bool requiresStormHeartSetup = hasCapability(def, Capability::RequiresStormHeartSetup);
    auto ownedFormation = [&](CardId id) {
        auto host = s.cards.find(id);
        return host != s.cards.end() && host->second.owner == actor && host->second.zone == Zone::Analysis &&
            catalog().at(host->second.definition).type == CardType::Formation;
    };
    auto pay = [&](int mana, int) { return affordable(s, catalog(), actor, mana); };
    auto ring = [&](CardId host, bool faceDown = false) {
        return (!requiresStormHeartSetup ||
                (s.cards.count(host) && hasCapability(catalog().at(s.cards.at(host).definition), Capability::StormHeart))) &&
            Rules::canHost(s, catalog(), cardId, host, faceDown) &&
            Rules::freeRing(s, catalog(), host) >= 0;
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
    if (std::holds_alternative<PlayAction>(command)) return "unsupported_command";
    if (const auto *cmd = std::get_if<ActivateAbility>(&command)) {
        if (cmd->ability == "seven_kaiyang") return "seven_kaiyang_prepare_only";
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
            std::vector<CardId> options;
            if (isSearch) {
                for (auto id : pl.deck) {
                    const auto &candidate = s.cards.at(id);
                    const auto &candidateDefinition = catalog().at(candidate.definition);
                    const auto &tags = candidateDefinition.tags;
                    const bool spell = candidateDefinition.type == CardType::Analytic ||
                        candidateDefinition.type == CardType::Word || candidateDefinition.type == CardType::Talisman;
                    if (candidate.zone == Zone::Deck && spell &&
                        detail::stormTags(tags))
                        options.push_back(id);
                }
                if (options.empty()) return "ability_target";
            } else {
                options.push_back(1); // Draw one card.
                const bool recoverable = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                    const auto &candidate = entry.second;
                    const auto &candidateDefinition = catalog().at(candidate.definition);
                    return candidate.owner == actor && candidate.zone == Zone::Ash &&

                        detail::stormTags(candidateDefinition.tags);
                });
                if (recoverable) options.push_back(2); // Recover a Storm card from Ash.
            }
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
        if (!affordable(s, catalog(), actor, fee)) return "cost: 魔素不足";
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
        return {};
    }
    if (const auto *cmd = std::get_if<RemoveFormation>(&command)) {

        if (s.phase != Phase::Main || c.zone != Zone::Analysis || def.type != CardType::Formation || pl.formations)
            return "invalid_removal";
        return {};
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
        return {};
    }
    if (const auto *cmd = std::get_if<SetAmbush>(&command)) {
        if (def.type == CardType::Analytic && greatStormLocksAnalysis(s, catalog(), actor))
            return "great_storm_lock: 大风暴术阻止设置其他解析法术";
        if (requiresStormForSetup && !hasStormFormation(s, catalog(), actor))
            return "storm_formation: 需要己方风暴阵法";
        if (s.phase != Phase::Main || c.zone != Zone::Hand || !spellType(def.type) || !ring(cmd->formation, true))
            return "invalid_ambush";
        return {};
    }
    if (const auto *cmd = std::get_if<FlipAmbush>(&command)) {
        if (def.type != CardType::Analytic)
            return validateActivation(actor, cardId, cmd->target, cmd->targets, cmd->discard, true, 0);
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
            return {};
        }
        return "invalid_spell";
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
        return {};
    }
    if (const auto *cmd = std::get_if<PrepareCast>(&command)) return validatePreparation(actor, *cmd, false);
    if (const auto *cmd = std::get_if<ActivateSpell>(&command)) return validateActivation(actor, cardId, cmd->target, cmd->targets, cmd->discard, false, 0);
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
        return {};
    }
    if (std::holds_alternative<RemoveSeal>(command)) return "seal_limit";
    if (const auto *cmd = std::get_if<Abandon>(&command)) {
        if (!spellType(def.type) || (c.zone != Zone::Analysis && c.zone != Zone::Casting) ||
            (c.zone == Zone::Casting ? s.phase != Phase::Cast : s.phase != Phase::Main)) return "invalid_abandon";
        return {};
    }
    return "unsupported_command";
}

std::string GameEngine::validate(PlayerId actor, const Command &input) const {
    const auto command = normalizeCommand(input);
    const auto &s = state_;
    if (actor < 0 || actor > 1) return "invalid_player";
    if (s.result != -1) return "game_over";
    if (std::holds_alternative<Timeout>(command) || std::holds_alternative<Surrender>(command)) return {};
    if (s.opening) {
        const auto *choice = std::get_if<SelectInitialFormation>(&command);
        if (!choice) return "opening_required";
        if (s.initialFormations[actor]) return "initial_confirmed";
        const auto found = s.cards.find(choice->card);
        if (found == s.cards.end() || controllerOf(found->second) != actor || found->second.zone != Zone::SideDeck) return "invalid_initial";
        const auto &def = catalog().at(found->second.definition);
        return def.type == CardType::Formation && def.level == 1 ? "" : "invalid_initial";
    }
    if (std::holds_alternative<SelectInitialFormation>(command)) return "opening_finished";
    if (s.payment) {
        const auto *choice = std::get_if<Choose>(&command);
        if (!choice || !s.decision || choice->decision != s.decision->id || actor != s.payment->player) return "payment_pending";
        return (choice->option.kind == ChoiceKind::Pass || choice->option.kind == ChoiceKind::Payment) && choice->option.value <= s.payment->choices.size() ? "" : "invalid_option";
    }
    if (s.decision) {
        const auto &d = *s.decision;
        if (d.player != actor) return "decision_mismatch";
        if (d.kind == DecisionKind::Response) {
            if (std::holds_alternative<PrepareCast>(command) || std::holds_alternative<FlipAmbush>(command) || std::holds_alternative<PreloadWord>(command)) return validateResponseSetup(actor, command);
            if (const auto *pass = std::get_if<PassResponse>(&command)) return pass->decision == d.id ? "" : "stale_decision";
            if (const auto *r = std::get_if<Respond>(&command)) {
                if (r->decision != d.id) return "stale_decision";
                return validateResponse(actor, *r);
            }
            if (const auto *old = std::get_if<Choose>(&command)) {
                if (old->decision != d.id) return "stale_decision";
                if (!old->option) return {};
                const auto candidates = responsesFor(actor, d.id);
                const auto count = std::count_if(candidates.begin(), candidates.end(), [&](const Respond &r) { return r.card == old->option.value; });
                return count == 1 ? "" : "ambiguous_response";
            }
            return "decision_mismatch";
        }
        if (d.kind == DecisionKind::EffectPrepare) {
            if (const auto *prepare = std::get_if<PrepareCast>(&command)) {
                if (prepare->decision != d.id || std::find(d.options.begin(), d.options.end(), ChoiceValue::card(prepare->card)) == d.options.end()) return "decision_mismatch";
                return validatePreparation(actor, *prepare, true);
            }
        }
        const auto *choice = std::get_if<Choose>(&command);
        if (!choice || choice->decision != d.id) return "decision_mismatch";
        const auto chosen = choice->option;
        const auto opt = static_cast<CardId>(chosen.value);
        if (!(!chosen && d.mayPass) && std::find(d.options.begin(), d.options.end(), chosen) == d.options.end()) return "invalid_option";
        if (d.kind == DecisionKind::EffectPrepare && opt) return "decision_mismatch";
        if (d.kind == DecisionKind::ReturnSpell && opt) {
            const auto source = s.maintenanceCard();
            if (!s.cards.count(source) || !s.cards.count(opt)) return "invalid_return";
            const auto &card = s.cards.at(source); const auto &host = s.cards.at(opt);
            if (host.owner != actor || host.zone != Zone::Analysis || catalog().at(host.definition).type != CardType::Formation || !Rules::canHost(s, catalog(), source, opt)) return "invalid_return";
            const int ring = choice->ringIndex < 0 ? Rules::freeRing(s, catalog(), opt) : choice->ringIndex;
            if (ring < 0 || ring >= Rules::rings(s, catalog(), opt)) return "invalid_return";
            for (const auto &[id, other] : s.cards) if (id != source && other.zone == Zone::Analysis && other.host == opt && other.ringIndex == ring) return "invalid_return";
            if (hasCapability(catalog().at(card.definition), Capability::RequiresStormHeartSetup) &&
                !hasCapability(catalog().at(host.definition), Capability::StormHeart)) return "invalid_return";
        }
        return {};
    }
    return validateAction(actor, command);
}
PlanningResult GameEngine::plan(PlayerId actor, const Command &input, std::optional<PaymentChoice> payment) const {
    const auto command = normalizeCommand(input);
    if (const auto error = validate(actor, command); !error.empty()) return error;
    if (!payment) if (auto request = paymentRequest(actor, command)) return *request;
    return CommandPlan{actor, command, payment};
}
std::string GameEngine::apply(const CommandPlan &plan) {
    state_.paymentInput = plan.payment;
    auto error = execute(plan.actor, plan.command);
    state_.paymentInput.reset();
    return error;
}
} // namespace wizard
