#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
struct Payment {
    int turn{}, paid{}, load{};
    bool canceled{};
};
struct CardInstance {
    CardId id{};
    std::string definition;
    PlayerId owner{};
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
};
struct TemporaryLoad {
    std::uint32_t id{};
    PlayerId owner{};
    CardId source{};
    int amount{}, expiryTurn{};
    bool independent{}; // does not expire and is excluded from temporary-load removal
    bool sourceBound{}; // loads expire only when their source leaves
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
