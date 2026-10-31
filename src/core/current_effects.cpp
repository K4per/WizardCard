// 2.0 content choices live beside EffectResolver, separate from turn scheduling.
#include "engine_internal.hpp"
#include <set>
#include "wizard/core/card_rules.hpp"
#include "wizard/core/card_rules.hpp"

namespace wizard {
namespace {
bool isSpell(const CardDefinition &d) {
    return d.type == CardType::Analytic || d.type == CardType::Word || d.type == CardType::Talisman;
}
bool hasTrait(const CardDefinition &d, Capability capability) {
    return hasCapability(d, capability);
}
bool protectedFromEffect(const CardDefinition &d) {
    return hasTrait(d, Capability::EffectImmune) || hasTrait(d, Capability::EffectIndestructible);
}
bool fieldZone(Zone zone) {
    return zone == Zone::Analysis || zone == Zone::Casting || zone == Zone::Words || zone == Zone::Attached;
}
bool effectImmune(const GameState &s, const CardCatalog &catalog, CardId id) {
    const auto found = s.cards.find(id);
    if (found == s.cards.end()) return false;
    const auto &card = found->second;
    const auto &definition = catalog.at(card.definition);
    if (card_rules::effectImmune(s, catalog, id) || hasTrait(definition, Capability::EffectImmune) ||
        card_rules::protectedFormation(s, catalog, id)) return true;
    if (!hasTrait(definition, Capability::GreatStorm)) return false;
    return true;
}
bool protectedCardFromEffect(const GameState &s, const CardCatalog &catalog, CardId id) {
    const auto found = s.cards.find(id);
    return found == s.cards.end() || effectImmune(s, catalog, id) ||
           hasTrait(catalog.at(found->second.definition), Capability::EffectIndestructible);
}
bool formationHasSpells(const GameState &s, const CardCatalog &catalog, CardId formation) {
    return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        const auto &definition = catalog.at(card.definition);
        return isSpell(definition) &&
               ((card.zone == Zone::Analysis && card.host == formation) ||
                (card.zone == Zone::Casting && card.sourceFormation == formation));
    });
}
int fieldTreeSize(const GameState &s, CardId root) {
    int count = 0;
    std::vector<CardId> pending{root};
    std::set<CardId> visited;
    while (!pending.empty()) {
        const auto id = pending.back();
        pending.pop_back();
        if (!visited.insert(id).second || !s.cards.count(id)) continue;
        const auto &card = s.cards.at(id);
        if (fieldZone(card.zone)) ++count;
        for (const auto &[childId, child] : s.cards)
            if (child.host == id && fieldZone(child.zone)) pending.push_back(childId);
    }
    return count;
}
void shuffleDeck(MutationContext &s, PlayerId owner) {
    auto &deck = s.players[owner].deck;
    for (std::size_t size = deck.size(); size > 1; --size)
        std::swap(deck[size - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(size))]);
}
void returnToHand(MutationContext &s, const CardCatalog &catalog, CardId id) {
    card_rules::returnCard(s, catalog, id, true);
}
void returnFormationToSideDeck(MutationContext &s, const CardCatalog &catalog, CardId id) {
    const auto owner = s.cards.at(id).owner;
    StateMaintenance::leave(s, id, &catalog);
    auto &card = s.cards.at(id);
    card.zone = Zone::SideDeck;
    card.controller = card.owner;
    card.base = false;
    card.host = card.sourceFormation = 0;
    card.spell = SpellState::None;
    card.analysisLoad = card.castLoad = 0;
    card.analysisProgress = card.remaining = 0;
    card.ringIndex = -1;
    card.stormPoints = 0;
    card.faceDown = card.publiclyRevealed = false;
    s.events.push_back({"return_side_deck", "阵法返回副卡组", owner, id});
}
} // namespace

