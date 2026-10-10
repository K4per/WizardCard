#include "wizard/content.hpp"
#include <fstream>
#include <set>
#include <stdexcept>

namespace wizard {
namespace {
[[noreturn]] void fail(const std::string &path, const std::string &message) {
    throw std::runtime_error(path + ": " + message);
}
int number(const Json &j, const char *key, const std::string &path, int fallback = 0) {
    if (!j.contains(key))
        return fallback;
    const auto &x = j.at(key);
    if (!x.is_number_integer() || x.get<std::int64_t>() < 0 || x.get<std::int64_t>() > 1000)
        fail(path + "/" + key, "expected integer in [0,1000]");
    return x.get<int>();
}
std::string text(const Json &j, const char *key, const std::string &path) {
    if (!j.contains(key) || !j.at(key).is_string() || j.at(key).get<std::string>().empty())
        fail(path + "/" + key, "expected nonempty string");
    return j.at(key).get<std::string>();
}
template <typename T>
T enumeration(const std::string &value, const std::map<std::string, T> &table, const std::string &path) {
    auto i = table.find(value);
    if (i == table.end())
        fail(path, "unknown value: " + value);
    return i->second;
}
DamageType damageType(const std::string &v, const std::string &path) {
    return enumeration(v,
                       std::map<std::string, DamageType>{{"fire", DamageType::Fire},
                                                         {"cold", DamageType::Cold},
                                                         {"radiant", DamageType::Radiant},
                                                         {"necrotic", DamageType::Necrotic},
                                                         {"poison", DamageType::Poison},
                                                         {"lightning", DamageType::Lightning},
                                                         {"psychic", DamageType::Psychic},
                                                         {"thunder", DamageType::Thunder},
                                                         {"force", DamageType::Force},
                                                         {"slashing", DamageType::Slashing},
                                                         {"bludgeoning", DamageType::Bludgeoning},
                                                         {"piercing", DamageType::Piercing}},
                       path);
}
std::vector<Effect> effects(const Json &j, const std::string &path) {
    if (!j.is_array())
        fail(path, "expected effect array");
    std::vector<Effect> out;
    int n = 0;
    const std::map<std::string, EffectKind> names = {
        {"damage", EffectKind::Damage},
        {"heal", EffectKind::Heal},
        {"draw", EffectKind::Draw},
        {"temporary", EffectKind::AddTemporary},
        {"clear_temporary", EffectKind::ClearTemporary},
        {"destroy", EffectKind::Destroy},
        {"cancel", EffectKind::CancelPreparation},
        {"mana", EffectKind::GainMana},
        {"negate_link", EffectKind::NegateLink},
        {"optional_destroy_own_formation", EffectKind::OptionalDestroyOwnFormation},
        {"discard_hand", EffectKind::DiscardHand},
        {"discard_formation", EffectKind::DiscardFormation},
        {"clear_all_temporary", EffectKind::ClearAllTemporary},
        {"optional_prepare", EffectKind::OptionalPrepare},
        {"independent", EffectKind::AddIndependent},
        {"clear_independent", EffectKind::ClearIndependent},
        {"source_temporary", EffectKind::AddSourceTemporary},
        {"temporary_life", EffectKind::AddTemporaryLife},
        {"block_actions", EffectKind::BlockActions},
        {"counter_spell", EffectKind::CounterSpell},
        {"search_action", EffectKind::SearchAction},
        {"destroy_ambush", EffectKind::DestroyAmbush},
        {"accelerate_analysis", EffectKind::AccelerateAnalysis},
        {"grant_resistance", EffectKind::GrantResistance},
        {"conceal", EffectKind::Conceal}};
    for (const auto &e : j) {
        const auto p = path + "/" + std::to_string(n++);
        Effect effect{enumeration(text(e, "kind", p), names, p + "/kind"), number(e, "amount", p)};
        effect.damageType = damageType(e.value("damageType", std::string("force")), p + "/damageType");
        effect.recipient =
            enumeration(e.value("recipient", std::string("target")),
                        std::map<std::string, EffectRecipient>{{"target", EffectRecipient::Target},
                                                               {"owner", EffectRecipient::Owner},
                                                               {"opponent", EffectRecipient::Opponent}},
                        p + "/recipient");
        out.push_back(effect);
    }
    return out;
}
} // namespace
Json readJson(const std::filesystem::path &p) {
    std::ifstream f(p);
    if (!f)
        fail(p.string(), "cannot open");
    try {
        return Json::parse(f);
    } catch (const Json::exception &e) {
        fail(p.string(), e.what());
    }
}
void writeJson(const std::filesystem::path &p, const Json &j) {
    if (p.has_parent_path())
        std::filesystem::create_directories(p.parent_path());
    std::ofstream f(p);
    if (!f)
        fail(p.string(), "cannot write");
    f << j.dump(2) << '\n';
    if (!f)
        fail(p.string(), "write failed");
}
CardCatalog parseCatalog(const Json &j, const std::string &source) {
    CardCatalog cat;
    const auto revision = number(j, "mechanicsRevision", source);
    if (revision > 1)
        fail(source + "/mechanicsRevision", "unsupported mechanics revision");
    cat.advancedRules = revision == 1;
    cat.baseLoadCapacity = number(j, "baseLoadCapacity", source);
    cat.rulesVersion = text(j, "rulesVersion", source);
    cat.cardSetVersion = text(j, "cardSetVersion", source);
    if (!j.contains("cards") || !j.at("cards").is_array())
        fail(source + "/cards", "expected array");
    const std::map<std::string, CardType> types = {{"action", CardType::Action},
                                                   {"analytic", CardType::Analytic},
                                                   {"word", CardType::Word},
                                                   {"formation", CardType::Formation},
                                                   {"seal", CardType::Seal}};
    const std::map<std::string, TargetKind> targets = {
        {"none", TargetKind::None},
        {"self", TargetKind::Self},
        {"opponent", TargetKind::Opponent},
        {"empty_enemy_formation", TargetKind::EmptyEnemyFormation},
        {"enemy_card", TargetKind::EnemyCard},
        {"own_card", TargetKind::OwnCard},
        {"any_card", TargetKind::AnyCard},
        {"pending_link", TargetKind::PendingLink},
        {"preparation_root", TargetKind::PreparationRoot},
        {"own_analyzing_spell", TargetKind::OwnAnalyzingSpell},
        {"enemy_spell_link", TargetKind::EnemySpellLink}};
    std::size_t n = 0;
    for (const auto &card : j.at("cards")) {
        const auto p = source + "/cards/" + std::to_string(n++);
        CardDefinition d;
        d.id = text(card, "id", p);
        d.name = text(card, "name", p);
        d.text = text(card, "text", p);
        d.rarity = text(card, "rarity", p);
        d.type = enumeration(text(card, "type", p), types, p + "/type");
        d.target = enumeration(card.contains("target") ? text(card, "target", p) : std::string("none"),
                               targets, p + "/target");
        d.cost = number(card, "cost", p);
        d.castCost = number(card, "castCost", p);
        d.rank = number(card, "rank", p);
        d.analysisTurns = number(card, "analysisTurns", p, 1);
        d.duration = number(card, "duration", p);
        d.burden = number(card, "burden", p);
        d.extraDiscard = number(card, "extraDiscard", p);
        d.body = number(card, "body", p);
        d.capacity = number(card, "capacity", p);
        d.income = number(card, "income", p);
        d.rings = number(card, "rings", p);
        d.maxRank = number(card, "maxRank", p);
        d.ringBonus = number(card, "ringBonus", p);
        d.refundCast = number(card, "refundCast", p);
        d.effects = effects(card.value("effects", Json::array()), p + "/effects");
        d.onPrepare = effects(card.value("onPrepare", Json::array()), p + "/onPrepare");
        d.onEnd = effects(card.value("onEnd", Json::array()), p + "/onEnd");
        auto boolean = [&](const Json &obj, const char *key, const std::string &path) {
            if (!obj.contains(key))
                return false;
            if (!obj.at(key).is_boolean())
                fail(path + "/" + key, "expected boolean");
            return obj.at(key).get<bool>();
        };
        d.baseEligible = boolean(card, "baseEligible", p);
        d.immediate = boolean(card, "immediate", p);
        d.concentration = boolean(card, "concentration", p);
        d.opponentDestroyProtected = boolean(card, "opponentDestroyProtected", p);
        if (card.contains("incomeBonus") &&
            (!card.at("incomeBonus").is_number_integer() || card.at("incomeBonus").get<int>() < -1000 ||
             card.at("incomeBonus").get<int>() > 1000))
            fail(p + "/incomeBonus", "expected integer in [-1000,1000]");
        d.incomeBonus = card.value("incomeBonus", 0);
        d.school = card.value("school", std::string());
        if (!d.school.empty() && schoolName(d.school).empty())
            fail(p + "/school", "unknown school");
        d.speed = number(card, "speed", p, 1);
        if (d.speed < 1 || d.speed > 4 || (d.type == CardType::Analytic && d.speed > 2))
            fail(p + "/speed", "invalid spell speed");
        d.capacityBonus = number(card, "capacityBonus", p);
        d.rankBonus = number(card, "rankBonus", p);
        d.castCostBonus = number(card, "castCostBonus", p);
        d.damageBonus = number(card, "damageBonus", p);
        d.responseOnly = boolean(card, "responseOnly", p);
        d.baseAnalysisDiscount = boolean(card, "baseAnalysisDiscount", p);
        d.quickRankOne = boolean(card, "quickRankOne", p);
        for (const auto *key : {"resistances", "immunities"}) {
            if (!card.contains(key))
                continue;
            if (!card.at(key).is_array())
                fail(p + "/" + key, "expected array");
            for (const auto &v : card.at(key)) {
                if (!v.is_string())
                    fail(p + "/" + key, "expected damage type");
                (std::string(key) == "resistances" ? d.resistances : d.immunities)
                    .push_back(damageType(v.get<std::string>(), p + "/" + key));
            }
        }
        d.refundSetting = number(card, "refundSetting", p);
        if (card.contains("flavor"))
            d.flavor = text(card, "flavor", p);
        if (d.immediate && (d.type != CardType::Word || d.rank != 0 || d.concentration || d.duration ||
                            (!cat.advancedRules && d.target != TargetKind::Opponent)))
            fail(p + "/immediate", "requires instant rank-zero opponent-target word");
        if (d.concentration && ((d.type != CardType::Analytic && d.type != CardType::Word) || d.duration ||
                                !d.onPrepare.empty() || !d.onEnd.empty()))
            fail(p + "/concentration",
                 "requires concentration spell without finite duration or prepare/end effects");
        if (d.opponentDestroyProtected && d.type != CardType::Formation)
            fail(p + "/opponentDestroyProtected", "requires formation");
        if ((d.incomeBonus || d.refundSetting) && d.type != CardType::Seal)
            fail(p, "attachment bonuses require seal");
        if (d.refundSetting > 1 || (d.refundSetting && d.refundCast))
            fail(p + "/refundSetting", "requires 0 or 1 and excludes refundCast");
        d.targetCount = number(card, "targetCount", p, 1);
        if (d.targetCount < 1 || d.targetCount > 5)
            fail(p + "/targetCount", "expected 1..5");
        if (d.baseEligible && (d.type != CardType::Formation || d.body < 1 || d.body > 5 || !d.capacity ||
                               !d.rings || !d.maxRank))
            fail(p + "/baseEligible", "requires usable formation");
        if (card.contains("tags")) {
            if (!card.at("tags").is_array())
                fail(p + "/tags", "expected array");
            for (const auto &tag : card.at("tags")) {
                if (!tag.is_string() || tag.get<std::string>().empty())
                    fail(p + "/tags", "expected nonempty strings");
                d.tags.push_back(tag.get<std::string>());
            }
        }
        d.onDraw = effects(card.value("onDraw", Json::array()), p + "/onDraw");
        d.onMain = effects(card.value("onMain", Json::array()), p + "/onMain");
        d.onCast = effects(card.value("onCast", Json::array()), p + "/onCast");
        d.onLeave = effects(card.value("onLeave", Json::array()), p + "/onLeave");
        d.onMana = effects(card.value("onMana", Json::array()), p + "/onMana");
        if ((!d.onDraw.empty() || !d.onMain.empty() || !d.onCast.empty()) &&
            ((d.type != CardType::Analytic && !d.concentration) || (!d.duration && !d.concentration)))
            fail(p, "phase triggers require finite analytic duration");
        if (card.contains("responses")) {
            if (!card.at("responses").is_array())
                fail(p + "/responses", "expected array");
            std::set<std::string> ids;
            std::size_t index = 0;
            const std::map<std::string, ResponseWindow> windows = {
                {"phase_start", ResponseWindow::PhaseStart},
                {"phase_end", ResponseWindow::PhaseEnd},
                {"action", ResponseWindow::Action},
                {"prepare", ResponseWindow::Prepare},
                {"cast", ResponseWindow::Cast},
                {"trigger", ResponseWindow::Trigger}};
            const std::map<std::string, EventOwner> owners = {
                {"any", EventOwner::Any}, {"self", EventOwner::Self}, {"opponent", EventOwner::Opponent}};
            for (const auto &entry : card.at("responses")) {
                auto rp = p + "/responses/" + std::to_string(index++);
                ResponseAbility a;
                a.id = text(entry, "id", rp);
                a.name = entry.contains("name") ? text(entry, "name", rp) : a.id;
                if (!ids.insert(a.id).second)
                    fail(rp + "/id", "duplicate ability");
                if (!entry.contains("sources") || !entry.at("sources").is_array() ||
                    entry.at("sources").empty())
                    fail(rp + "/sources", "expected nonempty array");
                std::set<std::string> sources;
                for (const auto &sourceValue : entry.at("sources")) {
                    if (!sourceValue.is_string())
                        fail(rp + "/sources", "expected strings");
                    auto value = sourceValue.get<std::string>();
                    if (!sources.insert(value).second)
                        fail(rp + "/sources", "duplicate source");
                    if (value == "hand")
                        a.fromHand = true;
                    else if (value == "words" && d.type == CardType::Word)
                        a.fromWords = true;
                    else if (value == "analysis" && d.type == CardType::Analytic)
                        a.fromAnalysis = true;
                    else if (value == "ambush" && (d.type == CardType::Word || d.type == CardType::Action))
                        a.fromAmbush = true;
                    else
                        fail(rp + "/sources", "invalid source");
                }
                if (a.fromHand && !entry.contains("handCost"))
                    fail(rp + "/handCost", "explicit hand cost required");
                a.handCost = number(entry, "handCost", rp, -1);
                a.preloadedCost = number(entry, "preloadedCost", rp);
                a.burden = number(entry, "burden", rp);
                a.extraDiscard = number(entry, "extraDiscard", rp);
                a.targetCount = number(entry, "targetCount", rp, 1);
                if (a.extraDiscard > 1 || a.targetCount < 1 || a.targetCount > 5)
                    fail(rp, "unsupported cost or target count");
                a.requiresSource = boolean(entry, "requiresSource", rp);
                a.target = enumeration(entry.value("target", std::string("none")), targets, rp + "/target");
                a.eventOwner =
                    enumeration(entry.value("eventOwner", std::string("any")), owners, rp + "/eventOwner");
                if (!entry.contains("windows") || !entry.at("windows").is_array() ||
                    entry.at("windows").empty())
                    fail(rp + "/windows", "expected nonempty array");
                std::set<ResponseWindow> seen;
                for (const auto &window : entry.at("windows")) {
                    if (!window.is_string())
                        fail(rp + "/windows", "expected strings");
                    auto w = enumeration(window.get<std::string>(), windows, rp + "/windows");
                    if (!seen.insert(w).second)
                        fail(rp + "/windows", "duplicate window");
                    a.windows.push_back(w);
                }
                if (entry.contains("phases")) {
                    if (!entry.at("phases").is_array())
                        fail(rp + "/phases", "expected array");
                    for (const auto &v : entry.at("phases"))
                        a.phases.push_back(
                            enumeration(v.get<std::string>(),
                                        std::map<std::string, Phase>{{"draw", Phase::Draw},
                                                                     {"prepare", Phase::Prepare},
                                                                     {"main", Phase::Main},
                                                                     {"cast", Phase::Cast},
                                                                     {"end", Phase::End}},
                                        rp + "/phases"));
                }
                if (cat.advancedRules && a.fromHand)
                    fail(rp + "/sources", "hand cannot respond under revision 1");
                a.effects = effects(entry.value("effects", Json::array()), rp + "/effects");
                if (a.effects.empty())
                    fail(rp + "/effects", "response must have effects");
                for (const auto &e : a.effects) {
                    if (e.kind == EffectKind::NegateLink && a.target != TargetKind::PendingLink)
                        fail(rp + "/target", "negation requires pending_link");
                    if (e.kind == EffectKind::CancelPreparation && a.target != TargetKind::PreparationRoot)
                        fail(rp + "/target", "cancellation requires preparation_root");
                }
                d.responses.push_back(std::move(a));
            }
        }
        if (d.extraDiscard > 1 || (d.extraDiscard && d.type != CardType::Analytic))
            fail(p + "/extraDiscard", "prototype supports one discard at analytic preparation");
        if (d.type == CardType::Analytic && d.analysisTurns < (cat.advancedRules ? 0 : 1))
            fail(p + "/analysisTurns", "must be positive");
        if (d.type == CardType::Formation &&
            (d.body < 1 || d.body > 5 || (!cat.advancedRules && d.cost != 0)))
            fail(p, "formation needs body 1..5 and cost 0");
        if ((d.type == CardType::Analytic || d.type == CardType::Word) && d.rank < 1 && !d.immediate)
            fail(p + "/rank", "spell rank must be positive");
        if (d.type == CardType::Word && d.duration)
            fail(p + "/duration", "first-set words must be instant");
        if (d.type != CardType::Analytic && (!d.onPrepare.empty() || !d.onEnd.empty()))
            fail(p, "periodic effects require analytic spell");
        if ((!d.onPrepare.empty() || !d.onEnd.empty()) && !d.duration)
            fail(p + "/duration", "periodic effects require finite duration");
        if (d.concentration && (!d.onDraw.empty() || !d.onMain.empty() || !d.onMana.empty()))
            fail(p + "/concentration", "concentration effects run only in cast phase");
        if (d.ringBonus && d.type != CardType::Seal)
            fail(p + "/ringBonus", "requires seal");
        if (d.refundCast > 1 || (d.refundCast && d.type != CardType::Seal))
            fail(p + "/refundCast", "requires seal and value 0 or 1");
        if (!cat.cards.emplace(d.id, d).second)
            fail(p + "/id", "duplicate card id");
    }
    if (!cat.cards.count("balance") || !cat.at("balance").baseEligible)
        fail(source + "/cards", "missing eligible base formation balance");
    cat.contentHash = fingerprint(j.dump());
    return cat;
}
std::vector<std::string> parseDeck(const Json &j, const CardCatalog &cat, const std::string &source) {
    if (!j.is_array())
        fail(source, "expected deck array");
    std::vector<std::string> deck;
    std::map<std::string, int> names;
    int n = 0;
    for (const auto &e : j) {
        auto p = source + "/" + std::to_string(n++);
        auto id = text(e, "id", p);
        int count = number(e, "count", p);
        if (!cat.cards.count(id))
            fail(p + "/id", "unknown definition");
        if (count < 1 || (names[cat.at(id).name] += count) > 3)
            fail(p + "/count", "same-name limit is 3");
        for (int k = 0; k < count; ++k)
            deck.push_back(id);
    }
    if (deck.size() != 30)
        fail(source, "deck must contain exactly 30 cards");
    return deck;
}
Content loadContent(const std::filesystem::path &dir) {
    Content c;
    c.catalog = parseCatalog(readCatalogJson(dir), dir.string());
    c.deck = parseDeck(readJson(dir / "deck.json"), c.catalog, (dir / "deck.json").string());
    return c;
}
} // namespace wizard
