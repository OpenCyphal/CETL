/// @file
/// Unit tests for string_view.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include <cetl/pf17/string_view.hpp>
#include "cetlvast/helpers_gtest.hpp"

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include <array>
#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <cetl/cetl.hpp>

#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <string_view>
#endif

using testing::Gt;
using testing::Lt;
using testing::IsNull;

/// Used to compare the STL version of basic_string_view with the CETL version.
/// @tparam TagType If cetlvast::CETLTag then the CETL version is used, otherwise the STL version is used.
/// @tparam CharT The character type used in the string view.
template <typename TagType, typename CharT>
struct TestStringViewTypes
{
    using stl_flag   = TagType;
    using value_type = CharT;
};

#if (__cplusplus >= CETL_CPP_STANDARD_17)
using StringViewTypes = testing::Types<TestStringViewTypes<cetlvast::CETLTag, char>,
                                       TestStringViewTypes<cetlvast::STLTag, char>,
                                       TestStringViewTypes<cetlvast::CETLTag, wchar_t>,
                                       TestStringViewTypes<cetlvast::STLTag, wchar_t>>;
#else
using StringViewTypes =
    testing::Types<TestStringViewTypes<cetlvast::CETLTag, char>, TestStringViewTypes<cetlvast::CETLTag, wchar_t>>;
#endif

template <typename StringViewTestParameters>
class TestStringView : public ::testing::Test
{
public:
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    template <typename ValueType>
    using TestSubjectType =
        std::conditional_t<std::is_same<cetlvast::CETLTag, typename StringViewTestParameters::stl_flag>::value,
                           cetl::pf17::basic_string_view<ValueType>,
                           std::basic_string_view<ValueType>>;
#else
    static_assert(not std::is_same<cetlvast::STLTag, typename StringViewTestParameters::stl_flag>::value,
                  "Cannot use STL version of basic_string_view with C++14.");
    template <typename ValueType>
    using TestSubjectType = cetl::pf17::basic_string_view<ValueType>;
#endif

    template <typename CharType>
    static char toChar(typename std::enable_if<std::is_same<char, CharType>::value, const char>::type ch)
    {
        return ch;
    }

    template <typename CharType>
    static wchar_t toChar(typename std::enable_if<std::is_same<wchar_t, CharType>::value, const char>::type ch)
    {
        return ch;
    }

    template <typename CharType>
    static auto toStr(typename std::enable_if<std::is_same<char, CharType>::value, const char*>::type cstr)
        -> TestSubjectType<CharType>
    {
        return {cstr};
    }

    template <typename CharType>
    static auto toStr(typename std::enable_if<std::is_same<wchar_t, CharType>::value, const char*>::type cstr)
        -> TestSubjectType<CharType>
    {
        // This is wonky. The test is using static storage filled out by the last caller to this method. This is error
        // prone.
        static std::array<wchar_t, 256> wstr{};
        const auto                      result = std::mbstowcs(wstr.data(), cstr, wstr.size());
        assert(result == std::strlen(cstr));
        (void) result;
        return {wstr.data()};
    }

};  // TestStringView

TYPED_TEST_SUITE(TestStringView, StringViewTypes, );

/// TESTS -----------------------------------------------------------------------------------------------------------

TYPED_TEST(TestStringView, DefaultConstructor)
{
    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv;
    EXPECT_THAT(sv.data(), IsNull());
    EXPECT_THAT(sv.size(), 0u);
    EXPECT_TRUE(sv.empty());
}

TYPED_TEST(TestStringView, ConstructFromCString)
{
    const auto test_str = TestFixture::template toStr<typename TypeParam::value_type>("Hello, World!");

    const typename TypeParam::value_type* const                                          cstr = test_str.data();
    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv(cstr);
    EXPECT_THAT(sv.data(), cstr);
    EXPECT_THAT(sv.size(), test_str.size());
    EXPECT_THAT(sv, TestFixture::template toStr<typename TypeParam::value_type>("Hello, World!"));
}

TYPED_TEST(TestStringView, ConstructFromCStringWithLength)
{
    const auto test_str = TestFixture::template toStr<typename TypeParam::value_type>("Hello, World!");

    const typename TypeParam::value_type*                                                cstr = test_str.data();
    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv(cstr, 5);
    EXPECT_THAT(sv.size(), 5u);
    EXPECT_THAT(sv, TestFixture::template toStr<typename TypeParam::value_type>("Hello"));
}

