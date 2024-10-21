/// @file
/// Unit tests for string_view.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include <cetl/pf17/string_view.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <string>
#include <cetl/cetl.hpp>

using testing::Gt;
using testing::Lt;
using testing::IsNull;

using CharTs = testing::Types<char, wchar_t>;

template <typename CharT>
class TestStringView;
//
template <>
class TestStringView<char> : public testing::Test
{
public:
    static char toChar(const char ch)
    {
        return ch;
    }
    static std::basic_string<char> toStr(const char* cstr)
    {
        return {cstr};
    }
};
//
template <>
class TestStringView<wchar_t> : public testing::Test
{
public:
    static wchar_t toChar(const char ch)
    {
        return ch;
    }
    static std::basic_string<wchar_t> toStr(const char* cstr)
    {
        std::array<wchar_t, 256> wstr{};
        const auto result = std::mbstowcs(wstr.data(), cstr, wstr.size());
        assert(result == std::strlen(cstr));
        return {wstr.data()};
    }

};  // TestStringView

TYPED_TEST_SUITE(TestStringView, CharTs);

/// TESTS -----------------------------------------------------------------------------------------------------------

using cetl::pf17::basic_string_view;

TYPED_TEST(TestStringView, DefaultConstructor)
{
    const basic_string_view<TypeParam> sv;
    EXPECT_THAT(sv.data(), IsNull());
    EXPECT_THAT(sv.size(), 0u);
    EXPECT_TRUE(sv.empty());
}

TYPED_TEST(TestStringView, ConstructFromCString)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const TypeParam* const             cstr = test_str.c_str();
    const basic_string_view<TypeParam> sv(cstr);
    EXPECT_THAT(sv.data(), cstr);
    EXPECT_THAT(sv.size(), test_str.size());
    EXPECT_THAT(sv, TestFixture::toStr("Hello, World!"));
}

TYPED_TEST(TestStringView, ConstructFromCStringWithLength)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const TypeParam*                   cstr = test_str.c_str();
    const basic_string_view<TypeParam> sv(cstr, 5);
    EXPECT_THAT(sv.size(), 5u);
    EXPECT_THAT(sv, TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, ConstructFromStdString)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv.data(), test_str.data());
    EXPECT_THAT(sv.size(), test_str.size());
    EXPECT_THAT(sv, test_str);
}

TYPED_TEST(TestStringView, SizeAndLength)
{
    using string_view = basic_string_view<TypeParam>;

    const auto test_str = TestFixture::toStr("Test string");

    const string_view sv(test_str);
    EXPECT_THAT(sv.size(), 11u);
    EXPECT_THAT(sv.length(), 11u);
    EXPECT_THAT(sv.max_size(),
                (string_view::npos - sizeof(typename string_view::size_type) - sizeof(void*)) /
                    sizeof(typename string_view::value_type) / 4);
}

TYPED_TEST(TestStringView, Empty)
{
    const basic_string_view<TypeParam> sv1;
    EXPECT_TRUE(sv1.empty());

    const auto                         test_str2 = TestFixture::toStr("");
    const basic_string_view<TypeParam> sv2(test_str2);
    EXPECT_TRUE(sv2.empty());

    const auto                         test_str3 = TestFixture::toStr("Non-empty");
    const basic_string_view<TypeParam> sv3(test_str3);
    EXPECT_FALSE(sv3.empty());
}

TYPED_TEST(TestStringView, ElementAccessOperator)
{
    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv[0], TestFixture::toChar('a'));
    EXPECT_THAT(sv[1], TestFixture::toChar('b'));
    EXPECT_THAT(sv[5], TestFixture::toChar('f'));
}

TYPED_TEST(TestStringView, ElementAccessAt)
{
    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv.at(0), TestFixture::toChar('a'));
    EXPECT_THAT(sv.at(5), TestFixture::toChar('f'));
#if defined(__cpp_exceptions)
    EXPECT_THROW(sv.at(6), std::out_of_range);
#endif
}

TYPED_TEST(TestStringView, FrontBack)
{
    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv.front(), TestFixture::toChar('a'));
    EXPECT_THAT(sv.back(), TestFixture::toChar('f'));
}

