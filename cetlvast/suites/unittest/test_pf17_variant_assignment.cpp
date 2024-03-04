/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
// CSpell: words borked

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
/// For the copy assignment to work, T shall be both copy-constructible and copy-assignable. Also destructible.
/// This test is very large because the copy assignment is the most complex operation in the variant class.
/// Notation: noexcept -- can't throw; throwable -- can throw but doesn't; throwing -- can throw and does.
template <typename T,
          std::uint8_t CopyCtorPolicy       = T::copy_ctor_policy_value,
          std::uint8_t CopyAssignmentPolicy = T::copy_assignment_policy_value>
struct test_assignment_1
{
    static void test_matching_assignment_noexcept()
    {
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetlvast::smf_policies::policy_nontrivial;
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, std::int64_t> v1;
            variant<T, std::int64_t> v2;
            get<T>(v1).configure_destruction_counter(&dtor1);
            get<T>(v2).configure_destruction_counter(&dtor2);
            v1 = v2;              // Invoke copy assignment.
            EXPECT_EQ(0, dtor1);  // Copy assignment does not destroy the source.
            EXPECT_EQ(0, dtor2);
            EXPECT_EQ(0, get<T>(v1).get_copy_ctor_count());
            EXPECT_EQ(0, get<T>(v1).get_move_ctor_count());
            EXPECT_EQ((T::copy_assignment_policy_value == policy_nontrivial) ? 1 : 0,
                      get<T>(v1).get_copy_assignment_count());
            EXPECT_EQ(0, get<T>(v1).get_move_assignment_count());
            EXPECT_EQ(0, get<T>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<T>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<T>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<T>(v2).get_move_assignment_count());
        }
        EXPECT_EQ(0, dtor1);  // This is because of the assignment.
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, dtor2);
    }

    static void test_matching_assignment_throwing()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetlvast::smf_policies::policy_nontrivial;
        struct U : T
        {
            U()                  = default;
            U(const U&) noexcept = default;
            U(U&&) noexcept      = default;
            U& operator=(const U&)
            {
                throw std::exception();
            }
            U& operator=(U&&) noexcept = default;
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<U, std::int64_t> v1;
            variant<U, std::int64_t> v2;
            get<U>(v1).configure_destruction_counter(&dtor1);
            get<U>(v2).configure_destruction_counter(&dtor2);
            EXPECT_ANY_THROW(v1 = v2);  // Invoke copy assignment. It will throw.
            EXPECT_EQ(0, dtor1);        // Neither is destroyed.
            EXPECT_EQ(0, dtor2);
            EXPECT_FALSE(v1.valueless_by_exception());  // Throwing assignment does not render the variant valueless.
            EXPECT_FALSE(v2.valueless_by_exception());
            EXPECT_EQ(0, get<U>(v1).get_copy_ctor_count());
            EXPECT_EQ(0, get<U>(v1).get_move_ctor_count());
            EXPECT_EQ(0, get<U>(v1).get_copy_assignment_count());  // Did not succeed.
            EXPECT_EQ(0, get<U>(v1).get_move_assignment_count());
            EXPECT_EQ(0, get<U>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<U>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<U>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<U>(v2).get_move_assignment_count());
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);  // Assignment did not succeed.
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
#endif
    }

    static void test_nonmatching_copy_noexcept_move_noexcept()
    {
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_nontrivial;
        // A direct-copyable type because the copy ctor is noexcept.
        struct U : T
        {};
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, U> v1(in_place_type<T>);
            variant<T, U> v2(in_place_type<U>);
            get<T>(v1).configure_destruction_counter(&dtor1);
            get<1>(v2).configure_destruction_counter(&dtor2);
            v1 = v2;  // Invoke copy ctor.
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
            EXPECT_EQ(0, dtor2);
            EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<1>(v1).get_copy_ctor_count());
            EXPECT_EQ(0, get<1>(v1).get_move_ctor_count());
            EXPECT_EQ(0, get<1>(v1).get_copy_assignment_count());
            EXPECT_EQ(0, get<1>(v1).get_move_assignment_count());
            EXPECT_EQ(0, get<1>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<1>(v2).get_move_assignment_count());
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, dtor2);
    }

    static void test_nonmatching_copy_throwable_move_noexcept()
    {
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_deleted;
        using cetlvast::smf_policies::policy_trivial;
        using cetlvast::smf_policies::policy_nontrivial;
        // A non-direct-copyable type because the copy ctor is not noexcept but the move ctor is noexcept.
        struct U : T
        {
            U() = default;
            U(const U& other)  // not noexcept
                : T(other)
            {
            }
            U(U&&) noexcept                       = default;
            U& operator=(const U& other) noexcept = default;
            U& operator=(U&&) noexcept            = default;
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, U> v1(in_place_type<T>);
            variant<T, U> v2(in_place_type<U>);
            get<T>(v1).configure_destruction_counter(&dtor1);
            get<1>(v2).configure_destruction_counter(&dtor2);
            v1 = v2;  // Invoke copy ctor. This time we use a temporary side-copy.
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);  // T
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);  // side-copy
            switch (T::move_ctor_policy_value)
            {
            case policy_deleted:
                EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 2 : 0, get<1>(v1).get_copy_ctor_count());
                break;
            case policy_trivial:
                EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<1>(v1).get_copy_ctor_count());
                break;
            case policy_nontrivial:
                EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<1>(v1).get_copy_ctor_count());
                EXPECT_EQ(1, get<1>(v1).get_move_ctor_count());
                break;
            default:
                std::terminate();
            }
            EXPECT_EQ(0, get<1>(v1).get_copy_assignment_count());
            EXPECT_EQ(0, get<1>(v1).get_move_assignment_count());
            EXPECT_EQ(0, get<1>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<1>(v2).get_move_assignment_count());
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 3 : 0, dtor2);
    }

    static void test_nonmatching_copy_throwing_move_noexcept()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_nontrivial;
        // A non-direct-copyable type because the copy ctor is not noexcept but the move ctor is noexcept.
        struct U : T
        {
            U() = default;
            U(const U& other)  // Throws.
                : T(other)     // But the base is constructed first! This means its dtor will be called after throwing.
            {
                throw std::exception();
            }
            U(U&&) noexcept                       = default;
            U& operator=(const U& other) noexcept = default;
            U& operator=(U&&) noexcept            = default;
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, U> v1(in_place_type<T>);
            variant<T, U> v2(in_place_type<U>);
            get<T>(v1).configure_destruction_counter(&dtor1);
            get<1>(v2).configure_destruction_counter(&dtor2);
            EXPECT_ANY_THROW(v1 = v2);  // Invoke copy ctor. This time we use a temporary side-copy, which throws.
            // The side-copy got borked but v1 is retained in its original state. The dtor counter goes up anyway
            // because we throw the exception from the copy ctor after the base is already fully constructed;
            // when the exception is throw, the base destructor is called, which causes the counter to go up by one.
            EXPECT_EQ(0, dtor1);
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
            EXPECT_EQ(0, get<0>(v1).get_copy_ctor_count());
            EXPECT_EQ(0, get<0>(v1).get_move_ctor_count());
            EXPECT_EQ(0, get<0>(v1).get_copy_assignment_count());
            EXPECT_EQ(0, get<0>(v1).get_move_assignment_count());
            EXPECT_EQ(0, get<1>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<1>(v2).get_move_assignment_count());
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, dtor2);
#endif
    }

    static void test_nonmatching_copy_throwing_move_throwable()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_nontrivial;
        // A direct-copyable type because both the copy ctor and the move ctor are not noexcept.
        struct U : T
        {
            U() = default;
            U(const U& other)  // Throws.
                : T(other)     // But the base is constructed first! This means its dtor will be called after throwing.
            {
                throw std::exception();
            }
            U(U&& other)    // Throws but we don't get to this part. NOLINT(*-noexcept-move-constructor)
                : T(other)  // NOLINT(*-move-constructor-init)
            {
                std::terminate();  // Unreachable.
            }
            U& operator=(const U& other) noexcept = default;
            U& operator=(U&&) noexcept            = default;
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, U> v1(in_place_type<T>);
            variant<T, U> v2(in_place_type<U>);
            get<T>(v1).configure_destruction_counter(&dtor1);
            get<1>(v2).configure_destruction_counter(&dtor2);
            EXPECT_ANY_THROW(v1 = v2);  // Invoke copy ctor. Here we use a direct assignment, which throws,
            // but this happens after the old value in v1 is already destroyed, leaving v1 valueless.
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
            // The dtor2 counter is one because the base which does the counting is already constructed by the time
            // the exception is thrown, hence its destructor is invoked, which increments the counter.
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
            EXPECT_TRUE(v1.valueless_by_exception());
            EXPECT_EQ(0, get<1>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<1>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<1>(v2).get_move_assignment_count());
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, dtor2);
#endif
    }

    static void test_valueless()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_nontrivial;
        struct U : T
        {
            U()
            {
                throw std::exception();
            }
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, U> v1;
            variant<T, U> v2;
            // Make v1 valueless because the ctor of U throws.
            EXPECT_ANY_THROW(v1.template emplace<U>());
            EXPECT_TRUE(v1.valueless_by_exception());
            EXPECT_FALSE(v2.valueless_by_exception());
            // Copy valueless into non-valueless.
            v2 = v1;
            EXPECT_TRUE(v1.valueless_by_exception());
            EXPECT_TRUE(v2.valueless_by_exception());
            // Copy valueless into valueless.
            v1 = v2;
            EXPECT_TRUE(v1.valueless_by_exception());
            EXPECT_TRUE(v2.valueless_by_exception());
            // Make v2 non-valueless, then copy that into v1.
            v2.template emplace<T>();
            get<T>(v2).configure_destruction_counter(&dtor2);
            v1 = v2;
            get<T>(v1).configure_destruction_counter(&dtor1);
            EXPECT_FALSE(v1.valueless_by_exception());
            EXPECT_FALSE(v2.valueless_by_exception());
            EXPECT_EQ(0, dtor1);
            EXPECT_EQ(0, dtor2);
            EXPECT_EQ(0, v1.index());
            EXPECT_EQ(0, v2.index());
            EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<T>(v1).get_copy_ctor_count());
            EXPECT_EQ(0, get<T>(v1).get_move_ctor_count());
            EXPECT_EQ(0, get<T>(v1).get_copy_assignment_count());
            EXPECT_EQ(0, get<T>(v1).get_move_assignment_count());
            EXPECT_EQ(0, get<T>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<T>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<T>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<T>(v2).get_move_assignment_count());
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
#endif
    }

    static void test()
    {
        test_matching_assignment_noexcept();
        test_matching_assignment_throwing();
        test_nonmatching_copy_noexcept_move_noexcept();
        test_nonmatching_copy_throwable_move_noexcept();
        test_nonmatching_copy_throwing_move_noexcept();
        test_nonmatching_copy_throwing_move_throwable();
        test_valueless();
    }
};
template <typename T, std::uint8_t CopyCtorPolicy>
struct test_assignment_1<T, CopyCtorPolicy, cetlvast::smf_policies::policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_assignable<cetl::pf17::variant<int, T>>::value, "");
    static void test() {}
};
template <typename T, std::uint8_t CopyAssignmentPolicy>
struct test_assignment_1<T, cetlvast::smf_policies::policy_deleted, CopyAssignmentPolicy>
{
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<cetl::pf17::variant<int, T>>::value, "");
    static_assert(!std::is_copy_assignable<cetl::pf17::variant<int, T>>::value, "");
    static void test() {}
};
template <typename T>  // This is to avoid specialization ambiguity.
struct test_assignment_1<T, cetlvast::smf_policies::policy_deleted, cetlvast::smf_policies::policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<cetl::pf17::variant<int, T>>::value, "");
    static_assert(!std::is_copy_assignable<cetl::pf17::variant<int, T>>::value, "");
    static void test() {}
};

