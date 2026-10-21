/// @file
/// Differential fuzz test: the cetl::pf26 saturation functions (both the builtin-backed and the portable
/// implementations) against a 128-bit oracle, for every fixed-width integer type.
///
/// Input: byte 0 selects the operand type (low three bits); bytes 1..16 are the two operands.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/pf26/numeric.hpp"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <limits>
#include <type_traits>

__extension__ typedef __int128          fuzz_int128;
__extension__ typedef unsigned __int128 fuzz_uint128;

namespace
{

template <typename T>
using wide_t = typename std::conditional<std::is_signed<T>::value, fuzz_int128, fuzz_uint128>::type;

template <typename T>
T clamp_wide(fuzz_int128 v)
{
    const fuzz_int128 lo = std::numeric_limits<T>::min();
    const fuzz_int128 hi = std::numeric_limits<T>::max();
    return static_cast<T>(std::min(std::max(v, lo), hi));
}

template <typename T>
void require(bool condition, const char* what, T x, T y)
{
    if (!condition)
    {
        std::fprintf(stderr,
                     "saturation fuzz invariant failed: %s (%zu-byte %s operands %lld, %lld)\n",
                     what,
                     sizeof(T),
                     std::is_signed<T>::value ? "signed" : "unsigned",
                     static_cast<long long>(x),
                     static_cast<long long>(y));
        __builtin_trap();
    }
}

template <typename To, typename From>
void check_cast(From x)
{
    const fuzz_int128 expected = clamp_wide<To>(static_cast<fuzz_int128>(x));
    require(static_cast<fuzz_int128>(cetl::pf26::saturating_cast<To>(x)) == expected, "saturating_cast", x, x);
}

template <typename T>
void check(const std::uint8_t* const operands)
{
    T x{};
    T y{};
    std::memcpy(&x, operands, sizeof(T));
    std::memcpy(&y, operands + 8, sizeof(T));

    const fuzz_int128 a   = x;
    const fuzz_int128 b   = y;
    const T           sum = clamp_wide<T>(a + b);
    require(cetl::pf26::saturating_add(x, y) == sum, "saturating_add", x, y);
    require(cetl::pf26::detail::saturating_add_portable(x, y) == sum, "saturating_add_portable", x, y);

    const T difference = clamp_wide<T>(a - b);
    require(cetl::pf26::saturating_sub(x, y) == difference, "saturating_sub", x, y);
    require(cetl::pf26::detail::saturating_sub_portable(x, y) == difference, "saturating_sub_portable", x, y);

    // The product of two 64-bit values fits in the 128-bit type of the same signedness.
    const wide_t<T> product_wide = static_cast<wide_t<T>>(x) * static_cast<wide_t<T>>(y);
    const T         product = std::is_signed<T>::value
                                  ? clamp_wide<T>(static_cast<fuzz_int128>(product_wide))
                                  : static_cast<T>(std::min(static_cast<fuzz_uint128>(product_wide),
                                                            static_cast<fuzz_uint128>(std::numeric_limits<T>::max())));
    require(cetl::pf26::saturating_mul(x, y) == product, "saturating_mul", x, y);
    require(cetl::pf26::detail::saturating_mul_portable(x, y) == product, "saturating_mul_portable", x, y);

    if (y != 0)
    {
        require(cetl::pf26::saturating_div(x, y) == clamp_wide<T>(a / b), "saturating_div", x, y);
    }

    check_cast<std::int8_t>(x);
    check_cast<std::uint8_t>(x);
    check_cast<std::int16_t>(x);
    check_cast<std::uint16_t>(x);
    check_cast<std::int32_t>(x);
    check_cast<std::uint32_t>(x);
    check_cast<std::int64_t>(x);
    check_cast<std::uint64_t>(x);
}

}  // namespace

extern "C" int LLVMFuzzerTestOneInput(const std::uint8_t* data, std::size_t size)
{
    if (size < 17)
    {
        return 0;
    }
    const std::uint8_t* const operands = data + 1;
    switch (data[0] & 7U)
    {
    case 0:
        check<std::int8_t>(operands);
        break;
    case 1:
        check<std::uint8_t>(operands);
        break;
    case 2:
        check<std::int16_t>(operands);
        break;
    case 3:
        check<std::uint16_t>(operands);
        break;
    case 4:
        check<std::int32_t>(operands);
        break;
    case 5:
        check<std::uint32_t>(operands);
        break;
    case 6:
        check<std::int64_t>(operands);
        break;
    default:
        check<std::uint64_t>(operands);
        break;
    }
    return 0;
}
