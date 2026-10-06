#include "wizard/tutorial.hpp"
#include <algorithm>

namespace wizard::app {
namespace {
const CardView *find(const GameView &v, CardId id) {
    for (const auto &c : v.cards)
        if (c.instance.id == id)
            return &c;
    return nullptr;
}
bool named(const GameView &v, CardId id, const char *name) {
    const auto *c = find(v, id);
    return c && c->definition.id == name;
}
const LegalAction *advance(const GameView &v) {
    for (const auto &a : v.actions)
        if (std::holds_alternative<AdvancePhase>(a.command) ||
            std::holds_alternative<PassResponse>(a.command))
            return &a;
    return nullptr;
}
} // namespace
MatchConfig tutorialConfiguration(const Content &content) {
    MatchConfig c;
    c.seed = 39; // Normal shuffle gives both players the fixed lesson's necessary starting cards.
    const std::array<std::vector<std::string>, 2> names{
        {{"fireball", "conduit", "reservoir", "recall", "spark", "clarity", "mend", "ring", "barbs", "ward"},
         {"barbs", "conduit", "reservoir", "recall", "spark", "clarity", "mend", "ring", "ward", "unravel"}}};
    for (int p = 0; p < 2; ++p) {
        for (const auto &id : names[p]) {
            (void)content.catalog.at(id);
            c.players[p].cards.insert(c.players[p].cards.end(), 3, id);
        }
    }
    return c;
}
std::string Tutorial::title() const {
    return std::array<const char *, 12>{"欢迎来到巫师牌", "阶段与魔素",   "设置阵法",       "附着符文",
                                        "开始解析",       "等待解析完成", "准备施法",       "观察反制与连锁",
                                        "再次准备施法",   "释放法术",     "结算与荷载清理", "教学完成"}
        .at(static_cast<std::size_t>(lesson));
}
std::string Tutorial::hint(const GameView &v) const {
    switch (lesson) {
    case Lesson::Intro:
        return "固定卡组、固定种子。你操控下方场地，AI手牌始终隐藏。点击开始学习。";
    case Lesson::Resources:
        return v.active == 1 ? "回合有五阶段。教学对手先行动，等待它完成回合；准备阶段会获得阵法提供的魔素。"
                             : "轮到你了。点击结束当前阶段，进入主要阶段；准备阶段会获得阵法提供的魔素。";
    case Lesson::Formation:
        return "点击或拖动阿克乌姆之弧，选择设置阵法并确认。它增加5点荷载上限，保护后续施法。";
    case Lesson::Rune:
        return "将逆三角星符附着在基础阵法均衡之六芒星上并确认，下回合魔素收入会增加2。";
    case Lesson::Analysis:
        return v.players[0].mana < 3
                   ? "火球术解析费用为3。当前魔素不足，先结束主要阶段，再逐阶段推进到下一回合。"
                   : "将火球术放到均衡之六芒星开始解析，支付3魔素并占用环位与绑定荷载。";
    case Lesson::Waiting:
        return "解析需要等待回合。结束主要阶段并继续推进；等待火球术解析完成、对方预置银光锐语。";
    case Lesson::Preparation:
        return "选择已解析完成的火球术，准备施法并确认。先支付1魔素，再开放对方的响应窗口。";
    case Lesson::Response:
        return countered ? "银光锐语取消了这次准备，支付不退款。法术仍在解析区，本回合不能重试。连锁逆序结算"
                           "后继续。"
                         : "对方将用预置言灵响应。新增响应交接优先权，双方连续放弃后逆序结算。留意左侧连锁。";
    case Lesson::Retry:
        return "结束当前回合，推进到下一回合后，再将同一张火球术准备施法。本次对方会放弃响应。";
    case Lesson::Release:
        return "准备成功后法术进入施法区。结束主要阶段，在施法阶段选择火球术并确认释放。可选销毁阵法时选择跳"
               "过。";
    case Lesson::Review:
        return "火球术造成10点伤害，成功释放后进入灰烬区并清理绑定荷载。释放、准备与支付是不同步骤。";
    case Lesson::Complete:
        return "已学会资源、阵法、符文、解析、准备、连锁与释放。可继续练习，或从结果/"
               "设置返回菜单；教学可重开。";
    }
    return {};
}
bool Tutorial::canContinue(const GameView &v) const {
    return lesson == Lesson::Intro || lesson == Lesson::Review ||
           (lesson == Lesson::Response && countered && !v.chain && !v.decision);
}
void Tutorial::continueLesson(const GameView &v) {
    if (!canContinue(v))
        return;
    if (lesson == Lesson::Intro)
        lesson = Lesson::Resources;
    else if (lesson == Lesson::Response)
        lesson = Lesson::Retry;
    else if (lesson == Lesson::Review)
        lesson = Lesson::Complete;
}
bool Tutorial::allows(const GameView &v, const Command &cmd) const {
    if (std::holds_alternative<Surrender>(cmd) || lesson == Lesson::Complete)
        return true;
    if (lesson == Lesson::Intro || lesson == Lesson::Review || canContinue(v))
        return false;
    if (v.decision)
        return true; // All forced choices still go through the ordinary core.
    if (v.phase != Phase::Main)
        return std::holds_alternative<AdvancePhase>(cmd);
    if (lesson == Lesson::Formation) {
        const auto *c = std::get_if<SetFormation>(&cmd);
        return c && !c->replace && named(v, c->card, "reservoir");
    }
    if (lesson == Lesson::Rune) {
        const auto *c = std::get_if<AttachSeal>(&cmd);
        const auto *host = c ? find(v, c->host) : nullptr;
        return c && named(v, c->card, "conduit") && host && host->instance.base;
    }
    if (lesson == Lesson::Analysis) {
        const auto *c = std::get_if<StartAnalysis>(&cmd);
        const auto *host = c ? find(v, c->formation) : nullptr;
        if (c)
            return named(v, c->card, "fireball") && host && host->instance.base;
        bool canAnalyze = false;
        for (const auto &a : v.actions)
            if (const auto *aCmd = std::get_if<StartAnalysis>(&a.command))
                if (named(v, aCmd->card, "fireball"))
                    canAnalyze = true;
        return !canAnalyze && std::holds_alternative<AdvancePhase>(cmd);
    }
    if (lesson == Lesson::Preparation || lesson == Lesson::Retry) {
        if (const auto *c = std::get_if<PrepareCast>(&cmd))
            return c->card == spell;
        bool canPrepare = false;
        for (const auto &a : v.actions)
            if (const auto *c = std::get_if<PrepareCast>(&a.command))
                if (c->card == spell)
                    canPrepare = true;
        return !canPrepare && std::holds_alternative<AdvancePhase>(cmd);
    }
    return std::holds_alternative<AdvancePhase>(cmd);
}
void Tutorial::observe(PlayerId actor, const Command &cmd, const GameView &after,
                       const std::vector<GameEvent> &events) {
    if (lesson == Lesson::Resources && after.active == 0 && after.phase == Phase::Main)
        lesson = Lesson::Formation;
    if (actor == 0) {
        if (lesson == Lesson::Formation && std::holds_alternative<SetFormation>(cmd))
            lesson = Lesson::Rune;
        else if (lesson == Lesson::Rune && std::holds_alternative<AttachSeal>(cmd))
            lesson = Lesson::Analysis;
        else if (lesson == Lesson::Analysis)
            if (const auto *c = std::get_if<StartAnalysis>(&cmd)) {
                spell = c->card;
                lesson = Lesson::Waiting;
            }
        if (std::holds_alternative<PrepareCast>(cmd)) {
            if (lesson == Lesson::Preparation)
                lesson = Lesson::Response;
            else if (lesson == Lesson::Retry)
                lesson = Lesson::Release;
        }
    }
    if (actor == 1 && lesson == Lesson::Response && std::holds_alternative<Respond>(cmd))
        countered = true;
    if (lesson == Lesson::Waiting && after.active == 0 && after.phase == Phase::Main) {
        const auto *c = find(after, spell);
        const bool word = std::any_of(after.cards.begin(), after.cards.end(), [](const CardView &x) {
            return x.instance.owner == 1 && x.instance.zone == Zone::Words && x.definition.id == "barbs";
        });
        if (c && c->instance.spell == SpellState::Ready && word)
            lesson = Lesson::Preparation;
    }
    if (lesson == Lesson::Release) {
        const auto *c = find(after, spell);
        if (c && c->instance.zone == Zone::Ash && after.players[1].life <= startingLife - 10)
            lesson = Lesson::Review;
    }
    (void)events;
}
std::optional<ai::Selection> Tutorial::opponent(const GameView &v) const {
    if (lesson == Lesson::Complete)
        return ai::choose(v, ai::Difficulty::Easy);
    if (v.decision) {
        if (v.decision->kind == DecisionKind::Response) {
            if (!countered)
                for (const auto &a : v.actions)
                    if (const auto *c = std::get_if<Respond>(&a.command))
                        if (named(v, c->card, "barbs"))
                            return ai::Selection{a.command, 0, 1};
            if (const auto *a = advance(v))
                return ai::Selection{a->command, 0, 1};
        }
        return ai::choose(v, ai::Difficulty::Easy);
    }
    if (v.phase == Phase::Main) {
        for (const auto &a : v.actions)
            if (const auto *c = std::get_if<AttachSeal>(&a.command)) {
                const auto *host = find(v, c->host);
                if (named(v, c->card, "conduit") && host && host->instance.base)
                    return ai::Selection{a.command, 0, 1};
            }
        if (!countered) {
            const bool hasWord = std::any_of(v.cards.begin(), v.cards.end(), [](const CardView &c) {
                return c.instance.zone == Zone::Words && c.definition.id == "barbs";
            });
            if (!hasWord)
                for (const auto &a : v.actions)
                    if (const auto *c = std::get_if<PreloadWord>(&a.command))
                        if (named(v, c->card, "barbs"))
                            return ai::Selection{a.command, 0, 1};
        }
    }
    if (const auto *a = advance(v))
        return ai::Selection{a->command, 0, 1};
    return {};
}
} // namespace wizard::app
