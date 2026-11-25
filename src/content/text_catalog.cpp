#include "wizard/text_catalog.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <stdexcept>

namespace wizard {
namespace {
using Json = nlohmann::json;
constexpr unsigned maxTextDepth = 32;

[[noreturn]] void invalid(const std::string &source, const std::string &reason) {
    throw std::runtime_error(source + ": " + reason);
}

bool lower(char ch) { return ch >= 'a' && ch <= 'z'; }
bool digit(char ch) { return ch >= '0' && ch <= '9'; }
bool letter(char ch) { return lower(ch) || (ch >= 'A' && ch <= 'Z'); }
bool identifierChar(char ch) { return letter(ch) || digit(ch) || ch == '_'; }

bool validKey(std::string_view value) {
    return !value.empty() && value.front() != '.' && value.back() != '.' &&
           value.find("..") == std::string_view::npos && std::count(value.begin(), value.end(), '.') >= 2 &&
           std::all_of(value.begin(), value.end(), [](char ch) {
               return lower(ch) || digit(ch) || ch == '_' || ch == '.';
           });
}

bool validArgument(std::string_view value) {
    return !value.empty() && (letter(value.front()) || value.front() == '_') &&
           std::all_of(value.begin(), value.end(), identifierChar);
}

bool validCardId(std::string_view value) {
    return !value.empty() && std::all_of(value.begin(), value.end(), [](char ch) {
        return identifierChar(ch) || ch == '-';
    });
}

Json readTextJson(const std::filesystem::path &path) {
    const auto source = path.u8string();
    std::ifstream file(path, std::ios::binary);
    if (!file) invalid(source, "cannot open text catalog file");
    std::vector<std::set<std::string>> objectKeys;
    try {
        return Json::parse(file, [&](int, Json::parse_event_t event, Json &value) {
            using Event = Json::parse_event_t;
            if (event == Event::object_start) objectKeys.emplace_back();
            if (event == Event::object_end) objectKeys.pop_back();
            if (event == Event::key && !objectKeys.back().insert(value.get<std::string>()).second)
                invalid(source, "duplicate key " + value.get<std::string>());
            return true;
        });
    } catch (const Json::exception &e) {
        invalid(source, e.what());
    }
}

std::vector<std::string> readStringList(const Json &value, const std::string &source) {
    if (!value.is_array() || value.empty()) invalid(source, "expected nonempty array");
    std::vector<std::string> strings;
    std::set<std::string> seen;
    for (const auto &item : value) {
        if (!item.is_string()) invalid(source, "expected string");
        auto name = item.get<std::string>();
        if (!seen.insert(name).second) invalid(source, "duplicate entry " + name);
        strings.push_back(std::move(name));
    }
    return strings;
}

TextRef decodeRef(const Json &value, unsigned depth) {
    if (depth >= maxTextDepth) invalid("TextRef", "maximum nesting exceeded");
    if (!value.is_object() || !value.contains("key") || !value.at("key").is_string() ||
        value.size() != (value.contains("args") ? 2u : 1u))
        invalid("TextRef", "expected key with optional args object");

    TextRef ref{value.at("key").get<std::string>(), {}};
    if (!validKey(ref.key)) invalid("TextRef", "invalid key " + ref.key);
    if (!value.contains("args")) return ref;
    const auto &args = value.at("args");
    if (!args.is_object()) invalid(ref.key, "expected args object");
    for (const auto &[name, arg] : args.items()) {
        if (!validArgument(name)) invalid(ref.key, "invalid argument name " + name);
        if (arg.is_string()) ref.args.emplace_back(name, arg.get<std::string>());
        else if (arg.is_number_unsigned()) ref.args.emplace_back(name, arg.get<std::uint64_t>());
        else if (arg.is_number_integer()) ref.args.emplace_back(name, arg.get<std::int64_t>());
        else if (arg.is_number_float() && std::isfinite(arg.get<double>()))
            ref.args.emplace_back(name, arg.get<double>());
        else if (arg.is_object() && arg.size() == 1 && arg.contains("card") && arg.at("card").is_string()) {
            auto id = arg.at("card").get<std::string>();
            if (!validCardId(id)) invalid(ref.key, "invalid card ID " + id);
            ref.args.emplace_back(name, CardTextRef{std::move(id)});
        } else if (arg.is_object() && arg.contains("key")) {
            ref.args.emplace_back(name, decodeRef(arg, depth + 1));
        } else {
            invalid(ref.key, "invalid argument " + name);
        }
    }
    return ref;
}

Json encodeRef(const TextRef &ref, unsigned depth) {
    if (depth >= maxTextDepth) invalid(ref.key, "maximum nesting exceeded");
    if (!validKey(ref.key)) invalid("TextRef", "invalid key " + ref.key);
    auto args = Json::object();
    for (const auto &[name, arg] : ref.args) {
        if (!validArgument(name) || args.contains(name)) invalid(ref.key, "invalid or duplicate argument " + name);
        args[name] = std::visit([&](const auto &item) -> Json {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, std::shared_ptr<const TextRef>>) {
                return encodeRef(*item, depth + 1);
            } else if constexpr (std::is_same_v<Item, CardTextRef>) {
                if (!validCardId(item.id)) invalid(ref.key, "invalid card ID " + item.id);
                return {{"card", item.id}};
            } else {
                if constexpr (std::is_same_v<Item, double>) {
                    if (!std::isfinite(item)) invalid(ref.key, "non-finite number in argument " + name);
                }
                return item;
            }
        }, arg.value());
    }
    return {{"key", ref.key}, {"args", std::move(args)}};
}
} // namespace

