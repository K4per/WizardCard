// 2.0 content choices live beside EffectResolver, separate from turn scheduling.
#include "engine_internal.hpp"

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
} // namespace

bool GameEngine::contentEffectV2() {
    auto &s = state_;
    if (!catalog_.usesAlphaV2() || !s.effect || s.effect->cursor >= s.effect->item.effects.size())
        return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto owner = f.item.owner;
    if (e.kind == EffectKind::SearchSameSchool && f.clearRemaining < 0 && !f.item.targetCards.empty()) {
        const auto &source = catalog_.at(s.cards.at(f.item.source).definition);
        if (hasTrait(source, "discard_spell_search")) f.clearRemaining = static_cast<int>(f.item.targetCards.front());
    }
    if (e.kind == EffectKind::RecoverEnemyAshSpell && f.clearRemaining < 0 && f.item.targetCard)
        f.clearRemaining = static_cast<int>(f.item.targetCard);
    std::vector<CardId> choices;
    bool optional = true;
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
            eligible = card.owner != owner && card.zone == Zone::Analysis &&
                       isSpell(definition) && !protectedFromEffect(definition);
            break;
        case EffectKind::ConcealEnemySpell:
            eligible = card.owner != owner && card.zone == Zone::Analysis && !card.faceDown && isSpell(definition);
            break;
        case EffectKind::RevealEnemyAmbush:
            eligible = card.owner != owner && card.zone == Zone::Analysis && card.faceDown && isSpell(definition);
            break;
        case EffectKind::ReturnEnemyAnalysis:
            eligible = card.owner != owner && card.zone == Zone::Analysis &&
                       isSpell(definition);
            break;
        case EffectKind::DestroyFormationSpells:
            eligible = card.owner != owner && card.zone == Zone::Analysis &&
                       definition.type == CardType::Formation && !hasTrait(definition, "effect_immune");
            if (eligible)
                eligible = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &item) {
                    const auto &spell = item.second;
                    return isSpell(catalog_.at(spell.definition)) &&
                           ((spell.zone == Zone::Analysis && spell.host == id) ||
                            (spell.zone == Zone::Casting && spell.sourceFormation == id));
                });
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
        if (eligible && ((e.kind == EffectKind::RecoverEnemyAshSpell && f.clearRemaining >= 0) || !hasTrait(definition, "effect_immune")))
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
    auto &s = state_;
    if (!catalog_.usesAlphaV2() || !s.effect ||
        (kind != DecisionKind::EffectChoice && kind != DecisionKind::DamageChoice))
        return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
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
            if (!protectedFromEffect(catalog_.at(s.cards.at(id).definition)))
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
