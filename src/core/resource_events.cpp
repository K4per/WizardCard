#include "wizard/core/rules.hpp"
#include <algorithm>

namespace wizard {
void ResourceRules::damage(GameState &state, PlayerId player, int amount, const std::string &reason) {
    if (amount <= 0 || state.result != -1)
        return;
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    const auto shield = std::min(owner.temporaryLife, amount);
    owner.temporaryLife -= shield;
    owner.life -= amount - shield;
    GameEvent event{reason, reason == "overload" ? "过载规则伤害" : "枯竭规则伤害", -1, 0, amount};
    event.actualAmount = amount;
    event.affectedPlayer = player;
    state.events.push_back(std::move(event));
}
void ResourceRules::increasedLoad(GameState &state, const CardCatalog &catalog, PlayerId player, int increment) {
    if (catalog.alphaV2Draft && increment > 0 && Rules::load(state, player) > Rules::capacity(state, catalog, player))
        damage(state, player, increment * 2, "overload");
}
void ResourceRules::drawOne(GameState &state, const CardCatalog &catalog, PlayerId player) {
    auto &owner = state.players.at(static_cast<std::size_t>(player));
    if (catalog.alphaV2Draft && (state.result != -1 || owner.life <= 0))
        return;
    if (owner.deck.empty()) {
        if (catalog.alphaV2Draft)
            damage(state, player, ++owner.exhaustion, "exhaustion");
        else
            owner.drawFailed = true;
        return;
    }
    const auto card = owner.deck.back();
    owner.deck.pop_back();
    state.cards.at(card).zone = Zone::Hand;
    state.events.push_back({"draw", "抽取一张牌", player, card});
}
} // namespace wizard
