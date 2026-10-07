#pragma once
#include "wizard/core.hpp"
#include <filesystem>
#include <nlohmann/json.hpp>

namespace wizard {
using Json=nlohmann::json;
struct Content { CardCatalog catalog; std::vector<std::string> deck; };
Content loadContent(const std::filesystem::path& directory);
CardCatalog parseCatalog(const Json&,const std::string& source="cards.json");
std::vector<std::string> parseDeck(const Json&,const CardCatalog&,const std::string& source="deck.json");
Json encodeCommand(const Command&);
Command decodeCommand(const Json&);
struct RecordedCommand { PlayerId actor{}; Command command; std::string digest; };
class MatchSession {
public:
    MatchSession(Content content,std::uint32_t seed);
    MatchSession(Content content,MatchConfig config);
    CommandResult submit(PlayerId,const Command&);
    const GameEngine& engine() const { return engine_; }
    Json recording() const;
    void save(const std::filesystem::path&) const;
private:
    Content content_; MatchConfig config_; GameEngine engine_; std::vector<RecordedCommand> commands_;
};
GameEngine replay(const Content&,const Json&);
Json readJson(const std::filesystem::path&);
void writeJson(const std::filesystem::path&,const Json&);
void writeAtomicJson(const std::filesystem::path&,const Json&);
}