TYPED_TEST(TestStringView, Data)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const TypeParam* const             cstr = test_str.c_str();
    const basic_string_view<TypeParam> sv(cstr);
    EXPECT_THAT(sv.data(), cstr);
}

TYPED_TEST(TestStringView, Iterators)
{
    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view<TypeParam> sv(test_str);
    std::basic_string<TypeParam>       str;
    for (auto it = sv.begin(); it != sv.end(); ++it)  // NOLINT
    {
        str += *it;
    }
    EXPECT_THAT(str, TestFixture::toStr("abcdef"));
}

TYPED_TEST(TestStringView, ConstIterators)
{
    const auto test_str = TestFixture::toStr("abcdef");

    const basic_string_view<TypeParam> sv(test_str);
    std::basic_string<TypeParam>       str;
    for (auto it = sv.cbegin(); it != sv.cend(); ++it)  // NOLINT
    {
        str += *it;
    }
    EXPECT_THAT(str, TestFixture::toStr("abcdef"));
}

TYPED_TEST(TestStringView, RemovePrefix)
{
    const auto test_str = TestFixture::toStr("abcdef");

    basic_string_view<TypeParam> sv(test_str);
    sv.remove_prefix(2);
    EXPECT_THAT(sv, TestFixture::toStr("cdef"));
}

TYPED_TEST(TestStringView, RemoveSuffix)
{
    const auto test_str = TestFixture::toStr("abcdef");

    basic_string_view<TypeParam> sv(test_str);
    sv.remove_suffix(2);
    EXPECT_THAT(sv, TestFixture::toStr("abcd"));
}

TYPED_TEST(TestStringView, Swap)
{
    const auto test_str1 = TestFixture::toStr("Hello");
    const auto test_str2 = TestFixture::toStr("World");

    basic_string_view<TypeParam> sv1(test_str1);
    basic_string_view<TypeParam> sv2(test_str2);
    sv1.swap(sv2);
    EXPECT_THAT(sv1, TestFixture::toStr("World"));
    EXPECT_THAT(sv2, TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, SwapNonMember)
{
    const auto test_str1 = TestFixture::toStr("Hello");
    const auto test_str2 = TestFixture::toStr("World");

    basic_string_view<TypeParam> sv1(test_str1);
    basic_string_view<TypeParam> sv2(test_str2);
    swap(sv1, sv2);
    EXPECT_THAT(sv1, TestFixture::toStr("World"));
    EXPECT_THAT(sv2, TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, Copy)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view<TypeParam> sv(test_str);
    TypeParam                          buffer[20] = {};
    const size_t                       copied     = sv.copy(buffer, 5);
    EXPECT_THAT(copied, 5u);
    EXPECT_THAT(std::basic_string<TypeParam>(buffer, copied), TestFixture::toStr("Hello"));
}

TYPED_TEST(TestStringView, Substr)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view<TypeParam> sv(test_str);
    const basic_string_view<TypeParam> sub = sv.substr(7, 5);
    EXPECT_THAT(sub, TestFixture::toStr("World"));
#if defined(__cpp_exceptions)
    EXPECT_THROW(sv.substr(20), std::out_of_range);
#endif
}

TYPED_TEST(TestStringView, Compare)
{
    const auto test_str1 = TestFixture::toStr("abc");
    const auto test_str2 = TestFixture::toStr("abc");
    const auto test_str3 = TestFixture::toStr("abd");
    const auto test_str4 = TestFixture::toStr("abcd");

    const basic_string_view<TypeParam> sv1(test_str1);
    const basic_string_view<TypeParam> sv2(test_str2);
    const basic_string_view<TypeParam> sv3(test_str3);
    const basic_string_view<TypeParam> sv4(test_str4);

    EXPECT_THAT(sv1.compare(sv2), 0);
    EXPECT_THAT(sv1.compare(sv3), Lt(0));
    EXPECT_THAT(sv3.compare(sv1), Gt(0));
    EXPECT_THAT(sv1.compare(sv4), Lt(0));
    EXPECT_THAT(sv4.compare(sv1), Gt(0));
}

TYPED_TEST(TestStringView, FindChar)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv.find(TestFixture::toChar('W')), 7u);
    EXPECT_THAT(sv.find(TestFixture::toChar('z')), basic_string_view<TypeParam>::npos);
}