bool GameEngine::contentEffectV2() {
    if (cardEffect()) return true;
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size())
        return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto owner = f.item.owner;
    if (e.kind == EffectKind::RayVolley) {
        if (!f.resume<RepeatedChoiceContinuation>().completed) {
            const auto trigger = f.item;
            ++f.resume<RepeatedChoiceContinuation>().completed;
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, e.amount, e.damageType});
            return true;
        }
        if (f.resume<RepeatedChoiceContinuation>().completed >= 3) {
            ++f.cursor; f.resume<RepeatedChoiceContinuation>().completed = 0; return true;
        }
        typedDecision(owner, DecisionKind::EffectChoice, {ChoiceValue::player(0), ChoiceValue::player(1)});
        return true;
    }
    if (e.kind == EffectKind::DelayAllEnemyAnalysis) {
        typedDecision(owner, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::DelayAnalysis)}, true);
        return true;
    }

    const auto sourceCard = s.cards.find(f.item.source);
    const bool swiftWindReturn = e.kind == EffectKind::ReturnEnemyAnalysis &&
        sourceCard != s.cards.end() && hasCapability(catalog().at(sourceCard->second.definition), Capability::SwiftWind);
    const bool windStrikeDestroy = e.kind == EffectKind::DestroyEnemyAnalysis &&
        sourceCard != s.cards.end() && hasCapability(catalog().at(sourceCard->second.definition), Capability::WindStrike);
    if (e.kind == EffectKind::SearchSameSchool && !f.resume<EntityChoiceContinuation>().entity && !f.item.targetCards.empty()) {
        const auto &source = catalog().at(s.cards.at(f.item.source).definition);
        if (hasTrait(source, Capability::DiscardSpellSearch)) f.resume<EntityChoiceContinuation>().entity = f.item.targetCards.front();
    }
    if (e.kind == EffectKind::RecoverEnemyAshSpell && !f.resume<EntityChoiceContinuation>().entity && f.item.targetCard)
        f.resume<EntityChoiceContinuation>().entity = f.item.targetCard;
    if (e.kind == EffectKind::LockAnalysis) {
        if (sourceCard == s.cards.end()) {
            ++f.cursor;
            f.continuation = RepeatedChoiceContinuation{};
            return true;
        }
        auto &cloudMist = sourceCard->second;
        if (cloudMist.targets.size() >= 2) {
            ++f.cursor;
            f.continuation = RepeatedChoiceContinuation{};
            return true;
        }
        std::vector<CardId> choices;
        for (const auto &[id, card] : s.cards) {
            const auto &definition = catalog().at(card.definition);
            if (id != cloudMist.id && card.zone == Zone::Analysis && !card.faceDown &&
                definition.type == CardType::Analytic &&
                (card.spell == SpellState::Analyzing || card.spell == SpellState::Ready) &&
                !effectImmune(s, catalog(), id) &&
                std::find(cloudMist.targets.begin(), cloudMist.targets.end(), id) == cloudMist.targets.end())
                choices.push_back(id);
        }
        if (choices.empty()) {
            ++f.cursor;
            f.continuation = RepeatedChoiceContinuation{};
        } else {
            decision(owner, DecisionKind::EffectChoice, std::move(choices),
                     !cloudMist.targets.empty());
        }
        return true;
    }
    if (e.kind == EffectKind::SearchAction) {
        std::vector<CardId> cards;
        const bool stormTalismanSearch = sourceCard != s.cards.end() &&
            hasTrait(catalog().at(sourceCard->second.definition), Capability::StormSearchTalisman);
        const bool stormSpellSearch = sourceCard != s.cards.end() &&
            hasTrait(catalog().at(sourceCard->second.definition), Capability::StormSearch);
        for (const auto id : s.players[owner].deck) {
            const auto found = s.cards.find(id);
            if (found == s.cards.end() || found->second.owner != owner || found->second.zone != Zone::Deck)
                continue;
            const auto &definition = catalog().at(found->second.definition);
            const bool stormTagged = detail::stormTags(definition.tags);
            const bool eligible = stormTalismanSearch
                ? definition.type == CardType::Talisman && definition.rank <= 2 && stormTagged
                : stormSpellSearch
                    ? isSpell(definition) && stormTagged
                    : definition.type == CardType::Action &&
                          (definition.rarity == "common" || definition.rarity == "uncommon");
            if (eligible) cards.push_back(id);
        }
        if (cards.empty()) {
            ++f.cursor;
            f.continuation = RepeatedChoiceContinuation{};
        } else {
            decision(owner, DecisionKind::SearchDeck, std::move(cards));
        }
        return true;
    }
    if (e.kind == EffectKind::AdvanceAllEnemyAnalysis) {
        const int advance = std::max(1, e.amount);
        for (auto &[id, card] : s.cards) {
            const auto &definition = catalog().at(card.definition);
            if (controllerOf(card) == owner || card.zone != Zone::Analysis || card.faceDown ||
                definition.type != CardType::Analytic ||
                card.spell != SpellState::Analyzing ||
                effectImmune(s, catalog(), id))
                continue;
            card_rules::advanceAnalysis(s, catalog(), id, advance);
        }
        ++f.cursor;
        f.continuation = RepeatedChoiceContinuation{};
        return true;
    }
    if (e.kind == EffectKind::StormGuidance) {
        if (f.resume<GuidanceContinuation>().stage == GuidanceStage::SelectField) {
            std::vector<CardId> field;
            for (const auto &[id, card] : s.cards)
                if (fieldZone(card.zone) && (!card.faceDown || controllerOf(card) == owner) && !effectImmune(s, catalog(), id))
                    field.push_back(id);
            if (field.empty()) {
                ++f.cursor;
                return true;
            }
            decision(owner, DecisionKind::EffectChoice, std::move(field));
        } else if (f.resume<GuidanceContinuation>().stage == GuidanceStage::SelectMode) {
            std::vector<ChoiceValue> modes{ChoiceValue::mode(ChoiceMode::StormSearch), ChoiceValue::mode(ChoiceMode::SupportingSearch)};
            if (detail::hasStormFormation(s, catalog(), owner)) modes.insert(modes.begin() + 1, ChoiceValue::mode(ChoiceMode::StormPoints));
            typedDecision(owner, DecisionKind::EffectChoice, std::move(modes), true);
        } else if (f.resume<GuidanceContinuation>().stage == GuidanceStage::SearchStorm || f.resume<GuidanceContinuation>().stage == GuidanceStage::SearchSupporting) {
            std::vector<CardId> deck;
            for (const auto id : s.players[owner].deck) {
                const auto &definition = catalog().at(s.cards.at(id).definition);
                const bool stormSpell = detail::stormTags(definition.tags) && isSpell(definition);
                const bool supportingSpell = hasCapability(definition, Capability::CloudMist) ||
                    hasCapability(definition, Capability::ThunderSummon);
                if (f.resume<GuidanceContinuation>().stage == GuidanceStage::SearchStorm ? stormSpell : supportingSpell) deck.push_back(id);
            }
            if (deck.empty()) {
                ++f.cursor;
                f.resume<GuidanceContinuation>().stage = GuidanceStage::SelectField;
            } else
                decision(owner, DecisionKind::EffectChoice, std::move(deck));
        }
        return true;
    }
    if (e.kind == EffectKind::StormCyclone) {
        if (f.resume<CycloneContinuation>().offerDamage) {
            typedDecision(owner, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::CycloneDamage)}, true); return true;
        }
        if (!f.resume<CycloneContinuation>().offerDamage) {
            std::vector<CardId> formations;
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) != owner && card.zone == Zone::Analysis && !card.faceDown &&
                    catalog().at(card.definition).type == CardType::Formation &&
                    !protectedCardFromEffect(s, catalog(), id) && formationHasSpells(s, catalog(), id))
                    formations.push_back(id);
            if (formations.empty()) {
                ++f.cursor;
                return true;
            }
            decision(owner, DecisionKind::EffectChoice, std::move(formations));
        }
        return true;
    }
    if (e.kind == EffectKind::CuttingWind) {
        if (!f.resume<OnceContinuation>().performed) {
            f.resume<OnceContinuation>().performed = true;
            const auto trigger = f.item;
            EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, 4, DamageType::Slashing});
            return true;
        }
        if (f.resume<OnceContinuation>().performed) {
            std::vector<CardId> formations;
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) == owner && card.zone == Zone::Analysis && !card.faceDown && !card.base &&
                    catalog().at(card.definition).type == CardType::Formation &&
                    !protectedCardFromEffect(s, catalog(), id) && !formationHasSpells(s, catalog(), id) &&
                    Rules::occupied(s, id) == 0)
                    formations.push_back(id);
            if (formations.empty()) {
                ++f.cursor;
                return true;
            }
            decision(owner, DecisionKind::EffectChoice, std::move(formations));
        }
        return true;
    }
    if (e.kind == EffectKind::Destroy) {
        if (!f.resume<TargetWalkContinuation>().next) {
            if (f.item.targetCards.empty() && f.item.targetCard)
                f.item.targetCards.push_back(f.item.targetCard);
            f.resume<TargetWalkContinuation>().next = 0;
        }
        while (*f.resume<TargetWalkContinuation>().next < f.item.targetCards.size()) {
            const auto id = f.item.targetCards[*f.resume<TargetWalkContinuation>().next];
            const auto target = s.cards.find(id);
            if (target == s.cards.end() || !fieldZone(target->second.zone) ||
                protectedCardFromEffect(s, catalog(), id)) {
                ++*f.resume<TargetWalkContinuation>().next;
                continue;
            }
            if (hasCapability(catalog().at(target->second.definition), Capability::StormHeart) && target->second.stormPoints > 0) {
                f.item.targetCard = id;
                decision(controllerOf(target->second), DecisionKind::EffectChoice, {id}, true);
                return true;
            }
            StateMaintenance::leave(s, id, &catalog());
            ++*f.resume<TargetWalkContinuation>().next;
        }
        ++f.cursor;
        f.resume<TargetWalkContinuation>().next.reset();
        f.item.targetCard = 0;
        return true;
    }
    if (e.kind == EffectKind::DestroyAllEnemyField) {
        if (!f.resume<TargetWalkContinuation>().next) {

            f.item.targetCards.clear();
            f.resume<TargetWalkContinuation>().destroyed = 0;
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) != owner && fieldZone(card.zone) && card.zone == Zone::Analysis && isSpell(catalog().at(card.definition)))
                    f.item.targetCards.push_back(id);
            f.resume<TargetWalkContinuation>().next = 0;
        }
        while (*f.resume<TargetWalkContinuation>().next < f.item.targetCards.size()) {
            const auto id = f.item.targetCards[*f.resume<TargetWalkContinuation>().next];
            const auto found = s.cards.find(id);
            if (found == s.cards.end() || !fieldZone(found->second.zone) ||
                protectedCardFromEffect(s, catalog(), id)) {
                ++*f.resume<TargetWalkContinuation>().next;
                continue;
            }
            if (hasCapability(catalog().at(found->second.definition), Capability::StormHeart) && found->second.stormPoints > 0) {
                f.item.targetCard = id;
                decision(found->second.owner, DecisionKind::EffectChoice, {id}, true);
                return true;
            }
            f.resume<TargetWalkContinuation>().destroyed += 1;
            StateMaintenance::leave(s, id, &catalog());
            ++*f.resume<TargetWalkContinuation>().next;
        }
        const int damage = f.resume<TargetWalkContinuation>().destroyed * 4;
        if (damage > 0)
            EffectResolver::apply(s, catalog(), f.item,
                                  {EffectKind::Damage, damage, DamageType::Slashing, EffectRecipient::Opponent});
        if (s.effect) { ++f.cursor; f.resume<TargetWalkContinuation>().next.reset(); f.resume<TargetWalkContinuation>().destroyed = 0; }
        return true;
    }
    std::vector<CardId> choices;
    bool optional = true;
    if (windStrikeDestroy) optional = false;
    if (e.kind == EffectKind::ChooseDamage) {
        typedDecision(owner, DecisionKind::DamageChoice,
                      {ChoiceValue::damage(DamageType::Acid), ChoiceValue::damage(DamageType::Cold),
                       ChoiceValue::damage(DamageType::Fire), ChoiceValue::damage(DamageType::Lightning),
                       ChoiceValue::damage(DamageType::Poison), ChoiceValue::damage(DamageType::Thunder)});
        return true;
    }
    switch (e.kind) {
    case EffectKind::DelayEnemyAnalysis:
    case EffectKind::ConcealEnemySpell:
    case EffectKind::RevealEnemyAmbush:
    case EffectKind::ReturnEnemyAnalysis:
    case EffectKind::DestroyFormationSpells:
    case EffectKind::SearchSameSchool:
    case EffectKind::RecoverEnemyAshSpell:
    case EffectKind::RecoverFormation:
    case EffectKind::DestroyEnemyAnalysis:
    case EffectKind::LockAnalysis:
        break;
    default:
        return false;
    }
    for (const auto &[id, card] : s.cards) {
        const auto &definition = catalog().at(card.definition);
        bool eligible = false;
        switch (e.kind) {
        case EffectKind::DelayEnemyAnalysis:
            eligible = controllerOf(card) != owner && card.zone == Zone::Analysis && !card.faceDown &&
                       definition.type == CardType::Analytic &&
                       (card.spell == SpellState::Analyzing || card.spell == SpellState::Ready);
            break;
        case EffectKind::DestroyEnemyAnalysis:
            eligible = controllerOf(card) != owner && fieldZone(card.zone) &&
                       isSpell(definition) && !protectedCardFromEffect(s, catalog(), id) &&
                       (!windStrikeDestroy || id == f.item.targetCard) &&
                       (sourceCard == s.cards.end() ||
                        !hasCapability(catalog().at(sourceCard->second.definition), Capability::GreatStorm) ||
                        card.zone == Zone::Analysis);
            break;
        case EffectKind::ConcealEnemySpell:
            eligible = controllerOf(card) != owner && card.zone == Zone::Analysis && !card.faceDown && isSpell(definition);
            break;
        case EffectKind::RevealEnemyAmbush:
            eligible = controllerOf(card) != owner && card.zone == Zone::Analysis && card.faceDown && isSpell(definition);
            break;
        case EffectKind::ReturnEnemyAnalysis:
            eligible = controllerOf(card) != owner && fieldZone(card.zone) &&
                       (swiftWindReturn
                            ? definition.type == CardType::Formation && definition.level <= 2
                            : isSpell(definition));
            break;
        case EffectKind::DestroyFormationSpells:
            eligible = controllerOf(card) != owner && card.zone == Zone::Analysis &&
                       definition.type == CardType::Formation && !protectedCardFromEffect(s, catalog(), id) &&
                       formationHasSpells(s, catalog(), id);
            break;
        case EffectKind::SearchSameSchool:
            optional = false;
            if (!f.resume<EntityChoiceContinuation>().entity)
                eligible = controllerOf(card) == owner && card.zone == Zone::Analysis && !card.faceDown && isSpell(definition);
            else {
                const auto &material = catalog().at(s.cards.at(*f.resume<EntityChoiceContinuation>().entity).definition);
                eligible = card.owner == owner && card.zone == Zone::Deck && isSpell(definition) &&
                           !definition.school.empty() && definition.school == material.school;
            }
            break;
        case EffectKind::RecoverEnemyAshSpell:
            optional = false;
            if (!f.resume<EntityChoiceContinuation>().entity) {
                eligible = card.owner != owner && card.zone == Zone::Ash && !card.faceDown &&
                           definition.type == CardType::Analytic && definition.rank <= 5;
                if (eligible)
                    eligible = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &item) {
                        const auto &host = item.second;
                        return controllerOf(host) == owner && host.zone == Zone::Analysis && !host.faceDown &&
                               catalog().at(host.definition).type == CardType::Formation &&
                               Rules::maxRank(s, catalog(), host.id) >= definition.rank &&
                               Rules::freeRing(s, catalog(), host.id) >= 0;
                    });
            } else {
                const auto &spell = catalog().at(s.cards.at(*f.resume<EntityChoiceContinuation>().entity).definition);
                eligible = card.owner == owner && card.zone == Zone::Analysis && !card.faceDown &&
                           definition.type == CardType::Formation &&
                           Rules::maxRank(s, catalog(), id) >= spell.rank && Rules::freeRing(s, catalog(), id) >= 0;
            }
            break;
        case EffectKind::RecoverFormation:
            eligible = card.owner == owner && card.zone == Zone::Ash && !card.faceDown &&
                       definition.type == CardType::Formation;
            break;
        default:
            break;
        }
        if (eligible && ((e.kind == EffectKind::RecoverEnemyAshSpell && f.resume<EntityChoiceContinuation>().entity.has_value()) ||
                         !effectImmune(s, catalog(), id)))
            choices.push_back(id);
    }
    if (choices.empty()) {
        ++f.cursor;
        f.continuation = RepeatedChoiceContinuation{};
    } else
        decision(owner, DecisionKind::EffectChoice, std::move(choices), optional);
    return true;
}

