#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <variant>
#include <vector>

namespace wizard {
struct TextRef;

struct CardTextRef {
    std::string id;
};

class TextArg {
public:
    using Value = std::variant<std::int64_t, std::uint64_t, double, std::string,
                               std::shared_ptr<const TextRef>, CardTextRef>;

    template <class Integer, std::enable_if_t<std::is_integral_v<Integer> && !std::is_same_v<Integer, bool>, int> = 0>
    TextArg(Integer value)
        : value_(static_cast<std::conditional_t<std::is_signed_v<Integer>, std::int64_t, std::uint64_t>>(value)) {}
    TextArg(double value) : value_(value) {}
    TextArg(bool) = delete;
    TextArg(std::string value) : value_(std::move(value)) {}
    TextArg(const char *value) : TextArg(std::string(value)) {}
    TextArg(TextRef ref);
    TextArg(CardTextRef ref) : value_(std::move(ref)) {}

    const Value &value() const { return value_; }

private:
    Value value_;
};

struct TextRef {
    std::string key;
    std::vector<std::pair<std::string, TextArg>> args;
};

inline TextArg::TextArg(TextRef ref) : value_(std::make_shared<const TextRef>(std::move(ref))) {}
} // namespace wizard
