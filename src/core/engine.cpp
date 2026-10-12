#include "engine_internal.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard {
using namespace detail;
GameEngine::GameEngine(CardCatalog cat, std::array<std::vector<std::string>, 2> decks, std::uint32_t seed)
    : GameEngine(std::move(cat), shared(std::move(decks), seed)) {}
GameEngine::GameEngine(CardCatalog cat, const MatchConfig &config) : catalog_(std::move(cat)) {
    if (catalog_.alphaV2Draft && !catalog_.alphaV2)
        throw std::invalid_argument("Alpha v2 draft requires the explicit offline scenario entry point");
    for (const auto &deck : config.players) {
        auto errors = Rules::deckErrors(catalog_, deck);
        if (!errors.empty())
            throw std::invalid_argument(errors.front());
    }
    state_.rng = config.seed ? config.seed : 0x9e3779b9u;
    state_.opening = catalog_.usesAlphaV25();
    if (state_.opening) state_.globalTurn = 0;
    CardId id = 1;
    for (int p = 0; p < 2; ++p) {
        if (!state_.opening) {
            CardInstance base;
            base.id = id++; base.definition = config.players[p].baseFormation;
            base.owner = p; base.base = true; base.zone = Zone::Analysis;
            if (catalog_.usesAlphaV2()) base.fieldOrder = state_.nextFieldOrder++;
            state_.cards.emplace(base.id, base);
        }
        bool selected = false;
        for (const auto &key : config.players[p].sideDeck) {
            if (!state_.opening && !selected && key == config.players[p].baseFormation) { selected = true; continue; }
            CardInstance c;
            c.id = id++; c.owner = p; c.definition = key; c.zone = Zone::SideDeck;
            state_.cards.emplace(c.id, c);
        }
        for (const auto &key : config.players[p].cards) {
            CardInstance c;
            c.id = id++;
            c.owner = p;
            c.definition = key;
            state_.cards.emplace(c.id, c);
            state_.players[p].deck.push_back(c.id);
        }
    }
    if (state_.opening) {
        const auto errors = Rules::invariants(state_, catalog_);
        if (!errors.empty()) throw std::logic_error("invalid opening: " + errors.front());
        return;
    }
    finishOpening();
    pump();
}
void GameEngine::finishOpening() {
    if (state_.opening) {
        for (int p = 0; p < 2; ++p) {
            auto &card = state_.cards.at(state_.initialFormations[p]);
            card.zone = Zone::Analysis; card.base = true;
            card.fieldOrder = state_.nextFieldOrder++;
        }
        state_.opening = false;
        state_.globalTurn = 1;
        for (int p = 0; p < 2; ++p) {
            const auto id = state_.initialFormations[p];
            state_.events.push_back({"initial_formation", "玩家 " + std::to_string(p + 1) + " 设置初始阵法 " +
                catalog_.at(state_.cards.at(id).definition).name, -1, id});
        }
    }
    for (int p = 0; p < 2; ++p) {
        auto &deck = state_.players[p].deck;
        for (std::size_t i = deck.size(); i > 1; --i)
            std::swap(deck[i - 1], deck[bounded(state_, static_cast<std::uint32_t>(i))]);
        Trigger t;
        t.owner = p;
        EffectResolver::apply(state_, catalog_, t, {EffectKind::Draw, 5});
    }
    state_.first = state_.active = static_cast<int>(bounded(state_, 2));
    state_.players[state_.active].ownTurn = 1;
}
GameEngine GameEngine::scenario(CardCatalog cat, GameState state) {
    GameEngine e;
    e.catalog_ = std::move(cat);
    e.state_ = std::move(state);
    if (e.catalog_.usesAlphaV2()) {
        for (const auto &[id, card] : e.state_.cards)
            e.state_.nextFieldOrder = std::max(e.state_.nextFieldOrder, card.fieldOrder + 1);
        for (auto &[id, card] : e.state_.cards)
            if (card.zone == Zone::Analysis && !card.fieldOrder)
                card.fieldOrder = e.state_.nextFieldOrder++;
        for (auto &[id, card] : e.state_.cards)
            if (card.zone == Zone::Analysis && card.host && card.ringIndex < 0)
                card.ringIndex = Rules::freeRing(e.state_, e.catalog_, card.host);
        for (auto &[id, card] : e.state_.cards)
            if (card.zone == Zone::Analysis && card.host) {
                card.sourceFormation = card.host;
                card.sourceRingIndex = card.ringIndex;
            }
    }
    if (e.state_.phase == Phase::Main && e.state_.flow == Flow::Main &&
        e.state_.phaseStep == PhaseStep::Enter)
        e.state_.phaseStep = PhaseStep::Body;
    if (e.state_.decision)
        e.state_.nextDecision = std::max(e.state_.nextDecision, e.state_.decision->id + 1);
    if (!e.state_.opening && e.state_.phaseStep == PhaseStep::Body && !e.state_.decision && !e.state_.chain && !e.state_.effect &&
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
CommandResult GameEngine::submit(PlayerId actor, const Command &command) {
    if (std::holds_alternative<Timeout>(command))
        return {false, "authority_only: 超时只能由权威会话产生", {}, state_.decision, "authority_only"};
    return submitAuthority(actor, command);
}
CommandResult GameEngine::timeout(PlayerId actor) {
    return submitAuthority(actor, Timeout{});
}
CommandResult GameEngine::submitAuthority(PlayerId actor, const Command &command) {
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
} // namespace wizard
