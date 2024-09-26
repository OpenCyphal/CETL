/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
// CSpell: words chronomorphize

#include <cetl/type_traits_ext.hpp>  // The tested header always goes first.
#include <cstdint>

// NOLINTBEGIN(*-explicit-constructor)

namespace test_find
{
using cetl::type_traits_ext::find_v;

static_assert(find_v<std::is_integral, int, char, double, std::int64_t, std::int16_t, std::int8_t> == 0, "");
static_assert(find_v<std::is_integral, double, float, std::int64_t, std::int16_t, std::int8_t> == 2, "");
static_assert(find_v<std::is_integral, double, float> == std::numeric_limits<std::size_t>::max(), "");
}  // namespace test_find

// --------------------------------------------------------------------------------------------

namespace test_count
{
using cetl::type_traits_ext::count_v;

static_assert(count_v<std::is_integral, int, char, double, std::int64_t, std::int16_t, std::int8_t> == 5, "");
static_assert(count_v<std::is_integral, double, float, std::int64_t, std::int16_t, std::int8_t> == 3, "");
static_assert(count_v<std::is_integral, double, float> == 0, "");
}  // namespace test_count

// --------------------------------------------------------------------------------------------

namespace test_partial
{
using cetl::type_traits_ext::count_v;
using cetl::type_traits_ext::partial;

static_assert(partial<std::is_same, int>::type<int>::value, "");
static_assert(!partial<std::is_same, int>::type<long>::value, "");
static_assert(count_v<partial<std::is_same, int>::type, char, double, int, long, int> == 2, "");
}  // namespace test_partial

// --------------------------------------------------------------------------------------------

namespace test_convertible_without_narrowing
{
using cetl::type_traits_ext::is_convertible_without_narrowing;

static_assert(is_convertible_without_narrowing<int, long long>::value, "");
static_assert(!is_convertible_without_narrowing<long long, int>::value, "");

static_assert(is_convertible_without_narrowing<float, long double>::value, "");
static_assert(!is_convertible_without_narrowing<long double, float>::value, "");

struct foo_ref
{
    foo_ref(const std::uint8_t&);
};
struct foo_val
{
    foo_val(std::uint8_t);
};

static_assert(is_convertible_without_narrowing<std::uint8_t, foo_val>::value, "");
static_assert(is_convertible_without_narrowing<std::uint8_t, foo_ref>::value, "");

static_assert(!is_convertible_without_narrowing<std::uint16_t, foo_val>::value, "");
#if defined(__GNUC__) && !defined(__clang__)  // https://twitter.com/PavelKirienko/status/1766446559971914238
static_assert(!is_convertible_without_narrowing<std::uint16_t, foo_ref>::value, "");
#endif

}  // namespace test_convertible_without_narrowing

// --------------------------------------------------------------------------------------------

namespace test_best_conversion_index
{
using cetl::type_traits_ext::best_conversion_index_v;
using cetl::type_traits_ext::universal_predicate;
using cetl::type_traits_ext::is_convertible_without_narrowing;
using cetl::type_traits_ext::partial;

constexpr auto bad = std::numeric_limits<std::size_t>::max();

// Easy cases
static_assert(best_conversion_index_v<universal_predicate, float, long, float, double, bool> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, float&, long, float, double, bool> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, long, float> == 0, "");
static_assert(best_conversion_index_v<universal_predicate, long&, float> == 0, "");
static_assert(best_conversion_index_v<universal_predicate, int, float, int> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, int&, float, int> == 1, "");
// Ambiguous case
static_assert(best_conversion_index_v<universal_predicate, int, long, float, bool> == bad, "");
// No longer ambiguous because we prohibit narrowing conversions
static_assert(
    best_conversion_index_v<partial<is_convertible_without_narrowing, int>::template type, int, float, bool, long> == 2,
    "");

static_assert(best_conversion_index_v<std::is_signed, long, char, long, unsigned long> == 1, "");
static_assert(best_conversion_index_v<std::is_unsigned, long, char, long, unsigned long> == 2, "");
static_assert(best_conversion_index_v<std::is_volatile, char, int, const int, volatile int> == 2, "");

struct foo
{
    foo(std::uint16_t);
};
// Shifting priorities depending on the available conversions.
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::uint64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::uint32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::uint16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::uint8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::int64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::int32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::int16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo, std::int8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint8_t, foo> == 0, "");
//
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::uint64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::uint32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::uint16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::uint8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::int64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::int32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::int16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo, std::int8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint16_t, foo> == 0, "");
//
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::uint64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::uint32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::uint16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::uint8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::int64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::int32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::int16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo, std::int8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint32_t, foo> == 0, "");
//
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::uint64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::uint32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::uint16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::uint8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::int64_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::int32_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::int16_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo, std::int8_t> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, std::uint64_t, foo> == 0, "");

static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::uint64_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::uint32_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::uint16_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::uint8_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::int64_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::int32_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::int16_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo, std::int8_t> == 0, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint8_t>::template type, std::uint8_t, foo> == 0, "");
//
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::uint64_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::uint32_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::uint16_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::uint8_t> == 0, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::int64_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::int32_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::int16_t> == 0, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo, std::int8_t> == 0, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint16_t>::template type, std::uint16_t, foo> == 0, "");
//
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::uint64_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::uint32_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::uint16_t> == bad, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::uint8_t> == bad, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::int64_t> == 1, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::int32_t> == bad, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::int16_t> == bad, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo, std::int8_t> == bad, "");
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, std::uint32_t>::template type, std::uint32_t, foo> == bad, "");

}  // namespace test_best_conversion_index

// NOLINTEND(*-explicit-constructor)