bool GameEngine::chooseContentEffectV2(PlayerId actor, DecisionKind kind, ChoiceValue choice) {
    const auto selected = static_cast<CardId>(choice.value);
    if ((kind == DecisionKind::EffectChoice || kind == DecisionKind::DamageChoice) &&
        chooseCardEffect(actor, choice)) return true;
    auto &s = state_;
    if (!s.effect || kind != DecisionKind::EffectChoice && kind != DecisionKind::DamageChoice)
        return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto source = s.cards.find(f.item.source);
    if (e.kind == EffectKind::RayVolley) {
        auto trigger = f.item;
        trigger.targetPlayer = static_cast<PlayerId>(choice.value);
        ++f.resume<RepeatedChoiceContinuation>().completed;
        EffectResolver::apply(s, catalog(), trigger, {EffectKind::Damage, e.amount, e.damageType});
        return true;
    }
    if (e.kind == EffectKind::DelayAllEnemyAnalysis) {
        if (selected)
            for (auto &[id, card] : s.cards)
                if (controllerOf(card) != f.item.owner && card.zone == Zone::Analysis && !card.faceDown &&
                    catalog().at(card.definition).type == CardType::Analytic &&
                    (card.spell == SpellState::Analyzing || card.spell == SpellState::Ready) &&
                    !effectImmune(s, catalog(), id)) {
                    --card.analysisProgress;
                    card.spell = SpellState::Analyzing;
                    s.events.push_back({"analysis_delayed", "冰风暴：解析时间增加一回合", -1, id});
                }
        ++f.cursor; return true;
    }

    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::DestroyAllEnemyField &&
        f.resume<TargetWalkContinuation>().next.has_value() && *f.resume<TargetWalkContinuation>().next < f.item.targetCards.size() &&
        f.item.targetCard) {
        const auto id = f.item.targetCard;
        auto heart = s.cards.find(id);
        if (heart != s.cards.end() && hasCapability(catalog().at(heart->second.definition), Capability::StormHeart) &&
            heart->second.owner == actor && heart->second.stormPoints > 0 && selected == id)
            --heart->second.stormPoints;
        else if (heart != s.cards.end() && fieldZone(heart->second.zone)) {
            f.resume<TargetWalkContinuation>().destroyed += 1;
            StateMaintenance::leave(s, id, &catalog());
        }
        f.item.targetCard = 0;
        ++*f.resume<TargetWalkContinuation>().next;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::Destroy &&
        f.resume<TargetWalkContinuation>().next.has_value() && f.item.targetCard) {
        const auto id = f.item.targetCard;
        auto heart = s.cards.find(id);
        if (heart != s.cards.end() && hasCapability(catalog().at(heart->second.definition), Capability::StormHeart) &&
            heart->second.owner == actor && heart->second.stormPoints > 0 && selected == id) {
            --heart->second.stormPoints;
        } else if (heart != s.cards.end() && fieldZone(heart->second.zone)) {
            StateMaintenance::leave(s, id, &catalog());
        }
        f.item.targetCard = 0;
        ++*f.resume<TargetWalkContinuation>().next;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::DestroyAllEnemyField &&
        f.resume<TargetWalkContinuation>().next.has_value() && *f.resume<TargetWalkContinuation>().next < f.item.targetCards.size() &&
        f.item.targetCard && !selected) {
        const auto id = f.item.targetCard;
        const auto target = s.cards.find(id);
        if (target != s.cards.end() && fieldZone(target->second.zone)) {
            f.resume<TargetWalkContinuation>().destroyed += 1;
            StateMaintenance::leave(s, id, &catalog());
        }
        f.item.targetCard = 0;
        ++*f.resume<TargetWalkContinuation>().next;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::StormGuidance) {
        if (f.resume<GuidanceContinuation>().stage == GuidanceStage::SelectField) {
            StateMaintenance::leave(s, selected, &catalog());
            if (s.cards.at(selected).zone != Zone::Ash) {
                ++f.cursor; f.resume<GuidanceContinuation>().stage = GuidanceStage::SelectField;
            } else f.resume<GuidanceContinuation>().stage = GuidanceStage::SelectMode;
            return true;
        }
        if (f.resume<GuidanceContinuation>().stage == GuidanceStage::SelectMode) {
            if (!selected) {
                ++f.cursor; f.resume<GuidanceContinuation>().stage = GuidanceStage::SelectField; return true;
            }
            if (choice == ChoiceValue::mode(ChoiceMode::StormSearch) || choice == ChoiceValue::mode(ChoiceMode::SupportingSearch)) {
                const bool stormSearch = choice == ChoiceValue::mode(ChoiceMode::StormSearch);
                std::vector<CardId> cards;
                for (const auto id : s.players[actor].deck) {
                    const auto &definition = catalog().at(s.cards.at(id).definition);
                    const bool isStormSpell = isSpell(definition) &&
                        detail::stormTags(definition.tags);
                    const bool isAuxiliary = hasCapability(definition, Capability::CloudMist) ||
                        hasCapability(definition, Capability::ThunderSummon);
                    if (stormSearch ? isStormSpell : isAuxiliary) cards.push_back(id);
                }
                if (cards.empty()) {
                    ++f.cursor;
                    f.resume<GuidanceContinuation>().stage = GuidanceStage::SelectField;
                } else {
                    f.resume<GuidanceContinuation>().stage = stormSearch ? GuidanceStage::SearchStorm : GuidanceStage::SearchSupporting;
                    decision(actor, DecisionKind::EffectChoice, std::move(cards));
                }
                return true;
            }
            if (choice == ChoiceValue::mode(ChoiceMode::StormPoints)) {
                for (auto &[id, formation] : s.cards)
                    if (controllerOf(formation) == actor && formation.zone == Zone::Analysis &&
                        catalog().at(formation.definition).type == CardType::Formation &&
                        detail::stormTags(catalog().at(formation.definition).tags)) {
                        formation.stormPoints += 2;
                        break;
                    }
                ++f.cursor;
                f.resume<GuidanceContinuation>().stage = GuidanceStage::SelectField;
                return true;
            }
            return false;
        }
        if (f.resume<GuidanceContinuation>().stage == GuidanceStage::SearchStorm || f.resume<GuidanceContinuation>().stage == GuidanceStage::SearchSupporting) {
            auto &deck = s.players[actor].deck;
            deck.erase(std::remove(deck.begin(), deck.end(), selected), deck.end());
            auto &card = s.cards.at(selected);
            card.zone = Zone::Hand;
            shuffleDeck(s, actor);
            s.events.push_back({"search", "风暴导向：法术加入手牌", actor, selected});
            ++f.cursor;
            f.resume<GuidanceContinuation>().stage = GuidanceStage::SelectField;
            return true;
        }
    }
    if (!selected) {
        ++f.cursor;
        f.continuation = RepeatedChoiceContinuation{};
        return true;
    }
    if (kind == DecisionKind::DamageChoice) {
        Effect damage{EffectKind::Damage, e.amount, static_cast<DamageType>(choice.value - static_cast<unsigned>(ChoiceMode::DamageTypeBase))};
        EffectResolver::apply(s, catalog(), f.item, damage);
        if (s.effect)
            ++s.effect->cursor;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::StormCyclone) {
        if (f.resume<CycloneContinuation>().offerDamage) {
            const auto trigger = f.item; const int damage = f.resume<CycloneContinuation>().destroyed * 2;
            ++f.cursor; f.resume<CycloneContinuation>().offerDamage = false; f.resume<CycloneContinuation>().destroyed = 0;
            if (selected) EffectResolver::apply(s, catalog(), trigger,
                {EffectKind::Damage, damage, DamageType::Bludgeoning, EffectRecipient::Opponent});
            return true;
        }
        int destroyed = 0;
        std::vector<CardId> spells;
        for (const auto &[id, spell] : s.cards) {
            const bool hosted = (spell.zone == Zone::Analysis && spell.host == selected) ||
                                (spell.zone == Zone::Casting && spell.sourceFormation == selected);
            if (hosted && controllerOf(spell) != actor && isSpell(catalog().at(spell.definition)) &&
                !protectedCardFromEffect(s, catalog(), id))
                spells.push_back(id);
        }
        for (const auto id : spells) {
            if (!s.cards.count(id) || !fieldZone(s.cards.at(id).zone)) continue;
            destroyed += 1;
            StateMaintenance::leave(s, id, &catalog());
        }
        { f.resume<CycloneContinuation>().destroyed = destroyed; f.resume<CycloneContinuation>().offerDamage = true; return true; }
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::CuttingWind) {
        const int rank = Rules::level(s, catalog(), selected);
        StateMaintenance::leave(s, selected, &catalog());
        const auto trigger = f.item;
        ++f.cursor; f.resume<OnceContinuation>().performed = false;

        if (s.result == -1) EffectResolver::apply(s, catalog(), trigger,
                              {EffectKind::Damage, rank, DamageType::Thunder,
                               EffectRecipient::Target});
        return true;
    }
    if (e.kind == EffectKind::LockAnalysis) {
        if (source != s.cards.end() &&
            std::find(source->second.targets.begin(), source->second.targets.end(), selected) ==
                source->second.targets.end())
            source->second.targets.push_back(selected);
        return true;
    }
    auto &card = s.cards.at(selected);
    const auto &definition = catalog().at(card.definition);
    switch (e.kind) {
    case EffectKind::DestroyEnemyAnalysis:
        StateMaintenance::leave(s, selected, &catalog());
        break;
    case EffectKind::DelayEnemyAnalysis:
        card.analysisProgress = card.analysisProgress - std::max(1, e.amount);
        card.analysisStarted += std::max(1, e.amount);
        if (definition.analysisTurns > card.analysisProgress)
            card.spell = SpellState::Analyzing;
        break;
    case EffectKind::ConcealEnemySpell:
        card.faceDown = true;
        card.publiclyRevealed = false;
        card.ambushedTurn = s.globalTurn;
        card.analysisProgress = 0;
        card.analysisLoad = card.castLoad = 0;
        card.wasAmbushed = true;
        s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
            [&](const TemporaryLoad &load) { return load.source == selected && load.sourceBound; }), s.temporary.end());
        card.spell = SpellState::None;
        card.analysisStarted = s.players[controllerOf(card)].ownTurn;
        break;
    case EffectKind::RevealEnemyAmbush:
        // Showing a card communicates its identity without flipping or starting analysis.
        card.publiclyRevealed = true;
        s.events.push_back({"show_card", "展示盖伏法术：" + definition.name, -1, selected});
        break;
    case EffectKind::ReturnEnemyAnalysis:
        if (source != s.cards.end() && hasCapability(catalog().at(source->second.definition), Capability::SwiftWind))
            returnFormationToSideDeck(s, catalog(), selected);
        else
            returnToHand(s, catalog(), selected);
        break;
    case EffectKind::DestroyFormationSpells: {
        std::vector<CardId> spells;
        for (const auto &[id, spell] : s.cards)
            if (isSpell(catalog().at(spell.definition)) &&
                ((spell.zone == Zone::Analysis && spell.host == selected) ||
                 (spell.zone == Zone::Casting && spell.sourceFormation == selected)))
                spells.push_back(id);
        for (auto id : spells)
            if (!protectedCardFromEffect(s, catalog(), id))
                StateMaintenance::leave(s, id, &catalog());
        break;
    }
    case EffectKind::SearchSameSchool:
        if (!f.resume<EntityChoiceContinuation>().entity) {
            f.resume<EntityChoiceContinuation>().entity = selected;
            StateMaintenance::leave(s, selected, &catalog());
            return true;
        } else {
            auto &deck = s.players[actor].deck;
            deck.erase(std::remove(deck.begin(), deck.end(), selected), deck.end());
            card.zone = Zone::Hand;
            for (std::size_t size = deck.size(); size > 1; --size)
                std::swap(deck[size - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(size))]);
            s.events.push_back({"search", "同学派法术加入手牌", actor, selected});
        }
        break;
    case EffectKind::RecoverEnemyAshSpell:
        if (!f.resume<EntityChoiceContinuation>().entity) {
            f.resume<EntityChoiceContinuation>().entity = selected;
            return true;
        } else {
            auto &spell = s.cards.at(*f.resume<EntityChoiceContinuation>().entity);
            // Funny Box changes control, while the card's owner remains the
            // opponent so that later departure returns it to the owner's ash.
            spell.controller = actor;
            spell.zone = Zone::Analysis;
            spell.host = selected;
            spell.sourceFormation = selected;
            spell.spell = SpellState::None;
            spell.ringIndex = Rules::freeRing(s, catalog(), selected);
            spell.faceDown = true;
            spell.publiclyRevealed = false;
            spell.wasAmbushed = true;
            spell.ambushedTurn = s.globalTurn;
            spell.fieldOrder = s.nextFieldOrder++;
            s.events.push_back({"recover_ambush", "从灰烬区取得法术并盖伏", -1, spell.id});
        }
        break;
    case EffectKind::RecoverFormation:
        card.zone = Zone::SideDeck;
        card.base = false;
        card.faceDown = false;
        s.events.push_back({"recover_formation", "阵法返回副卡组", actor, selected});
        break;
    default:
        return false;
    }
    ++f.cursor;
    f.continuation = RepeatedChoiceContinuation{};
    StateMaintenance::check(s, catalog());
    return true;
}
} // namespace wizard
