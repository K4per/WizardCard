#include "wizard/content.hpp"
#include "wizard/runtime.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>

using namespace wizard;
using namespace wizard::runtime;
namespace {
std::string spellName(SpellState s) {return std::array<const char*,5>{"","解析中","解析完成","待施放","持续生效"}.at(static_cast<std::size_t>(s));}
std::string kindName(CardType t) {return std::array<const char*,5>{"行动","解析法术","言灵","阵法","符文"}.at(static_cast<std::size_t>(t));}
class Client {
public:
    Client(std::filesystem::path assets,std::uint32_t seed):assets_(std::move(assets)),content_(loadContent(assets_)),session_(content_,seed),resources_(assets_) { newLog(); refresh(true); }
    void run(const std::string& screenshot,bool showcase,const Json& smoke) {
        sf::RenderWindow window(sf::VideoMode({1600,1000}),"WizardCard - 巫师牌");window.setFramerateLimit(60);
        sf::View logical(sf::FloatRect({0.f,0.f},{1600.f,1000.f}));window.setView(logical);sf::Clock clock;
        if(showcase) scene_=Scene::Match;
        int frames=0;std::size_t smokeStep=0;
        while(window.isOpen()) {
            while(const auto event=window.pollEvent()) {
                if(event->is<sf::Event::Closed>()) {save();window.close();}
                if(const auto* resized=event->getIf<sf::Event::Resized>()) {
                    if(resized->size.x && resized->size.y) {
                        const float ratio=static_cast<float>(resized->size.x)/static_cast<float>(resized->size.y);const float base=1.6f;
                        logical.setViewport(ratio>base?sf::FloatRect({(1-base/ratio)/2,0},{base/ratio,1}):sf::FloatRect({0,(1-ratio/base)/2},{1,ratio/base}));window.setView(logical);
                    }
                }
                if(const auto* mouse=event->getIf<sf::Event::MouseButtonPressed>()) if(mouse->button==sf::Mouse::Button::Left) click(window.mapPixelToCoords(mouse->position));
                if(const auto* wheel=event->getIf<sf::Event::MouseWheelScrolled>()) {
                    auto point=window.mapPixelToCoords(wheel->position);int delta=wheel->delta<0?1:-1;
                    if(point.x>1140) actionScroll_=std::max(0,actionScroll_+delta);
                    else if(point.y>760) handScroll_=std::max(0,handScroll_+delta);
                    else for(std::size_t i=0;i<zoneRects_.size();++i) if(hit(point,zoneRects_[i])) zoneScroll_[i]=std::max(0,zoneScroll_[i]+delta);
                }
                if(const auto* key=event->getIf<sf::Event::KeyPressed>()) {
                    if(key->code==sf::Keyboard::Key::Escape) {pending_.reset();selected_=0;actionScroll_=0;}
                    if(key->code==sf::Keyboard::Key::F5) save();
                }
            }
            if(!window.isOpen())break;
            animation_.update(clock.restart().asSeconds());draw(window);window.display();
            if(!smoke.is_null()) {
                if(scene_==Scene::Handoff) click({700,850});
                else if(smokeStep<smoke.at("commands").size()) {
                    const auto& row=smoke.at("commands")[smokeStep];
                    if(row.at("actor").get<int>()!=viewer_)throw std::runtime_error("UI smoke viewer mismatch");
                    selected_=0;std::size_t index=0;
                    for(;index<view_.actions.size();++index)if(encodeCommand(view_.actions[index].command)==row.at("command"))break;
                    if(index==view_.actions.size())throw std::runtime_error("UI smoke operation unavailable");
                    actionScroll_=static_cast<int>(index);draw(window);
                    bool clicked=false;
                    for(const auto& item:actionHits_)if(encodeCommand(item.second.command)==row.at("command")) {auto point=item.first.position+sf::Vector2f{5,5};click(point);clicked=true;break;}
                    if(!clicked || !pending_)throw std::runtime_error("UI smoke click failed");
                    click({1200,850});
                    if(session_.engine().digest()!=row.at("digest").get<std::string>())throw std::runtime_error("UI smoke state mismatch");
                    ++smokeStep;
                } else {std::cout<<"UI replay passed: "<<smokeStep<<" confirmed actions, digest "<<session_.engine().digest()<<'\n';window.close();}
            }
            if(!screenshot.empty() && ++frames==3) {
                sf::Texture capture;if(!capture.resize(window.getSize()))throw std::runtime_error("capture resize failed");capture.update(window);
                if(!capture.copyToImage().saveToFile(screenshot))throw std::runtime_error("capture write failed");window.close();
            }
        }
    }
private:
    std::filesystem::path assets_;Content content_;MatchSession session_;Resources resources_;Theme theme_;Animation animation_;
    GameView view_;PlayerId viewer_{};Scene scene_{Scene::Handoff};CardId selected_{};std::optional<LegalAction> pending_;
    std::string status_;std::filesystem::path logPath_;int actionScroll_{},handScroll_{};std::array<int,10> zoneScroll_{};
    std::vector<std::pair<sf::FloatRect,CardId>> cardHits_;std::vector<std::pair<sf::FloatRect,LegalAction>> actionHits_;std::vector<sf::FloatRect> zoneRects_;
    void refresh(bool initial=false) {
        const auto& state=session_.engine().state();int next=state.decision?state.decision->player:state.active;
        bool handoff=initial || next!=viewer_;viewer_=next;view_=session_.engine().viewFor(viewer_);
        scene_=state.result!=-1?Scene::Result:handoff?Scene::Handoff:Scene::Match;
        selected_=0;pending_.reset();actionScroll_=0;handScroll_=0;
    }
    void newLog() {logPath_=std::filesystem::path("logs")/("match-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".json");}
    void save() {try {session_.save(logPath_);status_="已保存复盘到 logs 目录";}catch(const std::exception& e){status_=e.what();}}
    void click(sf::Vector2f p) {
        if(scene_==Scene::Handoff) {if(hit(p,{{510,835},{480,60}}))scene_=Scene::Match;return;}
        if(scene_==Scene::Result) {if(hit(p,{{1160,830},{410,48}})) {save();auto seed=static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count());session_=MatchSession(content_,seed);newLog();refresh(true);}return;}
        if(hit(p,{{1160,930},{195,44}})) {save();return;}
        if(pending_) {
            if(hit(p,{{1160,828},{195,48}})) {try {auto result=session_.submit(viewer_,pending_->command);if(result.accepted){animation_.start();resources_.play(assets_/"audio"/"confirm.wav");refresh();status_="操作完成";session_.save(logPath_);}else{status_=result.error;pending_.reset();}}catch(const std::exception& e){status_=e.what();pending_.reset();}return;}
            if(hit(p,{{1370,828},{200,48}})) {pending_.reset();return;}
        }
        for(const auto& [rect,id]:cardHits_) if(hit(p,rect)) {selected_=id;pending_.reset();actionScroll_=0;return;}
        if(hit(p,{{1370,930},{200,44}})) {selected_=0;actionScroll_=0;pending_.reset();return;}
        for(const auto& [rect,action]:actionHits_) if(hit(p,rect)) {pending_=action;return;}
    }
    void label(sf::RenderTarget& target,const std::string& s,float x,float y,unsigned size=18,sf::Color color=sf::Color::White) {text(target,resources_,s,{x,y},size,color);}
    void card(sf::RenderTarget& target,const CardView& c,sf::FloatRect rect) {
        const auto& i=c.instance;const auto& d=c.definition;
        box(target,rect,selected_==i.id?theme_.selected:theme_.panel,selected_==i.id?theme_.accent:sf::Color(52,65,81));
        label(target,d.name+(i.base?" [基础]":""),rect.position.x+10,rect.position.y+5,17,theme_.ink);
        std::string sub="#"+std::to_string(i.id)+" "+(i.spell==SpellState::None?kindName(d.type):spellName(i.spell));
        if(i.host)sub+=" →#"+std::to_string(i.host);
        label(target,sub,rect.position.x+10,rect.position.y+29,13,theme_.muted);cardHits_.push_back({rect,i.id});
    }
    void drawPlayer(sf::RenderTarget& target,PlayerId player,float y) {
        const auto& p=view_.players[player];
        label(target,"玩家 "+std::to_string(player+1)+(player==viewer_?" · 本方":" · 对方"),26,y,23,player==viewer_?theme_.accent:theme_.ink);
        label(target,"生命 "+std::to_string(p.life)+"    魔素 "+std::to_string(p.mana)+" / 12    荷载 "+std::to_string(p.load)+" / "+std::to_string(p.capacity)+"    手牌 "+std::to_string(p.handCount)+"    牌库 "+std::to_string(p.deckCount)+"    行动 "+std::to_string(p.actions)+" / 3",26,y+37,18,theme_.muted);
        const std::array<Zone,5> zones={Zone::Analysis,Zone::Words,Zone::Casting,Zone::Action,Zone::Ash};
        for(std::size_t z=0;z<zones.size();++z) {
            const float x=26+static_cast<float>(z)*219;sf::FloatRect rect({x,y+76},{208,222});zoneRects_.push_back(rect);box(target,rect,sf::Color(19,27,42));
            std::vector<const CardView*> cards;for(const auto& c:view_.cards) {
                auto zone=c.instance.zone;
                if(zone==Zone::Attached)for(const auto& host:view_.cards)if(host.instance.id==c.instance.host)zone=host.instance.zone;
                if(c.instance.owner==player && zone==zones[z])cards.push_back(&c);
            }
            const auto idx=zoneRects_.size()-1;zoneScroll_[idx]=std::min(zoneScroll_[idx],std::max(0,static_cast<int>(cards.size())-3));
            label(target,zoneName(zones[z])+"  "+std::to_string(cards.size())+(cards.size()>3?" · 滚轮":""),x+8,y+81,17,theme_.muted);
            for(int k=0;k<3;++k) {int at=zoneScroll_[idx]+k;if(at>=static_cast<int>(cards.size()))break;card(target,*cards[static_cast<std::size_t>(at)],{{x+6,y+113+57*static_cast<float>(k)},{196,52}});}
        }
    }
    void draw(sf::RenderWindow& target) {
        target.clear(theme_.background);cardHits_.clear();actionHits_.clear();zoneRects_.clear();
        label(target,"巫 师 牌",26,17,32,theme_.accent);label(target,"本地双人  /  第 "+std::to_string(view_.players[view_.active].ownTurn)+" 回合  /  "+phaseName(view_.phase)+"阶段",235,27,20,theme_.muted);
        if(animation_.remaining>0)box(target,{{26,66},{1090,3}},theme_.accent);
        drawPlayer(target,1-viewer_,88);drawPlayer(target,viewer_,413);
        box(target,{{26,760},{1090,212}},sf::Color(19,27,42));
        if(scene_==Scene::Handoff) {
            label(target,"请将操作交给玩家 "+std::to_string(viewer_+1),355,787,26,theme_.ink);
            box(target,{{510,835},{480,60}},theme_.selected);label(target,"确认接手 · 显示自己的手牌",535,846,24,theme_.accent);
            // Handoff hit area includes this button and never exposes private labels.
        } else {
            label(target,"手牌 · 点击卡牌查看可执行操作 · 滚轮浏览",40,770,18,theme_.muted);
            std::vector<const CardView*> hand;for(const auto& c:view_.cards)if(c.instance.zone==Zone::Hand && c.instance.owner==viewer_)hand.push_back(&c);
            handScroll_=std::min(handScroll_,std::max(0,static_cast<int>(hand.size())-7));
            for(int k=0;k<7;++k) {int index=handScroll_+k;if(index>=static_cast<int>(hand.size()))break;const auto& c=*hand[static_cast<std::size_t>(index)];float x=40+151*static_cast<float>(k);
                card(target,c,{{x,812},{143,57}});label(target,"费用 "+std::to_string(c.definition.cost)+(c.definition.type==CardType::Analytic?" / "+std::to_string(c.definition.castCost):""),x+8,882,16,theme_.muted);
                label(target,c.definition.rank?"位阶 "+std::to_string(c.definition.rank):kindName(c.definition.type),x+8,908,16,theme_.muted);
            }
        }
        box(target,{{1140,17},{445,957}},theme_.panel);
        if(scene_==Scene::Handoff) {label(target,"等待换手",1160,38,25,theme_.accent);wrapped(target,resources_,"私有手牌、操作和日志已遮挡。双方公开区域仍可查看。",{1160,95},19,theme_.muted,19);return;}
        label(target,scene_==Scene::Result?"对局结束":view_.decision?"等待选择":"操作面板",1160,35,26,theme_.accent);
        if(scene_==Scene::Result) {
            label(target,view_.result==2?"平局":"玩家 "+std::to_string(view_.result+1)+" 获胜",1160,100,30,theme_.ink);
            std::string reasons;for(const auto& e:view_.events)if(e.kind=="defeat_reason")reasons+=e.text+"\n";
            wrapped(target,resources_,reasons+"\n日志包含完整命令、种子、内容版本及统计，可用命令行重放验证。",{1160,170},20,theme_.muted,18);
            box(target,{{1160,830},{410,48}},theme_.selected);label(target,"保存并开始新对局",1180,841,21,theme_.accent);return;
        }
        std::string detail="选择一张卡牌，或直接使用下方操作。目标与额外成本会列在操作名称中。";
        for(const auto& c:view_.cards)if(c.instance.id==selected_) {
            detail=c.definition.name+" #"+std::to_string(selected_)+"\n"+c.definition.text+"\n绑定荷载 "+std::to_string(c.instance.analysisLoad+c.instance.castLoad)+(c.instance.spell==SpellState::Active?"；持续剩余 "+std::to_string(c.instance.remaining):"");
            if(c.definition.type==CardType::Formation && c.instance.zone==Zone::Analysis) detail+="\n环位 "+std::to_string(c.occupiedRings)+" / "+std::to_string(c.effectiveRings);
            if(c.instance.spell==SpellState::Analyzing)detail+="\n距解析完成："+std::to_string(c.turnsToReady)+" 个自己的准备阶段";
        }
        wrapped(target,resources_,detail,{1160,85},17,theme_.ink,23);
        std::vector<LegalAction> choices;for(const auto& a:view_.actions)if(!selected_ || !a.source || a.source==selected_)choices.push_back(a);
        actionScroll_=std::min(actionScroll_,std::max(0,static_cast<int>(choices.size())-5));
        label(target,"可用操作 "+std::to_string(choices.size())+" · 滚轮浏览",1160,270,17,theme_.muted);
        for(int k=0;k<5;++k) {int i=actionScroll_+k;if(i>=static_cast<int>(choices.size()))break;const auto& a=choices[static_cast<std::size_t>(i)];sf::FloatRect rect({1160,305+static_cast<float>(k)*62},{410,56});
            box(target,rect,theme_.background);wrapped(target,resources_,a.label,{1170,rect.position.y+6},16,theme_.ink,24);actionHits_.push_back({rect,a});}
        if(pending_) {
            wrapped(target,resources_,"确认："+pending_->label,{1160,645},18,theme_.accent,21);
            for(const auto& c:view_.cards)if(c.instance.id==pending_->source) {
                int cost=c.definition.cost,load=0;
                if(std::holds_alternative<PrepareCast>(pending_->command))cost=load=c.definition.castCost;
                else if(std::holds_alternative<StartAnalysis>(pending_->command) || std::holds_alternative<PreloadWord>(pending_->command))load=cost;
                else if(std::holds_alternative<PlayAction>(pending_->command))load=c.definition.burden;
                else if(std::holds_alternative<RemoveFormation>(pending_->command))cost=2;
                else if(auto set=std::get_if<SetFormation>(&pending_->command))cost=set->replace?2:0;
                else if(!std::holds_alternative<AttachSeal>(pending_->command))cost=0;
                label(target,"支付 "+std::to_string(cost)+" 魔素；新增荷载 "+std::to_string(load),1160,779,18,theme_.muted);
            }
            box(target,{{1160,828},{195,48}},theme_.selected);box(target,{{1370,828},{200,48}},theme_.background);label(target,"确认操作",1195,839,21,theme_.accent);label(target,"取消",1435,839,21,theme_.muted);
        } else {
            label(target,"最近记录",1160,640,18,theme_.muted);int row=0;for(auto i=view_.events.rbegin();i!=view_.events.rend() && row<5;++i,++row)wrapped(target,resources_,i->text,{1160,674+static_cast<float>(row)*28},14,theme_.muted,28);
        }
        wrapped(target,resources_,status_,{1160,886},14,theme_.accent,29);
        box(target,{{1160,930},{195,44}},theme_.background);label(target,"保存日志 F5",1175,939,18,theme_.muted);
        box(target,{{1370,930},{200,44}},theme_.background);label(target,"全部操作 Esc",1382,939,18,theme_.muted);
    }
};
}
int main(int argc,char** argv) {
    try {
        auto assets=std::filesystem::absolute(argv[0]).parent_path()/"assets";std::string screenshot;bool showcase=false;std::uint32_t seed=42;Json smoke;
        for(int i=1;i<argc;++i) {std::string arg=argv[i];if(arg=="--assets" && i+1<argc)assets=argv[++i];else if(arg=="--screenshot" && i+1<argc)screenshot=argv[++i];else if(arg=="--showcase")showcase=true;else if(arg=="--seed" && i+1<argc)seed=static_cast<std::uint32_t>(std::stoul(argv[++i]));else if(arg=="--ui-smoke" && i+1<argc){smoke=readJson(argv[++i]);seed=smoke.at("seed").get<std::uint32_t>();}}
        Client client(assets,seed);client.run(screenshot,showcase,smoke);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
