#pragma once

#include "wizard/core/text_ref.hpp"
#include <filesystem>
#include <functional>
#include <map>
#include <nlohmann/json_fwd.hpp>
#include <set>
#include <string_view>

namespace wizard {
class TextCatalog {
public:
    using CardLookup = std::function<std::string(const std::string &id)>;
    using Diagnostic = std::function<void(const std::string &message)>;

    static TextCatalog load(const std::filesystem::path &directory);
    const std::string &defaultLocale() const { return defaultLocale_; }
    std::string render(const TextRef &ref, std::string_view locale = {}, const CardLookup &card = {},
                       const Diagnostic &diagnostic = {}) const;

private:
    enum class PartKind { Literal, Argument, TextKey, CardId };
    struct Part {
        PartKind kind;
        std::string value;
    };
    struct Entry {
        std::vector<Part> parts;
        std::set<std::string> arguments;
    };
    struct InlineReference {
        std::string_view key;
        const InlineReference *parent;
    };
    using Entries = std::map<std::string, Entry, std::less<>>;

    static Entry parseTemplate(std::string_view text, const std::string &source);
    const Entry *find(std::string_view locale, std::string_view key) const;
    std::string render(const TextRef &ref, std::string_view locale, const CardLookup &card,
                       const Diagnostic &diagnostic, unsigned depth, const InlineReference *parent) const;

    std::string defaultLocale_{"zh_CN"};
    std::map<std::string, Entries, std::less<>> entries_;
    std::map<std::string, std::vector<std::string>, std::less<>> chains_;
};

nlohmann::json encodeTextRef(const TextRef &ref);
TextRef decodeTextRef(const nlohmann::json &value);
} // namespace wizard
