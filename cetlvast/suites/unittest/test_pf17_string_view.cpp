/// @file
/// Unit tests for string_view.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include <cetlvast/helpers_gtest.hpp>

#include <cetl/cetl.hpp>
#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pf17/string_view.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>

#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <string_view>
#endif

using testing::Gt;
using testing::Lt;
using testing::IsNull;

template <typename StringViewT, typename CharT>
struct TestSpec
{
    using sv_type   = StringViewT;
    using char_type = CharT;
};
//
#if (__cplusplus >= CETL_CPP_STANDARD_17)
using TestTypes = testing::Types<  //
    TestSpec<std::string_view, char>,
    TestSpec<std::basic_string_view<wchar_t>, wchar_t>,
    TestSpec<cetl::pf17::string_view, char>,
    TestSpec<cetl::pf17::basic_string_view<wchar_t>, wchar_t>>;
#else
using TestTypes = testing::Types<  //
    TestSpec<cetl::string_view, char>,
    TestSpec<cetl::basic_string_view<wchar_t>, wchar_t>>;
#endif

template <typename TestSpec>
class TestStringView;
//
template <typename StringViewT>
class TestStringView<TestSpec<StringViewT, char>> : public testing::Test
{
public:
    static char toChar(const char ch)
    {
        return ch;
    }
    static std::basic_string<char> toStr(const char* const cstr)
    {
        return {cstr};
    }
};
//
template <typename StringViewT>
class TestStringView<TestSpec<StringViewT, wchar_t>> : public testing::Test
{
public:
    static wchar_t toChar(const char ch)
    {
        return ch;
    }
    static std::basic_string<wchar_t> toStr(const char* const cstr)
    {
        std::array<wchar_t, 256> wstr{};
        const auto               result = std::mbstowcs(wstr.data(), cstr, wstr.size());
        assert(result == std::strlen(cstr));
        (void) result;
        return {wstr.data()};
    }

};  // TestStringView

TYPED_TEST_SUITE(TestStringView, TestTypes, );

/// TESTS -----------------------------------------------------------------------------------------------------------

TYPED_TEST(TestStringView, DefaultConstructor)
{
    using basic_string_view = typename TypeParam::sv_type;

    const basic_string_view sv;
    EXPECT_THAT(sv.data(), IsNull());
    EXPECT_THAT(sv.size(), 0u);
    EXPECT_TRUE(sv.empty());
}

TYPED_TEST(TestStringView, ConstructFromCString)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const char_type* const  cstr = test_str.c_str();
    const basic_string_view sv{cstr};
    EXPECT_THAT(sv.data(), cstr);
    EXPECT_THAT(sv.size(), test_str.size());
    EXPECT_THAT(sv, TestFixture::toStr("Hello, World!"));
}

TYPED_TEST(TestStringView, ConstructFromCStringWithLength)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const char_type*        cstr = test_str.c_str();
    const basic_string_view sv{cstr, 5};
    EXPECT_THAT(sv.size(), 5u);
    EXPECT_THAT(sv, TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, ConstructFromStdString)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv.data(), test_str.data());
    EXPECT_THAT(sv.size(), test_str.size());
    EXPECT_THAT(sv, test_str);
}

TYPED_TEST(TestStringView, SizeAndLength)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Test string");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv.size(), 11u);
    EXPECT_THAT(sv.length(), 11u);
    EXPECT_THAT(sv.max_size(),
                (basic_string_view::npos - sizeof(typename basic_string_view::size_type) - sizeof(void*)) /
                    sizeof(char_type) / 4);
}

TYPED_TEST(TestStringView, Empty)
{
    using basic_string_view = typename TypeParam::sv_type;

    const basic_string_view sv1;
    EXPECT_TRUE(sv1.empty());

    const auto              test_str2 = TestFixture::toStr("");
    const basic_string_view sv2{test_str2};
    EXPECT_TRUE(sv2.empty());

    const auto              test_str3 = TestFixture::toStr("Non-empty");
    const basic_string_view sv3{test_str3};
    EXPECT_FALSE(sv3.empty());
}

TYPED_TEST(TestStringView, ElementAccessOperator)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv[0], TestFixture::toChar('a'));
    EXPECT_THAT(sv[1], TestFixture::toChar('b'));
    EXPECT_THAT(sv[5], TestFixture::toChar('f'));
}

