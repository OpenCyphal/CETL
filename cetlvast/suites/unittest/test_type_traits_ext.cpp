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

// GCC 7 does not diagnose narrowing conversions of constructor arguments during list-initialization of a class type,
// so it accepts e.g. `struct foo { foo(bool); };` from a `long` (which is also what std::variant does).
// See the note on cetl::type_traits_ext::is_convertible_without_narrowing.
#if defined(__GNUC__) && !defined(__clang__) && (__GNUC__ <= 7)
#    define CETLVAST_GCC7_CTOR_ARG_NARROWING_UNDETECTED 1
#else
#    define CETLVAST_GCC7_CTOR_ARG_NARROWING_UNDETECTED 0
#endif

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
    foo_ref(const bool&);
};
struct foo_val
{
    foo_val(bool);
};

static_assert(is_convertible_without_narrowing<bool, foo_val>::value, "");
static_assert(is_convertible_without_narrowing<bool, foo_ref>::value, "");

#if CETLVAST_GCC7_CTOR_ARG_NARROWING_UNDETECTED
static_assert(is_convertible_without_narrowing<long, foo_val>::value, "GCC 7 limitation; see the trait docs");
static_assert(is_convertible_without_narrowing<long, foo_ref>::value, "GCC 7 limitation; see the trait docs");
#else
static_assert(!is_convertible_without_narrowing<long, foo_val>::value, "");
#    if defined(__GNUC__) && !defined(__clang__)  // https://twitter.com/PavelKirienko/status/1766446559971914238
static_assert(!is_convertible_without_narrowing<long, foo_ref>::value, "");
#    endif
#endif

// Scalars are exact on all supported compilers, including cv-qualified ones.
static_assert(is_convertible_without_narrowing<int, const long>::value, "");
static_assert(is_convertible_without_narrowing<int, volatile int>::value, "");
static_assert(!is_convertible_without_narrowing<long long, const int>::value, "");
static_assert(!is_convertible_without_narrowing<int, volatile float>::value, "");

// Explicit constructors and non-copyable types.
struct explicit_ctor
{
    explicit explicit_ctor(int);
};
struct non_copyable
{
    non_copyable(int);
    non_copyable(const non_copyable&) = delete;
};
static_assert(!is_convertible_without_narrowing<int, explicit_ctor>::value, "");
static_assert(is_convertible_without_narrowing<int, non_copyable>::value, "");
static_assert(!is_convertible_without_narrowing<long long, non_copyable>::value, "");

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
static_assert(best_conversion_index_v<universal_predicate, float, long, float, bool> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, float&, long, float, bool> == 1, "");
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

static_assert(best_conversion_index_v<std::is_signed, long, signed char, long, unsigned long> == 1, "");
static_assert(best_conversion_index_v<std::is_unsigned, long, signed char, long, unsigned long> == 2, "");
static_assert(best_conversion_index_v<std::is_volatile, char, int, const int, volatile int> == 2, "");

struct foo
{
    foo(bool);
};
// Shifting priorities depending on the available conversions.
static_assert(best_conversion_index_v<universal_predicate, char, foo, int> == 1, "");
static_assert(best_conversion_index_v<universal_predicate, char, foo> == 0, "");

static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, signed char>::template type, signed char, foo, int> ==
                  1,
              "");
#if CETLVAST_GCC7_CTOR_ARG_NARROWING_UNDETECTED
static_assert(
    best_conversion_index_v<partial<is_convertible_without_narrowing, signed char>::template type, signed char, foo> ==
        0,
    "GCC 7 limitation; see the trait docs");
#else
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, signed char>::template type, signed char, foo> == bad,
              "");
#endif
static_assert(best_conversion_index_v<partial<is_convertible_without_narrowing, bool>::template type, bool, foo> == 0,
              "");

}  // namespace test_best_conversion_index

// NOLINTEND(*-explicit-constructor)
