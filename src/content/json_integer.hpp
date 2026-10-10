#pragma once
#include <nlohmann/json.hpp>
#include <stdexcept>
namespace wizard::detail {
inline std::uint64_t integer(const Json &x, std::uint64_t max, bool nonzero = false) {
    if (!x.is_number_unsigned() && (!x.is_number_integer() || x.get<std::int64_t>() < 0))
        throw std::runtime_error("invalid unsigned integer");
    auto n = x.get<std::uint64_t>();
    if (n > max || (nonzero && !n))
        throw std::runtime_error("integer outside protocol range");
    return n;
}
}