TYPED_TEST(TestStringView, ElementAccessAt)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv.at(0), TestFixture::toChar('a'));
    EXPECT_THAT(sv.at(5), TestFixture::toChar('f'));
#if defined(__cpp_exceptions)
    EXPECT_THROW((void) sv.at(6), std::out_of_range);
#endif
}

TYPED_TEST(TestStringView, FrontBack)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv.front(), TestFixture::toChar('a'));
    EXPECT_THAT(sv.back(), TestFixture::toChar('f'));
}

TYPED_TEST(TestStringView, Data)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const char_type* const  cstr = test_str.c_str();
    const basic_string_view sv{cstr};
    EXPECT_THAT(sv.data(), cstr);
}

TYPED_TEST(TestStringView, Iterators)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view      sv{test_str};
    std::basic_string<char_type> str;
    for (auto it = sv.begin(); it != sv.end(); ++it)  // NOLINT
    {
        str += *it;
    }
    EXPECT_THAT(str, TestFixture::toStr("abcdef"));
}

TYPED_TEST(TestStringView, ConstIterators)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view      sv{test_str};
    std::basic_string<char_type> str;
    for (auto it = sv.cbegin(); it != sv.cend(); ++it)  // NOLINT
    {
        str += *it;
    }
    EXPECT_THAT(str, TestFixture::toStr("abcdef"));
}

TYPED_TEST(TestStringView, RemovePrefix)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("abcdef");

    basic_string_view sv{test_str};
    sv.remove_prefix(2);
    EXPECT_THAT(sv, TestFixture::toStr("cdef"));
}

TYPED_TEST(TestStringView, RemoveSuffix)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("abcdef");

    basic_string_view sv{test_str};
    sv.remove_suffix(2);
    EXPECT_THAT(sv, TestFixture::toStr("abcd"));
}

TYPED_TEST(TestStringView, Swap)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str1 = TestFixture::toStr("Hello");
    const auto test_str2 = TestFixture::toStr("World");

    basic_string_view sv1{test_str1};
    basic_string_view sv2{test_str2};
    sv1.swap(sv2);
    EXPECT_THAT(sv1, TestFixture::toStr("World"));
    EXPECT_THAT(sv2, TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, SwapNonMember)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str1 = TestFixture::toStr("Hello");
    const auto test_str2 = TestFixture::toStr("World");

    basic_string_view sv1{test_str1};
    basic_string_view sv2{test_str2};
    swap(sv1, sv2);
    EXPECT_THAT(sv1, TestFixture::toStr("World"));
    EXPECT_THAT(sv2, TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, Copy)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view sv{test_str};
    char_type               buffer[20] = {};
    const size_t            copied     = sv.copy(buffer, 5);
    EXPECT_THAT(copied, 5u);
    EXPECT_THAT(std::basic_string<char_type>(buffer, copied), TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, Substr)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view sv{test_str};
    const basic_string_view sub = sv.substr(7, 5);
    EXPECT_THAT(sub, TestFixture::toStr("World"));
#if defined(__cpp_exceptions)
    EXPECT_THROW((void) sv.substr(20), std::out_of_range);
#endif
}

TYPED_TEST(TestStringView, Compare)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str1 = TestFixture::toStr("abc");
    const auto test_str2 = TestFixture::toStr("abc");
    const auto test_str3 = TestFixture::toStr("abd");
    const auto test_str4 = TestFixture::toStr("abcd");

    const basic_string_view sv1{test_str1};
    const basic_string_view sv2{test_str2};
    const basic_string_view sv3{test_str3};
    const basic_string_view sv4{test_str4};

    EXPECT_THAT(sv1.compare(sv2), 0);
    EXPECT_THAT(sv1.compare(sv3), Lt(0));
    EXPECT_THAT(sv3.compare(sv1), Gt(0));
    EXPECT_THAT(sv1.compare(sv4), Lt(0));
    EXPECT_THAT(sv4.compare(sv1), Gt(0));
}

TYPED_TEST(TestStringView, FindChar)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv.find(TestFixture::toChar('W')), 7u);
    EXPECT_THAT(sv.find(TestFixture::toChar('z')), basic_string_view::npos);
}

