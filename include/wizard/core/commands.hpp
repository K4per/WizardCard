#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
struct SetFormation {
    CardId card{}, replace{};
};
struct RemoveFormation {
    CardId card{};
};
struct StartAnalysis {
    CardId card{}, formation{};
};
struct PrepareCast {
    CardId card{}, target{}, discard{};
    std::vector<CardId> targets;
    DecisionId decision{}; // nonzero only for an effect-granted preparation
};
struct PlayAction {
    CardId card{}, target{};
    std::vector<CardId> targets;
};
struct PreloadWord {
    CardId card{};
    bool release{};
};
struct AttachSeal {
    CardId card{}, host{};
};
struct RemoveSeal {
    CardId card{};
};
struct Abandon {
    CardId card{};
};
struct Advance {};
struct Choose {
    DecisionId decision{};
    std::uint32_t option{};
}; // 0 = pass/stop when permitted
struct Surrender {};
struct Respond {
    DecisionId decision{};
    CardId card{};
    std::string ability;
    CardId target{};
    LinkId link{};
    CardId discard{};
    std::vector<CardId> targets;
};
struct PassResponse {
    DecisionId decision{};
};
struct AdvancePhase {
    DecisionId gate{};
};
struct SetAmbush {
    CardId card{}, formation{};
};
struct FlipAmbush {
    CardId card{}, target{};
    std::vector<CardId> targets;
};
struct ActivateSpell {
    CardId card{}, target{};
    std::vector<CardId> targets;
};
using Command = std::variant<SetFormation, RemoveFormation, StartAnalysis, PrepareCast, PlayAction,
                             PreloadWord, AttachSeal, RemoveSeal, Abandon, Advance, Choose, Surrender,
                             Respond, PassResponse, AdvancePhase, SetAmbush, FlipAmbush, ActivateSpell>;
enum class DecisionKind {
    Response,
    CastOrder,
    TriggerOrder,
    Discard,
    ClearLoad,
    Overflow,
    DestroyOwnFormation,
    EffectDiscard,
    EffectPrepare,
    Concentration,
    DestroyAmbush,
    SearchDeck
};
struct PendingDecision {
    DecisionId id{};
    PlayerId player{};
    DecisionKind kind{};
    std::vector<std::uint32_t> options;
    bool mayPass{};
};
} // namespace wizard
