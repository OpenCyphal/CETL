/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
// CSpell: words chronomorphize borked

#include <cetl/pf17/variant.hpp>
#include <cetlvast/helpers.hpp>
#include <cetlvast/typelist.hpp>
#include <cetlvast/smf_policies.hpp>
#include <gtest/gtest.h>

namespace smf_policy_combinations
{
using cetlvast::smf_policies::copy_ctor_policy;
using cetlvast::smf_policies::move_ctor_policy;
using cetlvast::smf_policies::copy_assignment_policy;
using cetlvast::smf_policies::move_assignment_policy;
using cetlvast::smf_policies::dtor_policy;
using cetlvast::smf_policies::policy_deleted;
using cetlvast::smf_policies::policy_trivial;
using cetlvast::smf_policies::policy_nontrivial;
namespace typelist = cetlvast::typelist;

using policy_combinations = cetlvast::typelist::cartesian_product<  //
    std::tuple<copy_ctor_policy<policy_deleted>,                    //
               copy_ctor_policy<policy_trivial>,
               copy_ctor_policy<policy_nontrivial>>,
    std::tuple<move_ctor_policy<policy_deleted>,  //
               move_ctor_policy<policy_trivial>,
               move_ctor_policy<policy_nontrivial>>,
    std::tuple<copy_assignment_policy<policy_deleted>,  //
               copy_assignment_policy<policy_trivial>,
               copy_assignment_policy<policy_nontrivial>>,
    std::tuple<move_assignment_policy<policy_deleted>,  //
               move_assignment_policy<policy_trivial>,
               move_assignment_policy<policy_nontrivial>>,
    std::tuple<dtor_policy<policy_trivial>,  //
               dtor_policy<policy_nontrivial>>>;

/// This is a long list of all the possible combinations of special function policies.
/// Derive from each type to test all possible policies.
using testing_types = typelist::into<::testing::Types>::from<
    typelist::map<cetlvast::smf_policies::combine_bases, policy_combinations>::type>;
}  // namespace smf_policy_combinations

template <typename>
class test_smf_policy_combinations : public ::testing::Test
{};
TYPED_TEST_SUITE(test_smf_policy_combinations, smf_policy_combinations::testing_types, );

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, smf_asserts)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;

    using T = TypeParam;

    // Enrich the variant with SMF-trivial types to ensure we always pick the most restrictive policy.
    using V = variant<int, T, monostate, T>;
    static_assert(sizeof(V) == (cetlvast::align_size_up(sizeof(T), sizeof(std::size_t)) + sizeof(std::size_t)), "");

    // Ensure trivial copy/move policies are correctly inherited from the value type.
    // copy ctor
    static_assert(std::is_copy_constructible<T>::value == std::is_copy_constructible<V>::value, "");
    static_assert(std::is_trivially_copy_constructible<T>::value == std::is_trivially_copy_constructible<V>::value, "");
    // move ctor
    static_assert(std::is_move_constructible<T>::value == std::is_move_constructible<V>::value, "");
    static_assert(std::is_trivially_move_constructible<T>::value == std::is_trivially_move_constructible<V>::value, "");
    // copy assign
    // We don't check is_trivially_copyable because this check operates on memory representation rather than
    // the availability of the corresponding SMFs. As such, the is_trivially_copyable can be true even if the
    // copy ctor is deleted.
    static_assert((std::is_copy_assignable<T>::value &&     //
                   std::is_copy_constructible<T>::value &&  //
                   std::is_destructible<T>::value) == std::is_copy_assignable<V>::value,
                  "");
    static_assert((std::is_trivially_copy_assignable<T>::value &&     //
                   std::is_trivially_copy_constructible<T>::value &&  //
                   std::is_trivially_destructible<T>::value) == std::is_trivially_copy_assignable<V>::value,
                  "");
    // move assign
    static_assert((std::is_move_assignable<T>::value &&     //
                   std::is_move_constructible<T>::value &&  //
                   std::is_destructible<T>::value) == std::is_move_assignable<V>::value,
                  "");
    static_assert((std::is_trivially_move_assignable<T>::value &&     //
                   std::is_trivially_move_constructible<T>::value &&  //
                   std::is_trivially_destructible<T>::value) == std::is_trivially_move_assignable<V>::value,
                  "");
    // dtor
    static_assert(std::is_trivially_destructible<T>::value == std::is_trivially_destructible<V>::value, "");
}

