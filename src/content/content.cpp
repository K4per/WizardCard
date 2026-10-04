#include "wizard/content.hpp"
#include <fstream>
#include <stdexcept>

namespace wizard {
namespace {
[[noreturn]] void fail(const std::string& path,const std::string& message) { throw std::runtime_error(path+": "+message); }
int number(const Json& j,const char* key,const std::string& path,int fallback=0) {
    if(!j.contains(key)) return fallback;
    const auto& x=j.at(key);
    if(!x.is_number_integer() || x.get<std::int64_t>()<0 || x.get<std::int64_t>()>1000) fail(path+"/"+key,"expected integer in [0,1000]");
    return x.get<int>();
}
std::string text(const Json& j,const char* key,const std::string& path) {
    if(!j.contains(key) || !j.at(key).is_string() || j.at(key).get<std::string>().empty()) fail(path+"/"+key,"expected nonempty string");
    return j.at(key).get<std::string>();
}
template<typename T> T enumeration(const std::string& value,const std::map<std::string,T>& table,const std::string& path) {
    auto i=table.find(value); if(i==table.end()) fail(path,"unknown value: "+value); return i->second;
}
std::vector<Effect> effects(const Json& j,const std::string& path) {
    if(!j.is_array()) fail(path,"expected effect array");
    std::vector<Effect> out; int n=0;
    const std::map<std::string,EffectKind> names={{"damage",EffectKind::Damage},{"heal",EffectKind::Heal},{"draw",EffectKind::Draw},{"temporary",EffectKind::AddTemporary},{"clear_temporary",EffectKind::ClearTemporary},{"destroy",EffectKind::Destroy},{"cancel",EffectKind::CancelPreparation},{"mana",EffectKind::GainMana}};
    for(const auto& e:j) { const auto p=path+"/"+std::to_string(n++); out.push_back({enumeration(text(e,"kind",p),names,p+"/kind"),number(e,"amount",p)}); }
    return out;
}
}
Json readJson(const std::filesystem::path& p) {
    std::ifstream f(p); if(!f) fail(p.string(),"cannot open");
    try { return Json::parse(f); } catch(const Json::exception& e) { fail(p.string(),e.what()); }
}
void writeJson(const std::filesystem::path& p,const Json& j) {
    if(p.has_parent_path()) std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p); if(!f) fail(p.string(),"cannot write"); f<<j.dump(2)<<'\n';
    if(!f) fail(p.string(),"write failed");
}
CardCatalog parseCatalog(const Json& j,const std::string& source) {
    CardCatalog cat; cat.rulesVersion=text(j,"rulesVersion",source); cat.cardSetVersion=text(j,"cardSetVersion",source);
    if(!j.contains("cards") || !j.at("cards").is_array()) fail(source+"/cards","expected array");
    const std::map<std::string,CardType> types={{"action",CardType::Action},{"analytic",CardType::Analytic},{"word",CardType::Word},{"formation",CardType::Formation},{"seal",CardType::Seal}};
    const std::map<std::string,TargetKind> targets={{"none",TargetKind::None},{"self",TargetKind::Self},{"opponent",TargetKind::Opponent},{"empty_enemy_formation",TargetKind::EmptyEnemyFormation}};
    std::size_t n=0;
    for(const auto& card:j.at("cards")) {
        const auto p=source+"/cards/"+std::to_string(n++); CardDefinition d;
        d.id=text(card,"id",p); d.name=text(card,"name",p); d.text=text(card,"text",p); d.rarity=text(card,"rarity",p);
        d.type=enumeration(text(card,"type",p),types,p+"/type");
        d.target=enumeration(card.contains("target")?text(card,"target",p):std::string("none"),targets,p+"/target");
        d.cost=number(card,"cost",p); d.castCost=number(card,"castCost",p); d.rank=number(card,"rank",p);
        d.analysisTurns=number(card,"analysisTurns",p,1); d.duration=number(card,"duration",p); d.burden=number(card,"burden",p); d.extraDiscard=number(card,"extraDiscard",p);
        d.body=number(card,"body",p); d.capacity=number(card,"capacity",p); d.income=number(card,"income",p);
        d.rings=number(card,"rings",p); d.maxRank=number(card,"maxRank",p); d.ringBonus=number(card,"ringBonus",p); d.refundCast=number(card,"refundCast",p);
        d.effects=effects(card.value("effects",Json::array()),p+"/effects"); d.onPrepare=effects(card.value("onPrepare",Json::array()),p+"/onPrepare");
        d.onEnd=effects(card.value("onEnd",Json::array()),p+"/onEnd");
        if(d.extraDiscard>1 || (d.extraDiscard && d.type!=CardType::Analytic)) fail(p+"/extraDiscard","prototype supports one discard at analytic preparation");
        if(d.type==CardType::Analytic && d.analysisTurns<1) fail(p+"/analysisTurns","must be positive");
        if(d.type==CardType::Formation && (d.body<1 || d.body>5 || d.cost!=0)) fail(p,"formation needs body 1..5 and cost 0");
        if((d.type==CardType::Analytic || d.type==CardType::Word) && d.rank<1) fail(p+"/rank","spell rank must be positive");
        if(d.type==CardType::Word && d.duration) fail(p+"/duration","first-set words must be instant");
        if(d.type!=CardType::Analytic && (!d.onPrepare.empty() || !d.onEnd.empty())) fail(p,"periodic effects require analytic spell");
        if((!d.onPrepare.empty() || !d.onEnd.empty()) && !d.duration) fail(p+"/duration","periodic effects require finite duration");
        if(d.ringBonus && d.type!=CardType::Seal) fail(p+"/ringBonus","requires seal");
        if(d.refundCast>1 || (d.refundCast && d.type!=CardType::Seal)) fail(p+"/refundCast","requires seal and value 0 or 1");
        if(!cat.cards.emplace(d.id,d).second) fail(p+"/id","duplicate card id");
    }
    if(!cat.cards.count("balance") || cat.at("balance").type!=CardType::Formation) fail(source+"/cards","missing base formation balance");
    cat.contentHash=fingerprint(j.dump()); return cat;
}
std::vector<std::string> parseDeck(const Json& j,const CardCatalog& cat,const std::string& source) {
    if(!j.is_array()) fail(source,"expected deck array"); std::vector<std::string> deck; std::map<std::string,int> names;
    int n=0; for(const auto& e:j) {
        auto p=source+"/"+std::to_string(n++); auto id=text(e,"id",p); int count=number(e,"count",p);
        if(!cat.cards.count(id)) fail(p+"/id","unknown definition");
        if(count<1 || (names[cat.at(id).name]+=count)>3) fail(p+"/count","same-name limit is 3");
        for(int k=0;k<count;++k) deck.push_back(id);
    }
    if(deck.size()!=30) fail(source,"deck must contain exactly 30 cards"); return deck;
}
Content loadContent(const std::filesystem::path& dir) {
    Content c; c.catalog=parseCatalog(readJson(dir/"cards.json"),(dir/"cards.json").string());
    c.deck=parseDeck(readJson(dir/"deck.json"),c.catalog,(dir/"deck.json").string()); return c;
}
}
