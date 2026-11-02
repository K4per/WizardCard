#pragma once
#include "wizard/core/engine.hpp"
#include <algorithm>
namespace wizard::detail {
inline bool stormTags(const std::vector<std::string> &tags) {
    return std::find(tags.begin(), tags.end(), "风契") != tags.end() ||
           std::find(tags.begin(), tags.end(), "风暴") != tags.end();
}
std::uint32_t random32(MutationContext &);
std::uint32_t bounded(MutationContext &, std::uint32_t);
std::vector<CardId> inZone(const GameState &, PlayerId, Zone);
int seals(const GameState &, CardId);
int usedBody(const GameState &, const CardCatalog &, PlayerId);
bool hasOverflow(const GameState &, const CardCatalog &);
bool cardTarget(TargetKind);
std::vector<CardId> targetCards(CardId, const std::vector<CardId> &);
bool validTargets(const GameState &, TargetKind, int, PlayerId, CardId, const std::vector<CardId> &);
// Lazy, lexicographic combinations. A selected prefix fixes earlier choices;
// increment only visits the unresolved suffix and never materializes products.
class Combinations {
    std::vector<CardId> pool_, prefix_;
    std::size_t count_{};
    bool valid_{true};
  public:
    Combinations(std::vector<CardId> pool, std::size_t count, std::vector<CardId> prefix = {})
        : pool_(std::move(pool)), prefix_(std::move(prefix)), count_(count) {}
    static Combinations empty() { Combinations r({}, 0); r.valid_ = false; return r; }
    class iterator {
        const Combinations *range_{};
        std::vector<std::size_t> indices_;
        std::vector<CardId> selected_;
        bool done_{true};
        void refresh() { selected_.clear(); for (auto i : indices_) selected_.push_back(range_->pool_[i]); }
      public:
        iterator() = default;
        explicit iterator(const Combinations *range) : range_(range), done_(!range->valid_) {
            if (done_ || range->prefix_.size() > range->count_ || range->count_ > range->pool_.size()) { done_ = true; return; }
            std::size_t pos = 0;
            for (auto id : range->prefix_) {
                while (pos < range->pool_.size() && range->pool_[pos] != id) ++pos;
                if (pos == range->pool_.size()) { done_ = true; return; }
                indices_.push_back(pos++);
            }
            while (indices_.size() < range->count_) {
                if (pos == range->pool_.size()) { done_ = true; return; }
                indices_.push_back(pos++);
            }
            refresh();
        }
        const std::vector<CardId> &operator*() const { return selected_; }
        iterator &operator++() {
            auto i = indices_.size();
            while (i > range_->prefix_.size()) {
                --i;
                if (indices_[i] < range_->pool_.size() - (indices_.size() - i)) {
                    ++indices_[i];
                    for (auto j = i + 1; j < indices_.size(); ++j) indices_[j] = indices_[j-1] + 1;
                    refresh(); return *this;
                }
            }
            done_ = true; return *this;
        }
        bool operator!=(const iterator &) const { return !done_; }
    };
    iterator begin() const { return iterator(this); }
    iterator end() const { return {}; }
};
std::uint32_t legacyChoice(ChoiceValue, DecisionKind);
std::string choiceKey(ChoiceValue);
std::vector<ActionSelection> actionPath(const Command &);
bool matchesIntent(const Command &, const ActionIntent *);
bool intentAllows(const ActionIntent *, ActionField, std::uint64_t);
std::vector<CardId> intentPrefix(const ActionIntent *, ActionField);
Combinations sets(const GameState &, TargetKind, int, PlayerId, const ActionIntent * = nullptr);
Combinations activationSets(const GameState &, const CardCatalog &, PlayerId, const CardDefinition &, const ActionIntent * = nullptr);
const ResponseAbility *ability(const CardDefinition &, const std::string &);
MatchConfig shared(std::array<std::vector<std::string>, 2>, std::uint32_t);
Trigger snapshot(const CardInstance &, const CardDefinition &);
inline bool spellSpeedAllowed(const GameState &state, const CardDefinition &definition, bool setting = false) {
    if (state.chain) {
        const int top = state.chain->links.back().speed;
        return definition.speed >= 2 && top != 4 && definition.speed >= top &&
            (definition.speed != 2 || state.phase == Phase::Main || state.phase == Phase::Cast);
    }
    return definition.speed >= 3 ||
        (definition.speed == 2 && (state.phase == Phase::Main || state.phase == Phase::Cast)) ||
        (definition.speed == 1 && state.phase == (setting ? Phase::Main : Phase::Cast));
}
inline bool hasStormFormation(const GameState &state, const CardCatalog &catalog, PlayerId owner) {
    return std::any_of(state.cards.begin(), state.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        if (card.owner != owner || card.zone != Zone::Analysis || card.faceDown ||
            catalog.at(card.definition).type != CardType::Formation)
            return false;
        const auto &tags = catalog.at(card.definition).tags;
        return detail::stormTags(tags);
    });
}
inline int linkedSpellCost(const GameState &state, LinkId target) {
    if (!state.chain) return 0;
    const auto link = std::find_if(state.chain->links.begin(), state.chain->links.end(),
                                   [&](const ChainLink &candidate) { return candidate.id == target; });
    if (link == state.chain->links.end()) return 0;
    const auto source = state.cards.find(link->item.source);
    if (source == state.cards.end()) return std::max(0, link->castCost);
    const auto &card = source->second;
    int activation = 0;
    if (link->paymentIndex >= 0 && static_cast<std::size_t>(link->paymentIndex) < card.payments.size())
        activation = card.payments[static_cast<std::size_t>(link->paymentIndex)].paid;
    else if (!card.payments.empty())
        activation = card.payments.back().paid;
    return std::max(0, card.settingPaid) + std::max(0, activation);
}
inline bool greatStormLocksAnalysis(const GameState &state, const CardCatalog &catalog, PlayerId owner) {
    return std::any_of(state.cards.begin(), state.cards.end(), [&](const auto &entry) {
        const auto &card = entry.second;
        return card.owner == owner && card.zone == Zone::Analysis &&
               hasCapability(catalog.at(card.definition), Capability::GreatStorm);
    });
}
}
