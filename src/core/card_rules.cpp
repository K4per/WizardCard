#include "wizard/core/card_rules.hpp"
#include "wizard/core/engine.hpp"
#include "engine_internal.hpp"
#include <set>
#include <sstream>
#include <type_traits>

namespace wizard::card_rules {

bool member(const CardDefinition &d, const char *series) {
    return std::find(d.tags.begin(), d.tags.end(), series) != d.tags.end();
}
bool field(const CardInstance &c) {
    return c.zone == Zone::Analysis || c.zone == Zone::Casting || c.zone == Zone::Attached;
}
int lifeLimit(const GameState &s, const CardCatalog &cat, PlayerId p) {

    int modifier = 0;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Attached && hasCapability(cat.at(c.definition), Capability::BloodMark)) {
            const auto host = s.cards.find(c.host);
            if (host != s.cards.end() && host->second.zone == Zone::Analysis &&
                cat.at(host->second.definition).type == CardType::Formation && !effectImmune(s, cat, host->first)) modifier -= 6;
        }
    return std::clamp(s.players[p].lifeLimit + modifier, 1, maximumLife);
}
void normalizeLife(MutationContext &s, const CardCatalog &cat, PlayerId p) {
    s.players[p].life = std::min(s.players[p].life, lifeLimit(s, cat, p));
}
void changeLifeLimit(MutationContext &s, const CardCatalog &cat, PlayerId p, int delta) {
    const int before = lifeLimit(s, cat, p);
    const int after = std::clamp(before + delta, 1, maximumLife);
    s.players[p].lifeLimit += after - before;
    normalizeLife(s, cat, p);
    s.events.push_back({"life_limit", "普通生命上限变更", -1, 0, after - before});
}
bool canPayLife(const GameState &s, PlayerId p, int amount) {
    return amount >= 0 && (s.players[p].debugInfiniteLife || s.players[p].life > amount);
}
int activationLifeCost(const CardDefinition &definition) {
    if (hasCapability(definition, Capability::BloodWristCost)) return 4;
    if (hasCapability(definition, Capability::BloodOfferingCost)) return 1;
    return 0;
}
int attachmentLifeCost(const CardDefinition &definition) {
    return hasCapability(definition, Capability::BloodCurse) ? 2 : 0;
}
void payLife(MutationContext &s, const CardCatalog &cat, PlayerId p, int amount, CardId source) {
    if (amount <= 0 || !canPayLife(s, p, amount)) return;
    if (!s.players[p].debugInfiniteLife) s.players[p].life -= amount;
    s.events.push_back({"life_payment", "支付普通生命", -1, source, amount});
    if (p != s.active || s.players[p].firstLifePaymentTurn == s.globalTurn) return;
    s.players[p].firstLifePaymentTurn = s.globalTurn;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Analysis && hasCapability(cat.at(c.definition), Capability::BloodSacrificeReward))
            ResourceRules::drawOne(s, cat, p);
}
int heal(MutationContext &s, const CardCatalog &cat, PlayerId p, int amount, CardId source) {
    if (amount <= 0 || s.result != -1) return 0;
    const int actual = std::min(amount, std::max(0, lifeLimit(s, cat, p) - s.players[p].life));
    s.players[p].life += actual;
    if (!actual) return 0;
    s.events.push_back({"heal", "恢复普通生命", -1, source, actual});
    std::deque<std::pair<PlayerId, int>> queue{{p, actual}};
    std::set<CardId> visited;
    while (!queue.empty()) {
        const auto [restored, value] = queue.front(); queue.pop_front();
        for (const auto &[id, c] : s.cards) {
            if (c.owner == restored || !hasCapability(cat.at(c.definition), Capability::BloodSameOrigin) || !field(c) ||
                c.spell != SpellState::Active || c.faceDown || visited.count(id)) continue;
            visited.insert(id);
            const int gain = std::min(value, std::max(0, lifeLimit(s, cat, c.owner) - s.players[c.owner].life));
            if (!gain) continue;
            s.players[c.owner].life += gain;
            s.events.push_back({"heal", "血契·同源：恢复生命", -1, id, gain});
            queue.emplace_back(c.owner, gain);
        }
    }
    return actual;
}
int paymentRatio(const GameState &s, const CardCatalog &cat, PlayerId p) {
    int ratio = 0;
    for (const auto &[id, c] : s.cards) {
        if (c.owner != p || c.faceDown) continue;
        if (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation) {
            if (hasCapability(cat.at(c.definition), Capability::BloodAltar) && !ratio) ratio = 2;
            if (hasCapability(cat.at(c.definition), Capability::BloodPool) ||
                hasCapability(cat.at(c.definition), Capability::BloodHeart)) ratio = 1;
        }
        if (c.zone == Zone::Attached && hasCapability(cat.at(c.definition), Capability::BloodVein)) {
            const auto host = s.cards.find(c.host);
            if (host != s.cards.end() && host->second.zone == Zone::Analysis &&
                cat.at(host->second.definition).type == CardType::Formation && !effectImmune(s, cat, host->first)) ratio = 1;
        }
    }
    return ratio;
}
std::vector<PaymentChoice> paymentChoices(const GameState &s, const CardCatalog &cat, PlayerId p, int fee) {
    std::vector<PaymentChoice> out;
    if (fee < 0) return out;
    if (ResourceRules::canPayMana(s, p, fee)) out.push_back({fee, 0, p});
    const int ratio = paymentRatio(s, cat, p);
    if (ratio) for (int mana = std::min(fee - 1, s.players[p].mana); mana >= 0; --mana) {
        const int life = (fee - mana) * ratio;
        if (canPayLife(s, p, life)) out.push_back({mana, life, p});
    }
    if (s.players[p].siqueLifePaymentTurn && s.players[p].siqueLifePaymentTurn == s.globalTurn &&
        canPayLife(s, 1 - p, fee) && fee > 0) out.push_back({0, fee, 1 - p});
    return out;
}
bool canPay(const GameState &s, const CardCatalog &cat, PlayerId p, int fee, const PaymentChoice *selected) {
    if (selected) {
        const auto &choice = *selected;
        const int ratio = choice.lifePlayer == p ? paymentRatio(s, cat, p) : 1;
        return choice.mana + (ratio ? choice.life / ratio : 0) == fee &&
            ResourceRules::canPayMana(s, p, choice.mana) && canPayLife(s, choice.lifePlayer, choice.life);
    }
    return ResourceRules::canPayMana(s, p, fee);
}
void pay(MutationContext &s, const CardCatalog &cat, PlayerId p, int fee) {
    if (!s.paymentInput) { ResourceRules::payMana(s, p, fee); return; }
    const auto choice = *s.paymentInput;
    ResourceRules::payMana(s, p, choice.mana);
    payLife(s, cat, choice.lifePlayer, choice.life);
}
bool damageImmune(const GameState &s, const CardCatalog &cat, PlayerId p) {
    return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &c = entry.second;
        return c.owner == p && hasCapability(cat.at(c.definition), Capability::SevenStarsLamp) && field(c) &&
            !c.faceDown && c.spell == SpellState::Active;
    });
}
void damaged(MutationContext &s, const CardCatalog &cat, PlayerId, CardId source, int amount) {
    const auto card = s.cards.find(source);
    if (amount <= 0 || card == s.cards.end() || !member(cat.at(card->second.definition), "血契")) return;
    for (const auto &[id, c] : s.cards)
        if (c.owner == card->second.owner && hasCapability(cat.at(c.definition), Capability::BloodHeart) &&
            c.zone == Zone::Analysis)
            heal(s, cat, c.owner, amount, id);
}
void expiredLife(MutationContext &s, const CardCatalog &cat, PlayerId p, int amount) {
    if (amount <= 0) return;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Analysis && hasCapability(cat.at(c.definition), Capability::BloodPool))
            heal(s, cat, p, amount, id);
}
void returnCard(MutationContext &s, const CardCatalog &cat, CardId id, bool hand) {
    auto &c = s.cards.at(id);
    const auto owner = c.owner;
    if (field(c)) StateMaintenance::leave(s, id, &cat);
    auto &deck = s.players[owner].deck;
    deck.erase(std::remove(deck.begin(), deck.end(), id), deck.end());
    const bool formation = cat.at(c.definition).type == CardType::Formation;
    c.zone = formation ? Zone::SideDeck : hand ? Zone::Hand : Zone::Deck;
    c.controller = owner;
    c.host = c.sourceFormation = 0;
    c.spell = SpellState::None;
    c.analysisLoad = c.castLoad = c.analysisProgress = c.remaining = 0;
    c.faceDown = c.publiclyRevealed = c.wasAmbushed = false;
    c.ringIndex = c.sourceRingIndex = -1;
    c.stormPoints = 0;
    c.targets.clear(); c.targetCard = 0; c.targetPlayer = -1;
    if (c.zone == Zone::Deck) deck.push_back(id);
    s.events.push_back({formation ? "return_side_deck" : hand ? "return_hand" : "return_deck", "卡牌返回合法区域", -1, id});
}
}

