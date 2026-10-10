#include "engine_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard::detail {
std::uint32_t random32(GameState &s) {
    auto x = s.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return s.rng = x;
}
std::uint32_t bounded(GameState &s, std::uint32_t n) {
    const auto threshold = (std::uint32_t{0} - n) % n;
    std::uint32_t x;
    do {
        x = random32(s);
    } while (x < threshold);
    return x % n;
}
std::vector<CardId> inZone(const GameState &s, PlayerId p, Zone z) {
    std::vector<CardId> out;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == z)
            out.push_back(id);
    return out;
}
int seals(const GameState &s, CardId host) {
    int n = 0;
    for (const auto &[id, c] : s.cards)
        if (c.host == host && c.zone == Zone::Attached)
            ++n;
    return n;
}
int usedBody(const GameState &s, const CardCatalog &cat, PlayerId p) {
    int n = 0;
    for (auto id : inZone(s, p, Zone::Analysis))
        if (cat.at(s.cards.at(id).definition).type == CardType::Formation)
            n += cat.at(s.cards.at(id).definition).body;
    return n;
}
bool hasOverflow(const GameState &s, const CardCatalog &cat) {
    for (const auto &[id, c] : s.cards)
        if (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation &&
            Rules::occupied(s, id) > Rules::rings(s, cat, id))
            return true;
    return false;
}
bool cardTarget(TargetKind t) {
    return t == TargetKind::EmptyEnemyFormation || t == TargetKind::EnemyCard || t == TargetKind::OwnCard ||
           t == TargetKind::AnyCard || t == TargetKind::OwnAnalyzingSpell;
}
std::vector<CardId> targetCards(CardId target, const std::vector<CardId> &targets) {
    return targets.empty() ? (target ? std::vector<CardId>{target} : std::vector<CardId>{}) : targets;
}
bool validTargets(const GameState &s, TargetKind kind, int count, PlayerId p, CardId target,
                  const std::vector<CardId> &targets) {
    auto ids = targetCards(target, targets);
    if (!cardTarget(kind))
        return target == 0 && targets.empty() && kind != TargetKind::PendingLink &&
               kind != TargetKind::PreparationRoot && kind != TargetKind::EnemySpellLink;
    if (count < 1 || count > 5 || ids.size() != static_cast<std::size_t>(count) || ids.front() != target)
        return false;
    std::set<CardId> seen;
    for (auto id : ids)
        if (!seen.insert(id).second || !Rules::targetValid(s, kind, p, id))
            return false;
    return true;
}
std::vector<std::vector<CardId>> sets(const GameState &s, TargetKind kind, int count, PlayerId p) {
    if (!cardTarget(kind))
        return {{}};
    std::vector<CardId> ids;
    for (const auto &[id, c] : s.cards)
        if (Rules::targetValid(s, kind, p, id))
            ids.push_back(id);
    std::vector<std::vector<CardId>> out;
    std::vector<CardId> selected;
    auto visit = [&](auto &&self, std::size_t pos) -> void {
        if (selected.size() == static_cast<std::size_t>(count)) {
            out.push_back(selected);
            return;
        }
        for (std::size_t i = pos; i < ids.size(); ++i) {
            selected.push_back(ids[i]);
            self(self, i + 1);
            selected.pop_back();
        }
    };
    if (count > 0 && count <= 5)
        visit(visit, 0);
    return out;
}
const ResponseAbility *ability(const CardDefinition &d, const std::string &id) {
    for (const auto &a : d.responses)
        if (a.id == id)
            return &a;
    return nullptr;
}
MatchConfig shared(std::array<std::vector<std::string>, 2> decks, std::uint32_t seed) {
    MatchConfig c;
    c.seed = seed;
    for (int p = 0; p < 2; ++p)
        c.players[p].cards = std::move(decks[p]);
    return c;
}
Trigger snapshot(const CardInstance &c, const CardDefinition &d) {
    Trigger t;
    t.owner = c.owner;
    t.source = c.id;
    t.targetCard = c.targetCard;
    t.targetPlayer = c.targetPlayer;
    t.target = d.target;
    t.targetCards = c.targets;
    t.effects = d.effects;
    return t;
}
}