TYPED_TEST(TestStringView, FindStringView)
{
    const auto test_str1 = TestFixture::toStr("Hello, World!");
    const auto test_str2 = TestFixture::toStr("World");

    const basic_string_view<TypeParam> sv(test_str1);
    const basic_string_view<TypeParam> to_find(test_str2);
    EXPECT_THAT(sv.find(to_find), 7u);
    EXPECT_THAT(sv.find(TestFixture::toStr("")), 0u);
    EXPECT_THAT(sv.find(TestFixture::toStr("Earth")), basic_string_view<TypeParam>::npos);
    EXPECT_THAT(sv.find(TestFixture::toStr("too long too long too long")), basic_string_view<TypeParam>::npos);
}

TYPED_TEST(TestStringView, RelationalOperators)
{
    const auto test_str1 = TestFixture::toStr("abc");
    const auto test_str2 = TestFixture::toStr("abc");
    const auto test_str3 = TestFixture::toStr("abd");

    const basic_string_view<TypeParam> sv1(test_str1);
    const basic_string_view<TypeParam> sv2(test_str2);
    const basic_string_view<TypeParam> sv3(test_str3);

    EXPECT_TRUE(sv1 == sv2);
    EXPECT_FALSE(sv1 != sv2);
    EXPECT_TRUE(sv1 < sv3);
    EXPECT_TRUE(sv3 > sv1);
    EXPECT_TRUE(sv1 <= sv2);
    EXPECT_TRUE(sv1 >= sv2);
}

TYPED_TEST(TestStringView, FindOutOfBounds)
{
    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv.find(TestFixture::toChar('H'), 10), basic_string_view<TypeParam>::npos);
    EXPECT_THAT(sv.find(TestFixture::toStr("He"), 10), basic_string_view<TypeParam>::npos);
}

TYPED_TEST(TestStringView, RemovePrefixOutOfBounds)
{
    const auto test_str = TestFixture::toStr("Hello");

    basic_string_view<TypeParam> sv(test_str);
    sv.remove_prefix(10);
    EXPECT_THAT(sv.size(), 0u);
}

TYPED_TEST(TestStringView, RemoveSuffixOutOfBounds)
{
    const auto test_str = TestFixture::toStr("Hello");

    basic_string_view<TypeParam> sv(test_str);
    sv.remove_suffix(10);
    EXPECT_THAT(sv.size(), 0u);
}

TYPED_TEST(TestStringView, SubstrWithNpos)
{
    const auto test_str = TestFixture::toStr("Hello, World!");

    const basic_string_view<TypeParam> sv(test_str);
    const basic_string_view<TypeParam> sub = sv.substr(7);
    EXPECT_THAT(sub, TestFixture::toStr("World!"));
}

TYPED_TEST(TestStringView, EmptyStringViewOperations)
{
    const basic_string_view<TypeParam> sv;
    EXPECT_THAT(sv.size(), 0u);
    EXPECT_TRUE(sv.empty());
    EXPECT_THAT(sv.data(), IsNull());
    EXPECT_THAT(sv.begin(), sv.end());
}

TYPED_TEST(TestStringView, ComparisonWithString)
{
    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv, test_str);
    EXPECT_THAT(test_str, sv);
}

TYPED_TEST(TestStringView, ComparisonWithCString)
{
    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv, test_str.c_str());
    EXPECT_THAT(test_str.c_str(), sv);
}

TYPED_TEST(TestStringView, FindPartial)
{
    const auto test_str = TestFixture::toStr("ababab");

    const basic_string_view<TypeParam> sv(test_str);
    EXPECT_THAT(sv.find(TestFixture::toStr("aba")), 0u);
    EXPECT_THAT(sv.find(TestFixture::toStr("aba"), 1), 2u);
}

TYPED_TEST(TestStringView, CopyOutOfBounds)
{
#if defined(__cpp_exceptions)

    const auto test_str = TestFixture::toStr("Hello");

    const basic_string_view<TypeParam> sv(test_str);
    TypeParam                          buffer[10];
    EXPECT_THROW(sv.copy(buffer, 5, 6), std::out_of_range);

#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}
