/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

// +******************************************************************************************************************+
// * Common header for type-parameterized tests that are spread over several translation units to ensure no single
// * object will take longer than 1 minute (or so) to compile.
// +******************************************************************************************************************+

#ifndef CETLVAST_UNITTEST_OPTIONAL_TEST_PF17_OPTIONAL_COMBINATIONS_H_INCLUDED
#define CETLVAST_UNITTEST_OPTIONAL_TEST_PF17_OPTIONAL_COMBINATIONS_H_INCLUDED

#include "test_pf17_optional.hpp"

using namespace cetlvast::smf_policies;

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
using testing_types =
    cetlvast::typelist::into<::testing::Types>::from<cetlvast::typelist::map<combine_bases, policy_combinations>::type>;

/// TESTS -----------------------------------------------------------------------------------------------------------


static_assert(std::is_same<cetl::pf17::optional<bool>::value_type, bool>::value, "");
static_assert(std::is_same<cetl::pf17::optional<long>::value_type, long>::value, "");

static_assert(std::is_trivially_copy_constructible<cetl::pf17::optional<bool>>::value, "");
static_assert(std::is_trivially_move_constructible<cetl::pf17::optional<bool>>::value, "");
static_assert(std::is_trivially_copy_assignable<cetl::pf17::optional<bool>>::value, "");
static_assert(std::is_trivially_move_assignable<cetl::pf17::optional<bool>>::value, "");
static_assert(std::is_trivially_destructible<cetl::pf17::optional<bool>>::value, "");

#endif // CETLVAST_UNITTEST_OPTIONAL_TEST_PF17_OPTIONAL_COMBINATIONS_H_INCLUDED
