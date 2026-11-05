#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
enum class ModifierKind { SetupDelay, BlockConcentration, WordCastDamage, DoubleLoad,
    ProtectPlayer, BlockAnalytic, DamageTakenBonus, HalfDamage, HalfRings, BorrowControl, SetupDamage };
enum class ModifierExpiry { TurnEnd, NextDraw, NextPrepare, NextPrepareEnd, NextMain, NextTurnEnd };
struct SpellModifier {
    ModifierKind kind{};
    CardId source{}, target{};
    PlayerId owner{}, player{};
    int amount{};
    DamageType damageType{DamageType::Force};
    ModifierExpiry expiry{};
    std::uint64_t expiresTurn{};
    int expiresOwnTurn{};
    CardId returnHost{};
    int returnRing{-1}, returnController{-1};
};
struct Payment {
    int turn{}, paid{}, load{};
    bool canceled{};
};
struct CardInstance {
    CardId id{};
    std::string definition;
    PlayerId owner{};
    // The original owner never changes when an effect temporarily controls a card.
    // -1 keeps hand-authored/legacy scenarios compatible; gameplay treats it as owner.
    PlayerId controller{-1};
    Zone zone{Zone::Deck};
    SpellState spell{};
    CardId host{}, sourceFormation{}, targetCard{};
    PlayerId targetPlayer{-1};
    bool base{};
    int analysisStarted{}, remaining{}, analysisLoad{}, castLoad{}, canceledTurn{-1};
    std::vector<Payment> payments;
    std::vector<CardId> targets;
    int settingPaid{}, concentrationCost{};
    int stormPoints{};
    int stormReleaseProgress{};
    bool faceDown{};
    bool publiclyRevealed{}; // Showing a concealed card exposes its face without flipping it.
    std::uint64_t ambushedTurn{};
    int quickAnalysisTurn{-1};
    int stormAbilityTurn{-1};
    int analysisProgress{};
    std::uint64_t fieldOrder{};
    int ringIndex{-1};
    int sourceRingIndex{-1};
    bool wasAmbushed{};
    CardId siquePartner{};
    std::uint64_t siquePairTurn{};
    CardId paidDiscard{};
    int damageCounter{};
};
inline PlayerId controllerOf(const CardInstance &card) {
    return card.controller < 0 ? card.owner : card.controller;
}
struct TemporaryLoad {
    std::uint32_t id{};
    PlayerId owner{};
    CardId source{};
    int amount{}, expiryTurn{};
    bool independent{}; // does not expire and is excluded from temporary-load removal
    bool sourceBound{}; // loads expire only when their source leaves
    bool expiresAtEnd{};
};
struct TemporaryLife {
    std::uint32_t id{};
    PlayerId owner{};
    CardId source{};
    int amount{};
    int expiryOwnTurn{};
    std::uint64_t acquiredTurn{};
    Phase acquiredPhase{Phase::Draw};
};
} // namespace wizard
