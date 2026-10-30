#include "engine_internal.hpp"
#include "wizard/core/card_rules.hpp"
#include <algorithm>
#include <set>
#include <stdexcept>
#include <type_traits>
namespace wizard {
using namespace detail;
GameEngine::GameEngine(CardCatalog cat, std::array<std::vector<std::string>, 2> decks, std::uint32_t seed)
    : GameEngine(std::move(cat), shared(std::move(decks), seed)) {}
GameEngine::GameEngine(CardCatalog cat, const MatchConfig &config) {
    cat.compileAbilities();
    catalog_ = std::make_shared<const CardCatalog>(std::move(cat));
    if (catalog().rulesVersion != "2.5.5" || catalog().mechanicsRevision != 9)
        throw std::invalid_argument("unsupported ruleset: expected 2.5.5 revision 9");
    for (const auto &deck : config.players) {
        auto errors = Rules::deckErrors(catalog(), deck);
        if (!errors.empty())
            throw std::invalid_argument(errors.front());
    }
    state_.rng = config.seed ? config.seed : 0x9e3779b9u;
    // Frozen rule fixtures retain their original starting resource.

    state_.opening = true;
    if (state_.opening) state_.globalTurn = 0;
    CardId id = 1;
    for (int p = 0; p < 2; ++p) {
        for (const auto &key : config.players[p].sideDeck) {
            CardInstance c;
            c.id = id++; c.owner = p; c.controller = p; c.definition = key; c.zone = Zone::SideDeck;
            state_.cards.emplace(c.id, c);
        }
        for (const auto &key : config.players[p].cards) {
            CardInstance c;
            c.id = id++;
            c.owner = p;
            c.controller = p;
            c.definition = key;
            state_.cards.emplace(c.id, c);
            state_.players[p].deck.push_back(c.id);
        }
    }
    const auto errors = Rules::invariants(state_, catalog());
    if (!errors.empty()) throw std::logic_error("invalid opening: " + errors.front());
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
                catalog().at(state_.cards.at(id).definition).name, -1, id});
        }
    }
    for (int p = 0; p < 2; ++p) {
        auto &deck = state_.players[p].deck;
        for (std::size_t i = deck.size(); i > 1; --i)
            std::swap(deck[i - 1], deck[bounded(state_, static_cast<std::uint32_t>(i))]);
        Trigger t;
        t.owner = p;
        EffectResolver::apply(state_, catalog(), t, {EffectKind::Draw, 5});
    }
    state_.first = state_.active = static_cast<int>(bounded(state_, 2));
    state_.players[state_.active].ownTurn = 1;
}
GameEngine GameEngine::scenario(CardCatalog cat, GameState state) {
    GameEngine e;
    cat.compileAbilities();
    e.catalog_ = std::make_shared<const CardCatalog>(std::move(cat));
    static_cast<GameState &>(e.state_) = std::move(state);
    for (int p = 0; p < 2; ++p) ResourceRules::normalizeTemporaryLife(e.state_, p);
    {
        for (const auto &[id, card] : e.state_.cards)
            e.state_.nextFieldOrder = std::max(e.state_.nextFieldOrder, card.fieldOrder + 1);
        for (auto &[id, card] : e.state_.cards)
            if (card.zone == Zone::Analysis && !card.fieldOrder)
                card.fieldOrder = e.state_.nextFieldOrder++;
        for (auto &[id, card] : e.state_.cards)
            if (card.zone == Zone::Analysis && card.host && card.ringIndex < 0)
                card.ringIndex = Rules::freeRing(e.state_, e.catalog(), card.host);
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
    auto errors = Rules::invariants(e.state_, e.catalog());
    if (!errors.empty())
        throw std::invalid_argument("invalid scenario: " + errors.front());
    return e;
}
void GameEngine::typedDecision(PlayerId p, DecisionKind kind, std::vector<ChoiceValue> options, bool pass) {
    state_.decision = PendingDecision{state_.nextDecision++, p, kind, std::move(options), pass};
}
void GameEngine::decision(PlayerId p, DecisionKind kind, std::vector<CardId> options, bool pass) {
    std::vector<ChoiceValue> typed;
    typed.reserve(options.size());
    for (const auto value : options) typed.push_back(ChoiceValue::card(value));
    typedDecision(p, kind, std::move(typed), pass);
}
Command GameEngine::normalizeCommand(const Command &command) const {
    const auto *choice = std::get_if<Choose>(&command);
    if (!choice || choice->option.kind != ChoiceKind::Legacy || !state_.decision) return command;
    auto normalized = *choice;
    for (const auto &option : state_.decision->options)
        if (detail::legacyChoice(option, state_.decision->kind) == choice->option.value) {
            normalized.option = option; return normalized;
        }
    // Format-4 callers represented a response by its source card alone. The
    // current protocol carries a fully typed Respond command, but accepting
    // the unambiguous in-process form keeps existing replay adapters and CLI
    // helpers atomic while they migrate.
    if (state_.decision->kind == DecisionKind::Response) {
        const auto candidates = responsesFor(state_.decision->player, state_.decision->id);
        const auto found = std::find_if(candidates.begin(), candidates.end(), [&](const Respond &response) {
            return response.card == choice->option.value;
        });
        if (found != candidates.end() &&
            std::count_if(candidates.begin(), candidates.end(), [&](const Respond &response) {
                return response.card == choice->option.value;
            }) == 1)
            return *found;
    }
    return command;
}
CommandResult GameEngine::submit(PlayerId actor, const Command &command) {
    if (std::holds_alternative<Timeout>(command))
        return {false, "authority_only: 超时只能由权威会话产生", {}, state_.decision, "authority_only"};
    return submitAuthority(actor, command);
}
CommandResult GameEngine::timeout(PlayerId actor) {
    return submitAuthority(actor, Timeout{});
}
CommandResult GameEngine::submitAuthority(PlayerId actor, const Command &input) {
    const auto planned = plan(actor, input);
    if (const auto *error = std::get_if<std::string>(&planned))
        return {false, *error, {}, state_.decision, error->substr(0, error->find(':'))};
    GameEngine next = *this;
    next.state_.events.clear();
    std::string error;
    if (const auto *request = std::get_if<PaymentRequest>(&planned)) {
        next.state_.payment = request->continuation;
        std::vector<ChoiceValue> options;
        for (std::size_t index = 0; index < request->continuation.choices.size(); ++index)
            options.push_back({ChoiceKind::Payment, index + 1});
        next.typedDecision(actor, DecisionKind::Payment, std::move(options), true);
    } else {
        auto commandPlan = std::get<CommandPlan>(planned);
        if (next.state_.payment && !std::holds_alternative<Surrender>(commandPlan.command) && !std::holds_alternative<Timeout>(commandPlan.command)) {
            const auto choice = std::get<Choose>(commandPlan.command).option;
            const auto payment = *next.state_.payment;
            next.state_.payment.reset();
            next.state_.decision = payment.previousDecision;
            if (choice) {
                const auto resumed = next.plan(actor, payment.command, payment.choices.at(choice.value - 1));
                if (const auto *rejected = std::get_if<std::string>(&resumed)) error = *rejected;
                else error = next.apply(std::get<CommandPlan>(resumed));
            }
        } else error = next.apply(commandPlan);
    }
    if (!error.empty()) return {false, error, {}, state_.decision, error.substr(0, error.find(':'))};
    try { next.pump(); }
    catch (const ExecutionFault &fault) { return {false, fault.what(), {}, state_.decision, "execution_budget"}; }
    auto failures = Rules::invariants(next.state_, next.catalog());
    if (!failures.empty())
        throw std::logic_error("engine invariant: " + failures.front());
    auto events = std::move(next.state_.events);
    next.state_.events.clear();
    *this = std::move(next);
    return {true, {}, std::move(events), state_.decision, {}};
}
std::vector<GameEvent> GameEngine::takeInitialEvents() {
    auto events = std::move(state_.events);
    state_.events.clear();
    return events;
}
} // namespace wizard
