#include "wizard/core/rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <unordered_map>
#include <sstream>
#include <stdexcept>

namespace wizard {
const CardDefinition &CardCatalog::at(const std::string &id) const {
    return cards.at(id);
}
std::optional<Capability> findCapability(const std::string &name) {
    static const std::unordered_map<std::string, Capability> registry{
        {"activate_storm_crest_destroy", Capability::ActivateStormCrestDestroy},
        {"activate_storm_heart_choice", Capability::ActivateStormHeartChoice},
        {"activate_storm_search", Capability::ActivateStormSearch},
        {"arcana_chariot", Capability::ArcanaChariot}, {"arcana_death", Capability::ArcanaDeath},
        {"arcana_devil", Capability::ArcanaDevil}, {"arcana_emperor_cost", Capability::ArcanaEmperorCost},
        {"arcana_fool_condition", Capability::ArcanaFoolCondition}, {"arcana_hanged_cost", Capability::ArcanaHangedCost},
        {"arcana_priestess_cost", Capability::ArcanaPriestessCost}, {"arcana_strength", Capability::ArcanaStrength},
        {"arcana_temperance", Capability::ArcanaTemperance}, {"arcana_world", Capability::ArcanaWorld},
        {"ash_evocation_discount", Capability::AshEvocationDiscount},
        {"blood_altar", Capability::BloodAltar}, {"blood_curse", Capability::BloodCurse},
        {"blood_heart", Capability::BloodHeart}, {"blood_heartmark", Capability::BloodHeartmark},
        {"blood_last_stand", Capability::BloodLastStand}, {"blood_mark", Capability::BloodMark},
        {"blood_offering_cost", Capability::BloodOfferingCost}, {"blood_pool", Capability::BloodPool},
        {"blood_sacrifice_reward", Capability::BloodSacrificeReward}, {"blood_same_origin", Capability::BloodSameOrigin},
        {"blood_transfusion", Capability::BloodTransfusion}, {"blood_vein", Capability::BloodVein},
        {"blood_wrist_cost", Capability::BloodWristCost}, {"clear_load_on_analysis", Capability::ClearLoadOnAnalysis},
        {"destroy_enemy_ambush_end", Capability::DestroyEnemyAmbushEnd}, {"discard_spell_search", Capability::DiscardSpellSearch},
        {"distinct_host_schools", Capability::DistinctHostSchools}, {"double_enemy_concentration_upkeep", Capability::DoubleEnemyConcentrationUpkeep},
        {"effect_immune", Capability::EffectImmune}, {"effect_indestructible", Capability::EffectIndestructible},
        {"end_phase_start", Capability::EndPhaseStart}, {"fire_damage_spell", Capability::FireDamageSpell},
        {"great_storm", Capability::GreatStorm}, {"host_cast_life_for_temporary", Capability::HostCastLifeForTemporary},
        {"host_evocation_damage_plus_one", Capability::HostEvocationDamagePlusOne}, {"host_fire_only", Capability::HostFireOnly},
        {"host_heal_plus_one", Capability::HostHealPlusOne}, {"max_two", Capability::MaxTwo},
        {"moon", Capability::Moon}, {"opponent_spell_load_4", Capability::OpponentSpellLoad4},
        {"ping_on_spell_leave", Capability::PingOnSpellLeave}, {"quick_host_analysis_1", Capability::QuickHostAnalysis1},
        {"quick_host_analysis_2", Capability::QuickHostAnalysis2}, {"requires_storm_formation", Capability::RequiresStormFormation},
        {"requires_storm_heart_setup", Capability::RequiresStormHeartSetup}, {"requires_storm_setup", Capability::RequiresStormSetup},
        {"return_formation_from_ash", Capability::ReturnFormationFromAsh}, {"search_great_storm_on_setup", Capability::SearchGreatStormOnSetup},
        {"self_ambush_on_leave", Capability::SelfAmbushOnLeave}, {"seven_stars_banner", Capability::SevenStarsBanner},
        {"seven_stars_effect_only", Capability::SevenStarsEffectOnly}, {"seven_stars_kaiyang", Capability::SevenStarsKaiyang},
        {"seven_stars_lamp", Capability::SevenStarsLamp}, {"seven_stars_yaoguang", Capability::SevenStarsYaoguang},
        {"sique_campfire_only", Capability::SiqueCampfireOnly}, {"sique_cauldron_only", Capability::SiqueCauldronOnly},
        {"sique_detach_1", Capability::SiqueDetach1}, {"sique_detach_2", Capability::SiqueDetach2},
        {"sique_detach_3", Capability::SiqueDetach3}, {"sique_requires_campfire", Capability::SiqueRequiresCampfire},
        {"sique_requires_cauldron", Capability::SiqueRequiresCauldron}, {"storm_formation_host_only", Capability::StormFormationHostOnly},
        {"storm_formation_host_restriction", Capability::StormFormationHostRestriction}, {"storm_heart_upgrade", Capability::StormHeartUpgrade},
        {"storm_release_counter", Capability::StormReleaseCounter}, {"storm_search_talisman", Capability::StormSearchTalisman},
        {"storm_search", Capability::StormSearch}, {"inspect_enemy_ambush", Capability::InspectEnemyAmbush},
        {"surcharge_enemy_timed_cast", Capability::SurchargeEnemyTimedCast}, {"thunder_damage_bonus", Capability::ThunderDamageBonus},
        {"sique_boiling_surge", Capability::SiqueBoilingSurge}, {"sique_chili", Capability::SiqueChili},
        {"sique_sugar", Capability::SiqueSugar}, {"sique_vinegar", Capability::SiqueVinegar},
        {"moon_material_formation", Capability::MoonMaterialFormation}, {"counter_spell", Capability::CounterSpell},
        {"unique", Capability::Unique},
        // Alpha v2.6 staging traits share one runtime capability.  The
        // original string remains on CardDefinition::traits and is used by
        // the deterministic staging adapter when it needs the exact clause.
        {"accelerate_analysis", Capability::V260Structured},
        {"block_analytic", Capability::V260Structured},
        {"block_concentration", Capability::V260Structured},
        {"borrow_analysis", Capability::V260Structured},
        {"cancel_setup_return", Capability::V260Structured},
        {"command_choice", Capability::V260Structured},
        {"counter_physical", Capability::V260Structured},
        {"damage_threshold", Capability::V260Structured},
        {"delay_all_enemy_analysis", Capability::V260Structured},
        {"extra_turn_load_double", Capability::V260Structured},
        {"halve_casting_capacity", Capability::V260Structured},
        {"hand_or_psychic", Capability::V260Structured},
        {"increase_damage_taken", Capability::V260Structured},
        {"inspect_and_route", Capability::V260Structured},
        {"inspect_opponent_deck", Capability::V260Structured},
        {"inspect_own_deck", Capability::V260Structured},
        {"mana_on_elemental_damage", Capability::V260Structured},
        {"prevent_lethal", Capability::V260Structured},
        {"reflect_damage", Capability::V260Structured},
        {"retaliate_damage", Capability::V260Structured},
        {"setup_damage", Capability::V260Structured},
        {"target_lock", Capability::V260Structured},
        {"word_cast_damage", Capability::V260Structured}
    };
    const auto found = registry.find(name);
    return found == registry.end() ? std::nullopt : std::optional<Capability>(found->second);
}

void CardCatalog::compileAbilities() {
    static const std::unordered_map<std::string, Capability> namedExecutors{
        {"arcana-magician", Capability::ArcanaMagician},
        {"arcana-world", Capability::ArcanaWorld},
        {"arcana-tower", Capability::ArcanaTower},
         {"arcana-lovers", Capability::ArcanaLovers},
        {"blood-debt", Capability::BloodDebt},
        {"blood-repayment", Capability::BloodRepayment},
        {"cloud-mist", Capability::CloudMist},
        {"counter-spell", Capability::CounterSpell},
        {"qixing-divination", Capability::QixingDivination},
        {"qixing-gaze", Capability::QixingGaze},
        {"qixing-return", Capability::QixingReturn},
        {"qixing-ruin", Capability::QixingRuin},
        {"qixing-shift", Capability::QixingShift},
        {"qixing-tianquan", Capability::QixingTianquan},
        {"qixing-tianshu", Capability::QixingTianshu},
        {"qixing-tianxuan", Capability::QixingTianxuan},
        {"qixing-yuheng", Capability::QixingYuheng},
        {"qixing-banner", Capability::SevenStarsBanner},
        {"sique-aroma-lure", Capability::SiqueAromaLure},
        {"sique-boiling-surge", Capability::SiqueBoilingSurge},
        {"sique-campfire", Capability::SiqueCampfire},
        {"sique-cauldron", Capability::SiqueCauldron},
        {"sique-chili", Capability::SiqueChili},
        {"sique-sugar", Capability::SiqueSugar},
        {"sique-vinegar", Capability::SiqueVinegar},
        {"storm-crest", Capability::StormCrest},
        {"storm-heart", Capability::StormHeart},
        {"swift-wind", Capability::SwiftWind},
        {"thunder-summon", Capability::ThunderSummon},
        {"wind-strike", Capability::WindStrike}
    };
    static const std::unordered_map<std::string, Capability> existingExecutors{
        {"arcana-chariot", Capability::ArcanaChariot},
        {"arcana-death", Capability::ArcanaDeath},
        {"arcana-devil", Capability::ArcanaDevil},
        {"arcana-temperance", Capability::ArcanaTemperance},
        {"great-storm", Capability::GreatStorm},
        {"moon", Capability::Moon},
        {"qixing-kaiyang", Capability::SevenStarsKaiyang},
        {"qixing-lamp", Capability::SevenStarsLamp},
        {"qixing-yaoguang", Capability::SevenStarsYaoguang}
    };
    for (auto &[id, definition] : cards) {
        definition.capabilities.clear();
        definition.capabilities.reserve(definition.traits.size() + 1);
        for (const auto &trait : definition.traits) {
            const auto capability = findCapability(trait);
            if (!capability) throw std::invalid_argument("unsupported card capability '" + trait + "' on " + id);
            definition.capabilities.push_back(*capability);
        }
        if (const auto executor = namedExecutors.find(id); executor != namedExecutors.end())
            definition.capabilities.push_back(executor->second);
        if (const auto executor = existingExecutors.find(id); executor != existingExecutors.end())
            definition.capabilities.push_back(executor->second);
        if (id == "hexagram" || id == "pentagram-of-life" || id == "octagram")
            definition.capabilities.push_back(Capability::MoonMaterialFormation);
        std::sort(definition.capabilities.begin(), definition.capabilities.end());
        definition.capabilities.erase(std::unique(definition.capabilities.begin(), definition.capabilities.end()), definition.capabilities.end());
    }
}
void EffectQueue::append(Trigger t) {
    items.push_back(std::move(t));
}
} // namespace wizard

