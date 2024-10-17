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

#include <string>

using testing::Gt;
using testing::Lt;
using testing::IsNull;

class TestStringView : public testing::Test
{
protected:
};

/// TESTS -----------------------------------------------------------------------------------------------------------

using cetl::pf17::string_view;

TEST_F(TestStringView, DefaultConstructor)
{
    const string_view sv;
    EXPECT_THAT(sv.data(), IsNull());
    EXPECT_THAT(sv.size(), 0u);
    EXPECT_TRUE(sv.empty());
}

TEST_F(TestStringView, ConstructFromCString)
{
    const char* const cstr = "Hello, World!";
    const string_view sv(cstr);
    EXPECT_THAT(sv.data(), cstr);
    EXPECT_THAT(sv.size(), std::strlen(cstr));
    EXPECT_THAT(sv, "Hello, World!");
}

TEST_F(TestStringView, ConstructFromCStringWithLength)
{
    const char*       cstr = "Hello, World!";
    const string_view sv(cstr, 5);
    EXPECT_THAT(sv.size(), 5u);
    EXPECT_THAT(sv, "Hello");
}

TEST_F(TestStringView, ConstructFromStdString)
{
    std::string       str = "Hello, World!";
    const string_view sv(str);
    EXPECT_THAT(sv.data(), str.data());
    EXPECT_THAT(sv.size(), str.size());
    EXPECT_THAT(sv, str);
}

TEST_F(TestStringView, SizeAndLength)
{
    const string_view sv("Test string");
    EXPECT_THAT(sv.size(), 11u);
    EXPECT_THAT(sv.length(), 11u);
    EXPECT_THAT(sv.max_size(),
                (string_view::npos - sizeof(string_view::size_type) - sizeof(void*)) / sizeof(string_view::value_type) /
                    4);
}

TEST_F(TestStringView, Empty)
{
    const string_view sv1;
    EXPECT_TRUE(sv1.empty());

    const string_view sv2("");
    EXPECT_TRUE(sv2.empty());

    const string_view sv3("Non-empty");
    EXPECT_FALSE(sv3.empty());
}

TEST_F(TestStringView, ElementAccessOperator)
{
    const string_view sv("abcdef");
    EXPECT_THAT(sv[0], 'a');
    EXPECT_THAT(sv[1], 'b');
    EXPECT_THAT(sv[5], 'f');
}

TEST_F(TestStringView, ElementAccessAt)
{
    const string_view sv("abcdef");
    EXPECT_THAT(sv.at(0), 'a');
    EXPECT_THAT(sv.at(5), 'f');
#if defined(__cpp_exceptions)
    EXPECT_THROW(sv.at(6), std::out_of_range);
#endif
}

TEST_F(TestStringView, FrontBack)
{
    const string_view sv("abcdef");
    EXPECT_THAT(sv.front(), 'a');
    EXPECT_THAT(sv.back(), 'f');
}

TEST_F(TestStringView, Data)
{
    const char* const cstr = "Hello, World!";
    const string_view sv(cstr);
    EXPECT_THAT(sv.data(), cstr);
}

TEST_F(TestStringView, Iterators)
{
    const string_view sv("abcdef");
    std::string       str;
    for (auto it = sv.begin(); it != sv.end(); ++it)  // NOLINT
    {
        str += *it;
    }
    EXPECT_THAT(str, "abcdef");
}

TEST_F(TestStringView, ConstIterators)
{
    const string_view sv("abcdef");
    std::string       str;
    for (auto it = sv.cbegin(); it != sv.cend(); ++it)  // NOLINT
    {
        str += *it;
    }
    EXPECT_THAT(str, "abcdef");
}

TEST_F(TestStringView, RemovePrefix)
{
    string_view sv("abcdef");
    sv.remove_prefix(2);
    EXPECT_THAT(sv, "cdef");
}

TEST_F(TestStringView, RemoveSuffix)
{
    string_view sv("abcdef");
    sv.remove_suffix(2);
    EXPECT_THAT(sv, "abcd");
}

TEST_F(TestStringView, Swap)
{
    string_view sv1("Hello");
    string_view sv2("World");
    sv1.swap(sv2);
    EXPECT_THAT(sv1, "World");
    EXPECT_THAT(sv2, "Hello");
}

TEST_F(TestStringView, SwapNonMember)
{
    string_view sv1("Hello");
    string_view sv2("World");
    swap(sv1, sv2);
    EXPECT_THAT(sv1, "World");
    EXPECT_THAT(sv2, "Hello");
}

TEST_F(TestStringView, Copy)
{
    const string_view sv("Hello, World!");
    char              buffer[20] = {};
    const size_t      copied     = sv.copy(buffer, 5);
    EXPECT_THAT(copied, 5u);
    EXPECT_THAT(std::string(buffer, copied), "Hello");
}

