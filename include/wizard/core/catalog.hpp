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
};
struct CardDefinition {
    std::string id, name, text, rarity = "common", flavor;
    CardType type{};
    int cost{}, castCost{}, rank{}, analysisTurns{1}, duration{}, burden{}, extraDiscard{};
    int body{}, capacity{}, income{}, rings{}, maxRank{}, ringBonus{}, refundCast{};
    int incomeBonus{}, refundSetting{};
    bool immediate{}, concentration{}, opponentDestroyProtected{};
    TargetKind target{};
    std::vector<Effect> effects, onPrepare, onEnd;
    bool baseEligible{};
    int targetCount{1};
    std::vector<std::string> tags;
    std::vector<ResponseAbility> responses;
    std::vector<Effect> onDraw, onMain, onCast, onLeave, onMana;
    std::string school;
    int speed{1}, capacityBonus{}, rankBonus{}, castCostBonus{}, damageBonus{};
    bool responseOnly{}, baseAnalysisDiscount{}, quickRankOne{};
    std::vector<DamageType> resistances, immunities;
};
struct CardCatalog {
    std::string rulesVersion, cardSetVersion, contentHash;
    std::map<std::string, CardDefinition> cards;
    bool advancedRules{};
    // Offline Alpha v2 rule fixtures only, until construction/phase gates are complete.
    // The production JSON loader deliberately does not enable this draft mode.
    bool alphaV2Draft{};
    int baseLoadCapacity{};
    const CardDefinition &at(const std::string &id) const;
};
} // namespace wizard
