#include "wizard/core/sique.hpp"
#include "wizard/core/engine.hpp"
#include "engine_internal.hpp"
#include <algorithm>
#include <set>

namespace wizard::sique {
bool member(const CardDefinition &d) {
    return std::find(d.tags.begin(), d.tags.end(), "味契") != d.tags.end();
}
bool spell(const CardDefinition &d) {
    return d.type == CardType::Analytic || d.type == CardType::Word || d.type == CardType::Talisman;
}
bool hasFormation(const GameState &s, PlayerId p, const std::string &definition) {
    return std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        return entry.second.owner == p && entry.second.zone == Zone::Analysis &&
               entry.second.definition == definition;
    });
}
bool fireSpell(const CardDefinition &d) {
    const auto fire = [](const Effect &e) {
        return e.kind == EffectKind::Damage && e.damageType == DamageType::Fire;
    };
    return spell(d) && (std::any_of(d.effects.begin(), d.effects.end(), fire) ||
        std::any_of(d.responses.begin(), d.responses.end(), [&](const ResponseAbility &ability) {
            return std::any_of(ability.effects.begin(), ability.effects.end(), fire);
        }));
}
bool activationAllowed(const GameState &s, const CardCatalog &, PlayerId p, const CardDefinition &d) {
    return !hasFormation(s, p, "sique-cauldron") || member(d) || fireSpell(d);
}
bool protectedCampfire(const GameState &s, CardId id) {
    const auto card = s.cards.find(id);
    return card != s.cards.end() && card->second.definition == "sique-campfire" &&
           hasFormation(s, card->second.owner, "sique-cauldron");
}
int attachedCount(const GameState &s, PlayerId p, const std::string &definition) {
    return static_cast<int>(std::count_if(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        return entry.second.owner == p && entry.second.zone == Zone::Attached &&
               entry.second.definition == definition;
    }));
}
int detachCost(const CardDefinition &d) {
    for (int count = 1; count <= 3; ++count)
        if (std::find(d.traits.begin(), d.traits.end(), "sique_detach_" + std::to_string(count)) != d.traits.end())
            return count;
    return 0;
}
bool validDetach(const GameState &s, const CardCatalog &cat, PlayerId p, const CardDefinition &d,
                 const std::vector<CardId> &ids) {
    if (static_cast<int>(ids.size()) != detachCost(d)) return false;
    std::set<CardId> unique;
    for (auto id : ids) {
        auto card = s.cards.find(id);
        if (!unique.insert(id).second || card == s.cards.end() || card->second.owner != p ||
            card->second.zone != Zone::Attached || cat.at(card->second.definition).type != CardType::Seal ||
            !member(cat.at(card->second.definition))) return false;
    }
    return true;
}
std::vector<std::vector<CardId>> detachOptions(const GameState &s, const CardCatalog &cat, PlayerId p,
                                              const CardDefinition &d) {
    std::vector<std::vector<CardId>> out;
    std::vector<CardId> pool, selected;
    const int count = detachCost(d);
    for (const auto &[id, card] : s.cards)
        if (card.owner == p && card.zone == Zone::Attached && cat.at(card.definition).type == CardType::Seal &&
            member(cat.at(card.definition))) pool.push_back(id);
    auto visit = [&](auto &&self, std::size_t start) -> void {
        if (static_cast<int>(selected.size()) == count) { out.push_back(selected); return; }
        for (std::size_t i = start; i < pool.size(); ++i) {
            selected.push_back(pool[i]); self(self, i + 1); selected.pop_back();
        }
    };
    visit(visit, 0);
    return out;
}
bool canPay(const GameState &s, PlayerId p, int amount) {
    return amount >= 0 && ((s.players[p].siqueLifePaymentTurn && s.players[p].siqueLifePaymentTurn == s.globalTurn) || s.players[p].mana >= amount);
}
void pay(GameState &s, PlayerId p, int amount) {
    if (s.players[p].siqueLifePaymentTurn && s.players[p].siqueLifePaymentTurn == s.globalTurn) {
        s.players[1 - p].life -= amount;
        s.events.push_back({"life_payment", "神恩炖肉：以对方生命支付魔素费用", -1, 0, amount});
    } else s.players[p].mana -= amount;
}
void attached(GameState &s, const CardCatalog &cat, CardId id) {
    auto &card = s.cards.at(id);
    if (card.siquePairTurn == s.globalTurn && card.siquePartner) {
        const auto partner = s.cards.find(card.siquePartner);
        if (partner != s.cards.end() && partner->second.zone == Zone::Hand && partner->second.owner == card.owner)
            StateMaintenance::leave(s, partner->first, &cat);
    }
    const auto &def = cat.at(card.definition);
    if (member(def) && !def.effects.empty()) {
        Trigger t;
        t.id = s.nextTrigger++; t.batch = s.nextBatch++;
        t.owner = card.owner; t.source = id; t.effects = def.effects;
        s.queue.append(std::move(t));
    }
}
bool threatensFormation(const GameState &s, const CardCatalog &cat, const ChainLink &link, PlayerId owner) {
    const bool destroys = std::any_of(link.item.effects.begin(), link.item.effects.end(), [](const Effect &e) {
        return e.kind == EffectKind::Destroy || e.kind == EffectKind::DestroyAllEnemyField;
    });
    if (!destroys) return false;
    auto target = [&](CardId id) {
        const auto found = s.cards.find(id);
        return found != s.cards.end() && found->second.owner == owner && found->second.zone == Zone::Analysis &&
            cat.at(found->second.definition).type == CardType::Formation && member(cat.at(found->second.definition));
    };
    if (target(link.item.targetCard) || std::any_of(link.item.targetCards.begin(), link.item.targetCards.end(), target))
        return true;
    const bool broad = std::any_of(link.item.effects.begin(), link.item.effects.end(), [](const Effect &e) {
        return e.kind == EffectKind::DestroyAllEnemyField;
    });
    return broad && std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) { return target(entry.first); });
}
} // namespace wizard::sique