namespace wizard {
int EffectFrame::legacyStage() const {
    return std::visit([](const auto &value) -> int {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, RepeatedChoiceContinuation>) return value.remaining;
        else if constexpr (std::is_same_v<T, EntityChoiceContinuation>) return value.entity ? static_cast<int>(*value.entity) : value.inspected ? 0 : -1;
        else if constexpr (std::is_same_v<T, GuidanceContinuation>) return -1-static_cast<int>(value.stage);
        else if constexpr (std::is_same_v<T, TargetWalkContinuation> || std::is_same_v<T, PlayerWalkContinuation> || std::is_same_v<T, CatalogWalkContinuation>) return value.next ? static_cast<int>(*value.next) : -1;
        else if constexpr (std::is_same_v<T, OnceContinuation>) return value.performed ? -2 : -1;
        else return value.offerDamage ? 1 : -1;
    }, continuation);
}
int EffectFrame::legacyCount() const {
    return std::visit([](const auto &value) -> int {
        using T = std::decay_t<decltype(value)>;
        if constexpr (std::is_same_v<T, RepeatedChoiceContinuation> || std::is_same_v<T, EntityChoiceContinuation>) return value.completed;
        else if constexpr (std::is_same_v<T, TargetWalkContinuation> || std::is_same_v<T, CycloneContinuation>) return value.destroyed;
        else return 0;
    }, continuation);
}
}

