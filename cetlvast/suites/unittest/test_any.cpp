/// @file
/// Unit tests for cetl/any.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/any.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <array>
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

TEST(test_any, ctor_5_in_place)
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

TEST(test_any, ctor_6_in_place_initializer_list)
{
    struct TestType
    {
        std::size_t size_;
        int         number_;

        TestType(std::initializer_list<char> const chars, int const number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    const uut src{in_place_type_t<TestType>{}, {'A', 'B', 'C'}, 42};

    const auto ptr = any_cast<TestType>(&src);
    EXPECT_EQ(3, ptr->size_);
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

TEST(test_any, make_any_cppref_example)
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
    // auto function2_ptr = any_cast<lambda>(&a2);
    // EXPECT_FALSE(function2_ptr);

    auto a3 = cetl::make_any<lambda, any_lambda>([] { return "Lambda #2.\n"; });
    EXPECT_TRUE(a3.has_value());
    const auto function3_ptr = any_cast<lambda>(&a3);
    EXPECT_TRUE(function3_ptr);
    EXPECT_STREQ("Lambda #2.\n", (*function3_ptr)());
}

TEST(test_any, make_any_1)
{
    using uut = const any<sizeof(int)>;

    const auto test = cetl::make_any<int, uut>(42);
    EXPECT_EQ(42, *any_cast<int>(&test));
}

TEST(test_any, make_any_2_ilist)
{
    struct TestType
    {
        std::size_t size_;
        int         number_;

        TestType(std::initializer_list<char> const chars, int const number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    const auto src      = cetl::make_any<TestType, uut>({'A', 'C'}, 42);
    const auto test_ptr = any_cast<TestType>(&src);
    EXPECT_EQ(2, test_ptr->size_);
    EXPECT_EQ(42, test_ptr->number_);
}

TEST(test_any, any_cast_cppref_example)
{
    using uut = any<std::max(sizeof(int), sizeof(std::string))>;

    auto a1 = uut{12};
    EXPECT_EQ(12, any_cast<int>(a1));

#if defined(__cpp_exceptions)

    try
    {
        (void) any_cast<std::string>(uut{});  // throws!
        FAIL() << "Should have thrown an exception.";

    } catch (const cetl::bad_any_cast& e)
    {
        SUCCEED() << "`bad_any_cast` has been caught.";

    } catch (...)
    {
        FAIL() << "Should have thrown `bad_any_cast`.";
    }

#endif

    // Advanced example
    a1       = std::string("hello");
    auto& ra = any_cast<std::string&>(a1);  //< reference
    ra[1]    = 'o';
    EXPECT_STREQ("hollo", any_cast<const std::string&>(a1).c_str());  //< const reference

    auto s1 = any_cast<std::string&&>(std::move(a1));  //< rvalue reference
    // Note: “s1” is a move-constructed std::string, “a1” is empty
    static_assert(std::is_same<decltype(s1), std::string>::value, "");
    EXPECT_STREQ("hollo", s1.c_str());
}

TEST(test_any, any_cast_1_const)
{
    using uut = any<sizeof(int)>;

#if defined(__cpp_exceptions)
    try
    {
        const uut empty{};
        (void) any_cast<int>(empty);  // throws!
        FAIL() << "Should have thrown an exception.";

    } catch (const cetl::bad_any_cast&)
    {
        SUCCEED() << "`bad_any_cast` has been caught.";

    } catch (...)
    {
        FAIL() << "Should have thrown `bad_any_cast`.";
    }
#endif

    const uut src{42};
    EXPECT_EQ(42, any_cast<int>(src));
    // EXPECT_EQ(42, any_cast<int&>(src)); //< won't compile expectedly
    EXPECT_EQ(42, any_cast<const int>(src));
    EXPECT_EQ(42, any_cast<const int&>(src));
}

TEST(test_any, any_cast_2_non_const)
{
    using uut = any<sizeof(int)>;

#if defined(__cpp_exceptions)

    try
    {
        uut empty{};
        (void) any_cast<int>(empty);  // throws!
        FAIL() << "Should have thrown an exception.";

    } catch (const cetl::bad_any_cast&)
    {
        SUCCEED() << "`bad_any_cast` has been caught.";

    } catch (...)
    {
        FAIL() << "Should have thrown `bad_any_cast`.";
    }

#endif

    uut src{42};
    EXPECT_EQ(42, any_cast<int>(src));
    EXPECT_EQ(42, any_cast<int&>(src));
    EXPECT_EQ(42, any_cast<const int>(src));
    EXPECT_EQ(42, any_cast<const int&>(src));
}

TEST(test_any, any_cast_3_move_primitive_int)
{
    using uut = any<sizeof(int)>;

    uut src{147};
    EXPECT_EQ(147, any_cast<int>(std::move(src)));
    EXPECT_TRUE(src.has_value());  //< expectedly still contains the value - moved from.

    EXPECT_EQ(42, any_cast<int>(uut{42}));
    // EXPECT_EQ(42, any_cast<int&>(uut{42})); //< won't compile expectedly
    EXPECT_EQ(42, any_cast<const int>(uut{42}));
    EXPECT_EQ(42, any_cast<const int&>(uut{42}));
}

TEST(test_any, any_cast_3_move_empty_bad_cast)
{
#if defined(__cpp_exceptions)

    using uut = any<sizeof(int)>;

    try
    {
        (void) any_cast<int>(uut{});  // throws!
        FAIL() << "Should have thrown an exception.";

    } catch (const cetl::bad_any_cast&)
    {
        SUCCEED() << "`bad_any_cast` has been caught.";

    } catch (...)
    {
        FAIL() << "Should have thrown `bad_any_cast`.";
    }

#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}

TEST(test_any, any_cast_4_const_ptr)
{
    using uut = const any<sizeof(int)>;

    uut src{147};

    auto int_ptr = any_cast<int>(&src);
    static_assert(std::is_same<const int*, decltype(int_ptr)>::value, "");

    EXPECT_TRUE(int_ptr);
    EXPECT_EQ(147, *int_ptr);

    EXPECT_FALSE((any_cast<char, uut>(nullptr)));
}

TEST(test_any, any_cast_5_non_const_ptr)
{
    using uut = any<sizeof(char)>;

    uut src{'Y'};

    auto char_ptr = any_cast<char>(&src);
    static_assert(std::is_same<char*, decltype(char_ptr)>::value, "");
    EXPECT_TRUE(char_ptr);
    EXPECT_EQ('Y', *char_ptr);

    EXPECT_FALSE((any_cast<char, uut>(nullptr)));
}

TEST(test_any, swap)
{
    using uut = any<sizeof(char)>;

    uut empty{};
    uut a{'A'};
    uut b{'B'};

    // Self swap
    a.swap(a);
    EXPECT_EQ('A', *any_cast<char>(&a));

    a.swap(b);
    EXPECT_EQ('B', *any_cast<char>(&a));
    EXPECT_EQ('A', *any_cast<char>(&b));

    empty.swap(a);
    EXPECT_FALSE(a.has_value());
    EXPECT_EQ('B', *any_cast<char>(&empty));

    empty.swap(a);
    EXPECT_FALSE(empty.has_value());
    EXPECT_EQ('B', *any_cast<char>(&a));
}

TEST(test_any, emplace_1)
{
    // Primitive `char`
    {
        using uut = any<sizeof(char)>;

        uut src;
        src.emplace<char>('Y');
        EXPECT_EQ('Y', *any_cast<char>(&src));
    }

    // `TestType` with two params ctor.
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

        uut t;
        t.emplace<TestType>('Y', 147);
        EXPECT_EQ('Y', any_cast<TestType>(&t)->ch_);
        EXPECT_EQ(147, any_cast<TestType>(&t)->number_);
    }
}

TEST(test_any, emplace_2_initializer_list)
{
    struct TestType
    {
        std::size_t size_;
        int         number_;

        TestType(std::initializer_list<char> const chars, int const number)
        {
            size_   = chars.size();
            number_ = number;
        }
    };
    using uut = any<sizeof(TestType)>;

    uut src;
    src.emplace<TestType>({'A', 'B', 'C'}, 42);

    const auto test_ptr = any_cast<TestType>(&src);
    EXPECT_EQ(3, test_ptr->size_);
    EXPECT_EQ(42, test_ptr->number_);
}

}  // namespace