// --------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------

template <typename SMF, std::uint8_t CopyCtorPolicy = SMF::copy_ctor_policy_value>
struct test_ctor_2
{
    static void test_basic()
    {
        using cetl::pf17::variant;
        using cetl::pf17::in_place_type;
        using cetl::pf17::get;
        using cetl::pf17::monostate;
        using cetlvast::smf_policies::policy_nontrivial;
        struct T : SMF
        {
            explicit T(const std::int64_t val)
                : value(val)
            {
            }
            std::int64_t value = 0;
        };
        std::uint32_t destructed = 0;
        {
            const variant<T, std::int64_t, monostate> v1(in_place_type<T>, 123456);
            EXPECT_EQ(123456, get<T>(v1).value);
            get<T>(v1).configure_destruction_counter(&destructed);
            {
                variant<T, std::int64_t, monostate> v2(v1);
                EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<T>(v2).get_copy_ctor_count());
                EXPECT_EQ(123456, get<T>(v2).value);
                EXPECT_EQ(0, destructed);
                v2.template emplace<std::int64_t>(789);
                EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
            }
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
        // The valueless state cannot occur in a ctor test.
    }

    static void test_valueless()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::variant_npos;
        using cetl::pf17::get;
        using cetlvast::smf_policies::policy_nontrivial;
        struct T : SMF
        {};
        struct U : SMF
        {
            U()
            {
                throw std::exception();
            }
        };
        std::uint32_t destructed = 0;
        {
            variant<T, U> v1;
            get<T>(v1).configure_destruction_counter(&destructed);
            EXPECT_ANY_THROW(v1.template emplace<U>());
            EXPECT_EQ((U::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
            EXPECT_TRUE(v1.valueless_by_exception());
            {
                const variant<T, U> v2(v1);  // NOLINT(*-unnecessary-copy-initialization)
                EXPECT_TRUE(v1.valueless_by_exception());
                EXPECT_TRUE(v2.valueless_by_exception());
                EXPECT_EQ(variant_npos, v1.index());
                EXPECT_EQ(variant_npos, v2.index());
            }
        }
        EXPECT_EQ((U::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);  // Same.
#endif
    }

    static void test()
    {
        test_basic();
        test_valueless();
    }
};
template <typename SMF>
struct test_ctor_2<SMF, cetlvast::smf_policies::policy_deleted>
{
    static_assert(!std::is_copy_constructible<SMF>::value, "");
    static_assert(!std::is_copy_constructible<cetl::pf17::variant<SMF>>::value, "");
    static void test() {}
};

TYPED_TEST(test_smf_policy_combinations, ctor_2)
{
    test_ctor_2<TypeParam>::test();
}

// --------------------------------------------------------------------------------------------

// Caveat: types without a move constructor but with a copy constructor that accepts const T& arguments,
// satisfy std::is_move_constructible.
template <typename SMF,
          std::uint8_t CopyCtorPolicy = SMF::copy_ctor_policy_value,
          std::uint8_t MoveCtorPolicy = SMF::move_ctor_policy_value>
struct test_ctor_3
{
    static void test_basic()
    {
        using cetl::pf17::variant;
        using cetl::pf17::in_place_type;
        using cetl::pf17::get;
        using cetl::pf17::monostate;
        using cetlvast::smf_policies::policy_nontrivial;
        using cetlvast::smf_policies::policy_deleted;
        struct T : SMF
        {
            explicit T(const std::int64_t val)
                : value(val)
            {
            }
            T(T&& other) noexcept
                : SMF(std::forward<T>(other))
                , value(other.value)
            {
                other.value = 0;
            }
            std::int64_t value = 0;
        };
        std::uint32_t destructed = 0;
        {
            variant<T, std::int64_t, monostate> v1(in_place_type<T>, 123456);
            EXPECT_EQ(123456, get<T>(v1).value);
            get<T>(v1).configure_destruction_counter(&destructed);
            {
                variant<T, std::int64_t, monostate> v2(std::move(v1));
                EXPECT_EQ(((T::move_ctor_policy_value == policy_deleted) &&
                           (T::copy_ctor_policy_value == policy_nontrivial))
                              ? 1
                              : 0,
                          get<T>(v2).get_copy_ctor_count());
                EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<T>(v2).get_move_ctor_count());
                EXPECT_EQ(0, get<T>(v1).value);  // moved out
                EXPECT_EQ(123456, get<T>(v2).value);
                EXPECT_EQ(0, destructed);
                v2.template emplace<std::int64_t>(789);
                EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
                EXPECT_EQ(789, get<std::int64_t>(v2));
            }
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
        // The valueless state cannot occur in a ctor test.
    }

    static void test_valueless()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::variant_npos;
        using cetl::pf17::get;
        using cetlvast::smf_policies::policy_nontrivial;
        struct T : SMF
        {};
        struct U : SMF
        {
            U()
            {
                throw std::exception();
            }
        };
        std::uint32_t destructed = 0;
        {
            variant<T, U> v1;
            get<T>(v1).configure_destruction_counter(&destructed);
            EXPECT_ANY_THROW(v1.template emplace<U>());
            EXPECT_EQ((U::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
            EXPECT_TRUE(v1.valueless_by_exception());
            {
                const variant<T, U> v2(std::move(v1));
                EXPECT_TRUE(v1.valueless_by_exception());  // NOLINT(*-use-after-move)
                EXPECT_TRUE(v2.valueless_by_exception());
                EXPECT_EQ(variant_npos, v1.index());
                EXPECT_EQ(variant_npos, v2.index());
            }
        }
        EXPECT_EQ((U::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);  // Same.
#endif
    }

    static void test()
    {
        test_basic();
        test_valueless();
    }
};
template <typename SMF>
struct test_ctor_3<SMF, cetlvast::smf_policies::policy_deleted, cetlvast::smf_policies::policy_deleted>
{
    // Caveat: types without a move constructor but with a copy constructor that accepts const T& arguments,
    // satisfy std::is_move_constructible.
    static_assert(!std::is_move_constructible<SMF>::value, "");
    static_assert(!std::is_move_constructible<cetl::pf17::variant<SMF>>::value, "");
    static void test() {}
};

TYPED_TEST(test_smf_policy_combinations, ctor_3)
{
    test_ctor_3<TypeParam>::test();
}

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, ctor_4)
{
    using cetl::pf17::variant;
    using cetl::pf17::get;
    using cetl::pf17::monostate;
    struct tag_t
    {
        tag_t()                        = default;
        tag_t(const tag_t&)            = delete;
        tag_t(tag_t&&)                 = delete;
        tag_t& operator=(const tag_t&) = delete;
        tag_t& operator=(tag_t&&)      = delete;
        ~tag_t()                       = default;
    } const tag;
    struct T : TypeParam
    {
        T(const tag_t&) {}  // NOLINT(*-explicit-constructor)
    };
    using V = variant<std::int64_t, float, double, T, monostate>;
    EXPECT_EQ(0, V(123456).index());
    EXPECT_EQ(2, V(123.456).index());
    EXPECT_EQ(3, V(tag).index());
    EXPECT_EQ(4, V(monostate{}).index());
    static_assert(!std::is_constructible<V, float>::value, "");  // Float cannot be selected due to ambiguity.

    // Example from cppreference
    variant<float, long, double> v4 = 0;
    EXPECT_EQ(0, get<long>(v4));
}

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, ctor_5)
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
    using cetl::pf17::in_place_type;
    using cetl::pf17::get;
    std::uint32_t destructed = 0;
    {
        const variant<std::int64_t, T> var(in_place_type<T>, 123456);
        EXPECT_EQ(1, var.index());
        EXPECT_FALSE(var.valueless_by_exception());
        get<T>(var).configure_destruction_counter(&destructed);
        EXPECT_EQ(123456, get<T>(var).value);
        EXPECT_EQ(0, get<T>(var).get_copy_ctor_count());
        EXPECT_EQ(0, get<T>(var).get_move_ctor_count());
        EXPECT_EQ(0, get<T>(var).get_copy_assignment_count());
        EXPECT_EQ(0, get<T>(var).get_move_assignment_count());
        EXPECT_EQ(0, destructed);
    }
    EXPECT_EQ((T::dtor_policy_value == cetlvast::smf_policies::policy_nontrivial) ? 1 : 0, destructed);
}

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, ctor_6)
{
    struct T final : TypeParam
    {
        T(const std::initializer_list<std::int64_t> il)
            : value(static_cast<std::int64_t>(il.size()))
        {
        }
        std::int64_t value = 0;
    };
    using cetl::pf17::variant;
    using cetl::pf17::in_place_type;
    using cetl::pf17::get;
    std::uint32_t destructed = 0;
    {
        const variant<std::int64_t, T> var(in_place_type<T>, std::initializer_list<std::int64_t>{1, 2, 3, 4, 5, 6});
        EXPECT_EQ(1, var.index());
        EXPECT_FALSE(var.valueless_by_exception());
        get<T>(var).configure_destruction_counter(&destructed);
        EXPECT_EQ(6, get<T>(var).value);
        EXPECT_EQ(0, get<T>(var).get_copy_ctor_count());
        EXPECT_EQ(0, get<T>(var).get_move_ctor_count());
        EXPECT_EQ(0, get<T>(var).get_copy_assignment_count());
        EXPECT_EQ(0, get<T>(var).get_move_assignment_count());
        EXPECT_EQ(0, destructed);
    }
    EXPECT_EQ((T::dtor_policy_value == cetlvast::smf_policies::policy_nontrivial) ? 1 : 0, destructed);
}

