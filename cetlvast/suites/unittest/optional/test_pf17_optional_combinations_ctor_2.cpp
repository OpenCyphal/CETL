/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_ctor_2 : public ::testing::Test
{};

TYPED_TEST_SUITE(test_optional_combinations_ctor_2, testing_types, );

using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

template <typename T, std::uint8_t CopyCtorPolicy = T::copy_ctor_policy_value>
struct test_ctor_2
{
    static void test()
    {
        std::uint32_t destructed = 0;
        optional<T>   opt;
        opt.emplace().configure_destruction_counter(&destructed);
        {
            optional<T> opt2 = opt;
            EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_copy_ctor_count());
            EXPECT_EQ(0U, opt2->get_move_ctor_count());
            EXPECT_EQ(0U, opt2->get_copy_assignment_count());
            EXPECT_EQ(0U, opt2->get_move_assignment_count());
            EXPECT_EQ(0U, destructed);
            EXPECT_EQ(0U, opt->get_copy_ctor_count());
            EXPECT_EQ(0U, opt->get_move_ctor_count());
            EXPECT_EQ(0U, opt->get_copy_assignment_count());
            EXPECT_EQ(0U, opt->get_move_assignment_count());
            opt.reset();
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
    }
};
template <typename T>
struct test_ctor_2<T, policy_deleted>
{
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    static void test() {}
};

TYPED_TEST(test_optional_combinations_ctor_2, ctor)
{
    test_ctor_2<TypeParam>::test();
}
