#pragma once
#include "wizard/core/types.hpp"
#include <type_traits>

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
    DecisionId decision{}; // nonzero only when setting a word spell as a response.
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
enum class ChoiceKind : std::uint8_t { Pass, Card, Player, Load, Mode, Payment, Link, Trigger, Legacy };
enum class ChoiceMode : std::uint32_t {
    Confirm=1, SevenDamage=2, SevenLoad=3, DelayAnalysis=4, CycloneDamage=5,
    Draw=6, StormSearch=7, StormPoints=8, SupportingSearch=9,
    DamageTypeBase=100, HeartDraw=200, HeartRecover=201,
    ArcanaDeathNecrotic=300, ArcanaDeathRadiant=301
};
struct ChoiceValue {
    ChoiceKind kind{ChoiceKind::Pass};
    std::uint64_t value{};
    ChoiceValue() = default;
    ChoiceValue(ChoiceKind k, std::uint64_t v) : kind(k), value(v) {}
    // Format-4 input adapter; never produced by the rule engine.
    ChoiceValue(std::uint32_t old) : kind(old ? ChoiceKind::Legacy : ChoiceKind::Pass), value(old) {}
    static ChoiceValue card(CardId id) { return {ChoiceKind::Card, id}; }
    static ChoiceValue player(PlayerId id) { return {ChoiceKind::Player, static_cast<std::uint64_t>(id)}; }
    static ChoiceValue load(std::uint32_t id) { return {ChoiceKind::Load, id}; }
    static ChoiceValue mode(ChoiceMode mode) { return {ChoiceKind::Mode, static_cast<std::uint32_t>(mode)}; }
    static ChoiceValue damage(DamageType type) {
        return mode(static_cast<ChoiceMode>(static_cast<std::uint32_t>(ChoiceMode::DamageTypeBase) +
                                            static_cast<std::uint32_t>(type)));
    }
    explicit operator bool() const { return kind != ChoiceKind::Pass; }
    bool operator==(const ChoiceValue &other) const {
        if (kind == other.kind && value == other.value) return true;
        // Keep source compatibility for in-process format-4 helpers. Legacy
        // numeric values are never emitted by the current wire codec, but
        // they can still be compared with their typed current representation
        // while old fixtures are adapted.
        const auto matchesLegacy = [](ChoiceKind typed, std::uint64_t typedValue,
                                      std::uint64_t legacy) {
            if (typed == ChoiceKind::Card || typed == ChoiceKind::Load ||
                typed == ChoiceKind::Payment || typed == ChoiceKind::Trigger ||
                typed == ChoiceKind::Link)
                return typedValue == legacy;
            if (typed == ChoiceKind::Player)
                return legacy == playerTarget(static_cast<PlayerId>(typedValue));
            if (typed == ChoiceKind::Mode) {
                switch (legacy) {
                case 0xffffffa1u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::Confirm);
                case 0xffffffb1u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::SevenDamage);
                case 0xffffffb2u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::SevenLoad);
                case 0xffffffd1u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::DelayAnalysis);
                case 0xffffffd2u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::CycloneDamage);
                case 0xffffffe1u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::Draw);
                case 0xfffffff1u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::StormSearch);
                case 0xfffffff2u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::StormPoints);
                case 0xfffffff3u: return typedValue == static_cast<std::uint32_t>(ChoiceMode::SupportingSearch);
                case 1: return typedValue == static_cast<std::uint32_t>(ChoiceMode::HeartDraw) ||
                               typedValue == static_cast<std::uint32_t>(ChoiceMode::ArcanaDeathNecrotic);
                case 2: return typedValue == static_cast<std::uint32_t>(ChoiceMode::HeartRecover) ||
                               typedValue == static_cast<std::uint32_t>(ChoiceMode::ArcanaDeathRadiant);
                default: return false;
                }
            }
            return typed == ChoiceKind::Pass && legacy == 0;
        };
        if (kind == ChoiceKind::Legacy) return matchesLegacy(other.kind, other.value, value);
        if (other.kind == ChoiceKind::Legacy) return matchesLegacy(kind, value, other.value);
        return false;
    }
    bool operator!=(const ChoiceValue &other) const { return !(*this == other); }
};
struct Choose {
    DecisionId decision{};
    ChoiceValue option;
    int ringIndex{-1};
};
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
struct ActivateAbility {
    CardId card{}, target{};
    std::string ability;
};
// Confirms a private side-deck entity; both confirmations reveal the formations together.
struct SelectInitialFormation { CardId card{}; };
// Authority-generated terminal event; transports must reject user-submitted timeouts.
struct Timeout {};
using Command = std::variant<SetFormation, RemoveFormation, StartAnalysis, PrepareCast, PlayAction,
                             PreloadWord, AttachSeal, RemoveSeal, Abandon, Advance, Choose, Surrender,
                             Respond, PassResponse, AdvancePhase, SetAmbush, FlipAmbush, ActivateSpell,
                             SelectInitialFormation, Timeout, ActivateAbility>;
// Stable serialized identifiers; independent of variant storage order.
enum class CommandKind : std::uint16_t {
    SetFormation=0, RemoveFormation=1, StartAnalysis=2, PrepareCast=3, PlayAction=4,
    PreloadWord=5, AttachSeal=6, RemoveSeal=7, Abandon=8, Advance=9, Choose=10,
    Surrender=11, Respond=12, PassResponse=13, AdvancePhase=14, SetAmbush=15,
    FlipAmbush=16, ActivateSpell=17, SelectInitialFormation=18, Timeout=19, ActivateAbility=20
};
inline CommandKind commandKind(const Command &command) {
    return std::visit([](const auto &value) {
        using T=std::decay_t<decltype(value)>;
#define WIZARD_COMMAND_KIND(Type) if constexpr (std::is_same_v<T, Type>) return CommandKind::Type;
        WIZARD_COMMAND_KIND(SetFormation) WIZARD_COMMAND_KIND(RemoveFormation)
        WIZARD_COMMAND_KIND(StartAnalysis) WIZARD_COMMAND_KIND(PrepareCast)
        WIZARD_COMMAND_KIND(PlayAction) WIZARD_COMMAND_KIND(PreloadWord)
        WIZARD_COMMAND_KIND(AttachSeal) WIZARD_COMMAND_KIND(RemoveSeal)
        WIZARD_COMMAND_KIND(Abandon) WIZARD_COMMAND_KIND(Advance)
        WIZARD_COMMAND_KIND(Choose) WIZARD_COMMAND_KIND(Surrender)
        WIZARD_COMMAND_KIND(Respond) WIZARD_COMMAND_KIND(PassResponse)
        WIZARD_COMMAND_KIND(AdvancePhase) WIZARD_COMMAND_KIND(SetAmbush)
        WIZARD_COMMAND_KIND(FlipAmbush) WIZARD_COMMAND_KIND(ActivateSpell)
        WIZARD_COMMAND_KIND(SelectInitialFormation) WIZARD_COMMAND_KIND(Timeout)
        WIZARD_COMMAND_KIND(ActivateAbility)
#undef WIZARD_COMMAND_KIND
    }, command);
}
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
    ClearAnyLoad,
    StormSearch,
    StormHeartChoice,
    StormHeartRecover,
    Payment
};
struct PendingDecision {
    DecisionId id{};
    PlayerId player{};
    DecisionKind kind{};
    std::vector<ChoiceValue> options;
    bool mayPass{};
};
} // namespace wizard
