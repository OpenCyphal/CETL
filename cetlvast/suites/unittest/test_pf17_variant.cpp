/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/variant.hpp>
#include <cetlvast/helpers.hpp>
#include <cetlvast/typelist.hpp>
#include <cetlvast/smf_policies.hpp>
#include <gtest/gtest.h>

namespace test_detail_types
{
using cetlvast::smf_policies::copy_ctor_policy;
using cetlvast::smf_policies::move_ctor_policy;
using cetlvast::smf_policies::copy_assignment_policy;
using cetlvast::smf_policies::move_assignment_policy;
using cetlvast::smf_policies::dtor_policy;
using cetlvast::smf_policies::policy_deleted;
using cetlvast::smf_policies::policy_trivial;
using cetlvast::smf_policies::policy_nontrivial;
using cetl::pf17::detail::var::types;
using cetl::pf17::detail::var::smf_availability::smf_deleted;
using cetl::pf17::detail::var::smf_availability::smf_trivial;
using cetl::pf17::detail::var::smf_availability::smf_nontrivial;

static_assert(types<copy_ctor_policy<policy_deleted>>::avail_copy_ctor == smf_deleted, "");
static_assert(types<copy_ctor_policy<policy_trivial>>::avail_copy_ctor == smf_trivial, "");
static_assert(types<copy_ctor_policy<policy_nontrivial>>::avail_copy_ctor == smf_nontrivial, "");

static_assert(types<move_ctor_policy<policy_deleted>>::avail_move_ctor == smf_deleted, "");
static_assert(types<move_ctor_policy<policy_trivial>>::avail_move_ctor == smf_trivial, "");
static_assert(types<move_ctor_policy<policy_nontrivial>>::avail_move_ctor == smf_nontrivial, "");

static_assert(types<copy_assignment_policy<policy_deleted>>::avail_copy_assign == smf_deleted, "");
static_assert(types<copy_assignment_policy<policy_trivial>>::avail_copy_assign == smf_trivial, "");
static_assert(types<copy_assignment_policy<policy_nontrivial>>::avail_copy_assign == smf_nontrivial, "");

static_assert(types<move_assignment_policy<policy_deleted>>::avail_move_assign == smf_deleted, "");
static_assert(types<move_assignment_policy<policy_trivial>>::avail_move_assign == smf_trivial, "");
static_assert(types<move_assignment_policy<policy_nontrivial>>::avail_move_assign == smf_nontrivial, "");

static_assert(types<dtor_policy<policy_deleted>>::avail_dtor == smf_deleted, "");
static_assert(types<dtor_policy<policy_trivial>>::avail_dtor == smf_trivial, "");
static_assert(types<dtor_policy<policy_nontrivial>>::avail_dtor == smf_nontrivial, "");
}  // namespace test_detail_types

// --------------------------------------------------------------------------------------------

namespace test_variant_alternative
{
using cetl::pf17::variant;
using cetl::pf17::variant_alternative_t;
using cetl::pf17::monostate;

static_assert(std::is_same<int, variant_alternative_t<0, variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<char, variant_alternative_t<1, variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<monostate, variant_alternative_t<2, variant<int, char, monostate>>>::value, "");

static_assert(std::is_same<const int, variant_alternative_t<0, const variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<const char, variant_alternative_t<1, const variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<const monostate, variant_alternative_t<2, const variant<int, char, monostate>>>::value, "");

static_assert(std::is_same<int*, variant_alternative_t<0, variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<char*, variant_alternative_t<1, variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<monostate*, variant_alternative_t<2, variant<int*, char*, monostate*>>>::value, "");

static_assert(std::is_same<int* const, variant_alternative_t<0, const variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<char* const, variant_alternative_t<1, const variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<monostate* const, variant_alternative_t<2, const variant<int*, char*, monostate*>>>::value,
              "");
}  // namespace test_variant_alternative

// --------------------------------------------------------------------------------------------

namespace test_variant_size
{
using cetl::pf17::variant;
using cetl::pf17::variant_size_v;
static_assert(variant_size_v<variant<int>> == 1, "");
static_assert(variant_size_v<const variant<double>> == 1, "");
static_assert(variant_size_v<variant<int, char, double>> == 3, "");
static_assert(variant_size_v<const variant<int, char, double>> == 3, "");

// Well, this is also size...
using cetl::pf17::monostate;
static_assert(sizeof(variant<char>) == (2 * sizeof(std::size_t)), "");  // mind the index field alignment
static_assert(sizeof(variant<std::size_t, char>) == (2 * sizeof(std::size_t)), "");
static_assert(sizeof(variant<std::size_t, monostate>) == (2 * sizeof(std::size_t)), "");
static_assert(sizeof(variant<std::size_t, monostate, std::int64_t>) == (sizeof(std::int64_t) + sizeof(std::size_t)),
              "");
}  // namespace test_variant_size

// --------------------------------------------------------------------------------------------

