/// @file
/// Unit tests for cetl/pf17/any.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/cetlpf.hpp>

#include <gtest/gtest.h>

namespace
{

using cetl::bad_any_cast;

/// TESTS -----------------------------------------------------------------------------------------------------------

TEST(test_bad_any_cast, ctor)
{
    // Test the default constructor.
    bad_any_cast test_exception1;

    // Test the copy constructor.
    bad_any_cast test_exception2{test_exception1};

    // Test the move constructor.
    bad_any_cast test_exception3{std::move(test_exception2)};
    EXPECT_STREQ("bad any cast", test_exception3.what());
}

TEST(test_bad_any_cast, assignment)
{
    // Test the copy assignment operator.
    bad_any_cast test_exception1;
    bad_any_cast test_exception2;
    test_exception2 = test_exception1;

    // Test the move assignment operator.
    bad_any_cast test_exception3;
    test_exception3 = std::move(test_exception2);
    EXPECT_STREQ("bad any cast", test_exception3.what());
}

}  // namespace
