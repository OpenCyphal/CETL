/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT


#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_assignment_1 : public ::testing::Test
{};


TYPED_TEST_SUITE(test_optional_combinations_assignment_1, testing_types, );


using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations_assignment_1, assignment)
{
    optional<TypeParam> opt1(cetl::pf17::in_place);
    EXPECT_TRUE(opt1.has_value());
    opt1 = cetl::pf17::nullopt;
    EXPECT_FALSE(opt1.has_value());
}
