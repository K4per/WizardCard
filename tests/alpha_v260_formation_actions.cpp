#include "support/observed_engine.hpp"
#include "wizard/content.hpp"
#include "wizard/core/action_query.hpp"
#include "wizard/core/card_rules.hpp"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <set>

using namespace wizard;

namespace {
bool spellType(CardType type) {
    return type == CardType::Analytic || type == CardType::Word || type == CardType::Talisman;
}

bool matchesRecipe(const CardDefinition &definition, const RecipeCost &need) {
    if (!need.definition.empty() && definition.id != need.definition) return false;
    if (!need.definitions.empty() &&
        std::find(need.definitions.begin(), need.definitions.end(), definition.id) == need.definitions.end()) return false;
    if (!need.school.empty() && definition.school != need.school) return false;
    if (!need.schools.empty() &&
        std::find(need.schools.begin(), need.schools.end(), definition.school) == need.schools.end()) return false;
    if (need.kind == RecipeCostKind::SacrificeFormation && definition.type != CardType::Formation) return false;
    if (need.kind == RecipeCostKind::SacrificeSpell && !spellType(definition.type)) return false;
    if (need.fireDamage && !card_rules::isFireSpell(definition)) return false;
    return true;
}

CardInstance &addCard(MutationContext &state, CardId id, const std::string &definition, Zone zone,
                      SpellState spell = SpellState::None) {
    CardInstance card;
    card.id = id;
    card.owner = 0;
    card.definition = definition;
    card.zone = zone;
    card.spell = spell;
    if (zone == Zone::Analysis) card.fieldOrder = state.nextFieldOrder++;
    return state.cards.emplace(id, std::move(card)).first->second;
}

MutationContext formationScenario(const CardCatalog &catalog, const std::string &targetId) {
    MutationContext state;
    state.phase = Phase::Main;
    state.phaseStep = PhaseStep::Body;
    state.flow = Flow::Main;
    state.phaseGate = 1;
    state.active = 0;
    state.globalTurn = 4;
    state.rng = 73;
    for (auto &player : state.players) {
        player.mana = 20;
        player.life = 20;
        player.ownTurn = 2;
    }

    bool sacrificesBasic = targetId == "v260-formation-0009";
    for (const auto &need : catalog.at(targetId).recipe)
        if (need.kind == RecipeCostKind::SacrificeFormation &&
            (need.definition == "v260-formation-0009" ||
             std::find(need.definitions.begin(), need.definitions.end(), "v260-formation-0009") != need.definitions.end()))
            sacrificesBasic = true;
    const std::string baseId = sacrificesBasic ? "v260-formation-0001" : "v260-formation-0009";
    addCard(state, 1, baseId, Zone::Analysis);
    addCard(state, 10, targetId, Zone::SideDeck);

    CardId nextMaterial = 100;
    std::vector<std::string> distinctSchools;
    const auto &formation = catalog.at(targetId);
    for (const auto &need : formation.recipe) {
        if (need.kind == RecipeCostKind::Mana || need.kind == RecipeCostKind::Life) continue;
        for (int count = 0; count < need.amount; ++count) {
            const CardDefinition *selected = nullptr;
            for (const auto &[id, definition] : catalog.cards) {
                if (!matchesRecipe(definition, need)) continue;
                if (need.distinctSchools &&
                    std::find(distinctSchools.begin(), distinctSchools.end(), definition.school) != distinctSchools.end())
                    continue;
                selected = &definition;
                break;
            }
            if (!selected) continue;

            Zone zone = Zone::Casting;
            SpellState spell = SpellState::Ready;
            if (need.kind == RecipeCostKind::SacrificeFormation) {
                zone = Zone::Analysis;
                spell = SpellState::None;
            } else if (need.kind == RecipeCostKind::DiscardHand) {
                zone = Zone::Hand;
                spell = SpellState::None;
            }
            addCard(state, nextMaterial++, selected->id, zone, spell);
            if (need.distinctSchools) distinctSchools.push_back(selected->school);
        }
    }
    return state;
}

std::optional<SetFormation> formationAction(const test::ObservedView &view, CardId source, CardId replacement) {
    for (const auto &action : view.actions)
        if (const auto *command = std::get_if<SetFormation>(&action.command))
            if (command->card == source && command->replace == replacement) return *command;
    return std::nullopt;
}
}