TYPED_TEST(TestStringView, ConstructFromStdString)
{
    const auto test_str = TestFixture::template toStr<typename TypeParam::value_type>("Hello, World!");
    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv(test_str);
    EXPECT_THAT(sv.data(), test_str.data());
    EXPECT_THAT(sv.size(), test_str.size());
    EXPECT_THAT(sv, test_str);
}

TYPED_TEST(TestStringView, SizeAndLength)
{
    using string_view   = typename TestFixture::template TestSubjectType<typename TypeParam::value_type>;
    const auto test_str = TestFixture::template toStr<typename TypeParam::value_type>("Test string");

    const string_view sv(test_str);
    EXPECT_THAT(sv.size(), 11u);
    EXPECT_THAT(sv.length(), 11u);
    EXPECT_THAT(sv.max_size(),
                (string_view::npos - sizeof(typename string_view::size_type) - sizeof(void*)) /
                    sizeof(typename string_view::value_type) / 4);
}

TYPED_TEST(TestStringView, Empty)
{
    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv1;
    EXPECT_TRUE(sv1.empty());

    const auto test_str2 = TestFixture::template toStr<typename TypeParam::value_type>("");
    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv2(test_str2);
    EXPECT_TRUE(sv2.empty());

    const auto test_str3 = TestFixture::template toStr<typename TypeParam::value_type>("Non-empty");
    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv3(test_str3);
    EXPECT_FALSE(sv3.empty());
}

TYPED_TEST(TestStringView, ElementAccessOperator)
{
    const auto test_str = TestFixture::template toStr<typename TypeParam::value_type>("abcdef");

    const typename TestFixture::template TestSubjectType<typename TypeParam::value_type> sv(test_str);
    EXPECT_THAT(sv[0], TestFixture::template toChar<typename TypeParam::value_type>('a'));
    EXPECT_THAT(sv[1], TestFixture::template toChar<typename TypeParam::value_type>('b'));
    EXPECT_THAT(sv[5], TestFixture::template toChar<typename TypeParam::value_type>('f'));
}

// TYPED_TEST(TestStringView, ElementAccessAt)
// {
//     const auto test_str = TestFixture::toStr("abcdef");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     EXPECT_THAT(sv.at(0), TestFixture::toChar('a'));
//     EXPECT_THAT(sv.at(5), TestFixture::toChar('f'));
// #if defined(__cpp_exceptions)
//     EXPECT_THROW(sv.at(6), std::out_of_range);
// #endif
// }

// TYPED_TEST(TestStringView, FrontBack)
// {
//     const auto test_str = TestFixture::toStr("abcdef");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     EXPECT_THAT(sv.front(), TestFixture::toChar('a'));
//     EXPECT_THAT(sv.back(), TestFixture::toChar('f'));
// }

// TYPED_TEST(TestStringView, Data)
// {
//     const auto test_str = TestFixture::toStr("Hello, World!");

//     const typename TypeParam::value_type* const             cstr = test_str.c_str();
//     const basic_string_view<typename TypeParam::value_type> sv(cstr);
//     EXPECT_THAT(sv.data(), cstr);
// }

// TYPED_TEST(TestStringView, Iterators)
// {
//     const auto test_str = TestFixture::toStr("abcdef");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     std::basic_string<typename TypeParam::value_type>       str;
//     for (auto it = sv.begin(); it != sv.end(); ++it)  // NOLINT
//     {
//         str += *it;
//     }
//     EXPECT_THAT(str, TestFixture::toStr("abcdef"));
// }

// TYPED_TEST(TestStringView, ConstIterators)
// {
//     const auto test_str = TestFixture::toStr("abcdef");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     std::basic_string<typename TypeParam::value_type>       str;
//     for (auto it = sv.cbegin(); it != sv.cend(); ++it)  // NOLINT
//     {
//         str += *it;
//     }
//     EXPECT_THAT(str, TestFixture::toStr("abcdef"));
// }

// TYPED_TEST(TestStringView, RemovePrefix)
// {
//     const auto test_str = TestFixture::toStr("abcdef");

//     basic_string_view<typename TypeParam::value_type> sv(test_str);
//     sv.remove_prefix(2);
//     EXPECT_THAT(sv, TestFixture::toStr("cdef"));
// }

// TYPED_TEST(TestStringView, RemoveSuffix)
// {
//     const auto test_str = TestFixture::toStr("abcdef");

//     basic_string_view<typename TypeParam::value_type> sv(test_str);
//     sv.remove_suffix(2);
//     EXPECT_THAT(sv, TestFixture::toStr("abcd"));
// }

// TYPED_TEST(TestStringView, Swap)
// {
//     const auto test_str1 = TestFixture::toStr("Hello");
//     const auto test_str2 = TestFixture::toStr("World");

