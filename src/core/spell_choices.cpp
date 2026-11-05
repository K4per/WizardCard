#include "engine_internal.hpp"
#include "wizard/core/card_rules.hpp"
#include "wizard/core/spell_rules.hpp"

namespace wizard {
namespace {
bool spellChoice(EffectKind k) {
    return k == EffectKind::ChooseResistance || k == EffectKind::BorrowAnalysis || k == EffectKind::InspectEnemyDeck ||
        k == EffectKind::CommandChoice || k == EffectKind::BlessingChoice || k == EffectKind::CalamityChoice ||
        k == EffectKind::HandOrDamage || k == EffectKind::InspectAndRoute || k == EffectKind::InspectEnemyAmbushAll;
}
std::vector<ChoiceValue> modes() {
    return {ChoiceValue::mode(ChoiceMode::SpellFirst), ChoiceValue::mode(ChoiceMode::SpellSecond), ChoiceValue::mode(ChoiceMode::SpellThird)};
}
}
bool GameEngine::resolveSpellChoice() {
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect; const auto e = f.item.effects[f.cursor]; const auto p = f.item.owner;
    if (!spellChoice(e.kind)) return false;
    auto done = [&] { ++f.cursor; f.continuation = RepeatedChoiceContinuation{}; };
    if (e.kind == EffectKind::ChooseResistance) {
        std::vector<ChoiceValue> types;
        for (int n = 0; n <= static_cast<int>(DamageType::Acid); ++n) types.push_back(ChoiceValue::damage(static_cast<DamageType>(n)));
        typedDecision(p, DecisionKind::EffectChoice, std::move(types));
    } else if (e.kind == EffectKind::BlessingChoice || e.kind == EffectKind::CalamityChoice) {
        typedDecision(p, DecisionKind::EffectChoice, modes());
    } else if (e.kind == EffectKind::HandOrDamage) {
        auto hand = detail::inZone(s, 1 - p, Zone::Hand);
        if (hand.empty()) {
            const auto t = f.item; done();
            EffectResolver::apply(s, catalog(), t, {EffectKind::Damage, e.amount, e.damageType, EffectRecipient::Opponent});
        } else decision(1 - p, DecisionKind::EffectChoice, std::move(hand));
    } else if (e.kind == EffectKind::InspectEnemyAmbushAll) {
        typedDecision(p, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::Confirm)}, true);
    } else if (e.kind == EffectKind::InspectAndRoute) {
        auto target = s.cards.find(f.item.targetCard);
        if (target == s.cards.end() || !target->second.faceDown || target->second.zone != Zone::Analysis) done();
        else {
            target->second.publiclyRevealed = true;
            typedDecision(p, DecisionKind::EffectChoice, {ChoiceValue::mode(ChoiceMode::Confirm)});
        }
    } else if (e.kind == EffectKind::BorrowAnalysis) {
        auto target = s.cards.find(f.item.targetCard);
        if (target == s.cards.end() || target->second.zone != Zone::Analysis || !target->second.host || card_rules::effectImmune(s, catalog(), target->first)) done();
        else {
            auto copy = static_cast<GameState>(s); copy.cards.at(target->first).controller = p;
            std::vector<CardId> hosts;
            for (const auto &[id, c] : copy.cards)
                if (Rules::canHost(copy, catalog(), target->first, id, target->second.faceDown) && Rules::freeRing(copy, catalog(), id) >= 0) hosts.push_back(id);
            if (hosts.empty()) done(); else decision(p, DecisionKind::EffectChoice, std::move(hosts));
        }
    } else if (e.kind == EffectKind::InspectEnemyDeck) {
        const auto &deck = s.players[1 - p].deck;
        const auto count = std::min(deck.size(), static_cast<std::size_t>(e.amount));
        if (!count) done(); else decision(p, DecisionKind::EffectChoice, std::vector<CardId>(deck.end() - count, deck.end()));
    } else {
        auto &resume = f.resume<EntityChoiceContinuation>();
        if (!resume.completed) typedDecision(p, DecisionKind::EffectChoice, modes());
        else {
            const bool hand = resume.completed == 3;
            std::vector<CardId> options;
            for (const auto &[id, c] : s.cards)
                if (controllerOf(c) != p && (hand ? c.zone == Zone::Hand : c.zone == Zone::Analysis && c.host)) options.push_back(id);
            if (options.empty()) done(); else decision(hand ? 1 - p : p, DecisionKind::EffectChoice, std::move(options));
        }
    }
    return true;
}
bool GameEngine::chooseSpellChoice(PlayerId, ChoiceValue choice) {
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect; const auto e = f.item.effects[f.cursor]; const auto t = f.item; const auto p = t.owner;
    if (!spellChoice(e.kind)) return false;
    auto done = [&] { ++f.cursor; f.continuation = RepeatedChoiceContinuation{}; };
    const auto selected = static_cast<CardId>(choice.value);
    if (e.kind == EffectKind::ChooseResistance) {
        spell_rules::addModifier(s, t, ModifierKind::HalfDamage, ModifierExpiry::NextPrepare, 1, p, 0,
            static_cast<DamageType>(choice.value - static_cast<unsigned>(ChoiceMode::DamageTypeBase))); done();
    } else if (e.kind == EffectKind::BlessingChoice || e.kind == EffectKind::CalamityChoice) {
        done(); const bool bless = e.kind == EffectKind::BlessingChoice; const int player = bless ? p : 1 - p;
        if (choice == ChoiceValue::mode(ChoiceMode::SpellFirst))
            EffectResolver::apply(s, catalog(), t, {bless ? EffectKind::Heal : EffectKind::Damage, 2, DamageType::Necrotic,
                bless ? EffectRecipient::Owner : EffectRecipient::Opponent});
        else if (choice == ChoiceValue::mode(ChoiceMode::SpellSecond)) {
            if (bless) f.insertBeforeCurrent({EffectKind::ClearAnyLoad, 2, DamageType::Force, EffectRecipient::Owner});
            else {
                const int amount = 2 * spell_rules::loadMultiplier(s, player);
                s.temporary.push_back({s.nextLoad++, player, t.source, amount, s.players[player].ownTurn + (s.active != player), true, false, true});
                ResourceRules::increasedLoad(s, catalog(), player, amount);
            }
        } else if (bless) EffectResolver::apply(s, catalog(), t, {EffectKind::GainMana, 1, DamageType::Force, EffectRecipient::Owner});
        else s.players[player].mana = std::max(0, s.players[player].mana - 1);
    } else if (e.kind == EffectKind::HandOrDamage) { StateMaintenance::leave(s, selected, &catalog()); done();
    } else if (e.kind == EffectKind::InspectEnemyAmbushAll) { done(); if (choice) EffectResolver::apply(s, catalog(), t, e);
    } else if (e.kind == EffectKind::InspectAndRoute) {
        auto &c = s.cards.at(t.targetCard); const bool deck = catalog().at(c.definition).type != CardType::Talisman;
        const auto id = c.id; const auto owner = c.owner; StateMaintenance::leave(s, id, &catalog());
        if (deck) {
            c.zone = Zone::Deck; auto &cards = s.players[owner].deck; cards.push_back(id);
            for (std::size_t n = cards.size(); n > 1; --n) std::swap(cards[n - 1], cards[detail::bounded(s, static_cast<std::uint32_t>(n))]);
        }
        done();
    } else if (e.kind == EffectKind::BorrowAnalysis) {
        auto &c = s.cards.at(t.targetCard); const auto host = c.host; const auto ring = c.ringIndex; const auto controller = controllerOf(c);
        spell_rules::addModifier(s, t, ModifierKind::BorrowControl, ModifierExpiry::NextPrepareEnd, 1, p, c.id);
        auto &m = s.modifiers.back(); m.returnHost = host; m.returnRing = ring; m.returnController = controller;
        c.controller = p; c.host = c.sourceFormation = selected;
        c.ringIndex = c.sourceRingIndex = Rules::freeRing(s, catalog(), selected);
        for (auto &[id, child] : s.cards) if (child.host == c.id && child.zone == Zone::Attached) child.controller = p;
        s.events.push_back({"control_borrow", "法师之手：暂时控制法术", -1, c.id}); done();
    } else if (e.kind == EffectKind::InspectEnemyDeck) {
        auto &deck = s.players[1 - p].deck; const auto count = std::min(deck.size(), static_cast<std::size_t>(e.amount));
        std::vector<CardId> bottom(deck.end() - count, deck.end()); bottom.erase(std::remove(bottom.begin(), bottom.end(), selected), bottom.end());
        StateMaintenance::leave(s, selected, &catalog());
        for (auto id : bottom) deck.erase(std::remove(deck.begin(), deck.end(), id), deck.end());
        deck.insert(deck.begin(), bottom.begin(), bottom.end()); done();
    } else {
        auto &resume = f.resume<EntityChoiceContinuation>();
        if (!resume.completed) {
            resume.completed = static_cast<int>(choice.value - static_cast<unsigned>(ChoiceMode::SpellFirst)) + 1;
            if (resume.completed == 1) { auto &deck = s.players[1 - p].deck; if (!deck.empty()) StateMaintenance::leave(s, deck.back(), &catalog()); done(); }
        } else { StateMaintenance::leave(s, selected, &catalog()); done(); }
    }
    return true;
}
}
