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

        const uut srcAny{42};
        const uut dstAny{srcAny};

        EXPECT_EQ(42, *any_cast<int>(&srcAny));
        EXPECT_EQ(42, *any_cast<int>(&dstAny));
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

        const uut srcAny{TestCopyable{}};
        const uut dstAny{srcAny};

        EXPECT_EQ(1, *any_cast<int>(&srcAny));
        EXPECT_EQ(2, *any_cast<int>(&dstAny));
    }
}

TEST(test_any, ctor_3_move)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut       srcAny{42};
        const uut dstAny{std::move(srcAny)};

        EXPECT_FALSE(srcAny.has_value());
        EXPECT_EQ(42, *any_cast<int>(&dstAny));
    }

    // Movable
    {
        struct TestMovable
        {
            int value_     = 0;
            TestMovable()  = default;
            ~TestMovable() = default;
            TestMovable(const TestMovable& other)
            {
                value_ = other.value_ + 10;
            }
            TestMovable(TestMovable&& other) noexcept
            {
                value_ = other.value_ + 1;
            }
        };
        using uut = any<sizeof(TestMovable)>;

        uut srcAny{TestMovable{}};
        EXPECT_TRUE(srcAny.has_value());

        const uut dstAny{std::move(srcAny)};
        EXPECT_TRUE(dstAny.has_value());
        EXPECT_FALSE(srcAny.has_value());
        EXPECT_EQ(2, *any_cast<int>(&dstAny));
    }
}

TEST(test_any, ctor_4_move_value)
{
    struct TestMovable
    {
        int value_     = 0;
        TestMovable()  = default;
        ~TestMovable() = default;
        TestMovable(const TestMovable& other)
        {
            value_ = other.value_ + 10;
        }
        TestMovable(TestMovable&& other) noexcept
        {
            value_ = other.value_ + 1;
        }
    };
    using uut = any<sizeof(TestMovable)>;

    TestMovable test{};
    const uut   dstAny{std::move(test)};
    EXPECT_TRUE(dstAny.has_value());
    EXPECT_EQ(1, *any_cast<int>(&dstAny));
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

    const uut srcAny{in_place_type_t<TestType>{}, 'Y', 42};

    auto ptr = any_cast<TestType>(&srcAny);
    EXPECT_EQ('Y', ptr->ch_);
    EXPECT_EQ(42, ptr->number_);
}

TEST(test_any, assign_1_copy)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        const uut srcAny{42};
        EXPECT_TRUE(srcAny.has_value());

        uut dstAny{};
        EXPECT_FALSE(dstAny.has_value());

        dstAny = srcAny;
        EXPECT_TRUE(srcAny.has_value());
        EXPECT_TRUE(dstAny.has_value());
        EXPECT_EQ(42, *any_cast<int>(&dstAny));

        const uut srcAny2{147};
        dstAny = srcAny2;
        EXPECT_EQ(147, *any_cast<int>(&dstAny));

        const uut emptyAny{};
        dstAny = emptyAny;
        EXPECT_FALSE(dstAny.has_value());
    }
}

TEST(test_any, assign_2_move)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut srcAny{42};
        EXPECT_TRUE(srcAny.has_value());

        uut dstAny{};
        EXPECT_FALSE(dstAny.has_value());

        dstAny = std::move(srcAny);
        EXPECT_TRUE(dstAny.has_value());
        EXPECT_FALSE(srcAny.has_value());
        EXPECT_EQ(42, *any_cast<int>(&dstAny));

        dstAny = uut{147};
        EXPECT_EQ(147, *any_cast<int>(&dstAny));

        dstAny = uut{};
        EXPECT_FALSE(dstAny.has_value());
    }
}

TEST(test_any, assign_3_move_value)
{
    // Primitive `int`
    {
        using uut = any<sizeof(int)>;

        uut srcAny{42};
        EXPECT_TRUE(srcAny.has_value());

        uut dstAny{};
        EXPECT_FALSE(dstAny.has_value());

        dstAny = 147;
        EXPECT_EQ(147, *any_cast<int>(&dstAny));
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

    any_lambda a2 = [] { return "Lambda #1.\n"; };
    EXPECT_TRUE(a2.has_value());
    // TODO: Uncomment when RTTI will be available.
    // auto functionPtr = any_cast<lambda>(&a2);
    // EXPECT_FALSE(functionPtr);

    auto a3 = cetl::make_any<lambda, any_lambda>([] { return "Lambda #2.\n"; });
    EXPECT_TRUE(a3.has_value());
    auto functionPtr3 = any_cast<lambda>(&a3);
    EXPECT_TRUE(functionPtr3);
    EXPECT_STREQ("Lambda #2.\n", (*functionPtr3)());
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

}  // namespace
