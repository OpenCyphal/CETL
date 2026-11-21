/// @file
/// Unit tests for cetl/pf20/utility.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "cetlvast/helpers_gtest.hpp"
#include "cetl/pf20/utility.hpp"
#include "cetl/pf20/cetlpf.hpp"

#include <cstdint>
#include <limits>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
using cetl::pf20::cmp_equal;
using cetl::pf20::cmp_greater;
using cetl::pf20::cmp_greater_equal;
using cetl::pf20::cmp_less;
using cetl::pf20::cmp_less_equal;
using cetl::pf20::cmp_not_equal;
using cetl::pf20::in_range;

// +----------------------------------------------------------------------+
// | Constant expressions: the cases where the built-in operators, after the usual arithmetic conversions, give the
// | mathematically wrong answer.
// +----------------------------------------------------------------------+
static_assert(cmp_less(-1, 1U), "-1 < 1U is false with the built-in operator");
static_assert(cmp_greater(1U, -1), "");
static_assert(!cmp_equal(-1, std::numeric_limits<unsigned>::max()),
              "-1 == UINT_MAX is true with the built-in operator");
static_assert(cmp_not_equal(-1, std::numeric_limits<unsigned>::max()), "");
static_assert(cmp_equal(1, 1U), "");
static_assert(cmp_equal(std::int8_t{-1}, std::int64_t{-1}), "");
static_assert(cmp_less_equal(std::int8_t{-1}, std::uint64_t{0}), "");
static_assert(cmp_greater_equal(std::uint8_t{0}, std::int64_t{-1}), "");
static_assert(!cmp_less(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::int64_t>::max()), "");
static_assert(cmp_greater(std::numeric_limits<std::uint64_t>::max(), std::numeric_limits<std::int64_t>::max()), "");
static_assert(cmp_less(std::numeric_limits<std::int64_t>::min(), std::uint8_t{0}), "");

static_assert(in_range<std::uint8_t>(255), "");
static_assert(!in_range<std::uint8_t>(256), "");
static_assert(!in_range<std::uint8_t>(-1), "");
static_assert(in_range<std::int8_t>(-128), "");
static_assert(!in_range<std::int8_t>(-129), "");
static_assert(!in_range<std::int8_t>(128), "");
static_assert(!in_range<std::uint64_t>(-1), "");
static_assert(in_range<std::uint64_t>(std::numeric_limits<std::int64_t>::max()), "");
static_assert(!in_range<std::int64_t>(std::numeric_limits<std::uint64_t>::max()), "");
static_assert(in_range<std::int64_t>(std::numeric_limits<std::uint32_t>::max()), "");
static_assert(in_range<const std::int16_t>(-32768), "cv-qualified R");

// cv-qualified explicit template arguments name the same integer types.
static_assert(cmp_less<const int, const long>(1, 2L), "");
static_assert(cmp_equal<const unsigned, const int>(1U, 1), "");
static_assert(in_range<const std::uint8_t>(255), "");

#if defined(__SIZEOF_INT128__)
// Extended 128-bit integers, including GCC's strict -std=c++NN modes where std::is_integral does not report them.
__extension__ typedef __int128          test_int128;
__extension__ typedef unsigned __int128 test_uint128;
static_assert(cmp_less(std::numeric_limits<std::int64_t>::min(), test_int128{0}), "");
static_assert(cmp_less(test_int128{-1}, std::uint8_t{0}), "");
static_assert(cmp_equal(test_int128{-1}, std::int8_t{-1}), "");
static_assert(!cmp_equal(test_int128{-1}, ~test_uint128{0}), "");
static_assert(cmp_greater(static_cast<test_uint128>(std::numeric_limits<std::uint64_t>::max()) + 1,
                          std::numeric_limits<std::uint64_t>::max()),
              "");
static_assert(cmp_greater_equal(~test_uint128{0}, test_int128{-1}), "");
static_assert(in_range<test_int128>(std::numeric_limits<std::uint64_t>::max()), "");
static_assert(!in_range<std::int64_t>(static_cast<test_int128>(1) << 70), "");
static_assert(!in_range<test_uint128>(-1), "");
static_assert(in_range<test_uint128>(std::numeric_limits<std::int64_t>::max()), "");
static_assert(in_range<std::int8_t>(test_int128{-128}), "");
static_assert(!in_range<std::int8_t>(test_int128{128}), "");
#endif

static_assert(cetl::cmp_less(-1, 1U), "cetlpf alias");
static_assert(!cetl::in_range<std::uint8_t>(256), "cetlpf alias");

// +----------------------------------------------------------------------+
// | Sweep over a value set for every pair of fixed-width integer types, against an oracle that compares the
// | mathematical values by first looking at the signs.
// +----------------------------------------------------------------------+
template <typename T>
constexpr bool is_negative(T t, std::true_type)
{
    return t < 0;
}
template <typename T>
constexpr bool is_negative(T, std::false_type)
{
    return false;
}
template <typename T>
constexpr bool is_negative(T t)
{
    return is_negative(t, std::is_signed<T>{});
}

