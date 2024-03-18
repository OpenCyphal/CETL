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

static constexpr auto test_assignment_2_constexpr()
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::in_place_index;
    using cetl::pf17::get;
    struct U
    {
        constexpr explicit U(const std::int64_t value)
            : value(value)
        {
        }
        constexpr U(const U&) noexcept            = delete;
        constexpr U(U&&) noexcept                 = default;
        constexpr U& operator=(const U&) noexcept = delete;
        constexpr U& operator=(U&&) noexcept      = default;
        ~U() noexcept                             = default;
        std::int64_t value                        = 0;
    };
    variant<monostate, U> v1(in_place_index<1>, 123456);
    variant<monostate, U> v2;
    v2 = std::move(v1);
    return get<1>(v2).value;
}
static_assert(test_assignment_2_constexpr() == 123456, "");

}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
