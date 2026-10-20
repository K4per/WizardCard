#include "wizard/core/rules.hpp"
#include "wizard/core/sique.hpp"
#include <algorithm>

namespace wizard {
void ResourceRules::damage(GameState &state, PlayerId player, int amount, const std::string &reason) {
    if (amount <= 0 || state.result != -1)
        return;
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    amount = std::max(0, amount - 2 * sique::attachedCount(state, player, "sique-vinegar"));
    if (owner.siqueDamageCap) amount = std::min(amount, owner.siqueDamageCap);
    const auto shield = std::min(owner.temporaryLife, amount);
    owner.temporaryLife -= shield;
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
        damage(state, player, increment * 2, "overload");
        StateMaintenance::check(state, catalog);
    }
}
void ResourceRules::drawOne(GameState &state, const CardCatalog &catalog, PlayerId player) {
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    if (catalog.usesAlphaV2() && (state.result != -1 || owner.life <= 0))
        return;
    if (owner.deck.empty()) {
        if (catalog.usesAlphaV2())
            damage(state, player, ++owner.exhaustion, "exhaustion");
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
            trigger.effects = {{EffectKind::Damage, 2, DamageType::Fire, EffectRecipient::Opponent}};
            state.queue.append(std::move(trigger));
        }
}
} // namespace wizard
