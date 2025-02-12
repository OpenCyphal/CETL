/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_ctor_4 : public ::testing::Test
{};

TYPED_TEST_SUITE(test_optional_combinations_ctor_4, testing_types, );

using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations_ctor_4, ctor)
{
    using F = foo<TypeParam>;
    using B = bar<TypeParam>;
    static_assert(std::is_constructible<F, B>::value, "");
    static_assert(std::is_constructible<B, F>::value, "");
    static_assert(std::is_constructible<optional<F>, optional<B>>::value, "");
    static_assert(std::is_constructible<optional<B>, optional<F>>::value, "");
    std::uint32_t f_dtor = 0;
    std::uint32_t b_dtor = 0;
    optional<F>   f1;
    f1.emplace(12345).configure_destruction_counter(&f_dtor);
    optional<B> b1(f1);  // Use implicit constructor because foo is implicitly convertible to bar
    b1.value().configure_destruction_counter(&b_dtor);
    {
        optional<F> f2(b1);  // Use explicit constructor because bar is not implicitly convertible to foo
        f2.value().configure_destruction_counter(&f_dtor);
        EXPECT_EQ(12345, f1.value().value);
        EXPECT_EQ(12345, b1.value().value);
        EXPECT_EQ(12345, f2.value().value);
        // Ensure no copy/move of the base took place.
        EXPECT_EQ(0, f1->get_copy_ctor_count());
        EXPECT_EQ(0, f1->get_move_ctor_count());
        EXPECT_EQ(0, f1->get_copy_assignment_count());
        EXPECT_EQ(0, f1->get_move_assignment_count());
        EXPECT_EQ(0, b1->get_copy_ctor_count());
        EXPECT_EQ(0, b1->get_move_ctor_count());
        EXPECT_EQ(0, b1->get_copy_assignment_count());
        EXPECT_EQ(0, b1->get_move_assignment_count());
        EXPECT_EQ(0, f2->get_copy_ctor_count());
        EXPECT_EQ(0, f2->get_move_ctor_count());
        EXPECT_EQ(0, f2->get_copy_assignment_count());
        EXPECT_EQ(0, f2->get_move_assignment_count());
        EXPECT_EQ(0, f_dtor);
        EXPECT_EQ(0, b_dtor);
    }
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ(0, b_dtor);
    b1.reset();
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
    f1.reset();
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 2 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
    // Test empty copy.
    optional<B> b2(f1);
    EXPECT_FALSE(b2);
}
