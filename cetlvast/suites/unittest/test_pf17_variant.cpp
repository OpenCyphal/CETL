/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/variant.hpp>
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

namespace test_variant_size
{
using cetl::pf17::variant;
using cetl::pf17::variant_size;
static_assert(variant_size<variant<int>>::value == 1, "");
static_assert(variant_size<const variant<double>>::value == 1, "");
static_assert(variant_size<variant<int, char, double>>::value == 3, "");
static_assert(variant_size<const variant<int, char, double>>::value == 3, "");
}  // namespace test_variant_size

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

/// This test checks common behaviors that are independent of the SMF policies.
TYPED_TEST(test_smf_policy_combinations, basics)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::visit;
    using cetl::pf17::holds_alternative;
    using cetl::pf17::get;
    using cetl::pf17::get_if;
    using cetl::pf17::make_overloaded;
    using cetl::pf17::in_place_index;

    // Enrich the variant with SMF-trivial types to ensure we always pick the most restrictive policy.
    using T = TypeParam;
    using V = variant<int, T, monostate>;

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
