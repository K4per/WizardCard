#include "wizard/network.hpp"
#include <algorithm>
#include <charconv>
#include <iomanip>
#include <random>
#include <sstream>
namespace wizard {
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Effect, kind, amount, damageType, recipient)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Payment, turn, paid, load, canceled)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(CardInstance, id, definition, owner, zone, spell, host, sourceFormation,
                                   targetCard, targetPlayer, base, analysisStarted, remaining, analysisLoad,
                                   castLoad, canceledTurn, payments, targets, settingPaid, concentrationCost,
                                   faceDown, ambushedTurn, quickAnalysisTurn)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TemporaryLoad, id, owner, source, amount, expiryTurn, independent,
                                   sourceBound)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerView, life, mana, load, capacity, handCount, deckCount, ownTurn,
                                   actionsPlayed, temporaryLife, blockedActionTurn, resistances, immunities)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PendingDecision, id, player, kind, options, mayPass)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Trigger, id, batch, owner, source, targetCard, targetPlayer, effects,
                                   target, requiresSource, targetLink, targetCards)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChainLink, id, kind, item, canceled, sourceLost, ability, paymentIndex,
                                   preparationLoad, paid, addedLoad, discarded, speed, castCost)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ChainState, id, window, mode, initiator, priority, passes, links,
                                   responded)
} // namespace wizard
namespace wizard::net {
namespace {
bool wide(const std::string &k) {
    return k == "id" || k == "decision" || k == "gate" || k == "link" || k == "targetLink" ||
           k == "phaseGate" || k == "ambushedTurn" || k == "eventBase";
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
            {"protocol", 1},
            {"program", programVersion},
            {"rules", c.catalog.rulesVersion},
            {"cards", c.catalog.cardSetVersion},
            {"hash", c.catalog.contentHash}};
}
Json encodeView(const GameView &v) {
    Json j = {{"viewer", v.viewer},       {"active", v.active},       {"phase", v.phase},
              {"result", v.result},       {"players", v.players},     {"temporary", v.temporary},
              {"phaseStep", v.phaseStep}, {"phaseGate", v.phaseGate}, {"triggers", v.triggers}};
    j["decision"] = v.decision ? Json(*v.decision) : Json();
    j["chain"] = v.chain ? Json(*v.chain) : Json();
    j["cards"] = Json::array();
    for (const auto &c : v.cards)
        j["cards"].push_back({{"instance", c.instance},
                              {"hidden", c.hidden},
                              {"effectiveRings", c.effectiveRings},
                              {"occupiedRings", c.occupiedRings},
                              {"turnsToReady", c.turnsToReady},
                              {"effectiveCastCost", c.effectiveCastCost}});
    j["actions"] = Json::array();
    for (const auto &a : v.actions)
        j["actions"].push_back({{"label", a.label},
                                {"source", a.source},
                                {"target", a.target},
                                {"command", encodeCommand(a.command)}});
    j["events"] = Json::array();
    std::size_t skip = v.events.size() > 512 ? v.events.size() - 512 : 0;
    j["eventBase"] = std::to_string(v.eventBase + skip);
    std::size_t i = static_cast<std::size_t>(v.eventBase) + skip;
    for (auto it = v.events.begin() + skip; it != v.events.end(); ++it) {
        const auto &e = *it;
        j["events"].push_back({{"id", std::to_string(++i)},
                               {"kind", e.kind},
                               {"text", e.text},
                               {"audience", e.audience},
                               {"card", e.card},
                               {"amount", e.amount},
                               {"effectType", e.effectType ? Json(*e.effectType) : Json()},
                               {"actualAmount", e.actualAmount},
                               {"affectedPlayer", e.affectedPlayer},
                               {"spellReleased", e.spellReleased}});
    }
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
    j.at("temporary").get_to(v.temporary);
    j.at("phaseStep").get_to(v.phaseStep);
    j.at("phaseGate").get_to(v.phaseGate);
    j.at("triggers").get_to(v.triggers);
    j.at("eventBase").get_to(v.eventBase);
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
        v.cards.push_back(std::move(c));
    }
    for (const auto &x : j.at("actions"))
        v.actions.push_back({x.at("label"), x.at("source"), x.at("target"), decodeCommand(x.at("command"))});
    for (const auto &x : j.at("events")) {
        GameEvent e;
        e.kind = x.at("kind");
        e.text = x.at("text");
        e.audience = x.at("audience");
        e.card = x.at("card");
        e.amount = x.at("amount");
        if (!x.at("effectType").is_null())
            e.effectType = x.at("effectType").get<EffectKind>();
        e.actualAmount = x.at("actualAmount");
        e.affectedPlayer = x.at("affectedPlayer");
        e.spellReleased = x.at("spellReleased");
        v.events.push_back(std::move(e));
    }
    return v;
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