TextCatalog::Entry TextCatalog::parseTemplate(std::string_view text, const std::string &source) {
    Entry entry;
    std::string literal;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch != '{' && ch != '}') {
            literal += ch;
            continue;
        }
        if (i + 1 < text.size() && text[i + 1] == ch) {
            literal += ch;
            ++i;
            continue;
        }
        if (ch == '}') invalid(source, "unmatched closing brace");
        const auto end = text.find('}', i + 1);
        if (end == std::string_view::npos) invalid(source, "unclosed placeholder");
        if (!literal.empty()) {
            entry.parts.push_back({PartKind::Literal, std::move(literal)});
            literal.clear();
        }
        const auto token = text.substr(i + 1, end - i - 1);
        const auto colon = token.find(':');
        const auto name = token.substr(0, colon);
        if (!validArgument(name)) invalid(source, "invalid placeholder " + std::string(token));
        if (colon == std::string_view::npos) {
            entry.arguments.emplace(name);
            entry.parts.push_back({PartKind::Argument, std::string(name)});
        } else {
            const auto target = token.substr(colon + 1);
            if (name == "card" && validCardId(target)) {
                entry.parts.push_back({PartKind::CardId, std::string(target)});
            } else if (name != "card" && target.substr(0, 6) == "terms." && validKey(target)) {
                entry.parts.push_back({PartKind::TextKey, std::string(target)});
            } else {
                invalid(source, "invalid inline reference " + std::string(token));
            }
        }
        i = end;
    }
    if (!literal.empty()) entry.parts.push_back({PartKind::Literal, std::move(literal)});
    return entry;
}

TextCatalog TextCatalog::load(const std::filesystem::path &directory) {
    TextCatalog catalog;
    const auto manifestPath = directory / "manifest.json";
    const auto manifest = readTextJson(manifestPath);
    const auto source = manifestPath.u8string();
    if (!manifest.is_object() || manifest.size() != 4 || !manifest.contains("defaultLocale") ||
        !manifest.at("defaultLocale").is_string() || !manifest.contains("locales") ||
        !manifest.contains("files") || !manifest.contains("fallbacks") || !manifest.at("fallbacks").is_object())
        invalid(source, "expected defaultLocale, locales, files and fallbacks");

    catalog.defaultLocale_ = manifest.at("defaultLocale").get<std::string>();
    const auto locales = readStringList(manifest.at("locales"), source + "/locales");
    const auto files = readStringList(manifest.at("files"), source + "/files");
    for (const auto &locale : locales) {
        if (!validCardId(locale))
            invalid(source, "invalid locale " + locale);
        catalog.entries_.try_emplace(locale);
    }
    if (catalog.entries_.count(catalog.defaultLocale_) == 0) invalid(source, "default locale is not declared");
    for (const auto &file : files) {
        if (file.size() <= 5 || file.substr(file.size() - 5) != ".json" ||
            !std::all_of(file.begin(), file.end() - 5, [](char ch) { return lower(ch) || digit(ch) || ch == '_'; }))
            invalid(source, "invalid catalog filename " + file);
    }
    const auto &fallbacks = manifest.at("fallbacks");
    for (const auto &[locale, fallback] : fallbacks.items()) {
        if (locale == catalog.defaultLocale_ || catalog.entries_.count(locale) == 0 || !fallback.is_string() ||
            catalog.entries_.count(fallback.get<std::string>()) == 0)
            invalid(source, "invalid fallback for " + locale);
    }
    for (const auto &locale : locales) {
        auto &chain = catalog.chains_[locale];
        auto next = locale;
        while (next != catalog.defaultLocale_) {
            if (std::find(chain.begin(), chain.end(), next) != chain.end())
                invalid(source, "fallback cycle for " + locale);
            chain.push_back(next);
            next = fallbacks.value(next, catalog.defaultLocale_);
        }
        chain.push_back(catalog.defaultLocale_);

        auto &entries = catalog.entries_.at(locale);
        for (const auto &filename : files) {
            const auto path = directory / locale / filename;
            const auto contents = readTextJson(path);
            if (!contents.is_object()) invalid(path.u8string(), "expected object of text entries");
            for (const auto &[key, value] : contents.items()) {
                const auto entrySource = path.u8string() + "/" + key;
                if (!validKey(key) || !value.is_string()) invalid(entrySource, "expected valid key and string value");
                if (entries.count(key) != 0) invalid(entrySource, "duplicate key in locale " + locale);
                entries.emplace(key, parseTemplate(value.get_ref<const std::string &>(), entrySource));
            }
        }
    }
    const auto &defaults = catalog.entries_.at(catalog.defaultLocale_);
    for (const auto &[locale, entries] : catalog.entries_) {
        for (const auto &[key, entry] : entries) {
            const auto entrySource = (directory / locale).u8string() + "/" + key;
            const auto original = defaults.find(key);
            if (original == defaults.end()) invalid(entrySource, "key missing from default locale");
            if (original->second.arguments != entry.arguments) invalid(entrySource, "parameters differ from default locale");
            for (const auto &part : entry.parts) {
                if (part.kind != PartKind::TextKey) continue;
                const auto target = defaults.find(part.value);
                if (target == defaults.end() || !target->second.arguments.empty())
                    invalid(entrySource, "inline reference requires a key without parameters: " + part.value);
            }
        }
    }
    return catalog;
}