TEST_CASE("Alpha v2.6.0 all formal formations can be set independently or replace a formation", "[alpha-v260][formation]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets" / "release-v260");
    std::vector<std::string> formations;
    for (const auto &[id, definition] : content.catalog.cards)
        if (definition.type == CardType::Formation) formations.push_back(id);
    REQUIRE(formations.size() == 17);

    for (const auto &id : formations) {
        INFO(id);
        const auto state = formationScenario(content.catalog, id);
        const auto engine = test::ObservedEngine::scenario(content.catalog, state);
        const auto actions = engine.viewFor(0);
        const auto set = formationAction(actions, 10, 0);
        REQUIRE(set.has_value());
        const auto replace = formationAction(actions, 10, 1);
        REQUIRE(replace.has_value());

        ActionIntent intent;
        intent.selections = {
            {ActionField::Operation, std::to_string(static_cast<unsigned>(CommandKind::SetFormation))},
            {ActionField::Source, "10"}
        };
        const auto replacementOptions = engine.queryActions(0, intent);
        REQUIRE(replacementOptions.field == ActionField::Replacement);
        CHECK(std::any_of(replacementOptions.options.begin(), replacementOptions.options.end(),
            [](const ActionOption &option) { return option.value == "0"; }));
        CHECK(std::any_of(replacementOptions.options.begin(), replacementOptions.options.end(),
            [](const ActionOption &option) { return option.value == "1"; }));

        auto setEngine = test::ObservedEngine::scenario(content.catalog, state);
        const auto setResult = setEngine.submit(0, *set);
        INFO(setResult.error);
        REQUIRE(setResult.accepted);
        CHECK(setEngine.state().cards.at(10).zone == Zone::Analysis);
        CHECK(setEngine.state().cards.at(1).zone == Zone::Analysis);

        auto replaceEngine = test::ObservedEngine::scenario(content.catalog, state);
        const auto replaceResult = replaceEngine.submit(0, *replace);
        INFO(replaceResult.error);
        REQUIRE(replaceResult.accepted);
        CHECK(replaceEngine.state().cards.at(10).zone == Zone::Analysis);
        CHECK(replaceEngine.state().cards.at(1).zone == Zone::Ash);
    }
}

TEST_CASE("Engeas star and Elemental ring require replacement when formation body is full", "[alpha-v260][formation]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets" / "release-v260");
    for (const std::string id : {"v260-formation-0004", "v260-formation-0005"}) {
        INFO(id);
        auto state = formationScenario(content.catalog, id);
        addCard(state, 2, "v260-formation-0001", Zone::Analysis);
        addCard(state, 3, "v260-formation-0002", Zone::Analysis);
        const auto engine = test::ObservedEngine::scenario(content.catalog, state);
        const auto actions = engine.viewFor(0);
        CHECK_FALSE(formationAction(actions, 10, 0).has_value());
        CHECK(std::any_of(actions.actions.begin(), actions.actions.end(), [](const LegalAction &action) {
            const auto *command = std::get_if<SetFormation>(&action.command);
            return command && command->card == 10 && command->replace != 0;
        }));

        ActionIntent intent;
        intent.selections = {
            {ActionField::Operation, std::to_string(static_cast<unsigned>(CommandKind::SetFormation))},
            {ActionField::Source, "10"}
        };
        const auto replacementOptions = engine.queryActions(0, intent);
        REQUIRE(replacementOptions.field == ActionField::Replacement);
        CHECK_FALSE(std::any_of(replacementOptions.options.begin(), replacementOptions.options.end(),
            [](const ActionOption &option) { return option.value == "0"; }));
        CHECK_FALSE(replacementOptions.options.empty());
    }
}

TEST_CASE("Alpha v2.6.0 fire analytic spells can be analyzed on Sulfur", "[alpha-v260][formation]") {
    const auto content = loadContent(std::filesystem::path(WIZARD_SOURCE_DIR) / "assets" / "release-v260");
    for (const std::string spell : {"v260-analytic-0007", "v260-analytic-0010"}) {
        INFO(spell);
        MutationContext state;
        state.phase = Phase::Main;
        state.phaseStep = PhaseStep::Body;
        state.flow = Flow::Main;
        state.phaseGate = 1;
        state.active = 0;
        state.globalTurn = 4;
        state.rng = 73;
        for (auto &player : state.players) {
            player.mana = 20;
            player.life = 20;
            player.ownTurn = 2;
        }
        addCard(state, 1, "v260-formation-0008", Zone::Analysis);
        addCard(state, 10, spell, Zone::Hand);

        REQUIRE(card_rules::isFireSpell(content.catalog.at(spell)));
        auto engine = test::ObservedEngine::scenario(content.catalog, state);
        const auto actions = engine.viewFor(0);
        REQUIRE(std::any_of(actions.actions.begin(), actions.actions.end(), [](const LegalAction &action) {
            const auto *command = std::get_if<StartAnalysis>(&action.command);
            return command && command->card == 10 && command->formation == 1;
        }));

        const auto result = engine.submit(0, StartAnalysis{10, 1});
        INFO(result.error);
        REQUIRE(result.accepted);
        CHECK(engine.state().cards.at(10).zone == Zone::Analysis);
        CHECK(engine.state().cards.at(10).host == 1);
    }
}