// --------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, ctor_8)
{
    struct T final : TypeParam
    {
        T(const std::initializer_list<std::int64_t> il)
            : value(static_cast<std::int64_t>(il.size()))
        {
        }
        std::int64_t value = 0;
    };
    using cetl::pf17::variant;
    using cetl::pf17::in_place_index;
    using cetl::pf17::get;
    std::uint32_t destructed = 0;
    {
        const variant<std::int64_t, T, T> var(in_place_index<2>, std::initializer_list<std::int64_t>{1, 2, 3, 4, 5, 6});
        EXPECT_EQ(2, var.index());
        EXPECT_FALSE(var.valueless_by_exception());
        get<2>(var).configure_destruction_counter(&destructed);
        EXPECT_EQ(6, get<2>(var).value);
        EXPECT_EQ(0, get<2>(var).get_copy_ctor_count());
        EXPECT_EQ(0, get<2>(var).get_move_ctor_count());
        EXPECT_EQ(0, get<2>(var).get_copy_assignment_count());
        EXPECT_EQ(0, get<2>(var).get_move_assignment_count());
        EXPECT_EQ(0, destructed);
    }
    EXPECT_EQ((T::dtor_policy_value == cetlvast::smf_policies::policy_nontrivial) ? 1 : 0, destructed);
}

