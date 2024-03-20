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
#include <functional>
#include <string>
#include <gtest/gtest.h>

namespace
{

using cetl::any;
using cetl::any_cast;
using cetl::in_place_type_t;

/// TESTS -----------------------------------------------------------------------------------------------------------

TEST(test_any, cppref_example)
{
    using uut = any<sizeof(int)>;

    uut        a{1};
    const auto ptr = any_cast<int>(&a);
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
            int value_ = 0;

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

TEST(test_any, ctor_3_move)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut       src{42};
        const uut dst{std::move(src)};

        EXPECT_FALSE(src.has_value());
        EXPECT_EQ(42, *any_cast<int>(&dst));
    }

    // Movable
    {
        struct TestMovable
        {
            int value_ = 0;

            TestMovable() = default;
            TestMovable(const TestMovable& other)
            {
                value_ = other.value_ + 10;
            }
            TestMovable(TestMovable&& other) noexcept
            {
                value_ = other.value_ + 1;
            }
            ~TestMovable() = default;
        };
        using uut = any<sizeof(TestMovable)>;

        uut src{TestMovable{}};
        EXPECT_TRUE(src.has_value());

        const uut dst{std::move(src)};
        EXPECT_TRUE(dst.has_value());
        EXPECT_FALSE(src.has_value());
        EXPECT_EQ(2, *any_cast<int>(&dst));
    }
}

TEST(test_any, ctor_4_move_value)
{
    struct TestMovable
    {
        int value_ = 0;

        TestMovable() = default;
        TestMovable(const TestMovable& other)
        {
            value_ = other.value_ + 10;
        }
        TestMovable(TestMovable&& other) noexcept
        {
            value_ = other.value_ + 1;
        }
        ~TestMovable() = default;
    };
    using uut = any<sizeof(TestMovable)>;

    TestMovable src{};
    const uut   dst{std::move(src)};
    EXPECT_TRUE(dst.has_value());
    EXPECT_EQ(1, *any_cast<int>(&dst));
}

TEST(test_any, ctor_5)
{
    struct TestType
    {
        char ch_;
        int  number_;

        TestType(const char ch, const int number)
        {
            ch_     = ch;
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    const uut src{in_place_type_t<TestType>{}, 'Y', 42};

    const auto ptr = any_cast<TestType>(&src);
    EXPECT_EQ('Y', ptr->ch_);
    EXPECT_EQ(42, ptr->number_);
}

TEST(test_any, assign_1_copy)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        const uut src{42};
        EXPECT_TRUE(src.has_value());

        uut dst{};
        EXPECT_FALSE(dst.has_value());

        dst = src;
        EXPECT_TRUE(src.has_value());
        EXPECT_TRUE(dst.has_value());
        EXPECT_EQ(42, *any_cast<int>(&dst));

        const uut src2{147};
        dst = src2;
        EXPECT_EQ(147, *any_cast<int>(&dst));

        const uut empty{};
        dst = empty;
        EXPECT_FALSE(dst.has_value());
    }
}

TEST(test_any, assign_2_move)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut src{42};
        EXPECT_TRUE(src.has_value());

        uut dst{};
        EXPECT_FALSE(dst.has_value());

        dst = std::move(src);
        EXPECT_TRUE(dst.has_value());
        EXPECT_FALSE(src.has_value());
        EXPECT_EQ(42, *any_cast<int>(&dst));

        dst = uut{147};
        EXPECT_EQ(147, *any_cast<int>(&dst));

        dst = uut{};
        EXPECT_FALSE(dst.has_value());
    }
}

TEST(test_any, assign_3_move_value)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut dst{};
        EXPECT_FALSE(dst.has_value());

        dst = 147;
        EXPECT_EQ(147, *any_cast<int>(&dst));
    }
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
    using lambda     = std::function<const char*(void)>;
    using any_lambda = any<sizeof(lambda)>;

    const any_lambda a2 = [] { return "Lambda #1.\n"; };
    EXPECT_TRUE(a2.has_value());
    // TODO: Uncomment when RTTI will be available.
    // auto function_ptr = any_cast<lambda>(&a2);
    // EXPECT_FALSE(function_ptr);

    auto a3 = cetl::make_any<lambda, any_lambda>([] { return "Lambda #2.\n"; });
    EXPECT_TRUE(a3.has_value());
    const auto function3_ptr = any_cast<lambda>(&a3);
    EXPECT_TRUE(function3_ptr);
    EXPECT_STREQ("Lambda #2.\n", (*function3_ptr)());
}

TEST(test_any, any_cast_4_const_ptr)
{
    using uut = const any<sizeof(int)>;

    uut a{147};

    auto int_ptr = any_cast<int>(&a);
    static_assert(std::is_same<const int*, decltype(int_ptr)>::value, "");

    EXPECT_TRUE(int_ptr);
    EXPECT_EQ(147, *int_ptr);

    EXPECT_FALSE((any_cast<char, uut>(nullptr)));
}

TEST(test_any, any_cast_5_non_const_ptr)
{
    using uut = any<sizeof(char)>;

    uut a{'Y'};

    auto char_ptr = any_cast<char>(&a);
    static_assert(std::is_same<char*, decltype(char_ptr)>::value, "");
    EXPECT_TRUE(char_ptr);
    EXPECT_EQ('Y', *char_ptr);

    EXPECT_FALSE((any_cast<char, uut>(nullptr)));
}

}  // namespace
