#include "wizard/core/spell_rules.hpp"
#include "wizard/core/card_rules.hpp"
#include <algorithm>

namespace wizard::spell_rules {
bool active(const CardInstance &c) {
    return !c.faceDown && c.spell == SpellState::Active && (c.zone == Zone::Analysis || c.zone == Zone::Casting);
}
bool blocked(const GameState &s, ModifierKind kind, PlayerId p) {
    return std::any_of(s.modifiers.begin(), s.modifiers.end(), [&](const SpellModifier &m) {
        return m.kind == kind && (m.player < 0 || m.player == p);
    });
}
int loadMultiplier(const GameState &s, PlayerId p) {
    int result = 1;
    for (const auto &m : s.modifiers)
        if (m.kind == ModifierKind::DoubleLoad && m.player == p) result *= m.amount;
    return result;
}
void addModifier(MutationContext &s, const Trigger &t, ModifierKind kind, ModifierExpiry expiry,
                 int amount, PlayerId player, CardId target, DamageType type) {
    SpellModifier m;
    m.kind = kind; m.source = t.source; m.owner = t.owner; m.player = player;
    m.target = target; m.amount = amount; m.damageType = type; m.expiry = expiry;
    m.expiresTurn = s.globalTurn + (expiry == ModifierExpiry::NextTurnEnd ? 1 : 0);
    m.expiresOwnTurn = s.players[t.owner].ownTurn + 1;
    s.modifiers.push_back(m);
}
namespace {
void enqueue(MutationContext &s, CardId source, PlayerId owner, PlayerId target, Effect effect) {
    Trigger t; t.id = s.nextTrigger++; t.batch = s.nextBatch++; t.source = source;
    t.owner = owner; t.targetPlayer = target; t.effects = {effect}; s.queue.append(std::move(t));
}
void returnBorrowed(MutationContext &s, const CardCatalog &cat, const SpellModifier &m) {
    auto found = s.cards.find(m.target);
    if (found == s.cards.end() || (found->second.zone != Zone::Analysis && found->second.zone != Zone::Casting)) return;
    auto &c = found->second;
    // An effect cannot return a card that has already left and been played again.
    if (controllerOf(c) != m.owner) return;
    c.controller = m.returnController;
    for (auto &[id, child] : s.cards)
        if (child.host == c.id && child.zone == Zone::Attached) child.controller = m.returnController;
    CardId host = 0;
    if (s.cards.count(m.returnHost) && Rules::canHost(s, cat, c.id, m.returnHost, c.faceDown) &&
        Rules::freeRing(s, cat, m.returnHost) >= 0) host = m.returnHost;
    if (!host) for (const auto &[id, candidate] : s.cards)
        if (controllerOf(candidate) == controllerOf(c) && Rules::canHost(s, cat, c.id, id, c.faceDown) &&
            Rules::freeRing(s, cat, id) >= 0) { host = id; break; }
    if (!host) { StateMaintenance::leave(s, c.id, &cat); return; }
    int ring = Rules::freeRing(s, cat, host);
    if (host == m.returnHost && m.returnRing >= 0 && m.returnRing < Rules::rings(s, cat, host) &&
        std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
            return entry.first != c.id && entry.second.host == host && entry.second.zone == Zone::Analysis &&
                entry.second.ringIndex == m.returnRing;
        })) ring = m.returnRing;
    c.zone = Zone::Analysis; c.host = c.sourceFormation = host; c.ringIndex = c.sourceRingIndex = ring;
    s.events.push_back({"control_return", "法师之手：归还法术", -1, c.id});
}
}
void expire(MutationContext &s, const CardCatalog &cat, bool end) {
    std::vector<SpellModifier> due;
    s.modifiers.erase(std::remove_if(s.modifiers.begin(), s.modifiers.end(), [&](const SpellModifier &m) {
        bool expires = false;
        if (m.expiry == ModifierExpiry::TurnEnd || m.expiry == ModifierExpiry::NextTurnEnd)
            expires = end && s.phase == Phase::End && s.globalTurn >= m.expiresTurn;
        else if (s.active == m.owner && s.players[m.owner].ownTurn >= m.expiresOwnTurn) {
            if (m.expiry == ModifierExpiry::NextDraw) expires = !end && s.phase == Phase::Draw;
            if (m.expiry == ModifierExpiry::NextPrepare) expires = !end && s.phase == Phase::Prepare;
            if (m.expiry == ModifierExpiry::NextPrepareEnd) expires = end && s.phase == Phase::Prepare;
            if (m.expiry == ModifierExpiry::NextMain) expires = !end && s.phase == Phase::Main;
        }
        if (expires) due.push_back(m);
        return expires;
    }), s.modifiers.end());
    for (const auto &m : due) if (m.kind == ModifierKind::BorrowControl) returnBorrowed(s, cat, m);
}
void setup(MutationContext &s, const CardCatalog &cat, CardId id, bool ambush) {
    auto &c = s.cards.at(id); const auto p = controllerOf(c);
    if (!ambush && cat.at(c.definition).type == CardType::Analytic) {
        for (const auto &m : s.modifiers)
            if (m.kind == ModifierKind::SetupDelay && m.player == p) c.analysisProgress -= m.amount;
        if (c.analysisProgress < cat.at(c.definition).analysisTurns) c.spell = SpellState::Analyzing;
    }
    for (const auto &[source, watcher] : s.cards)
        if (controllerOf(watcher) != p && active(watcher) && hasCapability(cat.at(watcher.definition), Capability::SetupDamage))
            enqueue(s, source, controllerOf(watcher), p, {EffectKind::Damage, 2, DamageType::Piercing});
    if (!ambush) for (const auto &m : s.modifiers)
        if (m.kind == ModifierKind::SetupDamage && m.player == p)
            enqueue(s, m.source, m.owner, p, {EffectKind::Damage, m.amount, m.damageType});
}
void wordCast(MutationContext &s, const CardCatalog &cat, CardId id) {
    const auto &c = s.cards.at(id);
    if (cat.at(c.definition).type != CardType::Word) return;
    for (const auto &m : s.modifiers)
        if (m.kind == ModifierKind::WordCastDamage && m.player == controllerOf(c))
            enqueue(s, m.source, m.owner, controllerOf(c), {EffectKind::Damage, m.amount, m.damageType});
}
void damaged(MutationContext &s, const CardCatalog &cat, PlayerId p, const Trigger &cause, int amount, std::optional<DamageType> type) {
    if (amount <= 0) return;
    for (auto &[id, c] : s.cards) {
        if (controllerOf(c) != p || !active(c)) continue;
        const auto &d = cat.at(c.definition);
        if (hasCapability(d, Capability::DamageThreshold)) {
            c.damageCounter += amount;
            const int count = c.damageCounter / 3; c.damageCounter %= 3;
            for (int n = 0; n < count; ++n) enqueue(s, id, p, 1 - p, {EffectKind::Damage, 1, DamageType::Necrotic});
        }
        if (hasCapability(d, Capability::ManaOnElementalDamage) &&
            (type == DamageType::Acid || type == DamageType::Cold || type == DamageType::Fire ||
             type == DamageType::Lightning || type == DamageType::Poison || type == DamageType::Thunder))
            enqueue(s, id, p, p, {EffectKind::GainMana, 1});
        if (hasCapability(d, Capability::RetaliateDamage) && cause.owner != p)
            enqueue(s, id, p, cause.owner, {EffectKind::Damage, 2, DamageType::Psychic});
    }
    const bool mayReflect = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &c = entry.second;
        if (controllerOf(c) != p || (c.zone != Zone::Analysis && c.zone != Zone::Casting)) return false;
        for (const auto &ability : cat.at(c.definition).responses)
            for (const auto &effect : ability.effects)
                if (effect.kind == EffectKind::ReflectDamage) return true;
        return false;
    });
    if (mayReflect) {
        Trigger response; response.id = s.nextTrigger++; response.batch = s.nextBatch++;
        response.owner = p; response.damageAmount = amount;
        s.queue.append(std::move(response));
    }
}
bool preventLethal(MutationContext &s, const CardCatalog &cat, PlayerId p, int amount) {
    if (amount <= 0 || amount < s.players[p].life + s.players[p].temporaryLife) return false;
    for (auto &[id, c] : s.cards)
        if (controllerOf(c) == p && active(c) && hasCapability(cat.at(c.definition), Capability::PreventLethal)) {
            StateMaintenance::leave(s, id, &cat); c.zone = Zone::Exile;
            s.players[p].life = 1;
            s.events.push_back({"death_ward", "防死结界：免疫致命伤害，抹除自身，生命变为1", -1, id});
            return true;
        }
    return false;
}
bool playerTargetAllowed(const GameState &s, const CardCatalog &cat, PlayerId actor, PlayerId target) {
    if (actor == target) return true;
    if (blocked(s, ModifierKind::ProtectPlayer, target)) return false;
    return std::none_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &c = entry.second;
        return controllerOf(c) == target && active(c) && hasCapability(cat.at(c.definition), Capability::Fear);
    });
}
}
