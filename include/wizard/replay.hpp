#pragma once
#include "wizard/content.hpp"
#include "wizard/core/engine.hpp"
#include "wizard/command_codec.hpp"
#include "wizard/session.hpp"
namespace wizard {
struct RecordedCommand { PlayerId actor{}; Command command; std::string digest; };
class MatchSession : public Session {
public:
    MatchSession(Content content,std::uint32_t seed);
    MatchSession(Content content,MatchConfig config);
    CommandResult submit(PlayerId,const Command&) override;
    GameView viewFor(PlayerId viewer) const override { return engine_.viewFor(viewer); }
    std::uint64_t revision() const override { return commands_.size(); }
    bool started() const override { return true; }
    bool blocked() const override { return false; }
    void tick() override {}
    const GameEngine& engine() const { return engine_; }
    Json recording() const;
    void save(const std::filesystem::path&) const;
private:
    Content content_; MatchConfig config_; GameEngine engine_; std::vector<RecordedCommand> commands_;
};
GameEngine replay(const Content&,const Json&);
}
