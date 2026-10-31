#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
enum class Capability : std::uint8_t {
    ActivateStormCrestDestroy, ActivateStormHeartChoice, ActivateStormSearch,
    ArcanaChariot, ArcanaDeath, ArcanaDevil, ArcanaEmperorCost, ArcanaFoolCondition,
    ArcanaHangedCost, ArcanaPriestessCost, ArcanaStrength, ArcanaTemperance,
    AshEvocationDiscount, BloodAltar, BloodCurse, BloodHeart, BloodHeartmark,
    BloodLastStand, BloodMark, BloodOfferingCost, BloodPool, BloodSacrificeReward,
    BloodSameOrigin, BloodTransfusion, BloodVein, BloodWristCost,
    ClearLoadOnAnalysis, DestroyEnemyAmbushEnd, DiscardSpellSearch, DistinctHostSchools,
    DoubleEnemyConcentrationUpkeep, EffectImmune, EffectIndestructible, EndPhaseStart,
    FireDamageSpell, GreatStorm, HostCastLifeForTemporary, HostEvocationDamagePlusOne,
    HostFireOnly, HostHealPlusOne, MaxTwo, Moon, OpponentSpellLoad4, PingOnSpellLeave,
    QuickHostAnalysis1, QuickHostAnalysis2, RequiresStormFormation,
    RequiresStormHeartSetup, RequiresStormSetup, ReturnFormationFromAsh,
    SearchGreatStormOnSetup, SelfAmbushOnLeave, SevenStarsBanner, SevenStarsEffectOnly,
    SevenStarsKaiyang, SevenStarsLamp, SevenStarsYaoguang, SiqueCampfireOnly,
    SiqueCauldronOnly, SiqueDetach1, SiqueDetach2, SiqueDetach3,
    SiqueRequiresCampfire, SiqueRequiresCauldron, StormFormationHostOnly,
    StormFormationHostRestriction, StormHeartUpgrade, StormReleaseCounter,
    StormSearch, StormSearchTalisman, InspectEnemyAmbush,
    SurchargeEnemyTimedCast, ThunderDamageBonus, Unique, BloodDebt, BloodRepayment,
    ArcanaMagician, ArcanaTower, ArcanaLovers, CloudMist, QixingDivination, QixingRuin, QixingShift,
    QixingTianquan, QixingTianshu, QixingTianxuan, QixingYuheng, SiqueAromaLure,
    SiqueCampfire, SiqueCauldron, StormCrest, StormHeart, SwiftWind, ThunderSummon,
    WindStrike, QixingGaze, QixingReturn, ArcanaWorld, MoonMaterialFormation,
    SiqueBoilingSurge, SiqueChili, SiqueSugar, SiqueVinegar, CounterSpell,
    // v2.6 staging traits are parsed and retained as executable identities.
    // Their card-specific timing/choice adapter is keyed by the trait/effect
    // semantic rather than by inventing a new enum for every workbook clause.
    V260Structured
};
struct Effect {
    EffectKind kind{};
    int amount{};
    DamageType damageType{DamageType::Force};
    EffectRecipient recipient{};
    std::string semantic;
    bool operator==(const Effect &other) const {
        return kind == other.kind && amount == other.amount && damageType == other.damageType &&
            recipient == other.recipient && semantic == other.semantic;
    }
    bool operator!=(const Effect &other) const { return !(*this == other); }
};
struct ResponseAbility {
    std::string id;
    std::string name;
    bool fromHand{}, fromWords{}, requiresSource{};
    int handCost{-1}, preloadedCost{}, burden{}, extraDiscard{}, targetCount{1};
    EventOwner eventOwner{EventOwner::Any};
    TargetKind target{};
    std::vector<ResponseWindow> windows;
    std::vector<Effect> effects;
    bool fromAnalysis{}, fromAmbush{};
    std::vector<Phase> phases;
    bool fromCasting{};
};
enum class RecipeCostKind { Mana, SacrificeFormation, SacrificeSpell, DiscardHand, Life };
struct RecipeCost {
    RecipeCostKind kind{RecipeCostKind::Mana};
    int amount{};
    std::string definition, school;
    std::vector<std::string> definitions, schools;
    bool readyOnly{}, distinctSchools{}, fireDamage{};
};
struct CardDefinition {
    std::string id, name, text, rarity = "common", flavor;
    std::string settingConditions; // Display derived from the executable formation recipe.
    CardType type{};
    int cost{}, castCost{}, rank{}, analysisTurns{1}, duration{}, burden{}, extraDiscard{};
    int concentrationUpkeep{-1};
    int body{}, capacity{}, income{}, rings{}, maxRank{}, ringBonus{}, refundCast{};
    int incomeBonus{}, refundSetting{};
    bool immediate{}, concentration{}, opponentDestroyProtected{};
    TargetKind target{};
    std::vector<Effect> effects, onPrepare, onEnd;
    bool baseEligible{};
    int targetCount{1};
    std::vector<std::string> tags;
    std::vector<std::string> traits; // Content identity / format-4 adapter.
    std::vector<Capability> capabilities; // Compiled once from the executable trait registry.
    std::vector<ResponseAbility> responses;
    std::vector<Effect> onDraw, onMain, onCast, onLeave, onMana;
    std::string school;
    int speed{1}, capacityBonus{}, rankBonus{}, castCostBonus{}, damageBonus{};
    bool responseOnly{}, dynamicCost{}, baseAnalysisDiscount{}, quickRankOne{};
    std::vector<DamageType> resistances, immunities;
    std::string number, illustrationId;
    int level{1};
    bool perpetual{};
    std::vector<RecipeCost> recipe;
};
struct CardCatalog {
    std::string rulesVersion, cardSetVersion, contentHash;
    int mechanicsRevision{};
    std::map<std::string, CardDefinition> cards;
    int baseLoadCapacity{};
    const CardDefinition &at(const std::string &id) const;
    void compileAbilities();
};
inline bool hasCapability(const CardDefinition &definition, Capability capability) {
    return std::find(definition.capabilities.begin(), definition.capabilities.end(), capability) != definition.capabilities.end();
}
std::optional<Capability> findCapability(const std::string &name);

} // namespace wizard