TYPED_TEST(test_smf_policy_combinations, assignment_1)
{
    test_assignment_1<TypeParam>::test();
}

// --------------------------------------------------------------------------------------------

/// For the move assignment to work, every T in Ts shall be both
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
struct test_assignment_2
{
    static void test_matching_assignment_noexcept()
    {
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetlvast::smf_policies::policy_nontrivial;
        using cetlvast::smf_policies::policy_deleted;
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, std::int64_t> v1;
            variant<T, std::int64_t> v2;
            get<T>(v1).configure_destruction_counter(&dtor1);
            get<T>(v2).configure_destruction_counter(&dtor2);
            v2 = std::move(v1);                         // Invoke copy assignment.
            EXPECT_FALSE(v1.valueless_by_exception());  // NOLINT(*-use-after-move)
            EXPECT_FALSE(v2.valueless_by_exception());
            // Check v1 counters.
            EXPECT_EQ(0U, get<T>(v1).get_copy_ctor_count());
            EXPECT_EQ(0U, get<T>(v1).get_move_ctor_count());
            EXPECT_EQ(0U, get<T>(v1).get_copy_assignment_count());
            EXPECT_EQ(0U, get<T>(v1).get_move_assignment_count());
            EXPECT_EQ(0, dtor1);
            // Check v2 counters.
            EXPECT_EQ(0U, get<T>(v2).get_copy_ctor_count());
            EXPECT_EQ(0U, get<T>(v2).get_move_ctor_count());
            EXPECT_EQ(((T::copy_assignment_policy_value == policy_nontrivial) &&
                       (T::move_assignment_policy_value == policy_deleted))
                          ? 1
                          : 0,
                      get<T>(v2).get_copy_assignment_count());
            EXPECT_EQ((T::move_assignment_policy_value == policy_nontrivial) ? 1 : 0,
                      get<T>(v2).get_move_assignment_count());
            EXPECT_EQ(0, dtor2);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, dtor1);
        EXPECT_EQ(0, dtor2);  // This is because of the assignment.
    }

    static void test_matching_assignment_throwing()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetlvast::smf_policies::policy_deleted;
        using cetlvast::smf_policies::policy_nontrivial;
        struct U : T
        {
            U()                             = default;
            U(const U&) noexcept            = default;
            U(U&&) noexcept                 = default;
            U& operator=(const U&) noexcept = default;
            U& operator=(U&&)  // NOLINT(*-noexcept-move-constructor)
            {
                throw std::exception();
            }
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<U, std::int64_t> v1;
            variant<U, std::int64_t> v2;
            get<U>(v1).configure_destruction_counter(&dtor1);
            get<U>(v2).configure_destruction_counter(&dtor2);
            EXPECT_ANY_THROW(v2 = std::move(v1));  // Invoke move assignment. It will throw.
            // Destination does not become valueless despite the exception.
            EXPECT_FALSE(v1.valueless_by_exception());  // NOLINT(*-use-after-move)
            EXPECT_FALSE(v2.valueless_by_exception());
            // Check v1 counters.
            EXPECT_EQ(0U, get<U>(v1).get_copy_ctor_count());
            EXPECT_EQ(0U, get<U>(v1).get_move_ctor_count());
            EXPECT_EQ(0U, get<U>(v1).get_copy_assignment_count());
            EXPECT_EQ(0U, get<U>(v1).get_move_assignment_count());
            EXPECT_EQ(0U, dtor1);
            // Check v2 counters.
            EXPECT_EQ(0U, get<U>(v2).get_copy_ctor_count());
            EXPECT_EQ(0U, get<U>(v2).get_move_ctor_count());
            EXPECT_EQ(0U, get<U>(v2).get_copy_assignment_count());  // Assignment did not succeed.
            EXPECT_EQ(0U, get<U>(v2).get_move_assignment_count());  // Assignment did not succeed.
            EXPECT_EQ(0U, dtor2);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);  // Assignment did not succeed.
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
#endif
    }

    static void test_nonmatching_noexcept()
    {
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_deleted;
        using cetlvast::smf_policies::policy_nontrivial;
        struct U : T
        {};
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<U, T> v1(in_place_type<U>);
            variant<U, T> v2(in_place_type<T>);
            get<U>(v1).configure_destruction_counter(&dtor1);
            get<T>(v2).configure_destruction_counter(&dtor2);
            v2 = std::move(v1);                         // Invoke move construction.
            EXPECT_FALSE(v1.valueless_by_exception());  // NOLINT(*-use-after-move)
            EXPECT_FALSE(v2.valueless_by_exception());
            EXPECT_EQ(0, v1.index());
            EXPECT_EQ(0, v2.index());
            // Check v1 counters.
            EXPECT_EQ(0U, get<U>(v1).get_copy_ctor_count());
            EXPECT_EQ(0U, get<U>(v1).get_move_ctor_count());
            EXPECT_EQ(0U, get<U>(v1).get_copy_assignment_count());
            EXPECT_EQ(0U, get<U>(v1).get_move_assignment_count());
            EXPECT_EQ(0U, dtor1);
            // Check v2 counters.
            EXPECT_EQ(((T::copy_ctor_policy_value == policy_nontrivial) &&
                       (T::move_ctor_policy_value == policy_deleted))
                          ? 1
                          : 0,
                      get<U>(v2).get_copy_ctor_count());
            EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<U>(v2).get_move_ctor_count());
            EXPECT_EQ(0U, get<U>(v2).get_copy_assignment_count());
            EXPECT_EQ(0U, get<U>(v2).get_move_assignment_count());
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);  // T destroyed.
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
    }

    static void test_nonmatching_throwing()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::variant_npos;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_deleted;
        using cetlvast::smf_policies::policy_nontrivial;
        struct U : T
        {
            U()                  = default;
            U(const U&) noexcept = default;
            U(U&& other)               // NOLINT(*-noexcept-move-constructor)
                : T(std::move(other))  // This resolves either to the move ctor or copy ctor!
            {
                throw std::exception();
            }
            U& operator=(const U&) noexcept = default;
            U& operator=(U&&) noexcept      = default;
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<U, T> v1(in_place_type<U>);
            variant<U, T> v2(in_place_type<T>);
            get<U>(v1).configure_destruction_counter(&dtor1);
            get<T>(v2).configure_destruction_counter(&dtor2);
            EXPECT_ANY_THROW(v2 = std::move(v1));       // Invoke move construction.
            EXPECT_FALSE(v1.valueless_by_exception());  // NOLINT(*-use-after-move)
            EXPECT_TRUE(v2.valueless_by_exception());   // v2 is valueless because the move ctor of U throws.
            EXPECT_EQ(0, v1.index());
            EXPECT_EQ(variant_npos, v2.index());
            // Check v1 counters.
            EXPECT_EQ(0U, get<U>(v1).get_copy_ctor_count());
            EXPECT_EQ(0U, get<U>(v1).get_move_ctor_count());
            EXPECT_EQ(0U, get<U>(v1).get_copy_assignment_count());
            EXPECT_EQ(0U, get<U>(v1).get_move_assignment_count());
            // The dtor counter is 1 because the base which does the counting is already constructed by the time
            // the exception is thrown, hence its destructor is invoked, which increments the counter.
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
            // v2 counters cannot be checked because it is valueless, except for the dtor counter.
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
#endif
    }

    static void test_valueless()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        using cetlvast::smf_policies::policy_deleted;
        using cetlvast::smf_policies::policy_nontrivial;
        struct U : T
        {
            U()
            {
                throw std::exception();
            }
        };
        std::uint32_t dtor1 = 0;
        std::uint32_t dtor2 = 0;
        {
            variant<T, U> v1;
            variant<T, U> v2;
            // Make v1 valueless because the ctor of U throws.
            EXPECT_ANY_THROW(v1.template emplace<U>());
            EXPECT_TRUE(v1.valueless_by_exception());
            EXPECT_FALSE(v2.valueless_by_exception());
            // Move valueless into non-valueless.
            v2 = std::move(v1);
            EXPECT_TRUE(v1.valueless_by_exception());  // NOLINT(*-use-after-move)
            EXPECT_TRUE(v2.valueless_by_exception());
            // Move valueless into valueless.
            v1 = std::move(v2);
            EXPECT_TRUE(v1.valueless_by_exception());
            EXPECT_TRUE(v2.valueless_by_exception());  // NOLINT(*-use-after-move)
            // Make v2 non-valueless, then move that into v1.
            v2.template emplace<T>();
            get<T>(v2).configure_destruction_counter(&dtor2);
            v1 = std::move(v2);
            get<T>(v1).configure_destruction_counter(&dtor1);
            EXPECT_FALSE(v1.valueless_by_exception());
            EXPECT_FALSE(v2.valueless_by_exception());  // NOLINT(*-use-after-move)
            EXPECT_EQ(0, dtor1);
            EXPECT_EQ(0, dtor2);
            EXPECT_EQ(0, v1.index());
            EXPECT_EQ(0, v2.index());
            EXPECT_EQ(((T::copy_ctor_policy_value == policy_nontrivial) &&
                       (T::move_ctor_policy_value == policy_deleted))
                          ? 1
                          : 0,
                      get<T>(v1).get_copy_ctor_count());
            EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<T>(v1).get_move_ctor_count());
            EXPECT_EQ(0, get<T>(v1).get_copy_assignment_count());
            EXPECT_EQ(0, get<T>(v1).get_move_assignment_count());
            EXPECT_EQ(0, get<T>(v2).get_copy_ctor_count());
            EXPECT_EQ(0, get<T>(v2).get_move_ctor_count());
            EXPECT_EQ(0, get<T>(v2).get_copy_assignment_count());
            EXPECT_EQ(0, get<T>(v2).get_move_assignment_count());
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor1);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor2);
#endif
    }

    static void test()
    {
        test_matching_assignment_noexcept();
        test_matching_assignment_throwing();
        test_nonmatching_noexcept();
        test_nonmatching_throwing();
        test_valueless();
    }
};
template <typename T, std::uint8_t CopyAssignmentPolicy, std::uint8_t MoveAssignmentPolicy>
struct test_assignment_2<T,
                         cetlvast::smf_policies::policy_deleted,
                         CopyAssignmentPolicy,
                         cetlvast::smf_policies::policy_deleted,
                         MoveAssignmentPolicy>
{
    static_assert(std::is_copy_assignable<T>::value == (CopyAssignmentPolicy != cetlvast::smf_policies::policy_deleted),
                  "");
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(std::is_move_assignable<T>::value, "");  // requires either copy or move assignment
    static_assert(!std::is_move_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<cetl::pf17::variant<T>>::value, "");
    static_assert(!std::is_copy_assignable<cetl::pf17::variant<T>>::value, "");
    static_assert(!std::is_move_constructible<cetl::pf17::variant<T>>::value, "");
    static_assert(!std::is_move_assignable<cetl::pf17::variant<T>>::value, "");
    static void test() {}
};
template <typename T, std::uint8_t CopyCtorPolicy, std::uint8_t MoveCtorPolicy>
struct test_assignment_2<T,
                         CopyCtorPolicy,
                         cetlvast::smf_policies::policy_deleted,
                         MoveCtorPolicy,
                         cetlvast::smf_policies::policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(std::is_copy_constructible<T>::value == (CopyCtorPolicy != cetlvast::smf_policies::policy_deleted),
                  "");
    static_assert(!std::is_move_assignable<T>::value, "");
    static_assert(std::is_move_constructible<T>::value, "");  // requires either copy or move ctor
    static_assert(std::is_copy_constructible<cetl::pf17::variant<T>>::value ==
                      (CopyCtorPolicy != cetlvast::smf_policies::policy_deleted),
                  "");
    static_assert(!std::is_copy_assignable<cetl::pf17::variant<T>>::value, "");
    static_assert(std::is_move_constructible<cetl::pf17::variant<T>>::value, "");  // requires either copy or move ctor
    static_assert(!std::is_move_assignable<cetl::pf17::variant<T>>::value, "");
    static void test() {}
};
template <typename T>
struct test_assignment_2<T,
                         cetlvast::smf_policies::policy_deleted,
                         cetlvast::smf_policies::policy_deleted,
                         cetlvast::smf_policies::policy_deleted,
                         cetlvast::smf_policies::policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_move_assignable<T>::value, "");
    static_assert(!std::is_move_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<cetl::pf17::variant<T>>::value, "");
    static_assert(!std::is_copy_assignable<cetl::pf17::variant<T>>::value, "");
    static_assert(!std::is_move_constructible<cetl::pf17::variant<T>>::value, "");
    static_assert(!std::is_move_assignable<cetl::pf17::variant<T>>::value, "");
    static void test() {}
};

TYPED_TEST(test_smf_policy_combinations, assignment_2)
{
    test_assignment_2<TypeParam>::test();
}

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, assignment_3)
{
    // TODO FIXME NOT IMPLEMENTED
}
}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
