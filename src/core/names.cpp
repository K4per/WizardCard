#include "wizard/core/types.hpp"
#include <algorithm>
#include <array>
#include <iomanip>
#include <map>
#include <sstream>
#include <string_view>

namespace wizard {
namespace {
constexpr std::array<const char *, 13> damageTypeIds = {
    "fire", "cold", "radiant", "necrotic", "poison", "lightning", "psychic",
    "thunder", "force", "slashing", "bludgeoning", "piercing", "acid"
};
}
const char *damageTypeId(DamageType type) {
    return damageTypeIds.at(static_cast<std::size_t>(type));
}
std::optional<DamageType> damageTypeFromId(const std::string &id) {
    const auto it = std::find(damageTypeIds.begin(), damageTypeIds.end(), id);
    if (it == damageTypeIds.end()) return std::nullopt;
    return static_cast<DamageType>(it - damageTypeIds.begin());
}
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
std::string damageTermKey(DamageType type) { return std::string("terms.damage.") + damageTypeId(type); }

std::string schoolTermKey(const std::string &school) {
    static const std::array<std::string_view, 9> schools = {
        "evocation", "transmutation", "conjuration", "enchantment", "illusion",
        "abjuration", "necromancy", "divination", "arcana"
    };
    return std::find(schools.begin(), schools.end(), school) == schools.end() ? "" : "terms.school." + school;
}

std::string phaseTermKey(Phase value) {
    static constexpr std::array<const char *, 5> keys = {
        "terms.phase.draw", "terms.phase.prepare", "terms.phase.main",
        "terms.phase.cast", "terms.phase.end"
    };
    return keys.at(static_cast<std::size_t>(value));
}

std::string zoneTermKey(Zone value) {
    static constexpr std::array<const char *, 11> keys = {
        "terms.zone.deck", "terms.zone.hand", "terms.zone.action",
        "terms.zone.analysis", "terms.zone.words", "terms.zone.casting",
        "terms.zone.ash", "terms.zone.attached", "terms.zone.resolving",
        "terms.zone.side_deck", "terms.zone.exile"
    };
    return keys.at(static_cast<std::size_t>(value));
}

std::string windowTermKey(ResponseWindow value) {
    static constexpr std::array<const char *, 7> keys = {
        "terms.window.phase_start", "terms.window.phase_end", "terms.window.action",
        "terms.window.prepare", "terms.window.cast", "terms.window.trigger",
        "terms.window.damage"
    };
    return keys.at(static_cast<std::size_t>(value));
}

std::string linkTermKey(LinkKind value) {
    static constexpr std::array<const char *, 7> keys = {
        "terms.link.phase", "terms.link.action", "terms.link.preparation",
        "terms.link.spell", "terms.link.response", "terms.link.trigger",
        "terms.link.setup"
    };
    return keys.at(static_cast<std::size_t>(value));
}

std::string legacyTermName(const std::string &key) {
    static const std::map<std::string, std::string> names = {
#include "legacy_terms.inc"
    };
    const auto found = names.find(key);
    return found == names.end() ? "" : found->second;
}
std::string damageName(DamageType value) { return legacyTermName(damageTermKey(value)); }
std::string schoolName(const std::string &value) { return legacyTermName(schoolTermKey(value)); }
std::string phaseName(Phase value) { return legacyTermName(phaseTermKey(value)); }
std::string zoneName(Zone value) { return legacyTermName(zoneTermKey(value)); }
std::string windowName(ResponseWindow value) { return legacyTermName(windowTermKey(value)); }
std::string linkName(LinkKind value) { return legacyTermName(linkTermKey(value)); }
} // namespace wizard
