// 2.0 content choices live beside EffectResolver, separate from turn scheduling.
#include "engine_internal.hpp"
#include "wizard/core/sique.hpp"

namespace wizard {
namespace {
bool isSpell(const CardDefinition &d) {
    return d.type == CardType::Analytic || d.type == CardType::Word || d.type == CardType::Talisman;
}
bool hasTrait(const CardDefinition &d, const std::string &trait) {
    return std::find(d.traits.begin(), d.traits.end(), trait) != d.traits.end();
}
bool protectedFromEffect(const CardDefinition &d) {
    return hasTrait(d, "effect_immune") || hasTrait(d, "effect_indestructible");
}
bool fieldZone(Zone zone) {
    return zone == Zone::Analysis || zone == Zone::Casting || zone == Zone::Words || zone == Zone::Attached;
}
bool effectImmune(const GameState &s, const CardCatalog &catalog, CardId id) {
    const auto found = s.cards.find(id);
    if (found == s.cards.end()) return false;
    const auto &card = found->second;
    const auto &definition = catalog.at(card.definition);
    if (hasTrait(definition, "effect_immune") || sique::protectedCampfire(s, id)) return true;
    if (!hasTrait(definition, "great_storm")) return false;
    const auto host = s.cards.find(card.host ? card.host : card.sourceFormation);
    return host != s.cards.end() && host->second.zone == Zone::Analysis &&
           catalog.at(host->second.definition).id == "storm-heart";
}
bool protectedCardFromEffect(const GameState &s, const CardCatalog &catalog, CardId id) {
    const auto found = s.cards.find(id);
    return found == s.cards.end() || effectImmune(s, catalog, id) ||
           hasTrait(catalog.at(found->second.definition), "effect_indestructible");
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
void shuffleDeck(GameState &s, PlayerId owner) {
    auto &deck = s.players[owner].deck;
    for (std::size_t size = deck.size(); size > 1; --size)
        std::swap(deck[size - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(size))]);
}
void returnToHand(GameState &s, const CardCatalog &catalog, CardId id) {
    auto &card = s.cards.at(id);
    std::vector<CardId> attachments;
    for (const auto &[childId, child] : s.cards)
        if (child.host == id && child.zone == Zone::Attached)
            attachments.push_back(childId);
    for (auto child : attachments)
        StateMaintenance::leave(s, child, &catalog);
    card.zone = Zone::Hand;
    card.host = card.sourceFormation = 0;
    card.spell = SpellState::None;
    card.analysisLoad = card.castLoad = 0;
    card.analysisProgress = card.remaining = 0;
    card.ringIndex = -1;
    card.faceDown = false;
    card.publiclyRevealed = false;
    card.wasAmbushed = false;
    card.ambushedTurn = 0;
    card.canceledTurn = -1;
    card.settingPaid = card.concentrationCost = 0;
    card.payments.clear();
    card.targetCard = 0;
    card.targets.clear();
    card.targetPlayer = -1;
    s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                    [&](const TemporaryLoad &load) { return load.source == id && load.sourceBound; }),
                       s.temporary.end());
    s.events.push_back({"return_hand", "法术返回手牌", -1, id});
}
void returnFormationToSideDeck(GameState &s, const CardCatalog &catalog, CardId id) {
    const auto owner = s.cards.at(id).owner;
    StateMaintenance::leave(s, id, &catalog);
    auto &card = s.cards.at(id);
    card.zone = Zone::SideDeck;
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
    if (siqueEffect()) return true;
    auto &s = state_;
    if (!catalog_.usesAlphaV2() || !s.effect || s.effect->cursor >= s.effect->item.effects.size())
        return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto owner = f.item.owner;
    const auto sourceCard = s.cards.find(f.item.source);
    const bool swiftWindReturn = e.kind == EffectKind::ReturnEnemyAnalysis &&
        sourceCard != s.cards.end() && sourceCard->second.definition == "swift-wind";
    const bool windStrikeDestroy = e.kind == EffectKind::DestroyEnemyAnalysis &&
        sourceCard != s.cards.end() && sourceCard->second.definition == "wind-strike";
    if (e.kind == EffectKind::SearchSameSchool && f.clearRemaining < 0 && !f.item.targetCards.empty()) {
        const auto &source = catalog_.at(s.cards.at(f.item.source).definition);
        if (hasTrait(source, "discard_spell_search")) f.clearRemaining = static_cast<int>(f.item.targetCards.front());
    }
    if (e.kind == EffectKind::RecoverEnemyAshSpell && f.clearRemaining < 0 && f.item.targetCard)
        f.clearRemaining = static_cast<int>(f.item.targetCard);
    if (e.kind == EffectKind::LockAnalysis) {
        if (sourceCard == s.cards.end()) {
            ++f.cursor;
            f.clearRemaining = -1;
            return true;
        }
        auto &cloudMist = sourceCard->second;
        if (cloudMist.targets.size() >= 2) {
            ++f.cursor;
            f.clearRemaining = -1;
            return true;
        }
        std::vector<CardId> choices;
        for (const auto &[id, card] : s.cards) {
            const auto &definition = catalog_.at(card.definition);
            if (id != cloudMist.id && card.zone == Zone::Analysis && !card.faceDown &&
                definition.type == CardType::Analytic &&
                (card.spell == SpellState::Analyzing || card.spell == SpellState::Ready) &&
                !effectImmune(s, catalog_, id) &&
                std::find(cloudMist.targets.begin(), cloudMist.targets.end(), id) == cloudMist.targets.end())
                choices.push_back(id);
        }
        if (choices.empty()) {
            ++f.cursor;
            f.clearRemaining = -1;
        } else {
            decision(owner, DecisionKind::EffectChoice, std::move(choices), true);
        }
        return true;
    }
    if (e.kind == EffectKind::SearchAction) {
        if (f.clearRemaining >= 0) return false;
        std::vector<CardId> cards;
        const bool stormTalismanSearch = sourceCard != s.cards.end() &&
            hasTrait(catalog_.at(sourceCard->second.definition), "storm_search_talisman");
        const bool stormSpellSearch = sourceCard != s.cards.end() &&
            hasTrait(catalog_.at(sourceCard->second.definition), "storm_search");
        for (const auto id : s.players[owner].deck) {
            const auto found = s.cards.find(id);
            if (found == s.cards.end() || found->second.owner != owner || found->second.zone != Zone::Deck)
                continue;
            const auto &definition = catalog_.at(found->second.definition);
            const bool stormTagged = std::find(definition.tags.begin(), definition.tags.end(), "风暴") !=
                                     definition.tags.end();
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
            f.clearRemaining = -1;
        } else {
            decision(owner, DecisionKind::SearchDeck, std::move(cards));
        }
        return true;
    }
    if (e.kind == EffectKind::AdvanceAllEnemyAnalysis) {
        const int advance = std::max(1, e.amount);
        for (auto &[id, card] : s.cards) {
            const auto &definition = catalog_.at(card.definition);
            if (card.owner == owner || card.zone != Zone::Analysis || card.faceDown ||
                definition.type != CardType::Analytic ||
                card.spell != SpellState::Analyzing ||
                effectImmune(s, catalog_, id))
                continue;
            card.analysisProgress = std::min(definition.analysisTurns, card.analysisProgress + advance);
            if (card.analysisProgress >= definition.analysisTurns) {
                card.spell = SpellState::Ready;
                analysisCompletedV2(id);
            }
        }
        ++f.cursor;
        f.clearRemaining = -1;
        return true;
    }
    if (e.kind == EffectKind::StormGuidance) {
        if (f.clearRemaining == -1) {
            std::vector<CardId> field;
            for (const auto &[id, card] : s.cards)
                if (fieldZone(card.zone) && (!card.faceDown || card.owner == owner) &&
                    !protectedCardFromEffect(s, catalog_, id))
                    field.push_back(id);
            if (field.empty()) {
                ++f.cursor;
                return true;
            }
            decision(owner, DecisionKind::EffectChoice, std::move(field));
        } else if (f.clearRemaining == -2) {
            std::vector<std::uint32_t> modes{0xfffffff1u, 0xfffffff3u};
            if (detail::hasStormFormation(s, catalog_, owner)) modes.insert(modes.begin() + 1, 0xfffffff2u);
            decision(owner, DecisionKind::EffectChoice, std::move(modes));
        } else if (f.clearRemaining == -3 || f.clearRemaining == -4) {
            std::vector<CardId> deck;
            for (const auto id : s.players[owner].deck) {
                const auto &definition = catalog_.at(s.cards.at(id).definition);
                const bool stormSpell = std::find(definition.tags.begin(), definition.tags.end(), "风暴") !=
                                            definition.tags.end() && isSpell(definition);
                const bool supportingSpell = definition.id == "cloud-mist" || definition.id == "thunder-summon";
                if (f.clearRemaining == -3 ? stormSpell : supportingSpell) deck.push_back(id);
            }
            if (deck.empty()) {
                ++f.cursor;
                f.clearRemaining = -1;
            } else
                decision(owner, DecisionKind::EffectChoice, std::move(deck));
        }
        return true;
    }
    if (e.kind == EffectKind::StormCyclone) {
        if (f.clearRemaining < 0) {
            std::vector<CardId> formations;
            for (const auto &[id, card] : s.cards)
                if (card.owner != owner && card.zone == Zone::Analysis && !card.faceDown &&
                    catalog_.at(card.definition).type == CardType::Formation &&
                    !protectedCardFromEffect(s, catalog_, id) && formationHasSpells(s, catalog_, id))
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
        if (f.clearRemaining < 0) {
            std::vector<CardId> formations;
            for (const auto &[id, card] : s.cards)
                if (card.owner == owner && card.zone == Zone::Analysis && !card.faceDown && !card.base &&
                    catalog_.at(card.definition).type == CardType::Formation &&
                    !protectedCardFromEffect(s, catalog_, id) && !formationHasSpells(s, catalog_, id) &&
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
        if (f.clearRemaining < 0) {
            if (f.item.targetCards.empty() && f.item.targetCard)
                f.item.targetCards.push_back(f.item.targetCard);
            f.clearRemaining = 0;
        }
        while (f.clearRemaining < static_cast<int>(f.item.targetCards.size())) {
            const auto id = f.item.targetCards[static_cast<std::size_t>(f.clearRemaining)];
            const auto target = s.cards.find(id);
            if (target == s.cards.end() || !fieldZone(target->second.zone) ||
                protectedCardFromEffect(s, catalog_, id)) {
                ++f.clearRemaining;
                continue;
            }
            if (target->second.definition == "storm-heart" && target->second.stormPoints > 0) {
                f.item.targetCard = id;
                decision(target->second.owner, DecisionKind::EffectChoice, {id}, true);
                return true;
            }
            StateMaintenance::leave(s, id, &catalog_);
            ++f.clearRemaining;
        }
        ++f.cursor;
        f.clearRemaining = -1;
        f.item.targetCard = 0;
        return true;
    }
    if (e.kind == EffectKind::DestroyAllEnemyField) {
        if (f.clearRemaining < 0) {
            if (sourceCard != s.cards.end() && sourceCard->second.definition == "great-storm") {
                const auto heartId = sourceCard->second.sourceFormation;
                auto heart = s.cards.find(heartId);
                if (heart != s.cards.end() && heart->second.definition == "storm-heart" &&
                    heart->second.zone == Zone::Analysis) {
                    sourceCard->second.remaining = heart->second.stormPoints;
                    heart->second.stormPoints = 0;
                }
            }
            f.item.targetCards.clear();
            f.resolvedCount = 0;
            for (const auto &[id, card] : s.cards)
                if (card.owner != owner && fieldZone(card.zone)) f.item.targetCards.push_back(id);
            f.clearRemaining = 0;
        }
        while (f.clearRemaining < static_cast<int>(f.item.targetCards.size())) {
            const auto id = f.item.targetCards[static_cast<std::size_t>(f.clearRemaining)];
            const auto found = s.cards.find(id);
            if (found == s.cards.end() || !fieldZone(found->second.zone) ||
                protectedCardFromEffect(s, catalog_, id)) {
                ++f.clearRemaining;
                continue;
            }
            if (found->second.definition == "storm-heart" && found->second.stormPoints > 0) {
                f.item.targetCard = id;
                decision(found->second.owner, DecisionKind::EffectChoice, {id}, true);
                return true;
            }
            f.resolvedCount += fieldTreeSize(s, id);
            StateMaintenance::leave(s, id, &catalog_);
            ++f.clearRemaining;
        }
        const int damage = f.resolvedCount * 4;
        if (damage > 0)
            EffectResolver::apply(s, catalog_, f.item,
                                  {EffectKind::Damage, damage, DamageType::Slashing, EffectRecipient::Opponent});
        ++f.cursor;
        f.clearRemaining = -1;
        f.resolvedCount = 0;
        return true;
    }
    std::vector<CardId> choices;
    bool optional = true;
    if (windStrikeDestroy) optional = false;
    if (e.kind == EffectKind::ChooseDamage) {
        decision(owner, DecisionKind::DamageChoice,
                 {static_cast<CardId>(DamageType::Acid) + 1, static_cast<CardId>(DamageType::Cold) + 1,
                  static_cast<CardId>(DamageType::Fire) + 1, static_cast<CardId>(DamageType::Lightning) + 1,
                  static_cast<CardId>(DamageType::Poison) + 1, static_cast<CardId>(DamageType::Thunder) + 1});
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
        const auto &definition = catalog_.at(card.definition);
        bool eligible = false;
        switch (e.kind) {
        case EffectKind::DelayEnemyAnalysis:
            eligible = card.owner != owner && card.zone == Zone::Analysis && !card.faceDown &&
                       definition.type == CardType::Analytic &&
                       (card.spell == SpellState::Analyzing || card.spell == SpellState::Ready);
            break;
        case EffectKind::DestroyEnemyAnalysis:
            eligible = card.owner != owner && fieldZone(card.zone) &&
                       isSpell(definition) && !protectedCardFromEffect(s, catalog_, id) &&
                       (!windStrikeDestroy || id == f.item.targetCard);
            break;
        case EffectKind::ConcealEnemySpell:
            eligible = card.owner != owner && card.zone == Zone::Analysis && !card.faceDown && isSpell(definition);
            break;
        case EffectKind::RevealEnemyAmbush:
            eligible = card.owner != owner && card.zone == Zone::Analysis && card.faceDown && isSpell(definition);
            break;
        case EffectKind::ReturnEnemyAnalysis:
            eligible = card.owner != owner && fieldZone(card.zone) &&
                       (swiftWindReturn
                            ? definition.type == CardType::Formation && definition.level <= 2
                            : isSpell(definition));
            break;
        case EffectKind::DestroyFormationSpells:
            eligible = card.owner != owner && card.zone == Zone::Analysis &&
                       definition.type == CardType::Formation && !protectedCardFromEffect(s, catalog_, id) &&
                       formationHasSpells(s, catalog_, id);
            break;
        case EffectKind::SearchSameSchool:
            optional = false;
            if (f.clearRemaining < 0)
                eligible = card.owner == owner && card.zone == Zone::Analysis && !card.faceDown && isSpell(definition);
            else {
                const auto &material = catalog_.at(s.cards.at(static_cast<CardId>(f.clearRemaining)).definition);
                eligible = card.owner == owner && card.zone == Zone::Deck && isSpell(definition) &&
                           !definition.school.empty() && definition.school == material.school;
            }
            break;
        case EffectKind::RecoverEnemyAshSpell:
            optional = false;
            if (f.clearRemaining < 0) {
                eligible = card.owner != owner && card.zone == Zone::Ash && !card.faceDown &&
                           definition.type == CardType::Analytic && definition.rank <= 5;
                if (eligible)
                    eligible = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &item) {
                        const auto &host = item.second;
                        return host.owner == owner && host.zone == Zone::Analysis && !host.faceDown &&
                               catalog_.at(host.definition).type == CardType::Formation &&
                               Rules::maxRank(s, catalog_, host.id) >= definition.rank &&
                               Rules::freeRing(s, catalog_, host.id) >= 0;
                    });
            } else {
                const auto &spell = catalog_.at(s.cards.at(static_cast<CardId>(f.clearRemaining)).definition);
                eligible = card.owner == owner && card.zone == Zone::Analysis && !card.faceDown &&
                           definition.type == CardType::Formation &&
                           Rules::maxRank(s, catalog_, id) >= spell.rank && Rules::freeRing(s, catalog_, id) >= 0;
            }
            break;
        case EffectKind::RecoverFormation:
            eligible = card.owner == owner && card.zone == Zone::Ash && !card.faceDown &&
                       definition.type == CardType::Formation;
            break;
        default:
            break;
        }
        if (eligible && ((e.kind == EffectKind::RecoverEnemyAshSpell && f.clearRemaining >= 0) ||
                         !effectImmune(s, catalog_, id)))
            choices.push_back(id);
    }
    if (choices.empty()) {
        ++f.cursor;
        f.clearRemaining = -1;
    } else
        decision(owner, DecisionKind::EffectChoice, std::move(choices), optional);
    return true;
}

bool GameEngine::chooseContentEffectV2(PlayerId actor, DecisionKind kind, CardId selected) {
    if (kind == DecisionKind::EffectChoice && chooseSiqueEffect(actor, selected)) return true;
    auto &s = state_;
    if (!catalog_.usesAlphaV2() || !s.effect ||
        (kind != DecisionKind::EffectChoice && kind != DecisionKind::DamageChoice))
        return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto source = s.cards.find(f.item.source);
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::DestroyAllEnemyField &&
        f.clearRemaining >= 0 && f.clearRemaining < static_cast<int>(f.item.targetCards.size()) &&
        f.item.targetCard) {
        const auto id = f.item.targetCard;
        auto heart = s.cards.find(id);
        if (heart != s.cards.end() && heart->second.definition == "storm-heart" &&
            heart->second.owner == actor && heart->second.stormPoints > 0 && selected == id)
            --heart->second.stormPoints;
        else if (heart != s.cards.end() && fieldZone(heart->second.zone)) {
            f.resolvedCount += fieldTreeSize(s, id);
            StateMaintenance::leave(s, id, &catalog_);
        }
        f.item.targetCard = 0;
        ++f.clearRemaining;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::Destroy &&
        f.clearRemaining >= 0 && f.item.targetCard) {
        const auto id = f.item.targetCard;
        auto heart = s.cards.find(id);
        if (heart != s.cards.end() && heart->second.definition == "storm-heart" &&
            heart->second.owner == actor && heart->second.stormPoints > 0 && selected == id) {
            --heart->second.stormPoints;
        } else if (heart != s.cards.end() && fieldZone(heart->second.zone)) {
            StateMaintenance::leave(s, id, &catalog_);
        }
        f.item.targetCard = 0;
        ++f.clearRemaining;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::DestroyAllEnemyField &&
        f.clearRemaining >= 0 && f.clearRemaining < static_cast<int>(f.item.targetCards.size()) &&
        f.item.targetCard && !selected) {
        const auto id = f.item.targetCard;
        const auto target = s.cards.find(id);
        if (target != s.cards.end() && fieldZone(target->second.zone)) {
            f.resolvedCount += fieldTreeSize(s, id);
            StateMaintenance::leave(s, id, &catalog_);
        }
        f.item.targetCard = 0;
        ++f.clearRemaining;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::StormGuidance) {
        if (f.clearRemaining == -1) {
            StateMaintenance::leave(s, selected, &catalog_);
            f.clearRemaining = -2;
            return true;
        }
        if (f.clearRemaining == -2) {
            if (selected == 0xfffffff1u || selected == 0xfffffff3u) {
                const bool stormSearch = selected == 0xfffffff1u;
                std::vector<CardId> cards;
                for (const auto id : s.players[actor].deck) {
                    const auto &definition = catalog_.at(s.cards.at(id).definition);
                    const bool isStormSpell = isSpell(definition) &&
                        std::find(definition.tags.begin(), definition.tags.end(), "风暴") != definition.tags.end();
                    const bool isAuxiliary = definition.id == "cloud-mist" || definition.id == "thunder-summon";
                    if (stormSearch ? isStormSpell : isAuxiliary) cards.push_back(id);
                }
                if (cards.empty()) {
                    ++f.cursor;
                    f.clearRemaining = -1;
                } else {
                    f.clearRemaining = stormSearch ? -3 : -4;
                    decision(actor, DecisionKind::EffectChoice, std::move(cards));
                }
                return true;
            }
            if (selected == 0xfffffff2u) {
                for (auto &[id, formation] : s.cards)
                    if (formation.owner == actor && formation.zone == Zone::Analysis &&
                        catalog_.at(formation.definition).type == CardType::Formation &&
                        std::find(catalog_.at(formation.definition).tags.begin(),
                                  catalog_.at(formation.definition).tags.end(), "风暴") !=
                            catalog_.at(formation.definition).tags.end()) {
                        formation.stormPoints += 2;
                        break;
                    }
                ++f.cursor;
                f.clearRemaining = -1;
                return true;
            }
            return false;
        }
        if (f.clearRemaining == -3 || f.clearRemaining == -4) {
            auto &deck = s.players[actor].deck;
            deck.erase(std::remove(deck.begin(), deck.end(), selected), deck.end());
            auto &card = s.cards.at(selected);
            card.zone = Zone::Hand;
            shuffleDeck(s, actor);
            s.events.push_back({"search", "风暴导向：法术加入手牌", actor, selected});
            ++f.cursor;
            f.clearRemaining = -1;
            return true;
        }
    }
    if (!selected) {
        ++f.cursor;
        f.clearRemaining = -1;
        return true;
    }
    if (kind == DecisionKind::DamageChoice) {
        Effect damage{EffectKind::Damage, e.amount, static_cast<DamageType>(selected - 1)};
        EffectResolver::apply(s, catalog_, f.item, damage);
        if (s.effect)
            ++s.effect->cursor;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::StormCyclone) {
        int destroyed = 0;
        std::vector<CardId> spells;
        for (const auto &[id, spell] : s.cards) {
            const bool hosted = (spell.zone == Zone::Analysis && spell.host == selected) ||
                                (spell.zone == Zone::Casting && spell.sourceFormation == selected);
            if (hosted && spell.owner != actor && isSpell(catalog_.at(spell.definition)) &&
                !protectedCardFromEffect(s, catalog_, id))
                spells.push_back(id);
        }
        for (const auto id : spells) {
            if (!s.cards.count(id) || !fieldZone(s.cards.at(id).zone)) continue;
            destroyed += fieldTreeSize(s, id);
            StateMaintenance::leave(s, id, &catalog_);
        }
        if (destroyed)
            EffectResolver::apply(s, catalog_, f.item,
                                  {EffectKind::Damage, destroyed * 2, DamageType::Bludgeoning,
                                   EffectRecipient::Opponent});
        ++f.cursor;
        f.clearRemaining = -1;
        return true;
    }
    if (kind == DecisionKind::EffectChoice && e.kind == EffectKind::CuttingWind) {
        const int rank = Rules::level(s, catalog_, selected);
        StateMaintenance::leave(s, selected, &catalog_);
        EffectResolver::apply(s, catalog_, f.item,
                              {EffectKind::Damage, 4, DamageType::Slashing, EffectRecipient::Opponent});
        EffectResolver::apply(s, catalog_, f.item,
                              {EffectKind::Damage, rank, DamageType::Thunder, EffectRecipient::Opponent});
        ++f.cursor;
        f.clearRemaining = -1;
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
    const auto &definition = catalog_.at(card.definition);
    switch (e.kind) {
    case EffectKind::DestroyEnemyAnalysis:
        StateMaintenance::leave(s, selected, &catalog_);
        break;
    case EffectKind::DelayEnemyAnalysis:
        card.analysisProgress = std::max(0, card.analysisProgress - std::max(1, e.amount));
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
        card.analysisStarted = s.players[card.owner].ownTurn;
        break;
    case EffectKind::RevealEnemyAmbush:
        // Showing a card communicates its identity without flipping or starting analysis.
        card.publiclyRevealed = true;
        s.events.push_back({"show_card", "展示盖伏法术：" + definition.name, -1, selected});
        break;
    case EffectKind::ReturnEnemyAnalysis:
        if (source != s.cards.end() && source->second.definition == "swift-wind")
            returnFormationToSideDeck(s, catalog_, selected);
        else
            returnToHand(s, catalog_, selected);
        break;
    case EffectKind::DestroyFormationSpells: {
        std::vector<CardId> spells;
        for (const auto &[id, spell] : s.cards)
            if (isSpell(catalog_.at(spell.definition)) &&
                ((spell.zone == Zone::Analysis && spell.host == selected) ||
                 (spell.zone == Zone::Casting && spell.sourceFormation == selected)))
                spells.push_back(id);
        for (auto id : spells)
            if (!protectedCardFromEffect(s, catalog_, id))
                StateMaintenance::leave(s, id, &catalog_);
        break;
    }
    case EffectKind::SearchSameSchool:
        if (f.clearRemaining < 0) {
            f.clearRemaining = static_cast<int>(selected);
            StateMaintenance::leave(s, selected, &catalog_);
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
        if (f.clearRemaining < 0) {
            f.clearRemaining = static_cast<int>(selected);
            return true;
        } else {
            auto &spell = s.cards.at(static_cast<CardId>(f.clearRemaining));
            spell.owner = actor;
            spell.zone = Zone::Analysis;
            spell.host = selected;
            spell.sourceFormation = selected;
            spell.spell = SpellState::None;
            spell.ringIndex = Rules::freeRing(s, catalog_, selected);
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
    f.clearRemaining = -1;
    StateMaintenance::check(s, catalog_);
    return true;
}
} // namespace wizard
