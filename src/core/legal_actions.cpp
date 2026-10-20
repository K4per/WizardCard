#include "wizard/core/engine.hpp"
#include "wizard/core/sique.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wizard {
std::vector<LegalAction> GameEngine::legalActions(PlayerId p) const {
    std::vector<LegalAction> out;
    const auto &s = state_;
    auto add = [&](std::string label, CardId source, CardId target, Command cmd) {
        // These operations are intentionally no longer exposed by the battle
        // action surface. Keep command decoding backwards compatible for old
        // replays, but do not offer them as new legal choices.
        if (std::holds_alternative<RemoveFormation>(cmd))
            return;
        auto trial = *this;
        if (trial.execute(p, cmd).empty())
            out.push_back({std::move(label), source, target, std::move(cmd)});
    };
    if (s.result != -1)
        return out;
    if (s.opening) {
        if (!s.initialFormations[p])
            for (const auto &[id, card] : s.cards)
                if (card.owner == p && card.zone == Zone::SideDeck &&
                    catalog_.at(card.definition).type == CardType::Formation && catalog_.at(card.definition).level == 1)
                    add("选择初始阵法 " + catalog_.at(card.definition).name, id, 0, SelectInitialFormation{id});
        add("投降", 0, 0, Surrender{});
        return out;
    }
    if (s.decision) {
        const auto &d = *s.decision;
        if (d.player != p)
            return out;
        if (d.kind == DecisionKind::Response) {
            if (catalog_.usesAlphaV2()) {
                auto setups = responseSetupsV2(p, d.id);
                out.insert(out.end(), setups.begin(), setups.end());
            }
            for (auto r : responsesFor(p, d.id)) {
                const auto &source = s.cards.at(r.card);
                const auto &definition = catalog_.at(source.definition);
                auto label = "响应 " + definition.name;
                for (const auto &ability : definition.responses)
                    if (ability.id == r.ability) {
                        const bool hand = source.zone == Zone::Hand;
                        const int paid = catalog_.usesAlphaV2() ? Rules::castCost(s, catalog_, source.id) :
                            source.faceDown                 ? definition.cost
                            : source.zone == Zone::Analysis ? Rules::castCost(s, catalog_, source.id)
                            : hand                          ? ability.handCost
                                                            : ability.preloadedCost +
                                         (definition.type == CardType::Word
                                              ? Rules::castCost(s, catalog_, source.id) - definition.cost
                                              : 0);
                        const int added = ability.burden + (catalog_.usesAlphaV2() ? paid :
                                          ((source.zone == Zone::Analysis ||
                                            ((hand || source.faceDown) && definition.type == CardType::Word))
                                               ? paid
                                               : 0));
                        label += " · " + (ability.name.empty() ? ability.id : ability.name) + "（魔素" +
                                 std::to_string(paid) + " / 新增荷载" + std::to_string(added) + "）";
                    }
                if (r.link)
                    label += " → 链节 #" + std::to_string(r.link);
                for (auto target : r.targets)
                    label += " → 卡牌 #" + std::to_string(target);
                if (r.discard)
                    label += "；弃置 #" + std::to_string(r.discard);
                out.push_back({label, r.card, r.target, std::move(r)});
            }
            add("放弃响应", 0, 0, PassResponse{d.id});
            add("投降", 0, 0, Surrender{});
            return out;
        }
        if (d.kind == DecisionKind::EffectPrepare) {
            for (auto cmd : preparationsFor(p, d.id)) {
                std::string label = "准备施法 " + catalog_.at(s.cards.at(cmd.card).definition).name;
                for (auto id : cmd.targets)
                    label += " → #" + std::to_string(id);
                if (cmd.discard)
                    label += "；弃置 #" + std::to_string(cmd.discard);
                add(label, cmd.card, cmd.target, std::move(cmd));
            }
            add("放弃额外准备施法", 0, 0, Choose{d.id, 0});
            add("投降", 0, 0, Surrender{});
            return out;
        }
        const bool stormGuidanceMode = d.kind == DecisionKind::EffectChoice && s.effect &&
            s.effect->clearRemaining == -2 &&
            s.effect->cursor < s.effect->item.effects.size() &&
            s.effect->item.effects[s.effect->cursor].kind == EffectKind::StormGuidance;
        for (auto id : d.options) {
            std::string label;
            if (d.kind == DecisionKind::DamageChoice)
                label = "选择" + damageName(static_cast<DamageType>(id - 1)) + "伤害";
            else if (id == 0xffffffe1u)
                label = "抽取效果指定数量的牌";
            else if (stormGuidanceMode)
                label = id == 0xfffffff1u ? "检索一张风暴法术加入手牌"
                    : id == 0xfffffff2u ? "使己方风暴阵法获得2点风暴点数"
                    : id == 0xfffffff3u ? "检索云雾术或雷鸣召唤术加入手牌"
                                        : "选择风暴导向效果";
            else if (d.kind == DecisionKind::ReturnSpell) {
                for (int ring = 0; ring < Rules::rings(s, catalog_, id); ++ring) {
                    bool occupied = false;
                    for (const auto &[other, card] : s.cards)
                        occupied |= other != s.maintenanceSource && card.zone == Zone::Analysis &&
                                    card.host == id && card.ringIndex == ring;
                    if (occupied) continue;
                    const auto ringLabel = "回位至 " + catalog_.at(s.cards.at(id).definition).name + " 环位 " + std::to_string(ring + 1);
                    add(ringLabel, s.maintenanceSource, id, Choose{d.id, id, ring});
                }
                continue;
            }
            else if (d.kind == DecisionKind::ClearAnyLoad)
                label = id & 0x80000000u ? "清除荷载来源 #" + std::to_string(id & 0x7fffffffu) + " 的1点荷载"
                                       : "清除 " + catalog_.at(s.cards.at(id).definition).name + " 的1点绑定荷载";
            else if (d.kind == DecisionKind::ClearLoad)
                label = "移除临时荷载来源 #" + std::to_string(id) + " 的1点荷载";
            else if (d.kind == DecisionKind::StormSearch)
                label = "将 " + catalog_.at(s.cards.at(id).definition).name + " 加入手牌";
            else if (d.kind == DecisionKind::StormHeartChoice)
                label = id == 1 ? "抽1张牌" : "将风暴卡放回牌库";
            else if (d.kind == DecisionKind::StormHeartRecover)
                label = "将 " + catalog_.at(s.cards.at(id).definition).name + " 放回牌库";
            else if (d.kind == DecisionKind::Concentration) {
                const auto &c = s.cards.at(id);
                const auto fee = Rules::concentrationUpkeep(s, catalog_, id);
                label = "维持专注（魔素 " + std::to_string(fee) + " / 荷载 +" +
                        std::to_string(fee) + "）";
            } else if (d.kind == DecisionKind::TriggerOrder) {
                for (const auto &t : s.queue.items)
                    if (t.id == id)
                        label = "结算被动触发 " + catalog_.at(s.cards.at(t.source).definition).name;
            } else
                label = "选择 " + catalog_.at(s.cards.at(id).definition).name + " #" + std::to_string(id);
            add(label,
                (d.kind == DecisionKind::DamageChoice || d.kind == DecisionKind::ClearLoad ||
                 d.kind == DecisionKind::ClearAnyLoad || d.kind == DecisionKind::TriggerOrder ||
                 d.kind == DecisionKind::SearchDeck || d.kind == DecisionKind::StormSearch ||
                 d.kind == DecisionKind::StormHeartChoice || d.kind == DecisionKind::StormHeartRecover ||
                 stormGuidanceMode || id == 0xffffffe1u)
                    ? 0
                    : id,
                0, Choose{d.id, id});
        }
        if (d.mayPass)
            add(d.kind == DecisionKind::Concentration ? "销毁专注法术"
                : d.kind == DecisionKind::CastOrder   ? "本阶段不再释放"
                                                      : "跳过 / 停止",
                0, 0, Choose{d.id, 0});
        add("投降", 0, 0, Surrender{});
        return out;
    }
    if (s.active != p)
        return out;
    auto targets = [&](const CardDefinition &d) {
        std::vector<std::vector<CardId>> result;
        if (d.target == TargetKind::Player) return std::vector<std::vector<CardId>>{{playerTarget(0)}, {playerTarget(1)}};
        if (d.target == TargetKind::None || d.target == TargetKind::Self ||
            d.target == TargetKind::Opponent) {
            result.push_back({});
            return result;
        }
        std::vector<CardId> ids, selected;
        for (const auto &[id, c] : s.cards)
            if (Rules::targetValid(s, d, p, id))
                ids.push_back(id);
        auto visit = [&](auto &&self, std::size_t pos) -> void {
            if (selected.size() == static_cast<std::size_t>(d.targetCount)) {
                result.push_back(selected);
                return;
            }
            for (std::size_t i = pos; i < ids.size(); ++i) {
                selected.push_back(ids[i]);
                self(self, i + 1);
                selected.pop_back();
            }
        };
        if (d.targetCount > 0 && d.targetCount <= 5)
            visit(visit, 0);
        return result;
    };
    if (catalog_.usesAlphaV2()) {
        for (const auto &[id, card] : s.cards) {
            if (card.owner != p) continue;
            const auto &def = catalog_.at(card.definition);
            const auto name = def.name + " #" + std::to_string(id);
            const auto activationSets = sique::detachCost(def) ? sique::detachOptions(s, catalog_, p, def) : targets(def);
            const auto activationLabel = [&](const std::vector<CardId> &costs) {
                std::string label;
                if (sique::detachCost(def)) for (auto cost : costs)
                    label += "；拆卸 " + catalog_.at(s.cards.at(cost).definition).name + " #" + std::to_string(cost);
                return label;
            };
            if (card.zone == Zone::Analysis && card.stormPoints >= 2 &&
                card.stormAbilityTurn != s.players[p].ownTurn &&
                std::find(def.traits.begin(), def.traits.end(), "activate_storm_search") != def.traits.end()) {
                const bool canSearch = std::any_of(s.players[p].deck.begin(), s.players[p].deck.end(), [&](CardId deckId) {
                    const auto &candidate = s.cards.at(deckId);
                    const auto &candidateDefinition = catalog_.at(candidate.definition);
                    const bool spell = candidateDefinition.type == CardType::Analytic ||
                        candidateDefinition.type == CardType::Word || candidateDefinition.type == CardType::Talisman;
                    return candidate.zone == Zone::Deck && spell &&
                        std::find(candidateDefinition.tags.begin(), candidateDefinition.tags.end(), "风暴") !=
                            candidateDefinition.tags.end();
                });
                if (canSearch)
                    add("发动风暴纹章能力 " + name, id, 0, ActivateAbility{id, 0, "storm_search"});
            }
            if (card.zone == Zone::Analysis && card.stormPoints >= 2 &&
                card.stormAbilityTurn != s.players[p].ownTurn &&
                std::find(def.traits.begin(), def.traits.end(), "activate_storm_heart_choice") != def.traits.end())
                add("发动风暴之心能力 " + name, id, 0, ActivateAbility{id, 0, "storm_heart_choice"});
            std::vector<CardId> spellCosts{0};
            if (std::find(def.traits.begin(), def.traits.end(), "discard_spell_search") != def.traits.end()) {
                spellCosts.clear();
                for (const auto &[material, candidate] : s.cards)
                    if (material != id && candidate.owner == p && candidate.zone == Zone::Hand &&
                        (catalog_.at(candidate.definition).type == CardType::Analytic ||
                         catalog_.at(candidate.definition).type == CardType::Word ||
                         catalog_.at(candidate.definition).type == CardType::Talisman)) spellCosts.push_back(material);
            }
            if (card.zone == Zone::SideDeck) {
                int count = 0;
                for (const auto &part : def.recipe)
                    if (part.kind != RecipeCostKind::Mana) count += part.amount;
                std::vector<CardId> pool, selected;
                for (const auto &[materialId, material] : s.cards)
                    if (material.owner == p && (material.zone == Zone::Analysis || material.zone == Zone::Casting ||
                        material.zone == Zone::Hand) && !material.faceDown)
                        pool.push_back(materialId);
                auto choose = [&](auto &&self, std::size_t pos) -> void {
                    if (static_cast<int>(selected.size()) == count) {
                        auto add_formation = [&](CardId replace) {
                            auto trial = *this;
                            if (!trial.executeV2(p, SetFormation{id, replace, selected}).empty()) return;
                            if (!replace)
                                add("设置阵法 " + name, id, 0, SetFormation{id, 0, selected});
                            else
                                add("替换 " + catalog_.at(s.cards.at(replace).definition).name + " 为 " + name,
                                    id, replace, SetFormation{id, replace, selected});
                        };
                        add_formation(0);
                        for (const auto &[host, formation] : s.cards)
                            if (formation.owner == p && formation.zone == Zone::Analysis &&
                                catalog_.at(formation.definition).type == CardType::Formation)
                                add_formation(host);
                        return;
                    }
                    for (std::size_t i = pos; i < pool.size(); ++i) {
                        selected.push_back(pool[i]); self(self, i + 1); selected.pop_back();
                    }
                };
                if (count <= 5 && s.phase == Phase::Main) choose(choose, 0);
            }
            if (card.zone == Zone::Hand) {
                for (const auto &[host, formation] : s.cards) {
                    if (formation.owner != p) continue;
                    if (formation.zone == Zone::Analysis && catalog_.at(formation.definition).type == CardType::Formation) {
                        add("盖伏 " + name, id, host, SetAmbush{id, host});
                        add("设置解析法术 " + name, id, host, StartAnalysis{id, host});
                        for (const auto &ids : activationSets) {
                            const auto target = sique::detachCost(def) || ids.empty() ? 0 : ids.front();
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
                const auto prepareTargets = def.id == "unravel"
                    ? std::vector<std::vector<CardId>>{{}}
                    : targets(def);
                for (const auto &ids : prepareTargets) {
                    const auto target = ids.empty() ? 0 : ids.front();
                    if (!card.faceDown) {
                        if (card.zone == Zone::Analysis)
                            add("准备施法 " + name, id, target, PrepareCast{id, target, 0, ids});
                    }
                }
                for (const auto &ids : activationSets) {
                    const auto target = sique::detachCost(def) || ids.empty() ? 0 : ids.front();
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
            add("拆除符文 " + name, id, 0, RemoveSeal{id});
            add("拆除阵法 " + name, id, 0, RemoveFormation{id});
            if (card.zone == Zone::Attached &&
                std::find(def.traits.begin(), def.traits.end(), "activate_storm_crest_destroy") != def.traits.end() &&
                card.host && s.cards.count(card.host) && s.cards.at(card.host).stormPoints >= 2) {
                for (const auto &[targetId, target] : s.cards)
                    if (target.owner != p && target.zone == Zone::Analysis && !target.faceDown &&
                        catalog_.at(target.definition).type == CardType::Formation &&
                        target.spell == SpellState::None && Rules::occupied(s, targetId) == 0)
                        add("发动风暴湮灭能力 → " + catalog_.at(target.definition).name,
                            id, targetId, ActivateAbility{id, targetId, "storm_annihilation_destroy"});
            }
        }
        add("结束" + phaseName(s.phase) + "阶段", 0, 0, AdvancePhase{s.phaseGate});
        if (s.phase == Phase::Main)
            add("切换到结束阶段", 0, 0, AdvancePhase{s.phaseGate, true});
        add("投降", 0, 0, Surrender{});
        return out;
    }
    if (catalog_.advancedRules) {
        for (const auto &[id, c] : s.cards)
            if (c.owner == p) {
                const auto &def = catalog_.at(c.definition);
                if (!c.faceDown &&
                    (c.zone == Zone::Words || (c.zone == Zone::Analysis && c.spell == SpellState::Ready))) {
                    for (const auto &ids : targets(def)) {
                        const auto target = ids.empty() ? 0 : ids.front();
                        add("释放法术 " + def.name, id, target, ActivateSpell{id, target, ids});
                    }
                }
                if (c.faceDown) {
                    const auto choices =
                        def.type == CardType::Analytic ? std::vector<std::vector<CardId>>{{}} : targets(def);
                    for (const auto &ids : choices) {
                        const auto target = ids.empty() ? 0 : ids.front();
                        add("反转埋伏卡 " + def.name, id, target, FlipAmbush{id, target, ids});
                    }
                }
            }
    }
    if (s.phase != Phase::Main) {
        add("结束" + phaseName(s.phase) + "阶段", 0, 0, AdvancePhase{s.phaseGate});
        add("投降", 0, 0, Surrender{});
        return out;
    }
    for (const auto &[id, c] : s.cards)
        if (c.owner == p) {
            const auto &def = catalog_.at(c.definition);
            const auto name = def.name + " #" + std::to_string(id);
            if (c.zone == Zone::Hand) {
                if (catalog_.advancedRules) {
                    if (def.type == CardType::Action || def.type == CardType::Word)
                        add("埋伏 " + name, id, 0, SetAmbush{id, 0});
                    if (def.type == CardType::Analytic)
                        for (const auto &[host, h] : s.cards)
                            if (h.owner == p && h.zone == Zone::Analysis &&
                                catalog_.at(h.definition).type == CardType::Formation)
                                add("埋伏 " + name + " → " + catalog_.at(h.definition).name, id, host,
                                    SetAmbush{id, host});
                }
                if (def.type == CardType::Formation) {
                    add("设置阵法 " + name, id, 0, SetFormation{id, 0});
                    for (const auto &[other, h] : s.cards)
                        if (h.owner == p && h.zone == Zone::Analysis &&
                            catalog_.at(h.definition).type == CardType::Formation && !h.base)
                            add("替换阵法 " + catalog_.at(h.definition).name + " #" + std::to_string(other),
                                id, other, SetFormation{id, other});
                } else if (def.type == CardType::Analytic || def.type == CardType::Seal) {
                    for (const auto &[host, h] : s.cards)
                        if (h.owner == p && (h.zone == Zone::Analysis ||
                                             (def.type == CardType::Seal && h.zone == Zone::Words))) {
                            const auto hostName =
                                catalog_.at(h.definition).name + " #" + std::to_string(host);
                            if (def.type == CardType::Analytic)
                                add("开始解析 " + name + " → " + hostName, id, host, StartAnalysis{id, host});
                            else
                                add("附着符文 " + name + " → " + hostName, id, host, AttachSeal{id, host});
                        }
                } else if (def.type == CardType::Word) {
                    add("设置言灵法术 " + name, id, 0, PreloadWord{id});
                    if (def.immediate && catalog_.advancedRules)
                        add("设置并释放 " + name, id, 0, PreloadWord{id, true});
                } else
                    for (const auto &ids : targets(def)) {
                        auto target = ids.empty() ? 0 : ids.front();
                        add("使用行动卡 " + name + " → #" + std::to_string(target), id, target,
                            PlayAction{id, target, ids});
                    }
            } else if (c.zone == Zone::Analysis && def.type == CardType::Formation)
                add("拆除阵法 " + name, id, 0, RemoveFormation{id});
            else if (c.zone == Zone::Attached)
                add("拆除符文 " + name, id, 0, RemoveSeal{id});
            if (c.zone == Zone::Analysis && c.spell == SpellState::Ready) {
                for (const auto &ids : targets(def)) {
                    auto target = ids.empty() ? 0 : ids.front();
                    std::vector<CardId> discards{0};
                    if (def.extraDiscard) {
                        discards.clear();
                        for (const auto &[discard, h] : s.cards)
                            if (h.owner == p && h.zone == Zone::Hand)
                                discards.push_back(discard);
                    }
                    for (auto discard : discards) {
                        auto label = "准备施法 " + name;
                        for (auto tid : ids)
                            label += " → #" + std::to_string(tid);
                        if (discard)
                            label += "；弃置 #" + std::to_string(discard);
                        add(label, id, target, PrepareCast{id, target, discard, ids});
                    }
                }
            }
            if (c.zone == Zone::Analysis || c.zone == Zone::Casting || c.zone == Zone::Words)
                add("放弃法术 " + name, id, 0, Abandon{id});
        }
    add("结束" + phaseName(s.phase) + "阶段", 0, 0, AdvancePhase{s.phaseGate});
    add("投降", 0, 0, Surrender{});
    return out;
}
} // namespace wizard
