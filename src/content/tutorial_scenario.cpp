#include "wizard/tutorial_scenario.hpp"
#include <algorithm>
#include <fstream>
#include <iterator>
#include <set>
namespace wizard {
namespace {
[[noreturn]] void reject(const std::string &message) {
    throw std::invalid_argument("tutorial scenario: " + message);
}
void object(const Json &value, std::initializer_list<const char *> keys) {
    if (!value.is_object()) reject("expected object");
    for (const auto &[key, unused] : value.items()) {
        (void)unused;
        if (std::none_of(keys.begin(), keys.end(), [&](const char *allowed) { return key == allowed; }))
            reject("unknown field " + key);
    }
}
int number(const Json &value, const char *key, int fallback, int minimum, int maximum) {
    if (!value.contains(key)) return fallback;
    const auto &input = value.at(key);
    if (!input.is_number_integer() || input < minimum || input > maximum)
        reject(std::string(key) + " out of bounds");
    return input.get<int>();
}
const Json &array(const Json &value, const char *key) {
    static const Json empty = Json::array();
    if (!value.contains(key)) return empty;
    const auto &result = value.at(key);
    if (!result.is_array() || result.size() > 200) reject(std::string(key) + " must be an array of at most 200 cards");
    return result;
}
bool spell(CardType type) {
    return type == CardType::Analytic || type == CardType::Word || type == CardType::Talisman;
}
CardId add(GameState &state, const CardCatalog &catalog, PlayerId player, const Json &key, Zone zone) {
    if (!key.is_string() || !catalog.cards.count(key.get<std::string>())) reject("unknown card ID");
    const auto type = catalog.at(key.get<std::string>()).type;
    if ((zone == Zone::SideDeck) != (type == CardType::Formation) && zone != Zone::Analysis)
        reject("formation must be in the side deck or on the field");
    const auto id = static_cast<CardId>(state.cards.size() + 1);
    CardInstance card;
    card.id = id; card.owner = card.controller = player;
    card.definition = key.get<std::string>(); card.zone = zone;
    if (zone == Zone::Analysis) card.fieldOrder = state.nextFieldOrder++;
    state.cards.emplace(id, std::move(card));
    if (zone == Zone::Deck) state.players[player].deck.push_back(id);
    return id;
}
void presets(GameState &state, const CardCatalog &catalog, PlayerId player, const Json &input,
             const std::vector<CardId> &formations, bool faceDown) {
    for (const auto &entry : array(input, faceDown ? "faceDown" : "resolved")) {
        object(entry, {"card", "formation", "ring", "load", "ambushedTurn"});
        if (!entry.contains("card")) reject("preset missing card");
        const auto id = add(state, catalog, player, entry.at("card"), Zone::Analysis);
        auto &card = state.cards.at(id);
        const auto &definition = catalog.at(card.definition);
        if (!spell(definition.type) || (!faceDown && definition.type != CardType::Analytic))
            reject("resolved preset requires an analytic spell; faceDown requires a spell");
        const auto hostIndex = number(entry, "formation", 0, 0, 199);
        if (static_cast<std::size_t>(hostIndex) >= formations.size()) reject("preset formation index out of bounds");
        const auto host = formations[hostIndex];
        if (!Rules::canHost(state, catalog, id, host, faceDown)) reject("illegal preset host");
        const auto ring = number(entry, "ring", Rules::freeRing(state, catalog, host), 0, 199);
        if (ring < 0 || Rules::placementRing(state, catalog, host, ring) < 0) reject("occupied or invalid preset ring");
        card.host = card.sourceFormation = host;
        card.ringIndex = card.sourceRingIndex = ring;
        card.faceDown = card.wasAmbushed = faceDown;
        card.analysisProgress = faceDown ? 0 : definition.analysisTurns;
        card.spell = faceDown ? SpellState::None : SpellState::Ready;
        card.analysisStarted = state.players[player].ownTurn;
        card.analysisLoad = number(entry, "load", 0, 0, 1000);
        if (faceDown && card.analysisLoad) reject("faceDown preset cannot have analysis load");
        if (faceDown) card.ambushedTurn = static_cast<std::uint64_t>(number(entry, "ambushedTurn", 0, 0,
            static_cast<int>(state.globalTurn)));
        else if (entry.contains("ambushedTurn")) reject("resolved spell cannot have ambushedTurn");
    }
}
GameState build(const Json &scenario, const CardCatalog &catalog) {
    object(scenario, {"opening", "seed", "firstPlayer", "activePlayer", "phase", "turn", "players"});
    GameState state;
    state.rng = static_cast<std::uint32_t>(number(scenario, "seed", 42, 0, 2147483647));
    if (!state.rng) state.rng = 0x9e3779b9u;
    if (scenario.contains("opening") && !scenario.at("opening").is_boolean()) reject("opening must be boolean");
    state.opening = scenario.value("opening", false);
    state.openingStage = state.opening ? OpeningStage::Gestures : OpeningStage::Complete;
    state.first = number(scenario, "firstPlayer", 0, 0, 1);
    state.active = number(scenario, "activePlayer", state.first, 0, 1);
    state.globalTurn = static_cast<std::uint64_t>(number(scenario, "turn", state.opening ? 0 : 1,
        state.opening ? 0 : 1, 1000000));
    if (state.opening && (state.globalTurn || scenario.contains("phase") || scenario.contains("firstPlayer") ||
                          scenario.contains("activePlayer"))) reject("real opening determines turn order and phase");
    const std::map<std::string, std::pair<Phase, Flow>> phases = {
        {"Draw", {Phase::Draw, Flow::Draw}}, {"Prepare", {Phase::Prepare, Flow::Progress}},
        {"Main", {Phase::Main, Flow::Main}}, {"Cast", {Phase::Cast, Flow::Cast}}, {"End", {Phase::End, Flow::EndTriggers}}};
    const auto phase = scenario.value("phase", std::string(state.opening ? "Draw" : "Main"));
    const auto found = phases.find(phase);
    if (found == phases.end()) reject("unsupported phase");
    state.phase = found->second.first; state.flow = found->second.second;
    state.phaseStep = !state.opening && (state.phase == Phase::Main || state.phase == Phase::Cast)
        ? PhaseStep::Body : PhaseStep::Enter;
    const auto &players = array(scenario, "players");
    if (players.size() != 2) reject("expected exactly two players");
    for (PlayerId player : {0, 1}) {
        const auto &input = players[player];
        object(input, {"life", "mana", "load", "ownTurn", "deck", "sideDeck", "hand", "formation", "formations", "resolved", "faceDown"});
        auto &target = state.players[player];
        target.life = number(input, "life", startingLife, 1, maximumLife);
        target.mana = number(input, "mana", 0, 0, maximumMana);
        const auto expectedOwnTurn = static_cast<int>((state.globalTurn + (player == state.first ? 1 : 0)) / 2);
        target.ownTurn = number(input, "ownTurn", expectedOwnTurn, 0, 1000000);
        if (state.opening && target.ownTurn) reject("opening cannot have ownTurn");
        const auto &deck = array(input, "deck");
        if (deck.empty() || (state.opening && deck.size() != 40)) reject("deck must be nonempty; opening requires exactly 40 cards");
        for (const auto &key : deck) add(state, catalog, player, key, Zone::Deck);
        std::reverse(target.deck.begin(), target.deck.end()); // The engine draws from the back.
        bool initialFormation = false;
        for (const auto &key : array(input, "sideDeck")) {
            const auto id = add(state, catalog, player, key, Zone::SideDeck);
            initialFormation |= catalog.at(state.cards.at(id).definition).level == 1;
        }
        if (state.opening) {
            if (!initialFormation) reject("opening requires a level-one formation in each side deck");
            for (const auto *key : {"hand", "formation", "formations", "resolved", "faceDown"})
                if (input.contains(key)) reject("real opening cannot contain field presets or a hand");
        }
        for (const auto &key : array(input, "hand")) add(state, catalog, player, key, Zone::Hand);
        if (input.contains("formation") && input.contains("formations")) reject("use formation or formations, not both");
        std::vector<CardId> formations;
        const auto formationKeys = input.contains("formation") ? Json::array({input.at("formation")}) : array(input, "formations");
        for (const auto &key : formationKeys) {
            const auto id = add(state, catalog, player, key, Zone::Analysis);
            if (catalog.at(state.cards.at(id).definition).type != CardType::Formation) reject("preset formation has wrong type");
            state.cards.at(id).base = formations.empty();
            formations.push_back(id);
        }
        presets(state, catalog, player, input, formations, false);
        presets(state, catalog, player, input, formations, true);
        const auto load = number(input, "load", 0, 0, 1000);
        if (load) { TemporaryLoad entry; entry.id = state.nextLoad++; entry.owner = player;
            entry.amount = load; entry.independent = true; state.temporary.push_back(entry); }
    }
    return state;
}
}
TutorialScenario TutorialScenario::load(const std::filesystem::path &trustedDirectory,
                                       const std::string &lessonId, const CardCatalog &catalog) {
    if (lessonId.empty() || lessonId.size() > 96 || lessonId.front() < 'a' || lessonId.front() > 'z' ||
        std::any_of(lessonId.begin(), lessonId.end(), [](unsigned char c) {
            return !(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-';
        })) reject("invalid lesson ID");
    const auto directory = std::filesystem::canonical(trustedDirectory);
    const auto file = std::filesystem::canonical(directory / (lessonId + ".json"));
    if (file.parent_path() != directory || !std::filesystem::is_regular_file(file)) reject("lesson escaped trusted directory");
    std::ifstream input(file, std::ios::binary);
    if (!input) reject("cannot read lesson");
    const std::string bytes{std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>()};
    TutorialScenario result;
    result.document_ = Json::parse(bytes);
    if (!result.document_.is_object() || result.document_.value("id", std::string{}) != lessonId) reject("lesson ID mismatch");
    result.cardRevision_ = number(result.document_, "cardRevision", -1, 0, 1000);
    if (result.cardRevision_ != catalog.mechanicsRevision || catalog.mechanicsRevision != 13) reject("card revision mismatch");
    if (!result.document_.contains("scenario")) reject("lesson missing scenario");
    auto compiled = catalog; compiled.compileAbilities();
    result.initial_ = build(result.document_.at("scenario"), compiled);
    result.id_ = lessonId; result.digest_ = fingerprint(bytes);
    result.contentHash_ = catalog.contentHash; result.rulesVersion_ = catalog.rulesVersion;
    // Validate normalization and phase-entry work now, before accepting the lesson.
    (void)result.start(catalog);
    return result;
}
GameEngine TutorialScenario::start(const CardCatalog &catalog) const {
    if (catalog.contentHash != contentHash_ || catalog.mechanicsRevision != cardRevision_ || catalog.rulesVersion != rulesVersion_)
        reject("catalog changed since lesson load");
    auto engine = GameEngine::scenario(catalog, initial_);
    engine.orderedTutorialOpening_ = true;
    if (!engine.state_.opening && engine.state_.phaseStep == PhaseStep::Enter) engine.pump();
    const auto errors = Rules::invariants(engine.state_, engine.catalog());
    if (!errors.empty()) reject(errors.front());
    return engine;
}
}
