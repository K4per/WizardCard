#include "wizard/core.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard {
namespace {
std::uint32_t random32(GameState &s) {
    auto x = s.rng;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return s.rng = x;
}
std::uint32_t bounded(GameState &s, std::uint32_t n) {
    const auto threshold = (std::uint32_t{0} - n) % n;
    std::uint32_t x;
    do {
        x = random32(s);
    } while (x < threshold);
    return x % n;
}
std::vector<CardId> inZone(const GameState &s, PlayerId p, Zone z) {
    std::vector<CardId> out;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && c.zone == z)
            out.push_back(id);
    return out;
}
int seals(const GameState &s, CardId host) {
    int n = 0;
    for (const auto &[id, c] : s.cards)
        if (c.host == host && c.zone == Zone::Attached)
            ++n;
    return n;
}
int usedBody(const GameState &s, const CardCatalog &cat, PlayerId p) {
    int n = 0;
    for (auto id : inZone(s, p, Zone::Analysis))
        if (cat.at(s.cards.at(id).definition).type == CardType::Formation)
            n += cat.at(s.cards.at(id).definition).body;
    return n;
}
bool hasOverflow(const GameState &s, const CardCatalog &cat) {
    for (const auto &[id, c] : s.cards)
        if (c.zone == Zone::Analysis && cat.at(c.definition).type == CardType::Formation &&
            Rules::occupied(s, id) > Rules::rings(s, cat, id))
            return true;
    return false;
}
bool cardTarget(TargetKind t) {
    return t == TargetKind::EmptyEnemyFormation || t == TargetKind::EnemyCard || t == TargetKind::OwnCard ||
           t == TargetKind::AnyCard || t == TargetKind::OwnAnalyzingSpell;
}
std::vector<CardId> targetCards(CardId target, const std::vector<CardId> &targets) {
    return targets.empty() ? (target ? std::vector<CardId>{target} : std::vector<CardId>{}) : targets;
}
bool validTargets(const GameState &s, TargetKind kind, int count, PlayerId p, CardId target,
                  const std::vector<CardId> &targets) {
    auto ids = targetCards(target, targets);
    if (!cardTarget(kind))
        return target == 0 && targets.empty() && kind != TargetKind::PendingLink &&
               kind != TargetKind::PreparationRoot && kind != TargetKind::EnemySpellLink;
    if (count < 1 || count > 5 || ids.size() != static_cast<std::size_t>(count) || ids.front() != target)
        return false;
    std::set<CardId> seen;
    for (auto id : ids)
        if (!seen.insert(id).second || !Rules::targetValid(s, kind, p, id))
            return false;
    return true;
}
std::vector<std::vector<CardId>> sets(const GameState &s, TargetKind kind, int count, PlayerId p) {
    if (!cardTarget(kind))
        return {{}};
    std::vector<CardId> ids;
    for (const auto &[id, c] : s.cards)
        if (Rules::targetValid(s, kind, p, id))
            ids.push_back(id);
    std::vector<std::vector<CardId>> out;
    std::vector<CardId> selected;
    auto visit = [&](auto &&self, std::size_t pos) -> void {
        if (selected.size() == static_cast<std::size_t>(count)) {
            out.push_back(selected);
            return;
        }
        for (std::size_t i = pos; i < ids.size(); ++i) {
            selected.push_back(ids[i]);
            self(self, i + 1);
            selected.pop_back();
        }
    };
    if (count > 0 && count <= 5)
        visit(visit, 0);
    return out;
}
const ResponseAbility *ability(const CardDefinition &d, const std::string &id) {
    for (const auto &a : d.responses)
        if (a.id == id)
            return &a;
    return nullptr;
}
MatchConfig shared(std::array<std::vector<std::string>, 2> decks, std::uint32_t seed) {
    MatchConfig c;
    c.seed = seed;
    for (int p = 0; p < 2; ++p)
        c.players[p].cards = std::move(decks[p]);
    return c;
}
Trigger snapshot(const CardInstance &c, const CardDefinition &d) {
    Trigger t;
    t.owner = c.owner;
    t.source = c.id;
    t.targetCard = c.targetCard;
    t.targetPlayer = c.targetPlayer;
    t.target = d.target;
    t.targetCards = c.targets;
    t.effects = d.effects;
    return t;
}
} // namespace
GameEngine::GameEngine(CardCatalog cat, std::array<std::vector<std::string>, 2> decks, std::uint32_t seed)
    : GameEngine(std::move(cat), shared(std::move(decks), seed)) {}
