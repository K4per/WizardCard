#pragma once
#include "wizard/core/catalog.hpp"

namespace wizard {
struct Trigger {
    std::uint32_t id{}, batch{};
    PlayerId owner{};
    CardId source{}, targetCard{};
    PlayerId targetPlayer{-1};
    std::vector<Effect> effects;
    TargetKind target{};
    bool requiresSource{};
    LinkId targetLink{};
    std::vector<CardId> targetCards;
};
struct EffectQueue {
    std::deque<Trigger> items;
    void append(Trigger trigger);
};
enum class AfterEffect { None, Action, Spell, Word };
struct EffectFrame {
    Trigger item;
    std::size_t cursor{};
    AfterEffect after{};
    int clearRemaining{-1};
    LinkId link{};
};
struct ChainLink {
    LinkId id{};
    LinkKind kind{};
    Trigger item;
    bool canceled{}, sourceLost{};
    std::string ability;
    int paymentIndex{-1}, preparationLoad{};
    int paid{}, addedLoad{};
    CardId discarded{};
    int speed{}, castCost{};
};
enum class ChainMode { Building, Resolving };
struct ChainState {
    ChainId id{};
    ResponseWindow window{};
    ChainMode mode{ChainMode::Building};
    PlayerId initiator{}, priority{};
    int passes{};
    std::vector<ChainLink> links;
    std::vector<CardId> responded;
};
} // namespace wizard
