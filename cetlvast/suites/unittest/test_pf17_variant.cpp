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

// Import relevant entities from the cetl namespace.
using cetl::pf17::variant;
using cetl::pf17::variant_alternative_t;
using cetl::pf17::monostate;
using cetl::pf17::detail::var::types;
using cetl::pf17::detail::var::smf_availability::smf_deleted;
using cetl::pf17::detail::var::smf_availability::smf_trivial;
using cetl::pf17::detail::var::smf_availability::smf_nontrivial;

// Import relevant entities from the cetlvast namespace.
namespace smfp = cetlvast::smf_policies;

// Test the `types` typelist helper.
static_assert(types<smfp::copy_ctor_policy<smfp::policy_deleted>>::avail_copy_ctor == smf_deleted, "");
static_assert(types<smfp::copy_ctor_policy<smfp::policy_trivial>>::avail_copy_ctor == smf_trivial, "");
static_assert(types<smfp::copy_ctor_policy<smfp::policy_nontrivial>>::avail_copy_ctor == smf_nontrivial, "");

static_assert(types<smfp::move_ctor_policy<smfp::policy_deleted>>::avail_move_ctor == smf_deleted, "");
static_assert(types<smfp::move_ctor_policy<smfp::policy_trivial>>::avail_move_ctor == smf_trivial, "");
static_assert(types<smfp::move_ctor_policy<smfp::policy_nontrivial>>::avail_move_ctor == smf_nontrivial, "");

static_assert(types<smfp::copy_assignment_policy<smfp::policy_deleted>>::avail_copy_assign == smf_deleted, "");
static_assert(types<smfp::copy_assignment_policy<smfp::policy_trivial>>::avail_copy_assign == smf_trivial, "");
static_assert(types<smfp::copy_assignment_policy<smfp::policy_nontrivial>>::avail_copy_assign == smf_nontrivial, "");

static_assert(types<smfp::move_assignment_policy<smfp::policy_deleted>>::avail_move_assign == smf_deleted, "");
static_assert(types<smfp::move_assignment_policy<smfp::policy_trivial>>::avail_move_assign == smf_trivial, "");
static_assert(types<smfp::move_assignment_policy<smfp::policy_nontrivial>>::avail_move_assign == smf_nontrivial, "");

static_assert(types<smfp::dtor_policy<smfp::policy_deleted>>::avail_dtor == smf_deleted, "");
static_assert(types<smfp::dtor_policy<smfp::policy_trivial>>::avail_dtor == smf_trivial, "");
static_assert(types<smfp::dtor_policy<smfp::policy_nontrivial>>::avail_dtor == smf_nontrivial, "");

// Test variant_alternative_t.
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
