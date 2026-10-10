#include "wizard/replay.hpp"
#include "json_integer.hpp"
#include <limits>
#include <type_traits>
namespace wizard {
using detail::integer;
namespace {
MatchConfig sharedConfig(const Content &c, std::uint32_t seed) {
    MatchConfig config;
    config.seed = seed;
    for (auto &p : config.players)
        p.cards = c.deck;
    return config;
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
    if (j.at("format") != 2 || (j.at("programVersion") != programVersion &&
        j.at("programVersion") != "1.1.0-alpha" && j.at("programVersion") != "1.0.0-alpha") ||
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
