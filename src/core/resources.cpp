#include "wizard/core/rules.hpp"
#include "wizard/core/sique.hpp"
#include "wizard/core/arcana.hpp"
#include "wizard/core/engine.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
namespace {
bool effectImmune(const CardDefinition &definition) {
    return std::find(definition.traits.begin(), definition.traits.end(), "effect_immune") != definition.traits.end();
}
}
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
            const auto &definition = cat.at(c.definition);
            int capacity = definition.capacity;
            if (definition.id == "storm-crest")
                capacity = c.stormPoints >= 5 ? 10 : c.stormPoints >= 3 ? 8 : 5;
            n += capacity;
            for (const auto &[sid, seal] : s.cards)
                if (!effectImmune(cat.at(c.definition)) && seal.zone == Zone::Attached && seal.host == id)
                    n += cat.at(seal.definition).capacityBonus;
        }
    return n;
}
int Rules::income(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &card = s.cards.at(id);
    const auto &definition = cat.at(card.definition);
    if (definition.id == "storm-crest")
        return card.stormPoints >= 5 ? 2 : 1;
    return definition.income;
}
int Rules::level(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &card = s.cards.at(id);
    const auto &definition = cat.at(card.definition);
    if (definition.id == "storm-crest")
        return card.stormPoints >= 5 ? 3 : card.stormPoints >= 3 ? 2 : 1;
    return cat.usesAlphaV2() ? definition.level : definition.maxRank;
}
int Rules::rings(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &card = s.cards.at(id);
    const auto &definition = cat.at(card.definition);
    int n = definition.rings;
    if (definition.id == "storm-crest")
        n = card.stormPoints >= 5 ? 3 : card.stormPoints >= 3 ? 2 : 1;
    for (const auto &[cid, c] : s.cards)
        if (!effectImmune(cat.at(s.cards.at(id).definition)) && c.zone == Zone::Attached && c.host == id)
            n += cat.at(c.definition).ringBonus;
    return n;
}
int Rules::maxRank(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &definition = cat.at(s.cards.at(id).definition);
    int n = level(s, cat, id);
    for (const auto &[cid, c] : s.cards)
        if (!effectImmune(definition) && c.zone == Zone::Attached && c.host == id)
            n += cat.at(c.definition).rankBonus;
    return n;
}
int Rules::analysisCost(const GameState &s, const CardCatalog &cat, CardId id, CardId host) {
    const auto &d = cat.at(s.cards.at(id).definition);
    const auto h = s.cards.find(host);
    int discount = sique::attachedCount(s, s.cards.at(id).owner, "sique-sugar");
    if (cat.usesAlphaV2() && std::find(d.traits.begin(), d.traits.end(), "ash_evocation_discount") != d.traits.end())
        for (const auto &[cid, card] : s.cards) {
            const auto &definition = cat.at(card.definition);
            if (card.owner == s.cards.at(id).owner && card.zone == Zone::Ash &&
                definition.school == "evocation" && (definition.type == CardType::Analytic ||
                    definition.type == CardType::Word || definition.type == CardType::Talisman)) ++discount;
        }
    int surcharge = 0;
    if (cat.usesAlphaV252() && d.duration > 0)
        for (const auto &[sourceId, source] : s.cards)
            if (source.owner != s.cards.at(id).owner && source.spell == SpellState::Active &&
                (source.zone == Zone::Analysis || source.zone == Zone::Casting)) {
                const auto &traits = cat.at(source.definition).traits;
                if (std::find(traits.begin(), traits.end(), "surcharge_enemy_timed_cast") != traits.end()) surcharge += 2;
            }
    return std::max(0, d.cost + surcharge - discount - (h != s.cards.end() && h->second.base && cat.at(h->second.definition).baseAnalysisDiscount &&
                                         (d.school == "evocation" || d.school == "conjuration")
                                     ? 1
                                     : 0));
}
int Rules::castCost(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &c = s.cards.at(id);
    const auto &d = cat.at(c.definition);
    int n = (d.type == CardType::Word || (cat.usesAlphaV2() && d.type == CardType::Talisman) ? d.cost : d.castCost)
        - sique::attachedCount(s, c.owner, "sique-sugar");
    for (const auto &[sid, seal] : s.cards)
        if (seal.zone == Zone::Attached && seal.host == id)
            n += cat.at(seal.definition).castCostBonus;
    if (!cat.usesAlphaV252() && d.duration > 0)
        for (const auto &[sourceId, source] : s.cards) {
            const auto &sourceDefinition = cat.at(source.definition);
            if (source.owner != c.owner && source.spell == SpellState::Active &&
                (source.zone == Zone::Analysis || source.zone == Zone::Casting) &&
                std::find(sourceDefinition.traits.begin(), sourceDefinition.traits.end(),
                          "surcharge_enemy_timed_cast") != sourceDefinition.traits.end())
                n += 2;
        }
    return std::max(0, n);
}
int Rules::concentrationUpkeep(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto &card = s.cards.at(id);
    const bool doubled = std::any_of(s.cards.begin(), s.cards.end(), [&](const auto &entry) {
        const auto &source = entry.second;
        const auto &definition = cat.at(source.definition);
        return source.owner != card.owner && source.spell == SpellState::Active &&
            (source.zone == Zone::Analysis || source.zone == Zone::Casting) &&
            std::find(definition.traits.begin(), definition.traits.end(),
                      "double_enemy_concentration_upkeep") != definition.traits.end();
    });
    return card.concentrationCost * (doubled ? 2 : 1);
}
int Rules::damage(const GameState &s, const CardCatalog &cat, PlayerId p, const Effect &e) {
    if (s.players[p].debugInfiniteLife) return 0;
    if (cat.usesAlphaV252() && s.players[p].nextDamageImmune) return 0;
    bool resistance = false;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && (c.zone == Zone::Casting || (cat.usesAlphaV2() && c.zone == Zone::Analysis)) && c.spell == SpellState::Active) {
            const auto &d = cat.at(c.definition);
            if (std::find(d.immunities.begin(), d.immunities.end(), e.damageType) != d.immunities.end())
                return 0;
            resistance |=
                std::find(d.resistances.begin(), d.resistances.end(), e.damageType) != d.resistances.end();
        }
    const int base = arcana::active(s, cat, "arcana_strength") ? e.amount * 2 : e.amount;
    const int resisted = resistance ? base / 2 : base;
    const int reduction = e.damageType == DamageType::Force ? s.players[p].forceDamageReduction : s.players[p].damageReduction;
    const int amount = std::max(0, resisted - reduction - (cat.usesAlphaV252() ? 0 : 2 * sique::attachedCount(s, p, "sique-vinegar")));
    return s.players[p].siqueDamageCap > 0 ? std::min(amount, s.players[p].siqueDamageCap) : amount;
}
int Rules::occupied(const GameState &s, CardId host) {
    int n = 0;
    for (const auto &[id, c] : s.cards)
        if (c.host == host && c.zone == Zone::Analysis)
            ++n;
    return n;
}
int Rules::castingCapacity(const GameState &s, const CardCatalog &cat, PlayerId player) {
    int count = 0;
    for (const auto &[id, c] : s.cards)
        if (c.owner == player && c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation)
            count += rings(s, cat, id);
    return count;
}
int Rules::freeRing(const GameState &s, const CardCatalog &cat, CardId host) {
    for (int ring = 0; ring < rings(s, cat, host); ++ring) {
        bool occupied = false;
        for (const auto &[id, c] : s.cards)
            occupied |= c.zone == Zone::Analysis && c.host == host && c.ringIndex == ring;
        if (!occupied)
            return ring;
    }
    return -1;
}
bool Rules::canHost(const GameState &s, const CardCatalog &cat, CardId spell, CardId formation, bool faceDown) {
    const auto source = s.cards.find(spell), host = s.cards.find(formation);
    if (source == s.cards.end() || host == s.cards.end() || source->second.owner != host->second.owner ||
        host->second.zone != Zone::Analysis || host->second.faceDown ||
        cat.at(host->second.definition).type != CardType::Formation) return false;
    const auto &definition = cat.at(source->second.definition);
    if (cat.usesAlphaV252() && std::find(definition.traits.begin(), definition.traits.end(), "unique") != definition.traits.end())
        for (const auto &[id, card] : s.cards)
            if (id != spell && card.owner == source->second.owner && card.definition == source->second.definition &&
                (card.zone == Zone::Analysis || card.zone == Zone::Casting)) return false;
    if ((!faceDown || !cat.usesAlphaV253()) && definition.rank > maxRank(s, cat, formation)) return false;
    if (faceDown && cat.usesAlphaV253()) return true; // Restrictions apply when flipped.
    if (cat.usesAlphaV252() && std::find(definition.traits.begin(), definition.traits.end(), "sique_campfire_only") != definition.traits.end() &&
        host->second.definition != "sique-campfire") return false;
    const auto &traits = cat.at(host->second.definition).traits;
    if (std::find(traits.begin(), traits.end(), "host_fire_only") != traits.end() &&
        !sique::fireSpell(definition)) return false;
    if (std::find(traits.begin(), traits.end(), "distinct_host_schools") != traits.end())
        for (const auto &[id, card] : s.cards)
            if (id != spell && card.host == formation && card.zone == Zone::Analysis && !card.faceDown &&
                cat.at(card.definition).school == definition.school) return false;
    return true;
}
bool Rules::targetValid(const GameState &s, const CardDefinition &d, PlayerId p, CardId target) {
    return targetValid(s, d.target, p, target);
}
bool Rules::targetValid(const GameState &s, TargetKind kind, PlayerId p, CardId target) {
    if (kind == TargetKind::Player) return target == playerTarget(0) || target == playerTarget(1);
    if (kind == TargetKind::PendingLink || kind == TargetKind::PreparationRoot ||
        kind == TargetKind::EnemySpellLink)
        return false;
    if (kind == TargetKind::None || kind == TargetKind::Self || kind == TargetKind::Opponent)
        return target == 0;
    auto i = s.cards.find(target);
    if (i == s.cards.end())
        return false;
    const auto &c = i->second;
    if (kind == TargetKind::EnemyAshCard) return c.owner != p && c.zone == Zone::Ash && !c.faceDown;
    if ((c.base && !c.fieldOrder) || c.zone == Zone::Deck || c.zone == Zone::Hand || c.zone == Zone::Ash ||
        c.zone == Zone::SideDeck || c.zone == Zone::Exile)
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
    for (const auto &l : s.chain->links) {
        if (l.id == id) {
            if (l.kind == LinkKind::Setup)
                return !l.canceled && !l.sourceLost && kind == TargetKind::EnemySpellLink &&
                    (!s.effect || s.effect->link != id);
            return !l.canceled && l.kind != LinkKind::Phase && (!s.effect || s.effect->link != id) &&
                   (kind == TargetKind::PendingLink ||
                    (kind == TargetKind::PreparationRoot && l.kind == LinkKind::Preparation) ||
                    (kind == TargetKind::EnemySpellLink &&
                     (l.kind == LinkKind::Spell || l.kind == LinkKind::Response ||
                      (s.chain->window == ResponseWindow::Prepare && l.kind == LinkKind::Preparation))));
        }
    }
    return false;
}
std::vector<std::string> Rules::deckErrors(const CardCatalog &cat, const PlayerDeck &deck) {
    std::vector<std::string> out;
    if (cat.usesAlphaV2()) {
        if (deck.cards.size() != 40) out.push_back("main deck must contain exactly 40 cards");
        if (deck.sideDeck.size() != 10) out.push_back("side deck must contain exactly 10 formations");
        for (bool side : {false, true}) {
            std::map<std::string, int> counts;
            for (const auto &id : side ? deck.sideDeck : deck.cards) {
                const auto it = cat.cards.find(id);
                if (it == cat.cards.end()) { out.push_back("unknown card: " + id); continue; }
                const auto &d = it->second;
                if ((d.type == CardType::Formation) != side || d.type == CardType::Action)
                    out.push_back("card is not legal in its deck: " + id);
                if (++counts[d.name] > 3) out.push_back("at most 3 cards per name in each deck");
            }
        }
        if (cat.usesAlphaV25()) {
            if (std::none_of(deck.sideDeck.begin(), deck.sideDeck.end(), [&](const std::string &id) {
                const auto card = cat.cards.find(id);
                return card != cat.cards.end() && card->second.type == CardType::Formation && card->second.level == 1;
            })) out.push_back("side deck must contain a level 1 formation");
        } else {
            const auto initial = cat.cards.find(deck.baseFormation);
            if (initial == cat.cards.end() || initial->second.type != CardType::Formation ||
                initial->second.level != 1 || !initial->second.baseEligible ||
                std::find(deck.sideDeck.begin(), deck.sideDeck.end(), deck.baseFormation) == deck.sideDeck.end())
                out.push_back("initial formation must be an eligible level 1 entity in the side deck");
        }
        return out;
    }
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