TEST_F(TestStringView, Substr)
{
    const string_view sv("Hello, World!");
    const string_view sub = sv.substr(7, 5);
    EXPECT_THAT(sub, "World");
#if defined(__cpp_exceptions)
    EXPECT_THROW(sv.substr(20), std::out_of_range);
#endif
}

TEST_F(TestStringView, Compare)
{
    const string_view sv1("abc");
    const string_view sv2("abc");
    const string_view sv3("abd");
    const string_view sv4("abcd");

    EXPECT_THAT(sv1.compare(sv2), 0);
    EXPECT_THAT(sv1.compare(sv3), Lt(0));
    EXPECT_THAT(sv3.compare(sv1), Gt(0));
    EXPECT_THAT(sv1.compare(sv4), Lt(0));
    EXPECT_THAT(sv4.compare(sv1), Gt(0));
}

TEST_F(TestStringView, FindChar)
{
    const string_view sv("Hello, World!");
    EXPECT_THAT(sv.find('W'), 7u);
    EXPECT_THAT(sv.find('z'), string_view::npos);
}

TEST_F(TestStringView, FindStringView)
{
    const string_view sv("Hello, World!");
    const string_view to_find("World");
    EXPECT_THAT(sv.find(to_find), 7u);
    EXPECT_THAT(sv.find(""), 0u);
    EXPECT_THAT(sv.find("Earth"), string_view::npos);
    EXPECT_THAT(sv.find("too long too long too long"), string_view::npos);
}

TEST_F(TestStringView, StartsWith)
{
    const string_view sv("Hello, World!");
    EXPECT_TRUE(sv.starts_with("Hello"));
    EXPECT_TRUE(sv.starts_with(""));
    EXPECT_FALSE(sv.starts_with("World"));
    EXPECT_FALSE(sv.starts_with("too long too long too long"));
}

TEST_F(TestStringView, EndsWith)
{
    const string_view sv("Hello, World!");
    EXPECT_TRUE(sv.ends_with("World!"));
    EXPECT_TRUE(sv.ends_with(""));
    EXPECT_FALSE(sv.ends_with("Hello"));
    EXPECT_FALSE(sv.ends_with("too long too long too long"));
}

TEST_F(TestStringView, RelationalOperators)
{
    const string_view sv1("abc");
    const string_view sv2("abc");
    const string_view sv3("abd");

    EXPECT_TRUE(sv1 == sv2);
    EXPECT_FALSE(sv1 != sv2);
    EXPECT_TRUE(sv1 < sv3);
    EXPECT_TRUE(sv3 > sv1);
    EXPECT_TRUE(sv1 <= sv2);
    EXPECT_TRUE(sv1 >= sv2);
}

TEST_F(TestStringView, FindOutOfBounds)
{
    const string_view sv("Hello");
    EXPECT_THAT(sv.find('H', 10), string_view::npos);
    EXPECT_THAT(sv.find("He", 10), string_view::npos);
}

TEST_F(TestStringView, RemovePrefixOutOfBounds)
{
    string_view sv("Hello");
    sv.remove_prefix(10);
    EXPECT_THAT(sv.size(), 0u);
}

TEST_F(TestStringView, RemoveSuffixOutOfBounds)
{
    string_view sv("Hello");
    sv.remove_suffix(10);
    EXPECT_THAT(sv.size(), 0u);
}

TEST_F(TestStringView, SubstrWithNpos)
{
    const string_view sv("Hello, World!");
    const string_view sub = sv.substr(7);
    EXPECT_THAT(sub, "World!");
}

TEST_F(TestStringView, EmptyStringViewOperations)
{
    const string_view sv;
    EXPECT_THAT(sv.size(), 0u);
    EXPECT_TRUE(sv.empty());
    EXPECT_THAT(sv.data(), IsNull());
    EXPECT_THAT(sv.begin(), sv.end());
}

TEST_F(TestStringView, ComparisonWithString)
{
    const string_view sv("Hello");
    const std::string str = "Hello";
    EXPECT_THAT(sv, str);
    EXPECT_THAT(str, sv);
}

TEST_F(TestStringView, ComparisonWithCString)
{
    const string_view sv("Hello");
    const char* const cstr = "Hello";
    EXPECT_THAT(sv, cstr);
    EXPECT_THAT(cstr, sv);
}

TEST_F(TestStringView, FindPartial)
{
    const string_view sv("ababab");
    EXPECT_THAT(sv.find("aba"), 0u);
    EXPECT_THAT(sv.find("aba", 1), 2u);
}

TEST_F(TestStringView, CopyOutOfBounds)
{
#if defined(__cpp_exceptions)
    const string_view sv("Hello");
    char              buffer[10];
    EXPECT_THROW(sv.copy(buffer, 5, 6), std::out_of_range);
#else
    GTEST_SKIP() << "Not applicable when exceptions are disabled.";
#endif
}