namespace test_smf_availability_basics
{
using cetl::pf17::variant;
using cetl::pf17::monostate;

struct restricted
{
    restricted()                             = default;
    restricted(const restricted&)            = delete;
    restricted(restricted&&)                 = delete;
    restricted& operator=(const restricted&) = delete;
    restricted& operator=(restricted&&)      = delete;
    ~restricted()
    {
        std::terminate(); /* a nontrivial dtor */
    }
};

static_assert(std::is_trivially_copy_constructible<variant<bool>>::value, "");
static_assert(std::is_trivially_move_constructible<variant<bool>>::value, "");
static_assert(std::is_trivially_copy_assignable<variant<bool>>::value, "");
static_assert(std::is_trivially_move_assignable<variant<bool>>::value, "");
static_assert(std::is_trivially_destructible<variant<bool>>::value, "");
static_assert(std::is_trivially_copyable<variant<bool>>::value, "");

static_assert(std::is_trivially_copy_constructible<variant<monostate>>::value, "");
static_assert(std::is_trivially_move_constructible<variant<monostate>>::value, "");
static_assert(std::is_trivially_copy_assignable<variant<monostate>>::value, "");
static_assert(std::is_trivially_move_assignable<variant<monostate>>::value, "");
static_assert(std::is_trivially_destructible<variant<monostate>>::value, "");
static_assert(std::is_trivially_copyable<variant<monostate>>::value, "");

static_assert(!std::is_trivially_copy_constructible<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_move_constructible<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_copy_assignable<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_move_assignable<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_destructible<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_copyable<variant<monostate, restricted>>::value, "");
}  // namespace test_smf_availability_basics

// --------------------------------------------------------------------------------------------

TEST(test_variant, chronomorphize)
{
    using namespace cetl::pf17::detail::var;
    struct checker final
    {
        // clang-format off
        auto operator()(const std::integral_constant<std::size_t, 0> ix) { return check(decltype(ix)::value); }
        auto operator()(const std::integral_constant<std::size_t, 1> ix) { return check(decltype(ix)::value); }
        auto operator()(const std::integral_constant<std::size_t, 2> ix) { return check(decltype(ix)::value); }
        // clang-format on
        std::size_t check(const std::size_t value)
        {
            if ((!armed) || (value != expected_value))
            {
                std::terminate();
            }
            armed = false;
            return value;
        }
        std::size_t expected_value = 0;
        bool        armed          = false;
    };
    {
        checker chk{0, true};
        EXPECT_EQ(0, chronomorphize<3>(chk, 0));
        EXPECT_FALSE(chk.armed);
    }
    {
        checker chk{1, true};
        EXPECT_EQ(1, chronomorphize<3>(chk, 1));
        EXPECT_FALSE(chk.armed);
    }
    {
        checker chk{2, true};
        EXPECT_EQ(2, chronomorphize<3>(chk, 2));
        EXPECT_FALSE(chk.armed);
    }
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, monostate)
{
    using cetl::pf17::monostate;
    EXPECT_TRUE(monostate{} == monostate{});
    EXPECT_FALSE(monostate{} != monostate{});
    EXPECT_FALSE(monostate{} < monostate{});
    EXPECT_FALSE(monostate{} > monostate{});
    EXPECT_TRUE(monostate{} <= monostate{});
    EXPECT_TRUE(monostate{} >= monostate{});
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, basic_operations)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::visit;
    using cetl::pf17::holds_alternative;
    using cetl::pf17::get;
    using cetl::pf17::get_if;
    using cetl::pf17::make_overloaded;
    using cetl::pf17::in_place_index;

    variant<int, char, monostate> var;
    EXPECT_EQ(0, var.index());
    EXPECT_FALSE(var.valueless_by_exception());
    EXPECT_TRUE(holds_alternative<int>(var));
    EXPECT_FALSE(holds_alternative<char>(var));

    EXPECT_FALSE(get_if<char>(&var));
    EXPECT_FALSE(get_if<char>(static_cast<variant<int, char, monostate>*>(nullptr)));
    EXPECT_FALSE(get_if<char>(static_cast<const variant<int, char, monostate>*>(nullptr)));
    EXPECT_TRUE(get_if<int>(&var));
    *get_if<int>(&var) = 42;
    EXPECT_EQ(42, get<int>(var));
    EXPECT_EQ(42, *get_if<int>(&var));

    const auto& const_var = var;
    EXPECT_EQ(42, *get_if<int>(&const_var));
    EXPECT_EQ(42, get<int>(const_var));

    EXPECT_EQ(43,
              cetl::pf17::visit(make_overloaded([](const int arg) { return arg + 1; },
                                                [](const char arg) { return static_cast<int>(arg) + 2; },
                                                [](const monostate) {
                                                    std::terminate();
                                                    return '\0';
                                                }),
                                var));
    EXPECT_EQ(42 + 'a',
              cetl::pf17::visit(make_overloaded([](int, double) { return 0; },
                                                [](int a, char b) { return a + b; },
                                                [](char, double) { return 0; },
                                                [](char, char) { return 0; },
                                                [](monostate, double) { return 0; },
                                                [](monostate, char) { return 0; }),
                                var,
                                variant<double, char>{in_place_index<1>, 'a'}));
}

// --------------------------------------------------------------------------------------------

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
    struct T : SMF
    {
        explicit T(const std::int64_t val)
            : value(val)
        {
        }
        std::int64_t value = 0;
    };
    static void test()
    {
        using cetl::pf17::variant;
        using cetl::pf17::in_place_type;
        using cetl::pf17::get;
        using cetl::pf17::monostate;
        using cetlvast::smf_policies::policy_nontrivial;
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
    static void test()
    {
        using cetl::pf17::variant;
        using cetl::pf17::in_place_type;
        using cetl::pf17::get;
        using cetl::pf17::monostate;
        using cetlvast::smf_policies::policy_nontrivial;
        using cetlvast::smf_policies::policy_deleted;
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
    // TODO FIXME NOT IMPLEMENTED
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
            // beacuse we throw the exception from the copy ctor after the base is already fully constructed;
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
