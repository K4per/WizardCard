#include "wizard/network.hpp"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <limits>
#include <random>
#include <sstream>
namespace wizard {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Effect, kind, amount, damageType, recipient)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Payment, turn, paid, load, canceled)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE_WITH_DEFAULT(CardInstance, id, definition, owner, controller, zone, spell, host, sourceFormation,
                                   targetCard, targetPlayer, base, analysisStarted, remaining, analysisLoad,
                                   castLoad, canceledTurn, payments, targets, settingPaid, concentrationCost, stormPoints,
                                   stormReleaseProgress,
                                   faceDown, publiclyRevealed, ambushedTurn, quickAnalysisTurn, stormAbilityTurn, analysisProgress, fieldOrder,
                                   ringIndex, wasAmbushed, sourceRingIndex)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TemporaryLoad, id, owner, source, amount, expiryTurn, independent,
                                   sourceBound)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TemporaryLife, id, owner, source, amount, expiryOwnTurn, acquiredTurn, acquiredPhase)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerView, life, mana, load, capacity, handCount, deckCount, ownTurn,
                                   actionsPlayed, temporaryLife, blockedActionTurn, resistances, immunities,
                                   sideDeckCount, exhaustion, lifeLimit, sevenMode)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(MatchClockView, enabled, running, remainingMs, runningPlayers, runningPlayer, sequence)