//     basic_string_view<typename TypeParam::value_type> sv1(test_str1);
//     basic_string_view<typename TypeParam::value_type> sv2(test_str2);
//     sv1.swap(sv2);
//     EXPECT_THAT(sv1, TestFixture::toStr("World"));
//     EXPECT_THAT(sv2, TestFixture::toStr("Hello"));
// }

// TYPED_TEST(TestStringView, SwapNonMember)
// {
//     const auto test_str1 = TestFixture::toStr("Hello");
//     const auto test_str2 = TestFixture::toStr("World");

//     basic_string_view<typename TypeParam::value_type> sv1(test_str1);
//     basic_string_view<typename TypeParam::value_type> sv2(test_str2);
//     swap(sv1, sv2);
//     EXPECT_THAT(sv1, TestFixture::toStr("World"));
//     EXPECT_THAT(sv2, TestFixture::toStr("Hello"));
// }

// TYPED_TEST(TestStringView, Copy)
// {
//     const auto test_str = TestFixture::toStr("Hello, World!");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     typename TypeParam::value_type                          buffer[20] = {};
//     const size_t                       copied     = sv.copy(buffer, 5);
//     EXPECT_THAT(copied, 5u);
//     EXPECT_THAT(std::basic_string<typename TypeParam::value_type>(buffer, copied), TestFixture::toStr("Hello"));
// }

// TYPED_TEST(TestStringView, Substr)
// {
//     const auto test_str = TestFixture::toStr("Hello, World!");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     const basic_string_view<typename TypeParam::value_type> sub = sv.substr(7, 5);
//     EXPECT_THAT(sub, TestFixture::toStr("World"));
// #if defined(__cpp_exceptions)
//     EXPECT_THROW(sv.substr(20), std::out_of_range);
// #endif
// }

// TYPED_TEST(TestStringView, Compare)
// {
//     const auto test_str1 = TestFixture::toStr("abc");
//     const auto test_str2 = TestFixture::toStr("abc");
//     const auto test_str3 = TestFixture::toStr("abd");
//     const auto test_str4 = TestFixture::toStr("abcd");

//     const basic_string_view<typename TypeParam::value_type> sv1(test_str1);
//     const basic_string_view<typename TypeParam::value_type> sv2(test_str2);
//     const basic_string_view<typename TypeParam::value_type> sv3(test_str3);
//     const basic_string_view<typename TypeParam::value_type> sv4(test_str4);

//     EXPECT_THAT(sv1.compare(sv2), 0);
//     EXPECT_THAT(sv1.compare(sv3), Lt(0));
//     EXPECT_THAT(sv3.compare(sv1), Gt(0));
//     EXPECT_THAT(sv1.compare(sv4), Lt(0));
//     EXPECT_THAT(sv4.compare(sv1), Gt(0));
// }

// TYPED_TEST(TestStringView, FindChar)
// {
//     const auto test_str = TestFixture::toStr("Hello, World!");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     EXPECT_THAT(sv.find(TestFixture::toChar('W')), 7u);
//     EXPECT_THAT(sv.find(TestFixture::toChar('z')), basic_string_view<typename TypeParam::value_type>::npos);
// }

// TYPED_TEST(TestStringView, FindStringView)
// {
//     const auto test_str1 = TestFixture::toStr("Hello, World!");
//     const auto test_str2 = TestFixture::toStr("World");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str1);
//     const basic_string_view<typename TypeParam::value_type> to_find(test_str2);
//     EXPECT_THAT(sv.find(to_find), 7u);
//     EXPECT_THAT(sv.find(TestFixture::toStr("")), 0u);
//     EXPECT_THAT(sv.find(TestFixture::toStr("Earth")), basic_string_view<typename TypeParam::value_type>::npos);
//     EXPECT_THAT(sv.find(TestFixture::toStr("too long too long too long")), basic_string_view<typename
//     TypeParam::value_type>::npos);
// }

// TYPED_TEST(TestStringView, RelationalOperators)
// {
//     const auto test_str1 = TestFixture::toStr("abc");
//     const auto test_str2 = TestFixture::toStr("abc");
//     const auto test_str3 = TestFixture::toStr("abd");

//     const basic_string_view<typename TypeParam::value_type> sv1(test_str1);
//     const basic_string_view<typename TypeParam::value_type> sv2(test_str2);
//     const basic_string_view<typename TypeParam::value_type> sv3(test_str3);

