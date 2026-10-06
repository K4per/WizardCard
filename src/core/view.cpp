#include "wizard/core.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wizard {
std::vector<LegalAction> GameEngine::legalActions(PlayerId p) const {
    std::vector<LegalAction> out;
    const auto &s = state_;
    auto add = [&](std::string label, CardId source, CardId target, Command cmd) {
        auto trial = *this;
        if (trial.execute(p, cmd).empty())
            out.push_back({std::move(label), source, target, std::move(cmd)});
    };
    if (s.result != -1)
        return out;
    if (s.decision) {
        const auto &d = *s.decision;
        if (d.player != p)
            return out;
        if (d.kind == DecisionKind::Response) {
            for (auto r : responsesFor(p, d.id)) {
                const auto &source = s.cards.at(r.card);
                const auto &definition = catalog_.at(source.definition);
                auto label = "响应 " + definition.name;
                for (const auto &ability : definition.responses)
                    if (ability.id == r.ability) {
                        const bool hand = source.zone == Zone::Hand;
                        const int paid =
                            source.faceDown                 ? definition.cost
                            : source.zone == Zone::Analysis ? Rules::castCost(s, catalog_, source.id)
                            : hand                          ? ability.handCost
                                                            : ability.preloadedCost +
                                         (definition.type == CardType::Word
                                              ? Rules::castCost(s, catalog_, source.id) - definition.cost
                                              : 0);
                        const int added = ability.burden +
                                          ((source.zone == Zone::Analysis ||
                                            ((hand || source.faceDown) && definition.type == CardType::Word))
                                               ? paid
                                               : 0);
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
        for (auto id : d.options) {
            std::string label;
            if (d.kind == DecisionKind::ClearLoad)
                label = "移除临时荷载来源 #" + std::to_string(id) + " 的1点荷载";
            else if (d.kind == DecisionKind::Concentration) {
                const auto &c = s.cards.at(id);
                label = "维持专注（魔素 " + std::to_string(c.concentrationCost) + " / 荷载 +" +
                        std::to_string(c.concentrationCost) + "）";
            } else if (d.kind == DecisionKind::TriggerOrder) {
                for (const auto &t : s.queue.items)
                    if (t.id == id)
                        label = "结算被动触发 " + catalog_.at(s.cards.at(t.source).definition).name;
            } else
                label = "选择 " + catalog_.at(s.cards.at(id).definition).name + " #" + std::to_string(id);
            add(label,
                (d.kind == DecisionKind::ClearLoad || d.kind == DecisionKind::TriggerOrder ||
                 d.kind == DecisionKind::SearchDeck)
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
GameView GameEngine::viewFor(PlayerId p) const {
    if (p < 0 || p > 1)
        throw std::out_of_range("invalid viewer");
    GameView v;
    v.viewer = p;
    v.active = state_.active;
    v.phase = state_.phase;
    v.result = state_.result;
    v.phaseStep = state_.phaseStep;
    v.phaseGate = state_.phaseGate;
    v.chain = state_.chain;
    for (int i = 0; i < 2; ++i) {
        const auto &pl = state_.players[i];
        int hand = 0;
        for (const auto &[id, c] : state_.cards)
            if (c.owner == i && c.zone == Zone::Hand)
                ++hand;
        v.players[i] = {pl.life,
                        pl.mana,
                        Rules::load(state_, i),
                        Rules::capacity(state_, catalog_, i),
                        hand,
                        static_cast<int>(pl.deck.size()),
                        pl.ownTurn,
                        pl.actionsPlayed,
                        pl.temporaryLife,
                        pl.blockedActionTurn};
        for (const auto &[id, c] : state_.cards)
            if (c.owner == i && c.zone == Zone::Casting && c.spell == SpellState::Active) {
                const auto &d = catalog_.at(c.definition);
                v.players[i].resistances.insert(v.players[i].resistances.end(), d.resistances.begin(),
                                                d.resistances.end());
                v.players[i].immunities.insert(v.players[i].immunities.end(), d.immunities.begin(),
                                               d.immunities.end());
            }
    }
    for (const auto &[id, c] : state_.cards)
        if ((c.zone != Zone::Deck ||
             (state_.decision && state_.decision->player == p &&
              state_.decision->kind == DecisionKind::SearchDeck &&
              std::find(state_.decision->options.begin(), state_.decision->options.end(), id) !=
                  state_.decision->options.end())) &&
            (c.zone != Zone::Hand || c.owner == p)) {
            const auto &def = catalog_.at(c.definition);
            CardView card{c, def};
            card.effectiveCastCost = Rules::castCost(state_, catalog_, id);
            if (c.faceDown && c.owner != p) {
                card.hidden = true;
                card.instance = {};
                card.instance.id = id;
                card.instance.owner = c.owner;
                card.instance.zone = c.zone;
                card.instance.host = c.host;
                card.instance.faceDown = true;
                card.definition = {};
                card.definition.name = "埋伏卡";
                card.effectiveCastCost = 0;
                v.cards.push_back(std::move(card));
                continue;
            }
            if (def.type == CardType::Formation && c.zone == Zone::Analysis) {
                card.effectiveRings = Rules::rings(state_, catalog_, id);
                card.occupiedRings = Rules::occupied(state_, id);
            }
            if (c.spell == SpellState::Analyzing)
                card.turnsToReady =
                    std::max(0, def.analysisTurns - (state_.players[c.owner].ownTurn - c.analysisStarted));
            v.cards.push_back(std::move(card));
        }
    if (state_.decision && state_.decision->player == p)
        v.decision = state_.decision;
    for (const auto &e : state_.events)
        if (e.audience == -1 || e.audience == p)
            v.events.push_back(e);
    v.temporary = state_.temporary;
    v.actions = legalActions(p);
    if (v.decision && v.decision->kind == DecisionKind::TriggerOrder)
        for (const auto &t : state_.queue.items)
            if (std::find(v.decision->options.begin(), v.decision->options.end(), t.id) !=
                    v.decision->options.end() &&
                std::any_of(v.cards.begin(), v.cards.end(),
                            [&](const CardView &c) { return c.instance.id == t.source; }))
                v.triggers.push_back(t);
    return v;
}
std::string GameEngine::stateKey(bool logical) const {
    const auto &s = state_;
    std::ostringstream o;
    o << s.active << ',' << s.first << ',' << static_cast<int>(s.phase) << ','
      << static_cast<int>(s.phaseStep) << ',' << static_cast<int>(s.flow) << ',' << s.result << ',' << s.rng
      << ',' << s.durationApplied << ',' << (logical ? s.globalTurn % 2 : s.globalTurn) << ';';
    if (!logical)
        o << s.nextLoad << ',' << s.nextTrigger << ',' << s.nextBatch << ',' << s.nextDecision << ','
          << s.nextChain << ',' << s.nextLink << ',' << s.phaseGate << ';';
    auto linkId = [&](LinkId id) {
        if (!logical || !id)
            return id;
        if (s.chain)
            for (std::size_t i = 0; i < s.chain->links.size(); ++i)
                if (s.chain->links[i].id == id)
                    return static_cast<LinkId>(i + 1);
        return LinkId{0};
    };
    for (const auto &p : s.players) {
        o << p.life << ',' << p.mana << ',' << p.ownTurn << ',' << p.actionsPlayed << ',' << p.formations
          << ',' << p.sealRemovals << ',' << p.drawFailed << ',' << p.surrendered << ',' << p.temporaryLife
          << ',' << p.blockedActionTurn << ':';
        for (auto id : p.deck)
            o << id << ',';
        o << ';';
    }
    for (const auto &[id, c] : s.cards) {
        o << id << ',' << std::quoted(c.definition) << ',' << c.owner << ',' << static_cast<int>(c.zone)
          << ',' << static_cast<int>(c.spell) << ',' << c.host << ',' << c.sourceFormation << ','
          << c.targetCard << ',' << c.targetPlayer << ',' << c.base << ',' << c.analysisStarted << ','
          << c.remaining << ',' << c.analysisLoad << ',' << c.castLoad << ',' << c.canceledTurn << ','
          << c.settingPaid << ',' << c.concentrationCost << ',' << c.faceDown << ','
          << (logical ? (c.faceDown && c.ambushedTurn == s.globalTurn) : c.ambushedTurn) << ','
          << c.quickAnalysisTurn << ':';
        for (const auto &pay : c.payments)
            o << pay.turn << ',' << pay.paid << ',' << pay.load << ',' << pay.canceled << '/';
        o << ':';
        for (auto target : c.targets)
            o << target << ',';
        o << ';';
    }
    for (const auto &t : s.temporary)
        o << 'L' << (logical ? 0 : t.id) << ',' << t.owner << ',' << t.source << ',' << t.amount << ','
          << t.expiryTurn << ',' << t.independent << ',' << t.sourceBound << ';';
    std::map<std::uint32_t, std::uint32_t> batches;
    std::uint32_t nextBatch = 1;
    auto trigger = [&](const Trigger &t) {
        auto batch = t.batch;
        if (logical) {
            auto [it, inserted] = batches.emplace(batch, nextBatch);
            if (inserted)
                ++nextBatch;
            batch = it->second;
        }
        o << (logical ? 0 : t.id) << ',' << batch << ',' << t.owner << ',' << t.source << ',' << t.targetCard
          << ',' << t.targetPlayer << ',' << static_cast<int>(t.target) << ',' << t.requiresSource << ','
          << linkId(t.targetLink) << ':';
        for (auto id : t.targetCards)
            o << id << ',';
        o << ':';
        for (const auto &e : t.effects)
            o << static_cast<int>(e.kind) << ',' << e.amount << ',' << static_cast<int>(e.damageType) << ','
              << static_cast<int>(e.recipient) << '/';
        o << ';';
    };
    for (const auto &t : s.queue.items) {
        o << 'Q';
        trigger(t);
    }
    for (const auto &l : s.deferredPreparations) {
        o << 'P' << l.sourceLost << ',' << l.paymentIndex << ',' << l.preparationLoad << ',' << l.paid << ','
          << l.addedLoad << ',' << l.discarded << ',' << l.speed << ',' << l.castCost << ';';
        trigger(l.item);
    }
    if (s.effect) {
        o << 'E' << s.effect->cursor << ',' << static_cast<int>(s.effect->after) << ','
          << s.effect->clearRemaining << ',' << linkId(s.effect->link) << ';';
        trigger(s.effect->item);
    }
    if (s.chain) {
        const auto &c = *s.chain;
        o << 'C' << (logical ? 0 : c.id) << ',' << static_cast<int>(c.window) << ','
          << static_cast<int>(c.mode) << ',' << c.initiator << ',' << c.priority << ',' << c.passes << ';';
        for (const auto &l : c.links) {
            o << 'N' << linkId(l.id) << ',' << static_cast<int>(l.kind) << ',' << l.canceled << ','
              << l.sourceLost << ',' << std::quoted(l.ability) << ',' << l.paymentIndex << ','
              << l.preparationLoad << ',' << l.paid << ',' << l.addedLoad << ',' << l.discarded << ','
              << l.speed << ',' << l.castCost << ';';
            trigger(l.item);
        }
        o << 'R';
        for (auto id : c.responded)
            o << id << ',';
    }
    if (s.decision) {
        const auto &d = *s.decision;
        o << 'D' << d.id << ',' << d.player << ',' << static_cast<int>(d.kind) << ',' << d.mayPass << ':';
        for (auto id : d.options)
            o << id << ',';
    }
    return o.str();
}
std::string GameEngine::canonicalState() const {
    return stateKey(false);
}
std::string GameEngine::cycleKey() const {
    return stateKey(true);
}
std::string GameEngine::digest() const {
    return fingerprint(canonicalState());
}
} // namespace wizard
