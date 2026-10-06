#include "wizard/content.hpp"
#include <limits>
#include <type_traits>
namespace wizard {
namespace {
std::uint64_t integer(const Json &x, std::uint64_t max, bool nonzero = false) {
    if (!x.is_number_unsigned() && (!x.is_number_integer() || x.get<std::int64_t>() < 0))
        throw std::runtime_error("invalid unsigned integer");
    auto n = x.get<std::uint64_t>();
    if (n > max || (nonzero && !n))
        throw std::runtime_error("integer outside protocol range");
    return n;
}
MatchConfig sharedConfig(const Content &c, std::uint32_t seed) {
    MatchConfig config;
    config.seed = seed;
    for (auto &p : config.players)
        p.cards = c.deck;
    return config;
}
} // namespace
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
MatchSession::MatchSession(Content c, std::uint32_t seed) : MatchSession(c, sharedConfig(c, seed)) {}
MatchSession::MatchSession(Content c, MatchConfig config)
    : content_(std::move(c)), config_(std::move(config)), engine_(content_.catalog, config_) {}
CommandResult MatchSession::submit(PlayerId p, const Command &c) {
    auto result = engine_.submit(p, c);
    if (result.accepted)
        commands_.push_back({p, c, engine_.digest()});
    return result;
}
Json MatchSession::recording() const {
    Json players = Json::array();
    for (const auto &p : config_.players)
        players.push_back({{"baseFormation", p.baseFormation}, {"cards", p.cards}});
    Json j = {{"format", 2},
              {"programVersion", programVersion},
              {"rulesVersion", content_.catalog.rulesVersion},
              {"cardSetVersion", content_.catalog.cardSetVersion},
              {"contentHash", content_.catalog.contentHash},
              {"seed", config_.seed},
              {"players", players},
              {"commands", Json::array()},
              {"finalDigest", engine_.digest()}};
    for (const auto &c : commands_)
        j["commands"].push_back(
            {{"actor", c.actor}, {"command", encodeCommand(c.command)}, {"digest", c.digest}});
    const auto &s = engine_.state();
    j["statistics"] = {{"turns", {s.players[0].ownTurn, s.players[1].ownTurn}},
                       {"result", s.result},
                       {"firstPlayer", s.first},
                       {"commandCount", commands_.size()}};
    j["events"] = Json::array();
    for (const auto &e : s.events)
        j["events"].push_back({{"kind", e.kind},
                               {"text", e.text},
                               {"audience", e.audience},
                               {"card", e.card},
                               {"amount", e.amount},
                               {"effectType", e.effectType ? Json(static_cast<int>(*e.effectType)) : Json()},
                               {"actualAmount", e.actualAmount},
                               {"affectedPlayer", e.affectedPlayer},
                               {"spellReleased", e.spellReleased}});
    return j;
}
void MatchSession::save(const std::filesystem::path &path) const {
    writeJson(path, recording());
}
GameEngine replay(const Content &c, const Json &j) {
    if (j.at("format") != 2 || j.at("programVersion") != programVersion ||
        j.at("rulesVersion") != c.catalog.rulesVersion ||
        j.at("cardSetVersion") != c.catalog.cardSetVersion || j.at("contentHash") != c.catalog.contentHash)
        throw std::runtime_error("incompatible replay version or content hash");
    MatchConfig config;
    config.seed =
        static_cast<std::uint32_t>(integer(j.at("seed"), std::numeric_limits<std::uint32_t>::max()));
    if (!j.at("players").is_array() || j.at("players").size() != 2)
        throw std::runtime_error("replay requires two player decks");
    for (int p = 0; p < 2; ++p) {
        config.players[p].baseFormation = j.at("players").at(p).at("baseFormation").get<std::string>();
        config.players[p].cards = j.at("players").at(p).at("cards").get<std::vector<std::string>>();
    }
    GameEngine engine(c.catalog, config);
    if (!j.at("commands").is_array())
        throw std::runtime_error("invalid replay commands");
    int n = 0;
    for (const auto &row : j.at("commands")) {
        auto actor = static_cast<int>(integer(row.at("actor"), 1));
        auto result = engine.submit(actor, decodeCommand(row.at("command")));
        if (!result.accepted || engine.digest() != row.at("digest").get<std::string>())
            throw std::runtime_error("replay diverged at command " + std::to_string(n));
        ++n;
    }
    if (engine.digest() != j.at("finalDigest").get<std::string>())
        throw std::runtime_error("final digest mismatch");
    return engine;
}
} // namespace wizard
