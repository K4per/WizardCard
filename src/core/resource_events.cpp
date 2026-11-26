#include "engine_internal.hpp"
#include "wizard/core/spell_rules.hpp"
#include "wizard/core/card_rules.hpp"
#include <algorithm>

namespace wizard {
namespace {
void resourceDelta(MutationContext &s, PlayerId p, const char *kind, int delta,
                   std::optional<DamageType> damageType = std::nullopt) {
    if (!delta) return;
    GameEvent event{kind, ""};
    event.affectedPlayer = p;
    event.actualAmount = delta;
    event.damageType = damageType;
    s.events.push_back(std::move(event));
}
}
void ResourceRules::setLife(MutationContext &s, PlayerId p, int value, std::optional<DamageType> damageType) {
    resourceDelta(s, p, "life_delta", value - s.players[p].life, damageType);
    s.players[p].life = value;
}
void ResourceRules::gainMana(MutationContext &s, PlayerId p, int amount) {
    const int before = s.players[p].mana;
    s.players[p].mana = std::min(maximumMana, before + amount);
    if (s.players[p].mana > before) resourceDelta(s, p, "mana_gain", s.players[p].mana - before);
}
bool ResourceRules::canPayMana(const GameState &s, PlayerId p, int amount) {
    return amount >= 0 && (s.players[p].debugInfiniteMana || s.players[p].mana >= amount);
}
void ResourceRules::payMana(MutationContext &s, PlayerId p, int amount) {
    if (amount > 0 && !s.players[p].debugInfiniteMana) s.players[p].mana -= amount;
}
namespace {
void syncTemporaryLife(MutationContext &s, PlayerId player) {
    int total = 0;
    for (const auto &entry : s.temporaryLives)
        if (entry.owner == player) total += entry.amount;
    resourceDelta(s, player, "temporary_life_delta", total - s.players[player].temporaryLife);
    s.players[player].temporaryLife = total;
}
}
void ResourceRules::normalizeTemporaryLife(MutationContext &s, PlayerId player) {
    int total = 0;
    for (const auto &entry : s.temporaryLives) {
        s.nextLoad = std::max(s.nextLoad, entry.id + 1);
        if (entry.owner == player) total += entry.amount;
    }
    const int legacy = s.players[player].temporaryLife - total;
    if (legacy > 0)
        s.temporaryLives.push_back({s.nextLoad++, player, 0, legacy, s.players[player].ownTurn + 1,
                                    s.globalTurn, s.phase});
    syncTemporaryLife(s, player);
}
void ResourceRules::grantTemporaryLife(MutationContext &s, PlayerId player, CardId source, int amount, int expiry) {
    normalizeTemporaryLife(s, player);
    if (amount > 0)
        s.temporaryLives.push_back({s.nextLoad++, player, source, amount, expiry, s.globalTurn, s.phase});
    syncTemporaryLife(s, player);
}
void ResourceRules::absorbTemporaryLife(MutationContext &s, PlayerId player, int amount) {
    if (amount <= 0) return;
    // Numeric-only historical states retain their old behavior.
    const bool tracked = std::any_of(s.temporaryLives.begin(), s.temporaryLives.end(),
        [&](const TemporaryLife &entry) { return entry.owner == player; });
    if (!tracked) {
        auto &aggregate = s.players[player].temporaryLife;
        const auto absorbed = std::min(aggregate, amount);
        aggregate -= absorbed;
        resourceDelta(s, player, "temporary_life_delta", -absorbed);
        return;
    }
    normalizeTemporaryLife(s, player);
    // Consume earliest expiry first; stable grant order resolves ties deterministically.
    std::stable_sort(s.temporaryLives.begin(), s.temporaryLives.end(),
        [](const TemporaryLife &a, const TemporaryLife &b) { return a.expiryOwnTurn < b.expiryOwnTurn; });
    for (auto &entry : s.temporaryLives) {
        if (entry.owner != player) continue;
        const int absorbed = std::min(entry.amount, amount);
        entry.amount -= absorbed;
        amount -= absorbed;
        if (!amount) break;
    }
    s.temporaryLives.erase(std::remove_if(s.temporaryLives.begin(), s.temporaryLives.end(),
        [](const TemporaryLife &entry) { return entry.amount == 0; }), s.temporaryLives.end());
    syncTemporaryLife(s, player);
}
void ResourceRules::clearTemporaryLife(MutationContext &s, PlayerId player, CardId source) {
    normalizeTemporaryLife(s, player);
    s.temporaryLives.erase(std::remove_if(s.temporaryLives.begin(), s.temporaryLives.end(),
        [&](const TemporaryLife &entry) { return entry.owner == player && (!source || entry.source == source); }),
        s.temporaryLives.end());
    syncTemporaryLife(s, player);
}
void ResourceRules::expireTemporaryLife(MutationContext &s, PlayerId player) {
    normalizeTemporaryLife(s, player);
    s.temporaryLives.erase(std::remove_if(s.temporaryLives.begin(), s.temporaryLives.end(),
        [&](const TemporaryLife &entry) {
            return entry.owner == player && entry.expiryOwnTurn <= s.players[player].ownTurn;
        }), s.temporaryLives.end());
    syncTemporaryLife(s, player);
}
void ResourceRules::damage(MutationContext &state, PlayerId player, int amount, const std::string &reason, const CardCatalog *catalog) {
    if (amount <= 0 || state.result != -1)
        return;
    if (catalog && card_rules::damageImmune(state, *catalog, player)) return;
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    if (owner.debugInfiniteLife) return;
    const int previousLife = owner.life;
    if (!catalog || catalog->mechanicsRevision < 11) {
        if (owner.nextDamageImmune) { owner.nextDamageImmune = false; amount = 0; }
        else if (catalog && card_rules::hasActiveTrait(state, *catalog, Capability::ArcanaStrength)) amount *= 2;
        if (owner.siqueDamageCap) amount = std::min(amount, owner.siqueDamageCap);
        const auto shield = std::min(owner.temporaryLife, amount);
        ResourceRules::absorbTemporaryLife(state, player, shield);
        setLife(state, player, owner.life - (amount - shield));
    } else setLife(state, player, std::max(0, owner.life - amount));
    GameEvent event{reason, reason == "overload" ? "过载规则伤害" : "枯竭规则伤害", -1, 0, amount};
    event.actualAmount = amount;
    event.affectedPlayer = player;
    state.events.push_back(std::move(event));
    if (catalog && catalog->mechanicsRevision >= 11) {
        Trigger cause; cause.owner = player;
        spell_rules::damaged(state, *catalog, player, cause, std::min(previousLife, amount), std::nullopt);
    }
}
int Rules::overloadDamage(const GameState &state, const CardCatalog &catalog, PlayerId player,
                          int previousLoad, int capacity, int increment) {
    if (increment <= 0 || previousLoad + increment <= capacity || state.players[player].debugInfiniteLoad ||
        state.players[player].debugInfiniteLife || card_rules::hasFormation(state, catalog, player, Capability::Moon) ||
        card_rules::damageImmune(state, catalog, player))
        return 0;
    if (catalog.mechanicsRevision < 11) return 2 * increment;
    return 2 * (previousLoad + increment - capacity - std::max(0, previousLoad - capacity));
}
void ResourceRules::increasedLoad(MutationContext &state, const CardCatalog &catalog, PlayerId player, int increment) {
    if (!state.players[0].debugInfiniteLife && state.players[0].life <= 0 || !state.players[1].debugInfiniteLife && state.players[1].life <= 0) {
        StateMaintenance::check(state, catalog);
        return;
    }
    if (state.players[player].debugInfiniteLoad || increment <= 0) return;
    const int load = Rules::load(state, player), capacity = Rules::capacity(state, catalog, player);
    if (load > capacity) {
        damage(state, player, Rules::overloadDamage(state, catalog, player, load - increment, capacity, increment),
               "overload", &catalog);
        StateMaintenance::check(state, catalog);
    }
}
int ResourceRules::addBoundLoad(MutationContext &s, const CardCatalog &cat, CardId id, int amount, bool casting) {
    auto &c = s.cards.at(id); const auto p = controllerOf(c);
    const int added = amount * spell_rules::loadMultiplier(s, p);
    (casting ? c.castLoad : c.analysisLoad) += added;
    increasedLoad(s, cat, p, added);
    return added;
}
int ResourceRules::grantLoad(MutationContext &s, const CardCatalog &cat, TemporaryLoad load) {
    load.amount *= spell_rules::loadMultiplier(s, load.owner);
    if (load.amount > 0) { s.temporary.push_back(load); increasedLoad(s, cat, load.owner, load.amount); }
    return load.amount;
}
void ResourceRules::drawOne(MutationContext &state, const CardCatalog &catalog, PlayerId player) {
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    if (state.result != -1 || !owner.debugInfiniteLife && owner.life <= 0)
        return;
    if (owner.deck.empty()) {
        damage(state, player, ++owner.exhaustion, "exhaustion", &catalog);
        return;
    }
    const auto card = owner.deck.back();
    owner.deck.pop_back();
    state.cards.at(card).zone = Zone::Hand;
    state.events.push_back({"draw", "抽取一张牌", player, card});
    detail::recordTravel(state, card, {Zone::Deck, Zone::Hand, player, player});
    std::uint32_t batch = 0;
    for (const auto &[id, rune] : state.cards)
        if (rune.owner != player && rune.zone == Zone::Attached &&
            hasCapability(catalog.at(rune.definition), Capability::SiqueChili)) {
            if (!batch) batch = state.nextBatch++;
            Trigger trigger;
            trigger.id = state.nextTrigger++; trigger.batch = batch;
            trigger.owner = rune.owner; trigger.source = id;
            trigger.target = TargetKind::Opponent; trigger.targetPlayer = player;
            trigger.effects = {{EffectKind::Damage, 1, DamageType::Fire, EffectRecipient::Opponent}};
            state.queue.append(std::move(trigger));
        }
}
} // namespace wizard
