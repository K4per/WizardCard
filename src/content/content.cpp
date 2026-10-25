#include "wizard/content.hpp"
#include <algorithm>
#include <fstream>
#include <cctype>
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
                                                         {"acid", DamageType::Acid},
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
        {"conceal", EffectKind::Conceal},
        {"delay_enemy_analysis", EffectKind::DelayEnemyAnalysis},
        {"advance_all_enemy_analysis", EffectKind::AdvanceAllEnemyAnalysis},
        {"conceal_enemy_spell", EffectKind::ConcealEnemySpell},
        {"reveal_enemy_ambush", EffectKind::RevealEnemyAmbush},
        {"choose_damage", EffectKind::ChooseDamage},
        {"destroy_formation_spells", EffectKind::DestroyFormationSpells},
        {"return_enemy_analysis", EffectKind::ReturnEnemyAnalysis},
        {"search_same_school", EffectKind::SearchSameSchool},
        {"reduce_spell_damage", EffectKind::ReduceSpellDamage},
        {"recover_enemy_ash_spell", EffectKind::RecoverEnemyAshSpell},
        {"recover_formation", EffectKind::RecoverFormation},
        {"destroy_enemy_analysis", EffectKind::DestroyEnemyAnalysis},
        {"shield_for_turn", EffectKind::ShieldForTurn},
        {"reveal_all_ambush", EffectKind::RevealAllAmbush},
        {"storm_guidance", EffectKind::StormGuidance},
        {"storm_cyclone", EffectKind::StormCyclone},
        {"cutting_wind", EffectKind::CuttingWind},
        {"destroy_all_enemy_field", EffectKind::DestroyAllEnemyField},
        {"lock_analysis", EffectKind::LockAnalysis},
        {"sique_detach_all", EffectKind::SiqueDetachAll},
        {"sique_recover_spell", EffectKind::SiqueRecoverSpell},
        {"sique_recover_seal", EffectKind::SiqueRecoverSeal},
        {"sique_mill_seal", EffectKind::SiqueMillSeal},
        {"sique_mill_spell", EffectKind::SiqueMillSpell},
        {"sique_damage_cap", EffectKind::SiqueDamageCap},
        {"sique_life_payment", EffectKind::SiqueLifePayment},
        {"sique_search_pair", EffectKind::SiqueSearchPair},
        {"sique_attach_from_deck", EffectKind::SiqueAttachFromDeck},
        {"sique_recycle", EffectKind::SiqueRecycle},
        {"sique_optional_draw", EffectKind::SiqueOptionalDraw},
        {"sique_shuffle_hands", EffectKind::SiqueShuffleHands},
        {"sique_negate_destroy", EffectKind::SiqueNegateDestroy},
        {"sique_roast_recover", EffectKind::SiqueRoastRecover},
        {"sique_blowfire", EffectKind::SiqueBlowfire},
        {"sique_backfire", EffectKind::SiqueBackfire},
        {"ray_volley", EffectKind::RayVolley},
        {"delay_all_enemy_analysis", EffectKind::DelayAllEnemyAnalysis},
        {"clear_all_ambush", EffectKind::ClearAllAmbush},
        {"arcana_fool", EffectKind::ArcanaFool},
        {"arcana_magician", EffectKind::ArcanaMagician},
        {"arcana_priestess", EffectKind::ArcanaPriestess},
        {"arcana_emperor", EffectKind::ArcanaEmperor},
        {"prevent_next_damage", EffectKind::PreventNextDamage},
        {"arcana_lovers", EffectKind::ArcanaLovers},
        {"arcana_hermit", EffectKind::ArcanaHermit},
        {"arcana_justice", EffectKind::ArcanaJustice},
        {"arcana_hanged_man", EffectKind::ArcanaHangedMan},
        {"arcana_death", EffectKind::ArcanaDeath},
        {"arcana_devil", EffectKind::ArcanaDevil},
        {"arcana_tower", EffectKind::ArcanaTower},
        {"arcana_world", EffectKind::ArcanaWorld},
        {"change_life_limit", EffectKind::ChangeLifeLimit},
        {"blood_pact_damage", EffectKind::BloodDamage},
        {"blood_pact_search", EffectKind::BloodSearch},
        {"blood_pact_clot", EffectKind::BloodClot},
        {"blood_heart_seal", EffectKind::BloodHeartSeal},
        {"blood_tube", EffectKind::BloodTube},
        {"seven_stars_awakening", EffectKind::SevenAwakening},
        {"seven_stars_insight", EffectKind::SevenInsight},
        {"seven_stars_shift", EffectKind::SevenShift},
        {"seven_stars_ruin", EffectKind::SevenRuin},
        {"seven_stars_gaze", EffectKind::SevenGaze},
        {"seven_stars_return", EffectKind::SevenReturn},
        {"clear_any_load", EffectKind::ClearAnyLoad}};
    for (const auto &e : j) {
        const auto p = path + "/" + std::to_string(n++);
        const auto kind = enumeration(text(e, "kind", p), names, p + "/kind");
        int amount = 0;
        if (kind == EffectKind::ChangeLifeLimit) {
            if (!e.contains("amount") || !e["amount"].is_number_integer() ||
                e["amount"].get<std::int64_t>() < -1000 || e["amount"].get<std::int64_t>() > 1000)
                fail(p + "/amount", "expected signed life limit change in [-1000,1000]");
            amount = e["amount"].get<int>();
        } else amount = number(e, "amount", p);
        Effect effect{kind, amount};
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
    if (revision > 9)
        fail(source + "/mechanicsRevision", "unsupported mechanics revision");
    cat.advancedRules = revision >= 1;
    cat.mechanicsRevision = revision;
    cat.alphaV2 = revision >= 2;
    cat.baseLoadCapacity = number(j, "baseLoadCapacity", source);
    cat.rulesVersion = text(j, "rulesVersion", source);
    cat.cardSetVersion = text(j, "cardSetVersion", source);
    if (!j.contains("cards") || !j.at("cards").is_array())
        fail(source + "/cards", "expected array");
    const std::map<std::string, CardType> types = {{"action", CardType::Action},
                                                   {"analytic", CardType::Analytic},
                                                   {"word", CardType::Word},
                                                   {"formation", CardType::Formation},
                                                   {"seal", CardType::Seal},
                                                   {"talisman", CardType::Talisman}};
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
        {"enemy_spell_link", TargetKind::EnemySpellLink}, {"player", TargetKind::Player}, {"enemy_ash_card", TargetKind::EnemyAshCard}};
    std::size_t n = 0;
    std::set<std::string> cardNumbers;
    for (const auto &card : j.at("cards")) {
        const auto p = source + "/cards/" + std::to_string(n++);
        CardDefinition d;
        d.id = text(card, "id", p);
        d.name = text(card, "name", p);
        d.illustrationId = card.value("illustrationId", d.id);
        d.text = text(card, "text", p);
        d.rarity = text(card, "rarity", p);
        d.type = enumeration(text(card, "type", p), types, p + "/type");
        if (cat.alphaV2) {
            d.number = text(card, "number", p);
            const std::map<CardType, std::string> prefixes{{CardType::Formation, "01"},
                                                            {CardType::Analytic, "02"},
                                                            {CardType::Word, "03"},
                                                            {CardType::Talisman, "04"},
                                                            {CardType::Seal, "05"}};
            auto prefix = prefixes.find(d.type);
            if (d.number.size() != 6 ||
                !std::all_of(d.number.begin(), d.number.end(), [](unsigned char ch) { return std::isdigit(ch); }) ||
                prefix == prefixes.end() || d.number.substr(0, 2) != prefix->second ||
                d.number.substr(2) == "0000" || !cardNumbers.insert(d.number).second)
                fail(p + "/number", "expected unique six-digit card number with matching type prefix");
            d.level = number(card, "level", p);
            if (d.level > 7 || (d.level == 0 && d.type != CardType::Formation && d.type != CardType::Seal))
                fail(p + "/level", "invalid card level");
        }
        d.target = enumeration(card.contains("target") ? text(card, "target", p) : std::string("none"),
                               targets, p + "/target");
        d.cost = number(card, "cost", p);
        d.castCost = number(card, "castCost", p);
        d.rank = number(card, "rank", p);
        d.analysisTurns = number(card, "analysisTurns", p, 1);
        d.duration = number(card, "duration", p);
        d.burden = number(card, "burden", p);
        d.extraDiscard = number(card, "extraDiscard", p);
        d.concentrationUpkeep = number(card, "concentrationUpkeep", p, -1);
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
        d.perpetual = boolean(card, "perpetual", p);
        if (cat.alphaV2 && card.contains("recipe")) {
            if (d.type != CardType::Formation || !card.at("recipe").is_array())
                fail(p + "/recipe", "formation recipe must be an array");
            const std::map<std::string, RecipeCostKind> recipeKinds{
                {"mana", RecipeCostKind::Mana},
                {"sacrifice_formation", RecipeCostKind::SacrificeFormation},
                {"sacrifice_spell", RecipeCostKind::SacrificeSpell},
                {"discard_hand", RecipeCostKind::DiscardHand}, {"life", RecipeCostKind::Life}};
            for (const auto &row : card.at("recipe")) {
                const auto rp = p + "/recipe/" + std::to_string(d.recipe.size());
                RecipeCost cost;
                cost.kind = enumeration(text(row, "kind", rp), recipeKinds, rp + "/kind");
                cost.amount = number(row, "amount", rp);
                if (cost.amount < 1)
                    fail(rp + "/amount", "recipe amount must be positive");
                cost.definition = row.value("definition", std::string{});
                cost.school = row.value("school", std::string{});
                if (!cost.school.empty() && schoolName(cost.school).empty())
                    fail(rp + "/school", "unknown school");
                cost.readyOnly = boolean(row, "readyOnly", rp);
                cost.distinctSchools = boolean(row, "distinctSchools", rp);
                cost.fireDamage = boolean(row, "fireDamage", rp);
                d.recipe.push_back(std::move(cost));
            }
        }
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
        if (d.speed < 1 || d.speed > 4 || (!cat.alphaV2 && d.type == CardType::Analytic && d.speed > 2))
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
        if (d.immediate && (d.type != CardType::Word || (cat.alphaV2 ? d.rank != 1 : d.rank != 0) ||
                            d.concentration || d.duration ||
                            (!cat.advancedRules && d.target != TargetKind::Opponent)))
            fail(p + "/immediate", "requires instant rank-zero opponent-target word");
        if (d.concentration && ((d.type != CardType::Analytic && d.type != CardType::Word &&
                                 d.type != CardType::Talisman) || d.duration || d.perpetual ||
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
        if (!cat.usesAlphaV25() && d.baseEligible && (d.type != CardType::Formation || d.body < 1 || d.body > 5 || !d.capacity ||
                               !d.rings || !d.maxRank))
            fail(p + "/baseEligible", "requires usable formation");
        if (!cat.usesAlphaV25() && cat.alphaV2 && d.baseEligible && d.level != 1)
            fail(p + "/baseEligible", "initial formation must be level 1");
        if (card.contains("tags")) {
            if (!card.at("tags").is_array())
                fail(p + "/tags", "expected array");
            for (const auto &tag : card.at("tags")) {
                if (!tag.is_string() || tag.get<std::string>().empty())
                    fail(p + "/tags", "expected nonempty strings");
                d.tags.push_back(tag.get<std::string>());
            }
        }
        if (cat.alphaV2 && card.contains("traits")) {
            static const std::set<std::string> allowed{
                "unique", "effect_indestructible", "effect_immune", "host_heal_plus_one",
                "host_cast_life_for_temporary", "host_evocation_damage_plus_one",
                "distinct_host_schools", "host_fire_only", "clear_load_on_analysis",
                "ping_on_spell_leave", "ash_evocation_discount", "return_formation_from_ash",
                "self_ambush_on_leave", "inspect_enemy_ambush", "discard_spell_search", "end_phase_start",
                "storm_search", "storm_search_talisman", "requires_storm_formation", "requires_storm_setup",
                "surcharge_enemy_timed_cast", "double_enemy_concentration_upkeep", "storm_release_counter",
                "storm_formation_host_only", "storm_formation_host_restriction", "thunder_damage_bonus",
                "activate_storm_crest_destroy", "activate_storm_search", "activate_storm_heart_choice",
                "storm_heart_upgrade", "search_great_storm_on_setup", "requires_storm_heart_setup",
                "great_storm", "sique_cauldron_only", "sique_requires_campfire", "sique_requires_cauldron",
                "sique_detach_1", "sique_detach_2", "sique_detach_3", "fire_damage_spell",
                "opponent_spell_load_4", "sique_campfire_only", "max_two", "quick_host_analysis_1", "quick_host_analysis_2", "moon",
                "destroy_enemy_ambush_end", "arcana_fool_condition", "arcana_priestess_cost",
                "arcana_emperor_cost", "arcana_chariot", "arcana_strength", "arcana_hanged_cost",
                "arcana_death", "arcana_temperance", "arcana_devil",
                "blood_altar", "blood_curse", "blood_heart", "blood_heartmark", "blood_last_stand",
                "blood_mark", "blood_offering_cost", "blood_pool", "blood_sacrifice_reward",
                "blood_same_origin", "blood_transfusion", "blood_vein", "blood_wrist_cost",
                "seven_stars_banner", "seven_stars_effect_only", "seven_stars_kaiyang",
                "seven_stars_lamp", "seven_stars_yaoguang"};
            if (!card.at("traits").is_array())
                fail(p + "/traits", "expected trait array");
            for (const auto &value : card.at("traits")) {
                if (!value.is_string() || !allowed.count(value.get<std::string>()) ||
                    std::find(d.traits.begin(), d.traits.end(), value.get<std::string>()) != d.traits.end())
                    fail(p + "/traits", "unknown or duplicate executable trait");
                d.traits.push_back(value.get<std::string>());
            }
            if (std::find(d.traits.begin(), d.traits.end(), "fire_damage_spell") != d.traits.end() &&
                d.type != CardType::Analytic && d.type != CardType::Word && d.type != CardType::Talisman)
                fail(p + "/traits", "fire_damage_spell requires a spell card type");
        }
        d.onDraw = effects(card.value("onDraw", Json::array()), p + "/onDraw");
        d.onMain = effects(card.value("onMain", Json::array()), p + "/onMain");
        d.onCast = effects(card.value("onCast", Json::array()), p + "/onCast");
        d.onLeave = effects(card.value("onLeave", Json::array()), p + "/onLeave");
        d.onMana = effects(card.value("onMana", Json::array()), p + "/onMana");
        if ((!d.onDraw.empty() || !d.onMain.empty() || !d.onCast.empty()) &&
            ((d.type != CardType::Analytic && !d.concentration) ||
             (!d.duration && !d.concentration && !d.perpetual)))
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
                    else if (value == "ambush" && (d.type == CardType::Word || d.type == CardType::Action ||
                                                   d.type == CardType::Talisman))
                        a.fromAmbush = true;
                    else if (value == "casting" && cat.alphaV2 &&
                             (d.type == CardType::Analytic || d.type == CardType::Word ||
                              d.type == CardType::Talisman))
                        a.fromCasting = true;
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
        if ((d.type == CardType::Analytic || d.type == CardType::Word || d.type == CardType::Talisman) &&
            d.rank < 1 && !d.immediate)
            fail(p + "/rank", "spell rank must be positive");
        if (!cat.alphaV2 && d.type == CardType::Word && d.duration)
            fail(p + "/duration", "first-set words must be instant");
        if (d.type != CardType::Analytic && !(cat.alphaV2 && d.type == CardType::Formation) &&
            (!d.onPrepare.empty() || !d.onEnd.empty()))
            fail(p, "periodic effects require analytic spell");
        if ((!d.onPrepare.empty() || !d.onEnd.empty()) && !d.duration && !d.perpetual &&
            !(cat.alphaV2 && (d.type == CardType::Formation || d.concentration)))
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
    if (cat.usesAlphaV25()) {
        if (std::none_of(cat.cards.begin(), cat.cards.end(), [](const auto &entry) {
            return entry.second.type == CardType::Formation && entry.second.level == 1;
        })) fail(source + "/cards", "missing level 1 formation");
    } else {
        const std::string requiredBase = cat.alphaV2 ? "small-hexagram" : "balance";
        if (!cat.cards.count(requiredBase) || !cat.at(requiredBase).baseEligible)
            fail(source + "/cards", "missing eligible base formation " + requiredBase);
    }
    // Resolve names only after the complete catalogue exists. Conditions describe the
    // same recipe that the engine executes, so display text cannot authorize a cost.
    if (cat.alphaV2) for (auto &[id, formation] : cat.cards) {
        if (formation.type != CardType::Formation) continue;
        std::vector<std::string> clauses;
        if (formation.cost) clauses.push_back("支付" + std::to_string(formation.cost) + "魔素");
        for (const auto &cost : formation.recipe) {
            if (cost.kind == RecipeCostKind::Life) {
                clauses.push_back("支付" + std::to_string(cost.amount) + "普通生命");
                continue;
            }
            if (cost.kind == RecipeCostKind::Mana) {
                clauses.push_back("支付" + std::to_string(cost.amount) + "魔素");
                continue;
            }
            std::string material;
            if (!cost.definition.empty()) {
                const auto named = cat.cards.find(cost.definition);
                if (named == cat.cards.end()) fail(source + "/cards/" + id + "/recipe", "unknown material definition");
                material = "「" + named->second.name + "」";
            } else material = cost.kind == RecipeCostKind::SacrificeFormation ? "阵法" :
                cost.kind == RecipeCostKind::SacrificeSpell ? "法术" : "手牌";
            if (!cost.school.empty()) material = schoolName(cost.school) + material;
            if (cost.readyOnly) material = "已解析完成或持续生效的" + material;
            if (cost.fireDamage) material = "具有火焰伤害效果的" + material;
            auto clause = (cost.kind == RecipeCostKind::DiscardHand ? "弃置" : "将己方场上的") +
                std::to_string(cost.amount) + "张" + material;
            if (cost.distinctSchools) clause += "（学派互不相同）";
            if (cost.kind != RecipeCostKind::DiscardHand) clause += "送入灰烬区";
            clauses.push_back(std::move(clause));
        }
        formation.settingConditions = clauses.empty() ? "无额外设置成本" : clauses.front();
        for (std::size_t i = 1; i < clauses.size(); ++i) formation.settingConditions += "；" + clauses[i];
        formation.settingConditions += "。需有足够阵体空间；每己方回合最多设置或替换1个阵法。";
        if (std::find(formation.traits.begin(), formation.traits.end(), "unique") != formation.traits.end())
            formation.settingConditions += "己方场上最多存在1个同名阵法。";
        if (cat.usesAlphaV25() && formation.level == 1)
            formation.settingConditions += "开局可从副卡组免费选择，双方确认后同时设置，不计本回合额度。";
    }
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
        if (cat.alphaV2 && (cat.at(id).type == CardType::Formation ||
                            cat.at(id).type == CardType::Action))
            fail(p + "/id", "card cannot enter main deck");
        if (count < 1 || (names[cat.at(id).name] += count) > 3)
            fail(p + "/count", "same-name limit is 3");
        for (int k = 0; k < count; ++k)
            deck.push_back(id);
    }
    if (deck.size() != (cat.alphaV2 ? 40u : 30u))
        fail(source, cat.alphaV2 ? "main deck must contain exactly 40 cards"
                                 : "deck must contain exactly 30 cards");
    return deck;
}
Content loadContent(const std::filesystem::path &dir) {
    Content c;
    c.catalog = parseCatalog(readCatalogJson(dir), dir.string());
    c.deck = parseDeck(readJson(dir / "deck.json"), c.catalog, (dir / "deck.json").string());
    if (c.catalog.alphaV2) {
        auto side = readJson(dir / "side-deck.json");
        if (!side.is_object() || (!c.catalog.usesAlphaV25() && !side.contains("initialFormation")) ||
            (side.contains("initialFormation") && !side.at("initialFormation").is_string()) || !side.contains("cards") ||
            !side.at("cards").is_array())
            fail((dir / "side-deck.json").string(), "invalid side deck");
        c.initialFormation = side.value("initialFormation", std::string{});
        std::map<std::string, int> names;
        for (const auto &row : side.at("cards")) {
            auto id = text(row, "id", (dir / "side-deck.json").string());
            auto count = number(row, "count", (dir / "side-deck.json").string());
            if (!c.catalog.cards.count(id) || c.catalog.at(id).type != CardType::Formation ||
                count < 1 || (names[c.catalog.at(id).name] += count) > 3)
                fail((dir / "side-deck.json").string(), "invalid formation or duplicate name");
            c.sideDeck.insert(c.sideDeck.end(), count, id);
        }
        const bool hasInitial = c.catalog.usesAlphaV25() ?
            std::any_of(c.sideDeck.begin(), c.sideDeck.end(), [&](const auto &id) { return c.catalog.at(id).level == 1; }) :
            c.catalog.cards.count(c.initialFormation) && c.catalog.at(c.initialFormation).level == 1 &&
            std::find(c.sideDeck.begin(), c.sideDeck.end(), c.initialFormation) != c.sideDeck.end();
        if (c.sideDeck.size() != 10 || !hasInitial)
            fail((dir / "side-deck.json").string(), "invalid initial formation or side deck size");
    }
    return c;
}
} // namespace wizard
