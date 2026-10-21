/// @file
/// Unit tests for cetl/pf26/numeric.hpp and the saturation arithmetic selected by cetl/pf26/cetlpf.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "cetlvast/helpers_gtest.hpp"
#include "cetl/pf26/numeric.hpp"
#include "cetl/pf26/cetlpf.hpp"

#include <algorithm>
#include <cstdint>
#include <limits>
#include <random>
#include <string>
#include <type_traits>
#include <vector>

#if defined(__SIZEOF_INT128__)
// 128-bit integers are an extension; they are only used as a test oracle for the 64-bit types.
__extension__ typedef __int128          cetlvast_int128;
__extension__ typedef unsigned __int128 cetlvast_uint128;
#    define CETLVAST_HAS_INT128 1
#else
#    define CETLVAST_HAS_INT128 0
#endif

namespace
{
using cetl::pf26::saturating_add;
using cetl::pf26::saturating_cast;
using cetl::pf26::saturating_div;
using cetl::pf26::saturating_mul;
using cetl::pf26::saturating_sub;
using cetl::pf26::detail::saturating_add_portable;
using cetl::pf26::detail::saturating_mul_portable;
using cetl::pf26::detail::saturating_sub_portable;

constexpr std::int64_t  i64_min = std::numeric_limits<std::int64_t>::min();
constexpr std::int64_t  i64_max = std::numeric_limits<std::int64_t>::max();
constexpr std::uint64_t u64_max = std::numeric_limits<std::uint64_t>::max();

// +----------------------------------------------------------------------+
// | Constant expressions ([numeric.sat] examples and the classic edge cases)
// +----------------------------------------------------------------------+
static_assert(saturating_add<std::int8_t>(100, 100) == 127, "");
static_assert(saturating_add<std::int8_t>(-100, -100) == -128, "");
static_assert(saturating_add<std::int8_t>(-128, 127) == -1, "");
static_assert(saturating_add<std::uint8_t>(250, 10) == 255, "");
static_assert(saturating_add<std::int64_t>(i64_max, 1) == i64_max, "");
static_assert(saturating_add<std::int64_t>(i64_min, -1) == i64_min, "");
static_assert(saturating_add<std::uint64_t>(u64_max, 1) == u64_max, "");
static_assert(saturating_add(3, 4) == 7, "");

static_assert(saturating_sub<std::uint8_t>(3, 5) == 0, "");
static_assert(saturating_sub<std::int8_t>(-128, 1) == -128, "");
static_assert(saturating_sub<std::int8_t>(127, -1) == 127, "");
static_assert(saturating_sub<std::int8_t>(-128, -128) == 0, "");
static_assert(saturating_sub<std::int64_t>(0, i64_min) == i64_max, "");
static_assert(saturating_sub<std::int64_t>(i64_min, i64_max) == i64_min, "");
static_assert(saturating_sub<std::uint64_t>(0, 1) == 0, "");

static_assert(saturating_mul<std::int8_t>(-128, -1) == 127, "");
static_assert(saturating_mul<std::int8_t>(-1, -128) == 127, "");
static_assert(saturating_mul<std::int8_t>(100, -100) == -128, "");
static_assert(saturating_mul<std::int8_t>(-100, -100) == 127, "");
static_assert(saturating_mul<std::int8_t>(-64, 2) == -128, "");
static_assert(saturating_mul<std::uint8_t>(16, 16) == 255, "");
static_assert(saturating_mul<std::uint8_t>(15, 17) == 255, "");
static_assert(saturating_mul<std::int64_t>(INT64_C(4000000000), INT64_C(4000000000)) == i64_max, "");
static_assert(saturating_mul<std::int64_t>(INT64_C(-4000000000), INT64_C(4000000000)) == i64_min, "");
static_assert(saturating_mul<std::int64_t>(i64_min, -1) == i64_max, "");
static_assert(saturating_mul<std::int64_t>(-3, i64_max) == i64_min, "");
static_assert(saturating_mul<std::uint64_t>(u64_max, 2) == u64_max, "");
static_assert(saturating_mul<std::uint64_t>(0, u64_max) == 0, "");

static_assert(saturating_div<std::int8_t>(-128, -1) == 127, "");
static_assert(saturating_div<std::int8_t>(7, -2) == -3, "truncation toward zero");
static_assert(saturating_div<std::int32_t>(std::numeric_limits<std::int32_t>::min(), -1) ==
                  std::numeric_limits<std::int32_t>::max(),
              "");
static_assert(saturating_div<std::int64_t>(i64_min, -1) == i64_max, "");
static_assert(saturating_div<std::int64_t>(i64_min, 1) == i64_min, "");
static_assert(saturating_div<std::uint8_t>(255, 1) == 255, "");
static_assert(saturating_div<std::uint64_t>(u64_max, u64_max) == 1, "");

static_assert(saturating_cast<std::int8_t>(696) == 127, "");
static_assert(saturating_cast<std::uint8_t>(696) == 255, "");
static_assert(saturating_cast<std::int8_t>(-696) == -128, "");
static_assert(saturating_cast<std::uint8_t>(-696) == 0, "");
static_assert(saturating_cast<std::int8_t>(std::uint64_t{5}) == 5, "");
static_assert(saturating_cast<std::int32_t>(std::uint32_t{5}) == 5, "");
static_assert(saturating_cast<std::int32_t>(std::uint32_t{0x80000000U}) == std::numeric_limits<std::int32_t>::max(),
              "");
static_assert(saturating_cast<std::uint64_t>(std::int8_t{-1}) == 0, "");
static_assert(saturating_cast<std::int8_t>(std::uint8_t{200}) == 127, "");
static_assert(saturating_cast<std::int64_t>(u64_max) == i64_max, "");
static_assert(saturating_cast<std::uint64_t>(i64_min) == 0, "");
static_assert(saturating_cast<std::uint64_t>(i64_max) == static_cast<std::uint64_t>(i64_max), "");
static_assert(saturating_cast<std::int8_t>(i64_min) == -128, "");
static_assert(saturating_cast<std::uint32_t>(std::int64_t{-1}) == 0, "");
static_assert(saturating_cast<std::uint32_t>(INT64_C(4294967296)) == std::numeric_limits<std::uint32_t>::max(), "");
static_assert(saturating_cast<std::int64_t>(std::int64_t{42}) == 42, "same type");

// The portable implementations are constant expressions as well.
static_assert(saturating_add_portable<std::int8_t>(100, 100) == 127, "");
static_assert(saturating_sub_portable<std::uint8_t>(3, 5) == 0, "");
static_assert(saturating_mul_portable<std::int64_t>(i64_min, -1) == i64_max, "");

// Nothing is promoted.
static_assert(std::is_same<decltype(saturating_add(std::int8_t{1}, std::int8_t{2})), std::int8_t>::value, "");
static_assert(std::is_same<decltype(saturating_sub(std::uint8_t{1}, std::uint8_t{2})), std::uint8_t>::value, "");
static_assert(std::is_same<decltype(saturating_cast<std::uint16_t>(std::int64_t{1})), std::uint16_t>::value, "");

// Whatever cetl/pf26/cetlpf.hpp selected (std, adapted std or cetl::pf26) agrees with the standard.
static_assert(cetl::saturating_add<std::int8_t>(100, 100) == 127, "");
static_assert(cetl::saturating_sub<std::uint8_t>(3, 5) == 0, "");
static_assert(cetl::saturating_mul<std::int8_t>(-128, -1) == 127, "");
static_assert(cetl::saturating_div<std::int8_t>(-128, -1) == 127, "");
static_assert(cetl::saturating_cast<std::uint8_t>(-5) == 0, "");
static_assert(cetl::saturating_cast<std::int8_t>(696) == 127, "");
static_assert(std::is_same<decltype(cetl::saturating_add(std::int8_t{1}, std::int8_t{2})), std::int8_t>::value, "");

// +----------------------------------------------------------------------+
// | Oracles. long long is exact for every operation on the types up to 32 bits (signed) and for add/sub on the
// | unsigned 32-bit type; unsigned long long covers unsigned 32-bit multiplication.
// +----------------------------------------------------------------------+
template <typename T>
std::string show(T value)
{
    return std::is_signed<T>::value ? std::to_string(static_cast<long long>(value))
                                    : std::to_string(static_cast<unsigned long long>(value));
}

template <typename T>
long long clamp_ll(long long v)
{
    const long long lo = static_cast<long long>(std::numeric_limits<T>::min());
    const long long hi = static_cast<long long>(std::numeric_limits<T>::max());
    return std::min(std::max(v, lo), hi);
}

template <typename T>
unsigned long long clamp_ull(unsigned long long v)
{
    return std::min(v, static_cast<unsigned long long>(std::numeric_limits<T>::max()));
}

/// Checks every operation on one operand pair against the long long oracle (types up to 32 bits only).
template <typename T>
void check_pair_ll(T x, T y)
{
    static_assert(sizeof(T) <= 4, "long long oracle");
    const long long   a       = static_cast<long long>(x);
    const long long   b       = static_cast<long long>(y);
    const std::string context = show(x) + ", " + show(y);
    ASSERT_EQ(clamp_ll<T>(a + b), static_cast<long long>(saturating_add(x, y))) << context;
    ASSERT_EQ(clamp_ll<T>(a + b), static_cast<long long>(saturating_add_portable(x, y))) << context;
    ASSERT_EQ(clamp_ll<T>(a - b), static_cast<long long>(saturating_sub(x, y))) << context;
    ASSERT_EQ(clamp_ll<T>(a - b), static_cast<long long>(saturating_sub_portable(x, y))) << context;
    if (std::is_signed<T>::value)
    {
        ASSERT_EQ(clamp_ll<T>(a * b), static_cast<long long>(saturating_mul(x, y))) << context;
        ASSERT_EQ(clamp_ll<T>(a * b), static_cast<long long>(saturating_mul_portable(x, y))) << context;
    }
    else
    {
        const unsigned long long product = static_cast<unsigned long long>(a) * static_cast<unsigned long long>(b);
        ASSERT_EQ(clamp_ull<T>(product), static_cast<unsigned long long>(saturating_mul(x, y))) << context;
        ASSERT_EQ(clamp_ull<T>(product), static_cast<unsigned long long>(saturating_mul_portable(x, y))) << context;
    }
    if (b != 0)
    {
        ASSERT_EQ(clamp_ll<T>(a / b), static_cast<long long>(saturating_div(x, y))) << context;
    }
}

template <typename T>
std::vector<T> edge_values()
{
    return {std::numeric_limits<T>::min(),
            static_cast<T>(std::numeric_limits<T>::min() + 1),
            static_cast<T>(0),
            static_cast<T>(1),
            static_cast<T>(2),
            static_cast<T>(-1),
            static_cast<T>(-2),
            static_cast<T>(std::numeric_limits<T>::max() / 2),
            static_cast<T>(std::numeric_limits<T>::max() - 1),
            std::numeric_limits<T>::max()};
}

// +----------------------------------------------------------------------+
// | 8-bit types: every operand pair.
// +----------------------------------------------------------------------+
template <typename T>
void exhaustive_8bit()
{
    static_assert(sizeof(T) == 1, "");
    const int lo = std::numeric_limits<T>::min();
    const int hi = std::numeric_limits<T>::max();
    for (int a = lo; a <= hi; ++a)
    {
        for (int b = lo; b <= hi; ++b)
        {
            check_pair_ll(static_cast<T>(a), static_cast<T>(b));
        }
    }
}

TEST(PF26NumericTest, ExhaustiveInt8)
{
    exhaustive_8bit<std::int8_t>();
}

TEST(PF26NumericTest, ExhaustiveUint8)
{
    exhaustive_8bit<std::uint8_t>();
}

// +----------------------------------------------------------------------+
// | 16-bit types: every value of the first operand against the edge values and a sample of the second.
// +----------------------------------------------------------------------+
template <typename T>
void sampled_16bit()
{
    static_assert(sizeof(T) == 2, "");
    std::vector<T> second = edge_values<T>();
    for (long v = std::numeric_limits<T>::min(); v <= std::numeric_limits<T>::max(); v += 257)
    {
        second.push_back(static_cast<T>(v));
    }
    for (long a = std::numeric_limits<T>::min(); a <= std::numeric_limits<T>::max(); ++a)
    {
        for (const T y : second)
        {
            check_pair_ll(static_cast<T>(a), y);
        }
    }
}

TEST(PF26NumericTest, SampledInt16)
{
    sampled_16bit<std::int16_t>();
}

TEST(PF26NumericTest, SampledUint16)
{
    sampled_16bit<std::uint16_t>();
}

// +----------------------------------------------------------------------+
// | 32-bit types: edge values crossed with each other and with random operands.
// +----------------------------------------------------------------------+
template <typename T>
void random_32bit()
{
    static_assert(sizeof(T) == 4, "");
    const std::vector<T> edges = edge_values<T>();
    for (const T x : edges)
    {
        for (const T y : edges)
        {
            check_pair_ll(x, y);
        }
    }
    std::mt19937                     generator(39U);
    std::uniform_int_distribution<T> any(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    for (int i = 0; i < 100000; ++i)
    {
        const T x = any(generator);
        const T y = (i % 4 == 0) ? edges[static_cast<std::size_t>(i / 4) % edges.size()] : any(generator);
        check_pair_ll(x, y);
    }
}

TEST(PF26NumericTest, RandomInt32)
{
    random_32bit<std::int32_t>();
}

TEST(PF26NumericTest, RandomUint32)
{
    random_32bit<std::uint32_t>();
}

// +----------------------------------------------------------------------+
// | 64-bit types: edge values against hand-computed results, and (where the compiler has 128-bit integers) random
// | operands against a 128-bit oracle.
// +----------------------------------------------------------------------+
TEST(PF26NumericTest, Int64Edges)
{
    EXPECT_EQ(i64_max, saturating_add(i64_max, std::int64_t{1}));
    EXPECT_EQ(i64_max, saturating_add_portable(i64_max, std::int64_t{1}));
    EXPECT_EQ(i64_min, saturating_add(i64_min, std::int64_t{-1}));
    EXPECT_EQ(std::int64_t{-1}, saturating_add(i64_max, i64_min));
    EXPECT_EQ(i64_min, saturating_sub(i64_min, std::int64_t{1}));
    EXPECT_EQ(i64_min, saturating_sub_portable(i64_min, std::int64_t{1}));
    EXPECT_EQ(i64_max, saturating_sub(std::int64_t{0}, i64_min));
    EXPECT_EQ(i64_max, saturating_sub(i64_max, i64_min));
    EXPECT_EQ(i64_max, saturating_mul(i64_min, std::int64_t{-1}));
    EXPECT_EQ(i64_max, saturating_mul_portable(i64_min, std::int64_t{-1}));
    EXPECT_EQ(i64_min, saturating_mul(i64_min, std::int64_t{1}));
    EXPECT_EQ(i64_min, saturating_mul(std::int64_t{2}, i64_min));
    EXPECT_EQ(i64_max, saturating_mul(std::int64_t{-2}, i64_min));
    EXPECT_EQ(INT64_C(-9223372036854775807), saturating_mul(std::int64_t{-1}, i64_max));
    EXPECT_EQ(i64_max, saturating_div(i64_min, std::int64_t{-1}));
    EXPECT_EQ(INT64_C(-9223372036854775807), saturating_div(i64_max, std::int64_t{-1}));
    EXPECT_EQ(std::int64_t{0}, saturating_div(std::int64_t{0}, i64_min));
}

TEST(PF26NumericTest, Uint64Edges)
{
    EXPECT_EQ(u64_max, saturating_add(u64_max, std::uint64_t{1}));
    EXPECT_EQ(u64_max, saturating_add_portable(u64_max, std::uint64_t{1}));
    EXPECT_EQ(u64_max, saturating_add(u64_max, u64_max));
    EXPECT_EQ(std::uint64_t{0}, saturating_sub(std::uint64_t{0}, u64_max));
    EXPECT_EQ(std::uint64_t{0}, saturating_sub_portable(std::uint64_t{0}, std::uint64_t{1}));
    EXPECT_EQ(std::uint64_t{1}, saturating_sub(u64_max, static_cast<std::uint64_t>(u64_max - 1U)));
    EXPECT_EQ(u64_max, saturating_mul(u64_max, std::uint64_t{2}));
    EXPECT_EQ(u64_max, saturating_mul_portable(u64_max, std::uint64_t{2}));
    EXPECT_EQ(u64_max, saturating_mul(UINT64_C(4294967296), UINT64_C(4294967296)));
    EXPECT_EQ(UINT64_C(18446744065119617025), saturating_mul(UINT64_C(4294967295), UINT64_C(4294967295)));
    EXPECT_EQ(std::uint64_t{1}, saturating_div(u64_max, u64_max));
}

#if CETLVAST_HAS_INT128

template <typename T>
using wide_t = std::conditional_t<std::is_signed<T>::value, cetlvast_int128, cetlvast_uint128>;

template <typename T>
T clamp_wide(cetlvast_int128 v)
{
    const cetlvast_int128 lo = std::numeric_limits<T>::min();
    const cetlvast_int128 hi = std::numeric_limits<T>::max();
    return static_cast<T>(std::min(std::max(v, lo), hi));
}

template <typename T>
void check_pair_wide(T x, T y)
{
    // Products of two 64-bit values fit in the 128-bit type of the same signedness; sums and differences are
    // evaluated in the signed 128-bit type so that unsigned subtraction cannot wrap in the oracle.
    const cetlvast_int128 a       = x;
    const cetlvast_int128 b       = y;
    const wide_t<T>       product = static_cast<wide_t<T>>(x) * static_cast<wide_t<T>>(y);
    const std::string     context = show(x) + ", " + show(y);
    ASSERT_EQ(clamp_wide<T>(a + b), saturating_add(x, y)) << context;
    ASSERT_EQ(clamp_wide<T>(a + b), saturating_add_portable(x, y)) << context;
    ASSERT_EQ(clamp_wide<T>(a - b), saturating_sub(x, y)) << context;
    ASSERT_EQ(clamp_wide<T>(a - b), saturating_sub_portable(x, y)) << context;
    const T expected_product =
        std::is_signed<T>::value
            ? clamp_wide<T>(static_cast<cetlvast_int128>(product))
            : static_cast<T>(std::min(static_cast<cetlvast_uint128>(product), static_cast<cetlvast_uint128>(u64_max)));
    ASSERT_EQ(expected_product, saturating_mul(x, y)) << context;
    ASSERT_EQ(expected_product, saturating_mul_portable(x, y)) << context;
    if (b != 0)
    {
        ASSERT_EQ(clamp_wide<T>(a / b), saturating_div(x, y)) << context;
    }
}

template <typename T>
void random_64bit()
{
    static_assert(sizeof(T) == 8, "");
    const std::vector<T> edges = edge_values<T>();
    for (const T x : edges)
    {
        for (const T y : edges)
        {
            check_pair_wide(x, y);
        }
    }
    std::mt19937_64                  generator(39U);
    std::uniform_int_distribution<T> any(std::numeric_limits<T>::min(), std::numeric_limits<T>::max());
    for (int i = 0; i < 100000; ++i)
    {
        const T x = (i % 3 == 0) ? edges[static_cast<std::size_t>(i / 3) % edges.size()] : any(generator);
        const T y = (i % 5 == 0) ? edges[static_cast<std::size_t>(i / 5) % edges.size()] : any(generator);
        check_pair_wide(x, y);
        // Values near the limits are where multiplication overflow decisions are made.
        check_pair_wide(static_cast<T>(x / 2), static_cast<T>(y % 4));
    }
}

TEST(PF26NumericTest, RandomInt64)
{
    random_64bit<std::int64_t>();
}

TEST(PF26NumericTest, RandomUint64)
{
    random_64bit<std::uint64_t>();
}

#endif  // CETLVAST_HAS_INT128

// +----------------------------------------------------------------------+
// | saturating_cast: every value of the 8- and 16-bit source types into every fixed-width type.
// +----------------------------------------------------------------------+
template <typename To>
To expected_cast(long long v)
{
    if (std::is_signed<To>::value)
    {
        return static_cast<To>(clamp_ll<To>(v));
    }
    return (v < 0) ? static_cast<To>(0) : static_cast<To>(clamp_ull<To>(static_cast<unsigned long long>(v)));
}

template <typename To, typename From>
void exhaustive_cast()
{
    static_assert(sizeof(From) <= 2, "");
    for (long v = std::numeric_limits<From>::min(); v <= std::numeric_limits<From>::max(); ++v)
    {
        const From x = static_cast<From>(v);
        ASSERT_EQ(expected_cast<To>(v), saturating_cast<To>(x)) << show(x) << " to " << sizeof(To) << " bytes";
    }
}

template <typename From>
void exhaustive_casts_from()
{
    exhaustive_cast<std::int8_t, From>();
    exhaustive_cast<std::uint8_t, From>();
    exhaustive_cast<std::int16_t, From>();
    exhaustive_cast<std::uint16_t, From>();
    exhaustive_cast<std::int32_t, From>();
    exhaustive_cast<std::uint32_t, From>();
    exhaustive_cast<std::int64_t, From>();
    exhaustive_cast<std::uint64_t, From>();
}

TEST(PF26NumericTest, ExhaustiveCastsFromNarrowTypes)
{
    exhaustive_casts_from<std::int8_t>();
    exhaustive_casts_from<std::uint8_t>();
    exhaustive_casts_from<std::int16_t>();
    exhaustive_casts_from<std::uint16_t>();
}

TEST(PF26NumericTest, CastsBetweenWideTypes)
{
    EXPECT_EQ(std::numeric_limits<std::int32_t>::max(), saturating_cast<std::int32_t>(i64_max));
    EXPECT_EQ(std::numeric_limits<std::int32_t>::min(), saturating_cast<std::int32_t>(i64_min));
    EXPECT_EQ(std::numeric_limits<std::uint32_t>::max(), saturating_cast<std::uint32_t>(u64_max));
    EXPECT_EQ(std::uint32_t{0}, saturating_cast<std::uint32_t>(i64_min));
    EXPECT_EQ(i64_max, saturating_cast<std::int64_t>(u64_max));
    EXPECT_EQ(static_cast<std::uint64_t>(i64_max), saturating_cast<std::uint64_t>(i64_max));
    EXPECT_EQ(std::uint64_t{0}, saturating_cast<std::uint64_t>(std::int64_t{-1}));
    EXPECT_EQ(std::int64_t{INT64_C(4294967295)},
              saturating_cast<std::int64_t>(std::numeric_limits<std::uint32_t>::max()));
}

// +----------------------------------------------------------------------+
// | The polyfill header's selection.
// +----------------------------------------------------------------------+
TEST(PF26CetlPfTest, SelectedFunctionsSaturate)
{
    EXPECT_EQ(127, cetl::saturating_add<std::int8_t>(100, 100));
    EXPECT_EQ(0U, cetl::saturating_sub<std::uint8_t>(3, 5));
    EXPECT_EQ(127, cetl::saturating_mul<std::int8_t>(-128, -1));
    EXPECT_EQ(127, cetl::saturating_div<std::int8_t>(-128, -1));
    EXPECT_EQ(std::numeric_limits<std::uint32_t>::max(), cetl::saturating_cast<std::uint32_t>(INT64_C(5000000000)));
    EXPECT_EQ(0U, cetl::saturating_cast<std::uint32_t>(std::int64_t{-1}));
    EXPECT_EQ(u64_max, cetl::saturating_mul(u64_max, u64_max));
}

// +----------------------------------------------------------------------+
// | Precondition
// +----------------------------------------------------------------------+
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

static void TestDivideByZero()
{
    flush_coverage_on_death();
    const int zero   = 0;
    const int result = saturating_div(1, zero);
    (void) result;
}

TEST(DeathTestPF26Numeric, DivideByZero)
{
    EXPECT_DEATH(TestDivideByZero(), "CDE_numeric_001");
}

#endif  // CETL_ENABLE_DEBUG_ASSERT

}  // namespace
