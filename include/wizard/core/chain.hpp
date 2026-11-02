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
    mutable std::uint64_t effectFingerprint{};
    mutable bool effectFingerprintReady{};
    std::uint64_t effectsKey() const {
        if (effectFingerprintReady) return effectFingerprint;
        std::uint64_t hash = 14695981039346656037ull;
        const auto consume = [&](std::uint64_t value) {
            for (int i = 0; i < 8; ++i) { hash ^= static_cast<std::uint8_t>(value >> (i * 8)); hash *= 1099511628211ull; }
        };
        consume(effects.size());
        for (const auto &effect : effects) {
            consume(static_cast<std::uint64_t>(effect.kind));
            consume(static_cast<std::uint64_t>(static_cast<std::int64_t>(effect.amount)));
            consume(static_cast<std::uint64_t>(effect.damageType));
            consume(static_cast<std::uint64_t>(effect.recipient));
            for (const auto byte : effect.semantic) {
                hash ^= static_cast<std::uint8_t>(byte);
                hash *= 1099511628211ull;
            }
            consume(effect.semantic.size());
        }
        effectFingerprint = hash;
        effectFingerprintReady = true;
        return hash;
    }
    void invalidateEffectsKey() const { effectFingerprintReady = false; }
};
struct EffectQueue {
    std::deque<Trigger> items;
    void append(Trigger trigger);
};
enum class AfterEffect { None, Action, Spell, Word };
struct RepeatedChoiceContinuation { int remaining{-1}, completed{}; };
struct EntityChoiceContinuation { std::optional<CardId> entity; int completed{}; bool inspected{}; };
enum class GuidanceStage { SelectField, SelectMode, SearchStorm, SearchSupporting };
struct GuidanceContinuation { GuidanceStage stage{GuidanceStage::SelectField}; };
struct TargetWalkContinuation { std::optional<std::size_t> next; int destroyed{}; };
struct PlayerWalkContinuation { std::optional<PlayerId> next; };
struct CatalogWalkContinuation { std::optional<std::size_t> next; };
struct OnceContinuation { bool performed{}; };
struct CycloneContinuation { bool offerDamage{}; int destroyed{}; };
using EffectContinuation = std::variant<RepeatedChoiceContinuation, EntityChoiceContinuation,
    GuidanceContinuation, TargetWalkContinuation, PlayerWalkContinuation,
    CatalogWalkContinuation, OnceContinuation, CycloneContinuation>;
struct EffectFrame {
    Trigger item;
    std::size_t cursor{};
    AfterEffect after{};
    LinkId link{};
    EffectContinuation continuation{RepeatedChoiceContinuation{}};
    int carriedResult{}; // Explicit result consumed by the following dependent effect.
    std::size_t continuationCursor{static_cast<std::size_t>(-1)};
    EffectKind continuationKind{EffectKind::Damage};
    // Inserted effects temporarily suspend the current typed continuation.
    std::optional<std::pair<std::size_t, EffectContinuation>> suspendedContinuation;
    EffectFrame() = default;
    EffectFrame(Trigger t, std::size_t c, AfterEffect a, int stage = -1, int count = 0, LinkId l = 0)
        : item(std::move(t)), cursor(c), after(a), link(l),
          continuation(RepeatedChoiceContinuation{stage, count}) {}
    template<class T> T &resume() {
        if (!std::holds_alternative<T>(continuation)) continuation = T{};
        return std::get<T>(continuation);
    }
    void beginCurrentEffect();
    void insertBeforeCurrent(Effect);
    int legacyStage() const;
    int legacyCount() const;
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
    bool hadActualLinks{};
};
} // namespace wizard
