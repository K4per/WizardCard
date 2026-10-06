#include "wizard/decks.hpp"
#include "wizard/application.hpp"
#include <algorithm>
#include <atomic>
#include <cctype>
#include <chrono>
#include <set>
#include <stdexcept>
#include <tuple>

namespace wizard::app {
std::string rarityLabel(const std::string &rarity) {
    if (rarity == "common")
        return "普通";
    if (rarity == "uncommon")
        return "罕见";
    if (rarity == "rare")
        return "稀有";
    if (rarity == "epic")
        return "史诗";
    if (rarity == "legendary")
        return "传说";
    return rarity;
}
bool hasSpellRank(CardType type) {
    return type == CardType::Analytic || type == CardType::Word;
}
std::string cardTypeLabel(CardType type) {
    return std::array<const char *, 5>{"行动卡", "解析法术", "言灵法术", "阵法卡", "符文卡"}.at(
        static_cast<std::size_t>(type));
}
std::string primaryCostLabel(CardType type) {
    return std::array<const char *, 5>{"使用费用", "解析费用", "设置费用", "设置费用", "附着费用"}.at(
        static_cast<std::size_t>(type));
}
namespace {
std::string folded(std::string s) {
    for (auto &c : s)
        if (static_cast<unsigned char>(c) < 128)
            c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return s;
}
bool blank(const std::string &s) {
    return s.empty() || std::all_of(s.begin(), s.end(), [](unsigned char c) { return std::isspace(c); });
}
} // namespace
int DeckDraft::size() const {
    int n = 0;
    for (const auto &[cardId, count] : cards) {
        (void)cardId;
        n += count;
    }
    return n;
}
PlayerDeck DeckDraft::playerDeck() const {
    PlayerDeck result;
    result.baseFormation = baseFormation;
    for (const auto &[cardId, count] : cards) {
        if (count < 1 || count > 1000)
            throw std::invalid_argument("卡牌份数无效");
        result.cards.insert(result.cards.end(), static_cast<std::size_t>(count), cardId);
    }
    return result;
}
Json encodeDraft(const DeckDraft &d) {
    return {{"id", d.id}, {"name", d.name}, {"baseFormation", d.baseFormation}, {"cards", d.cards}};
}
DeckDraft decodeDraft(const Json &j) {
    DeckDraft d;
    for (const auto *field : {"id", "name", "baseFormation"})
        if (!j.at(field).is_string())
            throw std::runtime_error(std::string("卡组字段无效：") + field);
    d.id = j.at("id").get<std::string>();
    d.name = j.at("name").get<std::string>();
    d.baseFormation = j.at("baseFormation").get<std::string>();
    if (d.id.empty() || d.id.size() > 160 || d.name.size() > 240 || d.baseFormation.size() > 160)
        throw std::runtime_error("卡组ID或名称长度无效");
    const auto &cards = j.at("cards");
    if (!cards.is_object() || cards.size() > 1000)
        throw std::runtime_error("卡组牌表格式无效");
    for (const auto &[id, count] : cards.items()) {
        if (id.empty() || id.size() > 160 || !count.is_number_integer() || count.get<std::int64_t>() < 1 ||
            count.get<std::int64_t>() > 1000)
            throw std::runtime_error("卡组卡牌ID或份数无效：" + id);
        d.cards.emplace(id, count.get<int>());
    }
    return d;
}
std::vector<std::string> deckProblems(const CardCatalog &cat, const DeckDraft &d) {
    std::vector<std::string> errors;
    if (blank(d.name))
        errors.push_back("请输入卡组名称");
    if (d.size() != 30)
        errors.push_back("主卡组需30张，当前" + std::to_string(d.size()) + "张");
    std::map<std::string, int> names;
    for (const auto &[id, count] : d.cards) {
        if (count < 1 || count > 1000)
            errors.push_back("份数无效：" + id);
        const auto it = cat.cards.find(id);
        if (it == cat.cards.end()) {
            errors.push_back("卡池已无此卡：" + id);
            continue;
        }
        names[it->second.name] += count;
    }
    for (const auto &[name, count] : names)
        if (count > 3)
            errors.push_back(name + "同名最多3张，当前" + std::to_string(count) + "张");
    auto base = cat.cards.find(d.baseFormation);
    if (base == cat.cards.end())
        errors.push_back("基础阵法已不存在：" + d.baseFormation);
    else if (!base->second.baseEligible || base->second.type != CardType::Formation)
        errors.push_back("该卡不具有基础阵法资格");
    // The engine remains the authority on all additional formation constraints.
    if (errors.empty())
        for (const auto &error : Rules::deckErrors(cat, d.playerDeck()))
            errors.push_back(error);
    return errors;
}
std::vector<std::string> cardTags(const CardDefinition &d) {
    std::set<std::string> tags(d.tags.begin(), d.tags.end());
    if (!d.school.empty())
        tags.insert(schoolName(d.school));
    if (hasSpellRank(d.type))
        tags.insert(std::to_string(d.speed) + "速");
    if (!d.resistances.empty())
        tags.insert("抗性");
    if (!d.immunities.empty())
        tags.insert("免疫");
    if (d.quickRankOne || (d.type == CardType::Analytic && !d.analysisTurns))
        tags.insert("立即解析");
    if (d.baseAnalysisDiscount)
        tags.insert("解析减费");
    if (d.concentration)
        tags.insert("专注");
    if (d.baseEligible)
        tags.insert("基础阵法");
    if (d.income || d.incomeBonus)
        tags.insert("魔素收入");
    if (d.type == CardType::Formation)
        tags.insert("阵法");
    if (d.type == CardType::Seal)
        tags.insert("符文");
    auto effects = [&](const std::vector<Effect> &list) {
        for (const auto &e : list)
            switch (e.kind) {
            case EffectKind::Damage:
                tags.insert("伤害");
                tags.insert(damageName(e.damageType));
                break;
            case EffectKind::Heal:
                tags.insert("回复");
                break;
            case EffectKind::Draw:
                tags.insert("抽牌");
                break;
            case EffectKind::CancelPreparation:
            case EffectKind::CounterSpell:
            case EffectKind::NegateLink:
                tags.insert("反制");
                break;
            case EffectKind::GainMana:
                tags.insert("魔素");
                break;
            case EffectKind::DestroyAmbush:
            case EffectKind::Destroy:
            case EffectKind::OptionalDestroyOwnFormation:
                tags.insert("销毁");
                break;
            case EffectKind::DiscardHand:
            case EffectKind::DiscardFormation:
                tags.insert("弃置");
                break;
            case EffectKind::OptionalPrepare:
                tags.insert("准备施法");
                break;
            case EffectKind::AddSourceTemporary:
            case EffectKind::AddTemporary:
            case EffectKind::AddIndependent:
            case EffectKind::ClearTemporary:
            case EffectKind::ClearAllTemporary:
            case EffectKind::ClearIndependent:
                tags.insert("魔力荷载");
                break;
            case EffectKind::AddTemporaryLife:
                tags.insert("临时生命之核");
                break;
            case EffectKind::BlockActions:
                tags.insert("行动封锁");
                break;
            case EffectKind::SearchAction:
                tags.insert("检索");
                break;
            case EffectKind::AccelerateAnalysis:
                tags.insert("立即解析");
                break;
            case EffectKind::GrantResistance:
                tags.insert("抗性");
                break;
            case EffectKind::Conceal:
                tags.insert("埋伏");
                break;
            }
    };
    effects(d.effects);
    effects(d.onPrepare);
    effects(d.onEnd);
    effects(d.onDraw);
    effects(d.onMain);
    effects(d.onCast);
    effects(d.onLeave);
    effects(d.onMana);
    for (const auto &r : d.responses)
        effects(r.effects);
    return {tags.begin(), tags.end()};
}
std::vector<std::string> queryCards(const CardCatalog &cat, const CardQuery &q) {
    std::vector<std::string> out;
    for (const auto &[id, d] : cat.cards) {
        if ((q.rank && !hasSpellRank(d.type)) || (q.castCost && d.type != CardType::Analytic))
            continue;
        if (!q.name.empty() && folded(d.name).find(folded(q.name)) == std::string::npos)
            continue;
        if ((q.type && d.type != *q.type) || (!q.rarity.empty() && d.rarity != q.rarity) ||
            (q.cost && d.cost != *q.cost) || (q.castCost && d.castCost != *q.castCost) ||
            (q.rank && d.rank != *q.rank))
            continue;
        const auto tags = cardTags(d);
        if (!std::all_of(q.tags.begin(), q.tags.end(), [&](const std::string &tag) {
                return std::find(tags.begin(), tags.end(), tag) != tags.end();
            }))
            continue;
        out.push_back(id);
    }
    std::sort(out.begin(), out.end(), [&](const std::string &a, const std::string &b) {
        return std::tie(cat.at(a).name, a) < std::tie(cat.at(b).name, b);
    });
    return out;
}
DeckLibrary::DeckLibrary(CardCatalog cat, std::filesystem::path directory)
    : catalog_(std::move(cat)), path_(std::move(directory) / "decks.json") {
    try {
        if (!std::filesystem::exists(path_))
            return;
        const auto j = readJson(path_);
        if (!j.at("format").is_number_integer() || j.at("format") != 1 || !j.at("decks").is_array() ||
            j.at("decks").size() > 10000)
            throw std::runtime_error("卡组文件版本或格式不兼容");
        std::set<std::string> ids;
        std::vector<DeckDraft> next;
        for (const auto &row : j.at("decks")) {
            auto d = decodeDraft(row);
            if (!ids.insert(d.id).second)
                throw std::runtime_error("重复卡组ID");
            next.push_back(std::move(d));
        }
        drafts_ = std::move(next);
        if (j.value("contentHash", std::string{}) != catalog_.contentHash)
            notice_ = "卡池已更新，已重新校验所有卡组；失效卡牌仍保留，可在编辑器修复。";
    } catch (const std::exception &e) {
        writable_ = false;
        notice_ = std::string("卡组文件无法读取，原文件已保留，保存和删除暂不可用：") + e.what();
    }
}
DeckDraft DeckLibrary::create(const std::string &name) const {
    static std::atomic<std::uint64_t> serial{};
    DeckDraft d;
    d.name = name;
    do {
        d.id = "deck-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + "-" +
               std::to_string(++serial);
    } while (find(d.id));
    return d;
}
const DeckDraft *DeckLibrary::find(const std::string &id) const {
    const auto it =
        std::find_if(drafts_.begin(), drafts_.end(), [&](const DeckDraft &d) { return d.id == id; });
    return it == drafts_.end() ? nullptr : &*it;
}
void DeckLibrary::persist(const std::vector<DeckDraft> &decks) const {
    Json list = Json::array();
    for (const auto &d : decks)
        list.push_back(encodeDraft(d));
    atomicWriteJson(path_, {{"format", 1},
                            {"cardSetVersion", catalog_.cardSetVersion},
                            {"contentHash", catalog_.contentHash},
                            {"decks", list}});
}
bool DeckLibrary::save(const DeckDraft &draft, std::string &error) {
    try {
        if (!writable_)
            throw std::runtime_error(notice_);
        const auto d = decodeDraft(encodeDraft(draft));
        auto next = drafts_;
        auto it =
            std::find_if(next.begin(), next.end(), [&](const DeckDraft &item) { return item.id == d.id; });
        if (it == next.end())
            next.push_back(d);
        else
            *it = d;
        persist(next);
        drafts_ = std::move(next);
        return true;
    } catch (const std::exception &e) {
        error = std::string("卡组未保存，编辑内容仍保留：") + e.what();
        return false;
    }
}
bool DeckLibrary::erase(const std::string &id, std::string &error) {
    try {
        if (!writable_)
            throw std::runtime_error(notice_);
        if (!find(id))
            throw std::runtime_error("卡组已不存在");
        auto next = drafts_;
        next.erase(std::remove_if(next.begin(), next.end(), [&](const DeckDraft &d) { return d.id == id; }),
                   next.end());
        persist(next);
        drafts_ = std::move(next);
        return true;
    } catch (const std::exception &e) {
        error = std::string("卡组未删除：") + e.what();
        return false;
    }
}
} // namespace wizard::app
