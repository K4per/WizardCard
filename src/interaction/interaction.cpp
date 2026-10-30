#include "wizard/interaction.hpp"
#include <algorithm>
#include <type_traits>

namespace wizard::ui {
std::optional<AdvancePhase> automaticAdvance(const GameView &view, bool enabled) {
    if (!enabled || view.result != -1 || view.technicalPaused || view.phase == Phase::Main ||
        view.phaseStep != PhaseStep::Body || view.decision || view.chain || view.viewer != view.active ||
        view.actionSummary.hasNonPhaseAction || !view.actionSummary.safeAdvance) return std::nullopt;
    if (const auto *advance = std::get_if<AdvancePhase>(&*view.actionSummary.safeAdvance)) return *advance;
    return std::nullopt;
}
void Interaction::update(GameView view, Query query) { view_ = std::move(view); query_ = std::move(query); cancel(); }
void Interaction::cancel() { selected_ = 0; step_ = Step::Inspect; intent_ = {}; options_ = {}; pending_.reset(); }
bool Interaction::select(CardId card) {
    if (std::none_of(view_.cards.begin(), view_.cards.end(), [&](const CardView &c) { return c.instance.id == card; })) return false;
    cancel(); selected_ = card; intent_.source = card; return true;
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
            else if constexpr (std::is_same_v<T, ActivateAbility>)
                return "发动符文能力";
            else if constexpr (std::is_same_v<T, Surrender>)
                return "投降";
            else if constexpr (std::is_same_v<T, SelectInitialFormation>)
                return "确认初始阵法";
            else if constexpr (std::is_same_v<T, Timeout>)
                return "思考时间耗尽";
            else {
                if (!c.option)
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
                case DecisionKind::Payment:
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
    std::vector<ActionGroup> out;
    if (!query_) return out;
    ActionIntent request; request.source = selected_;
    do {
        const auto options = query_(request);
        if (options.pending || !options.error.empty()) break;
        for (const auto &option : options.options) out.push_back({option.label, option.value});
        if (!options.hasMore) break;
        request.offset += options.options.size();
    } while (true);
    return out;
}
bool Interaction::activate(std::size_t index) {
    const auto available = groups(); if (index >= available.size()) return false;
    intent_ = {}; intent_.source = selected_;
    intent_.selections.push_back({ActionField::Operation, available[index].operation});
    pending_.reset(); advance(); return options_.error.empty();
}
void Interaction::advance() {
    if (!query_) return;
    for (std::size_t depth = 0; depth < 32; ++depth) {
        options_ = query_(intent_);
        if (options_.pending || !options_.error.empty()) return;
        if (options_.complete) {
            pending_ = LegalAction{"确认操作", selected_, target(*options_.complete), *options_.complete};
            step_ = Step::Confirm; return;
        }
        const bool fixedSource = options_.field == ActionField::Source && selected_;
        const bool soleHost = options_.field == ActionField::Host && options_.options.size() == 1 && !options_.hasMore;
        const bool fixedAbility = options_.field == ActionField::Ability && options_.options.size() == 1 && !options_.hasMore;
        const bool emptyReplacement = options_.field == ActionField::Replacement && options_.options.size() == 1 && options_.options.front().value == "0";
        if (fixedSource || soleHost || fixedAbility || emptyReplacement) {
            const auto value = fixedSource ? std::to_string(selected_) : options_.options.front().value;
            const auto found = std::find_if(options_.options.begin(), options_.options.end(), [&](const ActionOption &o) { return o.value == value; });
            if (found == options_.options.end()) return;
            intent_.selections.push_back({options_.field, value}); continue;
        }
        step_ = options_.field == ActionField::Link ? Step::LinkTarget :
            options_.field == ActionField::Discard || options_.field == ActionField::Material ? Step::Cost : Step::Target;
        return;
    }
}
bool Interaction::choose(const std::string &value) {
    if (std::none_of(options_.options.begin(), options_.options.end(), [&](const ActionOption &o) { return o.value == value; })) return false;
    intent_.selections.push_back({options_.field, value}); intent_.offset = 0; advance(); return true;
}
bool Interaction::choosingFormation() const { return step_ == Step::Target && options_.field == ActionField::Host; }
std::vector<CardId> Interaction::candidates() const {
    std::vector<CardId> out;
    if (step_ != Step::Target && step_ != Step::Cost) return out;
    for (const auto &option : options_.options) if (option.card) out.push_back(option.card);
    return out;
}
bool Interaction::pick(CardId card) { return choose(std::to_string(card)); }
std::vector<LinkId> Interaction::linkCandidates() const {
    std::vector<LinkId> out; if (step_ != Step::LinkTarget) return out;
    for (const auto &option : options_.options) out.push_back(std::stoull(option.value));
    return out;
}
bool Interaction::pickLink(LinkId link) { return step_ == Step::LinkTarget && choose(std::to_string(link)); }
void Interaction::offer(const LegalAction &action) {
    cancel();
    const auto *choose = std::get_if<Choose>(&action.command);
    const auto *respond = std::get_if<Respond>(&action.command);
    const auto *pass = std::get_if<PassResponse>(&action.command);
    const auto *prepare = std::get_if<PrepareCast>(&action.command);
    const auto decision = choose ? choose->decision : respond ? respond->decision : pass ? pass->decision : prepare ? prepare->decision : 0;
    if (decision && (!view_.decision || view_.decision->id != decision || view_.decision->player != view_.viewer)) return;
    if (const auto *phase = std::get_if<AdvancePhase>(&action.command)) if (phase->gate != view_.phaseGate) return;
    pending_ = action; step_ = Step::Confirm;
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
    if (step_ == Step::LinkTarget || step_ == Step::Confirm) return false;
    if (step_ == Step::Cost) return destination == Zone::Ash && pick(source);
    if (step_ == Step::Target) return source == selected_ && pick(targetCard);
    auto next = *this; if (!next.select(source)) return false;
    const auto available = next.groups();
    for (std::size_t i = 0; i < available.size(); ++i) {
        const auto kind = static_cast<CommandKind>(std::stoul(available[i].operation));
        const bool matches = (destination == Zone::Analysis && (kind == CommandKind::StartAnalysis || kind == CommandKind::SetFormation || kind == CommandKind::AttachSeal)) ||
            (destination == Zone::Casting && (kind == CommandKind::PrepareCast || kind == CommandKind::ActivateSpell || kind == CommandKind::Respond || kind == CommandKind::FlipAmbush)) ||
            (destination == Zone::Words && kind == CommandKind::PreloadWord) || (destination == Zone::Action && kind == CommandKind::PlayAction);
        if (!matches || !next.activate(i)) continue;
        if (targetCard && next.step() == Step::Target && !(destination == Zone::Casting && kind == CommandKind::PrepareCast)) next.pick(targetCard);
        *this = std::move(next); return true;
    }
    return false;
}
} // namespace wizard::ui
