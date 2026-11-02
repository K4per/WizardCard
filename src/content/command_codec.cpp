#include "wizard/replay.hpp"
#include "json_integer.hpp"
#include <limits>
#include <type_traits>
namespace wizard {
using detail::integer;
Json encodeChoice(const ChoiceValue &choice) {
    // Numeric choices can still be constructed by in-process format-4
    // adapters. Serialize them as their unambiguous card representation;
    // decoding never accepts the legacy numeric wire shape.
    const auto kind = choice.kind == ChoiceKind::Legacy ? ChoiceKind::Card : choice.kind;
    return {{"kind", static_cast<unsigned>(kind)}, {"value", std::to_string(choice.value)}};
}
ChoiceValue decodeChoice(const Json &j) {
    if (!j.is_object() || !j.at("value").is_string()) throw std::runtime_error("invalid typed choice");
    const auto kind = integer(j.at("kind"), static_cast<unsigned>(ChoiceKind::Trigger));
    const auto text = j.at("value").get<std::string>();
    if (text.empty() || text.size() > 20 || text.find_first_not_of("0123456789") != std::string::npos)
        throw std::runtime_error("invalid choice identifier");
    const auto value = std::stoull(text);
    const auto typed = static_cast<ChoiceKind>(kind);
    if (typed == ChoiceKind::Legacy)
        throw std::runtime_error("legacy choices are not valid on the wire");
    if ((typed == ChoiceKind::Pass && value != 0) || (typed == ChoiceKind::Player && value > 1) ||
        ((typed == ChoiceKind::Card || typed == ChoiceKind::Load || typed == ChoiceKind::Mode ||
          typed == ChoiceKind::Payment || typed == ChoiceKind::Trigger) && value > std::numeric_limits<std::uint32_t>::max()))
        throw std::runtime_error("choice identifier out of range");
    return {typed, value};
}
Json encodeCommand(const Command &c) {
    Json j;
    j["type"] = static_cast<unsigned>(commandKind(c));
    std::visit(
        [&](const auto &x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (!std::is_same_v<T, Advance> && !std::is_same_v<T, AdvancePhase> &&
                          !std::is_same_v<T, PassResponse> && !std::is_same_v<T, Surrender> &&
                          !std::is_same_v<T, Choose> && !std::is_same_v<T, Timeout>)
                j["card"] = x.card;
            if constexpr (std::is_same_v<T, SetFormation>) {
                j["replace"] = x.replace;
                if (!x.materials.empty()) j["materials"] = x.materials;
            }
            if constexpr (std::is_same_v<T, StartAnalysis> || std::is_same_v<T, SetAmbush>)
                j["formation"] = x.formation;
            if constexpr (std::is_same_v<T, StartAnalysis>) {
                if (x.target) j["target"] = x.target;
                if (!x.targets.empty()) j["targets"] = x.targets;
            }
            if constexpr (std::is_same_v<T, PrepareCast> || std::is_same_v<T, Respond>) {
                j["target"] = x.target;
                j["discard"] = x.discard;
            }
            if constexpr (std::is_same_v<T, PreloadWord> || std::is_same_v<T, FlipAmbush> || std::is_same_v<T, ActivateSpell>)
                if (x.discard) j["discard"] = x.discard;
            if constexpr (std::is_same_v<T, PlayAction> || std::is_same_v<T, FlipAmbush> ||
                          std::is_same_v<T, ActivateSpell> || std::is_same_v<T, ActivateAbility>)
                j["target"] = x.target;
            if constexpr (std::is_same_v<T, ActivateAbility>)
                j["ability"] = x.ability;
            if constexpr (std::is_same_v<T, PrepareCast>)
                if (x.decision)
                    j["decision"] = x.decision;
            if constexpr (std::is_same_v<T, PreloadWord>)
                if (x.decision) j["decision"] = x.decision;
            if constexpr (std::is_same_v<T, PrepareCast> || std::is_same_v<T, PlayAction> ||
                          std::is_same_v<T, Respond> || std::is_same_v<T, FlipAmbush> ||
                          std::is_same_v<T, ActivateSpell>)
                if (!x.targets.empty())
                    j["targets"] = x.targets;
            if constexpr (std::is_same_v<T, PreloadWord>) {
                j["release"] = x.release;
                if (x.formation) j["formation"] = x.formation;
                if (x.target) j["target"] = x.target;
                if (!x.targets.empty()) j["targets"] = x.targets;
            }
            if constexpr (std::is_same_v<T, AttachSeal>)
                j["host"] = x.host;
            if constexpr (std::is_same_v<T, Choose>) {
                j["decision"] = x.decision;
                j["option"] = encodeChoice(x.option);
                if (x.ringIndex >= 0) j["ringIndex"] = x.ringIndex;
            }
            if constexpr (std::is_same_v<T, FlipAmbush>)
                if (x.decision) j["decision"] = x.decision;
            if constexpr (std::is_same_v<T, Respond>) {
                j["decision"] = x.decision;
                j["ability"] = x.ability;
                j["link"] = x.link;
            }
            if constexpr (std::is_same_v<T, PassResponse>)
                j["decision"] = x.decision;
            if constexpr (std::is_same_v<T, AdvancePhase>) {
                j["gate"] = x.gate;
                if (x.skipCast) j["skipCast"] = true;
            }
        },
        c);
    return j;
}
Command decodeCommand(const Json &j) {
    auto id = [&](const char *key) {
        return static_cast<CardId>(integer(j.at(key), std::numeric_limits<CardId>::max()));
    };
    auto serial = [&](const char *key) {
        return integer(j.at(key), std::numeric_limits<std::uint64_t>::max(), true);
    };
    std::vector<CardId> targets;
    if (j.contains("targets")) {
        if (!j.at("targets").is_array())
            throw std::runtime_error("invalid targets");
        for (const auto &t : j.at("targets"))
            targets.push_back(static_cast<CardId>(integer(t, std::numeric_limits<CardId>::max(), true)));
    }
    std::vector<CardId> materials;
    if (j.contains("materials")) {
        if (!j.at("materials").is_array())
            throw std::runtime_error("invalid materials");
        for (const auto &material : j.at("materials"))
            materials.push_back(static_cast<CardId>(integer(material, std::numeric_limits<CardId>::max(), true)));
    }
    switch (id("type")) {
    case 0:
        return SetFormation{id("card"), id("replace"), materials};
    case 1:
        return RemoveFormation{id("card")};
    case 2:
        return StartAnalysis{id("card"), id("formation"),
            j.contains("target") ? id("target") : 0, targets};
    case 3:
        return PrepareCast{id("card"), id("target"), id("discard"), targets,
                           j.contains("decision") ? serial("decision") : 0};
    case 4:
        return PlayAction{id("card"), id("target"), targets};
    case 5:
        return PreloadWord{id("card"), j.value("release", false),
            j.contains("formation") ? id("formation") : 0,
            j.contains("target") ? id("target") : 0, targets, j.contains("discard") ? id("discard") : 0,
            j.contains("decision") ? serial("decision") : 0};
    case 6:
        return AttachSeal{id("card"), id("host")};
    case 7:
        return RemoveSeal{id("card")};
    case 8:
        return Abandon{id("card")};
    case 9:
        return Advance{};
    case 10:
        return Choose{serial("decision"), decodeChoice(j.at("option")),
            j.contains("ringIndex") ? static_cast<int>(integer(j.at("ringIndex"), 1000)) : -1};
    case 11:
        return Surrender{};
    case 12: {
        auto a = j.at("ability").get<std::string>();
        if (a.empty())
            throw std::runtime_error("empty response ability");
        return Respond{serial("decision"),
                       id("card"),
                       a,
                       id("target"),
                       integer(j.at("link"), std::numeric_limits<LinkId>::max()),
                       id("discard"),
                       targets};
    }
    case 13:
        return PassResponse{serial("decision")};
    case 14:
        return AdvancePhase{serial("gate"), j.value("skipCast", false)};
    case 15:
        return SetAmbush{id("card"), id("formation")};
    case 16:
        return FlipAmbush{id("card"), id("target"), targets,
            j.contains("decision") ? serial("decision") : 0, j.contains("discard") ? id("discard") : 0};
    case 17:
        return ActivateSpell{id("card"), id("target"), targets, j.contains("discard") ? id("discard") : 0};
    case 18:
        return SelectInitialFormation{id("card")};
    case 19:
        return Timeout{};
    case 20:
        return ActivateAbility{id("card"), id("target"), j.at("ability").get<std::string>()};
    default:
        throw std::runtime_error("unknown command type");
    }
}
Command decodeLegacyCommand(const Json &j) {
    if (integer(j.at("type"), std::numeric_limits<std::uint32_t>::max()) != static_cast<unsigned>(CommandKind::Choose))
        return decodeCommand(j);
    return Choose{integer(j.at("decision"), std::numeric_limits<std::uint64_t>::max(), true),
        ChoiceValue{static_cast<std::uint32_t>(integer(j.at("option"), std::numeric_limits<std::uint32_t>::max()))},
        j.contains("ringIndex") ? static_cast<int>(integer(j.at("ringIndex"), 1000)) : -1};
}
}
