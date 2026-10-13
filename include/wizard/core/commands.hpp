#pragma once
#include "wizard/core/types.hpp"

namespace wizard {
struct SetFormation {
    CardId card{}, replace{};
    std::vector<CardId> materials;
};
struct RemoveFormation {
    CardId card{};
};
struct StartAnalysis {
    CardId card{}, formation{};
    CardId target{};
    std::vector<CardId> targets;
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
    CardId formation{}, target{};
    std::vector<CardId> targets;
    CardId discard{}; // Optional executable spell cost, such as true-strike.
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
    int ringIndex{-1}; // ReturnSpell may select any empty ring; -1 keeps first-free compatibility.
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
    bool skipCast{};
};
struct SetAmbush {
    CardId card{}, formation{};
};
struct FlipAmbush {
    CardId card{}, target{};
    std::vector<CardId> targets;
    DecisionId decision{};
    CardId discard{};
};
struct ActivateSpell {
    CardId card{}, target{};
    std::vector<CardId> targets;
    CardId discard{};
};
// Confirms a private side-deck entity; both confirmations reveal the formations together.
struct SelectInitialFormation { CardId card{}; };
// Authority-generated terminal event; transports must reject user-submitted timeouts.
struct Timeout {};
using Command = std::variant<SetFormation, RemoveFormation, StartAnalysis, PrepareCast, PlayAction,
                             PreloadWord, AttachSeal, RemoveSeal, Abandon, Advance, Choose, Surrender,
                             Respond, PassResponse, AdvancePhase, SetAmbush, FlipAmbush, ActivateSpell,
                             SelectInitialFormation, Timeout>;
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
    SearchDeck,
    ReturnSpell,
    EffectChoice,
    DamageChoice,
    ClearAnyLoad
};
struct PendingDecision {
    DecisionId id{};
    PlayerId player{};
    DecisionKind kind{};
    std::vector<std::uint32_t> options;
    bool mayPass{};
};
} // namespace wizard
