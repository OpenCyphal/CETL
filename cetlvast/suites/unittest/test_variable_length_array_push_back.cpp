/// @file
/// push_back and emplace_back of an argument that refers to an element of the same array (issue #245), checked
/// against std::vector with and without reallocation.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace
{
template <typename T>
using Array = cetl::VariableLengthArray<T, std::allocator<T>>;

constexpr std::size_t ElementCount = 5;

// Long enough to need its own heap allocation, so a read from a moved-from or released element is visible.
std::string string_value(const std::size_t index)
{
    return std::string(64U, static_cast<char>('a' + index)) + std::to_string(index);
}

int int_value(const std::size_t index)
{
    return static_cast<int>(100U + (index * 7U));
}

bool bool_value(const std::size_t index)
{
    return (index % 3U) != 1U;
}

std::string make(const std::size_t index, const std::string*)
{
    return string_value(index);
}

int make(const std::size_t index, const int*)
{
    return int_value(index);
}

bool make(const std::size_t index, const bool*)
{
    return bool_value(index);
}

// The ways to append a value that the arguments derive from element `at`.
enum class Append
{
    PushBackCopy,
    PushBackMove,
    EmplaceBackCopy,
    EmplaceBackMove,
};

const char* to_string(const Append append)
{
    switch (append)
    {
    case Append::PushBackCopy:
        return "push_back(const T&)";
    case Append::PushBackMove:
        return "push_back(T&&)";
    case Append::EmplaceBackCopy:
        return "emplace_back(T&)";
    case Append::EmplaceBackMove:
        return "emplace_back(T&&)";
    }
    return "?";  // GCOVR_EXCL_LINE
}

template <typename Container>
void append(Container& container, const Append how, const std::size_t at)
{
    switch (how)
    {
    case Append::PushBackCopy:
        container.push_back(static_cast<const typename Container::value_type&>(container[at]));
        break;
    case Append::PushBackMove:
        container.push_back(std::move(container[at]));
        break;
    case Append::EmplaceBackCopy:
        container.emplace_back(container[at]);
        break;
    case Append::EmplaceBackMove:
        container.emplace_back(std::move(container[at]));
        break;
    }
}

bool moves(const Append how)
{
    return (how == Append::PushBackMove) || (how == Append::EmplaceBackMove);
}

template <typename T>
class VLAAppendElement : public ::testing::Test
{};
using ElementTypes = ::testing::Types<std::string, int>;
TYPED_TEST_SUITE(VLAAppendElement, ElementTypes, );

// The first, a middle, and the last element, appended by every overload, with the array full (so the append must
// reallocate) and with spare capacity. A moved-from element has an unspecified value, so it is not compared.
TYPED_TEST(VLAAppendElement, MatchesVector)
{
    const TypeParam* const tag = nullptr;
    for (const Append how :
         {Append::PushBackCopy, Append::PushBackMove, Append::EmplaceBackCopy, Append::EmplaceBackMove})
    {
        for (const std::size_t at : {std::size_t{0}, ElementCount / 2U, ElementCount - 1U})
        {
            for (const bool reallocates : {true, false})
            {
                SCOPED_TRACE(::testing::Message() << to_string(how) << " at=" << at << " reallocates=" << reallocates);
                Array<TypeParam>       actual{std::allocator<TypeParam>{}};
                std::vector<TypeParam> expected;
                actual.reserve(reallocates ? ElementCount : (ElementCount + 1U));
                for (std::size_t i = 0; i < ElementCount; ++i)
                {
                    actual.push_back(make(i, tag));
                    expected.push_back(make(i, tag));
                }
                ASSERT_EQ(actual.capacity() == actual.size(), reallocates);
                const TypeParam* const data_before = actual.data();

                append(actual, how, at);
                append(expected, how, at);

                EXPECT_EQ(actual.data() != data_before, reallocates);
                ASSERT_EQ(actual.size(), expected.size());
                for (std::size_t i = 0; i < actual.size(); ++i)
                {
                    if (!moves(how) || (i != at))
                    {
                        EXPECT_EQ(actual[i], expected[i]) << "index=" << i;
                    }
                }
                EXPECT_EQ(actual.back(), make(at, tag));
            }
        }
    }
}

// emplace_back arguments other than a whole element may also refer into the array, here a substring of one.
TEST(VLAAppendElement, EmplaceBackConstructorArgumentsMatchVector)
{
    for (const bool reallocates : {true, false})
    {
        SCOPED_TRACE(::testing::Message() << "reallocates=" << reallocates);
        Array<std::string>       actual{std::allocator<std::string>{}};
        std::vector<std::string> expected;
        actual.reserve(reallocates ? ElementCount : (ElementCount + 1U));
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            actual.push_back(string_value(i));
            expected.push_back(string_value(i));
        }
        const std::string* const data_before = actual.data();

        actual.emplace_back(actual[1], 2U, 40U);
        expected.emplace_back(expected[1], 2U, 40U);

        EXPECT_EQ(actual.data() != data_before, reallocates);
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t i = 0; i < actual.size(); ++i)
        {
            EXPECT_EQ(actual[i], expected[i]) << "index=" << i;
        }
        EXPECT_EQ(actual.back(), string_value(1).substr(2U, 40U));
    }
}

// Repeatedly appending the first and last elements grows through several capacities.
TEST(VLAAppendElement, RepeatedAppendsAcrossGrowthMatchVector)
{
    Array<std::string>       actual{std::allocator<std::string>{}};
    std::vector<std::string> expected;
    actual.push_back(string_value(0));
    expected.push_back(string_value(0));
    for (std::size_t i = 0; i < 40U; ++i)
    {
        if ((i % 2U) == 0U)
        {
            actual.push_back(actual.front());
            expected.push_back(expected.front());
        }
        else
        {
            actual.emplace_back(actual.back() + "+");
            expected.emplace_back(expected.back() + "+");
        }
    }
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        EXPECT_EQ(actual[i], expected[i]) << "index=" << i;
    }
}

// The bool specialization takes its argument by value, so the bit is read before the storage can change.
TEST(VLAAppendElement, BoolMatchesVector)
{
    const bool* const tag = nullptr;
    for (const std::size_t at : {std::size_t{0}, ElementCount / 2U, ElementCount - 1U})
    {
        for (const bool emplace : {false, true})
        {
            SCOPED_TRACE(::testing::Message() << "at=" << at << " emplace=" << emplace);
            Array<bool>       actual{std::allocator<bool>{}};
            std::vector<bool> expected;
            for (std::size_t i = 0; i < ElementCount; ++i)
            {
                actual.push_back(make(i, tag));
                expected.push_back(make(i, tag));
            }
            // One storage byte holds eight bits, so the ninth append grows the storage.
            actual.shrink_to_fit();
            ASSERT_EQ(actual.capacity(), 8U);
            for (std::size_t i = ElementCount; i < 9U; ++i)
            {
                if (emplace)
                {
                    actual.emplace_back(actual[at]);
                    expected.emplace_back(expected[at]);
                }
                else
                {
                    actual.push_back(actual[at]);
                    expected.push_back(expected[at]);
                }
            }
            ASSERT_EQ(actual.size(), expected.size());
            for (std::size_t i = 0; i < actual.size(); ++i)
            {
                EXPECT_EQ(actual[i], expected[i]) << "index=" << i;
            }
        }
    }
}
}  // namespace