namespace wizard {
namespace {
std::string paymentKey(const Command &command) {
    std::ostringstream out;
    out << command.index() << ':';
    std::visit([&](const auto &c) {
        using T = std::decay_t<decltype(c)>;
        if constexpr (std::is_same_v<T, StartAnalysis> || std::is_same_v<T, PreloadWord> ||
                      std::is_same_v<T, ActivateSpell> || std::is_same_v<T, Respond> || std::is_same_v<T, FlipAmbush>) {
            out << c.card << ',' << c.target << ':';
            for (auto id : c.targets) out << id << ',';
            if constexpr (std::is_same_v<T, StartAnalysis> || std::is_same_v<T, PreloadWord>) out << "h" << c.formation;
            if constexpr (!std::is_same_v<T, StartAnalysis>) out << "d" << c.discard;
            if constexpr (std::is_same_v<T, PreloadWord>) out << "r" << c.release;
            if constexpr (std::is_same_v<T, Respond>) out << c.ability << ',' << c.link << ',' << c.decision;
        }
    }, command);
    return out.str();
}
}
std::optional<PaymentRequest> GameEngine::paymentRequest(PlayerId actor, const Command &command) const {
    const auto &s = state_;

    CardId source = 0, host = 0;
    bool setting = false;
    if (const auto *c = std::get_if<StartAnalysis>(&command)) { source = c->card; host = c->formation; setting = true; }
    if (const auto *c = std::get_if<PreloadWord>(&command)) { source = c->card; host = c->formation; }
    if (const auto *c = std::get_if<ActivateSpell>(&command)) source = c->card;
    if (const auto *c = std::get_if<Respond>(&command)) source = c->card;
    if (const auto *c = std::get_if<FlipAmbush>(&command)) {
        source = c->card;
        if (s.cards.count(source) && catalog().at(s.cards.at(source).definition).type == CardType::Analytic) {
            setting = true; host = s.cards.at(source).host;
        }
    }
    if (!source || !s.cards.count(source) || s.cards.at(source).owner != actor) return std::nullopt;
    const int fee = setting ? Rules::analysisCost(s, catalog(), source, host) : Rules::castCost(s, catalog(), source);
    if (fee <= 0 || (!card_rules::paymentRatio(s, catalog(), actor) &&
        s.players[actor].siqueLifePaymentTurn != s.globalTurn)) return std::nullopt;
    auto choices = card_rules::paymentChoices(s, catalog(), actor, fee);
    if (choices.empty() || (choices.size() == 1 && !choices.front().life)) return std::nullopt;
    return PaymentRequest{PendingPayment{actor, command, s.decision, std::move(choices), paymentKey(command)}};
}
}