void to_json(Json &j, const ChoiceValue &value) { j = encodeChoice(value); }
void from_json(const Json &j, ChoiceValue &value) { value = decodeChoice(j); }
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PendingDecision, id, player, kind, options, mayPass)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Trigger, id, batch, owner, source, targetCard, targetPlayer, effects,
                                   target, requiresSource, targetLink, targetCards)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChainLink, id, kind, item, canceled, sourceLost, ability, paymentIndex,
                                   preparationLoad, paid, addedLoad, discarded, speed, castCost)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChainState, id, window, mode, initiator, priority, passes, links,
                                   responded, hadActualLinks)
} // namespace wizard
namespace wizard::net {
namespace {
bool wide(const std::string &k) {
    return k == "id" || k == "decision" || k == "gate" || k == "link" || k == "targetLink" ||
           k == "phaseGate" || k == "ambushedTurn" || k == "eventBase" || k == "fieldOrder" ||
           k == "globalTurn" || k == "sequence";
}
void transform(Json &j, bool encode) {
    if (j.is_array())
        for (auto &x : j)
            transform(x, encode);
    else if (j.is_object())
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (wide(it.key()) && encode && it.value().is_number_unsigned())
                it.value() = std::to_string(it.value().get<std::uint64_t>());
            else if (wide(it.key()) && !encode && it.value().is_string())
                it.value() = number(it.value());
            else
                transform(it.value(), encode);
        }
}
} // namespace
std::uint64_t number(const Json &j) {
    if (!j.is_string())
        throw std::runtime_error("Expected decimal identifier");
    auto s = j.get<std::string>();
    std::uint64_t n{};
    auto r = std::from_chars(s.data(), s.data() + s.size(), n);
    if (s.empty() || r.ec != std::errc{} || r.ptr != s.data() + s.size())
        throw std::runtime_error("Invalid decimal identifier");
    return n;
}
std::string token() {
    std::random_device rd;
    std::ostringstream s;
    for (int i = 0; i < 8; ++i)
        s << std::hex << std::setw(8) << std::setfill('0') << rd();
    return s.str();
}
Json wireCommand(const Command &c) {
    auto j = encodeCommand(c);
    transform(j, true);
    return j;
}
Command unwireCommand(Json j) {
    transform(j, false);
    return decodeCommand(j);
}
Json hello(const Content &c) {
    return {{"type", "Hello"},
            {"protocol", 4},
            {"program", programVersion},
            {"rules", c.catalog.rulesVersion},
            {"mechanics", c.catalog.mechanicsRevision},
            {"cards", c.catalog.cardSetVersion},
            {"hash", c.catalog.contentHash}};
}
Json encodeView(const GameView &v) {
    Json j = {{"viewer", v.viewer},       {"active", v.active},       {"phase", v.phase},
              {"result", v.result},       {"players", v.players},     {"temporary", v.temporary},
              {"temporaryLives", v.temporaryLives},
              {"phaseStep", v.phaseStep}, {"phaseGate", v.phaseGate}, {"triggers", v.triggers},
              {"opening", v.opening}, {"initialFormationConfirmed", v.initialFormationConfirmed},
              {"initialFormation", v.initialFormation}, {"globalTurn", v.globalTurn}, {"clock", v.clock},
              {"technicalPaused", v.technicalPaused}};
    j["actionSummary"] = {{"actionable", v.actionSummary.actionable},
        {"hasNonPhaseAction", v.actionSummary.hasNonPhaseAction},
        {"safeAdvance", v.actionSummary.safeAdvance ? wireCommand(*v.actionSummary.safeAdvance) : Json()}};
    j["decision"] = v.decision ? Json(*v.decision) : Json();
    j["chain"] = v.chain ? Json(*v.chain) : Json();
    j["cards"] = Json::array();
    for (const auto &c : v.cards)
        j["cards"].push_back({{"instance", c.instance},
                              {"hidden", c.hidden},
                              {"effectiveRings", c.effectiveRings},
                              {"occupiedRings", c.occupiedRings},
                              {"turnsToReady", c.turnsToReady},
                              {"effectiveCastCost", c.effectiveCastCost},
                              {"effectiveSettingCost", c.effectiveSettingCost}});
    transform(j, true);
    return j;
}
GameView decodeView(const Json &raw, const CardCatalog &catalog) {
    auto j = raw;
    transform(j, false);
    GameView v;
    j.at("viewer").get_to(v.viewer);
    j.at("active").get_to(v.active);
    j.at("phase").get_to(v.phase);
    j.at("result").get_to(v.result);
    if (v.viewer < 0 || v.viewer > 1 || v.active < 0 || v.active > 1 || static_cast<int>(v.phase) < 0 ||
        static_cast<int>(v.phase) > 4)
        throw std::runtime_error("Invalid view");
    j.at("players").get_to(v.players);
    const auto &summary = raw.at("actionSummary");
    v.actionSummary.actionable = summary.at("actionable");
    v.actionSummary.hasNonPhaseAction = summary.at("hasNonPhaseAction");
    if (!summary.at("safeAdvance").is_null()) v.actionSummary.safeAdvance = unwireCommand(summary.at("safeAdvance"));
    j.at("temporary").get_to(v.temporary);
    if (j.contains("temporaryLives"))
        j.at("temporaryLives").get_to(v.temporaryLives);
    j.at("phaseStep").get_to(v.phaseStep);
    j.at("phaseGate").get_to(v.phaseGate);
    j.at("triggers").get_to(v.triggers);
    j.at("opening").get_to(v.opening);
    j.at("initialFormationConfirmed").get_to(v.initialFormationConfirmed);
    j.at("initialFormation").get_to(v.initialFormation);
    j.at("globalTurn").get_to(v.globalTurn);
    j.at("clock").get_to(v.clock);
    j.at("technicalPaused").get_to(v.technicalPaused);
    if (!j.at("decision").is_null())
        v.decision = j.at("decision").get<PendingDecision>();
    if (!j.at("chain").is_null())
        v.chain = j.at("chain").get<ChainState>();
    for (const auto &x : j.at("cards")) {
        CardView c;
        c.instance = x.at("instance").get<CardInstance>();
        c.hidden = x.at("hidden").get<bool>();
        if (c.hidden)
            c.definition.name = "埋伏卡";
        else
            c.definition = catalog.at(c.instance.definition);
        c.effectiveRings = x.at("effectiveRings");
        c.occupiedRings = x.at("occupiedRings");
        c.turnsToReady = x.at("turnsToReady");
        c.effectiveCastCost = x.at("effectiveCastCost");
        c.effectiveSettingCost = x.at("effectiveSettingCost");
        v.cards.push_back(std::move(c));
    }
    return v;
}
Json encodeIntent(const ActionIntent &intent) {
    Json out = {{"source", std::to_string(intent.source)}, {"offset", intent.offset}, {"limit", intent.limit}, {"selections", Json::array()}};
    for (const auto &selection : intent.selections)
        out["selections"].push_back({{"field", static_cast<int>(selection.field)}, {"value", selection.value}});
    return out;
}
ActionIntent decodeIntent(const Json &value) {
    if (!value.is_object() || !value.at("selections").is_array() || value.at("selections").size() > 32)
        throw std::runtime_error("invalid action intent");
    ActionIntent intent;
    const auto source = number(value.at("source"));
    if (source > std::numeric_limits<CardId>::max()) throw std::runtime_error("invalid query source");
    intent.source = static_cast<CardId>(source);
    intent.offset = value.at("offset").get<std::size_t>();
    intent.limit = value.at("limit").get<std::size_t>();
    if (!intent.limit || intent.limit > 256 || intent.offset > 1000000) throw std::runtime_error("invalid query page");
    for (const auto &row : value.at("selections")) {
        const auto field = row.at("field").get<int>();
        const auto text = row.at("value").get<std::string>();
        if (field < 0 || field > static_cast<int>(ActionField::Confirm) || text.size() > 256)
            throw std::runtime_error("invalid action selection");
        intent.selections.push_back({static_cast<ActionField>(field), text});
    }
    return intent;
}
Json encodeOptions(const ActionOptions &options) {
    Json out = {{"field", static_cast<int>(options.field)}, {"options", Json::array()},
                {"hasMore", options.hasMore}, {"pending", options.pending}, {"error", options.error},
                {"complete", options.complete ? wireCommand(*options.complete) : Json()}};
    for (const auto &option : options.options)
        out["options"].push_back({{"value", option.value}, {"label", option.label},
                                  {"card", option.card}, {"player", option.player}});
    return out;
}
ActionOptions decodeOptions(const Json &j) {
    ActionOptions out;
    const auto field = j.at("field").get<int>();
    if (field < 0 || field > static_cast<int>(ActionField::Confirm) || !j.at("options").is_array() ||
        j.at("options").size() > 256) throw std::runtime_error("invalid action options");
    out.field = static_cast<ActionField>(field);
    out.hasMore = j.at("hasMore"); out.pending = j.at("pending"); out.error = j.at("error");
    if (!j.at("complete").is_null()) out.complete = unwireCommand(j.at("complete"));
    for (const auto &option : j.at("options"))
        out.options.push_back({option.at("value"), option.at("label"), option.at("card"), option.at("player")});
    return out;
}
Json encodeClock(const MatchClockView &clock) {
    Json out = clock;
    transform(out, true);
    return out;
}
MatchClockView decodeClock(Json value) {
    transform(value, false);
    return value.get<MatchClockView>();
}
Json encodeEvents(const EventBatch &batch) {
    Json out = {{"epoch", std::to_string(batch.epoch)}, {"base", std::to_string(batch.base)},
                {"next", std::to_string(batch.next)}, {"reset", batch.reset}, {"events", Json::array()}};
    auto cursor = batch.base;
    for (const auto &e : batch.events)
        out["events"].push_back({{"id", std::to_string(++cursor)}, {"kind", e.kind}, {"text", e.text},
            {"audience", e.audience}, {"card", e.card}, {"amount", e.amount},
            {"effectType", e.effectType ? Json(static_cast<int>(*e.effectType)) : Json()},
            {"actualAmount", e.actualAmount}, {"affectedPlayer", e.affectedPlayer}, {"spellReleased", e.spellReleased}});
    return out;
}
EventBatch decodeEvents(const Json &j) {
    EventBatch out{number(j.at("epoch")), number(j.at("base")), number(j.at("next")), j.at("reset").get<bool>(), {}};
    if (out.next < out.base || out.next - out.base > 512 || !j.at("events").is_array() ||
        j.at("events").size() != out.next - out.base) throw std::runtime_error("invalid event batch");
    auto cursor = out.base;
    for (const auto &row : j.at("events")) {
        if (number(row.at("id")) != ++cursor) throw std::runtime_error("noncontiguous event batch");
        GameEvent event;
        event.kind = row.at("kind"); event.text = row.at("text"); event.audience = row.at("audience");
        event.card = row.at("card"); event.amount = row.at("amount");
        if (!row.at("effectType").is_null()) event.effectType = row.at("effectType").get<EffectKind>();
        event.actualAmount = row.at("actualAmount"); event.affectedPlayer = row.at("affectedPlayer");
        event.spellReleased = row.at("spellReleased"); out.events.push_back(std::move(event));
    }
    return out;
}
std::vector<unsigned char> frame(const Json &j) {
    auto s = j.dump();
    if (s.empty() || s.size() > maxMessage)
        throw std::runtime_error("Message exceeds 1MiB");
    std::vector<unsigned char> b;
    auto n = static_cast<std::uint32_t>(s.size());
    for (int shift : {24, 16, 8, 0})
        b.push_back(static_cast<unsigned char>(n >> shift));
    b.insert(b.end(), s.begin(), s.end());
    return b;
}
std::vector<Json> Decoder::feed(const unsigned char *data, std::size_t size) {
    if (size > maxReceive - bytes_.size())
        throw std::runtime_error("Receive buffer exceeds 2MiB");
    if (size)
        bytes_.insert(bytes_.end(), data, data + size);
    std::vector<Json> out;
    std::size_t pos = 0;
    while (bytes_.size() - pos >= 4) {
        std::uint32_t n = 0;
        for (int i = 0; i < 4; ++i)
            n = (n << 8) | bytes_[pos + i];
        if (!n || n > maxMessage)
            throw std::runtime_error("Invalid message length");
        if (bytes_.size() - pos - 4 < n)
            break;
        int depth = 0;
        bool quoted = false, escaped = false;
        for (std::size_t at = pos + 4; at < pos + 4 + n; ++at) {
            auto ch = bytes_[at];
            if (quoted) {
                if (escaped)
                    escaped = false;
                else if (ch == '\\')
                    escaped = true;
                else if (ch == '"')
                    quoted = false;
            } else if (ch == '"')
                quoted = true;
            else if (ch == '{' || ch == '[') {
                if (++depth > 64)
                    throw std::runtime_error("JSON nesting exceeds 64");
            } else if (ch == '}' || ch == ']')
                --depth;
        }
        auto j = Json::parse(bytes_.begin() + pos + 4, bytes_.begin() + pos + 4 + n);
        if (!j.is_object())
            throw std::runtime_error("Expected message object");
        out.push_back(std::move(j));
        pos += 4 + n;
    }
    bytes_.erase(bytes_.begin(), bytes_.begin() + pos);
    return out;
}
} // namespace wizard::net
