/// @file
/// Unit tests for cetl/any.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/any.hpp>

#include <gtest/gtest.h>

namespace
{

using cetl::any;
using cetl::any_cast;
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
    EXPECT_STREQ("bad_any_cast", test_exception3.what());
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
    EXPECT_STREQ("bad_any_cast", test_exception3.what());
}

/// ------------------------------------------------------------------------------------------------

TEST(test_any, cppref_example)
{
    using uut = any<sizeof(int)>;

    uut  a{1};
    auto ptr = any_cast<int>(&a);
    EXPECT_TRUE(ptr);
    EXPECT_EQ(1, *ptr);

    // TODO: Add more from the example when corresponding api will be available.
    // https://en.cppreference.com/w/cpp/utility/any
}

TEST(test_any, ctor_1_default)
{
    EXPECT_FALSE((any<0>{}.has_value()));
    EXPECT_FALSE((any<0, false>{}.has_value()));
    EXPECT_FALSE((any<0, false, true>{}.has_value()));
    EXPECT_FALSE((any<0, true, false>{}.has_value()));

    EXPECT_FALSE((any<1>{}.has_value()));
    EXPECT_FALSE((any<1, false>{}.has_value()));
    EXPECT_FALSE((any<1, false, true>{}.has_value()));
    EXPECT_FALSE((any<1, true, false>{}.has_value()));

    EXPECT_FALSE((any<13>{}.has_value()));
    EXPECT_FALSE((any<13, false>{}.has_value()));
    EXPECT_FALSE((any<13, false, true>{}.has_value()));
    EXPECT_FALSE((any<13, true, false>{}.has_value()));
}

TEST(test_any, any_cast_4_const_ptr)
{
    using uut = const any<sizeof(int)>;

    uut a{147};

    auto intPtr = any_cast<int>(&a);
    static_assert(std::is_same_v<const int*, decltype(intPtr)>);

    EXPECT_TRUE(intPtr);
    EXPECT_EQ(147, *intPtr);

    EXPECT_FALSE((any_cast<char, uut>(nullptr)));
}

TEST(test_any, any_cast_5_non_const_ptr)
{
    using uut = any<sizeof(char)>;

    uut a{'Y'};

    auto charPtr = any_cast<char>(&a);
    static_assert(std::is_same_v<char*, decltype(charPtr)>);
    EXPECT_TRUE(charPtr);
    EXPECT_EQ('Y', *charPtr);

    EXPECT_FALSE((any_cast<char, uut>(nullptr)));
}

TEST(test_any, function_value)
{
    // TODO: Try put/get function.
    GTEST_SKIP() << "Implement me!";
}

TEST(test_any, lambda_value)
{
    // TODO: Try put/get lambda.
    GTEST_SKIP() << "Implement me!";
}

}  // namespace
