/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/variant.hpp>  // The tested header always goes first.
#include "test_pf17_variant.hpp"
#include <string>

namespace cetlvast
{
namespace unittest
{
namespace pf17
{
namespace variant
{
TYPED_TEST(test_smf_policy_combinations, ctor_4)
{
    using cetl::pf17::variant;
    using cetl::pf17::get;
    using cetl::pf17::monostate;
    struct tag_t
    {
        tag_t()                        = default;
        tag_t(const tag_t&)            = delete;
        tag_t(tag_t&&)                 = delete;
        tag_t& operator=(const tag_t&) = delete;
        tag_t& operator=(tag_t&&)      = delete;
        ~tag_t()                       = default;
    } const tag;
    struct T : TypeParam
    {
        T(const tag_t&) {}  // NOLINT(*-explicit-constructor)
    };
    using V = variant<std::int64_t, float, T, monostate>;
    EXPECT_EQ(0, V(123456).index());
    EXPECT_EQ(1, V(123.456F).index());
    EXPECT_EQ(2, V(tag).index());
    EXPECT_EQ(3, V(monostate{}).index());
    static_assert(!std::is_constructible<V, double>::value, "");  // Ambiguity!

    static_assert(1 == variant<int, bool>(true).index(), "");

    // Example from cppreference
    variant<float, long, double> v4 = 0;
    EXPECT_EQ(0, get<long>(v4));

    // Example from Scott
    EXPECT_EQ(1, (variant<std::string, void const*>("abc").index()));
    EXPECT_EQ(0, (variant<std::string, void*>("abc").index()));
}

static_assert(cetl::pf17::variant<int, void*, double>(123).index() == 0, "");
static_assert(cetl::pf17::variant<int, void*, double>(123.0).index() == 2, "");

}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
