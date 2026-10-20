/// @file
/// Reverse iterators, checked against std::vector.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <algorithm>
#include <cstddef>
#include <iterator>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
template <typename T>
using Array = cetl::variable_length_array<T, std::allocator<T>>;

template <typename T>
class VLAReverseIterators : public ::testing::Test
{};
using ElementTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLAReverseIterators, ElementTypes, );

// An irregular pattern, so reversing it changes the sequence.
template <typename T>
T pattern(const std::size_t index)
{
    return static_cast<T>(((index * 7U) % 5U) % 2U);
}

template <typename T>
std::pair<Array<T>, std::vector<T>> make_pair_of(const std::size_t size)
{
    std::pair<Array<T>, std::vector<T>> result{Array<T>{std::allocator<T>{}}, std::vector<T>{}};
    for (std::size_t i = 0; i < size; ++i)
    {
        result.first.push_back(pattern<T>(i));
        result.second.push_back(pattern<T>(i));
    }
    return result;
}

TYPED_TEST(VLAReverseIterators, MemberTypesAndReturnTypes)
{
    using A = Array<TypeParam>;
    static_assert(std::is_same<typename A::reverse_iterator, std::reverse_iterator<typename A::iterator>>::value,
                  "reverse_iterator adapts iterator");
    static_assert(std::is_same<typename A::const_reverse_iterator,
                               std::reverse_iterator<typename A::const_iterator>>::value,
                  "const_reverse_iterator adapts const_iterator");
    static_assert(std::is_same<decltype(std::declval<A&>().rbegin()), typename A::reverse_iterator>::value, "");
    static_assert(std::is_same<decltype(std::declval<A&>().rend()), typename A::reverse_iterator>::value, "");
    static_assert(std::is_same<decltype(std::declval<const A&>().rbegin()), typename A::const_reverse_iterator>::value,
                  "");
    static_assert(std::is_same<decltype(std::declval<const A&>().rend()), typename A::const_reverse_iterator>::value,
                  "");
    static_assert(std::is_same<decltype(std::declval<A&>().crbegin()), typename A::const_reverse_iterator>::value, "");
    static_assert(std::is_same<decltype(std::declval<A&>().crend()), typename A::const_reverse_iterator>::value, "");
    static_assert(noexcept(std::declval<A&>().rbegin()) && noexcept(std::declval<A&>().rend()) &&
                      noexcept(std::declval<const A&>().rbegin()) && noexcept(std::declval<const A&>().rend()) &&
                      noexcept(std::declval<A&>().crbegin()) && noexcept(std::declval<A&>().crend()),
                  "the accessors are noexcept, as std::vector's are");
}

// Sizes that end mid-byte, on a byte boundary, and just past one in the bool specialization.
TYPED_TEST(VLAReverseIterators, TraverseInReverseLikeVector)
{
    for (const std::size_t size : {0U, 1U, 7U, 8U, 9U, 17U})
    {
        SCOPED_TRACE(size);
        auto        subjects = make_pair_of<TypeParam>(size);
        auto&       actual   = subjects.first;
        const auto& expected = subjects.second;
        const auto& constant = actual;
        EXPECT_TRUE(std::equal(actual.rbegin(), actual.rend(), expected.rbegin(), expected.rend()));
        EXPECT_TRUE(std::equal(constant.rbegin(), constant.rend(), expected.rbegin(), expected.rend()));
        EXPECT_TRUE(std::equal(actual.crbegin(), actual.crend(), expected.crbegin(), expected.crend()));
        EXPECT_EQ(std::distance(actual.rbegin(), actual.rend()), static_cast<std::ptrdiff_t>(size));
        EXPECT_EQ(std::distance(actual.crbegin(), actual.crend()), static_cast<std::ptrdiff_t>(size));
        EXPECT_EQ(actual.rbegin().base(), actual.end());
        EXPECT_EQ(actual.rend().base(), actual.begin());
        EXPECT_EQ(actual.crbegin().base(), actual.cend());
        EXPECT_EQ(actual.crend().base(), actual.cbegin());
        if (size > 0U)
        {
            EXPECT_EQ(static_cast<TypeParam>(*actual.rbegin()), expected.back());
            EXPECT_EQ(static_cast<TypeParam>(*actual.crbegin()), expected.back());
            // Random access through the adaptor: the second-to-last element, or the only one.
            const std::size_t offset = 1U % size;
            EXPECT_EQ(static_cast<TypeParam>(actual.rbegin()[static_cast<std::ptrdiff_t>(offset)]),
                      expected[size - 1U - offset]);
        }
    }
}

// An array that has never allocated has an empty reverse range.
TYPED_TEST(VLAReverseIterators, EmptyArrayHasEmptyRange)
{
    Array<TypeParam>        empty{std::allocator<TypeParam>{}};
    const Array<TypeParam>& constant = empty;
    EXPECT_EQ(empty.capacity(), 0U);
    EXPECT_TRUE(empty.rbegin() == empty.rend());
    EXPECT_TRUE(constant.rbegin() == constant.rend());
    EXPECT_TRUE(empty.crbegin() == empty.crend());
}

