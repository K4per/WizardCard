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
MatchConfig tutorialConfiguration(const Content &content, int chapter) {
    MatchConfig c;
    if (content.catalog.mechanicsRevision >= 12) {
        c.seed = 263;
        c.tutorialChapter = chapter == 0 ? 1 : chapter;
        if (c.tutorialChapter < 1 || c.tutorialChapter > 5) throw std::invalid_argument("无效教学章节");
        for (auto &player : c.players) {
            for (int n = 1; n <= 11; ++n) {
                const auto id = "v260-analytic-" + std::string(n < 10 ? "000" : "00") + std::to_string(n);
                player.cards.insert(player.cards.end(), 3, id);
            }
            player.cards.insert(player.cards.end(), 3, "v260-word-0001");
            player.cards.insert(player.cards.end(), 3, "v260-talisman-0003");
            player.cards.push_back("v260-analytic-0012");
            player.sideDeck = {"v260-formation-0009", "v260-formation-0009", "v260-formation-0009",
                "v260-formation-0010", "v260-formation-0010", "v260-formation-0010",
                "v260-formation-0011", "v260-formation-0011", "v260-formation-0016", "v260-formation-0016"};
        }
        return c;
    }

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
    if (chapter) return std::array<const char *, 5>{"第一章 · 认识场地", "第二章 · 开局与阵法",
        "第三章 · 火球术", "第四章 · 言灵火花", "第五章 · 护盾与连锁"}.at(chapter - 1) +
        std::string(lesson == Lesson::Complete ? " · 已完成" : "");
    return std::array<const char *, 12>{"欢迎来到巫师牌", "抽牌与魔素", "设置进阶阵法", "附着符文",
        "开始解析", "等待解析完成", "准备施法", "观察护盾与连锁", "继续练习", "发动法术", "结束阶段清理", "教学完成"}
        .at(static_cast<std::size_t>(lesson));
}
std::string Tutorial::hint(const GameView &v) const {
    if (chapter) {
        if (lesson == Lesson::Complete) return "本章已完成。可返回巫师指南选择章节，或重试本章。";
        if (chapter == 1) return std::array<const char *, 9>{
            "点击左下方己方状态栏：生命40、临时生命0、魔素20。荷载上限为固定2点加阵法合计及修正。",
            "点击右上方对方状态栏，区分双方资源。生命降为0败北；临时生命优先抵消普通伤害。",
            "点击一张己方手牌，查看左侧属性、效果和风味。手牌上限8，结束阶段处理超额。",
            "点击己方牌库。牌库内容保持未知，抽牌从这里取得卡牌。",
            "点击己方副卡组，查看阵法。副卡组10张，阵法卡不进入手牌。",
            "关闭卡牌列表后，点击己方高阶法阵：查看等级、阵体、环位、收入和荷载上限。五个阵法槽的相邻部分属于同一阵法。",
            "点击中央己方施法区横带。解析区法术解析完成后，准备到这里发动；容量来自有效环位。",
            "点击己方灰烬区。离场卡会进入这里，当前为空。",
            "关闭列表后，点击右侧阶段文字。辨认回合玩家和当前主要阶段；等待操作方显示在左侧连锁区。"}.at(step);
        if (chapter == 2) return v.opening ?
            "双方秘密选择石头、布或剪刀，平局重来、胜者先手。双方先抽5张，再选择基础法阵，同时公开；初始设置免费。" :
            "进入己方主要阶段。点击副卡组中的标准法阵，使用场上的基础法阵和2魔素完成付费升级。";
        if (chapter == 3) return std::array<const char *, 5>{
            "点击火球术并开始解析，选择高亮环位；设置费用为3魔素。教学已提供合法高阶阵法。",
            "推进阶段和回合。火球术在己方下次准备阶段解析完成，对手会放弃响应。",
            "点击解析完成的火球术，选择准备施法，移入施法区；准备不支付魔素。",
            "进入施法阶段，发动火球术并选择对方；发动消耗1魔素，造成7点火焰伤害。",
            "双方放弃响应，观察连锁结算：对方生命从40降至33。"}.at(step);
        if (chapter == 4) return step == 0 ? "点击火花，选择阵法环位和对方目标。言灵设置即发动，共支付2魔素。" :
            "双方放弃响应后结算火花，实际造成3点火焰伤害。";
        return std::array<const char *, 4>{
            "点击护盾术并选择空环位盖伏（免费）。预置的另一张盖伏卡用于演示飞弹的可选后续。",
            "推进回合，让护盾满足等待期限。对手将设置、解析、准备并发动魔法飞弹。",
            "魔法飞弹已发动。选择响应“是”，再点击盖伏护盾并确认发动，支付3魔素。",
            "放弃后续响应以逆序结算：护盾先降低本回合力场伤害6点，飞弹的4点减至0；飞弹仍可销毁另一张盖伏卡。"}.at(std::min(step, 3));
    }

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
    if (chapter) return chapter == 1 && lesson != Lesson::Complete;
    return !v.opening && (lesson == Lesson::Intro || lesson == Lesson::Review);
}
void Tutorial::continueLesson(const GameView &v) {
    if (chapter) {
        if (canContinue(v) && ++step == 9) lesson = Lesson::Complete;
        return;
    }
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
    if (chapter) {
        if (std::holds_alternative<Surrender>(cmd)) return true;
        if (lesson == Lesson::Complete || chapter == 1) return false;
        if (v.opening) {
            if (const auto *c = std::get_if<SelectInitialFormation>(&cmd)) return named(v, c->card, "v260-formation-0009");
            return std::holds_alternative<SubmitOpeningGesture>(cmd);
        }
        const auto relevant = [&](const Command &candidate) {
            if (chapter == 2) {
                const auto *c = std::get_if<SetFormation>(&candidate);
                return c && named(v, c->card, "v260-formation-0010");
            }
            if (chapter == 3) {
                if (step == 0) { const auto *c = std::get_if<StartAnalysis>(&candidate); return c && named(v, c->card, "v260-analytic-0001"); }
                if (step == 2) { const auto *c = std::get_if<PrepareCast>(&candidate); return c && c->card == spell; }
                if (step == 3) { const auto *c = std::get_if<ActivateSpell>(&candidate); return c && c->card == spell && c->target == playerTarget(1); }
            }
            if (chapter == 4 && step == 0) { const auto *c = std::get_if<PreloadWord>(&candidate); return c && named(v, c->card, "v260-word-0001") && c->target == playerTarget(1); }
            if (chapter == 5) {
                if (step == 0) { const auto *c = std::get_if<SetAmbush>(&candidate); return c && named(v, c->card, "v260-talisman-0003"); }
                if (step == 2) {
                    if (const auto *c = std::get_if<FlipAmbush>(&candidate)) return c->decision && named(v, c->card, "v260-talisman-0003");
                    const auto *c = std::get_if<Respond>(&candidate); return c && named(v, c->card, "v260-talisman-0003");
                }
            }
            return false;
        };
        if (relevant(cmd)) return true;
        if (v.decision && v.decision->kind != DecisionKind::Response) return std::holds_alternative<Choose>(cmd);
        bool hasRelevant = false;
        if (source) source([&](const LegalAction &action) { hasRelevant = relevant(action.command); return !hasRelevant; });
        return !hasRelevant && (std::holds_alternative<AdvancePhase>(cmd) || std::holds_alternative<PassResponse>(cmd));
    }
    if (v.opening) return std::holds_alternative<SelectInitialFormation>(cmd) ||
        std::holds_alternative<SubmitOpeningGesture>(cmd) || std::holds_alternative<Surrender>(cmd);
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
    if (chapter) {
        if (lesson == Lesson::Complete) return;
        for (const auto &event : events)
            if (event.effectType == EffectKind::Damage && event.affectedPlayer == 1) damageObserved += event.actualAmount;
        if (chapter == 2 && actor == 0 && std::holds_alternative<SetFormation>(cmd)) lesson = Lesson::Complete;
        if (chapter == 3) {
            if (actor == 0) {
                if (step == 0) { if (const auto *c = std::get_if<StartAnalysis>(&cmd)) { spell = c->card; step = 1; } }
                else if (step == 2 && std::holds_alternative<PrepareCast>(cmd)) step = 3;
                else if (step == 3 && std::holds_alternative<ActivateSpell>(cmd)) step = 4;
            }
            const auto *card = find(after, spell);
            if (step == 1 && card && card->instance.spell == SpellState::Ready) step = 2;
            if (step == 4 && !after.chain && !after.decision && damageObserved == 7) lesson = Lesson::Complete;
        }
        if (chapter == 4) {
            if (actor == 0 && std::holds_alternative<PreloadWord>(cmd)) step = 1;
            if (step == 1 && !after.chain && !after.decision && damageObserved == 3) lesson = Lesson::Complete;
        }
        if (chapter == 5) {
            if (actor == 0 && step == 0 && std::holds_alternative<SetAmbush>(cmd)) step = 1;
            if (actor == 1) if (const auto *c = std::get_if<ActivateSpell>(&cmd)) { spell = c->card; step = 2; }
            if (actor == 0 && step == 2 && (std::holds_alternative<Respond>(cmd) || std::holds_alternative<FlipAmbush>(cmd))) { countered = true; step = 3; }
            const auto *card = find(after, spell);
            if (step == 3 && countered && card && card->instance.spell == SpellState::Resolved &&
                !after.chain && !after.decision && after.players[0].life == 40) lesson = Lesson::Complete;
        }
        return;
    }

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
    if (chapter) {
        if (lesson == Lesson::Complete || chapter == 1) return {};
        std::optional<ai::Selection> selected;
        source([&](const LegalAction &action) {
            const auto &c = action.command;
            bool wanted = false;
            if (v.opening) {
                if (const auto *gesture = std::get_if<SubmitOpeningGesture>(&c)) wanted = gesture->gesture == 2;
                if (const auto *formation = std::get_if<SelectInitialFormation>(&c)) wanted = named(v, formation->card, "v260-formation-0009");
            } else if (chapter == 5) {
                if (const auto *analysis = std::get_if<StartAnalysis>(&c)) wanted = named(v, analysis->card, "v260-analytic-0004") &&
                    std::none_of(v.cards.begin(), v.cards.end(), [](const CardView &card) { return card.instance.owner == 1 &&
                        card.definition.id == "v260-analytic-0004" && (card.instance.zone == Zone::Analysis || card.instance.zone == Zone::Casting); });
                if (const auto *prepare = std::get_if<PrepareCast>(&c)) wanted = named(v, prepare->card, "v260-analytic-0004");
                if (const auto *cast = std::get_if<ActivateSpell>(&c)) wanted = named(v, cast->card, "v260-analytic-0004") && cast->target == playerTarget(0);
                if (const auto *choice = std::get_if<Choose>(&c)) wanted = v.decision && v.decision->kind == DecisionKind::DestroyAmbush && bool(choice->option);
            }
            if (wanted) { selected = ai::Selection{c, 0, 1}; return false; }
            return true;
        });
        if (selected) return selected;
        if (v.actionSummary.safeAdvance) return ai::Selection{*v.actionSummary.safeAdvance, 0, 1};
        return {};
    }
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
