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
TYPED_TEST(test_smf_policy_combinations, ctor_7)
{
    struct T final : TypeParam
    {
        explicit T(const std::int64_t val)
            : value(val)
        {
        }
        std::int64_t value = 0;
    };
    using cetl::pf17::variant;
    using cetl::pf17::in_place_index;
    using cetl::pf17::get;
    std::uint32_t destructed = 0;
    {
        const variant<std::int64_t, T, T> var(in_place_index<1>, 123456);
        EXPECT_EQ(1, var.index());
        EXPECT_FALSE(var.valueless_by_exception());
        get<1>(var).configure_destruction_counter(&destructed);
        EXPECT_EQ(123456, get<1>(var).value);
        EXPECT_EQ(0, get<1>(var).get_copy_ctor_count());
        EXPECT_EQ(0, get<1>(var).get_move_ctor_count());
        EXPECT_EQ(0, get<1>(var).get_copy_assignment_count());
        EXPECT_EQ(0, get<1>(var).get_move_assignment_count());
        EXPECT_EQ(0, destructed);
    }
    EXPECT_EQ((T::dtor_policy_value == cetlvast::smf_policies::policy_nontrivial) ? 1 : 0, destructed);
}

static_assert(cetl::pf17::variant<int, void*, double>(cetl::pf17::in_place_index<2>).index() == 2, "");

}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
