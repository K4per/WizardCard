#include "wizard/replay.hpp"
#include "wizard/application.hpp"
#include <iostream>

using namespace wizard;
#include "ai.inc"
int main(int argc,char** argv) {
    try {
        if(argc<3) { std::cout<<"wizard_cli validate|play|smoke|replay|script|ai-match|tutorial-smoke ASSETS [FILE] [SEED] [DIFFICULTY 0-2]\n"; return 1; }
        const std::string mode=argv[1]; auto content=loadContent(argv[2]);
        if(mode=="ai-match" || mode=="tutorial-smoke") return runAi(content,argv[2],argc,argv);
        if(mode=="validate") { std::cout<<content.catalog.cards.size()<<" definitions; "<<content.deck.size()<<" cards; hash "<<content.catalog.contentHash<<'\n'; return 0; }
        if(mode=="replay") { if(argc<4) return 1; auto engine=replay(content,readJson(argv[3])); std::cout<<engine.digest()<<'\n'; return 0; }
        std::uint32_t seed=argc>4?static_cast<std::uint32_t>(std::stoul(argv[4])):42;
        MatchSession session(content,seed);
        if(mode=="script") {
            if(argc<4) return 1;
            for(const auto& row:readJson(argv[3])) { auto result=session.submit(row.at("actor").get<int>(),decodeCommand(row.at("command"))); if(!result.accepted) throw std::runtime_error(result.error); }
        } else if(mode=="smoke" || mode=="play") {
            for(int step=0;session.engine().state().result==-1;++step) {
                if(step>4000) throw std::runtime_error("scenario did not terminate");
                const auto& s=session.engine().state(); int p=s.decision?s.decision->player:s.active; auto view=session.engine().viewFor(p);
                std::size_t selected=0;
                if(mode=="play") {
                    std::cout<<"\nPlayer "<<p+1<<" / "<<phaseName(view.phase)<<" mana="<<view.players[p].mana<<" load="<<view.players[p].load<<'/'<<view.players[p].capacity<<'\n';
                    for(std::size_t i=0;i<view.actions.size();++i) std::cout<<i<<": "<<view.actions[i].label<<'\n';
                    if(!(std::cin>>selected)) break;
                } else {
                    // Deterministic test driver, not a player-facing AI or balance simulator.
                    int best=-10000;
                    for(std::size_t i=0;i<view.actions.size();++i) {
                        const auto& a=view.actions[i]; int score=-1000;
                        if(std::holds_alternative<Choose>(a.command) || std::holds_alternative<Respond>(a.command)) score=100;
                        else if(std::holds_alternative<PrepareCast>(a.command)) score=s.decision && s.decision->kind==DecisionKind::EffectPrepare?110:80;
                        else if(std::holds_alternative<SetFormation>(a.command) && !std::get<SetFormation>(a.command).replace) score=75;
                        else if(std::holds_alternative<StartAnalysis>(a.command)) score=65;
                        else if(std::holds_alternative<PlayAction>(a.command)) score=55;
                        else if(std::holds_alternative<AttachSeal>(a.command)) score=45;
                        else if(std::holds_alternative<PreloadWord>(a.command)) score=35;
                        else if(std::holds_alternative<Advance>(a.command) || std::holds_alternative<AdvancePhase>(a.command) || std::holds_alternative<PassResponse>(a.command)) score=0;
                        if(score>best) { best=score; selected=i; }
                    }
                }
                if(selected>=view.actions.size()) continue;
                auto result=session.submit(p,view.actions[selected].command); if(!result.accepted) throw std::runtime_error(result.error);
            }
        } else throw std::runtime_error("unknown mode");
        const auto path=mode=="smoke" && argc>3?std::filesystem::path(argv[3]):std::filesystem::path("logs/match.json");
        session.save(path);
        auto verified=replay(content,session.recording());
        std::cout<<"result="<<verified.state().result<<" digest="<<verified.digest()<<" recording="<<path.string()<<'\n';
        return 0;
    } catch(const std::exception& e) { std::cerr<<e.what()<<'\n'; return 2; }
}
