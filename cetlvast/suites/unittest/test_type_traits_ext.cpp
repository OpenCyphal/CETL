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
