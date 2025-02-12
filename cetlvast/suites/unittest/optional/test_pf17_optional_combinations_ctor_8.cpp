/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_ctor_8 : public ::testing::Test
{};

TYPED_TEST_SUITE(test_optional_combinations_ctor_8, testing_types, );

using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations_ctor_8, ctor)
{
    std::uint32_t            f_dtor = 0;
    std::uint32_t            b_dtor = 0;
    optional<foo<TypeParam>> f1(12345);  // Use explicit constructor.
    optional<bar<TypeParam>> b1(23456);  // Use implicit constructor.
    f1.value().configure_destruction_counter(&f_dtor);
    b1.value().configure_destruction_counter(&b_dtor);
    EXPECT_TRUE(f1.has_value());
    EXPECT_EQ(12345, f1.value().value);
    EXPECT_TRUE(b1.has_value());
    EXPECT_EQ(23456, b1.value().value);
    // Ensure no copy/move of the base took place.
    EXPECT_EQ(0, f1->get_copy_ctor_count());
    EXPECT_EQ(0, f1->get_move_ctor_count());
    EXPECT_EQ(0, f1->get_copy_assignment_count());
    EXPECT_EQ(0, f1->get_move_assignment_count());
    EXPECT_EQ(0, f_dtor);
    EXPECT_EQ(0, b1->get_copy_ctor_count());
    EXPECT_EQ(0, b1->get_move_ctor_count());
    EXPECT_EQ(0, b1->get_copy_assignment_count());
    EXPECT_EQ(0, b1->get_move_assignment_count());
    EXPECT_EQ(0, b_dtor);
    f1 = cetl::pf17::nullopt;
    b1 = cetl::pf17::nullopt;
    EXPECT_FALSE(f1);
    EXPECT_FALSE(b1);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
}
