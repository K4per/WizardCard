#include "wizard/content.hpp"
#include <type_traits>
#include <limits>

namespace wizard {
Json encodeCommand(const Command& c) {
    Json j; j["type"]=c.index();
    std::visit([&](const auto& x){using T=std::decay_t<decltype(x)>;
        if constexpr(!std::is_same_v<T,Advance> && !std::is_same_v<T,Surrender> && !std::is_same_v<T,Choose>) j["card"]=x.card;
        if constexpr(std::is_same_v<T,SetFormation>) j["replace"]=x.replace;
        if constexpr(std::is_same_v<T,StartAnalysis>) j["formation"]=x.formation;
        if constexpr(std::is_same_v<T,PrepareCast>) {j["target"]=x.target;j["discard"]=x.discard;}
        if constexpr(std::is_same_v<T,PlayAction>) j["target"]=x.target;
        if constexpr(std::is_same_v<T,AttachSeal>) j["host"]=x.host;
        if constexpr(std::is_same_v<T,Choose>) {j["decision"]=x.decision;j["option"]=x.option;}
    },c); return j;
}
Command decodeCommand(const Json& j) {
    auto id=[&](const char* key)->CardId { const auto& x=j.at(key); if((!x.is_number_unsigned() && (!x.is_number_integer() || x.get<std::int64_t>()<0)) || x.get<std::uint64_t>()>std::numeric_limits<CardId>::max()) throw std::runtime_error(std::string("invalid command field: ")+key); return x.get<CardId>(); };
    switch(id("type")) {
    case 0:return SetFormation{id("card"),id("replace")};
    case 1:return RemoveFormation{id("card")};
    case 2:return StartAnalysis{id("card"),id("formation")};
    case 3:return PrepareCast{id("card"),id("target"),id("discard")};
    case 4:return PlayAction{id("card"),id("target")};
    case 5:return PreloadWord{id("card")};
    case 6:return AttachSeal{id("card"),id("host")};
    case 7:return RemoveSeal{id("card")};
    case 8:return Abandon{id("card")};
    case 9:return Advance{};
    case 10: {
        const auto& d=j.at("decision");
        if(!d.is_number_unsigned() && (!d.is_number_integer() || d.get<std::int64_t>()<1))throw std::runtime_error("invalid decision id");
        return Choose{d.get<DecisionId>(),id("option")}; }
    case 11:return Surrender{};
    default:throw std::runtime_error("unknown command type");
    }
}
MatchSession::MatchSession(Content c,std::uint32_t seed):content_(std::move(c)),seed_(seed),engine_(content_.catalog,{content_.deck,content_.deck},seed) {}
CommandResult MatchSession::submit(PlayerId p,const Command& c) {
    auto result=engine_.submit(p,c); if(result.accepted) commands_.push_back({p,c,engine_.digest()}); return result;
}
Json MatchSession::recording() const {
    Json j={{"format",1},{"programVersion",programVersion},{"rulesVersion",content_.catalog.rulesVersion},{"cardSetVersion",content_.catalog.cardSetVersion},{"contentHash",content_.catalog.contentHash},{"seed",seed_},{"decks",Json::array({content_.deck,content_.deck})},{"commands",Json::array()},{"finalDigest",engine_.digest()}};
    for(const auto& c:commands_) j["commands"].push_back({{"actor",c.actor},{"command",encodeCommand(c.command)},{"digest",c.digest}});
    const auto& s=engine_.state();
    j["statistics"]={{"turns",{s.players[0].ownTurn,s.players[1].ownTurn}},{"result",s.result},{"firstPlayer",s.first},{"commandCount",commands_.size()}};
    j["events"]=Json::array(); for(const auto& e:s.events) j["events"].push_back({{"kind",e.kind},{"text",e.text},{"audience",e.audience},{"card",e.card},{"amount",e.amount}});
    return j;
}
void MatchSession::save(const std::filesystem::path& path) const { writeJson(path,recording()); }
GameEngine replay(const Content& c,const Json& j) {
    if(j.at("format")!=1 || j.at("programVersion")!=programVersion || j.at("rulesVersion")!=c.catalog.rulesVersion || j.at("cardSetVersion")!=c.catalog.cardSetVersion || j.at("contentHash")!=c.catalog.contentHash) throw std::runtime_error("incompatible replay version or content hash");
    std::array<std::vector<std::string>,2> decks{j.at("decks").at(0).get<std::vector<std::string>>(),j.at("decks").at(1).get<std::vector<std::string>>()};
    GameEngine engine(c.catalog,decks,j.at("seed").get<std::uint32_t>()); int n=0;
    for(const auto& row:j.at("commands")) {
        auto result=engine.submit(row.at("actor").get<int>(),decodeCommand(row.at("command")));
        if(!result.accepted || engine.digest()!=row.at("digest").get<std::string>()) throw std::runtime_error("replay diverged at command "+std::to_string(n)); ++n;
    }
    if(engine.digest()!=j.at("finalDigest").get<std::string>()) throw std::runtime_error("final digest mismatch"); return engine;
}
}
