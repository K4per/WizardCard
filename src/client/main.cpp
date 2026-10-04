#include "wizard/content.hpp"
#include "wizard/runtime.hpp"
#include "wizard/interaction.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <map>
#include <set>
#include <cmath>

using namespace wizard;
using namespace wizard::runtime;
namespace {
using Rect=sf::FloatRect;
const sf::Color ink{231,229,213}, muted{139,162,177}, gold{171,131,69}, mint{103,220,196}, panel{18,30,41};
const Rect handArea{{260,862},{1080,134}}, handoffButton{{580,910},{440,54}}, phaseButton{{1360,889},{215,54}};
const Rect sortType{{25,865},{100,34}},sortCost{{135,865},{100,34}};
const Rect detailPanel{{1360,330},{215,354}},detailClose{{1542,340},{25,27}};
const Rect saveButton{{1278,24},{90,34}},logButton{{1380,24},{90,34}},surrenderButton{{1484,24},{90,34}},cancelButton{{1100,26},{120,34}};
std::string spellName(SpellState s) {return std::array<const char*,5>{"","解析中","解析完成","待施放","持续生效"}.at(static_cast<std::size_t>(s));}
std::string kindName(CardType t) {return std::array<const char*,5>{"行动","解析法术","言灵","阵法","符文"}.at(static_cast<std::size_t>(t));}
sf::Color accent(CardType t) {return std::array<sf::Color,5>{sf::Color{216,156,90},sf::Color{109,171,219},sf::Color{175,147,224},sf::Color{113,196,166},sf::Color{218,197,122}}.at(static_cast<std::size_t>(t));}
class Client {
public:
    Client(std::filesystem::path assets,std::uint32_t seed):assets_(std::move(assets)),content_(loadContent(assets_)),session_(content_,seed),resources_(assets_) {
        const auto manifest=readJson(assets_/"art"/"runtime.json");
        for(auto it=manifest.at("cards").begin();it!=manifest.at("cards").end();++it)art_[it.key()]=assets_/"art"/it.value().get<std::string>();
        const auto skin=readJson(assets_/"art"/"ui"/"manifest.json");
        for(const auto& item:skin.at("assets")) {
            SkinPart part{assets_/"art"/item.at("file").get<std::string>(),{}};
            if(item.contains("insets"))part.insets=item.at("insets").get<std::array<int,4>>();
            skin_.emplace(item.at("id").get<std::string>(),std::move(part));
        }
        newLog();refresh(true);
    }
    void run(const std::string& screenshot,bool showcase,const Json& smoke,const std::string& inspect,int captureStep,bool dragSmoke,bool previewDrag) {
        smokeDragging_=dragSmoke;
        if(!smoke.is_null())(void)replay(content_,smoke);
        sf::RenderWindow window(sf::VideoMode({1600,1000}),utf8("巫师牌 · 奥术对决"));window.setFramerateLimit(60);
        sf::View logical(Rect({0,0},{1600,1000}));window.setView(logical);sf::Clock clock;
        if(showcase)scene_=Scene::Match;
        std::size_t smokeStep=0;int frames=0;bool exercised=false;
        while(window.isOpen()) {
            while(const auto event=window.pollEvent()) {
                if(event->is<sf::Event::Closed>()){save();window.close();}
                if(const auto* e=event->getIf<sf::Event::Resized>())if(e->size.x && e->size.y) {
                    float r=static_cast<float>(e->size.x)/static_cast<float>(e->size.y),b=1.6f;
                    logical.setViewport(r>b?Rect({(1-b/r)/2,0},{b/r,1}):Rect({0,(1-r/b)/2},{1,r/b}));window.setView(logical);
                }
                if(const auto* e=event->getIf<sf::Event::MouseButtonPressed>()) {
                    if(e->button==sf::Mouse::Button::Left)press(window.mapPixelToCoords(e->position));
                    if(e->button==sf::Mouse::Button::Right){resetPointer();interaction_.cancel();}
                }
                if(const auto* e=event->getIf<sf::Event::MouseMoved>())movePointer(window.mapPixelToCoords(e->position));
                if(const auto* e=event->getIf<sf::Event::MouseButtonReleased>())if(e->button==sf::Mouse::Button::Left)releasePointer(window.mapPixelToCoords(e->position));
                if(event->is<sf::Event::FocusLost>() || event->is<sf::Event::MouseLeft>()){resetPointer();pointer_={-100,-100};}
                if(const auto* e=event->getIf<sf::Event::MouseWheelScrolled>())scroll(window.mapPixelToCoords(e->position),e->delta<0?1:-1);
                if(const auto* e=event->getIf<sf::Event::KeyPressed>()) {
                    if(e->code==sf::Keyboard::Key::Escape){resetPointer();interaction_.cancel();showLog_=false;}
                    if(e->code==sf::Keyboard::Key::F5)save();
                }
            }
            if(!window.isOpen())break;
            float dt=clock.restart().asSeconds();animation_.update(dt);updateDrag(dt);draw(window);window.display();
            bool capture=!screenshot.empty() && (smoke.is_null()?++frames>=3:smokeStep>=static_cast<std::size_t>(captureStep));
            if(capture) {
                if(showcase)scene_=view_.result==-1?Scene::Match:Scene::Result;
                if(scene_==Scene::Match && !inspect.empty())for(const auto& c:view_.cards)if(c.definition.id==inspect){interaction_.select(c.instance.id);break;}
                if(previewDrag && scene_==Scene::Match && interaction_.selected()) {
                    auto id=interaction_.selected();auto origin=pointFor(window,id);sf::Vector2f destination{789,817};
                    for(const auto& a:view_.actions)if(a.source==id && ui::Interaction::target(a.command)){destination=pointFor(window,ui::Interaction::target(a.command));break;}
                    press(origin);movePointer(destination);
                }
                draw(window);sf::Texture texture;
                if(!texture.resize(window.getSize()))throw std::runtime_error("capture resize failed");texture.update(window);
                if(!texture.copyToImage().saveToFile(screenshot))throw std::runtime_error("capture write failed");window.close();break;
            }
            if(!smoke.is_null()) {
                if(scene_==Scene::Handoff)click(handoffButton.position+sf::Vector2f{20,20});
                else if(smokeStep<smoke.at("commands").size()) {if(smokeDragging_ && !exercised){exerciseGestures(window);exercised=true;}drive(window,smoke.at("commands")[smokeStep++]);}
                else {std::cout<<"UI replay passed: "<<smokeStep<<" contextual actions, "<<dragCount_<<" drag gestures, digest "<<session_.engine().digest()<<'\n';window.close();}
            }
        }
    }
private:
    struct SkinPart {std::filesystem::path path;std::array<int,4> insets;};
    struct ScrollArea {Rect rect;int key;std::vector<CardId> ids;int page;};
    std::filesystem::path assets_;Content content_;MatchSession session_;Resources resources_;Animation animation_;
    std::map<std::string,SkinPart> skin_;std::set<std::string> missingArt_;
    ui::HandOrder handOrder_;sf::Vector2f pointer_{-100,-100},pressPoint_;bool mouseDown_{},dragging_{};CardId pressedCard_{};float edgeScroll_{};
    GameView view_;ui::Interaction interaction_;PlayerId viewer_{};Scene scene_{Scene::Handoff};
    std::map<std::string,std::filesystem::path> art_;std::map<int,int> offsets_;std::filesystem::path logPath_;
    std::string status_;bool smokeDragging_{};int dragCount_{};bool showLog_{};int groupOffset_{};Rect actionArea_,confirm_,cancelAction_;
    std::vector<std::pair<Rect,CardId>> cardHits_;std::vector<std::pair<Rect,std::size_t>> groupHits_;
    std::vector<std::pair<Rect,LegalAction>> decisionHits_;std::vector<ScrollArea> scrollAreas_;
    void refresh(bool initial=false) {
        const auto& s=session_.engine().state();int next=s.decision?s.decision->player:s.active;
        bool handoff=initial || next!=viewer_;viewer_=next;view_=session_.engine().viewFor(viewer_);interaction_.update(view_);handOrder_.sync(view_);resetPointer();
        scene_=s.result!=-1?Scene::Result:handoff?Scene::Handoff:Scene::Match;showLog_=false;groupOffset_=0;
    }
    void newLog(){logPath_=std::filesystem::path("logs")/("match-"+std::to_string(std::chrono::system_clock::now().time_since_epoch().count())+".json");}
    void save(){try{session_.save(logPath_);status_="复盘已保存至 logs";}catch(const std::exception& e){status_=e.what();}}
    const CardView* find(CardId id)const{for(const auto& c:view_.cards)if(c.instance.id==id)return &c;return nullptr;}
    void label(sf::RenderTarget& t,const std::string& s,float x,float y,unsigned size=17,sf::Color color=ink){text(t,resources_,s,{x,y},size,color);}
    void frame(sf::RenderTarget& t,Rect r,sf::Color color=gold){box(t,r,panel,color);box(t,{r.position,{17,2}},color);box(t,{r.position+r.size-sf::Vector2f{17,2},{17,2}},color);}
    void paint(sf::RenderTarget& t,const std::string& id,Rect r,std::uint8_t alpha=255) {
        const auto& part=skin_.at(id);texturePatch(t,resources_.texture(part.path),r,part.insets,{255,255,255,alpha});
    }
    static std::string typeKey(CardType type){return std::array<const char*,5>{"action","analytic","word","formation","seal"}.at(static_cast<std::size_t>(type));}
    static std::string zoneKey(Zone zone){return std::array<const char*,8>{"deck","hand","action","analysis","words","casting","ash","analysis"}.at(static_cast<std::size_t>(zone));}
    void button(sf::RenderTarget& t,Rect r,const std::string& s,bool primary=false,bool enabled=true) {
        bool hover=hit(pointer_,r) && !dragging_;std::string state=!enabled?"disabled":hover?(mouseDown_?"pressed":"hover"):"normal";
        std::string type=s=="投降"?"danger":primary?"primary":"secondary";
        paint(t,"button_"+type+"_"+state,r);label(t,s,r.position.x+12,r.position.y+5+(hover && mouseDown_?1.f:0.f),17,enabled?ink:muted);
    }
    void backdrop(sf::RenderTarget& t) {
        paint(t,"match_background",{{0,0},{1600,1000}});
        // Subtle deterministic slate slabs beneath translucent zones, independent of game RNG.
        for(int row=0;row<8;++row)for(int col=0;col<8;++col) {
            float x=258+136.f*static_cast<float>(col),y=122+90.f*static_cast<float>(row);int tone=21+(row*13+col*7)%4;
            box(t,{{x,y},{134,88}},sf::Color(static_cast<std::uint8_t>(tone-5),static_cast<std::uint8_t>(tone+2),static_cast<std::uint8_t>(tone+6)),{22,32,38});
            box(t,{{x+3,y+3},{127,1}},{29,39,43,80});
        }
        paint(t,"board_decoration",{{0,0},{1600,1000}},125);
    }
    static Rect mirror(Rect r){return {{1600-r.position.x-r.size.x,960-r.position.y-r.size.y},r.size};}
    Rect side(Rect r,bool enemy){return enemy?mirror(r):r;}
    void illustration(sf::RenderTarget& t,const CardView& c,Rect r) {
        auto i=art_.find(c.definition.id);
        if(i!=art_.end() && !missingArt_.count(c.definition.id)) {
            try {
            const auto& tex=resources_.texture(i->second);sf::Sprite sprite(tex);auto size=tex.getSize();
            float scale=std::min(r.size.x/static_cast<float>(size.x),r.size.y/static_cast<float>(size.y));sprite.setScale({scale,scale});
            sprite.setPosition(r.position+(r.size-sf::Vector2f{static_cast<float>(size.x)*scale,static_cast<float>(size.y)*scale})/2.f);t.draw(sprite);return;
            }catch(const std::exception&){missingArt_.insert(c.definition.id);}
        }
        {
            float radius=std::min(r.size.x,r.size.y)*0.35f;sf::CircleShape sigil(radius,6);sigil.setOrigin({radius,radius});sigil.setPosition(r.position+r.size/2.f);sigil.setFillColor({25,43,54});sigil.setOutlineColor(accent(c.definition.type));sigil.setOutlineThickness(1);t.draw(sigil);
            auto name=utf8(c.definition.name);resources_.drawText(t,name.substring(0,1),r.position+r.size/2.f-sf::Vector2f{12,17},24,accent(c.definition.type));
        }
    }
    void card(sf::RenderTarget& t,const CardView& c,Rect r,bool registerHit=true) {
        const auto& d=c.definition;const auto& i=c.instance;
        std::string size=r.size.y<80?"strip":i.zone==Zone::Hand?"hand":"portrait";
        paint(t,"frame_"+typeKey(d.type)+"_"+size,r);
        if(r.size.y<80) {
            illustration(t,c,{r.position+sf::Vector2f{3,7},{26,r.size.y-13}});
            label(t,d.name,r.position.x+34,r.position.y+4,r.size.x<110?11:12);
            std::string sub=d.type==CardType::Formation?std::to_string(c.occupiedRings)+"/"+std::to_string(c.effectiveRings)+" 环":spellName(i.spell);
            if(i.spell==SpellState::Analyzing)sub="解析 · "+std::to_string(c.turnsToReady);
            label(t,sub,r.position.x+34,r.position.y+23,11,muted);
        } else {
            label(t,d.name,r.position.x+10,r.position.y+4,12);
            illustration(t,c,{r.position+sf::Vector2f{5,24},{r.size.x-10,r.size.y-56}});
            if(d.rank)label(t,std::to_string(d.rank),r.position.x+r.size.x-15,r.position.y+24,11,gold);
            std::string sub=i.zone==Zone::Hand?(d.type==CardType::Analytic?"解 "+std::to_string(d.cost)+" / 施 "+std::to_string(d.castCost):"魔素 "+std::to_string(d.cost)):spellName(i.spell);
            if(d.type==CardType::Formation && i.zone!=Zone::Hand)sub="环位 "+std::to_string(c.occupiedRings)+" / "+std::to_string(c.effectiveRings);
            if(i.spell==SpellState::Analyzing)sub="解析剩余 "+std::to_string(c.turnsToReady);
            label(t,sub,r.position.x+6,r.position.y+r.size.y-31,11,mint);
            label(t,kindName(d.type),r.position.x+6,r.position.y+r.size.y-17,10,muted);
            paint(t,"rarity_"+d.rarity,{{r.position.x+r.size.x-31,r.position.y+r.size.y-16},{27,8}});
        }
        auto valid=interaction_.candidates();bool candidate=std::find(valid.begin(),valid.end(),i.id)!=valid.end();
        if(dragging_ && i.id!=pressedCard_){auto trial=interaction_;candidate=(i.owner==viewer_ || targetsEnemy(pressedCard_)) && trial.drop(pressedCard_,dropTarget(pressedCard_,i.id),i.zone);}
        if(view_.decision && scene_==Scene::Match && !interaction_.selected() && !dragging_)for(const auto& a:view_.actions)if(a.source==i.id)candidate=true;
        if(hit(pointer_,r) && !dragging_ && scene_==Scene::Match)paint(t,"overlay_"+size+"_hover",r);
        if(interaction_.selected()==i.id || (dragging_ && pressedCard_==i.id))paint(t,"overlay_"+size+"_selected",r);
        if(candidate)paint(t,"overlay_"+size+(interaction_.step()==ui::Step::Cost?"_cost_candidate":"_target_candidate"),r);
        if(i.spell!=SpellState::None) {
            std::string state=std::array<const char*,5>{"","state_analyzing","state_analyzed","state_prepared","state_active"}.at(static_cast<std::size_t>(i.spell));
            if(i.canceledTurn==view_.players[i.owner].ownTurn)state="state_cancelled";
            paint(t,state,{{r.position.x+r.size.x-16,r.position.y+(r.size.y<80?23.f:4.f)},{13,13}});
        }
        if(i.base)paint(t,"state_protected",{{r.position.x+r.size.x-16,r.position.y+4},{13,13}});
        if(registerHit)cardHits_.push_back({r,i.id});
        if(registerHit)for(const auto& seal:view_.cards)if(seal.instance.zone==Zone::Attached && seal.instance.host==i.id) {
            Rect badge{{r.position.x+r.size.x-27,r.position.y+r.size.y-26},{26,25}};box(t,badge,{55,50,31},gold);paint(t,"state_attached",{badge.position+sf::Vector2f{2,2},{22,21}});cardHits_.push_back({badge,seal.instance.id});
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
        paint(t,"region_"+zoneKey(z),r,170);label(t,name,r.position.x+9,r.position.y+4,13,gold);
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
            paint(t,"slot_empty",slot,140);label(t,"阵法槽 "+std::to_string(row+1),slot.position.x+15,slot.position.y+16,13,muted);
            paint(t,"region_analysis",analysis,140);label(t,"解析区",analysis.position.x+14,analysis.position.y+16,13,{57,79,89});
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
        Rect r=enemy?Rect({1360,127},{215,190}):Rect({25,642},{215,190});paint(t,"hud",r);const auto& v=view_.players[p];
        label(t,(enemy?"对手 · 玩家 ":"本方 · 玩家 ")+std::to_string(p+1),r.position.x+14,r.position.y+10,17,gold);
        label(t,std::to_string(v.life),r.position.x+52,r.position.y+39, 40,ink);paint(t,"life",{{r.position.x+12,r.position.y+51},{30,30}});
        paint(t,"mana",{{r.position.x+13,r.position.y+109},{22,22}});label(t,"魔素 "+std::to_string(v.mana)+" / 12",r.position.x+43,r.position.y+105,18,mint);
        paint(t,"load",{{r.position.x+13,r.position.y+141},{22,22}});label(t,"荷载 "+std::to_string(v.load)+" / "+std::to_string(v.capacity),r.position.x+43,r.position.y+137,18,ink);
        label(t,"手牌 "+std::to_string(v.handCount)+"  ·  牌库 "+std::to_string(v.deckCount),r.position.x+14,r.position.y+168,12,muted);
        Rect deck=enemy?Rect({107,135},{100,132}):Rect({1388,696},{100,132});paint(t,"card_back_deck",deck);label(t,"牌 库",deck.position.x+23,deck.position.y+12,17,gold);label(t,std::to_string(v.deckCount),deck.position.x+35,deck.position.y+98,24,ink);
    }
    std::optional<LegalAction> phaseAction()const {
        for(const auto& a:view_.actions)if(std::holds_alternative<Advance>(a.command))return a;
        for(const auto& a:view_.actions)if(auto choice=std::get_if<Choose>(&a.command))if(!choice->option)return a;
        return {};
    }
    const CardView* inspectedCard()const {
        const auto* c=find(interaction_.selected());
        return c?c:interaction_.pending()?find(interaction_.pending()->source):nullptr;
    }
    void details(sf::RenderTarget& t,const CardView& c) {
        const auto& d=c.definition;const auto& i=c.instance;
        paint(t,"frame_"+typeKey(d.type)+"_detail",detailPanel);label(t,d.name,1372,341,20,gold);
        box(t,detailClose,panel,gold);label(t,"×",1547,342,17,ink);
        label(t,kindName(d.type)+(i.base?" · 基础阵法":""),1372,371,13,muted);
        illustration(t,c,{{1374,393},{187,83}});
        std::string stats=d.type==CardType::Analytic?"解析 "+std::to_string(d.cost)+" / 施法 "+std::to_string(d.castCost):"费用 "+std::to_string(d.cost);
        if(d.rank)stats+=" · 位阶 "+std::to_string(d.rank);
        label(t,stats,1372,484,13,mint);
        wrapped(t,resources_,d.text,{1372,512},14,ink,13);
        label(t,"绑定荷载 "+std::to_string(i.analysisLoad+i.castLoad),1372,609,13,muted);
        if(i.spell==SpellState::Analyzing)label(t,"解析剩余 "+std::to_string(c.turnsToReady)+" 回合",1372,628,13,muted);
        else if(i.spell==SpellState::Active)label(t,"持续剩余 "+std::to_string(i.remaining)+" 回合",1372,628,13,muted);
        else if(i.spell!=SpellState::None)label(t,spellName(i.spell),1372,628,13,muted);
        else if(d.type==CardType::Formation && i.zone==Zone::Analysis)label(t,"环位 "+std::to_string(c.occupiedRings)+" / "+std::to_string(c.effectiveRings),1372,628,13,muted);
        if(interaction_.pending()) {
            const auto& command=interaction_.pending()->command;int cost=0,load=0;
            if(std::holds_alternative<PrepareCast>(command))cost=load=d.castCost;
            else if(std::holds_alternative<StartAnalysis>(command) || std::holds_alternative<PreloadWord>(command))cost=load=d.cost;
            else if(std::holds_alternative<PlayAction>(command)){cost=d.cost;load=d.burden;}
            else if(std::holds_alternative<AttachSeal>(command))cost=d.cost;
            else if(std::holds_alternative<RemoveFormation>(command))cost=2;
            else if(auto set=std::get_if<SetFormation>(&command))cost=set->replace?2:0;
            label(t,"本次魔素 "+std::to_string(cost)+" / 荷载 +"+std::to_string(load),1372,658,13,mint);
        } else if(interaction_.step()==ui::Step::Inspect && interaction_.groups().empty())label(t,"当前没有可执行操作",1372,658,13,muted);
    }
    void context(sf::RenderTarget& t) {
        actionArea_={};confirm_={};cancelAction_={};
        if(!interaction_.selected() && !interaction_.pending())return;
        const auto* c=inspectedCard();if(c)details(t,*c);
        if(interaction_.step()==ui::Step::Target || interaction_.step()==ui::Step::Cost){button(t,cancelButton,"取消选择");return;}
        // The action row follows the visible card; the fixed sidebar is read-only.
        Rect anchor=phaseButton;bool visible=!c;
        if(c)for(const auto& h:cardHits_)if(h.second==c->instance.id){anchor=h.first;visible=true;break;}
        if(!visible)return;
        auto groups=interaction_.groups();
        groupOffset_=std::clamp(groupOffset_,0,std::max(0,static_cast<int>(groups.size())-4));
        int count=std::min(4,static_cast<int>(groups.size()));
        float width=interaction_.pending()?244.f:168.f*static_cast<float>(count)-8.f;
        if(!interaction_.pending() && !count)return;
        float x=std::clamp(anchor.position.x+anchor.size.x/2-width/2,260.f,(c?1340.f:1580.f)-width);
        float y=std::max(76.f,anchor.position.y-44.f);
        actionArea_={{x,y},{width,36}};
        if(interaction_.pending()) {
            label(t,"确认："+interaction_.pending()->label,320,56,15,mint);
            confirm_={{x,y},{148,36}};cancelAction_={{x+156,y},{88,36}};
            button(t,confirm_,"确认操作",true);button(t,cancelAction_,"取消");
        } else {
            for(int n=0;n<count;++n) {
                auto index=static_cast<std::size_t>(groupOffset_+n);Rect r{{x+168.f*static_cast<float>(n),y},{160,36}};
                button(t,r,groups[index].title,true);groupHits_.push_back({r,index});
            }
            if(groups.size()>4)label(t,"滚轮查看更多操作",x,y-19,12,muted);
        }
    }
    void draw(sf::RenderWindow& t) {
        t.clear({10,17,25});cardHits_.clear();groupHits_.clear();decisionHits_.clear();scrollAreas_.clear();
        backdrop(t);
        label(t,"巫 师 牌",25,16,31,gold);label(t,"ARCANE DUEL",27,55,12,muted);
        std::string prompt="第 "+std::to_string(view_.players[view_.active].ownTurn)+" 回合  /  "+phaseName(view_.phase)+"阶段";
        if(interaction_.step()==ui::Step::Target)prompt="选择高亮的目标卡牌";
        else if(interaction_.step()==ui::Step::Cost)prompt="选择高亮手牌，支付弃牌成本";
        else if(view_.decision)prompt+=std::string("  ·  ")+std::array<const char*,6>{"选择言灵，或跳过响应","选择待释放法术","选择触发顺序","选择要弃置的手牌","选择要移除的临时荷载","选择要移除的多余法术"}.at(static_cast<std::size_t>(view_.decision->kind));
        label(t,prompt,320,29,19,mint);
        board(t,1-viewer_,true);board(t,viewer_,false);hud(t,1-viewer_,true);hud(t,viewer_,false);
        box(t,{{260,479},{1080,2}},animation_.remaining>0?mint:gold);box(t,{{670,465},{260,29}},panel,gold);label(t,"奥 术 对 决",747,468,15,gold);
        int backs=std::min(12,view_.players[1-viewer_].handCount);for(int i=0;i<backs;++i){Rect r{{800-static_cast<float>(backs)*24+48.f*static_cast<float>(i),77},{39,41}};paint(t,"card_back_opponent",r);}
        paint(t,"region_hand",handArea);label(t,"手 牌",25,917,19,gold);
        if(scene_==Scene::Handoff) {
            paint(t,"handoff",handArea);
            label(t,"请将操作交给玩家 "+std::to_string(viewer_+1)+" · 手牌已遮挡",555,875,21,ink);button(t,handoffButton,"确认接手 · 显示自己的手牌",true);return;
        }
        std::vector<const CardView*> hand;for(auto id:handOrder_.cards(viewer_))if(auto c=find(id))hand.push_back(c);
        strip(t,{{270,871},{1060,122}},hand,-1,101,118);
        button(t,sortType,"按类型");button(t,sortCost,"按费用");
        label(t,"拖动排序 · 拖至场地使用",25,953,12,muted);
        button(t,saveButton,"保存");button(t,logButton,"记录");button(t,surrenderButton,"投降");
        auto action=phaseAction();button(t,phaseButton,action?(view_.decision?"跳过 / 停止":"结束主要阶段"):"请完成场内选择",action.has_value(),action.has_value());
        wrapped(t,resources_,status_,{1360,951},12,muted,17);
        if(view_.decision && !interaction_.selected() && !interaction_.pending()) {
            std::vector<LegalAction> choices;for(const auto& a:view_.actions)if(!a.source)if(auto ch=std::get_if<Choose>(&a.command))if(ch->option)choices.push_back(a);
            if(!choices.empty()) {
                int page=8;auto& offset=offsets_[-2];offset=std::clamp(offset,0,std::max(0,static_cast<int>(choices.size())-page));
                Rect r{{550,180},{500,70+43.f*static_cast<float>(std::min(page,static_cast<int>(choices.size())))}};paint(t,"decision",r);label(t,"待处理选择 · 滚轮浏览",566,194,20,mint);
                for(int n=0;n<page && n+offset<static_cast<int>(choices.size());++n){Rect b{{565,230+43.f*static_cast<float>(n)},{470,37}};const auto& a=choices[static_cast<std::size_t>(n+offset)];button(t,b,a.label);decisionHits_.push_back({b,a});}
            }
        }
        if(!dragging_)context(t);else {actionArea_={};confirm_={};if(auto c=find(pressedCard_))details(t,*c);}
        drawDrag(t);
        if(showLog_) {
            paint(t,"log_window",{{370,160},{860,640}});label(t,"对局记录 · 点击记录或 Esc 关闭",395,180,24,gold);int row=0;
            for(auto it=view_.events.rbegin();it!=view_.events.rend() && row<13;++it,++row)wrapped(t,resources_,it->text,{395,235+40.f*static_cast<float>(row)},16,ink,48);
        }
        if(scene_==Scene::Result) {
            paint(t,"result_window",{{520,327},{560,305}});label(t,view_.result==2?"对局结束 · 平局":"玩家 "+std::to_string(view_.result+1)+" 获胜",558,355,32,gold);
            std::string reasons;for(const auto& e:view_.events)if(e.kind=="defeat_reason")reasons+=e.text+"\n";
            wrapped(t,resources_,reasons,{558,414},18,ink,25);button(t,{{558,553},{484,48}},"保存并开始新对局",true);
        }
    }
    CardId cardAt(sf::Vector2f p)const {
        for(auto it=cardHits_.rbegin();it!=cardHits_.rend();++it)if(hit(p,it->first))return it->second;
        return 0;
    }
    CardId dropTarget(CardId source,CardId target)const {
        auto c=find(source);auto d=find(target);if(!c || !d)return target;
        if(d->instance.zone==Zone::Attached)target=d->instance.host;
        d=find(target);
        if(d && c->instance.zone==Zone::Hand && (c->definition.type==CardType::Analytic || c->definition.type==CardType::Formation) && d->instance.zone==Zone::Analysis && d->instance.host)target=d->instance.host;
        return target;
    }
    Zone zoneAt(sf::Vector2f p)const {
        if(hit(p,handArea))return Zone::Hand;
        if(hit(p,{{260,487},{1080,65}}))return Zone::Casting;
        if(hit(p,{{260,560},{540,272}}))return Zone::Analysis;
        if(hit(p,{{809,560},{531,106}}))return Zone::Words;
        if(hit(p,{{809,674},{531,49}}))return Zone::Action;
        if(hit(p,{{809,731},{531,101}}))return Zone::Ash;
        return Zone::Deck; // Outside a playable destination.
    }
    bool targetsEnemy(CardId source)const {
        const auto* c=find(source);return interaction_.step()==ui::Step::Target || (c && c->instance.spell==SpellState::Ready && c->definition.target==TargetKind::EmptyEnemyFormation);
    }
    bool previewDrop(ui::Interaction& trial,CardId source,sf::Vector2f p)const {
        CardId target=cardAt(p);auto zone=zoneAt(p);
        if(zone==Zone::Deck && (!target || !targetsEnemy(source)))return false;
        if(const auto* c=find(target))zone=c->instance.zone;
        return trial.drop(source,dropTarget(source,target),zone);
    }
    void resetPointer(){mouseDown_=false;dragging_=false;pressedCard_=0;edgeScroll_=0;}
    void press(sf::Vector2f p) {
        resetPointer();pointer_=pressPoint_=p;mouseDown_=true;
        if(scene_!=Scene::Match || showLog_ || hit(p,detailPanel) || hit(p,actionArea_))return;
        auto id=cardAt(p);const auto* c=find(id);if(!c || c->instance.owner!=viewer_)return;
        if(interaction_.step()==ui::Step::Confirm)return;
        if(interaction_.step()==ui::Step::Target && interaction_.selected()!=id)return;
        if(c->instance.zone==Zone::Hand || interaction_.selected()==id || std::any_of(view_.actions.begin(),view_.actions.end(),[&](const LegalAction& a){return a.source==id;}))pressedCard_=id;
    }
    void movePointer(sf::Vector2f p) {
        pointer_=p;auto delta=p-pressPoint_;
        if(mouseDown_ && pressedCard_ && delta.x*delta.x+delta.y*delta.y>=64)dragging_=true;
    }
    CardId insertBefore(sf::Vector2f p,CardId source)const {
        CardId last=0;
        for(const auto& h:cardHits_) {
            auto c=find(h.second);if(!c || c->instance.owner!=viewer_ || c->instance.zone!=Zone::Hand)continue;
            last=h.second;if(h.second!=source && p.x<h.first.position.x+h.first.size.x/2)return h.second;
        }
        const auto& order=handOrder_.cards(viewer_);auto it=std::find(order.begin(),order.end(),last);
        if(it!=order.end())for(++it;it!=order.end();++it)if(*it!=source)return *it;
        return 0;
    }
    void releasePointer(sf::Vector2f p) {
        if(!mouseDown_)return;movePointer(p);bool dragged=dragging_;auto source=pressedCard_;auto delta=p-pressPoint_;resetPointer();
        if(!dragged){if(delta.x*delta.x+delta.y*delta.y<64)click(p);return;}
        auto c=find(source);
        if(c && c->instance.zone==Zone::Hand && hit(p,handArea)) {
            handOrder_.moveBefore(viewer_,source,insertBefore(p,source));status_="已调整手牌顺序";return;
        }
        auto trial=interaction_;
        if(previewDrop(trial,source,p)){interaction_=std::move(trial);status_="已选择操作，请确认或继续选择";}
        else status_="此处不可使用，卡牌已返回";
    }
    void updateDrag(float dt) {
        if(!dragging_ || !hit(pointer_,handArea)){edgeScroll_=0;return;}
        int direction=pointer_.x<290?-1:pointer_.x>1315?1:0;
        if(!direction){edgeScroll_=0;return;}
        edgeScroll_+=dt;if(edgeScroll_<0.3f)return;edgeScroll_=0;
        offsets_[-1]=std::max(0,offsets_[-1]+direction);
    }
    void drawDrag(sf::RenderTarget& t) {
        if(!dragging_ || scene_!=Scene::Match || showLog_)return;
        const auto* c=find(pressedCard_);if(!c)return;
        const std::array<std::pair<Zone,Rect>,5> zones{{{Zone::Analysis,{{260,560},{540,272}}},{Zone::Words,{{809,560},{531,106}}},{Zone::Casting,{{260,487},{1080,65}}},{Zone::Action,{{809,674},{531,49}}},{Zone::Ash,{{809,731},{531,101}}}}};
        for(const auto& z:zones){auto trial=interaction_;if(trial.drop(pressedCard_,0,z.first))box(t,z.second,{45,110,104,24},mint);}
        bool reorder=c->instance.zone==Zone::Hand && hit(pointer_,handArea);auto trial=interaction_;bool valid=reorder || previewDrop(trial,pressedCard_,pointer_);
        sf::Color tint=valid?mint:sf::Color{224,136,112};
        if(reorder) {
            auto before=insertBefore(pointer_,pressedCard_);float x=1325;
            for(const auto& h:cardHits_)if(h.second==before)x=h.first.position.x-4;
            if(!before)for(const auto& h:cardHits_)if(const auto* hcard=find(h.second))if(hcard->instance.zone==Zone::Hand)x=h.first.position.x+h.first.size.x+4;
            box(t,{{x,870},{3,120}},mint);
        }
        sf::Vertex line[2];line[0].position=pressPoint_;line[1].position=pointer_;line[0].color=mint;line[1].color=tint;t.draw(line,2,sf::PrimitiveType::Lines);
        Rect ghost{{std::clamp(pointer_.x-45,4.f,1495.f),std::clamp(pointer_.y-90,76.f,876.f)},{101,118}};
        box(t,{ghost.position+sf::Vector2f{7,7},ghost.size},{0,0,0,145});card(t,*c,ghost,false);
        box(t,{{320,836},{850,25}},{10,22,30,240});label(t,reorder?"松手调整顺序 · 移至手牌边缘自动滚动":valid?"松手选择此操作 · 最后确认才会支付费用":"拖至高亮区域或目标 · Esc / 右键取消",332,838,14,tint);
    }
    void submit() {
        if(!interaction_.pending())return;auto command=interaction_.pending()->command;
        try{auto result=session_.submit(viewer_,command);if(result.accepted){animation_.start();resources_.play(assets_/"audio"/"confirm.wav");refresh();status_="操作完成";session_.save(logPath_);}else{status_=result.error;interaction_.cancel();}}
        catch(const std::exception& e){status_=e.what();interaction_.cancel();}
    }
    void click(sf::Vector2f p) {
        if(scene_==Scene::Handoff){if(hit(p,handoffButton))scene_=Scene::Match;return;}
        if(scene_==Scene::Result){if(hit(p,{{558,553},{484,48}})){save();session_=MatchSession(content_,static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()));handOrder_={};newLog();refresh(true);}return;}
        if(hit(p,saveButton)){save();return;}if(hit(p,logButton)){showLog_=!showLog_;return;}if(showLog_)return;
        if(hit(p,sortType) || hit(p,sortCost)){handOrder_.sort(view_,hit(p,sortCost));offsets_[-1]=0;status_="手牌已整理";return;}
        if(inspectedCard()) {
            if(hit(p,detailClose)){interaction_.cancel();return;}
            if(hit(p,detailPanel))return;
        }
        if(interaction_.step()==ui::Step::Target || interaction_.step()==ui::Step::Cost) {
            if(hit(p,cancelButton)){interaction_.cancel();return;}
            for(auto it=cardHits_.rbegin();it!=cardHits_.rend();++it)if(hit(p,it->first)){if(!interaction_.pick(it->second))status_="请选择高亮卡牌";return;}return;
        }
        if(interaction_.pending() || interaction_.selected()) {
            if(interaction_.pending() && hit(p,cancelAction_)){interaction_.cancel();return;}
            if(interaction_.pending() && hit(p,confirm_)){submit();return;}
            for(const auto& h:groupHits_)if(hit(p,h.first)){interaction_.activate(h.second);return;}
            if(hit(p,actionArea_))return;
            interaction_.cancel();
        }
        if(hit(p,phaseButton)){if(auto a=phaseAction())interaction_.offer(*a);return;}
        if(hit(p,surrenderButton)){for(const auto& a:view_.actions)if(std::holds_alternative<Surrender>(a.command)){interaction_.offer(a);break;}return;}
        for(const auto& h:decisionHits_)if(hit(p,h.first)){interaction_.offer(h.second);return;}
        for(auto it=cardHits_.rbegin();it!=cardHits_.rend();++it)if(hit(p,it->first)){interaction_.select(it->second);groupOffset_=0;return;}
    }
    void scroll(sf::Vector2f p,int delta) {
        if(scene_!=Scene::Match || showLog_)return;
        if(interaction_.selected() && interaction_.step()==ui::Step::Inspect && hit(p,actionArea_)){groupOffset_=std::max(0,groupOffset_+delta);return;}
        if(!decisionHits_.empty() && hit(p,{{550,180},{500,420}})){offsets_[-2]=std::max(0,offsets_[-2]+delta);return;}
        for(const auto& a:scrollAreas_)if(hit(p,a.rect)){offsets_[a.key]=std::clamp(offsets_[a.key]+delta,0,std::max(0,static_cast<int>(a.ids.size())-a.page));return;}
    }
    sf::Vector2f pointFor(sf::RenderWindow& w,CardId id) {
        draw(w);for(const auto& h:cardHits_)if(h.second==id){return h.first.position+sf::Vector2f{6,6};}
        const auto* c=find(id);if(c && c->instance.zone==Zone::Attached) {
            auto host=c->instance.host;for(const auto& a:scrollAreas_) {auto at=std::find(a.ids.begin(),a.ids.end(),host);if(at!=a.ids.end())offsets_[a.key]=static_cast<int>(at-a.ids.begin());}
        } else for(const auto& a:scrollAreas_){auto at=std::find(a.ids.begin(),a.ids.end(),id);if(at!=a.ids.end())offsets_[a.key]=static_cast<int>(at-a.ids.begin());}
        draw(w);for(const auto& h:cardHits_)if(h.second==id){return h.first.position+sf::Vector2f{6,6};}
        throw std::runtime_error("UI card not visible: "+std::to_string(id));
    }
    void clickCard(sf::RenderWindow& w,CardId id){auto p=pointFor(w,id);press(p);releasePointer(p);}
    void gesture(sf::RenderWindow& w,CardId source,sf::Vector2f target) {
        auto origin=pointFor(w,source);press(origin);movePointer(origin+sf::Vector2f{12,-12});draw(w);movePointer(target);draw(w);releasePointer(target);++dragCount_;
    }
    void exerciseGestures(sf::RenderWindow& w) {
        auto before=session_.engine().digest();auto ids=handOrder_.cards(viewer_);if(ids.size()<2)return;
        auto last=pointFor(w,ids.back());gesture(w,ids.front(),last+sf::Vector2f{80,30});
        if(handOrder_.cards(viewer_).front()==ids.front())throw std::runtime_error("hand drag did not reorder");
        draw(w);click(sortCost.position+sf::Vector2f{10,10});click(sortType.position+sf::Vector2f{10,10});
        gesture(w,ids.front(),{30,90});
        auto origin=pointFor(w,ids.front());press(origin);movePointer(origin+sf::Vector2f{20,-70});resetPointer();
        if(dragging_ || interaction_.pending() || session_.engine().digest()!=before)throw std::runtime_error("gesture cancellation changed rules");
    }
    bool dragCommand(sf::RenderWindow& w,const LegalAction& action) {
        Zone destination=Zone::Deck;CardId target=ui::Interaction::target(action.command);
        if(std::holds_alternative<SetFormation>(action.command) || std::holds_alternative<StartAnalysis>(action.command) || std::holds_alternative<AttachSeal>(action.command))destination=Zone::Analysis;
        else if(std::holds_alternative<PrepareCast>(action.command))destination=Zone::Casting;
        else if(std::holds_alternative<PlayAction>(action.command))destination=Zone::Action;
        else if(std::holds_alternative<PreloadWord>(action.command))destination=Zone::Words;
        else if(std::holds_alternative<Choose>(action.command) && view_.decision)destination=(view_.decision->kind==DecisionKind::Discard || view_.decision->kind==DecisionKind::Overflow)?Zone::Ash:Zone::Casting;
        if(destination==Zone::Deck)return false;
        sf::Vector2f drop=destination==Zone::Analysis?sf::Vector2f{789,817}:destination==Zone::Words?sf::Vector2f{1320,646}:destination==Zone::Action?sf::Vector2f{1300,700}:destination==Zone::Ash?sf::Vector2f{1320,814}:sf::Vector2f{1300,540};
        if(target)drop=pointFor(w,target);
        auto before=session_.engine().digest();gesture(w,action.source,drop);
        if(interaction_.step()==ui::Step::Cost)gesture(w,ui::Interaction::costCard(action.command),{1320,814});
        if(before!=session_.engine().digest())throw std::runtime_error("drag paid before confirmation");
        return true;
    }
    void drive(sf::RenderWindow& w,const Json& row) {
        if(row.at("actor").get<int>()!=viewer_)throw std::runtime_error("UI viewer mismatch");
        auto it=std::find_if(view_.actions.begin(),view_.actions.end(),[&](const LegalAction& a){return encodeCommand(a.command)==row.at("command");});
        if(it==view_.actions.end())throw std::runtime_error("UI operation unavailable");LegalAction a=*it;
        if(a.source && smokeDragging_ && dragCommand(w,a)) {
            // Pointer press/move/release has selected the same command as the click path.
        } else if(a.source) {
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
        auto assets=std::filesystem::absolute(argv[0]).parent_path()/"assets";std::string screenshot,inspect;bool showcase=false,dragSmoke=false,previewDrag=false;std::uint32_t seed=42;Json smoke;int captureStep=3;
        for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--assets" && i+1<argc)assets=argv[++i];else if(arg=="--screenshot" && i+1<argc)screenshot=argv[++i];else if(arg=="--showcase")showcase=true;else if(arg=="--preview-drag")previewDrag=true;else if(arg=="--inspect-card" && i+1<argc)inspect=argv[++i];else if(arg=="--capture-step" && i+1<argc)captureStep=std::stoi(argv[++i]);else if(arg=="--seed" && i+1<argc)seed=static_cast<std::uint32_t>(std::stoul(argv[++i]));else if((arg=="--ui-smoke" || arg=="--drag-smoke") && i+1<argc){dragSmoke=arg=="--drag-smoke";smoke=readJson(argv[++i]);seed=smoke.at("seed").get<std::uint32_t>();}}
        Client client(assets,seed);client.run(screenshot,showcase,smoke,inspect,captureStep,dragSmoke,previewDrag);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
