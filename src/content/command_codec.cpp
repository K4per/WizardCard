#include "wizard/replay.hpp"
#include "json_integer.hpp"
#include <limits>
#include <type_traits>
namespace wizard {
using detail::integer;
Json encodeCommand(const Command &c) {
    Json j;
    j["type"] = c.index();
    std::visit(
        [&](const auto &x) {
            using T = std::decay_t<decltype(x)>;
            if constexpr (!std::is_same_v<T, Advance> && !std::is_same_v<T, AdvancePhase> &&
                          !std::is_same_v<T, PassResponse> && !std::is_same_v<T, Surrender> &&
                          !std::is_same_v<T, Choose>)
                j["card"] = x.card;
            if constexpr (std::is_same_v<T, SetFormation>)
                j["replace"] = x.replace;
            if constexpr (std::is_same_v<T, StartAnalysis> || std::is_same_v<T, SetAmbush>)
                j["formation"] = x.formation;
            if constexpr (std::is_same_v<T, PrepareCast> || std::is_same_v<T, Respond>) {
                j["target"] = x.target;
                j["discard"] = x.discard;
            }
            if constexpr (std::is_same_v<T, PlayAction> || std::is_same_v<T, FlipAmbush> ||
                          std::is_same_v<T, ActivateSpell>)
                j["target"] = x.target;
            if constexpr (std::is_same_v<T, PrepareCast>)
                if (x.decision)
                    j["decision"] = x.decision;
            if constexpr (std::is_same_v<T, PrepareCast> || std::is_same_v<T, PlayAction> ||
                          std::is_same_v<T, Respond> || std::is_same_v<T, FlipAmbush> ||
                          std::is_same_v<T, ActivateSpell>)
                if (!x.targets.empty())
                    j["targets"] = x.targets;
            if constexpr (std::is_same_v<T, PreloadWord>)
                j["release"] = x.release;
            if constexpr (std::is_same_v<T, AttachSeal>)
                j["host"] = x.host;
            if constexpr (std::is_same_v<T, Choose>) {
                j["decision"] = x.decision;
                j["option"] = x.option;
            }
            if constexpr (std::is_same_v<T, Respond>) {
                j["decision"] = x.decision;
                j["ability"] = x.ability;
                j["link"] = x.link;
            }
            if constexpr (std::is_same_v<T, PassResponse>)
                j["decision"] = x.decision;
            if constexpr (std::is_same_v<T, AdvancePhase>)
                j["gate"] = x.gate;
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
    switch (id("type")) {
    case 0:
        return SetFormation{id("card"), id("replace")};
    case 1:
        return RemoveFormation{id("card")};
    case 2:
        return StartAnalysis{id("card"), id("formation")};
    case 3:
        return PrepareCast{id("card"), id("target"), id("discard"), targets,
                           j.contains("decision") ? serial("decision") : 0};
    case 4:
        return PlayAction{id("card"), id("target"), targets};
    case 5:
        return PreloadWord{id("card"), j.value("release", false)};
    case 6:
        return AttachSeal{id("card"), id("host")};
    case 7:
        return RemoveSeal{id("card")};
    case 8:
        return Abandon{id("card")};
    case 9:
        return Advance{};
    case 10:
        return Choose{serial("decision"), id("option")};
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
        return AdvancePhase{serial("gate")};
    case 15:
        return SetAmbush{id("card"), id("formation")};
    case 16:
        return FlipAmbush{id("card"), id("target"), targets};
    case 17:
        return ActivateSpell{id("card"), id("target"), targets};
    default:
        throw std::runtime_error("unknown command type");
    }
}
}
