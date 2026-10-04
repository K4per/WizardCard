#include "wizard/core.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>
#include <stdexcept>

namespace wizard {
const CardDefinition& CardCatalog::at(const std::string& id) const { return cards.at(id); }
void EffectQueue::append(Trigger t) { items.push_back(std::move(t)); }
int Rules::load(const GameState& s, PlayerId p) {
    int n=0;
    for (const auto& [id,c]:s.cards) if(c.owner==p) n+=c.analysisLoad+c.castLoad;
    for(const auto& t:s.temporary) if(t.owner==p) n+=t.amount;
    return n;
}
int Rules::capacity(const GameState& s,const CardCatalog& cat,PlayerId p) {
    int n=0; for(const auto& [id,c]:s.cards)
        if(c.owner==p && c.zone==Zone::Analysis && cat.at(c.definition).type==CardType::Formation) n+=cat.at(c.definition).capacity;
    return n;
}
int Rules::rings(const GameState& s,const CardCatalog& cat,CardId id) {
    int n=cat.at(s.cards.at(id).definition).rings;
    for(const auto& [cid,c]:s.cards) if(c.zone==Zone::Attached && c.host==id) n+=cat.at(c.definition).ringBonus;
    return n;
}
int Rules::occupied(const GameState& s,CardId host) {
    int n=0; for(const auto& [id,c]:s.cards) if(c.host==host && c.zone==Zone::Analysis) ++n; return n;
}
bool Rules::targetValid(const GameState& s,const CardDefinition& d,PlayerId p,CardId target) {
    if(d.target!=TargetKind::EmptyEnemyFormation) return target==0;
    auto i=s.cards.find(target); if(i==s.cards.end()) return false;
    const auto& c=i->second;
    return c.owner!=p && c.zone==Zone::Analysis && c.spell==SpellState::None && !c.base && occupied(s,target)==0;
}
void StateMaintenance::leave(GameState& s,CardId id) {
    auto i=s.cards.find(id); if(i==s.cards.end() || i->second.zone==Zone::Ash) return;
    auto& c=i->second; if(c.base) return;
    std::vector<CardId> children;
    for(const auto& [cid,x]:s.cards) if(x.host==id && (x.zone==Zone::Attached || x.zone==Zone::Analysis)) children.push_back(cid);
    for(auto cid:children) leave(s,cid);
    auto& deck=s.players[c.owner].deck; deck.erase(std::remove(deck.begin(),deck.end(),id),deck.end());
    c.zone=Zone::Ash; c.spell=SpellState::None; c.host=0; c.analysisLoad=0; c.castLoad=0; c.remaining=0;
    s.events.push_back({"leave","卡牌进入灰烬区",-1,id});
}
void StateMaintenance::check(GameState& s,const CardCatalog& cat) {
    if(s.result!=-1) return;
    bool loses[2]{};
    for(int p=0;p<2;++p) loses[p]=s.players[p].life<=0 || s.players[p].drawFailed || s.players[p].surrendered || Rules::load(s,p)>Rules::capacity(s,cat,p);
    if(loses[0] || loses[1]) { s.result=loses[0]&&loses[1]?2:loses[0]?1:0;
        for(int p=0;p<2;++p) if(loses[p]) {
            std::string reason="玩家 "+std::to_string(p+1)+" 败北：";
            if(s.players[p].life<=0) reason+="生命归零 ";
            if(s.players[p].drawFailed) reason+="抽牌失败 ";
            if(s.players[p].surrendered) reason+="投降 ";
            if(Rules::load(s,p)>Rules::capacity(s,cat,p)) reason+="魔力过载 ";
            s.events.push_back({"defeat_reason",reason});
        }
        s.events.push_back({"game_over",s.result==2?"平局":"玩家 "+std::to_string(s.result+1)+" 获胜"});
        s.queue.items.clear(); s.decision.reset(); s.preparation.reset(); s.effect.reset(); }
}
std::vector<std::string> Rules::invariants(const GameState& s,const CardCatalog& cat) {
    std::vector<std::string> errors;
    for(int p=0;p<2;++p) {
        if(s.players[p].mana<0 || s.players[p].mana>12) errors.push_back("mana bounds");
        std::map<CardId,int> seen;
        for(auto id:s.players[p].deck) {
            auto i=s.cards.find(id);
            if(++seen[id]!=1 || i==s.cards.end() || i->second.owner!=p || i->second.zone!=Zone::Deck) errors.push_back("deck membership");
        }
        int body=0,words=0;
        for(const auto& [id,c]:s.cards) if(c.owner==p) {
            if(c.zone==Zone::Deck && seen[id]!=1) errors.push_back("missing deck card");
            if(c.zone==Zone::Words) ++words;
            if(c.zone==Zone::Analysis && cat.at(c.definition).type==CardType::Formation) body+=cat.at(c.definition).body;
        }
        if(body>5 || words>3) errors.push_back("zone capacity");
    }
    for(const auto& [id,c]:s.cards) {
        if(c.id!=id || c.owner<0 || c.owner>1 || c.analysisLoad<0 || c.castLoad<0) errors.push_back("instance bounds");
        if(c.base && c.zone!=Zone::Analysis) errors.push_back("base protection");
        if(c.zone==Zone::Ash && (c.analysisLoad || c.castLoad || c.host)) errors.push_back("ash residue");
        if(c.host) {
            auto h=s.cards.find(c.host);
            if(h==s.cards.end() || h->second.owner!=c.owner || (h->second.zone!=Zone::Analysis && h->second.zone!=Zone::Casting)) errors.push_back("host relation");
            else if(c.zone==Zone::Analysis && cat.at(h->second.definition).type!=CardType::Formation) errors.push_back("analysis host is not formation");
        }
        if((c.zone==Zone::Attached || (c.zone==Zone::Analysis && cat.at(c.definition).type==CardType::Analytic)) && !c.host) errors.push_back("missing host");
        if(c.zone==Zone::Attached && cat.at(c.definition).type!=CardType::Seal) errors.push_back("non-seal attachment");
        int seals=0; for(const auto& [other,x]:s.cards) if(x.zone==Zone::Attached && x.host==id) ++seals;
        if(seals>1) errors.push_back("multiple seals");
        if(c.zone==Zone::Casting && c.host) errors.push_back("casting still attached to formation");
    }
    return errors;
}
void EffectResolver::apply(GameState& s,const CardCatalog&,const Trigger& t,const Effect& e) {
    int p=t.targetPlayer>=0?t.targetPlayer:t.owner;
    switch(e.kind) {
    case EffectKind::Damage: s.players[p].life-=e.amount; break;
    case EffectKind::Heal: s.players[p].life=std::min(maximumLife,s.players[p].life+e.amount); break;
    case EffectKind::GainMana: s.players[t.owner].mana=std::min(12,s.players[t.owner].mana+e.amount); break;
    case EffectKind::Draw:
        for(int k=0;k<e.amount;++k) {
            auto& pl=s.players[t.owner];
            if(pl.deck.empty()) { pl.drawFailed=true; continue; }
            auto id=pl.deck.back(); pl.deck.pop_back(); s.cards.at(id).zone=Zone::Hand;
            s.events.push_back({"draw","抽取一张牌",t.owner,id});
        } break;
    case EffectKind::AddTemporary:
        s.temporary.push_back({s.nextLoad++,p,t.source,e.amount,s.players[p].ownTurn+(p==s.active?0:1)}); break;
    case EffectKind::Destroy: StateMaintenance::leave(s,t.targetCard); break;
    case EffectKind::CancelPreparation:
        if(s.preparation) {
            auto& prep=*s.preparation; prep.canceled=true;
            auto& c=s.cards.at(prep.spell);
            if(c.zone==Zone::Analysis) {
                c.castLoad=0; c.canceledTurn=s.players[c.owner].ownTurn;
                if(!c.payments.empty()) { c.payments.back().canceled=true; c.payments.back().load=0; }
            }
        } break;
    case EffectKind::ClearTemporary: throw std::logic_error("clear temporary requires a decision");
    }
    std::string message;
    switch(e.kind) {
    case EffectKind::Damage:message="造成 "+std::to_string(e.amount)+" 点伤害";break;
    case EffectKind::Heal:message="恢复 "+std::to_string(e.amount)+" 点生命";break;
    case EffectKind::Draw:message="抽取 "+std::to_string(e.amount)+" 张牌";break;
    case EffectKind::AddTemporary:message="增加 "+std::to_string(e.amount)+" 点临时荷载";break;
    case EffectKind::ClearTemporary:message="清除临时荷载";break;
    case EffectKind::Destroy:message="销毁目标卡牌";break;
    case EffectKind::CancelPreparation:message="取消本次准备施法";break;
    case EffectKind::GainMana:message="获得 "+std::to_string(e.amount)+" 魔素";break;
    }
    s.events.push_back({"effect",message,-1,t.source,e.amount});
}
std::string fingerprint(const std::string& bytes) {
    std::uint64_t h=14695981039346656037ull;
    for(unsigned char c:bytes) { h^=c; h*=1099511628211ull; }
    std::ostringstream o; o<<std::hex<<std::setfill('0')<<std::setw(16)<<h; return o.str();
}
std::string phaseName(Phase p) { return std::array<const char*,5>{"抽卡","准备","主要","施法","结束"}.at(static_cast<std::size_t>(p)); }
std::string zoneName(Zone p) { return std::array<const char*,8>{"牌库","手牌","行动","解析","言灵","施法","灰烬","符文"}.at(static_cast<std::size_t>(p)); }
}
