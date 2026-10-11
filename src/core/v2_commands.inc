#include "engine_internal.hpp"
#include <algorithm>
#include <functional>
#include <set>
namespace wizard {
using namespace detail;
namespace {
bool spellType(CardType type) {
    return type == CardType::Analytic || type == CardType::Word || type == CardType::Talisman;
}
bool materialMatches(const CardDefinition &definition, const RecipeCost &need, const CardInstance &card,
                     const CardCatalog &catalog, PlayerId owner) {
    if (card.owner != owner || card.faceDown ||
        (card.zone != Zone::Analysis && card.zone != Zone::Casting && card.zone != Zone::Hand)) return false;
    if (need.kind == RecipeCostKind::SacrificeFormation &&
        (definition.type != CardType::Formation || card.zone != Zone::Analysis)) return false;
    if (need.kind == RecipeCostKind::SacrificeSpell &&
        (!spellType(definition.type) || card.zone == Zone::Hand)) return false;
    if (need.kind == RecipeCostKind::DiscardHand && card.zone != Zone::Hand) return false;
    if (!need.definition.empty() && card.definition != need.definition) return false;
    if (!need.school.empty() && definition.school != need.school) return false;
    if (need.readyOnly && card.spell != SpellState::Ready && card.spell != SpellState::Active) return false;
    if (need.fireDamage) {
        bool fire = false;
        for (const auto &effect : definition.effects)
            fire |= effect.kind == EffectKind::Damage && effect.damageType == DamageType::Fire;
        if (!fire) return false;
    }
    (void)catalog;
    return true;
}
bool recipeMatches(const GameState &state, const CardCatalog &catalog, PlayerId player,
                   const CardDefinition &formation, const std::vector<CardId> &materials) {
    std::vector<const RecipeCost *> slots;
    for (const auto &need : formation.recipe)
        if (need.kind != RecipeCostKind::Mana)
            for (int i = 0; i < need.amount; ++i) slots.push_back(&need);
    if (materials.size() != slots.size()) return false;
    std::set<CardId> distinct(materials.begin(), materials.end());
    if (distinct.size() != materials.size()) return false;
    std::vector<bool> taken(materials.size());
    std::function<bool(std::size_t)> search = [&](std::size_t index) {
        if (index == slots.size()) return true;
        for (std::size_t i = 0; i < materials.size(); ++i) {
            const auto it = state.cards.find(materials[i]);
            if (taken[i] || it == state.cards.end()) continue;
            const auto &card = it->second;
            const auto &definition = catalog.at(card.definition);
            if (!materialMatches(definition, *slots[index], card, catalog, player)) continue;
            if (slots[index]->distinctSchools) {
                bool repeated = false;
                for (std::size_t j = 0; j < materials.size(); ++j)
                    if (taken[j] && catalog.at(state.cards.at(materials[j]).definition).school == definition.school)
                        repeated = true;
                if (repeated) continue;
            }
            taken[i] = true;
            if (search(index + 1)) return true;
            taken[i] = false;
        }
        return false;
    };
    return search(0);
}
}
void GameEngine::actionTimingV2(ResponseWindow window) {
    if (state_.chain || state_.effect || state_.decision || state_.result != -1) return;
    ChainLink root;
    root.kind = LinkKind::Phase;
    root.item.owner = state_.active;
    startChain(window, std::move(root), 1 - state_.active);
}
void GameEngine::analysisCompletedV2(CardId spell) {
    auto &s = state_;
    const auto owner = s.cards.at(spell).owner;
    const auto batch = s.nextBatch++;
    for (const auto &[id, formation] : s.cards) {
        if (formation.owner != owner || formation.zone != Zone::Analysis) continue;
        const auto &def = catalog_.at(formation.definition);
        if (def.type == CardType::Formation &&
            std::find(def.traits.begin(), def.traits.end(), "clear_load_on_analysis") != def.traits.end()) {
            auto trigger = snapshot(formation, def); trigger.id = s.nextTrigger++; trigger.batch = batch;
            trigger.effects = {{EffectKind::ClearAnyLoad, 1}}; s.queue.append(std::move(trigger));
        }
    }
}
void GameEngine::quickAnalysisV2(CardId spell, CardId host) {
    auto &s = state_;
    auto &card = s.cards.at(spell);
    auto &formation = s.cards.at(host);
    const auto &def = catalog_.at(card.definition);
    if (!catalog_.at(formation.definition).quickRankOne || def.rank > 1 ||
        card.spell != SpellState::Analyzing || formation.quickAnalysisTurn == s.players[card.owner].ownTurn)
        return;
    card.analysisProgress = def.analysisTurns;
    card.spell = SpellState::Ready;
    formation.quickAnalysisTurn = s.players[card.owner].ownTurn;
    s.events.push_back({"analysis_ready", "阵法立即完成低位阶解析（每回合一次）", -1, spell});
    analysisCompletedV2(spell);
}
void GameEngine::castingTraitsV2(CardId spell) {
    auto &s = state_;
    if (s.result != -1) return;
    const auto &card = s.cards.at(spell);
    const auto host = s.cards.find(card.sourceFormation);
    if (host == s.cards.end() || host->second.zone != Zone::Analysis) return;
    const auto &traits = catalog_.at(host->second.definition).traits;
    if (std::find(traits.begin(), traits.end(), "host_cast_life_for_temporary") != traits.end()) {
        --s.players[card.owner].life;
        s.events.push_back({"life_loss", "逆十字：失去1点生命", -1, spell, 1});
        StateMaintenance::check(s, catalog_);
        if (s.result == -1)
            s.players[card.owner].temporaryLife = std::max(2, s.players[card.owner].temporaryLife);
    }
}
std::string GameEngine::responseSetupV2(PlayerId actor, const Command &command) {
    auto &s = state_;
    if (!s.decision || !s.chain || s.chain->mode != ChainMode::Building || s.chain->priority != actor)
        return "priority";
    const auto *flip = std::get_if<FlipAmbush>(&command);
    const auto *prepare = std::get_if<PrepareCast>(&command);
    const auto id = flip ? flip->card : prepare->card;
    const auto token = flip ? flip->decision : prepare->decision;
    if (token != s.decision->id) return "stale_decision";
    const auto found = s.cards.find(id);
    if (found == s.cards.end() || found->second.owner != actor) return "invalid_card";
    auto &card = found->second; const auto &def = catalog_.at(card.definition);
    if (def.type != CardType::Analytic || card.zone != Zone::Analysis || def.speed < 2 ||
        s.chain->links.back().speed == 4 || def.speed < s.chain->links.back().speed ||
        (def.speed == 2 && s.phase != Phase::Main && s.phase != Phase::Cast) ||
        (actor != s.active && (!card.wasAmbushed || card.ambushedTurn >= s.globalTurn))) return "response_source";
    if (flip) {
        if (!card.faceDown || card.ambushedTurn >= s.globalTurn ||
            !Rules::canHost(s, catalog_, id, card.host)) return "ambush_wait";
        const int fee = Rules::analysisCost(s, catalog_, id, card.host);
        if (s.players[actor].mana < fee) return "cost";
        s.players[actor].mana -= fee; card.analysisLoad += fee;
        ResourceRules::increasedLoad(s, catalog_, actor, fee);
        if (s.result != -1) return {};
        card.faceDown = false; card.settingPaid = fee; card.analysisProgress = 0;
        card.analysisStarted = s.players[actor].ownTurn;
        card.spell = def.analysisTurns ? SpellState::Analyzing : SpellState::Ready;
        if (card.spell == SpellState::Ready) analysisCompletedV2(id);
        else quickAnalysisV2(id, card.host);
        s.chain->window = ResponseWindow::Action;
    } else {
        if (card.faceDown || card.spell != SpellState::Ready || card.canceledTurn == s.players[actor].ownTurn ||
            prepare->discard || !validTargets(s, def.target, def.targetCount, actor, prepare->target, prepare->targets))
            return "not_ready";
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "casting_capacity";
        for (const auto &link : s.chain->links)
            if (link.item.source == id) return "response_used";
        card.targetCard = prepare->target; card.targets = prepare->targets;
        ChainLink link; link.id = s.nextLink++; link.kind = LinkKind::Preparation; link.speed = def.speed;
        link.item = snapshot(card, def); link.item.effects.clear(); link.item.requiresSource = true;
        s.chain->links.push_back(std::move(link)); s.chain->hadActualLinks = true;
        s.chain->window = ResponseWindow::Prepare;
    }
    s.events.push_back({flip ? "flip" : "prepare_declared", flip ? "反转解析法术" : "宣告准备施法", -1, id});
    s.chain->priority = 1 - s.active; s.chain->passes = 0;
    s.decision.reset();
    return {};
}
std::vector<LegalAction> GameEngine::responseSetupsV2(PlayerId actor, DecisionId token) const {
    std::vector<LegalAction> out;
    auto add = [&](std::string name, CardId id, Command command) {
        auto trial = *this;
        trial.state_.decision = PendingDecision{token, actor, DecisionKind::Response, {}, true};
        if (trial.responseSetupV2(actor, command).empty()) out.push_back({std::move(name), id, 0, std::move(command)});
    };
    for (const auto &[id, card] : state_.cards) {
        if (card.owner != actor || card.zone != Zone::Analysis) continue;
        const auto &definition = catalog_.at(card.definition);
        if (card.faceDown) add("反转 " + definition.name, id, FlipAmbush{id, 0, {}, token});
        else if (card.spell == SpellState::Ready)
            for (const auto &targets : sets(state_, definition.target, definition.targetCount, actor))
                add("准备施法 " + definition.name, id,
                    PrepareCast{id, targets.empty() ? 0 : targets.front(), 0, targets, token});
    }
    return out;
}
std::string GameEngine::executeV2(PlayerId actor, const Command &command) {
    auto &s = state_;
    if (std::holds_alternative<Choose>(command) || std::holds_alternative<Respond>(command) ||
        std::holds_alternative<PassResponse>(command)) return "stale_decision";
    if (s.decision || s.chain || s.effect || !s.queue.items.empty()) return "pending: 先处理待决事项";
    if (actor != s.active || s.phaseStep != PhaseStep::Body) return "priority: 当前没有行动权";
    if (const auto *advance = std::get_if<AdvancePhase>(&command)) {
        if (!s.phaseGate || advance->gate != s.phaseGate) return "phase_gate: 无效阶段令牌";
        s.phaseGate = 0;
        s.phaseStep = PhaseStep::EndWindow;
        if (s.phase == Phase::Cast) s.returnRequested = true;
        return {};
    }
    if (std::holds_alternative<Advance>(command)) {
        if (s.phase != Phase::Main || !s.phaseGate) return "phase_gate";
        s.phaseGate = 0; s.phaseStep = PhaseStep::EndWindow; return {};
    }
    auto cardId = std::visit([](const auto &item) -> CardId {
        using T = std::decay_t<decltype(item)>;
        if constexpr (std::is_same_v<T, Choose> || std::is_same_v<T, Advance> ||
                      std::is_same_v<T, AdvancePhase> || std::is_same_v<T, PassResponse> || std::is_same_v<T, Surrender>) return 0;
        else return item.card;
    }, command);
    auto it = s.cards.find(cardId);
    if (it == s.cards.end() || it->second.owner != actor) return "invalid_card";
    auto &c = it->second;
    const auto &def = catalog_.at(c.definition);
    auto &pl = s.players[actor];
    auto ownedFormation = [&](CardId id) {
        auto host = s.cards.find(id);
        return host != s.cards.end() && host->second.owner == actor && host->second.zone == Zone::Analysis &&
            catalog_.at(host->second.definition).type == CardType::Formation;
    };
    auto pay = [&](int mana, int load) {
        if (pl.mana < mana) return false;
        pl.mana -= mana;
        if (load > 0) { c.analysisLoad += load; ResourceRules::increasedLoad(s, catalog_, actor, load); }
        s.events.push_back({"pay", "支付魔素", -1, cardId, mana});
        return true;
    };
    auto ring = [&](CardId host, bool faceDown = false) {
        return Rules::canHost(s, catalog_, cardId, host, faceDown) &&
            Rules::freeRing(s, catalog_, host) >= 0;
    };
    auto place = [&](CardId host) {
        c.zone = Zone::Analysis; c.host = host; c.sourceFormation = host;
        c.ringIndex = Rules::freeRing(s, catalog_, host);
        c.sourceRingIndex = c.ringIndex;
        c.wasAmbushed = false;
        c.fieldOrder = s.nextFieldOrder++;
    };
    auto phaseSpeed = [&] {
        return (def.speed >= 3 || (def.speed >= 2 && (s.phase == Phase::Main || s.phase == Phase::Cast)) ||
                (def.speed == 1 && s.phase == Phase::Cast));
    };
    auto setupSpeed = [&] {
        return def.speed >= 3 || (def.speed >= 2 && (s.phase == Phase::Main || s.phase == Phase::Cast)) ||
               (def.speed == 1 && s.phase == Phase::Main);
    };
    if (const auto *cmd = std::get_if<SetFormation>(&command)) {
        if (s.phase != Phase::Main || c.zone != Zone::SideDeck || def.type != CardType::Formation || pl.formations)
            return "formation_limit";
        if (cmd->replace && (!ownedFormation(cmd->replace) || cmd->replace == cardId)) return "invalid_replacement";
        if (!recipeMatches(s, catalog_, actor, def, cmd->materials)) return "recipe_materials";
        if (std::find(def.traits.begin(), def.traits.end(), "unique") != def.traits.end())
            for (const auto &[id, other] : s.cards)
                if (other.owner == actor && other.zone == Zone::Analysis && other.definition == c.definition &&
                    id != cmd->replace && std::find(cmd->materials.begin(), cmd->materials.end(), id) == cmd->materials.end())
                    return "unique_formation";
        if (std::find(cmd->materials.begin(), cmd->materials.end(), cmd->replace) != cmd->materials.end())
            return "recipe_materials";
        int fee = def.cost;
        for (const auto &part : def.recipe)
            if (part.kind == RecipeCostKind::Mana) fee += part.amount;
        if (pl.mana < fee) return "cost: 魔素不足";
        int body = usedBody(s, catalog_, actor);
        if (cmd->replace) body -= catalog_.at(s.cards.at(cmd->replace).definition).body;
        for (auto id : cmd->materials)
            if (catalog_.at(s.cards.at(id).definition).type == CardType::Formation)
                body -= catalog_.at(s.cards.at(id).definition).body;
        if (body + def.body > 5) return "formation_capacity";
        pl.mana -= fee;
        for (auto id : cmd->materials) StateMaintenance::leave(s, id, &catalog_);
        if (cmd->replace) StateMaintenance::leave(s, cmd->replace, &catalog_);
        c.zone = Zone::Analysis; c.fieldOrder = s.nextFieldOrder++;
        c.settingPaid = fee; ++pl.formations;
        s.events.push_back({"formation_set", "设置阵法", -1, cardId});
        if (std::find(def.traits.begin(), def.traits.end(), "return_formation_from_ash") != def.traits.end()) {
            auto trigger = snapshot(c, def); trigger.id = s.nextTrigger++; trigger.batch = s.nextBatch++;
            trigger.effects = {{EffectKind::RecoverFormation, 1}}; s.queue.append(std::move(trigger));
        }
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<RemoveFormation>(&command)) {
        if (s.phase != Phase::Main || c.zone != Zone::Analysis || def.type != CardType::Formation || pl.formations)
            return "invalid_removal";
        StateMaintenance::leave(s, cardId, &catalog_); ++pl.formations; actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<StartAnalysis>(&command)) {
        if (!setupSpeed() || c.zone != Zone::Hand || def.type != CardType::Analytic || !ring(cmd->formation))
            return "ring_or_rank";
        int fee = Rules::analysisCost(s, catalog_, cardId, cmd->formation);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        place(cmd->formation); c.settingPaid = fee; c.analysisProgress = 0;
        c.spell = def.analysisTurns <= 0 ? SpellState::Ready : SpellState::Analyzing;
        c.analysisStarted = pl.ownTurn; c.targetCard = cmd->target; c.targets = cmd->targets;
        if (c.spell == SpellState::Ready) analysisCompletedV2(cardId);
        else quickAnalysisV2(cardId, cmd->formation);
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<SetAmbush>(&command)) {
        if (s.phase != Phase::Main || c.zone != Zone::Hand || !spellType(def.type) || !ring(cmd->formation, true))
            return "invalid_ambush";
        place(cmd->formation); c.faceDown = true; c.wasAmbushed = true;
        c.ambushedTurn = s.globalTurn; c.spell = SpellState::None;
        s.events.push_back({"ambush", "盖伏法术", -1, cardId});
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<FlipAmbush>(&command)) {
        if (cmd->decision || !c.faceDown || c.zone != Zone::Analysis || c.ambushedTurn >= s.globalTurn ||
            (actor != s.active) || !setupSpeed()) return "ambush_wait";
        if (!Rules::canHost(s, catalog_, cardId, c.host)) return "host_restriction";
        if (def.type == CardType::Analytic && def.speed == 1 && s.phase != Phase::Main)
            return "spell_speed";
        if (def.type == CardType::Analytic) {
            const int fee = Rules::analysisCost(s, catalog_, cardId, c.host);
            if (!pay(fee, fee)) return "cost: 魔素不足";
            if (s.result != -1) return {};
            c.faceDown = false; c.settingPaid = fee;
            c.analysisProgress = 0; c.analysisStarted = pl.ownTurn;
            c.spell = def.analysisTurns <= 0 ? SpellState::Ready : SpellState::Analyzing;
            if (c.spell == SpellState::Ready) analysisCompletedV2(cardId);
            else quickAnalysisV2(cardId, c.host);
            actionTimingV2(); return {};
        }
        if (static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "casting_capacity";
        if (def.responseOnly || !validTargets(s, def.target, def.targetCount, actor, cmd->target, cmd->targets))
            return "target_or_response_window";
        const int fee = Rules::castCost(s, catalog_, cardId);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        c.settingPaid = fee; c.faceDown = false; c.zone = Zone::Casting; c.host = 0; c.ringIndex = -1;
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = cmd->targets;
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<PreloadWord>(&command)) {
        if (!setupSpeed() || c.zone != Zone::Hand || def.type != CardType::Word || def.responseOnly ||
            !ring(cmd->formation) ||
            static_cast<int>(inZone(s, actor, Zone::Casting).size()) >= Rules::castingCapacity(s, catalog_, actor))
            return "word_setup";
        if (!validTargets(s, def.target, def.targetCount, actor, cmd->target, cmd->targets)) return "target";
        const int fee = Rules::castCost(s, catalog_, cardId);
        if (!pay(fee, fee)) return "cost: 魔素不足";
        if (s.result != -1) return {};
        place(cmd->formation); c.settingPaid = fee;
        c.spell = SpellState::Ready;
        c.zone = Zone::Casting; c.host = 0; c.ringIndex = -1;
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = cmd->targets;
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        analysisCompletedV2(cardId); castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<PrepareCast>(&command)) {
        if (!validTargets(s, def.target, def.targetCount, actor, cmd->target, cmd->targets)) return "target";
        return prepare(actor, *cmd, false);
    }
    if (const auto *cmd = std::get_if<ActivateSpell>(&command)) {
        if (c.zone != Zone::Casting || c.spell != SpellState::Ready || !phaseSpeed() ||
            !validTargets(s, def.target, def.targetCount, actor, cmd->target, cmd->targets))
            return "not_ready";
        const int fee = Rules::castCost(s, catalog_, cardId);
        if (pl.mana < fee) return "cost: 魔素不足";
        pl.mana -= fee; c.castLoad += fee; ResourceRules::increasedLoad(s, catalog_, actor, fee);
        if (s.result != -1) return {};
        c.payments.push_back({pl.ownTurn, fee, fee, false});
        c.spell = SpellState::Pending; c.targetCard = cmd->target; c.targets = cmd->targets;
        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
        castingTraitsV2(cardId);
        beginEffect(cardId, AfterEffect::Spell); return {};
    }
    if (const auto *cmd = std::get_if<AttachSeal>(&command)) {
        auto host = s.cards.find(cmd->host);
        if (s.phase != Phase::Main || c.zone != Zone::Hand || def.type != CardType::Seal ||
            host == s.cards.end() || host->second.owner != actor || host->second.faceDown ||
            (host->second.zone != Zone::Analysis && host->second.zone != Zone::Casting) ||
            seals(s, cmd->host)) return "seal_host";
        if (!pay(def.cost, 0)) return "cost: 魔素不足";
        c.zone = Zone::Attached; c.host = cmd->host;
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<RemoveSeal>(&command)) {
        if (s.phase != Phase::Main || c.zone != Zone::Attached || pl.sealRemovals) return "seal_limit";
        StateMaintenance::leave(s, cmd->card, &catalog_); ++pl.sealRemovals;
        actionTimingV2(); return {};
    }
    if (const auto *cmd = std::get_if<Abandon>(&command)) {
        if (!spellType(def.type) || (c.zone != Zone::Analysis && c.zone != Zone::Casting) ||
            (c.zone == Zone::Casting ? s.phase != Phase::Cast : s.phase != Phase::Main)) return "invalid_abandon";
        StateMaintenance::leave(s, cmd->card, &catalog_);
        actionTimingV2(); return {};
    }
    return "unsupported_v2_command";
}
}
