/// @file
/// Unit tests for cetl/any.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/any.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <complex>
#include <string>
#include <gtest/gtest.h>

namespace
{

using cetl::any;
using cetl::any_cast;
using cetl::bad_any_cast;
using cetl::in_place_type_t;

/// TESTS -----------------------------------------------------------------------------------------------------------

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

TEST(test_any, ctor_2_copy)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        const uut src{42};
        const uut dst{src};

        EXPECT_EQ(42, *any_cast<int>(&src));
        EXPECT_EQ(42, *any_cast<int>(&dst));
    }

    // Copyable
    {
        struct TestCopyable
        {
            int value_     = 0;
            TestCopyable() = default;
            TestCopyable(const TestCopyable& other)
            {
                value_ = other.value_ + 1;
            }
        };
        using uut = any<sizeof(TestCopyable)>;

        const uut src{TestCopyable{}};
        const uut dst{src};

        EXPECT_EQ(1, *any_cast<int>(&src));
        EXPECT_EQ(2, *any_cast<int>(&dst));
    }
}

TEST(test_any, ctor_5)
{
    struct TestType
    {
        char ch_;
        int  number_;
        TestType(char ch, int number)
        {
            ch_     = ch;
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    const uut src{in_place_type_t<TestType>{}, 'Y', 42};

    auto ptr = any_cast<TestType>(&src);
    EXPECT_EQ('Y', ptr->ch_);
    EXPECT_EQ(42, ptr->number_);
}

TEST(test_any, make_any_1_cppref_example)
{
    using uut = any<std::max(sizeof(std::string), sizeof(std::complex<double>))>;

    auto a0 = cetl::make_any<std::string, uut>("Hello, cetl::any!\n");
    auto a1 = cetl::make_any<std::complex<double>, uut>(0.1, 2.3);

    EXPECT_STREQ("Hello, cetl::any!\n", cetl::any_cast<std::string>(&a0)->c_str());
    EXPECT_EQ(std::complex<double>(0.1, 2.3), *cetl::any_cast<std::complex<double>>(&a1));

    // TODO: Add more from the example when corresponding api will be available.
    // https://en.cppreference.com/w/cpp/utility/any/make_any
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