TYPED_TEST(TestStringView, FindStringView)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str1 = TestFixture::toStr("Hello, World!");
    const auto test_str2 = TestFixture::toStr("World");

    const basic_string_view sv{test_str1};
    const basic_string_view to_find{test_str2};
    EXPECT_THAT(sv.find(to_find), 7u);
    EXPECT_THAT(sv.find(TestFixture::toStr("")), 0u);
    EXPECT_THAT(sv.find(TestFixture::toStr("Earth")), basic_string_view::npos);
    EXPECT_THAT(sv.find(TestFixture::toStr("too long too long too long")), basic_string_view::npos);
}

TYPED_TEST(TestStringView, RelationalOperators)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str1 = TestFixture::toStr("abc");
    const auto test_str2 = TestFixture::toStr("abc");
    const auto test_str3 = TestFixture::toStr("abd");

    const basic_string_view sv1{test_str1};
    const basic_string_view sv2{test_str2};
    const basic_string_view sv3{test_str3};

    EXPECT_TRUE(sv1 == sv2);
    EXPECT_FALSE(sv1 != sv2);
    EXPECT_TRUE(sv1 < sv3);
    EXPECT_TRUE(sv3 > sv1);
    EXPECT_TRUE(sv1 <= sv2);
    EXPECT_TRUE(sv1 >= sv2);
}

TYPED_TEST(TestStringView, FindOutOfBounds)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv.find(TestFixture::toChar('H'), 10), basic_string_view::npos);
    EXPECT_THAT(sv.find(TestFixture::toStr("He"), 10), basic_string_view::npos);
}

TYPED_TEST(TestStringView, SubstrWithNpos)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view sv{test_str};
    const basic_string_view sub = sv.substr(7);
    EXPECT_THAT(sub, TestFixture::toStr("World!"));
}

TYPED_TEST(TestStringView, EmptyStringViewOperations)
{
    using basic_string_view = typename TypeParam::sv_type;

    const basic_string_view sv;
    EXPECT_THAT(sv.size(), 0u);
    EXPECT_TRUE(sv.empty());
    EXPECT_THAT(sv.data(), IsNull());
    EXPECT_THAT(sv.begin(), sv.end());
}

TYPED_TEST(TestStringView, ComparisonWithString)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv, test_str);
    EXPECT_THAT(test_str, sv);
}

TYPED_TEST(TestStringView, ComparisonWithCString)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv, test_str.c_str());
    EXPECT_THAT(test_str.c_str(), sv);
}

TYPED_TEST(TestStringView, FindPartial)
{
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("ababab");

    const basic_string_view sv{test_str};
    EXPECT_THAT(sv.find(TestFixture::toStr("aba")), 0u);
    EXPECT_THAT(sv.find(TestFixture::toStr("aba"), 1), 2u);
}

TYPED_TEST(TestStringView, CopyOutOfBounds)
{
#if defined(__cpp_exceptions)

    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view sv{test_str};
    char_type               buffer[10];
    EXPECT_THROW(sv.copy(buffer, 5, 6), std::out_of_range);

#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}

TYPED_TEST(TestStringView, stream_operator)
{
    using char_type         = typename TypeParam::char_type;
    using basic_string_view = typename TypeParam::sv_type;

    const auto test_str = TestFixture::toStr("Test");
    {
        std::basic_stringstream<char_type> ss;
        ss << basic_string_view{test_str} << basic_string_view{test_str};
        EXPECT_THAT(ss.str(), TestFixture::toStr("TestTest"));
    }
    {
        std::basic_stringstream<char_type> ss;
        ss << std::setw(9) << std::setfill(TestFixture::toChar('-')) << std::left << basic_string_view{test_str};
        EXPECT_THAT(ss.str(), TestFixture::toStr("Test-----"));
    }
    {
        std::basic_stringstream<char_type> ss;
        ss << std::setw(9) << std::setfill(TestFixture::toChar('-')) << std::right << basic_string_view{test_str};
        EXPECT_THAT(ss.str(), TestFixture::toStr("-----Test"));
    }
    {
        std::basic_stringstream<char_type> ss;
        ss << std::setw(2) << basic_string_view{test_str};
        EXPECT_THAT(ss.str(), test_str);
    }
}
