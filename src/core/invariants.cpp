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
        if (s.players[p].mana < 0 || s.players[p].mana > 12)
            errors.push_back("mana bounds");
        std::map<CardId, int> seen;
        for (auto id : s.players[p].deck) {
            auto i = s.cards.find(id);
            if (++seen[id] != 1 || i == s.cards.end() || i->second.owner != p || i->second.zone != Zone::Deck)
                errors.push_back("deck membership");
        }
        int body = 0, words = 0, concentration = 0;
        for (const auto &[id, c] : s.cards)
            if (c.owner == p) {
                if (c.zone == Zone::Deck && seen[id] != 1)
                    errors.push_back("missing deck card");
                if (c.zone == Zone::Words)
                    ++words;
                if (c.zone == Zone::Casting && c.spell == SpellState::Active &&
                    cat.at(c.definition).concentration)
                    ++concentration;
                if (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation)
                    body += cat.at(c.definition).body;
            }
        if (concentration > 1)
            errors.push_back("multiple concentration spells");
        if (body > 5 || words > 3)
            errors.push_back("zone capacity");
    }
    for (const auto &[id, c] : s.cards) {
        if (c.id != id || c.owner < 0 || c.owner > 1 || c.analysisLoad < 0 || c.castLoad < 0)
            errors.push_back("instance bounds");
        if (c.base && c.zone != Zone::Analysis)
            errors.push_back("base protection");
        if (c.zone == Zone::Ash && (c.analysisLoad || c.castLoad || c.host))
            errors.push_back("ash residue");
        if (c.host) {
            auto h = s.cards.find(c.host);
            if (h == s.cards.end() || h->second.owner != c.owner ||
                (h->second.zone != Zone::Analysis && h->second.zone != Zone::Casting &&
                 h->second.zone != Zone::Words && h->second.zone != Zone::Resolving))
                errors.push_back("host relation");
            else if (c.zone == Zone::Analysis && cat.at(h->second.definition).type != CardType::Formation)
                errors.push_back("analysis host is not formation");
        }
        if ((c.zone == Zone::Attached ||
             (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Analytic)) &&
            !c.host)
            errors.push_back("missing host");
        if (c.zone == Zone::Attached && cat.at(c.definition).type != CardType::Seal)
            errors.push_back("non-seal attachment");
        int seals = 0;
        for (const auto &[other, x] : s.cards)
            if (x.zone == Zone::Attached && x.host == id)
                ++seals;
        if (seals > 1)
            errors.push_back("multiple seals");
        if (c.zone == Zone::Casting && c.host)
            errors.push_back("casting still attached to formation");
    }
    return errors;
}
} // namespace wizard
