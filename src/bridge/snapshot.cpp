#include "wizard/bridge.hpp"

namespace wizard::bridge {
Json commandDto(const Command &command) {
    auto encoded = encodeCommand(command);
    for (auto &[key, value] : encoded.items()) {
        if (key == "type" || key == "ability" || key == "release" || key == "skipCast") continue;
        if (key == "ringIndex") continue; // A small ordinal, not a card/decision ID.
        if (value.is_array()) {
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
Json cardDefinitionDto(const CardDefinition &d) {
    Json recipe = Json::array();
    for (const auto &cost : d.recipe)
        recipe.push_back({{"kind", static_cast<int>(cost.kind)}, {"amount", cost.amount},
            {"definition", cost.definition}, {"school", cost.school}, {"readyOnly", cost.readyOnly},
            {"distinctSchools", cost.distinctSchools}, {"fireDamage", cost.fireDamage}});
    return {{"id", d.id}, {"number", d.number}, {"name", d.name}, {"text", d.text}, {"flavor", d.flavor},
            {"type", static_cast<int>(d.type)}, {"rarity", d.rarity}, {"school", d.school},
            {"tags", d.tags}, {"cost", d.cost}, {"castCost", d.castCost}, {"rank", d.rank},
            {"analysisTurns", d.analysisTurns}, {"duration", d.duration}, {"speed", d.speed},
            {"body", d.body}, {"capacity", d.capacity}, {"income", d.income}, {"rings", d.rings},
            {"maxRank", d.maxRank}, {"baseEligible", d.baseEligible}, {"concentration", d.concentration},
            {"level", d.level}, {"perpetual", d.perpetual}, {"illustrationId", d.illustrationId},
            {"recipe", recipe}, {"settingConditions", d.settingConditions}};
}
Json viewDto(const GameView &v) {
    Json out = {{"viewer", v.viewer}, {"activePlayer", v.active}, {"phase", static_cast<int>(v.phase)},
                {"phaseName", phaseName(v.phase)}, {"phaseStep", static_cast<int>(v.phaseStep)},
                {"phaseGate", std::to_string(v.phaseGate)}, {"result", v.result},
                {"globalTurn", std::to_string(v.globalTurn)}, {"opening", v.opening},
                {"initialFormationConfirmed", v.initialFormationConfirmed},
                {"initialFormation", std::to_string(v.initialFormation)},
                {"clock", {{"enabled", v.clock.enabled}, {"running", v.clock.running},
                    {"runningPlayer", v.clock.runningPlayer}, {"runningPlayers", v.clock.runningPlayers},
                    {"remainingMs", v.clock.remainingMs}, {"sequence", std::to_string(v.clock.sequence)}}},
                {"players", Json::array()}, {"cards", Json::array()},
                {"events", Json::array()}, {"loads", Json::array()}, {"triggers", Json::array()},
                {"decision", nullptr}, {"chain", nullptr}};
    for (const auto &p : v.players)
        out["players"].push_back({{"life", p.life}, {"mana", p.mana}, {"load", p.load},
            {"capacity", p.capacity}, {"handCount", p.handCount}, {"deckCount", p.deckCount},
            {"sideDeckCount", p.sideDeckCount}, {"exhaustion", p.exhaustion},
            {"ownTurn", p.ownTurn}, {"temporaryLife", p.temporaryLife},
            {"resistances", p.resistances}, {"immunities", p.immunities}});
    for (const auto &c : v.cards) {
        const auto &i = c.instance;
        Json card = {{"id", std::to_string(i.id)}, {"owner", i.owner}, {"zone", static_cast<int>(i.zone)},
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
            {"kind", static_cast<int>(v.decision->kind)}, {"options", ids(v.decision->options)},
            {"mayPass", v.decision->mayPass}};
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
    }
    std::size_t eventIndex{};
    for (const auto &e : v.events) {
        out["events"].push_back({{"id", std::to_string(eventIndex++)}, {"kind", e.kind}, {"text", e.text},
            {"card", std::to_string(e.card)}, {"amount", e.amount}, {"actualAmount", e.actualAmount},
            {"effectType", e.effectType ? Json(static_cast<int>(*e.effectType)) : Json()},
            {"affectedPlayer", e.affectedPlayer}, {"spellReleased", e.spellReleased}});
    }
    for (const auto &load : v.temporary)
        out["loads"].push_back({{"id", std::to_string(load.id)}, {"owner", load.owner},
            {"source", std::to_string(load.source)}, {"amount", load.amount}, {"expiryTurn", load.expiryTurn},
            {"independent", load.independent}, {"sourceBound", load.sourceBound}});
    for (const auto &t : v.triggers)
        out["triggers"].push_back({{"id", std::to_string(t.id)}, {"source", std::to_string(t.source)},
                                   {"owner", t.owner}});
    return out;
}
} // namespace wizard::bridge
