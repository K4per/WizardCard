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
inline constexpr const char* programVersion = "0.4.1-dev";
enum class CardType { Action, Analytic, Word, Formation, Seal };
enum class Zone { Deck, Hand, Action, Analysis, Words, Casting, Ash, Attached };
enum class SpellState { None, Analyzing, Ready, Pending, Active };
enum class Phase { Draw, Prepare, Main, Cast, End };
enum class TargetKind { None, Self, Opponent, EmptyEnemyFormation };
enum class EffectKind { Damage, Heal, Draw, AddTemporary, ClearTemporary, Destroy, CancelPreparation, GainMana };
struct Effect { EffectKind kind{}; int amount{}; };
struct CardDefinition {
    std::string id, name, text, rarity = "common";
    CardType type{};
    int cost{}, castCost{}, rank{}, analysisTurns{1}, duration{}, burden{}, extraDiscard{};
    int body{}, capacity{}, income{}, rings{}, maxRank{}, ringBonus{}, refundCast{};
    TargetKind target{};
    std::vector<Effect> effects, onPrepare, onEnd;
};
struct CardCatalog {
    std::string rulesVersion, cardSetVersion, contentHash;
    std::map<std::string, CardDefinition> cards;
    const CardDefinition& at(const std::string& id) const;
};
struct Payment { int turn{}, paid{}, load{}; bool canceled{}; };
struct CardInstance {
    CardId id{}; std::string definition; PlayerId owner{};
    Zone zone{Zone::Deck}; SpellState spell{};
    CardId host{}, sourceFormation{}, targetCard{}; PlayerId targetPlayer{-1};
    bool base{}; int analysisStarted{}, remaining{}, analysisLoad{}, castLoad{}, canceledTurn{-1};
    std::vector<Payment> payments;
};
struct TemporaryLoad { std::uint32_t id{}; PlayerId owner{}; CardId source{}; int amount{}, expiryTurn{}; };
struct PlayerState {
    int life{30}, mana{1}, ownTurn{}, actionsPlayed{}, formations{}, sealRemovals{};
    bool drawFailed{}, surrendered{};
    std::vector<CardId> deck;
};
struct SetFormation { CardId card{}, replace{}; };
struct RemoveFormation { CardId card{}; };
struct StartAnalysis { CardId card{}, formation{}; };
struct PrepareCast { CardId card{}, target{}, discard{}; };
struct PlayAction { CardId card{}, target{}; };
struct PreloadWord { CardId card{}; };
struct AttachSeal { CardId card{}, host{}; };
struct RemoveSeal { CardId card{}; };
struct Abandon { CardId card{}; };
struct Advance {};
struct Choose { DecisionId decision{}; std::uint32_t option{}; }; // 0 = pass/stop when permitted
struct Surrender {};
using Command = std::variant<SetFormation, RemoveFormation, StartAnalysis, PrepareCast, PlayAction,
    PreloadWord, AttachSeal, RemoveSeal, Abandon, Advance, Choose, Surrender>;
enum class DecisionKind { Response, CastOrder, TriggerOrder, Discard, ClearLoad, Overflow };
struct PendingDecision {
    DecisionId id{}; PlayerId player{}; DecisionKind kind{};
    std::vector<std::uint32_t> options; bool mayPass{};
};
struct GameEvent { std::string kind, text; int audience{-1}; CardId card{}; int amount{}; };
struct Trigger {
    std::uint32_t id{}, batch{}; PlayerId owner{}; CardId source{}, targetCard{};
    PlayerId targetPlayer{-1}; std::vector<Effect> effects;
};
struct EffectQueue {
    std::deque<Trigger> items;
    void append(Trigger trigger);
};
enum class AfterEffect { None, Action, Spell, Word };
struct EffectFrame { Trigger item; std::size_t cursor{}; AfterEffect after{}; int clearRemaining{-1}; };
struct Preparation { CardId spell{}; int responder{}; bool canceled{}; };
enum class Flow { Draw, Income, Progress, PrepareTriggers, Main, Cast, EndTriggers, Expire, Duration, Discard, Finish };
struct GameState {
    std::array<PlayerState,2> players;
    std::map<CardId,CardInstance> cards;
    std::vector<TemporaryLoad> temporary;
    PlayerId active{}, first{}; Phase phase{Phase::Draw}; Flow flow{Flow::Draw};
    int result{-1}; // -1 running, 0/1 winner, 2 draw
    std::uint32_t rng{}, nextLoad{1}, nextTrigger{1}, nextBatch{1};
    DecisionId nextDecision{1};
    EffectQueue queue;
    std::optional<PendingDecision> decision;
    std::optional<EffectFrame> effect;
    std::optional<Preparation> preparation;
    std::vector<GameEvent> events;
};
struct Rules {
    static int load(const GameState&, PlayerId);
    static int capacity(const GameState&, const CardCatalog&, PlayerId);
    static int rings(const GameState&, const CardCatalog&, CardId);
    static int occupied(const GameState&, CardId);
    static bool targetValid(const GameState&, const CardDefinition&, PlayerId, CardId);
    static std::vector<std::string> invariants(const GameState&, const CardCatalog&);
};
struct StateMaintenance {
    static void leave(GameState&, CardId);
    static void check(GameState&, const CardCatalog&);
};
struct EffectResolver {
    static void apply(GameState&, const CardCatalog&, const Trigger&, const Effect&);
};
struct CommandResult { bool accepted{}; std::string error; std::vector<GameEvent> events; std::optional<PendingDecision> waiting; std::string errorCode; };
struct LegalAction { std::string label; CardId source{}, target{}; Command command; };
struct PlayerView { int life{}, mana{}, load{}, capacity{}, handCount{}, deckCount{}, ownTurn{}, actionsPlayed{}; };
struct CardView { CardInstance instance; CardDefinition definition; int effectiveRings{}, occupiedRings{}, turnsToReady{}; };
struct GameView {
    PlayerId viewer{}, active{}; Phase phase{}; int result{};
    std::array<PlayerView,2> players;
    std::vector<CardView> cards;
    std::vector<TemporaryLoad> temporary;
    std::optional<PendingDecision> decision;
    std::vector<GameEvent> events;
    std::vector<LegalAction> actions;
};
class GameEngine {
public:
    GameEngine(CardCatalog catalog, std::array<std::vector<std::string>,2> decks, std::uint32_t seed);
    // Explicit scenario entry point, used by regression tools; never accepts client state.
    static GameEngine scenario(CardCatalog catalog, GameState state);
    CommandResult submit(PlayerId actor, const Command& command);
    GameView viewFor(PlayerId viewer) const;
    const GameState& state() const { return state_; }
    const CardCatalog& catalog() const { return catalog_; }
    std::string canonicalState() const;
    std::string digest() const;
private:
    GameEngine() = default;
    CardCatalog catalog_; GameState state_;
    std::string execute(PlayerId, const Command&);
    void pump();
    void decision(PlayerId, DecisionKind, std::vector<std::uint32_t>, bool = false);
    void enqueuePhase(bool end);
    void beginEffect(CardId, AfterEffect);
    void finishEffect();
    std::vector<LegalAction> legalActions(PlayerId) const;
};
std::string fingerprint(const std::string& bytes);
std::string phaseName(Phase);
std::string zoneName(Zone);
}