const TextCatalog::Entry *TextCatalog::find(std::string_view locale, std::string_view key) const {
    const auto chain = chains_.find(locale);
    if (chain == chains_.end()) {
        if (entries_.empty()) return nullptr;
        return find(defaultLocale_, key);
    }
    for (const auto &candidate : chain->second) {
        const auto &entries = entries_.at(candidate);
        const auto entry = entries.find(key);
        if (entry != entries.end()) return &entry->second;
    }
    return nullptr;
}

std::string TextCatalog::render(const TextRef &ref, std::string_view locale, const CardLookup &card,
                                const Diagnostic &diagnostic) const {
    const Diagnostic log = diagnostic ? diagnostic : Diagnostic{[](const std::string &message) {
        std::clog << message << '\n';
    }};
    return render(ref, locale.empty() ? std::string_view(defaultLocale_) : locale, card, log, 0, nullptr);
}

std::string TextCatalog::render(const TextRef &ref, std::string_view locale, const CardLookup &card,
                                const Diagnostic &diagnostic, unsigned depth, const InlineReference *parent) const {
    const auto prefix = "text catalog [" + std::string(locale) + "] " + ref.key + ": ";
    if (depth >= maxTextDepth) {
        diagnostic(prefix + "maximum nesting exceeded");
        return ref.key;
    }
    for (auto ancestor = parent; ancestor; ancestor = ancestor->parent) {
        if (ancestor->key == ref.key) {
            diagnostic(prefix + "recursive inline reference");
            return ref.key;
        }
    }
    const auto *entry = find(locale, ref.key);
    if (!entry) {
        diagnostic(prefix + "missing key");
        return ref.key;
    }
    const auto cardName = [&](const std::string &id) {
        auto name = card ? card(id) : std::string{};
        if (!name.empty()) return name;
        diagnostic(prefix + "missing card name for " + id);
        return id;
    };
    std::map<std::string, const TextArg *, std::less<>> arguments;
    for (const auto &[name, argument] : ref.args) {
        if (entry->arguments.count(name) == 0) diagnostic(prefix + "unexpected argument " + name);
        else if (!arguments.emplace(name, &argument).second) diagnostic(prefix + "duplicate argument " + name);
    }
    std::map<std::string, std::string, std::less<>> rendered;
    for (const auto &name : entry->arguments) {
        const auto arg = arguments.find(name);
        if (arg == arguments.end()) {
            diagnostic(prefix + "missing argument " + name);
            rendered.emplace(name, "{" + name + "}");
            continue;
        }
        auto value = std::visit([&](const auto &item) -> std::string {
            using Item = std::decay_t<decltype(item)>;
            if constexpr (std::is_same_v<Item, std::shared_ptr<const TextRef>>)
                return render(*item, locale, card, diagnostic, depth + 1, nullptr);
            else if constexpr (std::is_same_v<Item, CardTextRef>) return cardName(item.id);
            else if constexpr (std::is_same_v<Item, std::string>) return item;
            else if constexpr (std::is_same_v<Item, double>) {
                if (std::isfinite(item)) {
                    auto number = Json(item).dump();
                    if (number.size() > 2 && number.compare(number.size() - 2, 2, ".0") == 0)
                        number.resize(number.size() - 2);
                    return number == "-0" ? "0" : number;
                }
                diagnostic(prefix + "non-finite number in argument " + name);
                return "{" + name + "}";
            } else return std::to_string(item);
        }, arg->second->value());
        rendered.emplace(name, std::move(value));
    }
    std::string result;
    const InlineReference reference{ref.key, parent};
    for (const auto &part : entry->parts) {
        switch (part.kind) {
        case PartKind::Literal: result += part.value; break;
        case PartKind::Argument: result += rendered.at(part.value); break;
        case PartKind::TextKey:
            result += render({part.value, {}}, locale, card, diagnostic, depth + 1, &reference);
            break;
        case PartKind::CardId: result += cardName(part.value); break;
        }
    }
    return result;
}

Json encodeTextRef(const TextRef &ref) { return encodeRef(ref, 0); }
TextRef decodeTextRef(const Json &value) { return decodeRef(value, 0); }
} // namespace wizard
