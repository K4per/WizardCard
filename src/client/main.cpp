#include "wizard/content.hpp"
#include "wizard/runtime.hpp"
#include "wizard/interaction.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>

using namespace wizard;
using namespace wizard::runtime;
namespace {
using Rect=sf::FloatRect;
const sf::Color ink{231,229,213}, muted{139,162,177}, gold{171,131,69}, mint{103,220,196}, panel{18,30,41};
const Rect handArea{{260,862},{1080,134}}, handoffButton{{580,910},{440,54}}, phaseButton{{1360,889},{215,54}};
const Rect saveButton{{1278,24},{90,34}},logButton{{1380,24},{90,34}},surrenderButton{{1484,24},{90,34}},cancelButton{{1100,26},{120,34}};
std::string spellName(SpellState s) {return std::array<const char*,5>{"","解析中","解析完成","待施放","持续生效"}.at(static_cast<std::size_t>(s));}
std::string kindName(CardType t) {return std::array<const char*,5>{"行动","解析法术","言灵","阵法","符文"}.at(static_cast<std::size_t>(t));}
sf::Color accent(CardType t) {return std::array<sf::Color,5>{sf::Color{216,156,90},sf::Color{109,171,219},sf::Color{175,147,224},sf::Color{113,196,166},sf::Color{218,197,122}}.at(static_cast<std::size_t>(t));}
class Client {
public:
    Client(std::filesystem::path assets,std::uint32_t seed):assets_(std::move(assets)),content_(loadContent(assets_)),session_(content_,seed),resources_(assets_) {
        const auto manifest=readJson(assets_/"art"/"runtime.json");
        for(auto it=manifest.at("cards").begin();it!=manifest.at("cards").end();++it)art_[it.key()]=assets_/"art"/it.value().get<std::string>();
        newLog();refresh(true);
    }
    void run(const std::string& screenshot,bool showcase,const Json& smoke,const std::string& inspect,int captureStep) {
        if(!smoke.is_null())(void)replay(content_,smoke);
        sf::RenderWindow window(sf::VideoMode({1600,1000}),utf8("巫师牌 · 奥术对决"));window.setFramerateLimit(60);
        sf::View logical(Rect({0,0},{1600,1000}));window.setView(logical);sf::Clock clock;
        if(showcase)scene_=Scene::Match;
        std::size_t smokeStep=0;int frames=0;
        while(window.isOpen()) {
            while(const auto event=window.pollEvent()) {
                if(event->is<sf::Event::Closed>()){save();window.close();}
                if(const auto* e=event->getIf<sf::Event::Resized>())if(e->size.x && e->size.y) {
                    float r=static_cast<float>(e->size.x)/static_cast<float>(e->size.y),b=1.6f;
                    logical.setViewport(r>b?Rect({(1-b/r)/2,0},{b/r,1}):Rect({0,(1-r/b)/2},{1,r/b}));window.setView(logical);
                }
                if(const auto* e=event->getIf<sf::Event::MouseButtonPressed>())if(e->button==sf::Mouse::Button::Left)click(window.mapPixelToCoords(e->position));
                if(const auto* e=event->getIf<sf::Event::MouseWheelScrolled>())scroll(window.mapPixelToCoords(e->position),e->delta<0?1:-1);
                if(const auto* e=event->getIf<sf::Event::KeyPressed>()) {
                    if(e->code==sf::Keyboard::Key::Escape){interaction_.cancel();showLog_=false;}
                    if(e->code==sf::Keyboard::Key::F5)save();
                }
            }
            if(!window.isOpen())break;
            animation_.update(clock.restart().asSeconds());draw(window);window.display();
            bool capture=!screenshot.empty() && (smoke.is_null()?++frames>=3:smokeStep>=static_cast<std::size_t>(captureStep));
            if(capture) {
                if(showcase)scene_=view_.result==-1?Scene::Match:Scene::Result;
                if(scene_==Scene::Match && !inspect.empty())for(const auto& c:view_.cards)if(c.definition.id==inspect){interaction_.select(c.instance.id);break;}
                draw(window);sf::Texture texture;
                if(!texture.resize(window.getSize()))throw std::runtime_error("capture resize failed");texture.update(window);
                if(!texture.copyToImage().saveToFile(screenshot))throw std::runtime_error("capture write failed");window.close();break;
            }
            if(!smoke.is_null()) {
                if(scene_==Scene::Handoff)click(handoffButton.position+sf::Vector2f{20,20});
                else if(smokeStep<smoke.at("commands").size())drive(window,smoke.at("commands")[smokeStep++]);
                else {std::cout<<"UI replay passed: "<<smokeStep<<" contextual actions, digest "<<session_.engine().digest()<<'\n';window.close();}
            }
        }
    }
private:
    struct ScrollArea {Rect rect;int key;std::vector<CardId> ids;int page;};
    std::filesystem::path assets_;Content content_;MatchSession session_;Resources resources_;Animation animation_;
    GameView view_;ui::Interaction interaction_;PlayerId viewer_{};Scene scene_{Scene::Handoff};
    std::map<std::string,std::filesystem::path> art_;std::map<int,int> offsets_;std::filesystem::path logPath_;
    std::string status_;bool showLog_{};int groupOffset_{};Rect popup_,confirm_;
    std::vector<std::pair<Rect,CardId>> cardHits_;std::vector<std::pair<Rect,std::size_t>> groupHits_;
    std::vector<std::pair<Rect,LegalAction>> decisionHits_;std::vector<ScrollArea> scrollAreas_;
    void refresh(bool initial=false) {
        const auto& s=session_.engine().state();int next=s.decision?s.decision->player:s.active;
        bool handoff=initial || next!=viewer_;viewer_=next;view_=session_.engine().viewFor(viewer_);interaction_.update(view_);
        scene_=s.result!=-1?Scene::Result:handoff?Scene::Handoff:Scene::Match;showLog_=false;groupOffset_=0;
    }
    void newLog(){logPath_=std::filesystem::path("logs")/("match-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".json");}
    void save(){try{session_.save(logPath_);status_="复盘已保存至 logs";}catch(const std::exception& e){status_=e.what();}}
    const CardView* find(CardId id)const{for(const auto& c:view_.cards)if(c.instance.id==id)return &c;return nullptr;}
    void label(sf::RenderTarget& t,const std::string& s,float x,float y,unsigned size=17,sf::Color color=ink){text(t,resources_,s,{x,y},size,color);}
    void frame(sf::RenderTarget& t,Rect r,sf::Color color=gold){box(t,r,panel,color);box(t,{r.position,{17,2}},color);box(t,{r.position+r.size-sf::Vector2f{17,2},{17,2}},color);}
    void button(sf::RenderTarget& t,Rect r,const std::string& s,bool primary=false){box(t,r,primary?sf::Color{33,67,68}:panel,primary?mint:gold);label(t,s,r.position.x+12,r.position.y+5,17,primary?mint:ink);}
    static Rect mirror(Rect r){return {{1600-r.position.x-r.size.x,960-r.position.y-r.size.y},r.size};}
    Rect side(Rect r,bool enemy){return enemy?mirror(r):r;}
    void illustration(sf::RenderTarget& t,const CardView& c,Rect r) {
        auto i=art_.find(c.definition.id);
        if(i!=art_.end()) {
            const auto& tex=resources_.texture(i->second);sf::Sprite sprite(tex);auto size=tex.getSize();
            float scale=std::min(r.size.x/static_cast<float>(size.x),r.size.y/static_cast<float>(size.y));sprite.setScale({scale,scale});
            sprite.setPosition(r.position+(r.size-sf::Vector2f{static_cast<float>(size.x)*scale,static_cast<float>(size.y)*scale})/2.f);t.draw(sprite);
        } else {
            float radius=std::min(r.size.x,r.size.y)*0.35f;sf::CircleShape sigil(radius,6);sigil.setOrigin({radius,radius});sigil.setPosition(r.position+r.size/2.f);sigil.setFillColor({25,43,54});sigil.setOutlineColor(accent(c.definition.type));sigil.setOutlineThickness(1);t.draw(sigil);
            auto name=utf8(c.definition.name);resources_.drawText(t,name.substring(0,1),r.position+r.size/2.f-sf::Vector2f{12,17},24,accent(c.definition.type));
        }
    }
    void card(sf::RenderTarget& t,const CardView& c,Rect r,bool registerHit=true) {
        auto valid=interaction_.candidates();bool glowing=std::find(valid.begin(),valid.end(),c.instance.id)!=valid.end();
        if(view_.decision && scene_==Scene::Match && !interaction_.selected())for(const auto& a:view_.actions)if(a.source==c.instance.id)glowing=true;
        if(glowing)box(t,{r.position-sf::Vector2f{3,3},r.size+sf::Vector2f{6,6}},sf::Color{41,99,90},mint);
        box(t,r,interaction_.selected()==c.instance.id?sf::Color{36,59,65}:sf::Color{20,32,43},interaction_.selected()==c.instance.id?mint:accent(c.definition.type));
        if(r.size.y<80) {
            float icon=r.size.x<110?25.f:40.f;illustration(t,c,{r.position+sf::Vector2f{2,5},{icon,r.size.y-10}});
            label(t,c.definition.name,r.position.x+icon+5,r.position.y+5,r.size.x<110?12:14);
            std::string sub=c.definition.type==CardType::Formation?std::to_string(c.occupiedRings)+"/"+std::to_string(c.effectiveRings)+" 环":spellName(c.instance.spell);
            if(c.instance.spell==SpellState::Analyzing)sub="解析 · "+std::to_string(c.turnsToReady);
            label(t,sub,r.position.x+icon+5,r.position.y+24,11,muted);
        } else {
            label(t,c.definition.name,r.position.x+6,r.position.y+4,13);
            illustration(t,c,{r.position+sf::Vector2f{3,23},{r.size.x-6,r.size.y-45}});
            std::string sub=c.instance.zone==Zone::Hand?"魔素 "+std::to_string(c.definition.cost)+" · "+kindName(c.definition.type):spellName(c.instance.spell);
            if(c.definition.type==CardType::Formation && c.instance.zone!=Zone::Hand)sub="环位 "+std::to_string(c.occupiedRings)+" / "+std::to_string(c.effectiveRings);
            if(c.instance.spell==SpellState::Analyzing)sub="解析剩余 "+std::to_string(c.turnsToReady);
            label(t,sub,r.position.x+5,r.position.y+r.size.y-21,11,muted);
        }
        if(registerHit)cardHits_.push_back({r,c.instance.id});
        if(registerHit)for(const auto& seal:view_.cards)if(seal.instance.zone==Zone::Attached && seal.instance.host==c.instance.id) {
            Rect badge{{r.position.x+r.size.x-27,r.position.y+r.size.y-26},{26,25}};box(t,badge,{55,50,31},gold);label(t,"符",badge.position.x+5,badge.position.y+2,14,gold);cardHits_.push_back({badge,seal.instance.id});
        }
    }
    std::vector<const CardView*> cards(PlayerId p,Zone z)const{std::vector<const CardView*> out;for(const auto& c:view_.cards)if(c.instance.owner==p && c.instance.zone==z)out.push_back(&c);return out;}
    void strip(sf::RenderTarget& t,Rect area,const std::vector<const CardView*>& list,int key,float width,float height) {
        int page=std::max(1,static_cast<int>((area.size.x+7)/(width+7)));auto& offset=offsets_[key];offset=std::clamp(offset,0,std::max(0,static_cast<int>(list.size())-page));
        ScrollArea scroll{area,key,{},page};for(auto c:list)scroll.ids.push_back(c->instance.id);scrollAreas_.push_back(scroll);
        for(int n=0;n<page && offset+n<static_cast<int>(list.size());++n)card(t,*list[static_cast<std::size_t>(offset+n)],{area.position+sf::Vector2f{static_cast<float>(n)*(width+7),0},{width,height}});
        if(static_cast<int>(list.size())>page)label(t,"滚轮 "+std::to_string(offset+1)+" / "+std::to_string(list.size()),area.position.x+area.size.x-115,area.position.y+area.size.y-15,11,muted);
    }
    void region(sf::RenderTarget& t,PlayerId p,Zone z,Rect r,const std::string& name) {
        frame(t,r,{54,77,86});label(t,name,r.position.x+9,r.position.y+4,13,gold);
        auto list=cards(p,z);
        if(list.empty()){label(t,z==Zone::Action?"行动在此结算 · 不限次数":"—",r.position.x+12,r.position.y+25,12,muted);return;}
        float h=r.size.y-27,w=h<55?155.f:115.f;
        strip(t,{r.position+sf::Vector2f{9,24},{r.size.x-18,h}},list,p*20+static_cast<int>(z),w,h-3);
    }
    void board(sf::RenderTarget& t,PlayerId p,bool enemy) {
        region(t,p,Zone::Casting,side({{260,487},{1080,65}},enemy),"施 法 区");
        region(t,p,Zone::Words,side({{809,560},{531,106}},enemy),"言 灵 区");
        region(t,p,Zone::Action,side({{809,674},{531,49}},enemy),"行 动 区");
        region(t,p,Zone::Ash,side({{809,731},{531,101}},enemy),"灰 烬 区");
        for(int row=0;row<5;++row) {
            float y=560+55.f*static_cast<float>(row);Rect slot=side({{260,y},{106,52}},enemy),analysis=side({{373,y},{427,52}},enemy);
            frame(t,slot,{54,77,86});label(t,"阵法槽 "+std::to_string(row+1),slot.position.x+15,slot.position.y+16,13,muted);
            frame(t,analysis,{44,65,76});label(t,"解析区",analysis.position.x+14,analysis.position.y+16,13,{57,79,89});
        }
        int slot=0;for(auto c:cards(p,Zone::Analysis))if(c->definition.type==CardType::Formation) {
            float height=55.f*static_cast<float>(c->definition.body)-11;
            Rect formation=side({{264,564+55.f*static_cast<float>(slot)},{98,height}},enemy);
            Rect analysis=side({{380,564+55.f*static_cast<float>(slot)},{413,height}},enemy);
            card(t,*c,formation);std::vector<const CardView*> hosted;
            for(auto spell:cards(p,Zone::Analysis))if(spell->instance.host==c->instance.id)hosted.push_back(spell);
            if(!hosted.empty())strip(t,analysis,hosted,1000+static_cast<int>(c->instance.id),height<80?125.f:90.f,std::min(112.f,height));
            slot+=c->definition.body;
        }
    }
    void hud(sf::RenderTarget& t,PlayerId p,bool enemy) {
        Rect r=enemy?Rect({1360,127},{215,190}):Rect({25,642},{215,190});frame(t,r);const auto& v=view_.players[p];
        label(t,(enemy?"对手 · 玩家 ":"本方 · 玩家 ")+std::to_string(p+1),r.position.x+14,r.position.y+10,17,gold);
        label(t,std::to_string(v.life),r.position.x+14,r.position.y+39, 40,ink);label(t,"生命",r.position.x+93,r.position.y+64,14,muted);
        label(t,"魔素   "+std::to_string(v.mana)+" / 12",r.position.x+14,r.position.y+105,19,mint);
        label(t,"荷载   "+std::to_string(v.load)+" / "+std::to_string(v.capacity),r.position.x+14,r.position.y+137,19,ink);
        label(t,"手牌 "+std::to_string(v.handCount)+"  ·  牌库 "+std::to_string(v.deckCount),r.position.x+14,r.position.y+168,12,muted);
        Rect deck=enemy?Rect({107,135},{100,132}):Rect({1388,696},{100,132});frame(t,deck);label(t,"牌 库",deck.position.x+23,deck.position.y+28,19,gold);label(t,std::to_string(v.deckCount),deck.position.x+35,deck.position.y+71,28,ink);
    }
    std::optional<LegalAction> phaseAction()const {
        for(const auto& a:view_.actions)if(std::holds_alternative<Advance>(a.command))return a;
        for(const auto& a:view_.actions)if(auto choice=std::get_if<Choose>(&a.command))if(!choice->option)return a;
        return {};
    }
    void context(sf::RenderTarget& t) {
        if(interaction_.step()==ui::Step::Target || interaction_.step()==ui::Step::Cost){button(t,cancelButton,"取消选择");return;}
        if(!interaction_.selected() && !interaction_.pending())return;
        float x=565,y=490;for(const auto& h:cardHits_)if(h.second==interaction_.selected()) {
            x=h.first.position.x+h.first.size.x+12;if(x+450>1580)x=h.first.position.x-462;
            y=h.first.position.y>850?490:h.first.position.y-60;
        }
        popup_={{std::clamp(x,20.f,1130.f),std::clamp(y,100.f,590.f)},{450,355}};frame(t,popup_,mint);x=popup_.position.x;y=popup_.position.y;
        const auto* c=find(interaction_.selected());if(!c && interaction_.pending())c=find(interaction_.pending()->source);
        label(t,c?c->definition.name:"确认操作",x+16,y+12,23,gold);button(t,{{x+342,y+12},{92,31}},"关闭 Esc");
        if(c){illustration(t,*c,{{x+16,y+55},{126,150}});wrapped(t,resources_,c->definition.text,{x+16,y+228},15,ink,27);
            std::string detail="绑定荷载 "+std::to_string(c->instance.analysisLoad+c->instance.castLoad);
            if(c->instance.spell==SpellState::Analyzing)detail+=" · 解析剩余 "+std::to_string(c->turnsToReady)+" 回合";
            if(c->instance.spell==SpellState::Active)detail+=" · 持续剩余 "+std::to_string(c->instance.remaining);
            label(t,detail,x+16,y+324,13,muted);
        }
        if(interaction_.pending()) {
            const auto& a=*interaction_.pending();wrapped(t,resources_,a.label,{x+158,y+57},17,ink,15);
            if(c) {
                int cost=0,load=0;
                if(std::holds_alternative<PrepareCast>(a.command))cost=load=c->definition.castCost;
                else if(std::holds_alternative<StartAnalysis>(a.command) || std::holds_alternative<PreloadWord>(a.command))cost=load=c->definition.cost;
                else if(std::holds_alternative<PlayAction>(a.command)){cost=c->definition.cost;load=c->definition.burden;}
                else if(std::holds_alternative<AttachSeal>(a.command))cost=c->definition.cost;
                else if(std::holds_alternative<RemoveFormation>(a.command))cost=2;
                else if(auto set=std::get_if<SetFormation>(&a.command))cost=set->replace?2:0;
                label(t,"支付 "+std::to_string(cost)+" 魔素 · 新增荷载 "+std::to_string(load),x+158,y+147,14,muted);
            }
            confirm_={{x+158,y+184},{274,37}};button(t,confirm_,"确认操作",true);
        } else {
            auto groups=interaction_.groups();groupOffset_=std::clamp(groupOffset_,0,std::max(0,static_cast<int>(groups.size())-4));
            if(groups.empty())wrapped(t,resources_,"当前没有可执行操作",{x+158,y+61},17,muted,15);
            for(int n=0;n<4 && n+groupOffset_<static_cast<int>(groups.size());++n){std::size_t index=static_cast<std::size_t>(n+groupOffset_);Rect r{{x+158,y+55+40.f*static_cast<float>(n)},{274,34}};button(t,r,groups[index].title);groupHits_.push_back({r,index});}
            if(groups.size()>4)label(t,"滚轮查看更多操作",x+158,y+213,12,muted);
        }
    }
    void draw(sf::RenderWindow& t) {
        t.clear({10,17,25});cardHits_.clear();groupHits_.clear();decisionHits_.clear();scrollAreas_.clear();
        sf::CircleShape ritual(360,80);ritual.setOrigin({360,360});ritual.setPosition({800,480});ritual.setFillColor(sf::Color::Transparent);ritual.setOutlineColor({25,43,50});ritual.setOutlineThickness(2);t.draw(ritual);
        label(t,"巫 师 牌",25,16,31,gold);label(t,"ARCANE DUEL",27,55,12,muted);
        std::string prompt="第 "+std::to_string(view_.players[view_.active].ownTurn)+" 回合  /  "+phaseName(view_.phase)+"阶段";
        if(interaction_.step()==ui::Step::Target)prompt="选择高亮的目标卡牌";
        else if(interaction_.step()==ui::Step::Cost)prompt="选择高亮手牌，支付弃牌成本";
        else if(view_.decision)prompt+=std::string("  ·  ")+std::array<const char*,6>{"选择言灵，或跳过响应","选择待释放法术","选择触发顺序","选择要弃置的手牌","选择要移除的临时荷载","选择要移除的多余法术"}.at(static_cast<std::size_t>(view_.decision->kind));
        label(t,prompt,320,29,19,mint);
        board(t,1-viewer_,true);board(t,viewer_,false);hud(t,1-viewer_,true);hud(t,viewer_,false);
        box(t,{{260,479},{1080,2}},animation_.remaining>0?mint:gold);box(t,{{670,465},{260,29}},panel,gold);label(t,"奥 术 对 决",747,468,15,gold);
        int backs=std::min(12,view_.players[1-viewer_].handCount);for(int i=0;i<backs;++i){Rect r{{800-static_cast<float>(backs)*24+48.f*static_cast<float>(i),77},{39,41}};frame(t,r,{68,87,98});label(t,"◇",r.position.x+9,r.position.y+7,19,gold);}
        frame(t,handArea,{51,74,86});label(t,"手 牌",174,902,19,gold);
        if(scene_==Scene::Handoff) {
            label(t,"请将操作交给玩家 "+std::to_string(viewer_+1)+" · 手牌已遮挡",555,875,21,ink);button(t,handoffButton,"确认接手 · 显示自己的手牌",true);return;
        }
        strip(t,{{270,871},{1060,122}},cards(viewer_,Zone::Hand),-1,101,118);
        button(t,saveButton,"保存");button(t,logButton,"记录");button(t,surrenderButton,"投降");
        auto action=phaseAction();button(t,phaseButton,action?(view_.decision?"跳过 / 停止":"结束主要阶段"):"请完成场内选择",action.has_value());
        wrapped(t,resources_,status_,{1360,951},12,muted,17);
        if(view_.decision && !interaction_.selected() && !interaction_.pending()) {
            std::vector<LegalAction> choices;for(const auto& a:view_.actions)if(!a.source)if(auto ch=std::get_if<Choose>(&a.command))if(ch->option)choices.push_back(a);
            if(!choices.empty()) {
                int page=8;auto& offset=offsets_[-2];offset=std::clamp(offset,0,std::max(0,static_cast<int>(choices.size())-page));
                Rect r{{550,180},{500,70+43.f*static_cast<float>(std::min(page,static_cast<int>(choices.size())))}};frame(t,r,mint);label(t,"待处理选择 · 滚轮浏览",566,194,20,mint);
                for(int n=0;n<page && n+offset<static_cast<int>(choices.size());++n){Rect b{{565,230+43.f*static_cast<float>(n)},{470,37}};const auto& a=choices[static_cast<std::size_t>(n+offset)];button(t,b,a.label);decisionHits_.push_back({b,a});}
            }
        }
        context(t);
        if(showLog_) {
            frame(t,{{370,160},{860,640}},gold);label(t,"对局记录 · 点击记录或 Esc 关闭",395,180,24,gold);int row=0;
            for(auto it=view_.events.rbegin();it!=view_.events.rend() && row<13;++it,++row)wrapped(t,resources_,it->text,{395,235+40.f*static_cast<float>(row)},16,ink,48);
        }
        if(scene_==Scene::Result) {
            frame(t,{{520,327},{560,305}},mint);label(t,view_.result==2?"对局结束 · 平局":"玩家 "+std::to_string(view_.result+1)+" 获胜",558,355,32,gold);
            std::string reasons;for(const auto& e:view_.events)if(e.kind=="defeat_reason")reasons+=e.text+"\n";
            wrapped(t,resources_,reasons,{558,414},18,ink,25);button(t,{{558,553},{484,48}},"保存并开始新对局",true);
        }
    }
    void submit() {
        if(!interaction_.pending())return;auto command=interaction_.pending()->command;
        try{auto result=session_.submit(viewer_,command);if(result.accepted){animation_.start();resources_.play(assets_/"audio"/"confirm.wav");refresh();status_="操作完成";session_.save(logPath_);}else{status_=result.error;interaction_.cancel();}}
        catch(const std::exception& e){status_=e.what();interaction_.cancel();}
    }
    void click(sf::Vector2f p) {
        if(scene_==Scene::Handoff){if(hit(p,handoffButton))scene_=Scene::Match;return;}
        if(scene_==Scene::Result){if(hit(p,{{558,553},{484,48}})){save();session_=MatchSession(content_,static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));newLog();refresh(true);}return;}
        if(hit(p,saveButton)){save();return;}if(hit(p,logButton)){showLog_=!showLog_;return;}if(showLog_)return;
        if(interaction_.step()==ui::Step::Target || interaction_.step()==ui::Step::Cost) {
            if(hit(p,cancelButton)){interaction_.cancel();return;}
            for(auto it=cardHits_.rbegin();it!=cardHits_.rend();++it)if(hit(p,it->first)){if(!interaction_.pick(it->second))status_="请选择高亮卡牌";return;}return;
        }
        if(interaction_.pending() || interaction_.selected()) {
            if(hit(p,{popup_.position+sf::Vector2f{342,12},{92,31}})){interaction_.cancel();return;}
            if(interaction_.pending() && hit(p,confirm_)){submit();return;}
            for(const auto& h:groupHits_)if(hit(p,h.first)){interaction_.activate(h.second);return;}
            if(hit(p,popup_))return;
            interaction_.cancel();
        }
        if(hit(p,phaseButton)){if(auto a=phaseAction())interaction_.offer(*a);return;}
        if(hit(p,surrenderButton)){for(const auto& a:view_.actions)if(std::holds_alternative<Surrender>(a.command)){interaction_.offer(a);break;}return;}
        for(const auto& h:decisionHits_)if(hit(p,h.first)){interaction_.offer(h.second);return;}
        for(auto it=cardHits_.rbegin();it!=cardHits_.rend();++it)if(hit(p,it->first)){interaction_.select(it->second);groupOffset_=0;return;}
    }
    void scroll(sf::Vector2f p,int delta) {
        if(scene_!=Scene::Match || showLog_)return;
        if(interaction_.selected() && interaction_.step()==ui::Step::Inspect && hit(p,popup_)){groupOffset_=std::max(0,groupOffset_+delta);return;}
        if(!decisionHits_.empty() && hit(p,{{550,180},{500,420}})){offsets_[-2]=std::max(0,offsets_[-2]+delta);return;}
        for(const auto& a:scrollAreas_)if(hit(p,a.rect)){offsets_[a.key]=std::clamp(offsets_[a.key]+delta,0,std::max(0,static_cast<int>(a.ids.size())-a.page));return;}
    }
    void clickCard(sf::RenderWindow& w,CardId id) {
        draw(w);for(const auto& h:cardHits_)if(h.second==id){click(h.first.position+sf::Vector2f{6,6});return;}
        const auto* c=find(id);if(c && c->instance.zone==Zone::Attached) {
            auto host=c->instance.host;for(const auto& a:scrollAreas_) {auto at=std::find(a.ids.begin(),a.ids.end(),host);if(at!=a.ids.end())offsets_[a.key]=static_cast<int>(at-a.ids.begin());}
        } else for(const auto& a:scrollAreas_){auto at=std::find(a.ids.begin(),a.ids.end(),id);if(at!=a.ids.end())offsets_[a.key]=static_cast<int>(at-a.ids.begin());}
        draw(w);for(const auto& h:cardHits_)if(h.second==id){click(h.first.position+sf::Vector2f{6,6});return;}
        throw std::runtime_error("UI card not visible: "+std::to_string(id));
    }
    void drive(sf::RenderWindow& w,const Json& row) {
        if(row.at("actor").get<int>()!=viewer_)throw std::runtime_error("UI viewer mismatch");
        auto it=std::find_if(view_.actions.begin(),view_.actions.end(),[&](const LegalAction& a){return encodeCommand(a.command)==row.at("command");});
        if(it==view_.actions.end())throw std::runtime_error("UI operation unavailable");LegalAction a=*it;
        if(a.source) {
            clickCard(w,a.source);auto groups=interaction_.groups();auto title=ui::Interaction::intent(a,view_.decision);std::size_t n=0;for(;n<groups.size();++n)if(groups[n].title==title)break;
            groupOffset_=static_cast<int>(n);draw(w);bool clicked=false;
            for(const auto& h:groupHits_)if(h.second==n){auto p=h.first.position+sf::Vector2f{6,6};click(p);clicked=true;break;}
            if(!clicked)throw std::runtime_error("UI context unavailable: "+a.label);
            if(interaction_.step()==ui::Step::Target)clickCard(w,ui::Interaction::target(a.command));
            if(interaction_.step()==ui::Step::Cost)clickCard(w,ui::Interaction::costCard(a.command));
        } else {
            bool clicked=false;draw(w);
            for(std::size_t page=0;page<=view_.actions.size()/8 && !clicked;++page) {
                offsets_[-2]=static_cast<int>(page*8);draw(w);
                for(const auto& h:decisionHits_)if(encodeCommand(h.second.command)==row.at("command")){auto p=h.first.position+sf::Vector2f{6,6};click(p);clicked=true;break;}
            }
            if(!clicked)click((std::holds_alternative<Surrender>(a.command)?surrenderButton:phaseButton).position+sf::Vector2f{6,6});
        }
        if(!interaction_.pending() || encodeCommand(interaction_.pending()->command)!=row.at("command"))throw std::runtime_error("UI selection mismatch: "+a.label);
        draw(w);click(confirm_.position+sf::Vector2f{6,6});
        if(session_.engine().digest()!=row.at("digest").get<std::string>())throw std::runtime_error("UI state mismatch: "+a.label);
    }
};
}
int main(int argc,char** argv) {
    try {
        auto assets=std::filesystem::absolute(argv[0]).parent_path()/"assets";std::string screenshot,inspect;bool showcase=false;std::uint32_t seed=42;Json smoke;int captureStep=3;
        for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--assets" && i+1<argc)assets=argv[++i];else if(arg=="--screenshot" && i+1<argc)screenshot=argv[++i];else if(arg=="--showcase")showcase=true;else if(arg=="--inspect-card" && i+1<argc)inspect=argv[++i];else if(arg=="--capture-step" && i+1<argc)captureStep=std::stoi(argv[++i]);else if(arg=="--seed" && i+1<argc)seed=static_cast<std::uint32_t>(std::stoul(argv[++i]));else if(arg=="--ui-smoke" && i+1<argc){smoke=readJson(argv[++i]);seed=smoke.at("seed").get<std::uint32_t>();}}
        Client client(assets,seed);client.run(screenshot,showcase,smoke,inspect,captureStep);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
