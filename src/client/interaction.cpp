#include "wizard/interaction.hpp"
#include <algorithm>
#include <type_traits>

namespace wizard::ui {
std::optional<AdvancePhase> automaticAdvance(const GameView &view, bool enabled) {
    if (!enabled || view.result != -1 || view.phase == Phase::Main || view.phaseStep != PhaseStep::Body ||
        view.decision || view.chain || view.viewer != view.active)
        return std::nullopt;
    for (const auto &action : view.actions)
        if (!std::holds_alternative<AdvancePhase>(action.command) &&
            !std::holds_alternative<Surrender>(action.command))
            return std::nullopt;
    for (const auto &action : view.actions)
        if (auto advance = std::get_if<AdvancePhase>(&action.command))
            return *advance;
    return std::nullopt;
}
void Interaction::update(GameView view) {
    view_ = std::move(view);
    cancel();
}
void Interaction::cancel() {
    selected_ = 0;
    step_ = Step::Inspect;
    options_.clear();
    pending_.reset();
}
bool Interaction::select(CardId card) {
    if (std::none_of(view_.cards.begin(), view_.cards.end(),
                     [&](const CardView &c) { return c.instance.id == card; }))
        return false;
    cancel();
    selected_ = card;
    return true;
}
CardId Interaction::target(const Command &cmd) {
    return std::visit(
        [](const auto &c) -> CardId {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, StartAnalysis> || std::is_same_v<T, SetAmbush>)
                return c.formation;
            else if constexpr (std::is_same_v<T, AttachSeal>)
                return c.host;
            else if constexpr (std::is_same_v<T, SetFormation>)
                return c.replace;
            else if constexpr (std::is_same_v<T, PrepareCast> || std::is_same_v<T, PlayAction> ||
                               std::is_same_v<T, Respond> || std::is_same_v<T, FlipAmbush> ||
                               std::is_same_v<T, ActivateSpell>)
                return c.target;
            else
                return 0;
        },
        cmd);
}
CardId Interaction::costCard(const Command &cmd) {
    if (auto c = std::get_if<PrepareCast>(&cmd))
        return c->discard;
    if (auto c = std::get_if<Respond>(&cmd))
        return c->discard;
    return 0;
}
std::string Interaction::intent(const LegalAction &a, const std::optional<PendingDecision> &d,
                                const GameView *view) {
    return std::visit(
        [&](const auto &c) -> std::string {
            using T = std::decay_t<decltype(c)>;
            if constexpr (std::is_same_v<T, SetFormation>)
                return c.replace ? "替换阵法" : "设置阵法";
            else if constexpr (std::is_same_v<T, RemoveFormation>)
                return "拆除阵法";
            else if constexpr (std::is_same_v<T, StartAnalysis>)
                return "开始解析";
            else if constexpr (std::is_same_v<T, PrepareCast> || std::is_same_v<T, PlayAction>) {
                std::string title = std::is_same_v<T, PrepareCast> ? "准备施法" : "使用行动卡";
                if (c.targets.size() > 1)
                    for (auto id : c.targets)
                        title += " #" + std::to_string(id);
                return title;
            } else if constexpr (std::is_same_v<T, PreloadWord>)
                return c.release ? "设置并释放" : "设置言灵法术";
            else if constexpr (std::is_same_v<T, AttachSeal>)
                return "附着符文";
            else if constexpr (std::is_same_v<T, RemoveSeal>)
                return "拆除符文";
            else if constexpr (std::is_same_v<T, Abandon>)
                return "放弃法术";
            else if constexpr (std::is_same_v<T, Advance>)
                return "进入施法阶段";
            else if constexpr (std::is_same_v<T, AdvancePhase> || std::is_same_v<T, PassResponse>)
                return a.label;
            else if constexpr (std::is_same_v<T, Respond>) {
                std::string name = c.ability;
                if (view)
                    for (const auto &card : view->cards)
                        if (card.instance.id == a.source)
                            for (const auto &ability : card.definition.responses)
                                if (ability.id == c.ability && !ability.name.empty())
                                    name = ability.name;
                std::string title = name;
                if (c.targets.size() > 1)
                    for (auto id : c.targets)
                        title += " #" + std::to_string(id);
                return title;
            } else if constexpr (std::is_same_v<T, SetAmbush>)
                return "埋伏卡牌";
            else if constexpr (std::is_same_v<T, FlipAmbush>)
                return "反转埋伏卡";
            else if constexpr (std::is_same_v<T, ActivateSpell>)
                return "释放法术";
            else if constexpr (std::is_same_v<T, Surrender>)
                return "投降";
            else {
                if (c.option == 0)
                    return a.label;
                if (!d)
                    return "确认选择";
                switch (d->kind) {
                case DecisionKind::Response:
                    return "释放言灵法术";
                case DecisionKind::CastOrder:
                    return "释放法术";
                case DecisionKind::Discard:
                case DecisionKind::EffectDiscard:
                    return "弃置此牌";
                case DecisionKind::DestroyOwnFormation:
                    return "销毁此阵法";
                case DecisionKind::EffectPrepare:
                case DecisionKind::Concentration:
                    return a.label;
                case DecisionKind::Overflow:
                    return "移除多余法术";
                case DecisionKind::TriggerOrder:
                    return a.label;
                case DecisionKind::ClearLoad:
                case DecisionKind::DestroyAmbush:
                case DecisionKind::SearchDeck:
                    return a.label;
                }
                return "确认选择";
            }
        },
        a.command);
}
std::vector<ActionGroup> Interaction::groups() const {
    std::vector<ActionGroup> groups;
    for (const auto &a : view_.actions)
        if (a.source && a.source == selected_) {
            auto title = intent(a, view_.decision, &view_);
            auto i = std::find_if(groups.begin(), groups.end(),
                                  [&](const ActionGroup &g) { return g.title == title; });
            if (i == groups.end())
                groups.push_back({title, {a}});
            else
                i->options.push_back(a);
        }
    return groups;
}
bool Interaction::activate(std::size_t index) {
    auto available = groups();
    if (index >= available.size())
        return false;
    options_ = available[index].options;
    pending_.reset();
    if (std::any_of(options_.begin(), options_.end(), [](const LegalAction &a) {
            auto r = std::get_if<Respond>(&a.command);
            return r && r->link != 0;
        })) {
        step_ = Step::LinkTarget;
        auto links = linkCandidates();
        if (links.size() == 1)
            pickLink(links.front());
        return true;
    }
    if (std::any_of(options_.begin(), options_.end(),
                    [](const LegalAction &a) { return target(a.command) != 0; }))
        step_ = Step::Target;
    else
        advance();
    if (choosingFormation() && candidates().size() == 1)
        pick(candidates().front());
    return true;
}
bool Interaction::choosingFormation() const {
    return step_ == Step::Target && !options_.empty() &&
           (std::holds_alternative<StartAnalysis>(options_.front().command) ||
            (std::holds_alternative<SetAmbush>(options_.front().command) &&
             target(options_.front().command)));
}
std::vector<CardId> Interaction::candidates() const {
    std::vector<CardId> ids;
    if (step_ != Step::Target && step_ != Step::Cost)
        return ids;
    for (const auto &a : options_) {
        auto id = step_ == Step::Target ? target(a.command) : costCard(a.command);
        if (id && std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(id);
    }
    return ids;
}
bool Interaction::pick(CardId card) {
    auto valid = candidates();
    if (std::find(valid.begin(), valid.end(), card) == valid.end())
        return false;
    const auto stage = step_;
    options_.erase(std::remove_if(options_.begin(), options_.end(),
                                  [&](const LegalAction &a) {
                                      return (stage == Step::Target ? target(a.command)
                                                                    : costCard(a.command)) != card;
                                  }),
                   options_.end());
    if (stage == Step::Cost) {
        pending_ = options_.front();
        step_ = Step::Confirm;
    } else
        advance();
    return true;
}
std::vector<LinkId> Interaction::linkCandidates() const {
    std::vector<LinkId> out;
    if (step_ != Step::LinkTarget)
        return out;
    for (const auto &a : options_)
        if (auto r = std::get_if<Respond>(&a.command))
            if (r->link && std::find(out.begin(), out.end(), r->link) == out.end())
                out.push_back(r->link);
    return out;
}
bool Interaction::pickLink(LinkId link) {
    auto links = linkCandidates();
    if (std::find(links.begin(), links.end(), link) == links.end())
        return false;
    options_.erase(
        std::remove_if(options_.begin(), options_.end(),
                       [&](const LegalAction &a) { return std::get<Respond>(a.command).link != link; }),
        options_.end());
    if (std::any_of(options_.begin(), options_.end(),
                    [](const LegalAction &a) { return target(a.command) != 0; }))
        step_ = Step::Target;
    else
        advance();
    return true;
}
void Interaction::advance() {
    if (options_.empty())
        return;
    if (std::any_of(options_.begin(), options_.end(),
                    [](const LegalAction &a) { return costCard(a.command) != 0; }))
        step_ = Step::Cost;
    else {
        pending_ = options_.front();
        step_ = Step::Confirm;
    }
}
void Interaction::offer(const LegalAction &action) {
    // Only current projected actions may be offered, including global/pass decisions.
    auto i = std::find_if(view_.actions.begin(), view_.actions.end(), [&](const LegalAction &a) {
        if (a.label != action.label || a.source != action.source || a.target != action.target ||
            a.command.index() != action.command.index())
            return false;
        return std::visit(
            [&](const auto &x) {
                using T = std::decay_t<decltype(x)>;
                const auto &y = std::get<T>(action.command);
                if constexpr (std::is_same_v<T, Choose>)
                    return x.decision == y.decision && x.option == y.option;
                else if constexpr (std::is_same_v<T, AdvancePhase>)
                    return x.gate == y.gate;
                else if constexpr (std::is_same_v<T, PassResponse>)
                    return x.decision == y.decision;
                else if constexpr (std::is_same_v<T, Respond>)
                    return x.decision == y.decision && x.card == y.card && x.ability == y.ability &&
                           x.target == y.target && x.link == y.link && x.discard == y.discard &&
                           x.targets == y.targets;
                else if constexpr (std::is_same_v<T, PrepareCast>)
                    return x.card == y.card && x.target == y.target && x.discard == y.discard &&
                           x.targets == y.targets && x.decision == y.decision;
                else if constexpr (std::is_same_v<T, PlayAction> || std::is_same_v<T, FlipAmbush> ||
                                   std::is_same_v<T, ActivateSpell>)
                    return x.card == y.card && x.target == y.target && x.targets == y.targets;
                else if constexpr (std::is_same_v<T, SetAmbush>)
                    return x.card == y.card && x.formation == y.formation;
                else if constexpr (std::is_same_v<T, PreloadWord>)
                    return x.card == y.card && x.release == y.release;
                else
                    return true;
            },
            a.command);
    });
    if (i == view_.actions.end())
        return;
    cancel();
    pending_ = *i;
    step_ = Step::Confirm;
}
void HandOrder::sync(const GameView &view) {
    auto &ids = order_.at(view.viewer);
    std::vector<CardId> visible;
    for (const auto &c : view.cards)
        if (c.instance.owner == view.viewer && c.instance.zone == Zone::Hand)
            visible.push_back(c.instance.id);
    ids.erase(std::remove_if(
                  ids.begin(), ids.end(),
                  [&](CardId id) { return std::find(visible.begin(), visible.end(), id) == visible.end(); }),
              ids.end());
    for (auto id : visible)
        if (std::find(ids.begin(), ids.end(), id) == ids.end())
            ids.push_back(id);
}
bool HandOrder::moveBefore(PlayerId p, CardId source, CardId before) {
    auto &ids = order_.at(p);
    auto it = std::find(ids.begin(), ids.end(), source);
    if (it == ids.end() || source == before ||
        (before && std::find(ids.begin(), ids.end(), before) == ids.end()))
        return false;
    ids.erase(it);
    ids.insert(before ? std::find(ids.begin(), ids.end(), before) : ids.end(), source);
    return true;
}
void HandOrder::sort(const GameView &view, bool byCost) {
    sync(view);
    std::map<CardId, const CardDefinition *> definitions;
    for (const auto &c : view.cards)
        if (c.instance.zone == Zone::Hand && c.instance.owner == view.viewer)
            definitions[c.instance.id] = &c.definition;
    auto &ids = order_.at(view.viewer);
    std::stable_sort(ids.begin(), ids.end(), [&](CardId a, CardId b) {
        const auto &x = *definitions.at(a);
        const auto &y = *definitions.at(b);
        if (byCost && x.cost != y.cost)
            return x.cost < y.cost;
        if (x.type != y.type)
            return x.type < y.type;
        if (x.cost != y.cost)
            return x.cost < y.cost;
        return x.id < y.id;
    });
}
bool Interaction::drop(CardId source, CardId targetCard, Zone destination) {
    if (step_ == Step::LinkTarget)
        return false;
    if (step_ == Step::Confirm)
        return false;
    if (step_ == Step::Cost)
        return destination == Zone::Ash && pick(source);
    if (step_ == Step::Target)
        return source == selected_ && pick(targetCard);
    Interaction next = *this;
    if (!next.select(source))
        return false;
    auto available = next.groups();
    std::vector<std::size_t> order;
    for (std::size_t n = 0; n < available.size(); ++n)
        order.push_back(n);
    // Shared drop regions prefer the ordinary operation; ambush remains an explicit menu choice.
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        return std::holds_alternative<SetAmbush>(available[a].options.front().command) <
               std::holds_alternative<SetAmbush>(available[b].options.front().command);
    });
    for (auto n : order) {
        bool matches = false;
        for (const auto &a : available[n].options) {
            matches = std::visit(
                [&](const auto &cmd) {
                    using T = std::decay_t<decltype(cmd)>;
                    if constexpr (std::is_same_v<T, StartAnalysis>)
                        return destination == Zone::Analysis;
                    else if constexpr (std::is_same_v<T, SetFormation>)
                        return destination == Zone::Analysis && cmd.replace == targetCard;
                    else if constexpr (std::is_same_v<T, AttachSeal>)
                        return targetCard && cmd.host == targetCard;
                    else if constexpr (std::is_same_v<T, PreloadWord>)
                        return destination == Zone::Words || (destination == Zone::Casting && cmd.release);
                    else if constexpr (std::is_same_v<T, PlayAction>)
                        return destination == Zone::Action || (targetCard && cmd.target == targetCard);
                    else if constexpr (std::is_same_v<T, PrepareCast>)
                        return destination == Zone::Casting || (targetCard && cmd.target == targetCard);
                    else if constexpr (std::is_same_v<T, SetAmbush>)
                        return cmd.formation ? destination == Zone::Analysis : destination == Zone::Words;
                    else if constexpr (std::is_same_v<T, FlipAmbush>)
                        return destination == Zone::Casting || destination == Zone::Action;
                    else if constexpr (std::is_same_v<T, ActivateSpell>)
                        return destination == Zone::Casting;
                    else if constexpr (std::is_same_v<T, Respond>)
                        return destination == Zone::Casting || destination == Zone::Resolving;
                    else if constexpr (std::is_same_v<T, Choose>)
                        return view_.decision &&
                               ((destination == Zone::Casting &&
                                 (view_.decision->kind == DecisionKind::CastOrder ||
                                  view_.decision->kind == DecisionKind::Response ||
                                  view_.decision->kind == DecisionKind::Concentration)) ||
                                (destination == Zone::Ash &&
                                 (view_.decision->kind == DecisionKind::Discard ||
                                  view_.decision->kind == DecisionKind::Overflow ||
                                  view_.decision->kind == DecisionKind::EffectDiscard ||
                                  view_.decision->kind == DecisionKind::DestroyOwnFormation ||
                                  view_.decision->kind == DecisionKind::DestroyAmbush)));
                    else
                        return false; // Dragging never implicitly abandons or removes a card.
                },
                a.command);
            if (matches)
                break;
        }
        if (!matches)
            continue;
        next.activate(n);
        // Dropping a ready spell anywhere in the casting area starts preparation;
        // existing spells there are not effect targets.
        bool castingArea = destination == Zone::Casting &&
                           std::holds_alternative<PrepareCast>(available[n].options.front().command);
        if (targetCard && !castingArea && next.step() == Step::Target) {
            bool formationChoice = next.choosingFormation();
            if (!next.pick(targetCard) && !formationChoice)
                continue;
        }
        *this = std::move(next);
        return true;
    }
    return false;
}

} // namespace wizard::ui
