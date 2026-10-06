#include "wizard/ai.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <type_traits>

namespace wizard::ai {
namespace {
const CardView *card(const GameView &v, CardId id) {
    for (const auto &c : v.cards)
        if (c.instance.id == id &&
            (c.instance.zone != Zone::Deck ||
             (v.decision && v.decision->kind == DecisionKind::SearchDeck &&
              std::find(v.decision->options.begin(), v.decision->options.end(), id) !=
                  v.decision->options.end())) &&
            (c.instance.zone != Zone::Hand || c.instance.owner == v.viewer))
            return &c;
    return nullptr;
}
double value(const CardView *c) {
    if (!c)
        return 0;
    const auto &d = c->definition;
    double n = 2 + d.cost + d.rank + d.capacity * .6 + d.income * 4 + d.incomeBonus * 5;
    for (const auto &e : d.effects) {
        if (e.kind == EffectKind::Damage)
            n += e.amount * 2;
        if (e.kind == EffectKind::Draw)
            n += e.amount * 3;
        if (e.kind == EffectKind::CancelPreparation || e.kind == EffectKind::NegateLink)
            n += 10;
    }
    if (c->instance.spell == SpellState::Ready || c->instance.spell == SpellState::Pending)
        n += 6;
    return n;
}
double effects(const GameView &v, const std::vector<Effect> &list, TargetKind target, CardId targetCard,
               PlayerId owner = -1) {
    const auto p = v.viewer, enemy = 1 - p;
    if (owner < 0)
        owner = p;
    const int targetRecipient = target == TargetKind::Opponent ? 1 - owner : owner;
    double score = 0;
    std::array<int, 2> loads{v.players[0].load, v.players[1].load};
    for (const auto &e : list) {
        const int recipient = e.recipient == EffectRecipient::Owner      ? owner
                              : e.recipient == EffectRecipient::Opponent ? 1 - owner
                                                                         : targetRecipient;
        auto &load = loads[recipient];
        switch (e.kind) {
        case EffectKind::Damage: {
            const auto &pl = v.players[recipient];
            int amount = e.amount;
            if (std::find(pl.immunities.begin(), pl.immunities.end(), e.damageType) != pl.immunities.end())
                amount = 0;
            else if (std::find(pl.resistances.begin(), pl.resistances.end(), e.damageType) !=
                     pl.resistances.end())
                amount /= 2;
            score += (recipient == enemy ? 1 : -1) *
                     (amount * 3.0 + (amount >= pl.life + pl.temporaryLife ? 10000 : 0));
            break;
        }
        case EffectKind::AddTemporaryLife:
            score +=
                (recipient == p ? 1 : -1) * std::max(0, e.amount - v.players[recipient].temporaryLife) * 2;
            break;
        case EffectKind::BlockActions:
            score += recipient == enemy ? 10 : -10;
            break;
        case EffectKind::GrantResistance:
            score += owner == p ? 12 : -12;
            break;
        case EffectKind::SearchAction:
            score += owner == p ? 12 : -12;
            break;
        case EffectKind::DestroyAmbush:
            score += 5;
            break;
        case EffectKind::AccelerateAnalysis: {
            const auto *c = card(v, targetCard);
            if (!c)
                break;
            score += 10 + value(c) / 2 - c->instance.settingPaid * 2;
            if (v.players[owner].load + c->instance.settingPaid > v.players[owner].capacity)
                score -= 25000;
            break;
        }
        case EffectKind::Conceal:
            score += 2;
            break;
        case EffectKind::Heal:
            score += (recipient == p ? 1 : -1) * std::min(e.amount, maximumLife - v.players[recipient].life) *
                     (v.players[recipient].life <= 10 ? 4.0 : 1.0);
            break;
        case EffectKind::Draw:
            score += (owner == p ? 1 : -1) *
                     (v.players[owner].deckCount < e.amount
                          ? -20000
                          : std::min(e.amount, std::max(0, 7 - v.players[owner].handCount)) * 4.0);
            break;
        case EffectKind::GainMana:
            score += (owner == p ? 1 : -1) * e.amount * 3.0;
            break;
        case EffectKind::AddSourceTemporary:
        case EffectKind::AddTemporary:
        case EffectKind::AddIndependent:
            load += e.amount;
            score += recipient == enemy ? e.amount * 2.0 : -e.amount * 3.0;
            if (load > v.players[recipient].capacity)
                score += recipient == enemy ? 15000 : -25000;
            break;
        case EffectKind::ClearAllTemporary:
        case EffectKind::ClearTemporary:
        case EffectKind::ClearIndependent: {
            int removable = 0;
            for (const auto &l : v.temporary)
                if (l.owner == owner &&
                    (e.kind == EffectKind::ClearIndependent ? l.independent : !l.independent))
                    removable += l.amount;
            if (e.kind == EffectKind::ClearTemporary)
                removable = std::min(removable, e.amount);
            score += (owner == p ? 1 : -1) * removable * 4.0;
            break;
        }
        case EffectKind::Destroy: {
            const auto *c = card(v, targetCard);
            if (c && !c->instance.base &&
                !(c->instance.owner == enemy && c->definition.opponentDestroyProtected))
                score += (c->instance.owner == enemy ? 1 : -1) * (8 + value(c));
            break;
        }
        case EffectKind::DiscardHand:
            score -= 6;
            break;
        case EffectKind::DiscardFormation:
            score -= 8;
            break;
        case EffectKind::OptionalPrepare:
            score += 3;
            break;
        case EffectKind::CancelPreparation:
        case EffectKind::CounterSpell:
        case EffectKind::NegateLink:
            score += 8;
            break;
        case EffectKind::OptionalDestroyOwnFormation:
            break;
        }
    }
    return score;
}
double counterRisk(const GameView &v) {
    double n = 0;
    for (const auto &c : v.cards)
        if (c.instance.owner != v.viewer && c.instance.zone == Zone::Words)
            for (const auto &r : c.definition.responses)
                if (r.fromWords && r.eventOwner != EventOwner::Self &&
                    r.extraDiscard <= v.players[1 - v.viewer].handCount &&
                    r.preloadedCost <= v.players[1 - v.viewer].mana &&
                    std::find(r.windows.begin(), r.windows.end(), ResponseWindow::Prepare) !=
                        r.windows.end()) {
                    for (const auto &e : r.effects)
                        if (e.kind == EffectKind::CancelPreparation || e.kind == EffectKind::NegateLink)
                            n = 1;
                }
    return n;
}
double evaluate(const GameView &v, const LegalAction &a, Difficulty difficulty) {
    const auto p = v.viewer;
    const bool easy = difficulty == Difficulty::Easy, hard = difficulty == Difficulty::Hard;
    const auto *source = card(v, a.source);
    return std::visit(
        [&](const auto &cmd) -> double {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, Surrender>)
                return -1e9;
            else if constexpr (std::is_same_v<T, Advance> || std::is_same_v<T, AdvancePhase> ||
                               std::is_same_v<T, PassResponse>)
                return 0;
            else if constexpr (std::is_same_v<T, RemoveFormation> || std::is_same_v<T, RemoveSeal> ||
                               std::is_same_v<T, Abandon>)
                return -200;
            else if constexpr (std::is_same_v<T, Choose>) {
                if (!v.decision)
                    return -100;
                const auto *c = card(v, cmd.option);
                switch (v.decision->kind) {
                case DecisionKind::SearchDeck:
                    return 20 + value(c);
                case DecisionKind::DestroyAmbush:
                    return cmd.option ? 30 + value(c) : 0;
                case DecisionKind::Discard:
                case DecisionKind::EffectDiscard:
                case DecisionKind::Overflow:
                    return cmd.option ? 100 - value(c) : -100;
                case DecisionKind::ClearLoad:
                    for (const auto &l : v.temporary)
                        if (l.id == cmd.option)
                            return 10 + l.amount;
                    return 0;
                case DecisionKind::DestroyOwnFormation:
                    // Optional self-destruction is safely skipped; removing useful capacity is not a benefit.
                    return cmd.option ? -100 - value(c) : 0;
                case DecisionKind::Concentration:
                    if (!cmd.option)
                        return 0;
                    if (!c)
                        return -1;
                    if (v.players[p].load + c->instance.concentrationCost >= v.players[p].capacity)
                        return -100;
                    return effects(v, c->definition.onCast, c->definition.target, c->instance.targetCard) +
                           effects(v, c->definition.onPrepare, c->definition.target, c->instance.targetCard) +
                           3 - c->instance.concentrationCost;
                case DecisionKind::CastOrder:
                    return c ? (easy ? 100
                                     : 20 + effects(v, c->definition.effects, c->definition.target,
                                                    c->instance.targetCard))
                             : -10;
                case DecisionKind::TriggerOrder:
                    for (const auto &t : v.triggers)
                        if (t.id == cmd.option)
                            return 10 + effects(v, t.effects, t.target, t.targetCard) +
                                   (hard && std::any_of(t.effects.begin(), t.effects.end(),
                                                        [](const Effect &e) {
                                                            return e.kind == EffectKind::GainMana;
                                                        })
                                        ? 15
                                        : 0);
                    return 10;
                case DecisionKind::EffectPrepare:
                    return cmd.option ? 10 : 0;
                case DecisionKind::Response:
                    return cmd.option ? 20 : 0;
                }
                return 0;
            } else {
                if (!source)
                    return -100;
                const auto &d = source->definition;
                if constexpr (std::is_same_v<T, SetAmbush>) {
                    return d.responseOnly ? 6 - d.cost : -5;
                } else if constexpr (std::is_same_v<T, FlipAmbush>) {
                    return 12 + effects(v, d.effects, d.target, cmd.target) - d.cost * 2;
                } else if constexpr (std::is_same_v<T, ActivateSpell>) {
                    return 18 + effects(v, d.effects, d.target, cmd.target) -
                           (source->effectiveCastCost - (d.type == CardType::Word ? d.cost : 0)) * 2;
                } else if constexpr (std::is_same_v<T, SetFormation>) {
                    if (easy)
                        return cmd.replace ? -50 : 70;
                    return 10 + d.capacity * 1.5 + d.income * 6 - d.cost * 2 -
                           (cmd.replace ? value(card(v, cmd.replace)) + 10 : 0);
                } else if constexpr (std::is_same_v<T, AttachSeal>) {
                    if (easy)
                        return 45;
                    const auto *host = card(v, cmd.host);
                    const bool formation = host && host->definition.type == CardType::Formation;
                    return 4 +
                           (formation ? d.incomeBonus * 8 + d.ringBonus * 3
                                      : d.refundSetting * (host ? host->instance.settingPaid : 0)) -
                           d.cost;
                } else if constexpr (std::is_same_v<T, StartAnalysis>) {
                    if (easy)
                        return 60;
                    double utility = effects(v, d.effects, d.target, 0);
                    // Self-load from a future spell is not immediate, but avoid preparing a useless dangerous
                    // spell.
                    if (utility < -1000)
                        return -20;
                    return 12 + utility / 2 - d.cost +
                           (hard && v.players[p].mana >= d.cost + d.castCost ? 3 : 0);
                } else if constexpr (std::is_same_v<T, PrepareCast>) {
                    double utility = effects(v, d.effects, d.target, cmd.target);
                    if (utility < -1000)
                        return utility;
                    if (easy)
                        return 90;
                    double score = 18 + utility - d.castCost * 2 - value(card(v, cmd.discard));
                    if (d.concentration)
                        score += 6;
                    if (hard)
                        score -= counterRisk(v) * std::max(0.0, utility) * 1.1;
                    return score;
                } else if constexpr (std::is_same_v<T, PlayAction>) {
                    double utility = effects(v, d.effects, d.target, cmd.target);
                    if (utility < -1000)
                        return utility;
                    return easy ? 55 : utility + 3 - d.cost * 2;
                } else if constexpr (std::is_same_v<T, PreloadWord>) {
                    if (easy)
                        return cmd.release ? 65 : 30;
                    if (cmd.release)
                        return 8 + effects(v, d.effects, d.target, 0) - d.cost;
                    bool readyEnemy = false;
                    for (const auto &c : v.cards)
                        if (c.instance.owner != p && c.instance.spell == SpellState::Ready)
                            readyEnemy = true;
                    return 3 + (readyEnemy ? 15 : 7) - d.cost * .5;
                } else if constexpr (std::is_same_v<T, Respond>) {
                    const ResponseAbility *ability = nullptr;
                    for (const auto &r : d.responses)
                        if (r.id == cmd.ability)
                            ability = &r;
                    if (!ability)
                        return -100;
                    bool cancel = false;
                    double utility = effects(v, ability->effects, ability->target, cmd.target);
                    for (const auto &e : ability->effects)
                        if (e.kind == EffectKind::NegateLink || e.kind == EffectKind::CancelPreparation ||
                            e.kind == EffectKind::CounterSpell)
                            cancel = true;
                    if (cancel && v.chain) {
                        const auto it = std::find_if(v.chain->links.begin(), v.chain->links.end(),
                                                     [&](const ChainLink &l) { return l.id == cmd.link; });
                        if (it == v.chain->links.end() || it->canceled || it->sourceLost)
                            return -1;
                        const bool enemy = it->item.owner != p;
                        double threat = effects(v, it->item.effects, it->item.target, it->item.targetCard,
                                                it->item.owner);
                        // A response countering an enemy cancellation protects our earlier beneficial link.
                        const bool cancels = std::any_of(it->item.effects.begin(), it->item.effects.end(),
                                                         [](const Effect &e) {
                                                             return e.kind == EffectKind::CancelPreparation ||
                                                                    e.kind == EffectKind::NegateLink;
                                                         });
                        if (it->kind == LinkKind::Response && enemy && cancels)
                            utility += 20;
                        else
                            utility += enemy ? -threat + 8 : -std::abs(threat) - 30;
                    }
                    const int paid =
                        source->instance.faceDown                 ? d.cost
                        : source->instance.zone == Zone::Analysis ? source->effectiveCastCost
                        : source->instance.zone == Zone::Hand
                            ? ability->handCost
                            : ability->preloadedCost +
                                  (d.type == CardType::Word ? source->effectiveCastCost - d.cost : 0);
                    if (v.chain &&
                        std::any_of(ability->effects.begin(), ability->effects.end(),
                                    [](const Effect &e) { return e.kind == EffectKind::CounterSpell; }))
                        for (const auto &l : v.chain->links)
                            if (l.id == cmd.link &&
                                v.players[p].load + l.castCost + (source->instance.faceDown ? paid : 0) >
                                    v.players[p].capacity)
                                return -25000;
                    if (utility < -1000)
                        return utility;
                    return easy ? 100 : utility - paid * 2 - value(card(v, cmd.discard));
                } else
                    return -100;
            }
        },
        a.command);
}
} // namespace
std::string difficultyLabel(Difficulty d) {
    return std::array<const char *, 3>{"简单", "普通", "困难"}.at(static_cast<std::size_t>(d));
}
std::optional<Selection> choose(const GameView &view, Difficulty difficulty, std::size_t budget) {
    if (view.result != -1 || view.actions.empty() || view.viewer < 0 || view.viewer > 1 || budget == 0)
        return {};
    budget = std::min<std::size_t>(budget, 4096);
    const LegalAction *best = nullptr;
    double score = -std::numeric_limits<double>::infinity();
    std::size_t evaluated = 0;
    // Always retain a safe pass/advance even when the candidate budget is exhausted.
    for (const auto &a : view.actions)
        if (std::holds_alternative<Advance>(a.command) || std::holds_alternative<AdvancePhase>(a.command) ||
            std::holds_alternative<PassResponse>(a.command)) {
            best = &a;
            score = 0;
            break;
        }
    for (const auto &a : view.actions) {
        if (evaluated == budget)
            break;
        if (std::holds_alternative<Surrender>(a.command))
            continue;
        const double n = evaluate(view, a, difficulty);
        ++evaluated;
        if (!best || n > score) {
            best = &a;
            score = n;
        }
    }
    if (!best)
        return {};
    return Selection{best->command, score, evaluated};
}
} // namespace wizard::ai
