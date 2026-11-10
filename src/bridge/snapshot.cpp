#include "wizard/bridge.hpp"

namespace wizard::bridge {
Json commandDto(const Command &command) {
    auto encoded = encodeCommand(command);
    for (auto &[key, value] : encoded.items()) {
        if (key == "type" || key == "ability" || key == "release" || key == "skipCast") continue;
        if (key == "ringIndex") continue; // A small ordinal, not a card/decision ID.
        if (key == "option" && value.is_object()) { if (!value.at("value").is_string()) value["value"] = std::to_string(value.at("value").get<std::uint64_t>()); }
        else if (value.is_array()) {
            for (auto &id : value) id = std::to_string(id.get<std::uint64_t>());
        } else value = std::to_string(value.get<std::uint64_t>());
    }
    return encoded;
}
namespace {
template<class T> Json ids(const std::vector<T> &values) {
    Json out = Json::array();
    for (const auto id : values) out.push_back(std::to_string(id));
    return out;
}
} // namespace
Json choicesDto(const std::vector<ChoiceValue> &values) {
    Json out=Json::array();
    for(const auto &choice:values) out.push_back({{"kind",static_cast<int>(choice.kind)},{"value",std::to_string(choice.value)}});
    return out;
}
Json cardDefinitionDto(const CardDefinition &d) {
    Json recipe = Json::array();
    for (const auto &cost : d.recipe)
        recipe.push_back({{"kind", static_cast<int>(cost.kind)}, {"amount", cost.amount},
            {"definition", cost.definition}, {"school", cost.school}, {"readyOnly", cost.readyOnly}, {"faceDownOnly", cost.faceDownOnly},
            {"distinctSchools", cost.distinctSchools}, {"fireDamage", cost.fireDamage}});
    return {{"id", d.id}, {"number", d.number}, {"name", d.name}, {"text", d.text}, {"flavor", d.flavor},
            {"type", static_cast<int>(d.type)}, {"rarity", d.rarity}, {"school", d.school},
            {"tags", d.tags}, {"cost", d.cost}, {"castCost", d.castCost}, {"rank", d.rank},
            {"analysisTurns", d.analysisTurns}, {"duration", d.duration}, {"speed", d.speed},
            {"body", d.body}, {"capacity", d.capacity}, {"income", d.income}, {"rings", d.rings},
            {"maxRank", d.maxRank}, {"baseEligible", d.baseEligible}, {"concentration", d.concentration},
            {"level", d.level}, {"perpetual", d.perpetual}, {"illustrationId", d.illustrationId},
            {"dynamicCost", d.dynamicCost},
            {"recipe", recipe}, {"settingConditions", d.settingConditions}};
}
Json eventDto(const GameEvent &e) {
    return {{"kind", e.kind}, {"text", e.text}, {"card", std::to_string(e.card)},
        {"amount", e.amount}, {"actualAmount", e.actualAmount},
        {"effectType", e.effectType ? Json(static_cast<int>(*e.effectType)) : Json()},
        {"affectedPlayer", e.affectedPlayer}, {"spellReleased", e.spellReleased}};
}
Json clockDto(const MatchClockView &clock) {
    return {{"enabled", clock.enabled}, {"running", clock.running}, {"runningPlayer", clock.runningPlayer},
        {"runningPlayers", clock.runningPlayers}, {"remainingMs", clock.remainingMs}, {"sequence", std::to_string(clock.sequence)}};
}
Json viewDto(const GameView &v) {
    Json out = {{"schema", schemaVersion}, {"viewer", v.viewer}, {"activePlayer", v.active}, {"phase", static_cast<int>(v.phase)},
                {"phaseName", phaseName(v.phase)}, {"phaseStep", static_cast<int>(v.phaseStep)},
                {"phaseGate", std::to_string(v.phaseGate)}, {"result", v.result},
                {"globalTurn", std::to_string(v.globalTurn)}, {"opening", v.opening},
                {"openingStage", openingStageName(v.openingStage)}, {"openingRound", v.openingRound},
                {"openingGesture", v.openingGesture}, {"gestureConfirmed", v.gestureConfirmed},
                {"revealedGestures", v.revealedGestures},
                {"initialFormationConfirmed", v.initialFormationConfirmed},
                {"initialFormation", std::to_string(v.initialFormation)},
                {"clock", {{"enabled", v.clock.enabled}, {"running", v.clock.running},
                    {"runningPlayer", v.clock.runningPlayer}, {"runningPlayers", v.clock.runningPlayers},
                    {"remainingMs", v.clock.remainingMs}, {"sequence", std::to_string(v.clock.sequence)}}},
                {"players", Json::array()}, {"cards", Json::array()},
                {"loads", Json::array()}, {"triggers", Json::array()},
                {"temporaryLives", Json::array()},
                {"modifiers", Json::array()}, {"extraTurns", v.extraTurns},
                {"decision", nullptr}, {"chain", nullptr}};
    for (const auto &m : v.modifiers)
        out["modifiers"].push_back({{"kind", static_cast<int>(m.kind)}, {"source", std::to_string(m.source)},
            {"target", std::to_string(m.target)}, {"owner", m.owner}, {"player", m.player}, {"amount", m.amount},
            {"damageType", static_cast<int>(m.damageType)}, {"expiry", static_cast<int>(m.expiry)},
            {"expiresTurn", std::to_string(m.expiresTurn)}, {"expiresOwnTurn", m.expiresOwnTurn},
            {"returnHost", std::to_string(m.returnHost)}, {"returnRing", m.returnRing}, {"returnController", m.returnController}});
    for (const auto &p : v.players)
        out["players"].push_back({{"life", p.life}, {"mana", p.mana}, {"load", p.load},
            {"capacity", p.capacity}, {"castingCapacity", p.castingCapacity}, {"castingCount", p.castingCount},
            {"handCount", p.handCount}, {"deckCount", p.deckCount},
            {"sideDeckCount", p.sideDeckCount}, {"exhaustion", p.exhaustion},
            {"ownTurn", p.ownTurn}, {"temporaryLife", p.temporaryLife},
            {"lifeLimit", p.lifeLimit}, {"sevenMode", p.sevenMode},
            {"resistances", p.resistances}, {"immunities", p.immunities},
            {"infiniteLife", p.infiniteLife}, {"infiniteMana", p.infiniteMana}, {"infiniteLoad", p.infiniteLoad}});
    for (const auto &entry : v.temporaryLives)
        out["temporaryLives"].push_back({{"id", std::to_string(entry.id)}, {"owner", entry.owner},
            {"source", std::to_string(entry.source)}, {"amount", entry.amount},
            {"expiryOwnTurn", entry.expiryOwnTurn}, {"acquiredTurn", std::to_string(entry.acquiredTurn)},
            {"acquiredPhase", static_cast<int>(entry.acquiredPhase)}});
    for (const auto &c : v.cards) {
        const auto &i = c.instance;
        Json card = {{"id", std::to_string(i.id)}, {"owner", controllerOf(i)}, {"originalOwner", i.owner}, {"controller", controllerOf(i)}, {"zone", static_cast<int>(i.zone)},
                     {"zoneName", zoneName(i.zone)}, {"host", std::to_string(i.host)},
                     {"hidden", c.hidden}, {"faceDown", i.faceDown}};
        // Location metadata is public even when the card definition is hidden.
        // The client needs the real ring/order to render anonymous covers without
        // moving them into the first free slot.
        card["ringIndex"] = i.ringIndex;
        card["fieldOrder"] = std::to_string(i.fieldOrder);
        if (!c.hidden) {
            card["definition"] = cardDefinitionDto(c.definition);
            card["spellState"] = static_cast<int>(i.spell);
            card["base"] = i.base;
            card["turnsToReady"] = c.turnsToReady;
            card["remaining"] = i.remaining;
            card["statuses"] = c.statuses;
            card["analysisProgress"] = i.analysisProgress;
            card["sourceRingIndex"] = i.sourceRingIndex;
            card["effectiveRings"] = c.effectiveRings;
            card["occupiedRings"] = c.occupiedRings;
            card["effectiveCastCost"] = c.effectiveCastCost;
            card["effectiveSettingCost"] = c.effectiveSettingCost;
            card["analysisLoad"] = i.analysisLoad;
            card["castLoad"] = i.castLoad;
            card["stormPoints"] = i.stormPoints;
        }
        out["cards"].push_back(std::move(card));
    }
    if (v.decision)
        out["decision"] = {{"id", std::to_string(v.decision->id)}, {"player", v.decision->player},
            {"kind", static_cast<int>(v.decision->kind)}, {"options", choicesDto(v.decision->options)},
            {"mayPass", v.decision->mayPass}, {"source", std::to_string(v.decisionSource)}};
    if (v.chain) {
        const auto &chain = *v.chain;
        Json links = Json::array();
        for (const auto &link : chain.links)
            links.push_back({{"id", std::to_string(link.id)}, {"kind", static_cast<int>(link.kind)},
                {"kindName", linkName(link.kind)}, {"owner", link.item.owner},
                {"source", std::to_string(link.item.source)}, {"target", std::to_string(link.item.targetCard)},
                {"targetLink", std::to_string(link.item.targetLink)}, {"targets", ids(link.item.targetCards)},
                {"speed", link.speed}, {"paid", link.paid}, {"addedLoad", link.addedLoad},
                {"canceled", link.canceled}, {"sourceLost", link.sourceLost}});
        out["chain"] = {{"id", std::to_string(chain.id)}, {"window", static_cast<int>(chain.window)},
            {"windowName", windowName(chain.window)}, {"mode", static_cast<int>(chain.mode)},
            {"priority", chain.priority}, {"passes", chain.passes}, {"links", links}};
        if (!chain.links.empty()) {
            const auto &last = chain.links.back();
            std::string name = "盖伏卡";
            for (const auto &card : v.cards)
                if (card.instance.id == last.item.source && !card.hidden) name = card.definition.name;
            const std::string actor = last.item.owner == v.viewer ? "己方" : "对方";
            std::string description;
            if (last.ability == "ambush") description = actor + "设置了1张盖伏卡";
            else if (last.kind == LinkKind::Phase)
                description = actor + (chain.window == ResponseWindow::PhaseEnd ? "即将结束" :
                    chain.window == ResponseWindow::PhaseStart ? "进入" : "完成操作：") + phaseName(v.phase) + "阶段";
            else if (last.kind == LinkKind::Setup) description = actor + "设置【" + name + "】";
            else if (last.kind == LinkKind::Preparation) description = actor + "准备施法【" + name + "】";
            else description = actor + "发动【" + name + "】";
            out["responseContext"] = {{"text", description}, {"actor", last.item.owner},
                {"waitingPlayer", chain.priority}, {"source", std::to_string(last.item.source)},
                {"chainCount", chain.links.size()}, {"window", static_cast<int>(chain.window)}};
        }
    }
    for (const auto &load : v.temporary)
        out["loads"].push_back({{"id", std::to_string(load.id)}, {"owner", load.owner},
            {"source", std::to_string(load.source)}, {"amount", load.amount}, {"expiryTurn", load.expiryTurn},
            {"independent", load.independent}, {"sourceBound", load.sourceBound}});
    for (const auto &t : v.triggers)
        out["triggers"].push_back({{"id", std::to_string(t.id)}, {"source", std::to_string(t.source)},
                                   {"owner", t.owner}});
    out["technicalPaused"] = v.technicalPaused;
    out["actionSummary"] = {{"actionable", v.actionSummary.actionable},
        {"hasNonPhaseAction", v.actionSummary.hasNonPhaseAction},
        {"safeAdvance", v.actionSummary.safeAdvance ? commandDto(*v.actionSummary.safeAdvance) : Json()}};
    return out;
}
} // namespace wizard::bridge
