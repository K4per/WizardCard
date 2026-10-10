#include "wizard/core/rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
int Rules::load(const GameState &s, PlayerId p) {
    int n = 0;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p)
            n += c.analysisLoad + c.castLoad;
    for (const auto &t : s.temporary)
        if (t.owner == p)
            n += t.amount;
    return n;
}
int Rules::capacity(const GameState &s, const CardCatalog &cat, PlayerId p) {
    int n = cat.baseLoadCapacity;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation) {
            n += cat.at(c.definition).capacity;
            for (const auto &[sid, seal] : s.cards)
                if (seal.zone == Zone::Attached && seal.host == id)
                    n += cat.at(seal.definition).capacityBonus;
        }
    return n;
}
int Rules::rings(const GameState &s, const CardCatalog &cat, CardId id) {
    int n = cat.at(s.cards.at(id).definition).rings;
    for (const auto &[cid, c] : s.cards)
        if (c.zone == Zone::Attached && c.host == id)
            n += cat.at(c.definition).ringBonus;
    return n;
}
int Rules::maxRank(const GameState &s, const CardCatalog &cat, CardId id) {
    int n = cat.at(s.cards.at(id).definition).maxRank;
    for (const auto &[cid, c] : s.cards)
        if (c.zone == Zone::Attached && c.host == id)
            n += cat.at(c.definition).rankBonus;
    return n;
}
int Rules::analysisCost(const GameState &s, const CardCatalog &cat, CardId id, CardId host) {
    const auto &d = cat.at(s.cards.at(id).definition);
    const auto &h = s.cards.at(host);
    return std::max(0, d.cost - (h.base && cat.at(h.definition).baseAnalysisDiscount &&
                                         (d.school == "evocation" || d.school == "conjuration")
                                     ? 1
                                     : 0));
}
int Rules::castCost(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &c = s.cards.at(id);
    const auto &d = cat.at(c.definition);
    int n = d.type == CardType::Word ? d.cost : d.castCost;
    for (const auto &[sid, seal] : s.cards)
        if (seal.zone == Zone::Attached && seal.host == id)
            n += cat.at(seal.definition).castCostBonus;
    return n;
}
int Rules::damage(const GameState &s, const CardCatalog &cat, PlayerId p, const Effect &e) {
    bool resistance = false;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == Zone::Casting && c.spell == SpellState::Active) {
            const auto &d = cat.at(c.definition);
            if (std::find(d.immunities.begin(), d.immunities.end(), e.damageType) != d.immunities.end())
                return 0;
            resistance |=
                std::find(d.resistances.begin(), d.resistances.end(), e.damageType) != d.resistances.end();
        }
    return resistance ? e.amount / 2 : e.amount;
}
int Rules::occupied(const GameState &s, CardId host) {
    int n = 0;
    for (const auto &[id, c] : s.cards)
        if (c.host == host && c.zone == Zone::Analysis)
            ++n;
    return n;
}
bool Rules::targetValid(const GameState &s, const CardDefinition &d, PlayerId p, CardId target) {
    return targetValid(s, d.target, p, target);
}
bool Rules::targetValid(const GameState &s, TargetKind kind, PlayerId p, CardId target) {
    if (kind == TargetKind::PendingLink || kind == TargetKind::PreparationRoot ||
        kind == TargetKind::EnemySpellLink)
        return false;
    if (kind == TargetKind::None || kind == TargetKind::Self || kind == TargetKind::Opponent)
        return target == 0;
    auto i = s.cards.find(target);
    if (i == s.cards.end())
        return false;
    const auto &c = i->second;
    if (c.base || c.zone == Zone::Deck || c.zone == Zone::Hand || c.zone == Zone::Ash)
        return false;
    if (kind == TargetKind::OwnAnalyzingSpell)
        return c.owner == p && c.zone == Zone::Analysis && !c.faceDown && c.spell == SpellState::Analyzing;
    if (kind == TargetKind::EmptyEnemyFormation)
        return c.owner != p && c.zone == Zone::Analysis && c.spell == SpellState::None &&
               occupied(s, target) == 0;
    return kind == TargetKind::AnyCard || (kind == TargetKind::EnemyCard && c.owner != p) ||
           (kind == TargetKind::OwnCard && c.owner == p);
}
bool Rules::linkTargetValid(const GameState &s, TargetKind kind, LinkId id) {
    if (!s.chain || !id)
        return false;
    for (const auto &l : s.chain->links)
        if (l.id == id)
            return !l.canceled && l.kind != LinkKind::Phase && (!s.effect || s.effect->link != id) &&
                   (kind == TargetKind::PendingLink ||
                    (kind == TargetKind::PreparationRoot && l.kind == LinkKind::Preparation) ||
                    (kind == TargetKind::EnemySpellLink &&
                     (l.kind == LinkKind::Spell || l.kind == LinkKind::Response)));
    return false;
}
std::vector<std::string> Rules::deckErrors(const CardCatalog &cat, const PlayerDeck &deck) {
    std::vector<std::string> out;
    if (deck.cards.size() != 30)
        out.push_back("deck must contain exactly 30 cards");
    std::map<std::string, int> counts;
    for (const auto &id : deck.cards) {
        auto i = cat.cards.find(id);
        if (i == cat.cards.end()) {
            out.push_back("unknown card: " + id);
            continue;
        }
        if (++counts[i->second.name] > 3)
            out.push_back("at most 3 cards per name");
    }
    auto i = cat.cards.find(deck.baseFormation);
    if (i == cat.cards.end())
        out.push_back("unknown base formation");
    else {
        const auto &d = i->second;
        if (d.type != CardType::Formation || !d.baseEligible || d.body < 1 || d.body > 5 || d.capacity <= 0 ||
            d.rings <= 0 || d.maxRank <= 0)
            out.push_back("ineligible base formation");
    }
    return out;
}
} // namespace wizard
