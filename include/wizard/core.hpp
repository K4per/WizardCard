#pragma once
#include <array>
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
inline constexpr const char *programVersion = "1.5.0-dev";
inline constexpr int startingLife = 40, maximumLife = 40;
enum class CardType { Action, Analytic, Word, Formation, Seal };
enum class Zone { Deck, Hand, Action, Analysis, Words, Casting, Ash, Attached, Resolving };
enum class SpellState { None, Analyzing, Ready, Pending, Active };
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
    EnemySpellLink
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
    Conceal
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
    Piercing
};
enum class EffectRecipient { Target, Owner, Opponent };
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
    int baseLoadCapacity{};
    const CardDefinition &at(const std::string &id) const;
};
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
struct PlayerState {
    int life{startingLife}, mana{1}, ownTurn{}, actionsPlayed{}, formations{}, sealRemovals{};
    bool drawFailed{}, surrendered{};
    std::vector<CardId> deck;
    int temporaryLife{}, blockedActionTurn{-1};
};
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
struct GameEvent {
    std::string kind, text;
    int audience{-1};
    CardId card{};
    int amount{};
    // Presentation metadata. Logs retain their text; consumers need not parse localized prose.
    std::optional<EffectKind> effectType{};
    int actualAmount{};
    PlayerId affectedPlayer{-1};
    bool spellReleased{};
};
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
enum class LinkKind { Phase, Action, Preparation, Spell, Response, Trigger };
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
struct PlayerDeck {
    std::string baseFormation{"balance"};
    std::vector<std::string> cards;
};
struct MatchConfig {
    std::array<PlayerDeck, 2> players;
    std::uint32_t seed{42};
};
enum class Flow {
    Draw,
    Income,
    Progress,
    PrepareTriggers,
    Main,
    Cast,
    EndTriggers,
    Expire,
    Duration,
    Discard,
    Finish
};
struct GameState {
    std::array<PlayerState, 2> players;
    std::map<CardId, CardInstance> cards;
    std::vector<TemporaryLoad> temporary;
    PlayerId active{}, first{};
    std::uint64_t globalTurn{1};
    Phase phase{Phase::Draw};
    Flow flow{Flow::Draw};
    PhaseStep phaseStep{PhaseStep::Enter};
    DecisionId phaseGate{};
    bool durationApplied{};
    int result{-1}; // -1 running, 0/1 winner, 2 draw
    std::uint32_t rng{}, nextLoad{1}, nextTrigger{1}, nextBatch{1};
    DecisionId nextDecision{1};
    ChainId nextChain{1};
    LinkId nextLink{1};
    EffectQueue queue;
    std::optional<PendingDecision> decision;
    std::optional<EffectFrame> effect;
    std::optional<ChainState> chain;
    std::deque<ChainLink> deferredPreparations;
    std::vector<GameEvent> events;
};
struct Rules {
    static int load(const GameState &, PlayerId);
    static int capacity(const GameState &, const CardCatalog &, PlayerId);
    static int rings(const GameState &, const CardCatalog &, CardId);
    static int maxRank(const GameState &, const CardCatalog &, CardId);
    static int analysisCost(const GameState &, const CardCatalog &, CardId, CardId);
    static int castCost(const GameState &, const CardCatalog &, CardId);
    static int damage(const GameState &, const CardCatalog &, PlayerId, const Effect &);
    static int occupied(const GameState &, CardId);
    static bool targetValid(const GameState &, const CardDefinition &, PlayerId, CardId);
    static bool targetValid(const GameState &, TargetKind, PlayerId, CardId);
    static bool linkTargetValid(const GameState &, TargetKind, LinkId);
    static std::vector<std::string> deckErrors(const CardCatalog &, const PlayerDeck &);
    static std::vector<std::string> invariants(const GameState &, const CardCatalog &);
};
struct StateMaintenance {
    static void leave(GameState &, CardId, const CardCatalog * = nullptr);
    static void check(GameState &, const CardCatalog &);
};
struct EffectResolver {
    static void apply(GameState &, const CardCatalog &, const Trigger &, const Effect &);
};
struct CommandResult {
    bool accepted{};
    std::string error;
    std::vector<GameEvent> events;
    std::optional<PendingDecision> waiting;
    std::string errorCode;
};
struct LegalAction {
    std::string label;
    CardId source{}, target{};
    Command command;
};
struct PlayerView {
    int life{}, mana{}, load{}, capacity{}, handCount{}, deckCount{}, ownTurn{}, actionsPlayed{};
    int temporaryLife{}, blockedActionTurn{-1};
    std::vector<DamageType> resistances, immunities;
};
struct CardView {
    CardInstance instance;
    CardDefinition definition;
    int effectiveRings{}, occupiedRings{}, turnsToReady{};
    bool hidden{};
    int effectiveCastCost{};
};
struct GameView {
    PlayerId viewer{}, active{};
    Phase phase{};
    int result{};
    std::array<PlayerView, 2> players;
    std::vector<CardView> cards;
    std::vector<TemporaryLoad> temporary;
    std::optional<PendingDecision> decision;
    std::vector<GameEvent> events;
    std::vector<LegalAction> actions;
    PhaseStep phaseStep{};
    DecisionId phaseGate{};
    std::optional<ChainState> chain;
    std::vector<Trigger> triggers; // only visible choices of this viewer's trigger-order decision
};
class GameEngine {
  public:
    GameEngine(CardCatalog catalog, std::array<std::vector<std::string>, 2> decks, std::uint32_t seed);
    GameEngine(CardCatalog catalog, const MatchConfig &);
    // Explicit scenario entry point, used by regression tools; never accepts client state.
    static GameEngine scenario(CardCatalog catalog, GameState state);
    CommandResult submit(PlayerId actor, const Command &command);
    GameView viewFor(PlayerId viewer) const;
    const GameState &state() const {
        return state_;
    }
    const CardCatalog &catalog() const {
        return catalog_;
    }
    std::string canonicalState() const;
    std::string cycleKey() const;
    std::string digest() const;

  private:
    GameEngine() = default;
    CardCatalog catalog_;
    GameState state_;
    std::string execute(PlayerId, const Command &);
    void pump();
    void decision(PlayerId, DecisionKind, std::vector<std::uint32_t>, bool = false);
    void enqueuePhase(bool end);
    void beginEffect(CardId, AfterEffect);
    void finishEffect();
    void startChain(ResponseWindow, ChainLink, PlayerId priority);
    void finishLink(bool success);
    void passResponse();
    std::string validateResponse(PlayerId, const Respond &) const;
    std::vector<Respond> responsesFor(PlayerId, DecisionId) const;
    void startTrigger(Trigger);
    std::string prepare(PlayerId, const PrepareCast &, bool deferred);
    std::vector<PrepareCast> preparationsFor(PlayerId, DecisionId) const;
    std::string stateKey(bool logical) const;
    std::vector<LegalAction> legalActions(PlayerId) const;
};
std::string fingerprint(const std::string &bytes);
std::string damageName(DamageType);
std::string schoolName(const std::string &);
std::string phaseName(Phase);
std::string zoneName(Zone);
std::string windowName(ResponseWindow);
std::string linkName(LinkKind);
} // namespace wizard
