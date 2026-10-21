#include "wizard/core/rules.hpp"
#include "wizard/core/sique.hpp"
#include "wizard/core/arcana.hpp"
#include <algorithm>

namespace wizard {
namespace {
void syncTemporaryLife(GameState &s, PlayerId player) {
    s.players[player].temporaryLife = 0;
    for (const auto &entry : s.temporaryLives)
        if (entry.owner == player) s.players[player].temporaryLife += entry.amount;
}
}
void ResourceRules::normalizeTemporaryLife(GameState &s, PlayerId player) {
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
void ResourceRules::grantTemporaryLife(GameState &s, PlayerId player, CardId source, int amount, int expiry) {
    normalizeTemporaryLife(s, player);
    if (amount > 0)
        s.temporaryLives.push_back({s.nextLoad++, player, source, amount, expiry, s.globalTurn, s.phase});
    syncTemporaryLife(s, player);
}
void ResourceRules::absorbTemporaryLife(GameState &s, PlayerId player, int amount) {
    if (amount <= 0) return;
    // Numeric-only historical states retain their old behavior.
    const bool tracked = std::any_of(s.temporaryLives.begin(), s.temporaryLives.end(),
        [&](const TemporaryLife &entry) { return entry.owner == player; });
    if (!tracked) {
        auto &aggregate = s.players[player].temporaryLife;
        aggregate -= std::min(aggregate, amount);
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
void ResourceRules::clearTemporaryLife(GameState &s, PlayerId player, CardId source) {
    normalizeTemporaryLife(s, player);
    s.temporaryLives.erase(std::remove_if(s.temporaryLives.begin(), s.temporaryLives.end(),
        [&](const TemporaryLife &entry) { return entry.owner == player && (!source || entry.source == source); }),
        s.temporaryLives.end());
    syncTemporaryLife(s, player);
}
void ResourceRules::expireTemporaryLife(GameState &s, PlayerId player) {
    normalizeTemporaryLife(s, player);
    s.temporaryLives.erase(std::remove_if(s.temporaryLives.begin(), s.temporaryLives.end(),
        [&](const TemporaryLife &entry) {
            return entry.owner == player && entry.expiryOwnTurn <= s.players[player].ownTurn;
        }), s.temporaryLives.end());
    syncTemporaryLife(s, player);
}
void ResourceRules::damage(GameState &state, PlayerId player, int amount, const std::string &reason, const CardCatalog *catalog) {
    if (amount <= 0 || state.result != -1)
        return;
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    if (catalog && catalog->usesAlphaV252()) {
        if (owner.nextDamageImmune) { owner.nextDamageImmune = false; amount = 0; }
        else if (arcana::active(state, *catalog, "arcana_strength")) amount *= 2;
    }
    if (!catalog || !catalog->usesAlphaV252())
        amount = std::max(0, amount - 2 * sique::attachedCount(state, player, "sique-vinegar"));
    if (owner.siqueDamageCap) amount = std::min(amount, owner.siqueDamageCap);
    const auto shield = std::min(owner.temporaryLife, amount);
    ResourceRules::absorbTemporaryLife(state, player, shield);
    owner.life -= amount - shield;
    GameEvent event{reason, reason == "overload" ? "过载规则伤害" : "枯竭规则伤害", -1, 0, amount};
    event.actualAmount = amount;
    event.affectedPlayer = player;
    state.events.push_back(std::move(event));
}
void ResourceRules::increasedLoad(GameState &state, const CardCatalog &catalog, PlayerId player, int increment) {
    if (catalog.usesAlphaV2() && (state.players[0].life <= 0 || state.players[1].life <= 0)) {
        StateMaintenance::check(state, catalog);
        return;
    }
    if (catalog.usesAlphaV2() && increment > 0 && Rules::load(state, player) > Rules::capacity(state, catalog, player)) {
        if (!catalog.usesAlphaV252() || !sique::hasFormation(state, player, "moon"))
            damage(state, player, increment * 2, "overload", &catalog);
        StateMaintenance::check(state, catalog);
    }
}
void ResourceRules::drawOne(GameState &state, const CardCatalog &catalog, PlayerId player) {
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    if (catalog.usesAlphaV2() && (state.result != -1 || owner.life <= 0))
        return;
    if (owner.deck.empty()) {
        if (catalog.usesAlphaV2())
            damage(state, player, ++owner.exhaustion, "exhaustion", &catalog);
        else
            owner.drawFailed = true;
        return;
    }
    const auto card = owner.deck.back();
    owner.deck.pop_back();
    state.cards.at(card).zone = Zone::Hand;
    state.events.push_back({"draw", "抽取一张牌", player, card});
    std::uint32_t batch = 0;
    for (const auto &[id, rune] : state.cards)
        if (rune.owner != player && rune.zone == Zone::Attached && rune.definition == "sique-chili") {
            if (!batch) batch = state.nextBatch++;
            Trigger trigger;
            trigger.id = state.nextTrigger++; trigger.batch = batch;
            trigger.owner = rune.owner; trigger.source = id;
            trigger.target = TargetKind::Opponent; trigger.targetPlayer = player;
            trigger.effects = {{EffectKind::Damage, catalog.usesAlphaV252() ? 1 : 2, DamageType::Fire, EffectRecipient::Opponent}};
            state.queue.append(std::move(trigger));
        }
}
} // namespace wizard