// --------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, emplace)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::get;
    using cetl::pf17::holds_alternative;
    struct T : TypeParam
    {
        explicit T(const std::int64_t val)
            : value(val)
        {
        }
        T(const std::initializer_list<std::int64_t> il, const std::int64_t val)
            : value(static_cast<std::int64_t>(il.size()) + val)
        {
        }
        std::int64_t value = 0;
    };
    variant<monostate, T, monostate, std::int64_t, std::int64_t> var;

    EXPECT_EQ(123456, var.template emplace<T>(123456).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(123456, get<T>(var).value);
    monostate* mono = &var.template emplace<0>();
    (void) mono;

    EXPECT_EQ(992, var.template emplace<T>(std::initializer_list<std::int64_t>{1, 2, 3, 4, 5}, 987).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(992, get<T>(var).value);
    mono = &var.template emplace<0>();
    (void) mono;

    EXPECT_EQ(123456, var.template emplace<1>(123456).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(123456, get<1>(var).value);
    mono = &var.template emplace<0>();
    (void) mono;

    EXPECT_EQ(992, var.template emplace<1>(std::initializer_list<std::int64_t>{1, 2, 3, 4, 5}, 987).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(992, get<1>(var).value);
    mono = &var.template emplace<0>();
    (void) mono;
}

// --------------------------------------------------------------------------------------------

template <typename T, bool = cetl::pf17::is_swappable_v<T>>
struct test_swap;
template <typename T>
struct test_swap<T, true>
{
    static void test_noexcept()
    {
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        struct U : T
        {
            explicit U(const std::int64_t val)
                : value(val)
            {
            }
            std::int64_t value = 0;
        };
        variant<U, std::int64_t> v1(in_place_type<U>, 123456);
        variant<U, std::int64_t> v2(in_place_type<U>, 987654);
        // For the case where both variants have the same active alternative we provide no exception safety
        // guarantee because these concerns are delegated to the ADL-selected swap implementation,
        // which we don't care about.
        EXPECT_EQ(123456, get<U>(v1).value);
        EXPECT_EQ(987654, get<U>(v2).value);
        v1.swap(v2);
        EXPECT_EQ(987654, get<U>(v1).value);
        EXPECT_EQ(123456, get<U>(v2).value);

        v1.template emplace<1>(147852);
        EXPECT_EQ(147852, get<1>(v1));
        EXPECT_EQ(123456, get<U>(v2).value);
        v2.swap(v1);
        EXPECT_EQ(123456, get<U>(v1).value);
        EXPECT_EQ(147852, get<1>(v2));
    }

    static void test_throwing()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::in_place_type;
        using cetl::pf17::in_place_index;
        using cetl::pf17::get;
        struct U : T
        {
            U()                  = default;
            U(const U&) noexcept = default;
            U(U&& other)                        // NOLINT(*-noexcept-move-constructor)
                : T(std::move(other))           // Note that this may get resolved to the copy ctor.
                , move_throw(other.move_throw)  // NOLINT(*-use-after-move)
            {
                if (move_throw)
                {
                    throw std::exception();
                }
            }
            U& operator=(const U&) noexcept = default;
            U& operator=(U&& other)  // NOLINT(*-noexcept-move-constructor)
            {
                if (move_throw || other.move_throw)
                {
                    throw std::exception();
                }
                T::operator=(std::move(other));
                move_throw = other.move_throw;  // NOLINT(*-use-after-move)
                return *this;
            }
            bool move_throw = false;
        };
        struct W : T
        {
            W()
            {
                throw std::exception();
            }
        };
        variant<U, std::int64_t, W> v1(in_place_type<U>);
        variant<U, std::int64_t, W> v2(in_place_index<1>);

        // Swap two distinct types.
        EXPECT_EQ(0, v1.index());
        EXPECT_EQ(1, v2.index());
        v1.swap(v2);
        EXPECT_EQ(1, v1.index());
        EXPECT_EQ(0, v2.index());

        // Induce valueless state in one of the operands.
        get<U>(v2).move_throw = true;
        EXPECT_ANY_THROW(v1.swap(v2));
        EXPECT_TRUE(v1.valueless_by_exception());
        EXPECT_FALSE(v2.valueless_by_exception());

        // Swap a normal with a valueless type.
        get<U>(v2).move_throw = false;
        v1.swap(v2);
        EXPECT_FALSE(v1.valueless_by_exception());
        EXPECT_TRUE(v2.valueless_by_exception());

        // Swap two valueless.
        EXPECT_ANY_THROW(v1.template emplace<W>());
        EXPECT_TRUE(v1.valueless_by_exception());
        EXPECT_TRUE(v2.valueless_by_exception());
        v1.swap(v2);
        EXPECT_TRUE(v1.valueless_by_exception());
        EXPECT_TRUE(v2.valueless_by_exception());
#endif
    }

    static void test()
    {
        test_noexcept();
        test_throwing();
    }
};
template <typename T>
struct test_swap<T, false>
{
    static_assert(!cetl::pf17::is_swappable_v<T>, "");
    static_assert(!cetl::pf17::is_swappable_v<cetl::pf17::variant<T>>, "");
    static void test() {}
};

TYPED_TEST(test_smf_policy_combinations, swap)
{
    test_swap<TypeParam>::test();
}
