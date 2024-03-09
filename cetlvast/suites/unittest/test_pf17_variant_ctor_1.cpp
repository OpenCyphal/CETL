/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/variant.hpp>  // The tested header always goes first.
#include "test_pf17_variant.hpp"

namespace cetlvast
{
namespace unittest
{
namespace pf17
{
namespace variant
{
TYPED_TEST(test_smf_policy_combinations, ctor_1)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::holds_alternative;
    using cetl::pf17::get;
    using cetl::pf17::get_if;

    struct T : public TypeParam
    {
        explicit T(const monostate) {}
    };

    static_assert(std::is_default_constructible<variant<int>>::value, "");
    static_assert(!std::is_default_constructible<variant<T>>::value, "");

    using V = variant<std::int64_t, T, monostate, T>;
    V var;  // The first alternative shall be value-initialized.
    EXPECT_EQ(0, var.index());
    EXPECT_FALSE(var.valueless_by_exception());
    EXPECT_TRUE(holds_alternative<std::int64_t>(var));
    EXPECT_FALSE(holds_alternative<monostate>(var));

    EXPECT_EQ(0, get<std::int64_t>(var));  // value-initialized
    EXPECT_EQ(0, get<0>(var));             // value-initialized
    EXPECT_TRUE(get_if<std::int64_t>(&var));
    EXPECT_FALSE(get_if<monostate>(&var));
    EXPECT_TRUE(get_if<0>(&var));
}

static_assert(cetl::pf17::variant<int, void*>().index() == 0, "");
}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
