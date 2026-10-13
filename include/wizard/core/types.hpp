#pragma once
#include <array>
#include <algorithm>
#include <cstdint>
#include <deque>
#include <map>
#include <optional>
#include <string>
#include <variant>
#include <vector>


namespace wizard {
using PlayerId = int;
using CardId = std::uint32_t;
using DecisionId = std::uint64_t;
using ChainId = std::uint64_t;
using LinkId = std::uint64_t;
inline constexpr const char *programVersion = "2.5.0-preview.2";
inline constexpr int startingLife = 40, maximumLife = 40;
inline constexpr int maximumMana = 20;
// Player target tokens share the existing deterministic command target field.
inline constexpr CardId playerTarget(PlayerId player) { return 0x40000001u + static_cast<CardId>(player); }
enum class CardType { Action, Analytic, Word, Formation, Seal, Talisman };
enum class Zone { Deck, Hand, Action, Analysis, Words, Casting, Ash, Attached, Resolving, SideDeck };
enum class SpellState { None, Analyzing, Ready, Pending, Active, Resolved };
enum class Phase { Draw, Prepare, Main, Cast, End };
enum class TargetKind {
    None,
    Self,
    Opponent,
    EmptyEnemyFormation,
    EnemyCard,
    OwnCard,
    AnyCard,
    PendingLink,
    PreparationRoot,
    OwnAnalyzingSpell,
    EnemySpellLink,
    Player,
    EnemyAshCard
};
enum class EffectKind {
    Damage,
    Heal,
    Draw,
    AddTemporary,
    ClearTemporary,
    Destroy,
    CancelPreparation,
    GainMana,
    NegateLink,
    OptionalDestroyOwnFormation,
    DiscardHand,
    DiscardFormation,
    ClearAllTemporary,
    OptionalPrepare,
    AddIndependent,
    ClearIndependent,
    AddSourceTemporary,
    AddTemporaryLife,
    BlockActions,
    CounterSpell,
    SearchAction,
    DestroyAmbush,
    AccelerateAnalysis,
    GrantResistance,
    Conceal,
    DelayEnemyAnalysis,
    ConcealEnemySpell,
    RevealEnemyAmbush,
    ChooseDamage,
    DestroyFormationSpells,
    ReturnEnemyAnalysis,
    SearchSameSchool,
    ReduceSpellDamage,
    RecoverEnemyAshSpell,
    RecoverFormation,
    RevealAllAmbush,
    ClearAnyLoad,
    DestroyEnemyAnalysis,
    ShieldForTurn
};
enum class ResponseWindow { PhaseStart, PhaseEnd, Action, Prepare, Cast, Trigger };
enum class EventOwner { Any, Self, Opponent };
enum class PhaseStep {
    Enter,
    StartWindow,
    Body,
    EndWindow,
    Cleanup,
    CleanupTriggers,
    Finish,
    EndTriggers,
    PrepareIncome
};
enum class DamageType {
    Fire,
    Cold,
    Radiant,
    Necrotic,
    Poison,
    Lightning,
    Psychic,
    Thunder,
    Force,
    Slashing,
    Bludgeoning,
    Piercing,
    Acid
};
enum class EffectRecipient { Target, Owner, Opponent };
enum class LinkKind { Phase, Action, Preparation, Spell, Response, Trigger };
std::string fingerprint(const std::string &bytes);
std::string damageName(DamageType);
std::string schoolName(const std::string &);
std::string phaseName(Phase);
std::string zoneName(Zone);
std::string windowName(ResponseWindow);
std::string linkName(LinkKind);
} // namespace wizard
