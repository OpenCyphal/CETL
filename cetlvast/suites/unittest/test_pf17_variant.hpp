/// @file
/// Shared components for the unit tests for cetl/pf17/variant.hpp.
/// We can't keep all tests in a single translation unit because the CI server runs out of memory.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
// CSpell: words borked

#ifndef CETLVAST_SUITES_UNITTEST_TEST_PF17_VARIANT_HPP_INCLUDED
#define CETLVAST_SUITES_UNITTEST_TEST_PF17_VARIANT_HPP_INCLUDED

#include <cetlvast/typelist.hpp>
#include <cetlvast/smf_policies.hpp>
#include <gtest/gtest.h>

namespace cetlvast
{
namespace unittest
{
namespace pf17
{
namespace variant
{
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

}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast

#endif  // CETLVAST_SUITES_UNITTEST_TEST_PF17_VARIANT_HPP_INCLUDED
