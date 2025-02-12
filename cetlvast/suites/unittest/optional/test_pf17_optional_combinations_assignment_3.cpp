/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT


#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_assignment_3 : public ::testing::Test
{};


TYPED_TEST_SUITE(test_optional_combinations_assignment_3, testing_types, );


using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

/// For the move assignment to work, T shall be both
/// (move-constructible or copy-constructible) and (move-assignable or copy-assignable).
/// Notes:
/// - A type does not have to implement move assignment operator in order to satisfy MoveAssignable:
///   a copy assignment operator that takes its parameter by value or as a const Type&, will bind to rvalue argument.
/// - A type does not have to implement a move constructor to satisfy MoveConstructible:
///   a copy constructor that takes a const T& argument can bind rvalue expressions.
template <typename T,
          std::uint8_t CopyCtorPolicy       = T::copy_ctor_policy_value,
          std::uint8_t CopyAssignmentPolicy = T::copy_assignment_policy_value,
          std::uint8_t MoveCtorPolicy       = T::move_ctor_policy_value,
          std::uint8_t MoveAssignmentPolicy = T::move_assignment_policy_value>
struct test_assignment_3
{
    static void test()
    {
        std::uint32_t destructed = 0;
        optional<T>   opt1;
        optional<T>   opt2;
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        // Empty to empty.
        opt1 = std::move(opt2);
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        // Non-empty to empty. A copy/move ctor is invoked.
        opt1.emplace().configure_destruction_counter(&destructed);
        EXPECT_TRUE(opt1);
        EXPECT_FALSE(opt2);
        opt2 = std::move(opt1);
        EXPECT_TRUE(opt1);
        EXPECT_TRUE(opt2);
        // Check opt1 counters.
        EXPECT_EQ(0U, opt1->get_copy_ctor_count());
        EXPECT_EQ(0U, opt1->get_move_ctor_count());
        EXPECT_EQ(0U, opt1->get_copy_assignment_count());
        EXPECT_EQ(0U, opt1->get_move_assignment_count());
        // Check opt2 counters.
        EXPECT_EQ(((T::copy_ctor_policy_value == policy_nontrivial) && (T::move_ctor_policy_value == policy_deleted))
                      ? 1
                      : 0,
                  opt2->get_copy_ctor_count());
        EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_move_ctor_count());
        EXPECT_EQ(0U, opt2->get_copy_assignment_count());
        EXPECT_EQ(0U, opt2->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        // Non-empty to non-empty. A copy/move assignment is invoked.
        opt1 = std::move(opt2);
        EXPECT_TRUE(opt1);
        EXPECT_TRUE(opt2);
        // Check opt1 counters. The copy/move ctor count is copied over from opt2!
        EXPECT_EQ(((T::copy_ctor_policy_value == policy_nontrivial) && (T::move_ctor_policy_value == policy_deleted))
                      ? 1
                      : 0,
                  opt1->get_copy_ctor_count());
        EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_move_ctor_count());
        EXPECT_EQ(((T::copy_assignment_policy_value == policy_nontrivial) &&
                   (T::move_assignment_policy_value == policy_deleted))
                      ? 1
                      : 0,
                  opt1->get_copy_assignment_count());
        EXPECT_EQ((T::move_assignment_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_move_assignment_count());
        // Check opt2 counters.
        EXPECT_EQ(((T::copy_ctor_policy_value == policy_nontrivial) && (T::move_ctor_policy_value == policy_deleted))
                      ? 1
                      : 0,
                  opt2->get_copy_ctor_count());
        EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_move_ctor_count());
        EXPECT_EQ(0U, opt2->get_copy_assignment_count());
        EXPECT_EQ(0U, opt2->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        // Empty to non-empty. Destructor is invoked.
        opt1 = cetl::pf17::nullopt;
        EXPECT_FALSE(opt1);
        EXPECT_TRUE(opt2);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        opt2 = std::move(opt1);
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
    }
};
template <typename T, std::uint8_t CopyAssignmentPolicy, std::uint8_t MoveAssignmentPolicy>
struct test_assignment_3<T, policy_deleted, CopyAssignmentPolicy, policy_deleted, MoveAssignmentPolicy>
{
    static_assert(std::is_copy_assignable<T>::value == (CopyAssignmentPolicy != policy_deleted), "");
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(std::is_move_assignable<T>::value, "");  // requires either copy or move assignment
    static_assert(!std::is_move_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static_assert(!std::is_move_constructible<optional<T>>::value, "");
    static_assert(!std::is_move_assignable<optional<T>>::value, "");
    static void test() {}
};
template <typename T, std::uint8_t CopyCtorPolicy, std::uint8_t MoveCtorPolicy>
struct test_assignment_3<T, CopyCtorPolicy, policy_deleted, MoveCtorPolicy, policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(std::is_copy_constructible<T>::value == (CopyCtorPolicy != policy_deleted), "");
    static_assert(!std::is_move_assignable<T>::value, "");
    static_assert(std::is_move_constructible<T>::value, "");  // requires either copy or move ctor
    static_assert(std::is_copy_constructible<optional<T>>::value == (CopyCtorPolicy != policy_deleted), "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static_assert(std::is_move_constructible<optional<T>>::value, "");  // requires either copy or move ctor
    static_assert(!std::is_move_assignable<optional<T>>::value, "");
    static void test() {}
};
template <typename T>
struct test_assignment_3<T, policy_deleted, policy_deleted, policy_deleted, policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_move_assignable<T>::value, "");
    static_assert(!std::is_move_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static_assert(!std::is_move_constructible<optional<T>>::value, "");
    static_assert(!std::is_move_assignable<optional<T>>::value, "");
    static void test() {}
};

TYPED_TEST(test_optional_combinations_assignment_3, assignment)
{
    test_assignment_3<TypeParam>::test();
}

/// ------------------------------------------------------------------------------------------------
