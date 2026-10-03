#include "wizard/core.hpp"
#include <algorithm>
#include <iomanip>
#include <sstream>

namespace wizard {
std::vector<LegalAction> GameEngine::legalActions(PlayerId p) const {
    std::vector<LegalAction> out; const auto& s=state_;
    auto add=[&](std::string label,CardId source,CardId target,Command cmd) {
        auto trial=*this;
        if(trial.execute(p,cmd).empty()) out.push_back({std::move(label),source,target,std::move(cmd)});
    };
    if(s.result!=-1) return out;
    if(s.decision) {
        const auto& d=*s.decision;
        if(d.player!=p) return out;
        for(auto id:d.options) {
            std::string label;
            if(d.kind==DecisionKind::ClearLoad) label="移除负担 #"+std::to_string(id)+" 的1点荷载";
            else if(d.kind==DecisionKind::TriggerOrder) {
                for(const auto& t:s.queue.items) if(t.id==id) label="先结算 "+catalog_.at(s.cards.at(t.source).definition).name;
            } else label="选择 "+catalog_.at(s.cards.at(id).definition).name+" #"+std::to_string(id);
            add(label,(d.kind==DecisionKind::ClearLoad || d.kind==DecisionKind::TriggerOrder)?0:id,0,Choose{d.id,id});
        }
        if(d.mayPass) add("跳过 / 停止",0,0,Choose{d.id,0});
        add("投降",0,0,Surrender{}); return out;
    }
    if(s.active!=p) return out;
    for(const auto& [id,c]:s.cards) if(c.owner==p) {
        const auto& def=catalog_.at(c.definition); const auto name=def.name+" #"+std::to_string(id);
        if(c.zone==Zone::Hand) {
            if(def.type==CardType::Formation) {
                add("设置 "+name,id,0,SetFormation{id,0});
                for(const auto& [other,h]:s.cards) if(h.owner==p && h.zone==Zone::Analysis && catalog_.at(h.definition).type==CardType::Formation && !h.base)
                    add("替换 "+catalog_.at(h.definition).name+" #"+std::to_string(other),id,other,SetFormation{id,other});
            } else if(def.type==CardType::Analytic || def.type==CardType::Seal) {
                for(const auto& [host,h]:s.cards) if(h.owner==p && h.zone==Zone::Analysis) {
                    const auto hostName=catalog_.at(h.definition).name+" #"+std::to_string(host);
                    if(def.type==CardType::Analytic) add("解析 "+name+" → "+hostName,id,host,StartAnalysis{id,host});
                    else add("附加 "+name+" → "+hostName,id,host,AttachSeal{id,host});
                }
            } else if(def.type==CardType::Word) add("预置 "+name,id,0,PreloadWord{id});
            else add("使用 "+name,id,0,PlayAction{id,0});
        } else if(c.zone==Zone::Analysis && def.type==CardType::Formation) add("拆除 "+name,id,0,RemoveFormation{id});
        else if(c.zone==Zone::Attached) add("拆除 "+name,id,0,RemoveSeal{id});
        if(c.zone==Zone::Analysis && c.spell==SpellState::Ready) {
            if(def.target==TargetKind::EmptyEnemyFormation) {
                for(const auto& [target,t]:s.cards) if(Rules::targetValid(s,def,p,target))
                    if(def.extraDiscard) {
                        for(const auto& [discard,h]:s.cards) if(h.owner==p && h.zone==Zone::Hand)
                            add("目标 "+catalog_.at(t.definition).name+" #"+std::to_string(target)+"；弃 "+catalog_.at(h.definition).name+" #"+std::to_string(discard),id,target,PrepareCast{id,target,discard});
                    } else add("目标 "+catalog_.at(t.definition).name+" #"+std::to_string(target),id,target,PrepareCast{id,target,0});
            } else if(def.extraDiscard) {
                for(const auto& [discard,h]:s.cards) if(h.owner==p && h.zone==Zone::Hand) add("准备 "+name+"；弃 "+catalog_.at(h.definition).name+" #"+std::to_string(discard),id,0,PrepareCast{id,0,discard});
            } else add("准备施法 "+name,id,0,PrepareCast{id,0,0});
        }
        if(c.zone==Zone::Analysis || c.zone==Zone::Casting || c.zone==Zone::Words) add("放弃 "+name,id,0,Abandon{id});
    }
    add("结束主要阶段",0,0,Advance{}); add("投降",0,0,Surrender{}); return out;
}
GameView GameEngine::viewFor(PlayerId p) const {
    if(p<0 || p>1) throw std::out_of_range("invalid viewer");
    GameView v; v.viewer=p; v.active=state_.active; v.phase=state_.phase; v.result=state_.result;
    for(int i=0;i<2;++i) {
        const auto& pl=state_.players[i]; int hand=0;
        for(const auto& [id,c]:state_.cards) if(c.owner==i && c.zone==Zone::Hand) ++hand;
        v.players[i]={pl.life,pl.mana,Rules::load(state_,i),Rules::capacity(state_,catalog_,i),hand,static_cast<int>(pl.deck.size()),pl.ownTurn,pl.actions};
    }
    for(const auto& [id,c]:state_.cards) if(c.zone!=Zone::Deck && (c.zone!=Zone::Hand || c.owner==p)) {
        const auto& def=catalog_.at(c.definition);CardView card{c,def};
        if(def.type==CardType::Formation && c.zone==Zone::Analysis) {card.effectiveRings=Rules::rings(state_,catalog_,id);card.occupiedRings=Rules::occupied(state_,id);}
        if(c.spell==SpellState::Analyzing)card.turnsToReady=std::max(0,def.analysisTurns-(state_.players[c.owner].ownTurn-c.analysisStarted));
        v.cards.push_back(std::move(card));
    }
    if(state_.decision && state_.decision->player==p) v.decision=state_.decision;
    for(const auto& e:state_.events) if(e.audience==-1 || e.audience==p) v.events.push_back(e);
    v.temporary=state_.temporary; v.actions=legalActions(p); return v;
}
std::string GameEngine::canonicalState() const {
    const auto& s=state_; std::ostringstream o;
    o<<s.active<<','<<s.first<<','<<static_cast<int>(s.phase)<<','<<static_cast<int>(s.flow)<<','<<s.result<<','<<s.rng<<','<<s.nextLoad<<','<<s.nextTrigger<<','<<s.nextBatch<<','<<s.nextDecision<<';';
    for(const auto& p:s.players) { o<<p.life<<','<<p.mana<<','<<p.ownTurn<<','<<p.actions<<','<<p.formations<<','<<p.sealRemovals<<','<<p.drawFailed<<','<<p.surrendered<<':'; for(auto id:p.deck) o<<id<<','; o<<';'; }
    for(const auto& [id,c]:s.cards) {
        o<<id<<','<<std::quoted(c.definition)<<','<<c.owner<<','<<static_cast<int>(c.zone)<<','<<static_cast<int>(c.spell)<<','<<c.host<<','<<c.sourceFormation<<','<<c.targetCard<<','<<c.targetPlayer<<','<<c.base<<','<<c.analysisStarted<<','<<c.remaining<<','<<c.analysisLoad<<','<<c.castLoad<<','<<c.canceledTurn<<':';
        for(const auto& pay:c.payments) o<<pay.turn<<','<<pay.paid<<','<<pay.load<<','<<pay.canceled<<'/'; o<<';';
    }
    for(const auto& t:s.temporary) o<<'L'<<t.id<<','<<t.owner<<','<<t.source<<','<<t.amount<<','<<t.expiryTurn<<';';
    auto trigger=[&](const Trigger& t) { o<<t.id<<','<<t.batch<<','<<t.owner<<','<<t.source<<','<<t.targetCard<<','<<t.targetPlayer<<':'; for(const auto& e:t.effects) o<<static_cast<int>(e.kind)<<','<<e.amount<<'/'; o<<';'; };
    for(const auto& t:s.queue.items) { o<<'Q'; trigger(t); }
    if(s.effect) { o<<'E'<<s.effect->cursor<<','<<static_cast<int>(s.effect->after)<<','<<s.effect->clearRemaining<<';'; trigger(s.effect->item); }
    if(s.preparation) o<<'P'<<s.preparation->spell<<','<<s.preparation->responder<<','<<s.preparation->canceled<<';';
    if(s.decision) { const auto& d=*s.decision; o<<'D'<<d.id<<','<<d.player<<','<<static_cast<int>(d.kind)<<','<<d.mayPass<<':'; for(auto id:d.options) o<<id<<','; }
    return o.str();
}
std::string GameEngine::digest() const { return fingerprint(canonicalState()); }
}