/// -1, 0 or +1 for t <=> u using the mathematical values.
template <typename T, typename U>
int oracle_compare(T t, U u)
{
    const bool t_negative = is_negative(t);
    const bool u_negative = is_negative(u);
    if (t_negative != u_negative)
    {
        return t_negative ? -1 : 1;
    }
    if (t_negative)
    {
        // Both negative, so both signed and both representable as long long.
        const long long a = static_cast<long long>(t);
        const long long b = static_cast<long long>(u);
        return (a < b) ? -1 : ((a > b) ? 1 : 0);
    }
    // Both non-negative, so both representable as unsigned long long.
    const unsigned long long a = static_cast<unsigned long long>(t);
    const unsigned long long b = static_cast<unsigned long long>(u);
    return (a < b) ? -1 : ((a > b) ? 1 : 0);
}

template <typename T>
bool fits(long long candidate)
{
    if (std::is_signed<T>::value)
    {
        return (candidate >= static_cast<long long>(std::numeric_limits<T>::min())) &&
               (candidate <= static_cast<long long>(std::numeric_limits<T>::max()));
    }
    return (candidate >= 0) && (static_cast<unsigned long long>(candidate) <=
                                static_cast<unsigned long long>(std::numeric_limits<T>::max()));
}

/// The limits of T plus every boundary of the narrower fixed-width types that T can represent.
template <typename T>
std::vector<T> samples()
{
    static const long long candidates[] = {-4294967296LL, -2147483649LL, -2147483648LL, -65537LL,    -65536LL, -32769LL,
                                           -32768LL,      -257LL,        -256LL,        -129LL,      -128LL,   -2LL,
                                           -1LL,          0LL,           1LL,           2LL,         127LL,    128LL,
                                           255LL,         256LL,         32767LL,       32768LL,     65535LL,  65536LL,
                                           2147483647LL,  2147483648LL,  4294967295LL,  4294967296LL};
    std::vector<T>         out{std::numeric_limits<T>::min(),
                               static_cast<T>(std::numeric_limits<T>::min() + 1),
                               static_cast<T>(std::numeric_limits<T>::max() - 1),
                               std::numeric_limits<T>::max()};
    for (const long long candidate : candidates)
    {
        if (fits<T>(candidate))
        {
            out.push_back(static_cast<T>(candidate));
        }
    }
    return out;
}

template <typename T>
std::string show(T value, std::true_type)
{
    return std::to_string(static_cast<long long>(value));
}
template <typename T>
std::string show(T value, std::false_type)
{
    return std::to_string(static_cast<unsigned long long>(value));
}
template <typename T>
std::string show(T value)
{
    return show(value, std::is_signed<T>{});
}

template <typename T, typename U>
void check_pair()
{
    for (const T t : samples<T>())
    {
        for (const U u : samples<U>())
        {
            const int         c       = oracle_compare(t, u);
            const std::string context = show(t) + " vs " + show(u);
            EXPECT_EQ(c == 0, cmp_equal(t, u)) << context;
            EXPECT_EQ(c != 0, cmp_not_equal(t, u)) << context;
            EXPECT_EQ(c < 0, cmp_less(t, u)) << context;
            EXPECT_EQ(c > 0, cmp_greater(t, u)) << context;
            EXPECT_EQ(c <= 0, cmp_less_equal(t, u)) << context;
            EXPECT_EQ(c >= 0, cmp_greater_equal(t, u)) << context;
            const bool u_fits_in_t = (oracle_compare(std::numeric_limits<T>::min(), u) <= 0) &&
                                     (oracle_compare(u, std::numeric_limits<T>::max()) <= 0);
            EXPECT_EQ(u_fits_in_t, in_range<T>(u)) << context;
        }
    }
}

template <typename T>
void check_against_all()
{
    check_pair<T, std::int8_t>();
    check_pair<T, std::uint8_t>();
    check_pair<T, std::int16_t>();
    check_pair<T, std::uint16_t>();
    check_pair<T, std::int32_t>();
    check_pair<T, std::uint32_t>();
    check_pair<T, std::int64_t>();
    check_pair<T, std::uint64_t>();
}

TEST(PF20UtilityTest, IntegerComparisonsAgainstOracle)
{
    check_against_all<std::int8_t>();
    check_against_all<std::uint8_t>();
    check_against_all<std::int16_t>();
    check_against_all<std::uint16_t>();
    check_against_all<std::int32_t>();
    check_against_all<std::uint32_t>();
    check_against_all<std::int64_t>();
    check_against_all<std::uint64_t>();
}

TEST(PF20UtilityTest, NonFixedWidthTypes)
{
    EXPECT_TRUE(cmp_less(-1, 1UL));
    EXPECT_TRUE(cmp_less(-1LL, 1ULL));
    EXPECT_TRUE(cmp_greater(std::numeric_limits<unsigned long>::max(), -1L));
    EXPECT_FALSE(in_range<unsigned short>(-1));
    EXPECT_TRUE(in_range<long long>(std::numeric_limits<int>::min()));
}

}  // namespace
