/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_ctor_6 : public ::testing::Test
{};

TYPED_TEST_SUITE(test_optional_combinations_ctor_6, testing_types, );

using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations_ctor_6, ctor)
{
    std::uint32_t            f_dtor = 0;
    optional<foo<TypeParam>> f1(cetl::pf17::in_place, 12345);
    f1.value().configure_destruction_counter(&f_dtor);
    EXPECT_TRUE(f1.has_value());
    EXPECT_EQ(12345, f1.value().value);
    // Ensure no copy/move of the base took place.
    EXPECT_EQ(0, f1->get_copy_ctor_count());
    EXPECT_EQ(0, f1->get_move_ctor_count());
    EXPECT_EQ(0, f1->get_copy_assignment_count());
    EXPECT_EQ(0, f1->get_move_assignment_count());
    EXPECT_EQ(0, f_dtor);
    f1 = cetl::pf17::nullopt;
    EXPECT_FALSE(f1);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
}
