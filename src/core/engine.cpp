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
} // namespace wizard
