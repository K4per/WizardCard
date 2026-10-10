#include "wizard/core/rules.hpp"
#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <set>
#include <sstream>
#include <stdexcept>

namespace wizard {
std::string fingerprint(const std::string &bytes) {
    std::uint64_t h = 14695981039346656037ull;
    for (unsigned char c : bytes) {
        h ^= c;
        h *= 1099511628211ull;
    }
    std::ostringstream o;
    o << std::hex << std::setfill('0') << std::setw(16) << h;
    return o.str();
}
std::string damageName(DamageType d) {
    return std::array<const char *, 12>{"火焰", "寒霜", "光耀", "黯蚀", "毒素", "闪电",
                                        "心灵", "声波", "力场", "挥砍", "钝击", "穿刺"}
        .at(static_cast<std::size_t>(d));
}
std::string schoolName(const std::string &s) {
    const std::map<std::string, std::string> names{{"evocation", "塑能系"},   {"transmutation", "变化系"},
                                                   {"conjuration", "咒法系"}, {"enchantment", "惑控系"},
                                                   {"illusion", "幻术系"},    {"abjuration", "防护系"},
                                                   {"necromancy", "死灵系"},  {"divination", "预言系"}};
    auto i = names.find(s);
    return i == names.end() ? "" : i->second;
}
std::string phaseName(Phase p) {
    return std::array<const char *, 5>{"抽卡", "准备", "主要", "施法", "结束"}.at(
        static_cast<std::size_t>(p));
}
std::string zoneName(Zone p) {
    return std::array<const char *, 9>{"牌库", "手牌", "行动", "解析", "言灵", "施法", "灰烬", "符文", "响应"}
        .at(static_cast<std::size_t>(p));
}
std::string windowName(ResponseWindow p) {
    return std::array<const char *, 6>{"阶段开始", "阶段结束", "行动宣告", "准备施法", "释放法术", "被动触发"}
        .at(static_cast<std::size_t>(p));
}
std::string linkName(LinkKind p) {
    return std::array<const char *, 6>{"阶段", "行动宣告", "准备施法", "释放法术", "响应", "被动触发"}.at(
        static_cast<std::size_t>(p));
}
} // namespace wizard
