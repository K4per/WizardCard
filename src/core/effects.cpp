#include "wizard/core/rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
void EffectResolver::apply(GameState &s, const CardCatalog &cat, const Trigger &t, const Effect &e) {
    int p = t.targetPlayer >= 0 ? t.targetPlayer : t.owner;
    if (e.recipient == EffectRecipient::Owner)
        p = t.owner;
    if (e.recipient == EffectRecipient::Opponent)
        p = 1 - t.owner;
    const int previousTemporaryLife = s.players[p].temporaryLife;
    const int previousLife = s.players[p].life;
    const auto previousDeck = s.players[t.owner].deck.size();
    switch (e.kind) {
    case EffectKind::Damage: {
        int amount = Rules::damage(s, cat, p, e);
        const int shield = std::min(amount, s.players[p].temporaryLife);
        s.players[p].temporaryLife -= shield;
        s.players[p].life -= amount - shield;
        break;
    }
    case EffectKind::AddTemporaryLife:
        s.players[p].temporaryLife = std::max(s.players[p].temporaryLife, e.amount);
        break;
    case EffectKind::BlockActions:
        s.players[p].blockedActionTurn = s.players[p].ownTurn + 1;
        break;
    case EffectKind::GrantResistance:
        break; // Successful release installs the passive resistance on the active spell.
    case EffectKind::AddSourceTemporary:
        s.temporary.push_back({s.nextLoad++, p, t.source, e.amount, 0, false, true});
        break;
    case EffectKind::AccelerateAnalysis: {
        auto &c = s.cards.at(t.targetCard);
        c.spell = SpellState::Ready;
        s.temporary.push_back(
            {s.nextLoad++, t.owner, t.source, c.settingPaid, s.players[t.owner].ownTurn + 1});
        break;
    }
    case EffectKind::Conceal: {
        auto &c = s.cards.at(t.targetCard);
        if (c.zone == Zone::Analysis && !c.base && cat.at(c.definition).type == CardType::Analytic) {
            c.faceDown = true;
            c.ambushedTurn = s.globalTurn;
            c.spell = SpellState::None;
            c.analysisStarted = s.players[c.owner].ownTurn;
            s.events.push_back({"conceal", "法术失去解析进度，转为埋伏", -1, c.id});
        }
        break;
    }
    case EffectKind::CounterSpell: {
        if (s.chain)
            for (auto &l : s.chain->links)
                if (l.id == t.targetLink) {
                    const int burden = l.castCost;
                    l.canceled = true;
                    const auto source = l.item.source;
                    StateMaintenance::leave(s, source, &cat);
                    s.temporary.push_back(
                        {s.nextLoad++, t.owner, t.source, burden, s.players[t.owner].ownTurn + 1});
                    break;
                }
        break;
    }
    case EffectKind::Heal:
        s.players[p].life = std::min(maximumLife, s.players[p].life + e.amount);
        break;
    case EffectKind::GainMana: {
        s.players[t.owner].mana = std::min(12, s.players[t.owner].mana + e.amount);
        const auto batch = s.nextBatch++;
        for (const auto &[id, c] : s.cards)
            if (c.owner == t.owner &&
                ((c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation) ||
                 (c.zone == Zone::Casting && c.spell == SpellState::Active)) &&
                !cat.at(c.definition).onMana.empty()) {
                Trigger next;
                next.id = s.nextTrigger++;
                next.batch = batch;
                next.owner = c.owner;
                next.source = id;
                next.targetCard = c.targetCard;
                next.targetPlayer = c.targetPlayer;
                next.target = cat.at(c.definition).target;
                next.targetCards = c.targets;
                next.effects = cat.at(c.definition).onMana;
                s.queue.append(std::move(next));
            }
        break;
    }
    case EffectKind::Draw:
        for (int k = 0; k < e.amount; ++k) {
            auto &pl = s.players[t.owner];
            if (pl.deck.empty()) {
                pl.drawFailed = true;
                continue;
            }
            auto id = pl.deck.back();
            pl.deck.pop_back();
            s.cards.at(id).zone = Zone::Hand;
            s.events.push_back({"draw", "抽取一张牌", t.owner, id});
        }
        break;
    case EffectKind::AddTemporary:
        s.temporary.push_back(
            {s.nextLoad++, p, t.source, e.amount, s.players[p].ownTurn + (p == s.active ? 0 : 1)});
        break;
    case EffectKind::AddIndependent:
        s.temporary.push_back({s.nextLoad++, p, t.source, e.amount, 0, true});
        break;
    case EffectKind::ClearIndependent:
        s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                         [&](const TemporaryLoad &load) {
                                             return load.owner == t.owner && load.independent;
                                         }),
                          s.temporary.end());
        break;
    case EffectKind::Destroy: {
        auto destroy = [&](CardId id) {
            auto i = s.cards.find(id);
            if (i != s.cards.end() && i->second.owner != t.owner &&
                cat.at(i->second.definition).opponentDestroyProtected) {
                s.events.push_back({"protected", "对手效果不能销毁此卡", -1, id});
                return;
            }
            StateMaintenance::leave(s, id, &cat);
        };
        if (t.targetCards.empty())
            destroy(t.targetCard);
        else
            for (auto id : t.targetCards)
                destroy(id);
        break;
    }
    case EffectKind::ClearAllTemporary:
        s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                         [&](const TemporaryLoad &load) {
                                             return load.owner == t.owner && !load.independent;
                                         }),
                          s.temporary.end());
        break;
    case EffectKind::SearchAction:
    case EffectKind::DestroyAmbush:
    case EffectKind::OptionalDestroyOwnFormation:
    case EffectKind::DiscardHand:
    case EffectKind::DiscardFormation:
    case EffectKind::OptionalPrepare:
        throw std::logic_error("effect requires a decision");
    case EffectKind::CancelPreparation:
    case EffectKind::NegateLink:
        if (s.chain)
            for (auto &l : s.chain->links)
                if (l.id == t.targetLink && l.id != (s.effect ? s.effect->link : 0) &&
                    l.kind != LinkKind::Phase &&
                    (e.kind == EffectKind::NegateLink || l.kind == LinkKind::Preparation))
                    l.canceled = true;
        break;
    case EffectKind::ClearTemporary:
        throw std::logic_error("clear temporary requires a decision");
    }
    std::string message;
    switch (e.kind) {
    case EffectKind::Damage:
        message =
            "造成 " + std::to_string(Rules::damage(s, cat, p, e)) + " 点" + damageName(e.damageType) + "伤害";
        break;
    case EffectKind::Heal:
        message = "恢复 " + std::to_string(e.amount) + " 点生命";
        break;
    case EffectKind::Draw:
        message = "抽取 " + std::to_string(e.amount) + " 张牌";
        break;
    case EffectKind::AddIndependent:
        message = "增加 " + std::to_string(e.amount) + " 点独立荷载";
        break;
    case EffectKind::ClearIndependent:
        message = "清除独立荷载";
        break;
    case EffectKind::AddTemporary:
        message = "增加 " + std::to_string(e.amount) + " 点临时荷载";
        break;
    case EffectKind::OptionalDestroyOwnFormation:
    case EffectKind::DiscardHand:
    case EffectKind::DiscardFormation:
    case EffectKind::OptionalPrepare:
    case EffectKind::ClearAllTemporary:
    case EffectKind::ClearTemporary:
        message = "清除临时荷载";
        break;
    case EffectKind::Destroy:
        message = "尝试销毁目标卡牌";
        break;
    case EffectKind::CancelPreparation:
        message = "取消本次准备施法";
        break;
    case EffectKind::GainMana:
        message = "获得 " + std::to_string(e.amount) + " 魔素";
        break;
    case EffectKind::AddSourceTemporary:
        message = "增加来源关联临时荷载";
        break;
    case EffectKind::AddTemporaryLife:
        message = "获得临时生命之核";
        break;
    case EffectKind::BlockActions:
        message = "对手下个自己的回合不能使用行动卡";
        break;
    case EffectKind::CounterSpell:
        message = "无效并销毁目标法术，承担下个自己的结束阶段清除的荷载";
        break;
    case EffectKind::SearchAction:
        message = "检索行动卡";
        break;
    case EffectKind::DestroyAmbush:
        message = "销毁埋伏卡";
        break;
    case EffectKind::AccelerateAnalysis:
        message = "立即完成解析并承担临时荷载";
        break;
    case EffectKind::GrantResistance:
        message = "获得伤害抗性";
        break;
    case EffectKind::Conceal:
        message = "法术转为埋伏";
        break;
    case EffectKind::NegateLink:
        message = "取消链节 #" + std::to_string(t.targetLink);
        break;
    }
    GameEvent event{"effect", message, -1, t.source, e.amount};
    event.effectType = e.kind;
    event.affectedPlayer = p;
    event.actualAmount = std::abs(s.players[p].life - previousLife);
    if (e.kind == EffectKind::Damage)
        event.actualAmount += previousTemporaryLife - s.players[p].temporaryLife;
    if (e.kind == EffectKind::Draw) {
        event.actualAmount = static_cast<int>(previousDeck - s.players[t.owner].deck.size());
        event.affectedPlayer = t.owner;
    }
    s.events.push_back(std::move(event));
}
} // namespace wizard
