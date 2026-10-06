#include "wizard/application.hpp"
#include "wizard/runtime.hpp"
#include "wizard/interaction.hpp"
#include "wizard/presentation.hpp"
#include "wizard/sound.hpp"
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
const Rect analysisArea{{260,560},{540,272}},castingArea{{260,487},{1080,65}},wordsArea{{809,560},{531,106}},actionZone{{809,674},{531,101}},ashArea{{809,783},{531,49}};
const Rect sortType{{25,865},{100,34}},sortCost{{135,865},{100,34}};
const Rect detailPanel{{1360,330},{215,550}},detailClose{{1542,340},{25,27}};
const Rect saveButton{{1278,24},{90,34}},logButton{{1380,24},{90,34}},surrenderButton{{1484,24},{90,34}},cancelButton{{1048,24},{98,34}};
const Rect settingsButton{{1158,24},{98,34}};
std::string spellName(SpellState s) {return std::array<const char*,5>{"","解析中","解析完成","待释放","持续生效"}.at(static_cast<std::size_t>(s));}
std::string kindName(CardType t) {return app::cardTypeLabel(t);}
sf::Color accent(CardType t) {return std::array<sf::Color,5>{sf::Color{216,156,90},sf::Color{109,171,219},sf::Color{175,147,224},sf::Color{113,196,166},sf::Color{218,197,122}}.at(static_cast<std::size_t>(t));}
MatchConfig configuration(const Content& content,std::uint32_t seed,const Json& recording){MatchConfig config;config.seed=seed;for(int p=0;p<2;++p){config.players[p].cards=content.deck;if(!recording.is_null()){config.players[p].baseFormation=recording.at("players").at(p).at("baseFormation").get<std::string>();config.players[p].cards=recording.at("players").at(p).at("cards").get<std::vector<std::string>>();}}return config;}
class Client {
public:
    Client(std::filesystem::path assets,std::uint32_t seed,const Json& recording,std::filesystem::path userDirectory,bool singleReplay=false):assets_(std::move(assets)),content_(loadContent(assets_)),app_(content_,std::move(userDirectory),app::loadPresets(content_,assets_)),resources_(assets_) {
        const auto manifest=readJson(assets_/"art"/"runtime.json");
        for(auto it=manifest.at("cards").begin();it!=manifest.at("cards").end();++it)art_[it.key()]=assets_/"art"/it.value().get<std::string>();
        const auto skin=readJson(assets_/"art"/"ui"/"manifest.json");
        for(const auto& item:skin.at("assets")) {
            SkinPart part{assets_/"art"/item.at("file").get<std::string>(),{}};
            if(item.contains("insets"))part.insets=item.at("insets").get<std::array<int,4>>();
            skin_.emplace(item.at("id").get<std::string>(),std::move(part));
        }
        seedText_=std::to_string(seed);status_=app_.notice();
        resources_.setSoundVolume(app_.settings().effectiveSoundVolume());
        if(!recording.is_null()) {std::string error;if(!app_.start(configuration(content_,seed,recording),error,{singleReplay?app::MatchMode::Ai:app::MatchMode::Hotseat,ai::Difficulty::Normal}))throw std::runtime_error(error);refresh(true);}
    }
    void run(const std::string& screenshot,bool showcase,const Json& smoke,const std::string& inspect,int captureStep,bool dragSmoke,bool previewDrag,bool previewDropChoice,bool manualPhases,bool menuSmoke,bool deckSmoke,bool aiSmoke,const std::string& capturePage,float animationTime,const std::string& animationFrames) {
        smokeDragging_=dragSmoke;
        smokeDriver_=!smoke.is_null() || menuSmoke || deckSmoke || aiSmoke;
        if(!smoke.is_null())(void)replay(content_,smoke);
        sf::RenderWindow window;applyWindow(window);sf::Clock clock;
        if(showcase && !app_.hasMatch()) {std::string error;auto config=configuration(content_,static_cast<std::uint32_t>(std::stoul(seedText_)),Json{});if(!app_.start(config,error))throw std::runtime_error(error);refresh(true);}
        if(showcase)scene_=Scene::Match;
        if(capturePage=="settings")openSettings();
        else if(capturePage=="setup")app_.navigate(app::Page::HotseatSetup);
        else if(capturePage=="ai")app_.navigate(app::Page::AiSetup);
        else if(capturePage=="decks")app_.navigate(app::Page::Decks);
        else if(capturePage=="editor"){importDeck();inspectedDefinition_=inspect.empty()?"fireball":inspect;}
        else if(capturePage=="tutorial")startTutorial();
        else if(capturePage=="ai-match"){app_.navigate(app::Page::AiSetup);startConfigured();}
        bool menuExercised=false;
        std::size_t smokeStep=0;int frames=0;bool exercised=false;
        while(window.isOpen()) {
            while(const auto event=window.pollEvent()) {
                if(event->is<sf::Event::Closed>()){resetPointer();if(app_.page()==app::Page::DeckEditor || app_.page()==app::Page::ConfirmDeckDiscard)requestDeckLeave(true);else app_.requestLeave(true);}
                if(event->is<sf::Event::Resized>())viewport(window);
                if(const auto* e=event->getIf<sf::Event::TextEntered>()){seedInput(e->unicode);deckTextInput(e->unicode);}
                if(const auto* e=event->getIf<sf::Event::MouseButtonPressed>()) {
                    if(e->button==sf::Mouse::Button::Left)press(window.mapPixelToCoords(e->position));
                    if(e->button==sf::Mouse::Button::Right)rightClick(window.mapPixelToCoords(e->position));
                }
                if(const auto* e=event->getIf<sf::Event::MouseMoved>())movePointer(window.mapPixelToCoords(e->position));
                if(const auto* e=event->getIf<sf::Event::MouseButtonReleased>())if(e->button==sf::Mouse::Button::Left)releasePointer(window.mapPixelToCoords(e->position));
                if(event->is<sf::Event::FocusLost>() || event->is<sf::Event::MouseLeft>()){resetPointer();pointer_={-100,-100};}
                if(const auto* e=event->getIf<sf::Event::MouseWheelScrolled>())scroll(window.mapPixelToCoords(e->position),e->delta<0?1:-1);
                if(const auto* e=event->getIf<sf::Event::KeyPressed>()) {
                    if(deckKey(*e))continue;
                    if(e->code==sf::Keyboard::Key::Escape)escaped();
                    if(e->code==sf::Keyboard::Key::F5 && app_.hasMatch())save();
                }
            }
            if(app_.quitRequested()){window.close();break;}
            if(windowChange_)applyWindow(window);
            if((menuSmoke || deckSmoke || aiSmoke) && !menuExercised){if(aiSmoke)exerciseAi(window);else if(deckSmoke)exerciseDecks(window);else exerciseMenu(window);menuExercised=true;if(screenshot.empty()){window.close();break;}}
            if(!window.isOpen())break;
            if(smoke.is_null() && screenshot.empty() && !presentation_.busy() && !app_.paused() && scene_==Scene::Match && !showLog_ && !mouseDown_ && !interaction_.selected() && !interaction_.pending())if(auto advance=ui::automaticAdvance(view_,app_.settings().automaticPhases && !manualPhases && app_.mode()!=app::MatchMode::Tutorial)){
                auto result=app_.submit(viewer_,*advance,app_.generation());if(!result.accepted)throw std::runtime_error(result.error);refresh();save();
            }
            float dt=std::min(.1f,clock.restart().asSeconds());
            updatePresentation(dt);
            if(smoke.is_null() && screenshot.empty())tickAi(dt);updateDrag(dt);draw(window);window.display();
            bool capture=!screenshot.empty() && (smoke.is_null()?++frames>=3:smokeStep>=static_cast<std::size_t>(captureStep));
            if(capture) {
                if(animationTime>=0)presentation_.sampleLatest(animationTime);
                if(showcase && app_.hasMatch())scene_=view_.result==-1?Scene::Match:Scene::Result;
                if(app_.page()==app::Page::Match && scene_==Scene::Match && !inspect.empty())for(const auto& c:view_.cards)if(c.definition.id==inspect){interaction_.select(c.instance.id);break;}
                if((previewDrag || previewDropChoice) && app_.page()==app::Page::Match && scene_==Scene::Match && interaction_.selected()) {
                    auto id=interaction_.selected();auto origin=pointFor(window,id);sf::Vector2f destination{789,817};
                    for(const auto& a:view_.actions)if(a.source==id && ui::Interaction::target(a.command)){destination=pointFor(window,ui::Interaction::target(a.command));break;}
                    if(previewDropChoice)destination={789,817};
                    press(origin);movePointer(destination);if(previewDropChoice)releasePointer(destination);
                }
                if(!animationFrames.empty()) {
                    std::filesystem::create_directories(animationFrames);
                    for(int frame=0;frame<20;++frame){
                        presentation_.sampleLatest(frame*.06f);draw(window);sf::Texture sample;
                        if(!sample.resize(window.getSize()))throw std::runtime_error("animation capture resize failed");sample.update(window);
                        if(!sample.copyToImage().saveToFile(std::filesystem::path(animationFrames)/("frame-"+std::to_string(100+frame)+".png")))throw std::runtime_error("animation frame write failed");
                    }
                    presentation_.sampleLatest(animationTime>=0?animationTime:.18f);
                }
                draw(window);sf::Texture texture;
                if(!texture.resize(window.getSize()))throw std::runtime_error("capture resize failed");texture.update(window);
                if(!texture.copyToImage().saveToFile(screenshot))throw std::runtime_error("capture write failed");window.close();break;
            }
            if(!smoke.is_null()) {
                if(scene_==Scene::Handoff)click(handoffButton.position+sf::Vector2f{20,20});
                else if(smokeStep<smoke.at("commands").size()) {
                    if(smokeDragging_ && !exercised){exerciseGestures(window);exercised=true;}
                    const auto &row=smoke.at("commands")[smokeStep++];
                    if(app_.mode()!=app::MatchMode::Hotseat && row.at("actor").get<int>()==1){auto result=app_.submit(1,decodeCommand(row.at("command")),app_.generation());if(!result.accepted)throw std::runtime_error(result.error);refresh();if(app_.match().engine().digest()!=row.at("digest").get<std::string>())throw std::runtime_error("AI replay state mismatch");}
                    else drive(window,row);
                }
                else {std::cout<<"UI replay passed: "<<smokeStep<<" contextual actions, "<<dragCount_<<" drag gestures, digest "<<app_.match().engine().digest()<<'\n';window.close();}
            }
        }
    }
    void audioSmoke() {
        std::set<std::string> files;Json audit=Json::array();
        for(int n=0;n<18;++n){auto id=static_cast<ui::SoundId>(n);auto name=ui::soundFile(id);if(!files.insert(name).second)continue;
            float seconds;unsigned rate,channels;
            if(!resources_.inspectSound(assets_/"audio"/name,seconds,rate,channels))throw std::runtime_error(std::string("audio decode failed: ")+name);
            audit.push_back({{"file",name},{"seconds",seconds},{"sampleRate",rate},{"channels",channels}});
        }
        resources_.setSoundVolume(64);
        for(const auto& name:files)resources_.play(assets_/"audio"/name,.2f,0,0);
        if(resources_.activeSounds()>6)throw std::runtime_error("unbounded sound voices");
        resources_.stopSounds();
        if(!resources_.play(assets_/"audio"/"confirm.mp3",.2f,0,10))throw std::runtime_error("audio playback failed");
        if(resources_.play(assets_/"audio"/"confirm.mp3",.2f,0,10))throw std::runtime_error("audio cooldown failed");
        if(!resources_.play(assets_/"audio"/"confirm.mp3",.2f,3,10))throw std::runtime_error("result fallback was suppressed by confirmation cooldown");
        resources_.play(assets_/"audio"/"victory.mp3",.2f,3,0);
        if(resources_.activeSounds()!=1)throw std::runtime_error("terminal cue did not replace previous voices");
        resources_.setSoundVolume(0);
        if(resources_.activeSounds() || resources_.play(assets_/"audio"/"damage.mp3"))throw std::runtime_error("audio mute failed");
        resources_.setSoundVolume(64);
        writeJson(app_.userDirectory()/"invalid.mp3",Json{{"test","invalid audio"}});
        float badSeconds;unsigned badRate,badChannels;
        if(resources_.inspectSound(app_.userDirectory()/"invalid.mp3",badSeconds,badRate,badChannels))throw std::runtime_error("corrupt audio did not fall back");
        if(resources_.play(assets_/"audio"/"missing.mp3"))throw std::runtime_error("missing audio did not fall back");
        resources_.stopSounds();resources_.setSoundVolume(app_.settings().effectiveSoundVolume());
        writeJson(app_.userDirectory()/"audio-audit.json",audit);
        std::cout<<"Audio smoke passed: "<<files.size()<<" decoded MP3 files, 6-voice limit, cooldown, terminal priority, mute, missing/corrupt-file fallback\n";
    }
private:
    struct SkinPart {std::filesystem::path path;std::array<int,4> insets;};
    struct ScrollArea {Rect rect;int key;std::vector<CardId> ids;int page;};
    std::filesystem::path assets_;Content content_;app::Application app_;Resources resources_;
    ui::MatchPresentation presentation_;float visualClock_{};bool smokeDriver_{};
    std::vector<Rect> uiButtonHits_;
    void cue(ui::SoundId id){resources_.play(assets_/"audio"/ui::soundFile(id),ui::soundGain(id),ui::soundPriority(id),ui::soundCooldown(id));}
    std::map<std::uint64_t,Rect> motionOrigins_;
    std::map<std::string,SkinPart> skin_;std::set<std::string> missingArt_;
    ui::HandOrder handOrder_;sf::Vector2f pointer_{-100,-100},pressPoint_;bool mouseDown_{},dragging_{};CardId pressedCard_{};float edgeScroll_{};
    GameView view_;ui::Interaction interaction_;PlayerId viewer_{};Scene scene_{Scene::Handoff};
    std::map<std::string,std::filesystem::path> art_;std::map<int,int> offsets_;
    std::string status_;bool smokeDragging_{};int dragCount_{};bool showLog_{};int groupOffset_{};Rect actionArea_,confirm_,cancelAction_;
    std::vector<std::pair<Rect,CardId>> cardHits_,formationHits_;std::vector<std::pair<Rect,std::size_t>> groupHits_;
    std::vector<std::pair<Rect,LinkId>> linkHits_;
    std::vector<std::pair<Rect,LegalAction>> decisionHits_;std::vector<ScrollArea> scrollAreas_;
#include "pages.inc"
#include "match_effects.inc"
    void refresh(bool initial=false) {
        const auto& s=app_.match().engine().state();int next=app_.mode()==app::MatchMode::Hotseat?(s.decision?s.decision->player:s.active):0;
        bool handoff=initial || next!=viewer_;auto previous=view_;viewer_=next;view_=app_.match().engine().viewFor(viewer_);
        if(initial){resources_.stopSounds();if(app_.mode()!=app::MatchMode::Hotseat)cue(ui::SoundId::TurnReady);}
        else {
            // Consume the old viewer's filtered transition before switching a hot-seat screen.
            auto audible=app_.match().engine().viewFor(previous.viewer);
            if(handoff)resources_.stopSounds();
            if(app_.mode()==app::MatchMode::Hotseat && previous.result==-1 && view_.result!=-1)
                cue(view_.result==2?ui::SoundId::DrawResult:view_.result==viewer_?ui::SoundId::Victory:ui::SoundId::Defeat);
            else for(auto id:ui::matchSounds(previous,audible,!handoff && !app_.aiTurn()))cue(id);
        }
        if(handoff){presentation_.clear();motionOrigins_.clear();}
        else {presentation_.observe(previous,view_,app_.settings().reducedMotion);captureMotionOrigins(previous);}
        interaction_.update(view_);handOrder_.sync(view_);resetPointer();
        scene_=s.result!=-1?Scene::Result:app_.mode()==app::MatchMode::Hotseat && handoff?Scene::Handoff:Scene::Match;showLog_=false;groupOffset_=0;
    }
    void save(){std::string error;if(app_.saveReplay(error))status_="复盘已保存到用户数据目录。";else status_=error;}
    const CardView* find(CardId id)const{for(const auto& c:view_.cards)if(c.instance.id==id)return &c;return nullptr;}
    void label(sf::RenderTarget& t,const std::string& s,float x,float y,unsigned size=17,sf::Color color=ink){text(t,resources_,s,{x,y},size,color);}
    void frame(sf::RenderTarget& t,Rect r,sf::Color color=gold){box(t,r,panel,color);box(t,{r.position,{17,2}},color);box(t,{r.position+r.size-sf::Vector2f{17,2},{17,2}},color);}
    void paint(sf::RenderTarget& t,const std::string& id,Rect r,std::uint8_t alpha=255) {
        const auto& part=skin_.at(id);texturePatch(t,resources_.texture(part.path),r,part.insets,{255,255,255,alpha});
    }
    static std::string typeKey(CardType type){return std::array<const char*,5>{"action","analytic","word","formation","seal"}.at(static_cast<std::size_t>(type));}
    static std::string zoneKey(Zone zone){return std::array<const char*,9>{"deck","hand","action","analysis","words","casting","ash","analysis","action"}.at(static_cast<std::size_t>(zone));}
    void button(sf::RenderTarget& t,Rect r,const std::string& s,bool primary=false,bool enabled=true,bool centered=false) {
        if(enabled)uiButtonHits_.push_back(r);
        bool hover=hit(pointer_,r) && !dragging_;std::string state=!enabled?"disabled":hover?(mouseDown_?"pressed":"hover"):"normal";
        std::string type=s=="投降"?"danger":primary?"primary":"secondary";
        paint(t,"button_"+type+"_"+state,r);
        fittedText(t,resources_,s,{r.position+sf::Vector2f{10,5},{r.size.x-20,r.size.y-10}},centered?20:17,enabled?ink:muted,centered);
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
    static Rect side(Rect r,bool enemy){return enemy?mirror(r):r;}
    void illustration(sf::RenderTarget& t,const CardView& c,Rect r,std::uint8_t alpha=255) {
        auto i=art_.find(c.definition.id);
        if(i!=art_.end() && !missingArt_.count(c.definition.id)) {
            try {
            const auto& tex=resources_.texture(i->second);sf::Sprite sprite(tex);auto size=tex.getSize();
            float scale=std::min(r.size.x/static_cast<float>(size.x),r.size.y/static_cast<float>(size.y));sprite.setScale({scale,scale});
            sprite.setPosition(r.position+(r.size-sf::Vector2f{static_cast<float>(size.x)*scale,static_cast<float>(size.y)*scale})/2.f);sprite.setColor({255,255,255,alpha});t.draw(sprite);return;
            }catch(const std::exception&){missingArt_.insert(c.definition.id);}
        }
        {
            float radius=std::min(r.size.x,r.size.y)*0.35f;sf::CircleShape sigil(radius,6);sigil.setOrigin({radius,radius});sigil.setPosition(r.position+r.size/2.f);sigil.setFillColor({25,43,54,alpha});auto tone=accent(c.definition.type);tone.a=alpha;sigil.setOutlineColor(tone);sigil.setOutlineThickness(1);t.draw(sigil);
            auto name=utf8(c.definition.name);resources_.drawText(t,name.substring(0,1),r.position+r.size/2.f-sf::Vector2f{12,17},24,tone);
        }
    }
    void card(sf::RenderTarget& t,const CardView& c,Rect r,bool registerHit=true) {
        const auto& d=c.definition;const auto& i=c.instance;
        std::string size=r.size.y<80?"strip":i.zone==Zone::Hand?"hand":"portrait";
        if(i.faceDown){
            paint(t,"card_back_deck",r);
            label(t,c.hidden?"埋伏卡":d.name,r.position.x+5,r.position.y+4,11,c.hidden?muted:ink);
            if(interaction_.selected()==i.id)box(t,r,sf::Color::Transparent,mint);
            const auto candidates=interaction_.candidates();if(std::find(candidates.begin(),candidates.end(),i.id)!=candidates.end())box(t,r,sf::Color::Transparent,gold);
            if(registerHit)cardHits_.push_back({r,i.id});return;
        }
        paint(t,"frame_"+typeKey(d.type)+"_"+size,r);
        if(r.size.y<34) {
            illustration(t,c,{r.position+sf::Vector2f{3,2},{14,14}});
            label(t,d.name,r.position.x+22,r.position.y+1,11);
        } else if(r.size.y<80) {
            illustration(t,c,{r.position+sf::Vector2f{3,7},{26,r.size.y-13}});
            fittedText(t,resources_,d.name,{{r.position.x+34,r.position.y+4},{r.size.x-40,17}},r.size.x<110?11:12,ink);
            std::string sub=d.type==CardType::Formation?std::to_string(c.occupiedRings)+"/"+std::to_string(c.effectiveRings)+" 环":spellName(i.spell);
            if(i.spell==SpellState::Analyzing)sub="解析 · "+std::to_string(c.turnsToReady);
            label(t,sub,r.position.x+34,r.position.y+23,11,muted);
        } else {
            fittedText(t,resources_,d.name,{{r.position.x+8,r.position.y+4},{r.size.x-(i.base || d.opponentDestroyProtected || i.spell!=SpellState::None?32.f:16.f),18}},12,ink);
            illustration(t,c,{r.position+sf::Vector2f{5,24},{r.size.x-10,r.size.y-56}});
            if(d.type==CardType::Analytic || d.type==CardType::Word)label(t,std::to_string(d.rank),r.position.x+r.size.x-15,r.position.y+24,11,gold);
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
            paint(t,state,{{r.position.x+r.size.x-16,r.position.y+(r.size.y<34?2.f:r.size.y<80?23.f:4.f)},{13,13}});
        }
        if(i.base || d.opponentDestroyProtected)paint(t,"state_protected",{{r.position.x+r.size.x-16,r.position.y+4},{13,13}});
        if(registerHit){cardHits_.push_back({r,i.id});drawCardMotionState(t,c,r);}
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
        region(t,p,Zone::Casting,side(castingArea,enemy),"施 法 区");
        region(t,p,Zone::Words,side(wordsArea,enemy),"后 场 · 言 灵 / 埋 伏");
        region(t,p,Zone::Action,side(actionZone,enemy),"行 动 区");
        region(t,p,Zone::Ash,side(ashArea,enemy),"灰 烬 区");
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
        std::string name=(enemy?"对手 · 玩家 ":"本方 · 玩家 ")+std::to_string(p+1);
        if(app_.mode()!=app::MatchMode::Hotseat)name=enemy?(app_.mode()==app::MatchMode::Tutorial?"教学对手":"AI · "+ai::difficultyLabel(app_.difficulty())):"本方 · 你";
        label(t,name,r.position.x+14,r.position.y+10,17,gold);
        label(t,std::to_string(v.life),r.position.x+52,r.position.y+39, 40,ink);paint(t,"life",{{r.position.x+12,r.position.y+51},{30,30}});
        label(t,"/"+std::to_string(maximumLife),r.position.x+94,r.position.y+63,12,muted);
        label(t,"基础容量 +"+std::to_string(content_.catalog.baseLoadCapacity),r.position.x+115,r.position.y+52,11,muted);
        std::string defense=v.temporaryLife?"临时生命 "+std::to_string(v.temporaryLife):"";
        if(v.blockedActionTurn==v.ownTurn)defense+=" 行动封锁";
        for(auto resistance:v.resistances)defense+=" "+damageName(resistance)+"抗性";
        if(!defense.empty())fittedText(t,resources_,defense,{{r.position.x+14,r.position.y+80},{187,14}},11,mint);
        paint(t,"mana",{{r.position.x+13,r.position.y+109},{22,22}});label(t,"魔素 "+std::to_string(v.mana)+" / 12",r.position.x+43,r.position.y+105,18,mint);
        paint(t,"load",{{r.position.x+13,r.position.y+141},{22,22}});label(t,"荷载 "+std::to_string(v.load)+" / "+std::to_string(v.capacity),r.position.x+43,r.position.y+137,18,ink);
        int independent=0;for(const auto& load:view_.temporary)if(load.owner==p && load.independent)independent+=load.amount;
        label(t,"手 "+std::to_string(v.handCount)+" · 牌 "+std::to_string(v.deckCount)+" · 独立 "+std::to_string(independent),r.position.x+14,r.position.y+168,12,muted);
        box(t,{{r.position.x+14,r.position.y+91},{187,3}},{35,52,64});
        box(t,{{r.position.x+14,r.position.y+91},{187*std::clamp(v.life/static_cast<float>(maximumLife),0.f,1.f),3}},v.life<=7?sf::Color{218,100,108}:mint);
        const float pressure=v.capacity>0?std::clamp(v.load/static_cast<float>(v.capacity),0.f,1.f):1.f;
        box(t,{{r.position.x+43,r.position.y+161},{150,3}},{35,52,64});
        box(t,{{r.position.x+43,r.position.y+161},{150*pressure,3}},pressure>=.85f?sf::Color{237,138,120}:gold);
        Rect deck=enemy?Rect({107,135},{100,132}):Rect({1388,696},{100,132});paint(t,"card_back_deck",deck);label(t,"牌 库",deck.position.x+23,deck.position.y+12,17,gold);label(t,std::to_string(v.deckCount),deck.position.x+35,deck.position.y+98,24,ink);
    }
    std::optional<LegalAction> phaseAction()const {
        for(const auto& a:view_.actions)if(std::holds_alternative<Advance>(a.command) || std::holds_alternative<AdvancePhase>(a.command) || std::holds_alternative<PassResponse>(a.command))return a;
        for(const auto& a:view_.actions)if(auto choice=std::get_if<Choose>(&a.command))if(!choice->option)return a;
        return {};
    }
    const CardView* inspectedCard()const {
        const auto* c=find(interaction_.selected());
        return c?c:interaction_.pending()?find(interaction_.pending()->source):nullptr;
    }
    void details(sf::RenderTarget& t,const CardView& c) {
        const auto& d=c.definition;const auto& i=c.instance;
        paint(t,"frame_"+typeKey(d.type)+"_detail",detailPanel);fittedText(t,resources_,d.name,{{1372,341},{166,25}},20,gold);
        box(t,detailClose,panel,gold);label(t,"×",1547,342,17,ink);
        label(t,kindName(d.type)+(i.base?" · 基础阵法":""),1372,371,13,muted);
        if(c.hidden){label(t,"对方的埋伏卡",1372,406,17,muted);label(t,"反转前信息隐藏",1372,441,13,muted);return;}
        illustration(t,c,{{1374,393},{187,83}});
        std::string stats=app::primaryCostLabel(d.type)+" "+std::to_string(d.cost);
        if(d.type==CardType::Analytic)stats+=" / 施法 "+std::to_string(d.castCost);
        if(d.type==CardType::Analytic || d.type==CardType::Word)stats+=" · 位阶 "+std::to_string(d.rank);
        fittedText(t,resources_,stats,{{1372,484},{191,35}},12,mint,false,true);
        float effectY=555;
        if(app::hasSpellRank(d.type))label(t,schoolName(d.school)+" · "+std::to_string(d.speed)+"速",1372,523,12,gold);
        if(d.type==CardType::Formation) {
            label(t,"阵体 "+std::to_string(d.body)+" / 荷载上限 "+std::to_string(d.capacity),1372,504,13,mint);
            label(t,"魔素收入 "+std::to_string(d.income)+" / 环位数 "+std::to_string(d.rings),1372,524,13,mint);
            label(t,"承载位阶上限 "+std::to_string(d.maxRank),1372,544,13,mint);
            effectY=570;
        } else if(d.concentration) {
            label(t,"持续类型：专注",1372,540,11,mint); effectY=562;
        }
        fittedText(t,resources_,d.text,{{1372,effectY},{191,685-effectY}},13,ink,false,true);
        if(!d.flavor.empty())fittedText(t,resources_,d.flavor,{{1372,699},{191,106}},11,muted,false,true);
        label(t,"绑定荷载 "+std::to_string(i.analysisLoad+i.castLoad),1372,816,13,muted);
        if(i.spell==SpellState::Analyzing)label(t,"解析剩余 "+std::to_string(c.turnsToReady)+" 回合",1372,835,13,muted);
        else if(i.spell==SpellState::Active)label(t,d.concentration?"专注中 · 维持费用 "+std::to_string(i.concentrationCost):"持续剩余 "+std::to_string(i.remaining)+" 回合",1372,835,13,muted);
        else if(i.spell!=SpellState::None)label(t,spellName(i.spell),1372,835,13,muted);
        else if(d.type==CardType::Formation && i.zone==Zone::Analysis)label(t,"环位 "+std::to_string(c.occupiedRings)+" / "+std::to_string(c.effectiveRings),1372,835,13,muted);
        if(interaction_.pending()) {
            const auto& command=interaction_.pending()->command;int cost=0,load=0;
            if(std::holds_alternative<PrepareCast>(command) || std::holds_alternative<ActivateSpell>(command))cost=load=c.effectiveCastCost-(d.type==CardType::Word?d.cost:0);
            else if(auto analysis=std::get_if<StartAnalysis>(&command)){
                const auto* host=find(analysis->formation);cost=load=std::max(0,d.cost-(host && host->instance.base && host->definition.baseAnalysisDiscount && (d.school=="evocation" || d.school=="conjuration")?1:0));
            }
            else if(std::holds_alternative<PreloadWord>(command))cost=load=d.cost;
            else if(std::holds_alternative<PlayAction>(command)){cost=d.cost;load=d.burden;}
            else if(std::holds_alternative<AttachSeal>(command))cost=d.cost;
            else if(std::holds_alternative<Choose>(command) && view_.decision && view_.decision->kind==DecisionKind::Concentration)cost=load=i.concentrationCost;
            else if(std::holds_alternative<RemoveFormation>(command))cost=2;
            else if(auto set=std::get_if<SetFormation>(&command))cost=d.cost+(set->replace?2:0);
            else if(std::holds_alternative<SetAmbush>(command))cost=1;
            else if(std::holds_alternative<FlipAmbush>(command))cost=d.cost;
            label(t,"本次魔素 "+std::to_string(cost)+" / 荷载 +"+std::to_string(load),1372,857,13,mint);
        } else if(interaction_.step()==ui::Step::Inspect && interaction_.groups().empty())label(t,"当前没有可执行操作",1372,857,13,muted);
    }
    void formationChooser(sf::RenderTarget& t) {
        auto ids=interaction_.candidates();
        paint(t,"decision",{{520,240},{560,100+64.f*static_cast<float>(ids.size())}});
        label(t,"选择承载阵法",544,255,24,mint);
        label(t,"仅显示可承载此法术的阵法 · Esc 取消",544,288,14,muted);
        for(std::size_t n=0;n<ids.size();++n)if(const auto* c=find(ids[n])) {
            Rect row{{544,318+64.f*static_cast<float>(n)},{512,54}};
            button(t,row,c->definition.name+" #"+std::to_string(ids[n]),true);
            label(t,"空余环位 "+std::to_string(c->effectiveRings-c->occupiedRings)+" / "+std::to_string(c->effectiveRings),row.position.x+12,row.position.y+31,13,muted);
            formationHits_.push_back({row,ids[n]});
        }
    }
    void context(sf::RenderTarget& t) {
        actionArea_={};confirm_={};cancelAction_={};
        if(!interaction_.selected() && !interaction_.pending())return;
        const auto* c=inspectedCard();if(c)details(t,*c);
        if(interaction_.step()==ui::Step::LinkTarget){auto links=interaction_.linkCandidates();auto& offset=offsets_[-4];offset=std::clamp(offset,0,std::max(0,static_cast<int>(links.size())-8));Rect modal{{500,180},{610,70+43.f*static_cast<float>(std::min(8,static_cast<int>(links.size())))}};paint(t,"decision",modal);label(t,"选择要响应的链节 · 滚轮浏览",515,193,20,mint);for(int n=0;n<8 && n+offset<static_cast<int>(links.size());++n){auto id=links[static_cast<std::size_t>(n+offset)];std::string name="链节 #"+std::to_string(id);if(view_.chain)for(const auto& link:view_.chain->links)if(link.id==id){auto source=find(link.item.source);name+=" · "+linkName(link.kind)+(source?" · "+source->definition.name:"");}Rect row{{515,230+43.f*static_cast<float>(n)},{580,37}};button(t,row,name);linkHits_.push_back({row,id});}button(t,cancelButton,"取消选择");return;}
        if(interaction_.step()==ui::Step::Target || interaction_.step()==ui::Step::Cost){if(interaction_.choosingFormation())formationChooser(t);button(t,cancelButton,"取消选择");return;}
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
        t.clear({10,17,25});uiButtonHits_.clear();cardHits_.clear();formationHits_.clear();linkHits_.clear();groupHits_.clear();decisionHits_.clear();scrollAreas_.clear();
        if(app_.page()==app::Page::Menu){drawPage(t);return;}
        if(isDeckPage()){drawDecks(t);return;}
        backdrop(t);
        if(app_.page()!=app::Page::Match){drawPage(t);return;}
        label(t,"巫 师 牌",56,35,28,gold);label(t,std::string(programVersion)+" / 规则 "+content_.catalog.rulesVersion,30,78,12,muted);
        std::string prompt="第 "+std::to_string(view_.players[view_.active].ownTurn)+" 回合  /  "+phaseName(view_.phase)+"阶段";
        if(interaction_.step()==ui::Step::LinkTarget)prompt="选择要响应的链节";
        else if(interaction_.step()==ui::Step::Target)prompt=interaction_.choosingFormation()?"请选择承载阵法":"选择高亮的目标卡牌";
        else if(interaction_.step()==ui::Step::Cost)prompt="选择高亮手牌，支付弃置成本";
        else if(view_.decision)prompt+=std::string("  ·  ")+std::array<const char*,12>{"选择响应能力，或放弃响应","选择待释放法术，或本阶段不再释放","选择被动触发顺序","选择要弃置的手牌","选择要移除的临时荷载","选择要移除的多余法术","可销毁高亮的空白阵法，或跳过","选择高亮手牌，完成效果弃置","可将高亮的就绪法术准备施法，或跳过","支付费用维持专注，或销毁法术","可销毁对方埋伏卡，或跳过","选择要检索的行动卡"}.at(static_cast<std::size_t>(view_.decision->kind));
        if(app_.aiTurn())prompt=app_.mode()==app::MatchMode::Tutorial?"教学对手正在行动…":"AI（"+ai::difficultyLabel(app_.difficulty())+"）正在思考…";
        fittedText(t,resources_,prompt,{{320,29},{815,27}},19,mint);
        board(t,1-viewer_,true);board(t,viewer_,false);hud(t,1-viewer_,true);hud(t,viewer_,false);
        if(view_.chain){const auto& chain=*view_.chain;frame(t,{{25,310},{215,340}});label(t,"连锁 #"+std::to_string(chain.id),37,320,17,mint);label(t,windowName(chain.window),37,350,14,gold);label(t,chain.mode==ChainMode::Building?"玩家 "+std::to_string(chain.priority+1)+" 响应":"逆序结算中",37,374,14,ink);
            auto& offset=offsets_[-3];offset=std::clamp(offset,0,std::max(0,static_cast<int>(chain.links.size())-6));int row=0;for(auto it=chain.links.rbegin()+offset;it!=chain.links.rend() && row<6;++it,++row){auto y=408+37.f*static_cast<float>(row);const auto* source=find(it->item.source);label(t,"#"+std::to_string(it->id)+" "+linkName(it->kind)+(it->speed?" · "+std::to_string(it->speed)+"速":"")+(it->canceled?" · 已取消":""),37,y,12,gold);fittedText(t,resources_,(source?source->definition.name:"阶段标记")+(it->item.targetLink?" → #"+std::to_string(it->item.targetLink):""),{{37,y+16},{189,16}},12,ink);if(it->item.source)cardHits_.push_back({{{30,y},{205,35}},it->item.source});}
            if(chain.links.size()>6)label(t,"滚轮查看较早链节",37,628,11,muted);
        }
        box(t,{{260,479},{1080,2}},presentation_.busy()?mint:gold);box(t,{{670,465},{260,29}},panel,gold);label(t,"奥 术 对 决",747,468,15,gold);
        drawMatchGuide(t);
        int backs=std::min(12,view_.players[1-viewer_].handCount);for(int i=0;i<backs;++i){Rect r{{1340-static_cast<float>(backs)*38+38.f*static_cast<float>(i),75},{30,34}};paint(t,"card_back_opponent",r);}
        paint(t,"region_hand",handArea);label(t,"手 牌",25,917,19,gold);
        button(t,settingsButton,"设置");
        if(scene_==Scene::Handoff) {
            paint(t,"handoff",handArea);
            label(t,"请将操作交给玩家 "+std::to_string(viewer_+1)+" · 手牌已遮挡",555,875,21,ink);button(t,handoffButton,"确认接手 · 显示自己的手牌",true);return;
        }
        std::vector<const CardView*> hand;for(auto id:handOrder_.cards(viewer_))if(auto c=find(id))hand.push_back(c);
        strip(t,{{270,871},{1060,122}},hand,-1,101,118);
        button(t,sortType,"按类型");button(t,sortCost,"按费用");
        label(t,"拖动排序 · 拖至场地使用",25,953,12,muted);
        button(t,saveButton,"保存");button(t,logButton,"记录");button(t,surrenderButton,"投降");
        auto action=phaseAction();button(t,phaseButton,action?action->label:"请完成场内选择",action.has_value(),action.has_value());
        wrapped(t,resources_,status_,{1360,951},12,muted,17);
        drawMatchEffects(t);
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
            for(auto it=view_.events.rbegin();it!=view_.events.rend() && row<13;++it,++row)fittedText(t,resources_,it->text,{{395,235+40.f*static_cast<float>(row)},{800,35}},16,ink,false,true);
        }
        drawTutorial(t);
        if(scene_==Scene::Result) {
            paint(t,"result_window",{{520,327},{560,305}});label(t,view_.result==2?"对局结束 · 平局":"玩家 "+std::to_string(view_.result+1)+" 获胜",558,355,32,gold);
            std::string reasons;for(const auto& e:view_.events)if(e.kind=="defeat_reason")reasons+=e.text+"\n";
            wrapped(t,resources_,reasons,{558,414},18,ink,25);button(t,resultRestart_,"再来一局",true);button(t,resultMenu_,"返回主菜单");
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
        if(hit(p,castingArea))return Zone::Casting;
        if(hit(p,analysisArea))return Zone::Analysis;
        if(hit(p,wordsArea))return Zone::Words;
        if(hit(p,actionZone))return Zone::Action;
        if(hit(p,ashArea))return Zone::Ash;
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
    void resetPointer(){resetDeckPointer();mouseDown_=false;dragging_=false;pressedCard_=0;edgeScroll_=0;}
    void press(sf::Vector2f p) {
        resetPointer();pointer_=pressPoint_=p;mouseDown_=true;
        if(app_.page()==app::Page::DeckEditor){pressDeck(p);return;}
        if(app_.page()!=app::Page::Match || scene_!=Scene::Match || showLog_ || interaction_.choosingFormation() || hit(p,detailPanel) || hit(p,actionArea_))return;
        auto id=cardAt(p);const auto* c=find(id);if(!c || c->instance.owner!=viewer_)return;
        if(interaction_.step()==ui::Step::Confirm)return;
        if(interaction_.step()==ui::Step::Target && interaction_.selected()!=id)return;
        if(c->instance.zone==Zone::Hand || interaction_.selected()==id || std::any_of(view_.actions.begin(),view_.actions.end(),[&](const LegalAction& a){return a.source==id;}))pressedCard_=id;
    }
    void movePointer(sf::Vector2f p) {
        pointer_=p;auto delta=p-pressPoint_;
        if(app_.page()==app::Page::DeckEditor){moveDeckPointer(p);return;}
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
        if(!mouseDown_)return;
        if(app_.page()==app::Page::DeckEditor){releaseDeck(p);return;}
        movePointer(p);bool dragged=dragging_;auto source=pressedCard_;auto delta=p-pressPoint_;resetPointer();
        if(!dragged){if(delta.x*delta.x+delta.y*delta.y<64)click(p);return;}
        auto c=find(source);
        if(c && c->instance.zone==Zone::Hand && hit(p,handArea)) {
            handOrder_.moveBefore(viewer_,source,insertBefore(p,source));cue(ui::SoundId::Place);status_="已调整手牌顺序";return;
        }
        auto trial=interaction_;
        if(previewDrop(trial,source,p)){interaction_=std::move(trial);cue(ui::SoundId::Place);status_="已选择操作，请确认或继续选择";}
        else {cue(ui::SoundId::Reject);status_="此处不可使用，卡牌已返回";}
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
        const std::array<std::pair<Zone,Rect>,5> zones{{{Zone::Analysis,analysisArea},{Zone::Words,wordsArea},{Zone::Casting,castingArea},{Zone::Action,actionZone},{Zone::Ash,ashArea}}};
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
        if(app_.paused() || !interaction_.pending())return;auto command=interaction_.pending()->command;
        try{auto result=app_.submit(viewer_,command,app_.generation());if(result.accepted){
            const auto choice=std::get_if<Choose>(&command);
            if(!std::holds_alternative<AdvancePhase>(command) && !std::holds_alternative<PassResponse>(command) && !(choice && !choice->option))cue(ui::SoundId::Confirm);
            refresh();status_="操作完成";std::string error;if(!app_.saveReplay(error)){status_=error;cue(ui::SoundId::Reject);}
        }else{status_=result.error;cue(ui::SoundId::Reject);interaction_.cancel();}}
        catch(const std::exception& e){status_=e.what();cue(ui::SoundId::Reject);interaction_.cancel();}
    }
    void click(sf::Vector2f p) {
        if(!hit(p,confirm_) && !hit(p,cancelAction_) &&
           std::any_of(uiButtonHits_.begin(),uiButtonHits_.end(),[&](Rect r){return hit(p,r);}))cue(ui::SoundId::Click);
        if(app_.page()!=app::Page::Match){const bool wasMatch=app_.hasMatch();clickPage(p);if(wasMatch && !app_.hasMatch())resources_.stopSounds();return;}
        if(hit(p,settingsButton)){openSettings();return;}
        if(hit(p,motionButton_)){auto setting=app_.settings();setting.reducedMotion=!setting.reducedMotion;std::string error;if(app_.applySettings(setting,error)){presentation_.clear();motionOrigins_.clear();status_=setting.reducedMotion?"动画已精简":"完整动画已开启";}else status_=error;return;}
        if(app_.tutorial()) {
            if(hit(p,tutorialContinue_)){app_.continueTutorial();status_.clear();return;}
            if(hit(p,tutorialRestart_)){restartTutorial();return;}
        }
        if(scene_==Scene::Handoff){if(hit(p,handoffButton)){scene_=Scene::Match;cue(ui::SoundId::TurnReady);if(view_.decision && view_.decision->kind==DecisionKind::Response)cue(ui::SoundId::ResponseOpen);}return;}
        if(scene_==Scene::Result){
            std::string error;
            if(hit(p,resultRestart_)){if(app_.restart(static_cast<std::uint32_t>(std::chrono::steady_clock::now().time_since_epoch().count()),error))resetMatchUi();else status_=error;}
            if(hit(p,resultMenu_)){app_.requestLeave();if(app_.confirmLeave(viewer_,error)){resources_.stopSounds();interaction_.cancel();resetPointer();status_="对局记录已保存。";}else status_=error;}
            return;
        }
        if(interaction_.step()==ui::Step::LinkTarget){if(hit(p,cancelButton) || hit(p,detailClose)){interaction_.cancel();cue(ui::SoundId::Cancel);return;}for(const auto& row:linkHits_)if(hit(p,row.first)){interaction_.pickLink(row.second);return;}return;}
        if(interaction_.choosingFormation()) {
            if(hit(p,cancelButton) || hit(p,detailClose)){interaction_.cancel();cue(ui::SoundId::Cancel);return;}
            for(const auto& h:formationHits_)if(hit(p,h.first)){interaction_.pick(h.second);return;}
            return;
        }
        if(hit(p,saveButton)){save();return;}if(hit(p,logButton)){showLog_=!showLog_;return;}if(showLog_)return;
        if(hit(p,sortType) || hit(p,sortCost)){handOrder_.sort(view_,hit(p,sortCost));offsets_[-1]=0;status_="手牌已整理";return;}
        if(inspectedCard()) {
            if(hit(p,detailClose)){interaction_.cancel();cue(ui::SoundId::Cancel);return;}
            if(hit(p,detailPanel))return;
        }
        if(interaction_.step()==ui::Step::Target || interaction_.step()==ui::Step::Cost) {
            if(hit(p,cancelButton)){interaction_.cancel();cue(ui::SoundId::Cancel);return;}
            for(auto it=cardHits_.rbegin();it!=cardHits_.rend();++it)if(hit(p,it->first)){if(!interaction_.pick(it->second))status_="请选择高亮卡牌";return;}return;
        }
        if(interaction_.pending() || interaction_.selected()) {
            if(interaction_.pending() && hit(p,cancelAction_)){interaction_.cancel();cue(ui::SoundId::Cancel);return;}
            if(interaction_.pending() && hit(p,confirm_)){submit();return;}
            for(const auto& h:groupHits_)if(hit(p,h.first)){interaction_.activate(h.second);return;}
            if(hit(p,actionArea_))return;
            interaction_.cancel();
        }
        if(hit(p,phaseButton)){if(auto a=phaseAction())interaction_.offer(*a);return;}
        if(hit(p,surrenderButton)){for(const auto& a:view_.actions)if(std::holds_alternative<Surrender>(a.command)){interaction_.offer(a);break;}return;}
        for(const auto& h:decisionHits_)if(hit(p,h.first)){interaction_.offer(h.second);return;}
        for(auto it=cardHits_.rbegin();it!=cardHits_.rend();++it)if(hit(p,it->first)){interaction_.select(it->second);cue(ui::SoundId::Select);groupOffset_=0;return;}
    }
    void scroll(sf::Vector2f p,int delta) {
        if(app_.page()!=app::Page::Match){scrollDecks(p,delta);return;}
        if(app_.page()!=app::Page::Match || scene_!=Scene::Match || showLog_ || interaction_.choosingFormation())return;
        if(interaction_.step()==ui::Step::LinkTarget){if(hit(p,{{500,180},{610,430}}))offsets_[-4]=std::clamp(offsets_[-4]+delta,0,std::max(0,static_cast<int>(interaction_.linkCandidates().size())-8));return;}
        if(view_.chain && hit(p,{{25,310},{215,340}})){offsets_[-3]=std::clamp(offsets_[-3]+delta,0,std::max(0,static_cast<int>(view_.chain->links.size())-6));return;}
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
    void clickCard(sf::RenderWindow& w,CardId id){
        if(interaction_.choosingFormation()){draw(w);for(const auto& h:formationHits_)if(h.second==id){auto p=h.first.position+sf::Vector2f{6,6};press(p);releasePointer(p);return;}throw std::runtime_error("formation option missing");}
        auto p=pointFor(w,id);press(p);releasePointer(p);
    }
    void clickLink(sf::RenderWindow& w,LinkId id){auto links=interaction_.linkCandidates();auto i=std::find(links.begin(),links.end(),id);if(i==links.end())throw std::runtime_error("UI chain target unavailable");offsets_[-4]=static_cast<int>(i-links.begin());draw(w);for(const auto& row:linkHits_)if(row.second==id){click(row.first.position+sf::Vector2f{6,6});return;}throw std::runtime_error("UI chain target not visible");}
    void gesture(sf::RenderWindow& w,CardId source,sf::Vector2f target) {
        auto origin=pointFor(w,source);press(origin);movePointer(origin+sf::Vector2f{12,-12});draw(w);movePointer(target);draw(w);releasePointer(target);++dragCount_;
    }
    void exerciseGestures(sf::RenderWindow& w) {
        auto before=app_.match().engine().digest();auto ids=handOrder_.cards(viewer_);if(ids.size()<2)return;
        auto last=pointFor(w,ids.back());gesture(w,ids.front(),last+sf::Vector2f{80,30});
        if(handOrder_.cards(viewer_).front()==ids.front())throw std::runtime_error("hand drag did not reorder");
        draw(w);click(sortCost.position+sf::Vector2f{10,10});click(sortType.position+sf::Vector2f{10,10});
        gesture(w,ids.front(),{30,90});
        auto origin=pointFor(w,ids.front());press(origin);movePointer(origin+sf::Vector2f{20,-70});resetPointer();
        if(dragging_ || interaction_.pending() || app_.match().engine().digest()!=before)throw std::runtime_error("gesture cancellation changed rules");
    }
    bool dragCommand(sf::RenderWindow& w,const LegalAction& action) {
        Zone destination=Zone::Deck;CardId target=ui::Interaction::target(action.command);
        if(std::holds_alternative<SetFormation>(action.command) || std::holds_alternative<StartAnalysis>(action.command) || std::holds_alternative<AttachSeal>(action.command))destination=Zone::Analysis;
        else if(std::holds_alternative<PrepareCast>(action.command) || std::holds_alternative<Respond>(action.command))destination=Zone::Casting;
        else if(std::holds_alternative<PlayAction>(action.command))destination=Zone::Action;
        else if(auto word=std::get_if<PreloadWord>(&action.command))destination=word->release?Zone::Casting:Zone::Words;
        else if(std::holds_alternative<Choose>(action.command) && view_.decision)destination=(view_.decision->kind==DecisionKind::Discard || view_.decision->kind==DecisionKind::Overflow || view_.decision->kind==DecisionKind::EffectDiscard || view_.decision->kind==DecisionKind::DestroyOwnFormation)?Zone::Ash:Zone::Casting;
        if(destination==Zone::Deck)return false;
        sf::Vector2f drop=destination==Zone::Analysis?sf::Vector2f{789,817}:destination==Zone::Words?sf::Vector2f{1320,646}:destination==Zone::Action?sf::Vector2f{1300,700}:destination==Zone::Ash?sf::Vector2f{1320,814}:sf::Vector2f{1300,540};
        if(target && !std::holds_alternative<StartAnalysis>(action.command))drop=pointFor(w,target);
        auto before=app_.match().engine().digest();gesture(w,action.source,drop);
        if(interaction_.step()==ui::Step::LinkTarget)clickLink(w,std::get<Respond>(action.command).link);
        if(interaction_.choosingFormation())clickCard(w,target);
        if(interaction_.step()==ui::Step::Cost)gesture(w,ui::Interaction::costCard(action.command),{1320,814});
        if(before!=app_.match().engine().digest())throw std::runtime_error("drag paid before confirmation");
        return true;
    }
    void drive(sf::RenderWindow& w,const Json& recorded) {
        Json row=recorded;auto legacy=decodeCommand(row.at("command"));
        if(std::holds_alternative<Advance>(legacy) && view_.phase==Phase::Main)row["command"]=encodeCommand(AdvancePhase{view_.phaseGate});
        if(auto choice=std::get_if<Choose>(&legacy))if(view_.decision && view_.decision->kind==DecisionKind::Response && choice->decision==view_.decision->id){if(!choice->option)row["command"]=encodeCommand(PassResponse{choice->decision});else{std::vector<Command> matches;for(const auto& action:view_.actions)if(auto r=std::get_if<Respond>(&action.command))if(r->card==choice->option)matches.push_back(action.command);if(matches.size()==1)row["command"]=encodeCommand(matches.front());}}
        if(row.at("actor").get<int>()!=viewer_)throw std::runtime_error("UI viewer mismatch");
        auto it=std::find_if(view_.actions.begin(),view_.actions.end(),[&](const LegalAction& a){return encodeCommand(a.command)==row.at("command");});
        if(it==view_.actions.end())throw std::runtime_error("UI operation unavailable");LegalAction a=*it;
        if(view_.decision && exercisedPauseKinds_.insert(view_.decision->kind).second)pauseAtDecision(w,a.command);
        if(a.source && smokeDragging_ && dragCommand(w,a)) {
            // Pointer press/move/release has selected the same command as the click path.
        } else if(a.source) {
            clickCard(w,a.source);auto groups=interaction_.groups();auto title=ui::Interaction::intent(a,view_.decision,&view_);std::size_t n=0;for(;n<groups.size();++n)if(groups[n].title==title)break;
            groupOffset_=static_cast<int>(n);draw(w);bool clicked=false;
            for(const auto& h:groupHits_)if(h.second==n){auto p=h.first.position+sf::Vector2f{6,6};click(p);clicked=true;break;}
            if(!clicked)throw std::runtime_error("UI context unavailable: "+a.label);
            if(interaction_.step()==ui::Step::LinkTarget)clickLink(w,std::get<Respond>(a.command).link);
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
        if(exercisedSelectionKinds_.insert(a.command.index()).second)pauseAtDecision(w,a.command);
        draw(w);click(confirm_.position+sf::Vector2f{6,6});
        if(app_.match().engine().digest()!=row.at("digest").get<std::string>())throw std::runtime_error("UI state mismatch: "+a.label);
    }
};
}
int main(int argc,char** argv) {
    try {
        auto assets=std::filesystem::absolute(argv[0]).parent_path()/"assets";std::string screenshot,inspect;bool showcase=false,dragSmoke=false,previewDrag=false,previewDropChoice=false,manualPhases=false,menuSmoke=false,deckSmoke=false,aiSmoke=false,audioSmoke=false;std::filesystem::path userDirectory;std::string capturePage;std::uint32_t seed=42;Json smoke;int captureStep=3;
        for(int i=1;i<argc;++i){std::string arg=argv[i];if(arg=="--assets" && i+1<argc)assets=argv[++i];else if(arg=="--screenshot" && i+1<argc)screenshot=argv[++i];else if(arg=="--showcase")showcase=true;else if(arg=="--preview-drop")previewDropChoice=true;else if(arg=="--preview-drag")previewDrag=true;else if(arg=="--inspect-card" && i+1<argc)inspect=argv[++i];else if(arg=="--capture-step" && i+1<argc)captureStep=std::stoi(argv[++i]);else if(arg=="--user-data" && i+1<argc)userDirectory=std::filesystem::u8path(argv[++i]);else if(arg=="--capture-page" && i+1<argc)capturePage=argv[++i];else if(arg=="--menu-smoke")menuSmoke=true;else if(arg=="--deck-smoke")deckSmoke=true;else if(arg=="--ai-smoke")aiSmoke=true;else if(arg=="--audio-smoke")audioSmoke=true;else if(arg=="--seed" && i+1<argc)seed=static_cast<std::uint32_t>(std::stoul(argv[++i]));else if((arg=="--ui-smoke" || arg=="--drag-smoke") && i+1<argc){dragSmoke=arg=="--drag-smoke";smoke=readJson(argv[++i]);seed=smoke.at("seed").get<std::uint32_t>();}}
        float animationTime=-1;
        std::string animationFrames;
        for(int i=1;i<argc;++i){if(std::string(argv[i])=="--manual-phases")manualPhases=true;else if(std::string(argv[i])=="--animation-frames" && i+1<argc)animationFrames=argv[++i];else if(std::string(argv[i])=="--animation-time" && i+1<argc){animationTime=std::stof(argv[++i]);if(!std::isfinite(animationTime) || animationTime<0 || animationTime>3)throw std::runtime_error("animation-time must be between 0 and 3 seconds");}}
        if(userDirectory.empty()){if(menuSmoke || deckSmoke || aiSmoke || audioSmoke)throw std::runtime_error("UI smoke requires an isolated --user-data directory");userDirectory=app::userDataDirectory();}
        Client client(assets,seed,smoke,userDirectory,capturePage=="ai-replay");if(audioSmoke){client.audioSmoke();return 0;}client.run(screenshot,showcase,smoke,inspect,captureStep,dragSmoke,previewDrag,previewDropChoice,manualPhases,menuSmoke,deckSmoke,aiSmoke,capturePage,animationTime,animationFrames);return 0;
    }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
