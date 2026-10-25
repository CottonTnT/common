#pragma once

#include <string_view>

#include "XType.hpp"

namespace X::StringUtil {

// template <auto c_X, auto... c_XS>
// inline constexpr bool isOneOfValues = ((c_X == c_XS) or ...);

/**
 * @brief FNV-1a hash
 *
 */
static constexpr auto hashString(std::string_view str,
    u64 prime)
    -> u64
{
    u64 hash_value = 0;
    for (auto c : str)
        hash_value = (hash_value ^ (c - '\0')) * prime;
    return hash_value;
}

/**
 * @brief FNV-1a hash
 *
 */
constexpr auto hashString(std::string_view str)
    -> u64
{
    constexpr u64 HASH_PRIME = 0x100000001B3ULL;

    return hashString(str, HASH_PRIME);
}

} // namespace X::StringUtil