namespace wizard {
void EffectFrame::beginCurrentEffect() {
    if (cursor >= item.effects.size() ||
        (continuationCursor == cursor && continuationKind == item.effects[cursor].kind)) return;
    carriedResult = legacyCount();
    continuationCursor = cursor;
    continuationKind = item.effects[cursor].kind;
    if (suspendedContinuation && suspendedContinuation->first == cursor) {
        continuation = std::move(suspendedContinuation->second);
        suspendedContinuation.reset();
        return;
    }
    switch (item.effects[cursor].kind) {
    case EffectKind::SearchSameSchool: case EffectKind::RecoverEnemyAshSpell:
    case EffectKind::SevenUpgrade: case EffectKind::SevenKaiyang:
    case EffectKind::SevenInsight: case EffectKind::SevenShift:
    case EffectKind::SiqueAttachFromDeck: continuation = EntityChoiceContinuation{}; break;
    case EffectKind::StormGuidance: continuation = GuidanceContinuation{}; break;
    case EffectKind::Destroy: case EffectKind::DestroyAllEnemyField: case EffectKind::SevenGaze:
        continuation = TargetWalkContinuation{}; break;
    case EffectKind::SiqueShuffleHands: continuation = PlayerWalkContinuation{}; break;
    case EffectKind::ArcanaWorld: continuation = CatalogWalkContinuation{}; break;
    case EffectKind::CuttingWind: continuation = OnceContinuation{}; break;
    case EffectKind::StormCyclone: continuation = CycloneContinuation{}; break;
    case EffectKind::BloodSearch: case EffectKind::SiqueOptionalDraw:
        continuation = RepeatedChoiceContinuation{-1, carriedResult}; break;
    default: continuation = RepeatedChoiceContinuation{}; break;
    }
}
void EffectFrame::insertBeforeCurrent(Effect effect) {
    suspendedContinuation = std::make_pair(cursor + 1, continuation);
    item.invalidateEffectsKey();
    item.effects.insert(item.effects.begin() + static_cast<std::ptrdiff_t>(cursor), effect);
}
}
