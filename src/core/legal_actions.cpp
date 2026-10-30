#include "wizard/core/engine.hpp"
#include "engine_internal.hpp"
#include "wizard/core/card_rules.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wizard {
namespace { struct StopActionVisit {}; }
void GameEngine::visitLegalActions(PlayerId p, const std::function<bool(const LegalAction &)> &visitor, const ActionIntent *intent) const {
    try {
    const auto &s = state_;
    if (p < 0 || p > 1) return;
    std::optional<unsigned> selectedOperation;
    CardId selectedSource = intent ? intent->source : 0;
    if (intent) for (const auto &selection : intent->selections) {
        if (selection.field == ActionField::Operation) selectedOperation = static_cast<unsigned>(std::stoul(selection.value));
        if (selection.field == ActionField::Source) selectedSource = static_cast<CardId>(std::stoul(selection.value));
    }
    auto add = [&](std::string label, CardId source, CardId target, Command cmd) {
        if (selectedOperation && static_cast<unsigned>(commandKind(cmd)) != *selectedOperation) return;
        if (selectedSource && source != selectedSource) return;

        if (detail::matchesIntent(cmd, intent) && validate(p, cmd).empty())
            if (!visitor({std::move(label), source, target, std::move(cmd)})) throw StopActionVisit{};
    };
    if (s.result != -1)
        return;
    if (s.opening) {
        if (!s.initialFormations[p])
            for (const auto &[id, card] : s.cards)
                if (controllerOf(card) == p && card.zone == Zone::SideDeck &&
                    catalog().at(card.definition).type == CardType::Formation && catalog().at(card.definition).level == 1)
                    add("选择初始阵法 " + catalog().at(card.definition).name, id, 0, SelectInitialFormation{id});
        add("投降", 0, 0, Surrender{});
        return;
    }
    if (s.decision) {
        const auto &d = *s.decision;
        if (d.player != p)
            return;
        if (d.kind == DecisionKind::Payment && s.payment) {
            for (std::size_t index = 0; index < s.payment->choices.size(); ++index) {
                const auto &choice = s.payment->choices[index];
                const auto label = "支付 " + std::to_string(choice.mana) + " 魔素 / " +
                    std::to_string(choice.life) + (choice.lifePlayer == p ? " 己方生命" : " 对方生命");
                add(label, 0, 0, Choose{d.id, {ChoiceKind::Payment, index + 1}});
            }
            add("取消支付", 0, 0, Choose{d.id, 0});
            return;
        }
        if (d.kind == DecisionKind::Response) {
            {
                visitResponseSetups(p, d.id, [&](const LegalAction &setup) { if (!visitor(setup)) throw StopActionVisit{}; }, intent);
            }
            visitResponses(p, d.id, [&](const Respond &r) {
                const auto &source = s.cards.at(r.card);
                const auto &definition = catalog().at(source.definition);
                auto label = "响应 " + definition.name;
                for (const auto &ability : definition.responses)
                    if (ability.id == r.ability) {
                        const int paid = Rules::castCost(s, catalog(), source.id);
                        const int added = ability.burden + paid;
                        label += " · " + (ability.name.empty() ? ability.id : ability.name) + "（魔素" +
                                 std::to_string(paid) + " / 新增荷载" + std::to_string(added) + "）";
                    }
                if (r.link)
                    label += " → 链节 #" + std::to_string(r.link);
                for (auto target : r.targets)
                    label += " → 卡牌 #" + std::to_string(target);
                if (r.discard)
                    label += "；弃置 #" + std::to_string(r.discard);
                if (!visitor({label, r.card, r.target, r})) throw StopActionVisit{};
            }, intent);
            add("放弃响应", 0, 0, PassResponse{d.id});
            add("投降", 0, 0, Surrender{});
            return;
        }
        if (d.kind == DecisionKind::EffectPrepare) {
            for (auto cmd : preparationsFor(p, d.id)) {
                std::string label = "准备施法 " + catalog().at(s.cards.at(cmd.card).definition).name;
                for (auto id : cmd.targets)
                    label += " → #" + std::to_string(id);
                if (cmd.discard)
                    label += "；弃置 #" + std::to_string(cmd.discard);
                add(label, cmd.card, cmd.target, std::move(cmd));
            }
            add("放弃额外准备施法", 0, 0, Choose{d.id, 0});
            add("投降", 0, 0, Surrender{});
            return;
        }
        for (const auto option : d.options) {
            const auto id = static_cast<CardId>(option.value);
            std::string label;
            if (option.kind == ChoiceKind::Player) label = id == static_cast<CardId>(p) ? "选择己方玩家" : "选择对方玩家";
            else if (option.kind == ChoiceKind::Load) label = "清除荷载来源 #" + std::to_string(id) + " 的1点荷载";
            else if (option.kind == ChoiceKind::Mode) {
                switch (static_cast<ChoiceMode>(id)) {
                case ChoiceMode::Confirm: label = "确认执行此效果"; break;
                case ChoiceMode::SevenDamage: label = "受到7点光耀伤害"; break;
                case ChoiceMode::SevenLoad: label = "获得7点独立荷载"; break;
                case ChoiceMode::DelayAnalysis: label = "延长对方全部解析法术的解析时间"; break;
                case ChoiceMode::CycloneDamage: label = "造成大旋风追加伤害"; break;
                case ChoiceMode::Draw: label = "抽取效果指定数量的牌"; break;
                case ChoiceMode::StormSearch: label = "检索一张风暴法术加入手牌"; break;
                case ChoiceMode::StormPoints: label = "使己方风暴阵法获得2点风暴点数"; break;
                case ChoiceMode::SupportingSearch: label = "检索云雾术或雷鸣召唤术加入手牌"; break;
                case ChoiceMode::HeartDraw: label = "抽1张牌"; break;
                case ChoiceMode::HeartRecover: label = "将风暴卡放回牌库"; break;
                case ChoiceMode::ArcanaDeathNecrotic: label = "选择恶咒伤害"; break;
                case ChoiceMode::ArcanaDeathRadiant: label = "选择光耀伤害"; break;
                default: label = "选择" + damageName(static_cast<DamageType>(id - static_cast<unsigned>(ChoiceMode::DamageTypeBase))) + "伤害"; break;
                }
            } else if (option.kind == ChoiceKind::Trigger) {
                for (const auto &trigger : s.queue.items) if (trigger.id == id)
                    label = "结算被动触发 " + catalog().at(s.cards.at(trigger.source).definition).name;
            } else if (d.kind == DecisionKind::ReturnSpell) {
                for (int ring = 0; ring < Rules::rings(s, catalog(), id); ++ring) {
                    bool occupied = false;
                    for (const auto &[other, card] : s.cards)
                        occupied |= other != s.maintenanceCard() && card.zone == Zone::Analysis && card.host == id && card.ringIndex == ring;
                    if (!occupied) add("回位至 " + catalog().at(s.cards.at(id).definition).name + " 环位 " + std::to_string(ring + 1),
                        s.maintenanceCard(), id, Choose{d.id, option, ring});
                }
                continue;
            } else if (d.kind == DecisionKind::ClearAnyLoad) label = "清除 " + catalog().at(s.cards.at(id).definition).name + " 的1点绑定荷载";
            else if (d.kind == DecisionKind::Concentration) {
                const auto fee = Rules::concentrationUpkeep(s, catalog(), id);
                label = "维持专注（魔素 " + std::to_string(fee) + " / 荷载 +" + std::to_string(fee) + "）";
            } else label = "选择 " + catalog().at(s.cards.at(id).definition).name + " #" + std::to_string(id);
            add(label, option.kind == ChoiceKind::Card ? id : 0, 0, Choose{d.id, option});
        }
        if (d.mayPass)
            add(d.kind == DecisionKind::Concentration ? "销毁专注法术"
                : d.kind == DecisionKind::CastOrder   ? "本阶段不再释放"
                                                      : "跳过 / 停止",
                0, 0, Choose{d.id, 0});
        add("投降", 0, 0, Surrender{});
        return;
    }
    if (s.active != p)
        return;
    {
        for (const auto &[id, card] : s.cards) {
            if (selectedSource && id != selectedSource) continue;
            if (controllerOf(card) != p) continue;
            const auto &def = catalog().at(card.definition);
            const auto name = def.name + " #" + std::to_string(id);
            const auto activationSets = detail::activationSets(s, catalog(), p, def, intent);
            const auto activationLabel = [&](const std::vector<CardId> &costs) {
                std::string label;
                if (card_rules::detachCost(def)) for (auto cost : costs)
                    label += "；拆卸 " + catalog().at(s.cards.at(cost).definition).name + " #" + std::to_string(cost);
                return label;
            };
            if (card.zone == Zone::Analysis && card.stormPoints >= 2 &&
                card.stormAbilityTurn != s.players[p].ownTurn &&
                hasCapability(def, Capability::ActivateStormSearch)) {
                const bool canSearch = std::any_of(s.players[p].deck.begin(), s.players[p].deck.end(), [&](CardId deckId) {
                    const auto &candidate = s.cards.at(deckId);
                    const auto &candidateDefinition = catalog().at(candidate.definition);
                    const bool spell = candidateDefinition.type == CardType::Analytic ||
                        candidateDefinition.type == CardType::Word || candidateDefinition.type == CardType::Talisman;
                    return candidate.zone == Zone::Deck && spell &&
                        detail::stormTags(candidateDefinition.tags);
                });
                if (canSearch)
                    add("发动风暴纹章能力 " + name, id, 0, ActivateAbility{id, 0, "storm_search"});
            }
            if (card.zone == Zone::Analysis && card.stormPoints >= 2 &&
                card.stormAbilityTurn != s.players[p].ownTurn &&
                hasCapability(def, Capability::ActivateStormHeartChoice))
                add("发动风暴之心能力 " + name, id, 0, ActivateAbility{id, 0, "storm_heart_choice"});
            std::vector<CardId> spellCosts = card_rules::costOptions(s, catalog(), p, id);
            if (hasCapability(def, Capability::DiscardSpellSearch)) {
                spellCosts.clear();
                for (const auto &[material, candidate] : s.cards)
                    if (material != id && controllerOf(candidate) == p && candidate.zone == Zone::Hand &&
                        (catalog().at(candidate.definition).type == CardType::Analytic ||
                         catalog().at(candidate.definition).type == CardType::Word ||
                         catalog().at(candidate.definition).type == CardType::Talisman)) spellCosts.push_back(material);
            }
            if (card.zone == Zone::SideDeck && (!selectedOperation || *selectedOperation == static_cast<unsigned>(CommandKind::SetFormation))) {
                int count = 0;
                for (const auto &part : def.recipe)
                    if (part.kind != RecipeCostKind::Mana && part.kind != RecipeCostKind::Life) count += part.amount;
                std::vector<CardId> pool, selected;
                for (const auto &[materialId, material] : s.cards)
                        if (controllerOf(material) == p && (material.zone == Zone::Analysis || material.zone == Zone::Casting ||
                        material.zone == Zone::Hand) && !material.faceDown)
                        pool.push_back(materialId);
                auto choose = [&](auto &&self, std::size_t pos) -> void {
                    if (static_cast<int>(selected.size()) == count) {
                        auto add_formation = [&](CardId replace) {
                            if (!detail::intentAllows(intent, ActionField::Replacement, replace)) return;
                            if (!validate(p, SetFormation{id, replace, selected}).empty()) return;
                            if (!replace)
                                add("设置阵法 " + name, id, 0, SetFormation{id, 0, selected});
                            else
                                add("替换 " + catalog().at(s.cards.at(replace).definition).name + " 为 " + name,
                                    id, replace, SetFormation{id, replace, selected});
                        };
                        add_formation(0);
                        for (const auto &[host, formation] : s.cards)
                            if (controllerOf(formation) == p && formation.zone == Zone::Analysis &&
                                catalog().at(formation.definition).type == CardType::Formation)
                                add_formation(host);
                        return;
                    }
                    const auto prefix = detail::intentPrefix(intent, ActionField::Material);
                    for (std::size_t i = pos; i < pool.size(); ++i) {
                        if (selected.size() < prefix.size() && pool[i] != prefix[selected.size()]) continue;
                        selected.push_back(pool[i]); self(self, i + 1); selected.pop_back();
                    }
                };
                if (count <= 5 && s.phase == Phase::Main) choose(choose, 0);
            }
            if (card.zone == Zone::Hand && (!selectedOperation || *selectedOperation == static_cast<unsigned>(CommandKind::SetAmbush) || *selectedOperation == static_cast<unsigned>(CommandKind::StartAnalysis) || *selectedOperation == static_cast<unsigned>(CommandKind::PreloadWord) || *selectedOperation == static_cast<unsigned>(CommandKind::AttachSeal))) {
                for (const auto &[host, formation] : s.cards) {
                    if (!detail::intentAllows(intent, ActionField::Host, host) || controllerOf(formation) != p) continue;
                    if (formation.zone == Zone::Analysis && catalog().at(formation.definition).type == CardType::Formation) {
                        add("盖伏 " + name, id, host, SetAmbush{id, host});
                        add("设置解析法术 " + name, id, host, StartAnalysis{id, host});
                        for (const auto &ids : activationSets) {
                            const auto target = card_rules::detachCost(def) || ids.empty() ? 0 : ids.front();
                            for (auto cost : spellCosts)
                                add("设置并发动 " + name + (cost ? "；弃置 #" + std::to_string(cost) : "") + activationLabel(ids), id, target, PreloadWord{id, true, host, target, ids, cost});
                        }
                    }
                    if (formation.zone == Zone::Analysis || formation.zone == Zone::Casting)
                        add("附着 " + name, id, host, AttachSeal{id, host});
                }
            }
            if (card.faceDown || card.spell == SpellState::Ready) {
                // Shatter Ring has no meaningful target during preparation;
                // choose the empty enemy formation when it is released.
                const std::vector<std::vector<CardId>> prepareTargets{{}};
                for (const auto &ids : prepareTargets) {
                    const auto target = ids.empty() ? 0 : ids.front();
                    if (!card.faceDown) {
                        if (card.zone == Zone::Analysis)
                            add("准备施法 " + name, id, target, PrepareCast{id, target, 0, ids});
                    }
                }
                for (const auto &ids : activationSets) {
                    const auto target = card_rules::detachCost(def) || ids.empty() ? 0 : ids.front();
                    for (auto cost : spellCosts) {
                        if (card.faceDown && def.type != CardType::Analytic)
                            add("反转 " + name + activationLabel(ids), id, target, FlipAmbush{id, target, ids, 0, cost});
                        if (!card.faceDown && card.zone == Zone::Casting)
                            add("发动 " + name + activationLabel(ids), id, target, ActivateSpell{id, target, ids, cost});
                    }
                }
                if (card.faceDown && def.type == CardType::Analytic)
                    add("反转 " + name, id, 0, FlipAmbush{id});
            }
            add("放弃 " + name, id, 0, Abandon{id});

            add("移除阵法 " + name, id, 0, RemoveFormation{id});
            if (card.zone == Zone::Attached &&
                hasCapability(def, Capability::ActivateStormCrestDestroy) &&
                card.host && s.cards.count(card.host) && s.cards.at(card.host).stormPoints >= 2) {
                for (const auto &[targetId, target] : s.cards)
                    if (controllerOf(target) != p && target.zone == Zone::Analysis && !target.faceDown &&
                        catalog().at(target.definition).type == CardType::Formation &&
                        target.spell == SpellState::None && Rules::occupied(s, targetId) == 0)
                        add("发动风暴湮灭能力 → " + catalog().at(target.definition).name,
                            id, targetId, ActivateAbility{id, targetId, "storm_annihilation_destroy"});
            }
        }
        add("结束" + phaseName(s.phase) + "阶段", 0, 0, AdvancePhase{s.phaseGate});
        if (s.phase == Phase::Main)
            add("切换到结束阶段", 0, 0, AdvancePhase{s.phaseGate, true});
        add("投降", 0, 0, Surrender{});
        return;
    }
    } catch (const StopActionVisit &) {}
      catch (const std::invalid_argument &) {}
      catch (const std::out_of_range &) {}
}
} // namespace wizard
