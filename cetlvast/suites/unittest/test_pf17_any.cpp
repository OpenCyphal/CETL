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

/// TESTS -----------------------------------------------------------------------------------------------------------

TEST(test_bad_any_cast, ctor)
{
#if defined(__cpp_exceptions)

    // Test the default constructor.
    cetl::bad_any_cast test_exception1;

    // Test the copy constructor.
    cetl::bad_any_cast test_exception2{test_exception1};

    // Test the move constructor.
    cetl::bad_any_cast test_exception3{std::move(test_exception2)};
    EXPECT_STREQ("bad any cast", test_exception3.what());

#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}

TEST(test_bad_any_cast, assignment)
{
#if defined(__cpp_exceptions)

    // Test the copy assignment operator.
    cetl::bad_any_cast test_exception1;
    cetl::bad_any_cast test_exception2;
    test_exception2 = test_exception1;

    // Test the move assignment operator.
    cetl::bad_any_cast test_exception3;
    test_exception3 = std::move(test_exception2);
    EXPECT_STREQ("bad any cast", test_exception3.what());

#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}

}  // namespace