TYPED_TEST(VLAReverseIterators, WriteThroughReverseIterators)
{
    auto  subjects     = make_pair_of<TypeParam>(13U);
    auto& actual       = subjects.first;
    auto& expected     = subjects.second;
    *actual.rbegin()   = TypeParam{0};
    *expected.rbegin() = TypeParam{0};
    std::fill(actual.rbegin() + 2, actual.rbegin() + 6, TypeParam{1});
    std::fill(expected.rbegin() + 2, expected.rbegin() + 6, TypeParam{1});
    ASSERT_EQ(actual.size(), expected.size());
    EXPECT_TRUE(std::equal(actual.begin(), actual.end(), expected.begin(), expected.end()));

    // Reversing through the adaptor gives the same result as std::reverse_copy.
    std::vector<TypeParam> reversed(actual.rbegin(), actual.rend());
    std::vector<TypeParam> oracle(expected.size());
    (void) std::reverse_copy(expected.begin(), expected.end(), oracle.begin());
    EXPECT_EQ(reversed, oracle);
}

// A reverse_iterator converts to a const_reverse_iterator, and the two compare and subtract in either order, as for
// std::vector.
TYPED_TEST(VLAReverseIterators, MixedReverseIterators)
{
    auto                                                    subjects  = make_pair_of<TypeParam>(10U);
    auto&                                                   actual    = subjects.first;
    const typename Array<TypeParam>::const_reverse_iterator converted = actual.rbegin();
    EXPECT_TRUE(converted == actual.crbegin());
    EXPECT_TRUE(actual.rbegin() == actual.crbegin());
    EXPECT_TRUE(actual.crbegin() == actual.rbegin());
    EXPECT_TRUE(actual.rbegin() != actual.crend());
    EXPECT_TRUE(actual.rbegin() < actual.crend());
    EXPECT_TRUE(actual.crend() > actual.rbegin());
    EXPECT_EQ(actual.crend() - actual.rbegin(), 10);
    EXPECT_EQ(actual.rend() - actual.crbegin(), 10);
}

// Algorithms that search from the back find the last match.
TYPED_TEST(VLAReverseIterators, FindLastMatch)
{
    auto        subjects = make_pair_of<TypeParam>(17U);
    const auto& actual   = subjects.first;
    const auto& expected = subjects.second;
    for (const TypeParam value : {TypeParam{0}, TypeParam{1}})
    {
        const auto found  = std::find(actual.crbegin(), actual.crend(), value);
        const auto oracle = std::find(expected.crbegin(), expected.crend(), value);
        EXPECT_EQ(std::distance(actual.crbegin(), found), std::distance(expected.crbegin(), oracle));
    }
}

// Algorithms that swap elements reach the bit proxies through std::iter_swap, so the proxies must be swappable as
// std::vector<bool>'s are.
TEST(VLAReverseIterators, SwappingAlgorithmsOnBits)
{
    auto       subjects = make_pair_of<bool>(19U);
    auto&      actual   = subjects.first;
    auto&      expected = subjects.second;
    const auto same     = [&actual, &expected]() {
        return std::equal(actual.begin(), actual.end(), expected.begin(), expected.end());
    };

    std::reverse(actual.rbegin(), actual.rend());
    std::reverse(expected.rbegin(), expected.rend());
    EXPECT_TRUE(same());
    std::reverse(actual.begin() + 3, actual.end());
    std::reverse(expected.begin() + 3, expected.end());
    EXPECT_TRUE(same());
    (void) std::rotate(actual.rbegin(), actual.rbegin() + 5, actual.rend());
    (void) std::rotate(expected.rbegin(), expected.rbegin() + 5, expected.rend());
    EXPECT_TRUE(same());
    std::iter_swap(actual.begin(), actual.rbegin());
    std::iter_swap(expected.begin(), expected.rbegin());
    EXPECT_TRUE(same());
    std::sort(actual.begin(), actual.end());
    std::sort(expected.begin(), expected.end());
    EXPECT_TRUE(same());

    // Swapping two proxies directly: through ADL, and through the static member std::vector<bool> also provides.
    actual[0] = false;
    actual[1] = true;
    using std::swap;
    swap(actual[0], actual[1]);
    EXPECT_TRUE(actual[0]);
    EXPECT_FALSE(actual[1]);
    Array<bool>::swap(actual[0], actual[1]);
    EXPECT_FALSE(actual[0]);
    EXPECT_TRUE(actual[1]);
    static_assert(noexcept(swap(actual[0], actual[1])), "swapping bits cannot fail");
    static_assert(noexcept(Array<bool>::swap(actual[0], actual[1])), "swapping bits cannot fail");
}

TEST(VLAReverseIterators, NonTrivialElements)
{
    const std::vector<std::string> source{"one", "two", "a string long enough to need its own heap allocation"};
    Array<std::string>             actual(source.begin(), source.end(), std::allocator<std::string>{});
    EXPECT_TRUE(std::equal(actual.rbegin(), actual.rend(), source.rbegin(), source.rend()));
    EXPECT_EQ(actual.rbegin()->size(), source.back().size());  // operator-> through the adaptor
    actual.rbegin()->append("!");
    EXPECT_EQ(actual.back(), source.back() + "!");
}

}  // namespace
