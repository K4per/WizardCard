#include "wizard/interaction.hpp"
#include <algorithm>
#include <type_traits>

namespace wizard::ui {
void Interaction::update(GameView view) { view_=std::move(view); cancel(); }
void Interaction::cancel() { selected_=0; step_=Step::Inspect; options_.clear(); pending_.reset(); }
bool Interaction::select(CardId card) {
    if(std::none_of(view_.cards.begin(),view_.cards.end(),[&](const CardView& c){return c.instance.id==card;}))return false;
    cancel(); selected_=card; return true;
}
CardId Interaction::target(const Command& cmd) {
    return std::visit([](const auto& c)->CardId {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,StartAnalysis>)return c.formation;
        else if constexpr(std::is_same_v<T,AttachSeal>)return c.host;
        else if constexpr(std::is_same_v<T,SetFormation>)return c.replace;
        else if constexpr(std::is_same_v<T,PrepareCast> || std::is_same_v<T,PlayAction>)return c.target;
        else return 0;
    },cmd);
}
CardId Interaction::costCard(const Command& cmd) { auto c=std::get_if<PrepareCast>(&cmd);return c?c->discard:0; }
std::string Interaction::intent(const LegalAction& a,const std::optional<PendingDecision>& d) {
    return std::visit([&](const auto& c)->std::string {
        using T=std::decay_t<decltype(c)>;
        if constexpr(std::is_same_v<T,SetFormation>)return c.replace?"替换阵法":"设置阵法";
        else if constexpr(std::is_same_v<T,RemoveFormation>)return "拆除阵法";
        else if constexpr(std::is_same_v<T,StartAnalysis>)return "开始解析";
        else if constexpr(std::is_same_v<T,PrepareCast>)return "准备施法";
        else if constexpr(std::is_same_v<T,PlayAction>)return "使用行动";
        else if constexpr(std::is_same_v<T,PreloadWord>)return "预置言灵";
        else if constexpr(std::is_same_v<T,AttachSeal>)return "附加符文";
        else if constexpr(std::is_same_v<T,RemoveSeal>)return "拆除符文";
        else if constexpr(std::is_same_v<T,Abandon>)return "放弃法术";
        else if constexpr(std::is_same_v<T,Advance>)return "进入施法阶段";
        else if constexpr(std::is_same_v<T,Surrender>)return "投降";
        else {
            if(c.option==0)return "跳过 / 停止";
            if(!d)return "确认选择";
            switch(d->kind) {
            case DecisionKind::Response:return "触发言灵";
            case DecisionKind::CastOrder:return "释放法术";
            case DecisionKind::Discard:return "弃置此牌";
            case DecisionKind::Overflow:return "移除多余法术";
            case DecisionKind::TriggerOrder:return a.label;
            case DecisionKind::ClearLoad:return a.label;
            }
            return "确认选择";
        }
    },a.command);
}
std::vector<ActionGroup> Interaction::groups() const {
    std::vector<ActionGroup> groups;
    for(const auto& a:view_.actions)if(a.source && a.source==selected_) {
        auto title=intent(a,view_.decision);
        auto i=std::find_if(groups.begin(),groups.end(),[&](const ActionGroup& g){return g.title==title;});
        if(i==groups.end())groups.push_back({title,{a}});else i->options.push_back(a);
    }
    return groups;
}
bool Interaction::activate(std::size_t index) {
    auto available=groups();if(index>=available.size())return false;
    options_=available[index].options; pending_.reset();
    if(std::any_of(options_.begin(),options_.end(),[](const LegalAction& a){return target(a.command)!=0;}))step_=Step::Target;
    else advance();
    return true;
}
std::vector<CardId> Interaction::candidates() const {
    std::vector<CardId> ids;
    if(step_!=Step::Target && step_!=Step::Cost)return ids;
    for(const auto& a:options_) {
        auto id=step_==Step::Target?target(a.command):costCard(a.command);
        if(id && std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);
    }
    return ids;
}
bool Interaction::pick(CardId card) {
    auto valid=candidates();if(std::find(valid.begin(),valid.end(),card)==valid.end())return false;
    const auto stage=step_;
    options_.erase(std::remove_if(options_.begin(),options_.end(),[&](const LegalAction& a){return (stage==Step::Target?target(a.command):costCard(a.command))!=card;}),options_.end());
    if(stage==Step::Cost) {pending_=options_.front();step_=Step::Confirm;} else advance();
    return true;
}
void Interaction::advance() {
    if(options_.empty())return;
    if(std::any_of(options_.begin(),options_.end(),[](const LegalAction& a){return costCard(a.command)!=0;}))step_=Step::Cost;
    else {pending_=options_.front();step_=Step::Confirm;}
}
void Interaction::offer(const LegalAction& action) {
    // Only current projected actions may be offered, including global/pass decisions.
    auto i=std::find_if(view_.actions.begin(),view_.actions.end(),[&](const LegalAction& a){return a.label==action.label && a.source==action.source && a.target==action.target && a.command.index()==action.command.index();});
    if(i==view_.actions.end())return;
    cancel();pending_=*i;step_=Step::Confirm;
}
void HandOrder::sync(const GameView& view) {
    auto& ids=order_.at(view.viewer);std::vector<CardId> visible;
    for(const auto& c:view.cards)if(c.instance.owner==view.viewer && c.instance.zone==Zone::Hand)visible.push_back(c.instance.id);
    ids.erase(std::remove_if(ids.begin(),ids.end(),[&](CardId id){return std::find(visible.begin(),visible.end(),id)==visible.end();}),ids.end());
    for(auto id:visible)if(std::find(ids.begin(),ids.end(),id)==ids.end())ids.push_back(id);
}
bool HandOrder::moveBefore(PlayerId p,CardId source,CardId before) {
    auto& ids=order_.at(p);auto it=std::find(ids.begin(),ids.end(),source);
    if(it==ids.end() || source==before || (before && std::find(ids.begin(),ids.end(),before)==ids.end()))return false;
    ids.erase(it);ids.insert(before?std::find(ids.begin(),ids.end(),before):ids.end(),source);return true;
}
void HandOrder::sort(const GameView& view,bool byCost) {
    sync(view);std::map<CardId,const CardDefinition*> definitions;
    for(const auto& c:view.cards)if(c.instance.zone==Zone::Hand && c.instance.owner==view.viewer)definitions[c.instance.id]=&c.definition;
    auto& ids=order_.at(view.viewer);
    std::stable_sort(ids.begin(),ids.end(),[&](CardId a,CardId b){
        const auto& x=*definitions.at(a);const auto& y=*definitions.at(b);
        if(byCost && x.cost!=y.cost)return x.cost<y.cost;
        if(x.type!=y.type)return x.type<y.type;
        if(x.cost!=y.cost)return x.cost<y.cost;
        return x.id<y.id;
    });
}
bool Interaction::drop(CardId source,CardId targetCard,Zone destination) {
    if(step_==Step::Confirm)return false;
    if(step_==Step::Cost)return destination==Zone::Ash && pick(source);
    if(step_==Step::Target)return source==selected_ && pick(targetCard);
    Interaction next=*this;if(!next.select(source))return false;
    auto available=next.groups();
    for(std::size_t n=0;n<available.size();++n) {
        bool matches=false;
        for(const auto& a:available[n].options) {
            matches=std::visit([&](const auto& cmd){
                using T=std::decay_t<decltype(cmd)>;
                if constexpr(std::is_same_v<T,StartAnalysis>)return destination==Zone::Analysis && (!targetCard || cmd.formation==targetCard);
                else if constexpr(std::is_same_v<T,SetFormation>)return destination==Zone::Analysis && cmd.replace==targetCard;
                else if constexpr(std::is_same_v<T,AttachSeal>)return targetCard && cmd.host==targetCard;
                else if constexpr(std::is_same_v<T,PreloadWord>)return destination==Zone::Words;
                else if constexpr(std::is_same_v<T,PlayAction>)return destination==Zone::Action;
                else if constexpr(std::is_same_v<T,PrepareCast>)return destination==Zone::Casting || (targetCard && cmd.target==targetCard);
                else if constexpr(std::is_same_v<T,Choose>)return view_.decision && ((destination==Zone::Casting && (view_.decision->kind==DecisionKind::CastOrder || view_.decision->kind==DecisionKind::Response)) || (destination==Zone::Ash && (view_.decision->kind==DecisionKind::Discard || view_.decision->kind==DecisionKind::Overflow)));
                else return false; // Dragging never implicitly abandons or removes a card.
            },a.command);
            if(matches)break;
        }
        if(!matches)continue;
        next.activate(n);
        // Dropping a ready spell anywhere in the casting area starts preparation;
        // existing spells there are not effect targets.
        bool castingArea=destination==Zone::Casting && std::holds_alternative<PrepareCast>(available[n].options.front().command);
        if(targetCard && !castingArea && next.step()==Step::Target && !next.pick(targetCard))continue;
        *this=std::move(next);return true;
    }
    return false;
}

}
