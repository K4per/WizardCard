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

} // namespace
MatchConfig tutorialConfiguration(const Content &) {
    MatchConfig c;

        c.seed = 5; // Recorded shuffle: learner opens fireball + conduit, opponent opens shield.
        for (auto &player : c.players) {
            player.baseFormation = "small-hexagram";
            player.sideDeck = {"small-hexagram", "small-hexagram", "small-hexagram", "hexagram", "hexagram",
                "hexagram", "reservoir", "reservoir", "elemental-ring", "engeas-four-point-star"};
            for (const auto *id : {"fireball", "conduit", "shield", "spark", "recall", "clarity", "ring",
                                  "aid", "mend", "ward", "ray-of-frost", "unravel", "arcane-recovery"})
                player.cards.insert(player.cards.end(), 3, id);
            player.cards.push_back("instant-circuit-overload");
        }
        return c;

}
std::string Tutorial::title() const {
    return std::array<const char *, 12>{"欢迎来到巫师牌", "抽牌与魔素", "设置进阶阵法", "附着符文",
        "开始解析", "等待解析完成", "准备施法", "观察护盾与连锁", "继续练习", "发动法术", "结束阶段清理", "教学完成"}
        .at(static_cast<std::size_t>(lesson));
}
std::string Tutorial::hint(const GameView &v) const {

        switch (lesson) {
        case Lesson::Intro: return v.opening ? "先从副卡组确认一张等级1阵法。双方确认后同时公开设置，再洗牌并抽取起手。" : "使用固定种子的40张主卡组与10张副卡组，初始小六芒星来自副卡组。对手手牌和盖伏卡身份保密。";
        case Lesson::Resources: return "抽牌阶段先获得阵法魔素收入，再抽牌；先手第一回合跳过抽牌。结束当前阶段，进入主要阶段。";
        case Lesson::Formation: return "从副卡组选择六芒星，以场上的小六芒星和2魔素支付配方。3级阵法才能承载3级火球术。";
        case Lesson::Rune: return "将逆三角星符附着于六芒星，使后续抽牌阶段魔素收入+2。魔素不足时先推进到下一回合。";
        case Lesson::Analysis: return "把火球术设置到六芒星：支付3魔素、占用环位并承担3点绑定荷载。魔素不足时推进回合。";
        case Lesson::Waiting: return "己方下次准备阶段推进解析。教学对手将先免费盖伏护盾术，再等待后续回合的发动时机。";
        case Lesson::Preparation: return "把解析完成的火球术移入施法区，准备施法不支付魔素。它仍需在合法时机发动。";
        case Lesson::Release: return "进入施法阶段，发动火球术，支付1魔素并增加1点绑定荷载。发动后对手可响应。";
        case Lesson::Response: return countered ? "护盾术将火球术的一次伤害降低5点。双方连续放弃后连锁逆序结算；已支付的费用不退还。"
                                               : "对手可发动此前盖伏的护盾术。响应必须来自场上；新增链节后优先权交接。";
        case Lesson::Review: return "火球术完成结算后仍留在施法区；双方施法区的普通法术在结束阶段清理，随后检查回合玩家过载。继续即可完成教学。";
        case Lesson::Retry: return "推进阶段并继续练习。";
        case Lesson::Complete: return "已完成40+10构筑、配方、符文、解析、准备、发动与连锁教学。可以继续对局，或从设置返回菜单。";
        }

return {};
}
bool Tutorial::canContinue(const GameView &v) const {
    return !v.opening && (lesson == Lesson::Intro || lesson == Lesson::Review);
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
bool Tutorial::allows(const GameView &v, const Command &cmd, const ai::CandidateSource &source) const {
    if (v.opening) return std::holds_alternative<SelectInitialFormation>(cmd) || std::holds_alternative<Surrender>(cmd);
    if (std::holds_alternative<Surrender>(cmd) || lesson == Lesson::Complete)
        return true;
    if (lesson == Lesson::Intro || lesson == Lesson::Review || canContinue(v))
        return false;
    if (v.decision)
        return true; // All forced choices still go through the ordinary core.

        auto relevant = [&](const Command &candidate) {
            if (lesson == Lesson::Formation) {
                const auto *c = std::get_if<SetFormation>(&candidate);
                return c && named(v, c->card, "hexagram");
            }
            if (lesson == Lesson::Rune) {
                const auto *c = std::get_if<AttachSeal>(&candidate);
                return c && named(v, c->card, "conduit") && named(v, c->host, "hexagram");
            }
            if (lesson == Lesson::Analysis) {
                const auto *c = std::get_if<StartAnalysis>(&candidate);
                return c && named(v, c->card, "fireball") && named(v, c->formation, "hexagram");
            }
            if (lesson == Lesson::Preparation) {
                const auto *c = std::get_if<PrepareCast>(&candidate);
                return c && c->card == spell;
            }
            if (lesson == Lesson::Release) {
                const auto *c = std::get_if<ActivateSpell>(&candidate);
                return c && c->card == spell;
            }
            return false;
        };
        auto hasRelevant = [&] {
            bool found = false;
            if (source) source([&](const LegalAction &action) { found = relevant(action.command); return !found; });
            return found;
        };
        if (relevant(cmd)) return true;
        return std::holds_alternative<AdvancePhase>(cmd) &&
            !hasRelevant();

}
void Tutorial::observe(PlayerId actor, const Command &cmd, const GameView &after,
                       const std::vector<GameEvent> &events) {
    (void)events;

        if (lesson == Lesson::Resources && after.active == 0 && after.phase == Phase::Main) lesson = Lesson::Formation;
        if (actor == 0) {
            if (lesson == Lesson::Formation && std::holds_alternative<SetFormation>(cmd)) lesson = Lesson::Rune;
            else if (lesson == Lesson::Rune && std::holds_alternative<AttachSeal>(cmd)) lesson = Lesson::Analysis;
            else if (lesson == Lesson::Analysis) {
                if (const auto *c = std::get_if<StartAnalysis>(&cmd)) { spell = c->card; lesson = Lesson::Waiting; }
            } else if (lesson == Lesson::Preparation && std::holds_alternative<PrepareCast>(cmd)) lesson = Lesson::Release;
            else if (lesson == Lesson::Release && std::holds_alternative<ActivateSpell>(cmd)) lesson = Lesson::Response;
        }
        if (actor == 1 && std::holds_alternative<Respond>(cmd)) countered = true;
        const auto *source = find(after, spell);
        if (lesson == Lesson::Waiting && source && source->instance.spell == SpellState::Ready && after.active == 0)
            lesson = Lesson::Preparation;
        if (lesson == Lesson::Response && source && source->instance.spell == SpellState::Resolved && !after.chain)
            lesson = Lesson::Review;
        return;

}
std::optional<ai::Selection> Tutorial::opponent(const GameView &v, const ai::CandidateSource &source) const {
    if (v.opening || lesson == Lesson::Complete) return ai::choose(v, source, ai::Difficulty::Easy);
    auto pick = [&](const auto &predicate) -> std::optional<ai::Selection> {
        std::optional<ai::Selection> selected;
        source([&](const LegalAction &action) {
            if (predicate(action.command)) { selected = ai::Selection{action.command,0,1}; return false; }
            return true;
        });
        return selected;
    };
    if (v.decision) {
        if (v.decision->kind == DecisionKind::Response && !countered) {
            auto response = pick([&](const Command &cmd) {
                const auto *c = std::get_if<Respond>(&cmd); return c && named(v,c->card,"shield");
            });
            if (response) return response;
        }
        if (v.actionSummary.safeAdvance) return ai::Selection{*v.actionSummary.safeAdvance,0,1};
        return ai::choose(v, source, ai::Difficulty::Easy);
    }
    if (v.phase == Phase::Main) {
        auto formation = pick([&](const Command &cmd) {
            const auto *c=std::get_if<SetFormation>(&cmd); return c && named(v,c->card,"hexagram");
        });
        const bool hasFormation=std::any_of(v.cards.begin(),v.cards.end(),[](const CardView &c){
            return c.instance.owner==1 && c.instance.zone==Zone::Analysis && c.definition.id=="hexagram";
        });
        if(!hasFormation && formation) return formation;
        const bool hasShield = std::any_of(v.cards.begin(),v.cards.end(),[](const CardView &c) {
            return c.instance.owner == 1 && c.instance.zone == Zone::Analysis && c.definition.id == "shield";
        });
        if(!countered && !hasShield) {
            auto ambush=pick([&](const Command &cmd){const auto*c=std::get_if<SetAmbush>(&cmd);return c&&named(v,c->card,"shield");});
            if(ambush) return ambush;
        }
    }
    if(v.actionSummary.safeAdvance) return ai::Selection{*v.actionSummary.safeAdvance,0,1};
    return {};
}
} // namespace wizard::app
