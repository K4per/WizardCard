#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
struct Effect {
    EffectKind kind{};
    int amount{};
    DamageType damageType{DamageType::Force};
    EffectRecipient recipient{};
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
enum class RecipeCostKind { Mana, SacrificeFormation, SacrificeSpell, DiscardHand };
struct RecipeCost {
    RecipeCostKind kind{RecipeCostKind::Mana};
    int amount{};
    std::string definition, school;
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
    std::vector<std::string> traits; // Validated executable 2.0 card capabilities.
    std::vector<ResponseAbility> responses;
    std::vector<Effect> onDraw, onMain, onCast, onLeave, onMana;
    std::string school;
    int speed{1}, capacityBonus{}, rankBonus{}, castCostBonus{}, damageBonus{};
    bool responseOnly{}, baseAnalysisDiscount{}, quickRankOne{};
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
    bool advancedRules{};
    // Offline Alpha v2 rule fixtures only, until construction/phase gates are complete.
    // The production JSON loader deliberately does not enable this draft mode.
    bool alphaV2Draft{};
    bool alphaV2{};
    bool usesAlphaV2() const { return alphaV2 || alphaV2Draft; }
    bool usesAlphaV25() const { return mechanicsRevision >= 3; }
    int baseLoadCapacity{};
    const CardDefinition &at(const std::string &id) const;
};
} // namespace wizard