namespace wizard {
namespace {
bool siqueChoice(EffectKind kind) {
    return kind == EffectKind::SiqueRecoverSpell || kind == EffectKind::SiqueRecoverSeal ||
        kind == EffectKind::SiqueMillSeal || kind == EffectKind::SiqueMillSpell ||
        kind == EffectKind::SiqueSearchPair || kind == EffectKind::SiqueAttachFromDeck ||
        kind == EffectKind::SiqueRecycle || kind == EffectKind::SiqueOptionalDraw ||
        kind == EffectKind::SiqueShuffleHands;
}
void shuffleSiqueDeck(GameState &s, PlayerId owner) {
    auto &deck = s.players[owner].deck;
    for (std::size_t size = deck.size(); size > 1; --size)
        std::swap(deck[size - 1], deck[detail::bounded(s, static_cast<std::uint32_t>(size))]);
}
void finishSiqueEffect(EffectFrame &frame) {
    ++frame.cursor; frame.clearRemaining = -1; frame.resolvedCount = 0;
    frame.item.targetCards.clear();
}
} // namespace
bool GameEngine::siqueEffect() {
    auto &s = state_;
    if (!catalog_.usesAlphaV2() || !s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto e = f.item.effects[f.cursor];
    const auto owner = f.item.owner;
    if (e.kind == EffectKind::SiqueDamageCap) {
        s.players[owner].siqueDamageCap = e.amount;
        s.players[owner].siqueCapUntilOwnTurn = s.players[owner].ownTurn + 1;
        finishSiqueEffect(f); return true;
    }
    if (e.kind == EffectKind::SiqueLifePayment) {
        s.players[owner].siqueLifePaymentTurn = s.globalTurn;
        finishSiqueEffect(f); return true;
    }
    if (e.kind == EffectKind::SiqueDetachAll) {
        std::vector<CardId> seals;
        for (const auto &[id, card] : s.cards)
            if (card.zone == Zone::Attached && card.host == f.item.source) seals.push_back(id);
        for (auto id : seals) StateMaintenance::leave(s, id, &catalog_);
        finishSiqueEffect(f); return true;
    }
    if (!siqueChoice(e.kind)) return false;
    if (e.kind == EffectKind::SiqueShuffleHands) {
        if (f.clearRemaining < 0) {
            f.item.targetCards = {0, 0};
            for (auto &[id, card] : s.cards) if (card.zone == Zone::Hand) {
                ++f.item.targetCards[card.owner]; card.zone = Zone::Deck;
                s.players[card.owner].deck.push_back(id);
                card.siquePartner = 0; card.siquePairTurn = 0;
            }
            shuffleSiqueDeck(s, 0); shuffleSiqueDeck(s, 1);
            f.clearRemaining = 0;
        }
        if (f.clearRemaining >= 2) { finishSiqueEffect(f); return true; }
        const auto player = f.clearRemaining;
        if (!f.item.targetCards[player]) { ++f.clearRemaining; return true; }
        decision(player, DecisionKind::EffectChoice, {0xffffffe1u}, true);
        return true;
    }
    if (e.kind == EffectKind::SiqueOptionalDraw) {
        decision(owner, DecisionKind::EffectChoice, {0xffffffe1u}, true); return true;
    }
    std::vector<CardId> choices;
    if (e.kind == EffectKind::SiqueAttachFromDeck && f.clearRemaining >= 0) {
        for (const auto &[id, card] : s.cards) {
            const auto &d = catalog_.at(card.definition);
            const bool occupied = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
                return entry.second.zone == Zone::Attached && entry.second.host == id;
            });
            if (card.owner == owner && card.zone == Zone::Analysis && d.type == CardType::Formation &&
                sique::member(d) && (!occupied || d.id == "sique-cauldron")) choices.push_back(id);
        }
    } else for (const auto &[id, card] : s.cards) {
        const auto &d = catalog_.at(card.definition);
        if (card.owner != owner || !sique::member(d)) continue;
        const bool rune = d.type == CardType::Seal;
        bool eligible = false;
        switch (e.kind) {
        case EffectKind::SiqueRecoverSpell: eligible = card.zone == Zone::Ash && sique::spell(d); break;
        case EffectKind::SiqueRecoverSeal: eligible = card.zone == Zone::Ash && rune; break;
        case EffectKind::SiqueMillSeal:
        case EffectKind::SiqueSearchPair:
        case EffectKind::SiqueAttachFromDeck: eligible = card.zone == Zone::Deck && rune; break;
        case EffectKind::SiqueMillSpell: eligible = card.zone == Zone::Deck && sique::spell(d); break;
        case EffectKind::SiqueRecycle: eligible = card.zone == Zone::Ash; break;
        default: break;
        }
        if (eligible) choices.push_back(id);
    }
    if (e.kind == EffectKind::SiqueSearchPair && f.resolvedCount >= 2) choices.clear();
    if (e.kind == EffectKind::SiqueRecycle && f.resolvedCount >= 5) choices.clear();
    if (choices.empty()) {
        if (e.kind == EffectKind::SiqueSearchPair || e.kind == EffectKind::SiqueAttachFromDeck ||
            e.kind == EffectKind::SiqueMillSeal || e.kind == EffectKind::SiqueMillSpell ||
            e.kind == EffectKind::SiqueRecycle) shuffleSiqueDeck(s, owner);
        finishSiqueEffect(f); return true;
    }
    decision(owner, DecisionKind::EffectChoice, std::move(choices), e.kind == EffectKind::SiqueRecycle);
    return true;
}
bool GameEngine::chooseSiqueEffect(PlayerId actor, CardId selected) {
    auto &s = state_;
    if (!s.effect || s.effect->cursor >= s.effect->item.effects.size()) return false;
    auto &f = *s.effect;
    const auto kind = f.item.effects[f.cursor].kind;
    if (!siqueChoice(kind)) return false;
    if (kind == EffectKind::SiqueShuffleHands) {
        const auto count = f.item.targetCards[static_cast<std::size_t>(f.clearRemaining)];
        ++f.clearRemaining;
        if (selected) for (CardId i = 0; i < count && s.result == -1; ++i) {
            ResourceRules::drawOne(s, catalog_, actor); StateMaintenance::check(s, catalog_);
        }
        return true;
    }
    if (kind == EffectKind::SiqueOptionalDraw) {
        finishSiqueEffect(f);
        if (selected) ResourceRules::drawOne(s, catalog_, actor);
        StateMaintenance::check(s, catalog_); return true;
    }
    if (!selected) { shuffleSiqueDeck(s, actor); finishSiqueEffect(f); return true; }
    auto &card = s.cards.at(selected);
    auto &deck = s.players[actor].deck;
    if (kind == EffectKind::SiqueAttachFromDeck) {
        if (f.clearRemaining < 0) { f.clearRemaining = static_cast<int>(selected); return true; }
        const auto runeId = static_cast<CardId>(f.clearRemaining);
        auto &rune = s.cards.at(runeId);
        deck.erase(std::remove(deck.begin(), deck.end(), runeId), deck.end());
        rune.zone = Zone::Attached; rune.host = selected;
        sique::attached(s, catalog_, runeId);
        shuffleSiqueDeck(s, actor); finishSiqueEffect(f); return true;
    }
    if (kind == EffectKind::SiqueMillSeal || kind == EffectKind::SiqueMillSpell) {
        StateMaintenance::leave(s, selected, &catalog_);
        shuffleSiqueDeck(s, actor); finishSiqueEffect(f); return true;
    }
    if (kind == EffectKind::SiqueRecycle) {
        if (catalog_.at(card.definition).type == CardType::Formation) card.zone = Zone::SideDeck;
        else { card.zone = Zone::Deck; deck.push_back(selected); }
        card.faceDown = false; card.siquePartner = 0; card.siquePairTurn = 0;
        ++f.resolvedCount; return true;
    }
    deck.erase(std::remove(deck.begin(), deck.end(), selected), deck.end());
    card.zone = Zone::Hand; card.faceDown = false;
    s.events.push_back({"sique_recover", "味契卡加入手牌", actor, selected});
    if (kind == EffectKind::SiqueSearchPair) {
        if (!f.item.targetCards.empty()) {
            const auto first = f.item.targetCards.front();
            card.siquePartner = first; card.siquePairTurn = s.globalTurn;
            s.cards.at(first).siquePartner = selected; s.cards.at(first).siquePairTurn = s.globalTurn;
        }
        f.item.targetCards.push_back(selected); ++f.resolvedCount;
        return true;
    }
    finishSiqueEffect(f); return true;
}
} // namespace wizard
