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
    if (!cat.usesCardRules()) return maximumLife;
    int modifier = 0;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Attached && c.definition == "blood-mark") {
            const auto host = s.cards.find(c.host);
            if (host != s.cards.end() && host->second.zone == Zone::Analysis &&
                cat.at(host->second.definition).type == CardType::Formation && !effectImmune(s, cat, host->first)) modifier -= 6;
        }
    return std::clamp(s.players[p].lifeLimit + modifier, 1, maximumLife);
}
void normalizeLife(GameState &s, const CardCatalog &cat, PlayerId p) {
    if (cat.usesCardRules()) s.players[p].life = std::min(s.players[p].life, lifeLimit(s, cat, p));
}
void changeLifeLimit(GameState &s, const CardCatalog &cat, PlayerId p, int delta) {
    const int before = lifeLimit(s, cat, p);
    const int after = std::clamp(before + delta, 1, maximumLife);
    s.players[p].lifeLimit += after - before;
    normalizeLife(s, cat, p);
    s.events.push_back({"life_limit", "普通生命上限变更", -1, 0, after - before});
}
bool canPayLife(const GameState &s, PlayerId p, int amount) {
    return amount >= 0 && (s.players[p].debugInfiniteLife || s.players[p].life > amount);
}
void payLife(GameState &s, const CardCatalog &cat, PlayerId p, int amount, CardId source) {
    if (amount <= 0 || !canPayLife(s, p, amount)) return;
    if (!s.players[p].debugInfiniteLife) s.players[p].life -= amount;
    s.events.push_back({"life_payment", "支付普通生命", -1, source, amount});
    if (p != s.active || s.players[p].firstLifePaymentTurn == s.globalTurn) return;
    s.players[p].firstLifePaymentTurn = s.globalTurn;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Analysis && c.definition == "blood-sacrificial-table")
            ResourceRules::drawOne(s, cat, p);
}
int heal(GameState &s, const CardCatalog &cat, PlayerId p, int amount, CardId source) {
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
            if (c.owner == restored || c.definition != "blood-same-origin" || !field(c) ||
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
            if (c.definition == "blood-altar" && !ratio) ratio = 2;
            if (c.definition == "blood-pool" || c.definition == "blood-heart") ratio = 1;
        }
        if (c.zone == Zone::Attached && c.definition == "blood-vein") {
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
bool canPay(const GameState &s, const CardCatalog &cat, PlayerId p, int fee) {
    if (s.selectedPayment) {
        const auto &choice = *s.selectedPayment;
        const int ratio = choice.lifePlayer == p ? paymentRatio(s, cat, p) : 1;
        return choice.mana + (ratio ? choice.life / ratio : 0) == fee &&
            ResourceRules::canPayMana(s, p, choice.mana) && canPayLife(s, choice.lifePlayer, choice.life);
    }
    return ResourceRules::canPayMana(s, p, fee);
}
void pay(GameState &s, const CardCatalog &cat, PlayerId p, int fee) {
    if (!s.selectedPayment) { ResourceRules::payMana(s, p, fee); return; }
    const auto choice = *s.selectedPayment;
    ResourceRules::payMana(s, p, choice.mana);
    payLife(s, cat, choice.lifePlayer, choice.life);
}
bool damageImmune(const GameState &s, const CardCatalog &cat, PlayerId p) {
    return cat.usesCardRules() && std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &c = entry.second;
        return c.owner == p && c.definition == "qixing-lamp" && field(c) && !c.faceDown && c.spell == SpellState::Active;
    });
}
void damaged(GameState &s, const CardCatalog &cat, PlayerId, CardId source, int amount) {
    const auto card = s.cards.find(source);
    if (!cat.usesCardRules() || amount <= 0 || card == s.cards.end() || !member(cat.at(card->second.definition), "血契")) return;
    for (const auto &[id, c] : s.cards)
        if (c.owner == card->second.owner && c.definition == "blood-heart" && c.zone == Zone::Analysis)
            heal(s, cat, c.owner, amount, id);
}
void expiredLife(GameState &s, const CardCatalog &cat, PlayerId p, int amount) {
    if (amount <= 0 || !cat.usesCardRules()) return;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Analysis && c.definition == "blood-pool") heal(s, cat, p, amount, id);
}
void returnCard(GameState &s, const CardCatalog &cat, CardId id, bool hand) {
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
std::optional<std::string> GameEngine::paymentForCardRules(PlayerId actor, const Command &command) {
    auto &s = state_;
    if (!catalog_.usesCardRules() || probingPayment_ || s.selectedPayment) return std::nullopt;
    if (s.payment) {
        const auto *choose = std::get_if<Choose>(&command);
        if (!choose || !s.decision || choose->decision != s.decision->id || actor != s.payment->player)
            return "payment_pending: 请先完成费用选择";
        auto pending = *s.payment;
        if (choose->option > pending.choices.size()) return "invalid_option";
        s.payment.reset(); s.decision = pending.previousDecision;
        if (!choose->option) return std::string{};
        s.selectedPayment = pending.choices[choose->option - 1];
        auto result = execute(actor, pending.command);
        s.selectedPayment.reset();
        return result;
    }
    CardId source = 0, host = 0;
    bool setting = false;
    if (const auto *c = std::get_if<StartAnalysis>(&command)) { source = c->card; host = c->formation; setting = true; }
    if (const auto *c = std::get_if<PreloadWord>(&command)) { source = c->card; host = c->formation; }
    if (const auto *c = std::get_if<ActivateSpell>(&command)) source = c->card;
    if (const auto *c = std::get_if<Respond>(&command)) source = c->card;
    if (const auto *c = std::get_if<FlipAmbush>(&command)) {
        source = c->card;
        if (s.cards.count(source) && catalog_.at(s.cards.at(source).definition).type == CardType::Analytic) {
            setting = true; host = s.cards.at(source).host;
        }
    }
    if (!source || !s.cards.count(source) || s.cards.at(source).owner != actor) return std::nullopt;
    const int fee = setting ? Rules::analysisCost(s, catalog_, source, host) : Rules::castCost(s, catalog_, source);
    if (fee <= 0 || (!card_rules::paymentRatio(s, catalog_, actor) &&
        s.players[actor].siqueLifePaymentTurn != s.globalTurn)) return std::nullopt;
    auto choices = card_rules::paymentChoices(s, catalog_, actor, fee);
    std::vector<PaymentChoice> valid;
    std::string error = "cost: 无可用支付方式";
    for (const auto &choice : choices) {
        auto trial = *this; trial.probingPayment_ = true; trial.state_.selectedPayment = choice;
        const auto result = trial.execute(actor, command);
        if (result.empty()) valid.push_back(choice);
        else error = result;
    }
    if (valid.empty()) return error;
    if (valid.size() == 1 && !valid.front().life) return std::nullopt;
    s.payment = PendingPayment{actor, command, s.decision, std::move(valid), paymentKey(command)};
    std::vector<std::uint32_t> options;
    for (std::uint32_t i = 1; i <= s.payment->choices.size(); ++i) options.push_back(i);
    decision(actor, DecisionKind::Payment, std::move(options), true);
    return std::string{};
}
}
