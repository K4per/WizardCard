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
    bool faceDown{};
    std::uint64_t ambushedTurn{};
    int quickAnalysisTurn{-1};
};
struct TemporaryLoad {
    std::uint32_t id{};
    PlayerId owner{};
    CardId source{};
    int amount{}, expiryTurn{};
    bool independent{}; // does not expire and is excluded from temporary-load removal
    bool sourceBound{}; // loads expire only when their source leaves
};
} // namespace wizard
