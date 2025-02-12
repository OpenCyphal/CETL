/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT


#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_assignment_2 : public ::testing::Test
{};


TYPED_TEST_SUITE(test_optional_combinations_assignment_2, testing_types, );


using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

/// For the copy assignment to work, T shall be both copy-constructible and copy-assignable.
template <typename T,
          std::uint8_t CopyCtorPolicy       = T::copy_ctor_policy_value,
          std::uint8_t CopyAssignmentPolicy = T::copy_assignment_policy_value>
struct test_assignment_2
{
    static void test()
    {
        std::uint32_t destructed = 0;
        optional<T>   opt1;
        optional<T>   opt2;
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        // Empty to empty.
        opt1 = opt2;
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        // Non-empty to empty. A copy ctor is invoked.
        opt1.emplace().configure_destruction_counter(&destructed);
        EXPECT_TRUE(opt1);
        EXPECT_FALSE(opt2);
        opt2 = opt1;
        EXPECT_TRUE(opt1);
        EXPECT_TRUE(opt2);
        EXPECT_EQ(0U, opt1->get_copy_ctor_count());
        EXPECT_EQ(0U, opt1->get_move_ctor_count());
        EXPECT_EQ(0U, opt1->get_copy_assignment_count());
        EXPECT_EQ(0U, opt1->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_copy_ctor_count());
        EXPECT_EQ(0U, opt2->get_move_ctor_count());
        EXPECT_EQ(0U, opt2->get_copy_assignment_count());
        EXPECT_EQ(0U, opt2->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        // Non-empty to non-empty. A copy assignment is invoked.
        opt1 = opt2;
        EXPECT_TRUE(opt1);
        EXPECT_TRUE(opt2);
        // The copy ctor count is copied over from opt2!
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_copy_ctor_count());
        EXPECT_EQ(0U, opt1->get_move_ctor_count());
        EXPECT_EQ((T::copy_assignment_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_copy_assignment_count());
        EXPECT_EQ(0U, opt1->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_copy_ctor_count());
        EXPECT_EQ(0U, opt2->get_move_ctor_count());
        EXPECT_EQ(0U, opt2->get_copy_assignment_count());
        EXPECT_EQ(0U, opt2->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        // Empty to non-empty. Destructor is invoked.
        opt1 = cetl::pf17::nullopt;
        EXPECT_FALSE(opt1);
        EXPECT_TRUE(opt2);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        opt2 = opt1;
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
    }
};
template <typename T, std::uint8_t CopyCtorPolicy>
struct test_assignment_2<T, CopyCtorPolicy, policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static void test() {}
};
template <typename T, std::uint8_t CopyAssignmentPolicy>
struct test_assignment_2<T, policy_deleted, CopyAssignmentPolicy>
{
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static void test() {}
};
template <typename T>
struct test_assignment_2<T, policy_deleted, policy_deleted>  // This is to avoid the specialization ambiguity.
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static void test() {}
};

TYPED_TEST(test_optional_combinations_assignment_2, assignment)
{
    test_assignment_2<TypeParam>::test();
}

/// ------------------------------------------------------------------------------------------------
