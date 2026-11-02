#include "wizard/core/rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
std::vector<std::string> Rules::invariants(const GameState &s, const CardCatalog &cat) {
    std::vector<std::string> errors;
    for (const auto &[id, c] : s.cards)
        if (!cat.cards.count(c.definition)) {
            errors.push_back("unknown definition");
            return errors;
        }
    if (s.active < 0 || s.active > 1 || s.first < 0 || s.first > 1)
        errors.push_back("invalid active player");
    if (s.opening) {
        if (s.globalTurn != 0 || s.chain || s.effect || s.decision || s.phaseGate || !s.queue.items.empty() || !s.deferredPreparations.empty())
            errors.push_back("invalid opening work");
        for (int p = 0; p < 2; ++p) {
            if (s.players[p].ownTurn || s.players[p].deck.size() != 40)
                errors.push_back("opening started too soon");
            const auto selected = s.initialFormations[p];
            if (selected) {
                const auto card = s.cards.find(selected);
                if (card == s.cards.end() || card->second.owner != p || card->second.zone != Zone::SideDeck ||
                    cat.at(card->second.definition).type != CardType::Formation || cat.at(card->second.definition).level != 1)
                    errors.push_back("invalid confirmed initial formation");
            }
            for (const auto &[id, card] : s.cards)
                if (card.owner == p && card.zone != Zone::Deck && card.zone != Zone::SideDeck)
                    errors.push_back("opening card already public");
        }
    }
    if (s.chain) {
        if (s.chain->links.empty() || s.chain->priority < 0 || s.chain->priority > 1 || s.chain->passes < 0 ||
            s.chain->passes > 2)
            errors.push_back("invalid chain");
        std::set<LinkId> ids;
        for (const auto &l : s.chain->links)
            if (!l.id || !ids.insert(l.id).second)
                errors.push_back("duplicate chain link");
    }
    if (s.result != -1 && (s.chain || s.effect || s.decision || !s.queue.items.empty() ||
                           !s.deferredPreparations.empty() || s.phaseGate))
        errors.push_back("terminal work remains");
    for (int p = 0; p < 2; ++p) {
        if (s.players[p].mana < 0 || s.players[p].mana > (maximumMana))
            errors.push_back("mana bounds");
        std::map<CardId, int> seen;
        for (auto id : s.players[p].deck) {
            auto i = s.cards.find(id);
            if (++seen[id] != 1 || i == s.cards.end() || i->second.owner != p || i->second.zone != Zone::Deck)
                errors.push_back("deck membership");
        }
        int body = 0, words = 0, concentration = 0;
        for (const auto &[id, c] : s.cards) {
            if (c.owner == p && c.zone == Zone::Deck && seen[id] != 1)
                errors.push_back("missing deck card");
            if (controllerOf(c) == p) {
                if (c.zone == Zone::Words)
                    ++words;
                if ((c.zone == Zone::Casting || c.zone == Zone::Analysis) && c.spell == SpellState::Active && cat.at(c.definition).concentration)
                    ++concentration;
                if (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation)
                    body += cat.at(c.definition).body;
            }
        }
        if (concentration > 1)
            errors.push_back("multiple concentration spells");
        if (body > 5 || words > 3)
            errors.push_back("zone capacity");
    }
    {
        std::set<std::uint32_t> lifeIds;
        for (const auto &entry : s.temporaryLives)
            if (!lifeIds.insert(entry.id).second || !entry.id || entry.owner < 0 || entry.owner > 1 || entry.amount < 0 || entry.expiryOwnTurn < 0)
                errors.push_back("temporary life bounds");
        for (int p = 0; p < 2; ++p) {
            int total = 0;
            for (const auto &entry : s.temporaryLives)
                if (entry.owner == p) total += entry.amount;
            if (total != s.players[p].temporaryLife)
                errors.push_back("temporary life aggregate");
        }
    }
    for (const auto &[id, c] : s.cards) {
        if (c.id != id || c.owner < 0 || c.owner > 1 || c.controller < -1 || c.controller > 1 ||
            c.analysisLoad < 0 || c.castLoad < 0)
            errors.push_back("instance bounds");

        if (c.zone == Zone::Ash && (c.analysisLoad || c.castLoad || c.host))
            errors.push_back("ash residue");
        if (c.host) {
            auto h = s.cards.find(c.host);
            if (h == s.cards.end() || controllerOf(h->second) != controllerOf(c) ||
                (h->second.zone != Zone::Analysis && h->second.zone != Zone::Casting &&
                 h->second.zone != Zone::Words && h->second.zone != Zone::Resolving))
                errors.push_back("host relation");
            else if (c.zone == Zone::Analysis && cat.at(h->second.definition).type != CardType::Formation)
                errors.push_back("analysis host is not formation");
        }
        if ((c.zone == Zone::Attached || c.zone == Zone::Analysis && (cat.at(c.definition).type == CardType::Analytic || cat.at(c.definition).type != CardType::Formation)) && !c.host)
            errors.push_back("missing host");
        if (c.zone == Zone::Attached && cat.at(c.definition).type != CardType::Seal)
            errors.push_back("non-seal attachment");
        int seals = 0;
        for (const auto &[other, x] : s.cards)
            if (x.zone == Zone::Attached && x.host == id)
                ++seals;

        if (c.zone == Zone::Casting && c.host)
            errors.push_back("casting still attached to formation");
        {
            if (c.zone == Zone::Words || c.zone == Zone::Action || c.zone == Zone::Resolving)
                errors.push_back("obsolete v2 zone");
            if (c.zone == Zone::SideDeck && cat.at(c.definition).type != CardType::Formation)
                errors.push_back("non-formation side card");
            if (c.zone == Zone::Analysis && c.host) {
                if (c.ringIndex < 0) errors.push_back("missing ring index");
                for (const auto &[other, x] : s.cards)
                    if (other != id && x.zone == Zone::Analysis && x.host == c.host && x.ringIndex == c.ringIndex)
                        errors.push_back("duplicate ring index");
            }
        }
    }
    return errors;
}
} // namespace wizard
