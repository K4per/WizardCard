#include "wizard/core.hpp"
#include <algorithm>
#include <set>
#include <sstream>
#include <stdexcept>
#include <type_traits>

namespace wizard {
namespace {
std::uint32_t random32(GameState& s) {
    auto x=s.rng; x^=x<<13; x^=x>>17; x^=x<<5; s.rng=x; return x;
}
std::uint32_t bounded(GameState& s,std::uint32_t n) {
    const auto threshold=(std::uint32_t{0}-n)%n;
    std::uint32_t x; do { x=random32(s); } while(x<threshold); return x%n;
}
std::vector<CardId> inZone(const GameState& s,PlayerId p,Zone z) {
    std::vector<CardId> ids; for(const auto& [id,c]:s.cards) if(c.owner==p && c.zone==z) ids.push_back(id); return ids;
}
int sealCount(const GameState& s,CardId host) {
    int n=0; for(const auto& [id,c]:s.cards) if(c.zone==Zone::Attached && c.host==host) ++n; return n;
}
int usedBody(const GameState& s,const CardCatalog& cat,PlayerId p) {
    int n=0; for(auto id:inZone(s,p,Zone::Analysis)) { const auto& d=cat.at(s.cards.at(id).definition); if(d.type==CardType::Formation) n+=d.body; } return n;
}
bool hasOverflow(const GameState& s,const CardCatalog& cat) {
    for(const auto& [id,c]:s.cards) if(c.zone==Zone::Analysis && cat.at(c.definition).type==CardType::Formation && Rules::occupied(s,id)>Rules::rings(s,cat,id)) return true;
    return false;
}
}
GameEngine::GameEngine(CardCatalog cat,std::array<std::vector<std::string>,2> decks,std::uint32_t seed):catalog_(std::move(cat)) {
    state_.rng=seed?seed:0x9e3779b9u;
    CardId id=1;
    for(int p=0;p<2;++p) {
        if(decks[p].size()!=30) throw std::invalid_argument("deck must contain 30 cards");
        std::map<std::string,int> counts;
        for(const auto& key:decks[p]) if(++counts[catalog_.at(key).name]>3) throw std::invalid_argument("at most 3 cards per name");
        catalog_.at("balance");
        CardInstance base; base.id=id++; base.definition="balance"; base.owner=p; base.base=true; base.zone=Zone::Analysis;
        state_.cards.emplace(base.id,base);
        for(const auto& key:decks[p]) { CardInstance c; c.id=id++; c.owner=p; c.definition=key; state_.cards.emplace(c.id,c); state_.players[p].deck.push_back(c.id); }
        auto& deck=state_.players[p].deck;
        for(std::size_t i=deck.size();i>1;--i) std::swap(deck[i-1],deck[bounded(state_,static_cast<std::uint32_t>(i))]);
        Trigger t; t.owner=p; EffectResolver::apply(state_,catalog_,t,{EffectKind::Draw,5});
    }
    state_.first=state_.active=static_cast<int>(bounded(state_,2)); state_.players[state_.active].ownTurn=1;
    pump();
}
GameEngine GameEngine::scenario(CardCatalog cat,GameState state) {
    GameEngine e; e.catalog_=std::move(cat); e.state_=std::move(state);
    if(!Rules::invariants(e.state_,e.catalog_).empty()) throw std::invalid_argument("invalid scenario");
    return e;
}
void GameEngine::decision(PlayerId p,DecisionKind k,std::vector<std::uint32_t> opts,bool pass) {
    state_.decision=PendingDecision{state_.nextDecision++,p,k,std::move(opts),pass};
}
void GameEngine::beginEffect(CardId id,AfterEffect after) {
    const auto& c=state_.cards.at(id); const auto& d=catalog_.at(c.definition);
    Trigger t; t.owner=c.owner; t.source=id; t.targetCard=c.targetCard; t.targetPlayer=c.targetPlayer; t.effects=d.effects;
    state_.effect=EffectFrame{std::move(t),0,after,-1};
}
std::string GameEngine::execute(PlayerId actor,const Command& command) {
    auto& s=state_;
    if(actor<0 || actor>1) return "invalid_player: 无效玩家";
    if(s.result!=-1) return "game_over: 对局已经结束";
    if(std::holds_alternative<Surrender>(command)) { s.players[actor].surrendered=true; StateMaintenance::check(s,catalog_); return {}; }
    if(s.decision) {
        auto choose=std::get_if<Choose>(&command);
        if(!choose || actor!=s.decision->player || choose->decision!=s.decision->id) return "decision_mismatch: 请完成当前玩家的选择";
        auto d=*s.decision; const auto opt=choose->option;
        if(!(opt==0 && d.mayPass) && std::find(d.options.begin(),d.options.end(),opt)==d.options.end()) return "invalid_option: 无效选择";
        s.decision.reset();
        s.events.push_back({"choice","玩家 "+std::to_string(actor+1)+" 完成选择"});
        switch(d.kind) {
        case DecisionKind::Response:
            ++s.preparation->responder;
            if(opt) { auto& c=s.cards.at(opt); c.zone=Zone::Casting; c.targetPlayer=1-actor; beginEffect(opt,AfterEffect::Word); }
            break;
        case DecisionKind::CastOrder: {
            auto& c=s.cards.at(opt); const auto& def=catalog_.at(c.definition);
            if(!Rules::targetValid(s,def,c.owner,c.targetCard)) { StateMaintenance::leave(s,opt); StateMaintenance::check(s,catalog_); }
            else beginEffect(opt,AfterEffect::Spell);
            break; }
        case DecisionKind::TriggerOrder: {
            auto i=std::find_if(s.queue.items.begin(),s.queue.items.end(),[&](const Trigger& t){return t.id==opt;});
            auto t=*i; s.queue.items.erase(i); s.effect=EffectFrame{std::move(t),0,AfterEffect::None,-1}; break; }
        case DecisionKind::Discard: StateMaintenance::leave(s,opt); StateMaintenance::check(s,catalog_); break;
        case DecisionKind::ClearLoad:
            if(!opt) { ++s.effect->cursor; s.effect->clearRemaining=-1; }
            else { auto i=std::find_if(s.temporary.begin(),s.temporary.end(),[&](const TemporaryLoad& t){return t.id==opt;}); --i->amount; --s.effect->clearRemaining;
                s.events.push_back({"load","移除1点临时荷载"}); }
            break;
        case DecisionKind::Overflow: StateMaintenance::leave(s,opt); break;
        }
        return {};
    }
    if(std::holds_alternative<Choose>(command)) return "stale_decision: 选择已过期";
    if(actor!=s.active || s.phase!=Phase::Main || s.flow!=Flow::Main || s.effect || s.preparation) return "wrong_phase: 当前不能执行主要阶段操作";
    auto& pl=s.players[actor];
    auto owned=[&](CardId id,Zone zone)->bool { auto i=s.cards.find(id); return i!=s.cards.end() && i->second.owner==actor && i->second.zone==zone; };
    auto cost=[&](int mana,int load)->bool { return pl.mana>=mana && Rules::load(s,actor)+load<=Rules::capacity(s,catalog_,actor); };
    auto pay=[&](CardId id,int mana) { pl.mana-=mana; s.events.push_back({"pay","支付 "+std::to_string(mana)+" 魔素",-1,id}); };
    std::string error=std::visit([&](const auto& cmd)->std::string {
        using T=std::decay_t<decltype(cmd)>;
        if constexpr(std::is_same_v<T,Advance>) { s.flow=Flow::Cast; s.phase=Phase::Cast; return {}; }
        else if constexpr(std::is_same_v<T,Choose> || std::is_same_v<T,Surrender>) { return "invalid_command"; }
        else {
            auto it=s.cards.find(cmd.card); if(it==s.cards.end() || it->second.owner!=actor) return "invalid_card: 不是自己的卡牌";
            auto& c=it->second; const auto& def=catalog_.at(c.definition);
            if constexpr(std::is_same_v<T,SetFormation>) {
                if(!owned(cmd.card,Zone::Hand) || def.type!=CardType::Formation || pl.formations) return "formation_limit: 阵法操作不可用";
                if(cmd.replace) {
                    if(!owned(cmd.replace,Zone::Analysis) || s.cards.at(cmd.replace).base || catalog_.at(s.cards.at(cmd.replace).definition).type!=CardType::Formation || pl.mana<2) return "invalid_replacement: 无法替换";
                    StateMaintenance::leave(s,cmd.replace); pay(cmd.card,2);
                }
                c.zone=Zone::Analysis; ++pl.formations;
                if(usedBody(s,catalog_,actor)>5 || Rules::load(s,actor)>Rules::capacity(s,catalog_,actor)) return "capacity: 替换后空间或荷载不合法";
            } else if constexpr(std::is_same_v<T,RemoveFormation>) {
                if(!owned(cmd.card,Zone::Analysis) || c.base || def.type!=CardType::Formation || pl.formations || pl.mana<2) return "invalid_removal: 无法拆除阵法";
                StateMaintenance::leave(s,cmd.card); pay(cmd.card,2); ++pl.formations;
                if(Rules::load(s,actor)>Rules::capacity(s,catalog_,actor)) return "overload: 拆除后会超载";
            } else if constexpr(std::is_same_v<T,StartAnalysis>) {
                if(!owned(cmd.card,Zone::Hand) || def.type!=CardType::Analytic || !owned(cmd.formation,Zone::Analysis)) return "invalid_analysis: 无效解析对象";
                const auto& host=catalog_.at(s.cards.at(cmd.formation).definition);
                if(host.type!=CardType::Formation || def.rank>host.maxRank || Rules::occupied(s,cmd.formation)>=Rules::rings(s,catalog_,cmd.formation)) return "ring_or_rank: 环位或位阶不允许";
                if(!cost(def.cost,def.cost)) return "cost: 魔素或荷载空间不足";
                pay(cmd.card,def.cost); c.analysisLoad=def.cost; c.zone=Zone::Analysis; c.spell=SpellState::Analyzing; c.host=cmd.formation; c.sourceFormation=cmd.formation; c.analysisStarted=pl.ownTurn;
            } else if constexpr(std::is_same_v<T,PrepareCast>) {
                if(!owned(cmd.card,Zone::Analysis) || c.spell!=SpellState::Ready || c.canceledTurn==pl.ownTurn) return "not_ready: 法术未就绪或本回合已被取消";
                if(!Rules::targetValid(s,def,actor,cmd.target)) return "target: 无效目标";
                const bool extra=def.extraDiscard!=0;
                if(extra && !owned(cmd.discard,Zone::Hand)) return "extra_cost: 必须选择手牌作为额外成本";
                if(!extra && cmd.discard) return "extra_cost: 此法术无需弃牌";
                if(!cost(def.castCost,def.castCost)) return "cost: 魔素或荷载空间不足";
                if(extra) StateMaintenance::leave(s,cmd.discard);
                pay(cmd.card,def.castCost); c.castLoad=def.castCost; c.payments.push_back({pl.ownTurn,def.castCost,def.castCost,false});
                c.targetCard=cmd.target; c.targetPlayer=def.target==TargetKind::Opponent?1-actor:actor;
                s.preparation=Preparation{cmd.card,0,false};
            } else if constexpr(std::is_same_v<T,PlayAction>) {
                if(!owned(cmd.card,Zone::Hand) || def.type!=CardType::Action) return "invalid_action: 行动不可用";
                if(!Rules::targetValid(s,def,actor,cmd.target)) return "target: 无效目标";
                if(!cost(def.cost,def.burden)) return "cost: 魔素或荷载空间不足";
                pay(cmd.card,def.cost); ++pl.actionsPlayed; c.zone=Zone::Action; c.targetCard=cmd.target; c.targetPlayer=def.target==TargetKind::Opponent?1-actor:actor;
                if(def.burden) s.temporary.push_back({s.nextLoad++,actor,cmd.card,def.burden,pl.ownTurn});
                beginEffect(cmd.card,AfterEffect::Action);
            } else if constexpr(std::is_same_v<T,PreloadWord>) {
                if(!owned(cmd.card,Zone::Hand) || def.type!=CardType::Word || inZone(s,actor,Zone::Words).size()>=3) return "word_limit: 无法预置言灵";
                if(!cost(def.cost,def.cost)) return "cost: 魔素或荷载空间不足";
                pay(cmd.card,def.cost); c.zone=Zone::Words; c.analysisLoad=def.cost;
            } else if constexpr(std::is_same_v<T,AttachSeal>) {
                if(!owned(cmd.card,Zone::Hand) || def.type!=CardType::Seal || !owned(cmd.host,Zone::Analysis) || sealCount(s,cmd.host)) return "seal_limit: 宿主不合法或已有符文";
                if(!cost(def.cost,0)) return "cost: 魔素不足";
                pay(cmd.card,def.cost); c.zone=Zone::Attached; c.host=cmd.host;
            } else if constexpr(std::is_same_v<T,RemoveSeal>) {
                if(!owned(cmd.card,Zone::Attached) || pl.sealRemovals) return "seal_limit: 无法主动拆除符文";
                StateMaintenance::leave(s,cmd.card); ++pl.sealRemovals;
                if(hasOverflow(s,catalog_) || Rules::load(s,actor)>Rules::capacity(s,catalog_,actor)) return "capacity: 请先放弃多余法术";
            } else if constexpr(std::is_same_v<T,Abandon>) {
                if((c.zone!=Zone::Analysis && c.zone!=Zone::Casting && c.zone!=Zone::Words) || (def.type!=CardType::Analytic && def.type!=CardType::Word)) return "invalid_abandon: 无法放弃该卡";
                StateMaintenance::leave(s,cmd.card);
            }
            s.events.push_back({"command","玩家 "+std::to_string(actor+1)+" 操作 "+def.name,-1,cmd.card});
            return {};
        }
    },command);
    if(error.empty()) StateMaintenance::check(s,catalog_);
    return error;
}
CommandResult GameEngine::submit(PlayerId actor,const Command& command) {
    GameEngine next=*this;
    const auto start=next.state_.events.size();
    auto error=next.execute(actor,command);
    if(!error.empty()) return {false,error,{},state_.decision,error.substr(0,error.find(':'))};
    next.pump();
    auto failures=Rules::invariants(next.state_,next.catalog_);
    if(!failures.empty()) throw std::logic_error("engine invariant: "+failures.front());
    std::vector<GameEvent> events(next.state_.events.begin()+static_cast<std::ptrdiff_t>(start),next.state_.events.end());
    *this=std::move(next); return {true,{},std::move(events),state_.decision,{}};
}
void GameEngine::enqueuePhase(bool end) {
    auto& s=state_; const auto batch=s.nextBatch++;
    for(int order=0;order<2;++order) {
        int p=order?1-s.active:s.active;
        for(const auto& [id,c]:s.cards) {
            const auto& def=catalog_.at(c.definition);
            const auto& effects=end?def.onEnd:def.onPrepare;
            if(c.owner==p && c.owner==s.active && c.spell==SpellState::Active && c.zone==Zone::Casting && !effects.empty())
                s.queue.append({s.nextTrigger++,batch,p,id,c.targetCard,c.targetPlayer,effects});
        }
    }
}
void GameEngine::finishEffect() {
    auto& s=state_; auto frame=*s.effect;
    auto it=s.cards.find(frame.item.source);
    if(frame.after==AfterEffect::Spell && it!=s.cards.end() && it->second.zone==Zone::Casting) {
        auto& c=it->second; const auto& def=catalog_.at(c.definition);
        for(const auto& [id,seal]:s.cards) if(seal.host==c.id && seal.zone==Zone::Attached && catalog_.at(seal.definition).refundCast && !c.payments.empty()) {
            s.players[c.owner].mana=std::min(12,s.players[c.owner].mana+c.payments.back().paid);
            s.events.push_back({"refund","成功释放：返还本次施法费用",-1,c.id});
        }
        if(def.duration) { c.spell=SpellState::Active; c.remaining=def.duration; }
        else StateMaintenance::leave(s,c.id);
    } else if(frame.after==AfterEffect::Action || frame.after==AfterEffect::Word) StateMaintenance::leave(s,frame.item.source);
    s.effect.reset(); StateMaintenance::check(s,catalog_);
}
void GameEngine::pump() {
    auto& s=state_; std::set<std::string> seen;
    for(int steps=0;s.result==-1 && !s.decision;++steps) {
        if(steps>=10000) throw std::runtime_error("engine automatic step limit exceeded");
        if(!seen.insert(canonicalState()).second) { s.result=2; s.queue.items.clear(); s.effect.reset(); s.preparation.reset(); s.events.push_back({"game_over","强制循环：平局"}); break; }
        // Forced ring reductions finish all associated choices before a state check.
        bool overflow=false;
        for(const auto& [id,c]:s.cards) if(c.zone==Zone::Analysis && catalog_.at(c.definition).type==CardType::Formation && Rules::occupied(s,id)>Rules::rings(s,catalog_,id)) {
            std::vector<CardId> opts; for(const auto& [sid,spell]:s.cards) if(spell.host==id && spell.zone==Zone::Analysis) opts.push_back(sid);
            decision(c.owner,DecisionKind::Overflow,std::move(opts)); overflow=true; break;
        }
        if(overflow) break;
        if(s.effect) {
            auto& f=*s.effect;
            if(f.cursor==f.item.effects.size()) { finishEffect(); continue; }
            const auto e=f.item.effects[f.cursor];
            if(e.kind==EffectKind::ClearTemporary) {
                if(f.clearRemaining<0) f.clearRemaining=e.amount;
                std::vector<std::uint32_t> opts;
                for(const auto& t:s.temporary) if(t.owner==f.item.owner && t.amount>0) opts.push_back(t.id);
                if(!f.clearRemaining || opts.empty()) { ++f.cursor; f.clearRemaining=-1; continue; }
                decision(f.item.owner,DecisionKind::ClearLoad,std::move(opts),true); break;
            }
            EffectResolver::apply(s,catalog_,f.item,e); ++f.cursor; continue;
        }
        if(!s.queue.items.empty()) {
            const auto& first=s.queue.items.front(); std::vector<std::uint32_t> opts;
            for(const auto& t:s.queue.items) if(t.batch==first.batch && t.owner==first.owner) opts.push_back(t.id);
            if(opts.size()>1) { decision(first.owner,DecisionKind::TriggerOrder,std::move(opts)); break; }
            auto t=first; s.queue.items.pop_front(); s.effect=EffectFrame{std::move(t),0,AfterEffect::None,-1}; continue;
        }
        if(s.preparation) {
            auto& prep=*s.preparation; auto& c=s.cards.at(prep.spell);
            if(prep.canceled || c.zone!=Zone::Analysis) { s.preparation.reset(); continue; }
            if(prep.responder<2) {
                int p=prep.responder==0?s.active:1-s.active;
                // First-set words respond to an opponent's preparation only.
                auto opts=p==c.owner?std::vector<CardId>{}:inZone(s,p,Zone::Words);
                if(opts.empty()) { ++prep.responder; continue; }
                decision(p,DecisionKind::Response,std::move(opts),true); break;
            }
            c.zone=Zone::Casting; c.spell=SpellState::Pending; c.host=0;
            s.events.push_back({"prepared","法术进入施法区",-1,c.id}); s.preparation.reset(); continue;
        }
        StateMaintenance::check(s,catalog_); if(s.result!=-1) break;
        auto& pl=s.players[s.active];
        switch(s.flow) {
        case Flow::Draw: {
            s.phase=Phase::Draw; s.flow=Flow::Income;
            if(!(s.active==s.first && pl.ownTurn==1)) { Trigger t; t.owner=s.active; EffectResolver::apply(s,catalog_,t,{EffectKind::Draw,1}); }
            StateMaintenance::check(s,catalog_); break; }
        case Flow::Income: {
            s.phase=Phase::Prepare; s.flow=Flow::Progress; int income=1;
            for(auto id:inZone(s,s.active,Zone::Analysis)) { const auto& d=catalog_.at(s.cards.at(id).definition); if(d.type==CardType::Formation) income+=d.income; }
            pl.mana=std::min(12,pl.mana+income); s.events.push_back({"income","准备阶段收入 "+std::to_string(income)}); break; }
        case Flow::Progress:
            for(auto& [id,c]:s.cards) if(c.owner==s.active && c.spell==SpellState::Analyzing && pl.ownTurn-c.analysisStarted>=catalog_.at(c.definition).analysisTurns) c.spell=SpellState::Ready;
            s.flow=Flow::PrepareTriggers; break;
        case Flow::PrepareTriggers: enqueuePhase(false); s.flow=Flow::Main; break;
        case Flow::Main: s.phase=Phase::Main; return;
        case Flow::Cast: {
            s.phase=Phase::Cast; std::vector<CardId> opts;
            for(auto id:inZone(s,s.active,Zone::Casting)) if(s.cards.at(id).spell==SpellState::Pending) opts.push_back(id);
            if(!opts.empty()) { decision(s.active,DecisionKind::CastOrder,std::move(opts)); return; }
            s.flow=Flow::EndTriggers; break; }
        case Flow::EndTriggers: s.phase=Phase::End; enqueuePhase(true); s.flow=Flow::Expire; break;
        case Flow::Expire:
            s.temporary.erase(std::remove_if(s.temporary.begin(),s.temporary.end(),[&](const TemporaryLoad& t){return t.amount==0 || (t.owner==s.active && t.expiryTurn<=pl.ownTurn);}),s.temporary.end());
            s.flow=Flow::Duration; break;
        case Flow::Duration: {
            std::vector<CardId> expired;
            for(auto& [id,c]:s.cards) if(c.owner==s.active && c.spell==SpellState::Active && --c.remaining==0) expired.push_back(id);
            for(auto id:expired) StateMaintenance::leave(s,id);
            s.flow=Flow::Discard; break; }
        case Flow::Discard: {
            auto hand=inZone(s,s.active,Zone::Hand);
            if(hand.size()>8) { decision(s.active,DecisionKind::Discard,std::move(hand)); return; }
            s.flow=Flow::Finish; break; }
        case Flow::Finish:
            pl.actionsPlayed=0; pl.formations=0; pl.sealRemovals=0;
            for(auto& [id,c]:s.cards) if(c.owner==s.active) c.canceledTurn=-1;
            StateMaintenance::check(s,catalog_); if(s.result!=-1) return;
            s.active=1-s.active; ++s.players[s.active].ownTurn; s.flow=Flow::Draw; break;
        }
    }
}
}