GameEngine::GameEngine(CardCatalog cat, const MatchConfig &config) : catalog_(std::move(cat)) {
    for (const auto &deck : config.players) {
        auto errors = Rules::deckErrors(catalog_, deck);
        if (!errors.empty())
            throw std::invalid_argument(errors.front());
    }
    state_.rng = config.seed ? config.seed : 0x9e3779b9u;
    CardId id = 1;
    for (int p = 0; p < 2; ++p) {
        CardInstance base;
        base.id = id++;
        base.definition = config.players[p].baseFormation;
        base.owner = p;
        base.base = true;
        base.zone = Zone::Analysis;
        state_.cards.emplace(base.id, base);
        for (const auto &key : config.players[p].cards) {
            CardInstance c;
            c.id = id++;
            c.owner = p;
            c.definition = key;
            state_.cards.emplace(c.id, c);
            state_.players[p].deck.push_back(c.id);
        }
        auto &deck = state_.players[p].deck;
        for (std::size_t i = deck.size(); i > 1; --i)
            std::swap(deck[i - 1], deck[bounded(state_, static_cast<std::uint32_t>(i))]);
        Trigger t;
        t.owner = p;
        EffectResolver::apply(state_, catalog_, t, {EffectKind::Draw, 5});
    }
    state_.first = state_.active = static_cast<int>(bounded(state_, 2));
    state_.players[state_.active].ownTurn = 1;
    pump();
}
GameEngine GameEngine::scenario(CardCatalog cat, GameState state) {
    GameEngine e;
    e.catalog_ = std::move(cat);
    e.state_ = std::move(state);
    if (e.state_.phase == Phase::Main && e.state_.flow == Flow::Main &&
        e.state_.phaseStep == PhaseStep::Enter)
        e.state_.phaseStep = PhaseStep::Body;
    if (e.state_.decision)
        e.state_.nextDecision = std::max(e.state_.nextDecision, e.state_.decision->id + 1);
    if (e.state_.phaseStep == PhaseStep::Body && !e.state_.decision && !e.state_.chain && !e.state_.effect &&
        !e.state_.phaseGate)
        e.state_.phaseGate = e.state_.nextDecision++;
    auto errors = Rules::invariants(e.state_, e.catalog_);
    if (!errors.empty())
        throw std::invalid_argument("invalid scenario: " + errors.front());
    return e;
}
void GameEngine::decision(PlayerId p, DecisionKind k, std::vector<std::uint32_t> opts, bool pass) {
    state_.decision = PendingDecision{state_.nextDecision++, p, k, std::move(opts), pass};
}
void GameEngine::startChain(ResponseWindow window, ChainLink root, PlayerId priority) {
    auto &s = state_;
    if (s.chain)
        throw std::logic_error("nested chain");
    if (root.item.source) {
        const auto &d = catalog_.at(s.cards.at(root.item.source).definition);
        root.speed = catalog_.advancedRules ? d.speed : 0;
        const auto &source = s.cards.at(root.item.source);
        root.castCost =
            root.kind == LinkKind::Preparation ? root.preparationLoad
            : root.kind == LinkKind::Spell && d.type == CardType::Analytic && !source.payments.empty()
                ? source.payments.back().paid
            : root.kind == LinkKind::Spell && d.type == CardType::Word
                ? source.settingPaid + (source.payments.empty() ? 0 : source.payments.back().paid)
                : Rules::castCost(s, catalog_, root.item.source);
    }
    root.id = s.nextLink++;
    ChainState chain;
    chain.id = s.nextChain++;
    chain.window = window;
    chain.initiator = root.item.owner;
    chain.priority = priority;
    chain.links.push_back(std::move(root));
    s.chain = std::move(chain);
    s.phaseGate = 0;
    s.events.push_back({"chain_open", "建立连锁：" + windowName(window)});
    s.events.push_back({"link_declared", "宣告链节 #" + std::to_string(s.chain->links.back().id), -1,
                        s.chain->links.back().item.source});
}
void GameEngine::beginEffect(CardId id, AfterEffect after) {
    const auto &c = state_.cards.at(id);
    const auto &d = catalog_.at(c.definition);
    ChainLink root;
    root.kind = after == AfterEffect::Action ? LinkKind::Action : LinkKind::Spell;
    root.item = snapshot(c, d);
    if (after != AfterEffect::Action)
        for (const auto &[sid, seal] : state_.cards)
            if (seal.zone == Zone::Attached && seal.host == id && catalog_.at(seal.definition).damageBonus)
                root.item.effects.push_back({EffectKind::Damage, catalog_.at(seal.definition).damageBonus,
                                             DamageType::Force, EffectRecipient::Opponent});
    if (after == AfterEffect::Spell && !c.payments.empty())
        root.paymentIndex = static_cast<int>(c.payments.size()) - 1;
    root.item.requiresSource = after == AfterEffect::Spell;
    root.paid = after == AfterEffect::Action ? d.cost : 0;
    root.addedLoad = after == AfterEffect::Action ? d.burden : 0;
    startChain(after == AfterEffect::Action ? ResponseWindow::Action : ResponseWindow::Cast, std::move(root),
               1 - c.owner);
}
void GameEngine::startTrigger(Trigger t) {
    ChainLink root;
    root.kind = LinkKind::Trigger;
    root.item = std::move(t);
    const int priority = 1 - root.item.owner;
    startChain(ResponseWindow::Trigger, std::move(root), priority);
}
void GameEngine::passResponse() {
    auto &c = *state_.chain;
    state_.events.push_back({"response_pass", "玩家 " + std::to_string(c.priority + 1) + " 放弃响应"});
    c.priority = 1 - c.priority;
    if (++c.passes == 2) {
        c.mode = ChainMode::Resolving;
        state_.events.push_back({"chain_resolve", "双方连续放弃，逆序结算"});
    }
}
std::string GameEngine::validateResponse(PlayerId actor, const Respond &cmd) const {
    const auto &s = state_;
    if (!s.chain || s.chain->mode != ChainMode::Building || actor != s.chain->priority)
        return "priority: 当前没有响应优先权";
    auto i = s.cards.find(cmd.card);
    if (i == s.cards.end() || i->second.owner != actor)
        return "invalid_card: 无效响应来源";
    const auto &c = i->second;
    const auto &d = catalog_.at(c.definition);
    auto a = ability(d, cmd.ability);
    if (!a)
        return "ability: 未声明该响应能力";
    const bool hand = c.zone == Zone::Hand;
    const bool ambush = c.faceDown && a->fromAmbush && c.ambushedTurn < s.globalTurn;
    const bool analytic = !c.faceDown && c.zone == Zone::Analysis && c.spell == SpellState::Ready &&
                          a->fromAnalysis && c.canceledTurn != s.players[actor].ownTurn;
    if (catalog_.advancedRules) {
        const auto top = s.chain->links.back().speed;
        if (hand || d.speed == 1 || top == 4 || d.speed < top)
            return "spell_speed: 当前速度或来源不能加入连锁";
        if (d.speed == 2 && s.phase != Phase::Main && s.phase != Phase::Cast)
            return "spell_speed: 二速仅可在主要或施法阶段响应";
        if (!a->phases.empty() && std::find(a->phases.begin(), a->phases.end(), s.phase) == a->phases.end())
            return "response_phase: 不符合响应阶段";
    }
    if (!((hand && a->fromHand) ||
          (!c.faceDown && c.zone == Zone::Words && d.type == CardType::Word && a->fromWords) || ambush ||
          analytic))
        return "response_source: 当前区域不允许响应";
    if (std::find(s.chain->responded.begin(), s.chain->responded.end(), cmd.card) != s.chain->responded.end())
        return "response_used: 本连锁已使用该实体";
    if (std::find(a->windows.begin(), a->windows.end(), s.chain->window) == a->windows.end() ||
        (a->eventOwner == EventOwner::Self && actor != s.chain->initiator) ||
        (a->eventOwner == EventOwner::Opponent && actor == s.chain->initiator))
        return "response_window: 不符合响应时机";
    if (a->target == TargetKind::PendingLink || a->target == TargetKind::PreparationRoot ||
        a->target == TargetKind::EnemySpellLink) {
        if (cmd.target || !cmd.targets.empty() || !Rules::linkTargetValid(s, a->target, cmd.link))
            return "target: 无效链节目标";
        if (a->target == TargetKind::EnemySpellLink) {
            const auto l = std::find_if(s.chain->links.begin(), s.chain->links.end(),
                                        [&](const ChainLink &x) { return x.id == cmd.link; });
            if (l == s.chain->links.end() || l->item.owner == actor || !l->item.source ||
                (catalog_.at(s.cards.at(l->item.source).definition).type != CardType::Analytic &&
                 catalog_.at(s.cards.at(l->item.source).definition).type != CardType::Word))
                return "target: 必须指定对手释放中的法术";
        }
    } else if (cmd.link || !validTargets(s, a->target, a->targetCount, actor, cmd.target, cmd.targets))
        return "target: 无效响应目标";
    auto discard = s.cards.find(cmd.discard);
    if (a->extraDiscard) {
        if (a->extraDiscard != 1 || cmd.discard == cmd.card || discard == s.cards.end() ||
            discard->second.owner != actor || discard->second.zone != Zone::Hand)
            return "extra_cost: 需要另一张手牌";
    } else if (cmd.discard)
        return "extra_cost: 无需弃牌";
    const int paid =
        analytic ? Rules::castCost(s, catalog_, c.id)
        : ambush ? d.cost
        : hand
            ? a->handCost
            : a->preloadedCost + (d.type == CardType::Word ? Rules::castCost(s, catalog_, c.id) - d.cost : 0);
    const int bound = analytic || d.type == CardType::Word ? paid : 0;
    if (paid < 0 || a->burden < 0 || s.players[actor].mana < paid ||
        Rules::load(s, actor) + bound + a->burden > Rules::capacity(s, catalog_, actor))
        return "cost: 魔素或荷载空间不足";
    return {};
}
std::vector<Respond> GameEngine::responsesFor(PlayerId p, DecisionId decisionId) const {
    std::vector<Respond> out;
    const auto &s = state_;
    if (!s.chain || s.chain->mode != ChainMode::Building || s.chain->priority != p)
        return out;
    for (const auto &[id, c] : s.cards)
        if (c.owner == p && (c.zone == Zone::Hand || c.zone == Zone::Words || c.zone == Zone::Analysis))
            for (const auto &a : catalog_.at(c.definition).responses) {
                if ((c.zone == Zone::Hand && !a.fromHand) ||
                    std::find(a.windows.begin(), a.windows.end(), s.chain->window) == a.windows.end())
                    continue;
                std::vector<LinkId> links{0};
                if (a.target == TargetKind::PendingLink || a.target == TargetKind::PreparationRoot ||
                    a.target == TargetKind::EnemySpellLink) {
                    links.clear();
                    for (const auto &l : s.chain->links)
                        if (Rules::linkTargetValid(s, a.target, l.id))
                            links.push_back(l.id);
                }
                auto targets = sets(s, a.target, a.targetCount, p);
                std::vector<CardId> discards{0};
                if (a.extraDiscard) {
                    discards = inZone(s, p, Zone::Hand);
                    discards.erase(std::remove(discards.begin(), discards.end(), id), discards.end());
                }
                for (auto link : links)
                    for (const auto &ids : targets)
                        for (auto discard : discards) {
                            Respond r{decisionId, id,      a.id, ids.empty() ? 0 : ids.front(),
                                      link,       discard, ids};
                            if (validateResponse(p, r).empty())
                                out.push_back(std::move(r));
                        }
            }
    return out;
}
std::string GameEngine::prepare(PlayerId actor, const PrepareCast &cmd, bool deferred) {
    auto &s = state_;
    auto i = s.cards.find(cmd.card);
    if (i == s.cards.end() || i->second.owner != actor)
        return "invalid_card: 无效法术";
    auto &c = i->second;
    const auto &def = catalog_.at(c.definition);
    auto &pl = s.players[actor];
    if (def.type != CardType::Analytic || c.zone != Zone::Analysis || c.spell != SpellState::Ready ||
        c.canceledTurn == pl.ownTurn)
        return "not_ready: 法术未就绪或本回合已被取消";
    if (!deferred && cmd.decision)
        return "stale_decision: 效果选择已过期";
    for (const auto &pending : s.deferredPreparations)
        if (pending.item.source == cmd.card)
            return "not_ready: 法术已经宣告准备";
    if (s.chain)
        for (const auto &link : s.chain->links)
            if (link.kind == LinkKind::Preparation && link.item.source == cmd.card)
                return "not_ready: 法术已经宣告准备";
    if (!validTargets(s, def.target, def.targetCount, actor, cmd.target, cmd.targets))
        return "target: 无效目标";
    auto discard = s.cards.find(cmd.discard);
    if ((def.extraDiscard && (discard == s.cards.end() || discard->second.owner != actor ||
                              discard->second.zone != Zone::Hand)) ||
        (!def.extraDiscard && cmd.discard))
        return "extra_cost: 额外成本不合法";
    const int castFee = Rules::castCost(s, catalog_, c.id);
    if (pl.mana < castFee || Rules::load(s, actor) + castFee > Rules::capacity(s, catalog_, actor))
        return "cost: 魔素或荷载不足";
    if (cmd.discard)
        StateMaintenance::leave(s, cmd.discard, &catalog_);
    pl.mana -= castFee;
    s.events.push_back({"pay", "准备支付 " + std::to_string(castFee) + " 魔素", -1, c.id, castFee});
    c.castLoad += castFee;
    c.payments.push_back({pl.ownTurn, castFee, castFee, false});
    c.targetCard = cmd.target;
    c.targets = targetCards(cmd.target, cmd.targets);
    c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
    ChainLink root;
    root.kind = LinkKind::Preparation;
    root.speed = catalog_.advancedRules ? def.speed : 0;
    root.castCost = castFee;
    root.item = snapshot(c, def);
    root.item.effects.clear();
    root.item.requiresSource = true;
    root.paymentIndex = static_cast<int>(c.payments.size()) - 1;
    root.preparationLoad = castFee;
    root.paid = castFee;
    root.addedLoad = castFee;
    root.discarded = cmd.discard;
    if (deferred) {
        s.deferredPreparations.push_back(std::move(root));
        s.events.push_back({"prepare_deferred", "效果宣告准备；当前连锁结束后开放响应", -1, c.id});
    } else
        startChain(ResponseWindow::Prepare, std::move(root), 1 - actor);
    s.phaseGate = 0;
    return {};
}
std::vector<PrepareCast> GameEngine::preparationsFor(PlayerId actor, DecisionId id) const {
    std::vector<PrepareCast> out;
    for (const auto &[cid, c] : state_.cards)
        if (c.owner == actor && c.zone == Zone::Analysis && c.spell == SpellState::Ready) {
            const auto &def = catalog_.at(c.definition);
            std::vector<CardId> discards{0};
            if (def.extraDiscard)
                discards = inZone(state_, actor, Zone::Hand);
            for (const auto &targets : sets(state_, def.target, def.targetCount, actor))
                for (auto discard : discards) {
                    PrepareCast cmd{cid, targets.empty() ? 0 : targets.front(), discard, targets, id};
                    auto trial = *this;
                    if (trial.prepare(actor, cmd, true).empty())
                        out.push_back(std::move(cmd));
                }
        }
    return out;
}
std::string GameEngine::execute(PlayerId actor, const Command &command) {
    auto &s = state_;
    if (actor < 0 || actor > 1)
        return "invalid_player: 无效玩家";
    if (s.result != -1)
        return "game_over: 对局已经结束";
    if (std::holds_alternative<Surrender>(command)) {
        s.players[actor].surrendered = true;
        StateMaintenance::check(s, catalog_);
        return {};
    }
    if (s.decision) {
        const auto d = *s.decision;
        if (actor != d.player)
            return "decision_mismatch: 请完成当前玩家的选择";
        if (d.kind == DecisionKind::Response) {
            Command selected = command;
            if (auto old = std::get_if<Choose>(&selected)) {
                if (old->decision != d.id)
                    return "stale_decision: 选择已过期";
                if (!old->option)
                    selected = PassResponse{d.id};
                else {
                    auto candidates = responsesFor(actor, d.id);
                    candidates.erase(std::remove_if(candidates.begin(), candidates.end(),
                                                    [&](const Respond &r) { return r.card != old->option; }),
                                     candidates.end());
                    if (candidates.size() != 1)
                        return "ambiguous_response: 请指定响应能力与目标";
                    selected = candidates.front();
                }
            }
            if (auto pass = std::get_if<PassResponse>(&selected)) {
                if (pass->decision != d.id)
                    return "stale_decision: 选择已过期";
                s.decision.reset();
                passResponse();
                return {};
            }
            auto r = std::get_if<Respond>(&selected);
            if (!r || r->decision != d.id)
                return "decision_mismatch: 请提交当前响应";
            auto error = validateResponse(actor, *r);
            if (!error.empty())
                return error;
            auto &c = s.cards.at(r->card);
            const auto &def = catalog_.at(c.definition);
            const auto a = *ability(def, r->ability);
            const bool hand = c.zone == Zone::Hand;
            const bool analytic = c.zone == Zone::Analysis && !c.faceDown;
            const bool ambush = c.faceDown;
            const int paid =
                analytic ? Rules::castCost(s, catalog_, c.id)
                : ambush ? def.cost
                : hand   ? a.handCost
                         : a.preloadedCost +
                             (def.type == CardType::Word ? Rules::castCost(s, catalog_, c.id) - def.cost : 0);
            const int bound = analytic || def.type == CardType::Word ? paid : 0;
            s.players[actor].mana -= paid;
            if (r->discard)
                StateMaintenance::leave(s, r->discard, &catalog_);
            if (a.burden)
                s.temporary.push_back(
                    {s.nextLoad++, actor, c.id, a.burden, s.players[actor].ownTurn + (actor != s.active)});
            if ((hand || ambush) && def.type == CardType::Word)
                c.settingPaid = paid;
            c.castLoad += bound;
            c.zone = analytic ? Zone::Casting : Zone::Resolving;
            c.faceDown = false;
            c.host = 0;
            c.spell = analytic ? SpellState::Pending : SpellState::None;
            c.targetCard = r->target;
            c.targets = targetCards(r->target, r->targets);
            c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
            c.payments.push_back({s.players[actor].ownTurn, paid, bound, false});
            ChainLink link;
            link.id = s.nextLink++;
            link.kind = analytic ? LinkKind::Spell : LinkKind::Response;
            link.speed = catalog_.advancedRules ? def.speed : 0;
            link.castCost = Rules::castCost(s, catalog_, c.id);
            link.ability = a.id;
            link.paymentIndex = static_cast<int>(c.payments.size()) - 1;
            link.paid = paid;
            link.addedLoad = bound + a.burden;
            link.discarded = r->discard;
            link.item.owner = actor;
            link.item.source = c.id;
            link.item.targetCard = r->target;
            link.item.targetCards = targetCards(r->target, r->targets);
            link.item.targetPlayer = a.target == TargetKind::Opponent ? 1 - actor : actor;
            link.item.effects = a.effects;
            for (const auto &[sid, seal] : s.cards)
                if (seal.zone == Zone::Attached && seal.host == c.id &&
                    catalog_.at(seal.definition).damageBonus)
                    link.item.effects.push_back({EffectKind::Damage, catalog_.at(seal.definition).damageBonus,
                                                 DamageType::Force, EffectRecipient::Opponent});
            link.item.target = a.target;
            link.item.targetLink = r->link;
            link.item.requiresSource = a.requiresSource;
            s.chain->links.push_back(std::move(link));
            s.chain->responded.push_back(c.id);
            s.chain->passes = 0;
            s.chain->priority = 1 - actor;
            s.decision.reset();
            s.events.push_back({"pay", "响应支付 " + std::to_string(paid) + " 魔素", -1, c.id, paid});
            s.events.push_back(
                {"link_declared", "响应链节 #" + std::to_string(s.chain->links.back().id), -1, c.id});
            StateMaintenance::check(s, catalog_);
            return {};
        }
        if (d.kind == DecisionKind::EffectPrepare && std::holds_alternative<PrepareCast>(command)) {
            const auto &cmd = std::get<PrepareCast>(command);
            if (cmd.decision != d.id)
                return "stale_decision: 效果选择已过期";
            auto error = prepare(actor, cmd, true);
            if (!error.empty())
                return error;
            s.decision.reset();
            ++s.effect->cursor;
            return {};
        }
        auto choose = std::get_if<Choose>(&command);
        if (!choose || choose->decision != d.id)
            return "decision_mismatch: 请完成当前选择";
        auto opt = choose->option;
        if (d.kind == DecisionKind::EffectPrepare && opt)
            return "decision_mismatch: 请指定完整的准备施法操作";
        if (!(opt == 0 && d.mayPass) && std::find(d.options.begin(), d.options.end(), opt) == d.options.end())
            return "invalid_option: 无效选择";
        s.decision.reset();
        s.events.push_back({"choice", "玩家 " + std::to_string(actor + 1) + " 完成选择"});
        switch (d.kind) {
        case DecisionKind::CastOrder:
            if (opt)
                beginEffect(opt, AfterEffect::Spell);
            else {
                s.phaseStep = PhaseStep::EndWindow;
                s.phaseGate = 0;
                s.events.push_back({"cast_pass", "本阶段不再释放；待释放法术及已支付荷载保留"});
            }
            break;
        case DecisionKind::TriggerOrder: {
            auto i = std::find_if(s.queue.items.begin(), s.queue.items.end(),
                                  [&](const Trigger &t) { return t.id == opt; });
            auto t = *i;
            s.queue.items.erase(i);
            startTrigger(std::move(t));
            break;
        }
        case DecisionKind::Discard:
            StateMaintenance::leave(s, opt, &catalog_);
            StateMaintenance::check(s, catalog_);
            break;
        case DecisionKind::Overflow:
            StateMaintenance::leave(s, opt, &catalog_);
            break;
        case DecisionKind::ClearLoad:
            if (!opt) {
                ++s.effect->cursor;
                s.effect->clearRemaining = -1;
            } else {
                auto i = std::find_if(s.temporary.begin(), s.temporary.end(),
                                      [&](const TemporaryLoad &t) { return t.id == opt; });
                --i->amount;
                --s.effect->clearRemaining;
                s.events.push_back({"load", "移除1点临时荷载"});
            }
            break;
        case DecisionKind::SearchDeck: {
            auto &deck = s.players[actor].deck;
            deck.erase(std::find(deck.begin(), deck.end(), opt));
            s.cards.at(opt).zone = Zone::Hand;
            s.events.push_back({"search", "检索一张行动卡加入手牌", actor, opt});
            for (std::size_t i = deck.size(); i > 1; --i)
                std::swap(deck[i - 1], deck[bounded(s, static_cast<std::uint32_t>(i))]);
            ++s.effect->cursor;
            break;
        }
        case DecisionKind::DestroyAmbush:
        case DecisionKind::DestroyOwnFormation:
            if (opt)
                StateMaintenance::leave(s, opt, &catalog_);
            ++s.effect->cursor;
            break;
        case DecisionKind::EffectDiscard:
            StateMaintenance::leave(s, opt, &catalog_);
            ++s.effect->cursor;
            break;
        case DecisionKind::EffectPrepare:
            ++s.effect->cursor; // explicit decline, no declaration
            break;
        case DecisionKind::Concentration: {
            CardId source = 0;
            for (const auto &[id, c] : s.cards)
                if (c.owner == actor && c.zone == Zone::Casting && c.spell == SpellState::Active &&
                    catalog_.at(c.definition).concentration)
                    source = id;
            auto &c = s.cards.at(source);
            if (opt) {
                const int paid = c.concentrationCost;
                s.players[actor].mana -= paid;
                c.castLoad += paid;
                c.payments.push_back({s.players[actor].ownTurn, paid, paid, false});
                s.events.push_back({"concentration", "维持专注：支付 " + std::to_string(paid) + " 魔素及荷载",
                                    -1, source, paid});
            } else {
                StateMaintenance::leave(s, source, &catalog_);
                StateMaintenance::check(s, catalog_);
            }
            break;
        }
        case DecisionKind::Response:
            break;
        }
        return {};
    }
    if (std::holds_alternative<Choose>(command) || std::holds_alternative<Respond>(command) ||
        std::holds_alternative<PassResponse>(command))
        return "stale_decision: 选择已过期";
    if (std::holds_alternative<AdvancePhase>(command) || std::holds_alternative<Advance>(command)) {
        if (std::holds_alternative<Advance>(command) && s.phase != Phase::Main)
            return "wrong_phase: 旧推进命令仅适用于主要阶段";
        auto gate = std::get_if<AdvancePhase>(&command);
        if (actor != s.active || s.phaseStep != PhaseStep::Body || !s.phaseGate ||
            (gate && gate->gate != s.phaseGate) || s.chain || s.effect || !s.queue.items.empty())
            return "phase_gate: 当前阶段不能结束";
        s.phaseGate = 0;
        s.phaseStep = PhaseStep::EndWindow;
        return {};
    }
    if (const auto *cmd = std::get_if<ActivateSpell>(&command)) {
        if (!catalog_.advancedRules || actor != s.active || s.phaseStep != PhaseStep::Body || s.chain ||
            s.effect || !s.queue.items.empty())
            return "wrong_phase: 当前不能主动释放";
        auto it = s.cards.find(cmd->card);
        if (it == s.cards.end() || it->second.owner != actor)
            return "invalid_card";
        auto &c = it->second;
        const auto &d = catalog_.at(c.definition);
        if (c.faceDown || d.responseOnly || d.extraDiscard ||
            (d.type == CardType::Analytic && c.canceledTurn == s.players[actor].ownTurn) ||
            (d.speed <= 2 && s.phase != Phase::Main && s.phase != Phase::Cast) ||
            !((d.type == CardType::Word && c.zone == Zone::Words) ||
              (d.type == CardType::Analytic && c.zone == Zone::Analysis && c.spell == SpellState::Ready &&
               s.phase == Phase::Cast)))
            return "not_ready: 法术不能在此时释放";
        if (!validTargets(s, d.target, d.targetCount, actor, cmd->target, cmd->targets))
            return "target: 无效目标";
        const int fee = Rules::castCost(s, catalog_, c.id) - (d.type == CardType::Word ? d.cost : 0);
        if (s.players[actor].mana < fee || Rules::load(s, actor) + fee > Rules::capacity(s, catalog_, actor))
            return "cost: 魔素或荷载不足";
        s.players[actor].mana -= fee;
        c.castLoad += fee;
        c.payments.push_back({s.players[actor].ownTurn, fee, fee, false});
        s.events.push_back({"pay", "释放支付 " + std::to_string(fee) + " 魔素", -1, c.id, fee});
        c.zone = Zone::Casting;
        c.spell = SpellState::Pending;
        c.host = 0;
        c.targetCard = cmd->target;
        c.targets = targetCards(cmd->target, cmd->targets);
        c.targetPlayer = d.target == TargetKind::Opponent ? 1 - actor : actor;
        beginEffect(c.id, AfterEffect::Spell);
        return {};
    }
    if (const auto *cmd = std::get_if<FlipAmbush>(&command)) {
        if (!catalog_.advancedRules || actor != s.active || s.phase != Phase::Main ||
            s.phaseStep != PhaseStep::Body || s.chain || s.effect || !s.queue.items.empty())
            return "wrong_phase: 当前不能反转";
        auto it = s.cards.find(cmd->card);
        if (it == s.cards.end() || it->second.owner != actor || !it->second.faceDown ||
            it->second.ambushedTurn >= s.globalTurn)
            return "ambush_turn: 埋伏卡须等到下一个全局回合才能反转";
        auto &c = it->second;
        const auto &d = catalog_.at(c.definition);
        const auto host = c.host;
        if (d.responseOnly)
            return "response_window: 该卡须在合法响应时点反转";
        c.faceDown = false;
        c.zone = Zone::Hand;
        c.host = 0;
        s.events.push_back({"reveal", "反转 " + d.name, -1, c.id});
        if (d.type == CardType::Analytic)
            return execute(actor, StartAnalysis{c.id, host});
        if (d.type == CardType::Action)
            return execute(actor, PlayAction{c.id, cmd->target, cmd->targets});
        auto error = execute(actor, PreloadWord{c.id, d.immediate});
        if (!error.empty() || d.immediate)
            return error;
        return execute(actor, ActivateSpell{c.id, cmd->target, cmd->targets});
    }
    if (actor != s.active || s.phase != Phase::Main || s.phaseStep != PhaseStep::Body || s.chain ||
        s.effect || !s.queue.items.empty())
        return "wrong_phase: 当前不能执行主要阶段操作";
    auto &pl = s.players[actor];
    auto owned = [&](CardId id, Zone z) {
        auto i = s.cards.find(id);
        return i != s.cards.end() && i->second.owner == actor && i->second.zone == z;
    };
    auto cost = [&](int mana, int load) {
        return pl.mana >= mana && Rules::load(s, actor) + load <= Rules::capacity(s, catalog_, actor);
    };
    auto pay = [&](CardId id, int mana) {
        pl.mana -= mana;
        s.events.push_back({"pay", "支付 " + std::to_string(mana) + " 魔素", -1, id, mana});
    };
    auto error = std::visit(
        [&](const auto &cmd) -> std::string {
            using T = std::decay_t<decltype(cmd)>;
            if constexpr (std::is_same_v<T, Advance> || std::is_same_v<T, AdvancePhase> ||
                          std::is_same_v<T, Choose> || std::is_same_v<T, PassResponse> ||
                          std::is_same_v<T, Respond> || std::is_same_v<T, Surrender> ||
                          std::is_same_v<T, FlipAmbush> || std::is_same_v<T, ActivateSpell>)
                return "invalid_command";
            else {
                auto i = s.cards.find(cmd.card);
                if (i == s.cards.end() || i->second.owner != actor)
                    return "invalid_card: 不是自己的卡牌";
                auto &c = i->second;
                const auto &def = catalog_.at(c.definition);
                if constexpr (std::is_same_v<T, SetAmbush>) {
                    if (!catalog_.advancedRules || !owned(cmd.card, Zone::Hand) ||
                        (def.type != CardType::Analytic && def.type != CardType::Word &&
                         def.type != CardType::Action))
                        return "invalid_ambush: 只有法术或行动卡可埋伏";
                    if (!cost(1, 0))
                        return "cost: 埋伏需要1魔素";
                    if (def.type == CardType::Analytic) {
                        if (!owned(cmd.formation, Zone::Analysis) ||
                            catalog_.at(s.cards.at(cmd.formation).definition).type != CardType::Formation ||
                            def.rank > Rules::maxRank(s, catalog_, cmd.formation) ||
                            Rules::occupied(s, cmd.formation) >= Rules::rings(s, catalog_, cmd.formation))
                            return "ring_or_rank";
                        c.zone = Zone::Analysis;
                        c.host = cmd.formation;
                        c.sourceFormation = cmd.formation;
                    } else {
                        if (cmd.formation || inZone(s, actor, Zone::Words).size() >= 3)
                            return "word_limit";
                        c.zone = Zone::Words;
                    }
                    pay(c.id, 1);
                    c.faceDown = true;
                    c.ambushedTurn = s.globalTurn;
                    c.spell = SpellState::None;
                    s.events.push_back({"ambush", "埋伏一张卡", -1, c.id});
                    return {};
                } else if constexpr (std::is_same_v<T, SetFormation>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Formation || pl.formations)
                        return "formation_limit: 阵法操作不可用";
                    if (cmd.replace) {
                        if (!owned(cmd.replace, Zone::Analysis) || s.cards.at(cmd.replace).base ||
                            catalog_.at(s.cards.at(cmd.replace).definition).type != CardType::Formation ||
                            pl.mana < 2)
                            return "invalid_replacement: 无法替换";
                        StateMaintenance::leave(s, cmd.replace, &catalog_);
                        pay(cmd.card, 2);
                    }
                    if (catalog_.advancedRules) {
                        if (pl.mana < def.cost)
                            return "cost: 阵法设置魔素不足";
                        pay(c.id, def.cost);
                        c.settingPaid = def.cost;
                    }
                    c.zone = Zone::Analysis;
                    ++pl.formations;
                    if (usedBody(s, catalog_, actor) > 5 ||
                        Rules::load(s, actor) > Rules::capacity(s, catalog_, actor))
                        return "capacity: 空间或荷载不合法";
                } else if constexpr (std::is_same_v<T, RemoveFormation>) {
                    if (!owned(cmd.card, Zone::Analysis) || c.base || def.type != CardType::Formation ||
                        pl.formations || pl.mana < 2)
                        return "invalid_removal: 无法拆除阵法";
                    StateMaintenance::leave(s, cmd.card, &catalog_);
                    pay(cmd.card, 2);
                    ++pl.formations;
                    if (Rules::load(s, actor) > Rules::capacity(s, catalog_, actor))
                        return "overload: 拆除后超载";
                } else if constexpr (std::is_same_v<T, StartAnalysis>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Analytic ||
                        !owned(cmd.formation, Zone::Analysis))
                        return "invalid_analysis: 无效解析对象";
                    const auto &host = catalog_.at(s.cards.at(cmd.formation).definition);
                    if (host.type != CardType::Formation ||
                        def.rank > Rules::maxRank(s, catalog_, cmd.formation) ||
                        Rules::occupied(s, cmd.formation) >= Rules::rings(s, catalog_, cmd.formation))
                        return "ring_or_rank: 环位或位阶不允许";
                    const int fee = Rules::analysisCost(s, catalog_, c.id, cmd.formation);
                    if (!cost(fee, fee))
                        return "cost: 魔素或荷载不足";
                    pay(cmd.card, fee);
                    c.analysisLoad = fee;
                    c.settingPaid = fee;
                    c.castLoad = 0;
                    c.payments.clear();
                    c.targets.clear();
                    c.zone = Zone::Analysis;
                    c.spell = def.analysisTurns == 0 ? SpellState::Ready : SpellState::Analyzing;
                    auto &formation = s.cards.at(cmd.formation);
                    if (host.quickRankOne && def.rank <= 1 && c.spell == SpellState::Analyzing &&
                        formation.quickAnalysisTurn != pl.ownTurn) {
                        c.spell = SpellState::Ready;
                        formation.quickAnalysisTurn = pl.ownTurn;
                        s.events.push_back(
                            {"analysis_ready", "阵法立即完成低位阶解析（每回合一次）", -1, c.id});
                    }
                    c.host = cmd.formation;
                    c.sourceFormation = cmd.formation;
                    c.analysisStarted = pl.ownTurn;
                    c.canceledTurn = -1;
                } else if constexpr (std::is_same_v<T, PrepareCast>) {
                    auto error = prepare(actor, cmd, false);
                    if (!error.empty())
                        return error;
                } else if constexpr (std::is_same_v<T, PlayAction>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Action ||
                        pl.blockedActionTurn == pl.ownTurn)
                        return "invalid_action: 行动不可用";
                    if (!validTargets(s, def.target, def.targetCount, actor, cmd.target, cmd.targets))
                        return "target: 无效目标";
                    if (!cost(def.cost, def.burden))
                        return "cost: 魔素或荷载不足";
                    pay(cmd.card, def.cost);
                    ++pl.actionsPlayed;
                    c.zone = Zone::Action;
                    c.targetCard = cmd.target;
                    c.targets = targetCards(cmd.target, cmd.targets);
                    c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
                    if (def.burden)
                        s.temporary.push_back({s.nextLoad++, actor, c.id, def.burden, pl.ownTurn});
                    beginEffect(c.id, AfterEffect::Action);
                } else if constexpr (std::is_same_v<T, PreloadWord>) {
                    if (cmd.release && !def.immediate)
                        return "word_release: 只有零阶言灵可设置并释放";
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Word ||
                        (!(def.immediate && (!catalog_.advancedRules || cmd.release)) &&
                         inZone(s, actor, Zone::Words).size() >= 3))
                        return "word_limit: 无法预置言灵";
                    if (!cost(def.cost, def.cost))
                        return "cost: 魔素或荷载不足";
                    pay(cmd.card, def.cost);
                    c.zone = Zone::Words;
                    c.analysisLoad = def.cost;
                    c.settingPaid = def.cost;
                    if (def.immediate && (!catalog_.advancedRules || cmd.release)) {
                        c.zone = Zone::Casting;
                        c.spell = SpellState::Pending;
                        c.targetPlayer = def.target == TargetKind::Opponent ? 1 - actor : actor;
                        beginEffect(c.id, AfterEffect::Spell);
                    }
                } else if constexpr (std::is_same_v<T, AttachSeal>) {
                    if (!owned(cmd.card, Zone::Hand) || def.type != CardType::Seal ||
                        !(owned(cmd.host, Zone::Analysis) || owned(cmd.host, Zone::Words)) ||
                        seals(s, cmd.host) || s.cards.at(cmd.host).faceDown)
                        return "seal_limit: 宿主不合法或已有符文";
                    if (!cost(def.cost, 0))
                        return "cost: 魔素不足";
                    pay(cmd.card, def.cost);
                    c.zone = Zone::Attached;
                    c.host = cmd.host;
                } else if constexpr (std::is_same_v<T, RemoveSeal>) {
                    if (!owned(cmd.card, Zone::Attached) || pl.sealRemovals)
                        return "seal_limit: 无法主动拆除符文";
                    StateMaintenance::leave(s, cmd.card, &catalog_);
                    ++pl.sealRemovals;
                    if (hasOverflow(s, catalog_) ||
                        Rules::load(s, actor) > Rules::capacity(s, catalog_, actor))
                        return "capacity: 请先放弃多余法术";
                } else if constexpr (std::is_same_v<T, Abandon>) {
                    if ((c.zone != Zone::Analysis && c.zone != Zone::Casting && c.zone != Zone::Words) ||
                        (def.type != CardType::Analytic && def.type != CardType::Word &&
                         !(c.faceDown && def.type == CardType::Action)))
                        return "invalid_abandon: 无法放弃该卡";
                    StateMaintenance::leave(s, cmd.card, &catalog_);
                }
                s.events.push_back(
                    {"command", "玩家 " + std::to_string(actor + 1) + " 操作 " + def.name, -1, cmd.card});
                return {};
            }
        },
        command);
    if (error.empty()) {
        s.phaseGate = 0;
        StateMaintenance::check(s, catalog_);
    }
    return error;
}
CommandResult GameEngine::submit(PlayerId actor, const Command &command) {
    GameEngine next = *this;
    const auto start = next.state_.events.size();
    auto error = next.execute(actor, command);
    if (!error.empty())
        return {false, error, {}, state_.decision, error.substr(0, error.find(':'))};
    next.pump();
    auto failures = Rules::invariants(next.state_, next.catalog_);
    if (!failures.empty())
        throw std::logic_error("engine invariant: " + failures.front());
    std::vector<GameEvent> events(next.state_.events.begin() + static_cast<std::ptrdiff_t>(start),
                                  next.state_.events.end());
    *this = std::move(next);
    return {true, {}, std::move(events), state_.decision, {}};
}
void GameEngine::enqueuePhase(bool) {
    auto &s = state_;
    const auto batch = s.nextBatch++;
    for (const auto &[id, c] : s.cards)
        if (c.owner == s.active && c.spell == SpellState::Active && c.zone == Zone::Casting) {
            const auto &d = catalog_.at(c.definition);
            const auto &effects = s.phase == Phase::Draw      ? d.onDraw
                                  : s.phase == Phase::Prepare ? d.onPrepare
                                  : s.phase == Phase::Main    ? d.onMain
                                  : s.phase == Phase::Cast    ? d.onCast
                                                              : d.onEnd;
            if (!effects.empty()) {
                auto t = snapshot(c, d);
                t.id = s.nextTrigger++;
                t.batch = batch;
                t.effects = effects;
                s.queue.append(std::move(t));
            }
        }
}
void GameEngine::finishLink(bool success) {
    auto &s = state_;
    const auto link = s.chain->links.back();
    auto i = s.cards.find(link.item.source);
    if (i != s.cards.end()) {
        auto &c = i->second;
        const auto &d = catalog_.at(c.definition);
        if (!success && link.paymentIndex >= 0)
            c.payments.at(static_cast<std::size_t>(link.paymentIndex)).canceled = true;
        if (link.kind == LinkKind::Preparation) {
            if (success) {
                c.zone = Zone::Casting;
                c.spell = SpellState::Pending;
                c.host = 0;
                s.events.push_back({"prepared", "法术进入施法区", -1, c.id});
            } else {
                if (c.zone == Zone::Analysis && !link.sourceLost) {
                    c.castLoad = std::max(0, c.castLoad - link.preparationLoad);
                    c.canceledTurn = s.players[c.owner].ownTurn;
                }
                if (link.paymentIndex >= 0) {
                    auto &pay = c.payments.at(static_cast<std::size_t>(link.paymentIndex));
                    pay.canceled = true;
                    pay.load = 0;
                }
            }
        } else if ((link.kind == LinkKind::Spell && c.zone == Zone::Casting) ||
                   (link.kind == LinkKind::Response && d.type == CardType::Word &&
                    c.zone == Zone::Resolving)) {
            if (success) {
                for (const auto &[sid, seal] : s.cards)
                    if (seal.host == c.id && seal.zone == Zone::Attached) {
                        const auto &sealDef = catalog_.at(seal.definition);
                        if (sealDef.refundSetting || (sealDef.refundCast && !c.payments.empty())) {
                            const int refund = sealDef.refundSetting ? c.settingPaid : c.payments.back().paid;
                            Trigger t;
                            t.owner = c.owner;
                            t.source = c.id;
                            EffectResolver::apply(s, catalog_, t, {EffectKind::GainMana, refund});
                            s.events.push_back(
                                {"refund",
                                 sealDef.refundSetting ? "成功释放：返还设置费用" : "成功释放：返还施法费用",
                                 -1, c.id, refund});
                        }
                    }
                if (d.concentration) {
                    std::vector<CardId> old;
                    for (const auto &[id, other] : s.cards)
                        if (id != c.id && other.owner == c.owner && other.zone == Zone::Casting &&
                            other.spell == SpellState::Active && catalog_.at(other.definition).concentration)
                            old.push_back(id);
                    for (auto id : old)
                        StateMaintenance::leave(s, id, &catalog_);
                    c.zone = Zone::Casting;
                    c.spell = SpellState::Active;
                    c.concentrationCost = Rules::castCost(s, catalog_, c.id);
                } else if (d.duration) {
                    c.spell = SpellState::Active;
                    c.remaining = d.duration;
                } else
                    StateMaintenance::leave(s, c.id, &catalog_);
            } else {
                if (link.paymentIndex >= 0)
                    c.payments.at(static_cast<std::size_t>(link.paymentIndex)).canceled = true;
                StateMaintenance::leave(s, c.id, &catalog_);
            }
        } else if (link.kind == LinkKind::Action || link.kind == LinkKind::Response) {
            if (!success && link.paymentIndex >= 0)
                c.payments.at(static_cast<std::size_t>(link.paymentIndex)).canceled = true;
            const auto expected = link.kind == LinkKind::Action ? Zone::Action : Zone::Resolving;
            if (c.zone == expected)
                StateMaintenance::leave(s, c.id, &catalog_);
        }
    }
    s.events.push_back({success ? "link_resolved" : "link_skipped",
                        (success ? "结算" : "取消或失效：") + std::string("链节 #") + std::to_string(link.id),
                        -1, link.item.source});
    s.events.back().spellReleased =
        success && (link.kind == LinkKind::Spell ||
                    (link.kind == LinkKind::Response &&
                     catalog_.at(s.cards.at(link.item.source).definition).type == CardType::Word));
    s.effect.reset();
    s.chain->links.pop_back();
    if (s.chain->links.empty()) {
        s.events.push_back({"chain_closed", "连锁结束"});
        s.chain.reset();
    }
    StateMaintenance::check(s, catalog_);
}
void GameEngine::finishEffect() {
    if (state_.effect->link) {
        finishLink(true);
        return;
    }
    auto frame = *state_.effect;
    state_.effect.reset();
    if (frame.after == AfterEffect::Action || frame.after == AfterEffect::Word)
        StateMaintenance::leave(state_, frame.item.source, &catalog_);
    StateMaintenance::check(state_, catalog_);
}
void GameEngine::pump() {
    auto &s = state_;
    std::set<std::string> seen;
    for (int steps = 0; s.result == -1 && !s.decision; ++steps) {
        if (steps >= 10000 || !seen.insert(cycleKey()).second) {
            s.result = 2;
            s.queue.items.clear();
            s.deferredPreparations.clear();
            s.effect.reset();
            s.chain.reset();
            s.phaseGate = 0;
            s.events.push_back({"game_over", "强制循环：平局"});
            break;
        }
        bool overflow = false;
        for (const auto &[id, c] : s.cards)
            if (c.zone == Zone::Analysis && catalog_.at(c.definition).type == CardType::Formation &&
                Rules::occupied(s, id) > Rules::rings(s, catalog_, id)) {
                std::vector<CardId> opts;
                for (const auto &[sid, x] : s.cards)
                    if (x.host == id && x.zone == Zone::Analysis)
                        opts.push_back(sid);
                decision(c.owner, DecisionKind::Overflow, std::move(opts));
                overflow = true;
                break;
            }
        if (overflow)
            break;
        if (s.effect) {
            auto &f = *s.effect;
            if (f.cursor == f.item.effects.size()) {
                finishEffect();
                continue;
            }
            auto e = f.item.effects[f.cursor];
            if (e.kind == EffectKind::ClearTemporary) {
                if (f.clearRemaining < 0)
                    f.clearRemaining = e.amount;
                std::vector<std::uint32_t> opts;
                for (const auto &t : s.temporary)
                    if (t.owner == f.item.owner && !t.independent && t.amount > 0)
                        opts.push_back(t.id);
                if (!f.clearRemaining || opts.empty()) {
                    ++f.cursor;
                    f.clearRemaining = -1;
                    continue;
                }
                decision(f.item.owner, DecisionKind::ClearLoad, std::move(opts), true);
                break;
            }
            if (e.kind == EffectKind::SearchAction || e.kind == EffectKind::DestroyAmbush) {
                std::vector<CardId> opts;
                for (const auto &[id, c] : s.cards) {
                    const auto &d = catalog_.at(c.definition);
                    if (e.kind == EffectKind::SearchAction
                            ? (c.owner == f.item.owner && c.zone == Zone::Deck &&
                               d.type == CardType::Action && (d.rarity == "common" || d.rarity == "uncommon"))
                            : (c.owner != f.item.owner && c.faceDown))
                        opts.push_back(id);
                }
                if (opts.empty()) {
                    ++f.cursor;
                    continue;
                }
                decision(f.item.owner,
                         e.kind == EffectKind::SearchAction ? DecisionKind::SearchDeck
                                                            : DecisionKind::DestroyAmbush,
                         std::move(opts), e.kind == EffectKind::DestroyAmbush);
                break;
            }
            if (e.kind == EffectKind::OptionalDestroyOwnFormation) {
                std::vector<CardId> opts;
                for (const auto &[id, c] : s.cards)
                    if (c.owner == f.item.owner && c.zone == Zone::Analysis && !c.base &&
                        catalog_.at(c.definition).type == CardType::Formation && Rules::occupied(s, id) == 0)
                        opts.push_back(id);
                if (opts.empty()) {
                    ++f.cursor;
                    continue;
                }
                decision(f.item.owner, DecisionKind::DestroyOwnFormation, std::move(opts), true);
                break;
            }
            if (e.kind == EffectKind::DiscardHand || e.kind == EffectKind::DiscardFormation) {
                auto opts = inZone(s, f.item.owner, Zone::Hand);
                if (e.kind == EffectKind::DiscardFormation)
                    opts.erase(std::remove_if(opts.begin(), opts.end(),
                                              [&](CardId id) {
                                                  return catalog_.at(s.cards.at(id).definition).type !=
                                                         CardType::Formation;
                                              }),
                               opts.end());
                if (opts.empty()) {
                    f.cursor = f.item.effects.size(); // dependent remainder cannot be performed
                    s.events.push_back(
                        {"effect_incomplete", "没有可弃置手牌，后续效果不执行", -1, f.item.source});
                    continue;
                }
                decision(f.item.owner, DecisionKind::EffectDiscard, std::move(opts));
                break;
            }
            if (e.kind == EffectKind::OptionalPrepare) {
                auto candidates = preparationsFor(f.item.owner, s.nextDecision);
                if (candidates.empty()) {
                    ++f.cursor;
                    continue;
                }
                std::vector<CardId> opts;
                for (const auto &cmd : candidates)
                    if (std::find(opts.begin(), opts.end(), cmd.card) == opts.end())
                        opts.push_back(cmd.card);
                decision(f.item.owner, DecisionKind::EffectPrepare, std::move(opts), true);
                break;
            }
            EffectResolver::apply(s, catalog_, f.item, e);
            ++f.cursor;
            continue;
        }
        if (s.chain) {
            auto &chain = *s.chain;
            if (chain.mode == ChainMode::Building) {
                auto responses = responsesFor(chain.priority, s.nextDecision);
                if (responses.empty()) {
                    passResponse();
                    continue;
                }
                std::vector<CardId> opts;
                for (const auto &r : responses)
                    if (std::find(opts.begin(), opts.end(), r.card) == opts.end())
                        opts.push_back(r.card);
                decision(chain.priority, DecisionKind::Response, std::move(opts), true);
                break;
            }
            auto &link = chain.links.back();
            bool valid = !link.canceled;
            auto source = s.cards.find(link.item.source);
            if (link.kind == LinkKind::Preparation)
                valid = valid && !link.sourceLost && source != s.cards.end() &&
                        source->second.zone == Zone::Analysis && source->second.spell == SpellState::Ready;
            else if (link.kind == LinkKind::Spell)
                valid = valid && !link.sourceLost && source != s.cards.end() &&
                        source->second.zone == Zone::Casting && source->second.spell == SpellState::Pending;
            else if (link.item.requiresSource)
                valid =
                    valid && !link.sourceLost && source != s.cards.end() && source->second.zone != Zone::Ash;
            auto item = link.item;
            if (cardTarget(item.target)) {
                auto ids = targetCards(item.targetCard, item.targetCards);
                const auto requiredCount = ids.size();
                ids.erase(std::remove_if(
                              ids.begin(), ids.end(),
                              [&](CardId id) { return !Rules::targetValid(s, item.target, item.owner, id); }),
                          ids.end());
                valid = valid && !ids.empty() && (!catalog_.advancedRules || ids.size() == requiredCount);
                item.targetCards = ids;
                item.targetCard = ids.empty() ? 0 : ids.front();
            }
            if (item.target == TargetKind::PendingLink || item.target == TargetKind::PreparationRoot ||
                item.target == TargetKind::EnemySpellLink)
                valid = valid && Rules::linkTargetValid(s, item.target, item.targetLink);
            if (!valid) {
                finishLink(false);
                continue;
            }
            s.effect = EffectFrame{std::move(item), 0, AfterEffect::None, -1, link.id};
            continue;
        }
        if (!s.deferredPreparations.empty()) {
            auto root = std::move(s.deferredPreparations.front());
            s.deferredPreparations.pop_front();
            const int priority = 1 - root.item.owner;
            startChain(ResponseWindow::Prepare, std::move(root), priority);
            continue;
        }
        // End cleanup batches are deferred until expiry, duration and forced discards finish.
        if (!s.queue.items.empty() && !(s.phase == Phase::End && s.phaseStep == PhaseStep::Cleanup)) {
            const auto batch = s.queue.items.front().batch;
            PlayerId owner = s.queue.items.front().owner;
            for (const auto &t : s.queue.items)
                if (t.batch == batch && t.owner == s.active)
                    owner = s.active;
            std::vector<std::uint32_t> opts;
            for (const auto &t : s.queue.items)
                if (t.batch == batch && t.owner == owner)
                    opts.push_back(t.id);
            if (opts.size() > 1) {
                decision(owner, DecisionKind::TriggerOrder, std::move(opts));
                break;
            }
            auto i = std::find_if(s.queue.items.begin(), s.queue.items.end(),
                                  [&](const Trigger &t) { return t.id == opts.front(); });
            auto t = *i;
            s.queue.items.erase(i);
            startTrigger(std::move(t));
            continue;
        }
        StateMaintenance::check(s, catalog_);
        if (s.result != -1)
            break;
        auto &pl = s.players[s.active];
        switch (s.phaseStep) {
        case PhaseStep::Enter: {
            s.phaseGate = 0;
            s.durationApplied = false;
            s.events.push_back({"phase_enter", "进入" + phaseName(s.phase) + "阶段"});
            if (s.phase == Phase::Draw) {
                s.flow = Flow::Draw;
                if (!(s.active == s.first && pl.ownTurn == 1)) {
                    Trigger t;
                    t.owner = s.active;
                    EffectResolver::apply(s, catalog_, t, {EffectKind::Draw, 1});
                }
                StateMaintenance::check(s, catalog_);
                if (s.result != -1)
                    break;
            }
            if (s.phase == Phase::Prepare) {
                s.phaseStep = PhaseStep::PrepareIncome;
                for (const auto &[id, c] : s.cards)
                    if (c.owner == s.active && c.zone == Zone::Casting && c.spell == SpellState::Active &&
                        catalog_.at(c.definition).concentration) {
                        std::vector<CardId> opts;
                        if (pl.mana >= c.concentrationCost &&
                            Rules::load(s, s.active) + c.concentrationCost <=
                                Rules::capacity(s, catalog_, s.active))
                            opts.push_back(id);
                        decision(s.active, DecisionKind::Concentration, std::move(opts), true);
                        break;
                    }
                break;
            }
            enqueuePhase(false);
            s.phaseStep = PhaseStep::StartWindow;
            break;
        }
        case PhaseStep::PrepareIncome: {
            s.flow = Flow::Income;
            int income = 1;
            for (auto id : inZone(s, s.active, Zone::Analysis)) {
                const auto &d = catalog_.at(s.cards.at(id).definition);
                if (d.type == CardType::Formation) {
                    income += d.income;
                    for (const auto &[sid, seal] : s.cards)
                        if (seal.zone == Zone::Attached && seal.host == id)
                            income += catalog_.at(seal.definition).incomeBonus;
                }
            }
            Trigger t;
            t.owner = s.active;
            EffectResolver::apply(s, catalog_, t, {EffectKind::GainMana, std::max(0, income)});
            s.events.push_back({"income", "准备阶段收入 " + std::to_string(income)});
            for (auto &[id, c] : s.cards)
                if (c.owner == s.active && !c.faceDown && c.spell == SpellState::Analyzing &&
                    pl.ownTurn - c.analysisStarted >= catalog_.at(c.definition).analysisTurns)
                    c.spell = SpellState::Ready;
            enqueuePhase(false);
            s.phaseStep = PhaseStep::StartWindow;
            break;
        }
        case PhaseStep::StartWindow: {
            s.phaseStep = PhaseStep::Body;
            ChainLink root;
            root.kind = LinkKind::Phase;
            root.item.owner = s.active;
            startChain(ResponseWindow::PhaseStart, std::move(root), s.active);
            break;
        }
        case PhaseStep::Body:
            if (s.phase == Phase::Main)
                s.flow = Flow::Main;
            if (s.phase == Phase::Cast) {
                s.flow = Flow::Cast;
                std::vector<CardId> opts;
                for (auto id : inZone(s, s.active, Zone::Casting))
                    if (s.cards.at(id).spell == SpellState::Pending)
                        opts.push_back(id);
                if (!opts.empty()) {
                    s.phaseGate = 0;
                    decision(s.active, DecisionKind::CastOrder, std::move(opts), catalog_.advancedRules);
                    return;
                }
            }
            if (!s.phaseGate)
                s.phaseGate = s.nextDecision++;
            return;
        case PhaseStep::EndWindow: {
            s.phaseStep = PhaseStep::EndTriggers;
            ChainLink root;
            root.kind = LinkKind::Phase;
            root.item.owner = s.active;
            startChain(ResponseWindow::PhaseEnd, std::move(root), s.active);
            break;
        }
        case PhaseStep::EndTriggers:
            s.phaseStep = PhaseStep::Cleanup;
            break;
        case PhaseStep::Cleanup:
            if (s.phase != Phase::End) {
                s.phaseStep = PhaseStep::Finish;
                break;
            }
            s.temporary.erase(std::remove_if(s.temporary.begin(), s.temporary.end(),
                                             [&](const TemporaryLoad &t) {
                                                 return t.amount == 0 ||
                                                        (!t.independent && !t.sourceBound &&
                                                         t.owner == s.active && t.expiryTurn <= pl.ownTurn);
                                             }),
                              s.temporary.end());
            if (!s.durationApplied) {
                s.durationApplied = true;
                std::vector<CardId> expired;
                for (auto &[id, c] : s.cards)
                    if (c.owner == s.active && c.spell == SpellState::Active &&
                        !catalog_.at(c.definition).concentration && --c.remaining <= 0)
                        expired.push_back(id);
                for (auto id : expired)
                    StateMaintenance::leave(s, id, &catalog_);
                StateMaintenance::check(s, catalog_);
                if (s.result != -1)
                    break;
            }
            {
                auto hand = inZone(s, s.active, Zone::Hand);
                if (hand.size() > 8) {
                    decision(s.active, DecisionKind::Discard, std::move(hand));
                    return;
                }
            }
            s.phaseStep = PhaseStep::CleanupTriggers;
            break;
        case PhaseStep::CleanupTriggers:
            if (inZone(s, s.active, Zone::Hand).size() > 8 ||
                std::any_of(s.temporary.begin(), s.temporary.end(), [&](const TemporaryLoad &t) {
                    return t.amount == 0 || (!t.independent && !t.sourceBound && t.owner == s.active &&
                                             t.expiryTurn <= pl.ownTurn);
                })) {
                s.phaseStep = PhaseStep::Cleanup;
                break;
            }
            s.phaseStep = PhaseStep::Finish;
            break;
        case PhaseStep::Finish:
            s.events.push_back({"phase_end", phaseName(s.phase) + "阶段结束"});
            if (s.phase == Phase::End) {
                pl.actionsPlayed = 0;
                pl.formations = 0;
                pl.sealRemovals = 0;
                for (auto &[id, c] : s.cards)
                    if (c.owner == s.active)
                        c.canceledTurn = -1;
                ++s.globalTurn;
                s.active = 1 - s.active;
                ++s.players[s.active].ownTurn;
                s.phase = Phase::Draw;
                s.flow = Flow::Draw;
            } else
                s.phase = static_cast<Phase>(static_cast<int>(s.phase) + 1);
            s.phaseStep = PhaseStep::Enter;
            s.phaseGate = 0;
            break;
        }
    }
}
} // namespace wizard