//     EXPECT_TRUE(sv1 == sv2);
//     EXPECT_FALSE(sv1 != sv2);
//     EXPECT_TRUE(sv1 < sv3);
//     EXPECT_TRUE(sv3 > sv1);
//     EXPECT_TRUE(sv1 <= sv2);
//     EXPECT_TRUE(sv1 >= sv2);
// }

// TYPED_TEST(TestStringView, FindOutOfBounds)
// {
//     const auto test_str = TestFixture::toStr("Hello");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     EXPECT_THAT(sv.find(TestFixture::toChar('H'), 10), basic_string_view<typename TypeParam::value_type>::npos);
//     EXPECT_THAT(sv.find(TestFixture::toStr("He"), 10), basic_string_view<typename TypeParam::value_type>::npos);
// }

// TYPED_TEST(TestStringView, RemovePrefixOutOfBounds)
// {
//     const auto test_str = TestFixture::toStr("Hello");

//     basic_string_view<typename TypeParam::value_type> sv(test_str);
//     sv.remove_prefix(10);
//     EXPECT_THAT(sv.size(), 0u);
// }

// TYPED_TEST(TestStringView, RemoveSuffixOutOfBounds)
// {
//     const auto test_str = TestFixture::toStr("Hello");

//     basic_string_view<typename TypeParam::value_type> sv(test_str);
//     sv.remove_suffix(10);
//     EXPECT_THAT(sv.size(), 0u);
// }

// TYPED_TEST(TestStringView, SubstrWithNpos)
// {
//     const auto test_str = TestFixture::toStr("Hello, World!");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     const basic_string_view<typename TypeParam::value_type> sub = sv.substr(7);
//     EXPECT_THAT(sub, TestFixture::toStr("World!"));
// }

// TYPED_TEST(TestStringView, EmptyStringViewOperations)
// {
//     const basic_string_view<typename TypeParam::value_type> sv;
//     EXPECT_THAT(sv.size(), 0u);
//     EXPECT_TRUE(sv.empty());
//     EXPECT_THAT(sv.data(), IsNull());
//     EXPECT_THAT(sv.begin(), sv.end());
// }

// TYPED_TEST(TestStringView, ComparisonWithString)
// {
//     const auto test_str = TestFixture::toStr("Hello");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     EXPECT_THAT(sv, test_str);
//     EXPECT_THAT(test_str, sv);
// }

// TYPED_TEST(TestStringView, ComparisonWithCString)
// {
//     const auto test_str = TestFixture::toStr("Hello");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     EXPECT_THAT(sv, test_str.c_str());
//     EXPECT_THAT(test_str.c_str(), sv);
// }

// TYPED_TEST(TestStringView, FindPartial)
// {
//     const auto test_str = TestFixture::toStr("ababab");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     EXPECT_THAT(sv.find(TestFixture::toStr("aba")), 0u);
//     EXPECT_THAT(sv.find(TestFixture::toStr("aba"), 1), 2u);
// }

// TYPED_TEST(TestStringView, CopyOutOfBounds)
// {
// #if defined(__cpp_exceptions)

//     const auto test_str = TestFixture::toStr("Hello");

//     const basic_string_view<typename TypeParam::value_type> sv(test_str);
//     typename TypeParam::value_type                          buffer[10];
//     EXPECT_THROW(sv.copy(buffer, 5, 6), std::out_of_range);

// #else
//     GTEST_SKIP() << "Not applicable when exceptions are disabled.";
// #endif
// }

// TYPED_TEST(TestStringView, stream_operator)
// {
//     using SV = basic_string_view<typename TypeParam::value_type>;

//     const auto test_str = TestFixture::toStr("Test");
//     {
//         std::basic_stringstream<typename TypeParam::value_type> ss;
//         ss << SV{test_str} << SV{test_str};
//         EXPECT_THAT(ss.str(), TestFixture::toStr("TestTest"));
//     }
//     {
//         std::basic_stringstream<typename TypeParam::value_type> ss;
//         ss << std::setw(9) << std::setfill(TestFixture::toChar('-')) << std::left << SV{test_str};
//         EXPECT_THAT(ss.str(), TestFixture::toStr("Test-----"));
//     }
//     {
//         std::basic_stringstream<typename TypeParam::value_type> ss;
//         ss << std::setw(9) << std::setfill(TestFixture::toChar('-')) << std::right << SV{test_str};
//         EXPECT_THAT(ss.str(), TestFixture::toStr("-----Test"));
//     }
//     {
//         std::basic_stringstream<typename TypeParam::value_type> ss;
//         ss << std::setw(2) << SV{test_str};
//         EXPECT_THAT(ss.str(), test_str);
//     }
// }
