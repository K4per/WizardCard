#include "wizard/core/rules.hpp"
#include "wizard/core/card_rules.hpp"
#include "wizard/core/card_rules.hpp"
#include "wizard/core/card_rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
namespace {
bool trait(const CardDefinition &d, const char *name) {
    return std::find(d.traits.begin(), d.traits.end(), name) != d.traits.end();
}
const CardDefinition *effectHost(const GameState &s, const CardCatalog &cat, const Trigger &t) {
    auto source = s.cards.find(t.source);
    if (source == s.cards.end()) return nullptr;
    auto host = s.cards.find(source->second.sourceFormation);
    return host != s.cards.end() && host->second.zone == Zone::Analysis
        ? &cat.at(host->second.definition) : nullptr;
}
bool protectedFromCardEffect(const GameState &s, const CardCatalog &cat, CardId id) {
    const auto card = s.cards.find(id);
    if (card == s.cards.end()) return false;
    const auto &definition = cat.at(card->second.definition);
    if (card_rules::effectImmune(s, cat, id) || trait(definition, "effect_immune") || trait(definition, "effect_indestructible") ||
        card_rules::protectedFormation(s, id)) return true;
    if (!trait(definition, "great_storm")) return false;
    const auto host = s.cards.find(card->second.host ? card->second.host : card->second.sourceFormation);
    return host != s.cards.end() && host->second.zone == Zone::Analysis &&
           cat.at(host->second.definition).id == "storm-heart";
}
int actualLinkedSpellCost(const GameState &s, const ChainLink &link) {
    const auto source = s.cards.find(link.item.source);
    if (source == s.cards.end()) return std::max(0, link.castCost);
    const auto &card = source->second;
    const int setting = std::max(0, card.settingPaid);
    int activation = 0;
    if (link.paymentIndex >= 0 && static_cast<std::size_t>(link.paymentIndex) < card.payments.size())
        activation = card.payments[static_cast<std::size_t>(link.paymentIndex)].paid;
    else if (!card.payments.empty())
        activation = card.payments.back().paid;
    return setting + std::max(0, activation);
}

}
void EffectResolver::apply(GameState &s, const CardCatalog &cat, const Trigger &t, const Effect &e) {
    if (cat.usesAlphaV2() && s.result != -1)
        return;
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
        auto adjusted = e;
        if (cat.usesAlphaV2()) {
            const auto *host = effectHost(s, cat, t);
            const auto source = s.cards.find(t.source);
            if (host && trait(*host, "host_evocation_damage_plus_one") && source != s.cards.end() &&
                cat.at(source->second.definition).school == "evocation") ++adjusted.amount;
            if (!cat.usesAlphaV253() && host && host->id == "sique-campfire" && source != s.cards.end() &&
                card_rules::isFireSpell(cat.at(source->second.definition)) && e.damageType == DamageType::Fire)
                ++adjusted.amount;
        }
        int amount = Rules::damage(s, cat, p, adjusted);
        if (card_rules::damageImmune(s, cat, p)) amount = 0;
        if (cat.usesAlphaV252() && s.players[p].nextDamageImmune && adjusted.amount > 0) {
            s.players[p].nextDamageImmune = false;
            s.events.push_back({"damage_prevented", "教皇：免疫本次伤害", -1, t.source});
        }
        const int shield = std::min(amount, s.players[p].temporaryLife);
        ResourceRules::absorbTemporaryLife(s, p, shield);
        const int actualDamage = amount - shield;
        s.players[p].life -= actualDamage;
        card_rules::damaged(s, cat, p, t.source, actualDamage);
        if (amount > 0 && e.damageType == DamageType::Fire) card_rules::onFireDamage(s, cat);
        break;
    }
    case EffectKind::AddTemporaryLife:
        if (cat.usesAlphaV252()) {
            ResourceRules::grantTemporaryLife(s, p, t.source, e.amount, s.players[p].ownTurn + 1);
        } else {
            s.players[p].temporaryLife = std::max(s.players[p].temporaryLife, e.amount);
        }
        break;
    case EffectKind::BlockActions:
        if (cat.usesAlphaV252()) s.players[t.owner].wordLockUntilOwnDraw = s.players[t.owner].ownTurn + 1;
        else s.players[p].blockedActionTurn = s.players[p].ownTurn + 1;
        break;
    case EffectKind::GrantResistance:
        break; // Successful release installs the passive resistance on the active spell.
    case EffectKind::ShieldForTurn:
        s.players[p].damageReduction = std::max(s.players[p].damageReduction, e.amount);
        s.players[p].forceDamageReduction = std::max(s.players[p].forceDamageReduction, 2 * e.amount);
        break;
    case EffectKind::AddSourceTemporary:
        s.temporary.push_back({s.nextLoad++, p, t.source, e.amount, 0, false, true});
        ResourceRules::increasedLoad(s, cat, p, e.amount);
        break;
    case EffectKind::AccelerateAnalysis: {
        auto &c = s.cards.at(t.targetCard);
        if (cat.usesAlphaV252()) card_rules::advanceAnalysis(s, cat, c.id, std::max(1, e.amount));
        else {
            c.spell = SpellState::Ready;
            if (cat.usesAlphaV2() && !cat.usesAlphaV252()) c.analysisProgress = cat.at(c.definition).analysisTurns;
        }
        s.temporary.push_back(
            {s.nextLoad++, t.owner, t.source, c.settingPaid, s.players[t.owner].ownTurn + 1});
        ResourceRules::increasedLoad(s, cat, t.owner, c.settingPaid);
        break;
    }
    case EffectKind::Conceal: {
        auto &c = s.cards.at(t.targetCard);
        if (c.zone == Zone::Analysis && !c.base && cat.at(c.definition).type == CardType::Analytic) {
            c.faceDown = true;
            c.publiclyRevealed = false;
            c.ambushedTurn = s.globalTurn;
            c.spell = SpellState::None;
            c.analysisStarted = s.players[c.owner].ownTurn;
            s.events.push_back({"conceal", "法术失去解析进度，转为埋伏", -1, c.id});
        }
        break;
    }
    case EffectKind::ReduceSpellDamage:
        if (s.chain)
            for (auto &link : s.chain->links)
                if (link.id == t.targetLink) {
                    for (auto &pending : link.item.effects)
                        if (pending.kind == EffectKind::Damage || pending.kind == EffectKind::ChooseDamage) {
                            pending.amount = std::max(0, pending.amount -
                                (pending.kind == EffectKind::Damage && pending.damageType == DamageType::Force ? 10 : e.amount));
                            break;
                        }
                    break;
                }
        break;
    case EffectKind::RevealAllAmbush:
        for (auto &[id, card] : s.cards)
            if (card.zone == Zone::Analysis && card.faceDown && cat.at(card.definition).type != CardType::Formation) {
                card.faceDown = false;
                card.spell = cat.at(card.definition).type == CardType::Analytic ? SpellState::Analyzing
                                                                            : SpellState::Ready;
                card.analysisStarted = s.players[card.owner].ownTurn;
                card.analysisProgress = 0;
                s.events.push_back({"reveal", "结束阶段反转盖伏法术：" + cat.at(card.definition).name, -1, id});
            }
        break;
    case EffectKind::DelayEnemyAnalysis:
    case EffectKind::AdvanceAllEnemyAnalysis:
    case EffectKind::ConcealEnemySpell:
    case EffectKind::RevealEnemyAmbush:
    case EffectKind::ChooseDamage:
    case EffectKind::DestroyFormationSpells:
    case EffectKind::ReturnEnemyAnalysis:
    case EffectKind::SearchSameSchool:
    case EffectKind::RecoverEnemyAshSpell:
    case EffectKind::RecoverFormation:
    case EffectKind::DestroyEnemyAnalysis:
    case EffectKind::LockAnalysis:
    case EffectKind::SiqueDetachAll:
    case EffectKind::SiqueRecoverSpell:
    case EffectKind::SiqueRecoverSeal:
    case EffectKind::SiqueMillSeal:
    case EffectKind::SiqueMillSpell:
    case EffectKind::SiqueDamageCap:
    case EffectKind::SiqueLifePayment:
    case EffectKind::SiqueSearchPair:
    case EffectKind::SiqueAttachFromDeck:
    case EffectKind::SiqueRecycle:
    case EffectKind::SiqueOptionalDraw:
    case EffectKind::SiqueShuffleHands:
    case EffectKind::RayVolley:
    case EffectKind::DelayAllEnemyAnalysis:
    case EffectKind::SiqueRoastRecover:
    case EffectKind::SiqueBlowfire:
    case EffectKind::SiqueBackfire:
    case EffectKind::ClearAllAmbush:
    case EffectKind::ArcanaFool:
    case EffectKind::ArcanaMagician:
    case EffectKind::ArcanaPriestess:
    case EffectKind::ArcanaEmperor:
    case EffectKind::PreventNextDamage:
    case EffectKind::ArcanaLovers:
    case EffectKind::ArcanaHermit:
    case EffectKind::ArcanaJustice:
    case EffectKind::ArcanaHangedMan:
    case EffectKind::ArcanaDeath:
    case EffectKind::ArcanaDevil:
    case EffectKind::ArcanaTower:
    case EffectKind::ArcanaWorld:
        throw std::logic_error("content effect requires a decision");
    case EffectKind::SiqueNegateDestroy:
        if (s.chain) for (auto &link : s.chain->links) if (link.id == t.targetLink) {
            const auto pending = s.cards.find(link.item.source);
            const bool negatable = pending != s.cards.end() && pending->second.owner != t.owner &&
                (pending->second.zone == Zone::Analysis || pending->second.zone == Zone::Casting) &&
                protectedFromCardEffect(s, cat, link.item.source) == false;
            if (negatable) {
                link.canceled = true;
                StateMaintenance::leave(s, link.item.source, &cat);
            }
            break;
        }
        break;
    case EffectKind::CounterSpell: {
        if (s.chain)
            for (auto &l : s.chain->links)
                if (l.id == t.targetLink) {
                    const auto sourceDefinition = s.cards.find(l.item.source);
                    const int burden = cat.usesAlphaV255()
                        ? actualLinkedSpellCost(s, l)
                        : cat.usesAlphaV25() && sourceDefinition != s.cards.end()
                            ? (cat.at(sourceDefinition->second.definition).type == CardType::Talisman ? 0 :
                               cat.at(sourceDefinition->second.definition).cost) : l.castCost;
                    l.canceled = true;
                    const auto source = l.item.source;
                    StateMaintenance::leave(s, source, &cat);
                    if (!cat.usesAlphaV255()) {
                        s.temporary.push_back(
                            {s.nextLoad++, t.owner, t.source, burden, s.players[t.owner].ownTurn + 1});
                        ResourceRules::increasedLoad(s, cat, t.owner, burden);
                    }
                    break;
                }
        break;
    }
    case EffectKind::Heal: {
        const auto *host = cat.usesAlphaV2() ? effectHost(s, cat, t) : nullptr;
        const int amount = e.amount + (host && trait(*host, "host_heal_plus_one") ? 1 : 0);
        if (cat.usesCardRules()) card_rules::heal(s, cat, p, amount, t.source);
        else s.players[p].life = std::min(maximumLife, s.players[p].life + amount);
        break;
    }
    case EffectKind::GainMana: {
        s.players[t.owner].mana = std::min(cat.usesAlphaV25() ? maximumMana : 12, s.players[t.owner].mana + e.amount);
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
            ResourceRules::drawOne(s, cat, t.owner);
            if (cat.usesAlphaV2() && !s.players[t.owner].debugInfiniteLife && s.players[t.owner].life <= 0)
                break;
        }
        break;
    case EffectKind::AddTemporary:
        s.temporary.push_back(
            {s.nextLoad++, p, t.source, e.amount, s.players[p].ownTurn + (p == s.active ? 0 : 1)});
        ResourceRules::increasedLoad(s, cat, p, e.amount);
        break;
    case EffectKind::AddIndependent:
        s.temporary.push_back({s.nextLoad++, p, t.source, e.amount, 0, true});
        ResourceRules::increasedLoad(s, cat, p, e.amount);
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
            if (i != s.cards.end() && ((i->second.owner != t.owner &&
                cat.at(i->second.definition).opponentDestroyProtected) ||
                (cat.usesAlphaV2() && protectedFromCardEffect(s, cat, id)))) {
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
        message = "获得 " + std::to_string(e.amount) + " 点临时生命";
        break;
    case EffectKind::BlockActions:
        message = "对手下个自己的回合不能使用行动卡";
        break;
    case EffectKind::CounterSpell:
        message = cat.usesAlphaV255()
            ? "无效并销毁目标法术，支付其实际设置与发动费用"
            : "无效并销毁目标法术，承担下个自己的结束阶段清除的荷载";
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
    case EffectKind::ReduceSpellDamage:
        message = "降低目标法术伤害";
        break;
    case EffectKind::ShieldForTurn:
        message = "本回合获得护盾减伤";
        break;
    case EffectKind::RevealAllAmbush:
        message = "揭示场上所有盖伏法术";
        break;
    case EffectKind::DelayEnemyAnalysis:
    case EffectKind::AdvanceAllEnemyAnalysis:
    case EffectKind::ConcealEnemySpell:
    case EffectKind::RevealEnemyAmbush:
    case EffectKind::ChooseDamage:
    case EffectKind::DestroyFormationSpells:
    case EffectKind::ReturnEnemyAnalysis:
    case EffectKind::SearchSameSchool:
    case EffectKind::RecoverEnemyAshSpell:
    case EffectKind::RecoverFormation:
    case EffectKind::DestroyEnemyAnalysis:
    case EffectKind::LockAnalysis:
    case EffectKind::SiqueDetachAll:
    case EffectKind::SiqueRecoverSpell:
    case EffectKind::SiqueRecoverSeal:
    case EffectKind::SiqueMillSeal:
    case EffectKind::SiqueMillSpell:
    case EffectKind::SiqueDamageCap:
    case EffectKind::SiqueLifePayment:
    case EffectKind::SiqueSearchPair:
    case EffectKind::SiqueAttachFromDeck:
    case EffectKind::SiqueRecycle:
    case EffectKind::SiqueOptionalDraw:
    case EffectKind::SiqueShuffleHands:
    case EffectKind::RayVolley:
    case EffectKind::DelayAllEnemyAnalysis:
    case EffectKind::SiqueRoastRecover:
    case EffectKind::SiqueBlowfire:
    case EffectKind::SiqueBackfire:
    case EffectKind::ClearAllAmbush:
    case EffectKind::ArcanaFool:
    case EffectKind::ArcanaMagician:
    case EffectKind::ArcanaPriestess:
    case EffectKind::ArcanaEmperor:
    case EffectKind::PreventNextDamage:
    case EffectKind::ArcanaLovers:
    case EffectKind::ArcanaHermit:
    case EffectKind::ArcanaJustice:
    case EffectKind::ArcanaHangedMan:
    case EffectKind::ArcanaDeath:
    case EffectKind::ArcanaDevil:
    case EffectKind::ArcanaTower:
    case EffectKind::ArcanaWorld:
        message = "选择卡牌效果";
        break;
    case EffectKind::SiqueNegateDestroy:
        message = "无效并销毁目标法术";
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
    if (e.kind == EffectKind::AddTemporaryLife)
        event.actualAmount = s.players[p].temporaryLife - previousTemporaryLife;
    if (e.kind == EffectKind::Draw) {
        event.actualAmount = static_cast<int>(previousDeck - s.players[t.owner].deck.size());
        event.affectedPlayer = t.owner;
    }
    s.events.push_back(std::move(event));
    if (cat.usesAlphaV2())
        StateMaintenance::check(s, cat);
}
} // namespace wizard
#include "effects_v2.inc"
