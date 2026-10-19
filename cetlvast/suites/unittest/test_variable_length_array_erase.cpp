/// @file
/// Single-element and range erase, checked against std::vector.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <cstddef>
#include <memory>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
template <typename T>
using Array = cetl::VariableLengthArray<T, std::allocator<T>>;

template <typename T>
class VLAErase : public ::testing::Test
{};
using ElementTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLAErase, ElementTypes, );

template <typename Actual, typename Expected>
void expect_sequence(const Actual& actual, const Expected& expected)
{
    ASSERT_EQ(actual.size(), expected.size());
    auto item = expected.begin();
    for (std::size_t i = 0; i < actual.size(); ++i, ++item)
    {
        EXPECT_EQ(actual[i], *item) << "index=" << i;
    }
}

// An irregular pattern, so a shift by any distance changes the sequence.
template <typename T>
T pattern(const std::size_t index)
{
    return static_cast<T>(((index * 7U) % 5U) % 2U);
}

// Equal arrays of `size` elements, reserved to `capacity` so shrinking is observable.
template <typename T>
std::pair<Array<T>, std::vector<T>> make_pair_of(const std::size_t size, const std::size_t capacity)
{
    std::pair<Array<T>, std::vector<T>> result{Array<T>{std::allocator<T>{}}, std::vector<T>{}};
    result.first.reserve(capacity);
    for (std::size_t i = 0; i < size; ++i)
    {
        result.first.push_back(pattern<T>(i));
        result.second.push_back(pattern<T>(i));
    }
    return result;
}

template <typename Iterator, typename Container>
std::ptrdiff_t index_of(const Iterator& position, Container& container)
{
    return position - container.begin();
}

// Every position of arrays that end mid-byte, on a byte boundary, and one past it in the bool specialization.
TYPED_TEST(VLAErase, SingleElementMatchesVector)
{
    for (const std::size_t size : {1U, 2U, 7U, 8U, 9U, 17U})
    {
        for (std::size_t at = 0; at < size; ++at)
        {
            SCOPED_TRACE(::testing::Message() << "size=" << size << " at=" << at);
            auto       subjects = make_pair_of<TypeParam>(size, 24U);
            auto&      actual   = subjects.first;
            auto&      expected = subjects.second;
            const auto capacity = actual.capacity();
            const auto offset   = static_cast<std::ptrdiff_t>(at);
            const auto result   = actual.erase(actual.cbegin() + offset);
            const auto oracle   = expected.erase(expected.cbegin() + offset);
            EXPECT_EQ(index_of(result, actual), index_of(oracle, expected));
            EXPECT_EQ(result == actual.end(), oracle == expected.end());
            expect_sequence(actual, expected);
            EXPECT_EQ(actual.capacity(), capacity);
        }
    }
}

// Every [first, last) of a 17-element array, which spans three bytes in the bool specialization.
TYPED_TEST(VLAErase, RangeMatchesVector)
{
    constexpr std::size_t size = 17U;
    for (std::size_t first = 0; first <= size; ++first)
    {
        for (std::size_t last = first; last <= size; ++last)
        {
            SCOPED_TRACE(::testing::Message() << "first=" << first << " last=" << last);
            auto       subjects = make_pair_of<TypeParam>(size, size);
            auto&      actual   = subjects.first;
            auto&      expected = subjects.second;
            const auto capacity = actual.capacity();
            const auto from     = static_cast<std::ptrdiff_t>(first);
            const auto to       = static_cast<std::ptrdiff_t>(last);
            const auto result   = actual.erase(actual.cbegin() + from, actual.cbegin() + to);
            const auto oracle   = expected.erase(expected.cbegin() + from, expected.cbegin() + to);
            EXPECT_EQ(index_of(result, actual), index_of(oracle, expected));
            expect_sequence(actual, expected);
            EXPECT_EQ(actual.capacity(), capacity);

            // The array must remain usable, including the packed-bit bookkeeping at the new end.
            actual.push_back(TypeParam{1});
            expected.push_back(TypeParam{1});
            expect_sequence(actual, expected);
        }
    }
}

TYPED_TEST(VLAErase, AcceptsMutableAndConstIterators)
{
    auto  subjects = make_pair_of<TypeParam>(9U, 9U);
    auto& actual   = subjects.first;
    auto& expected = subjects.second;
    (void) actual.erase(actual.begin());
    (void) expected.erase(expected.begin());
    (void) actual.erase(actual.begin() + 1, actual.end() - 1);
    (void) expected.erase(expected.begin() + 1, expected.end() - 1);
    expect_sequence(actual, expected);
    (void) actual.erase(actual.cbegin(), actual.cend());
    EXPECT_TRUE(actual.empty());

    static_assert(std::is_convertible<typename Array<TypeParam>::iterator,
                                      typename Array<TypeParam>::const_iterator>::value,
                  "iterator must convert to const_iterator");
    static_assert(!std::is_convertible<typename Array<TypeParam>::const_iterator,
                                       typename Array<TypeParam>::iterator>::value,
                  "const_iterator must not convert to iterator");
}

// Since iterator converts to const_iterator, mixed comparisons and differences work with either operand first, as for
// std::vector.
TYPED_TEST(VLAErase, MixedIteratorsCompareInEitherOrder)
{
    auto        subjects = make_pair_of<TypeParam>(10U, 10U);
    auto&       actual   = subjects.first;
    const auto  first    = actual.begin() + 2;
    const auto  cfirst   = actual.cbegin() + 2;
    const auto  last     = actual.begin() + 7;
    const auto  clast    = actual.cbegin() + 7;
    std::size_t visited  = 0;
    for (auto it = actual.begin(); it != actual.cend(); ++it)
    {
        ++visited;
    }
    EXPECT_EQ(visited, 10U);

    EXPECT_TRUE(first == cfirst);
    EXPECT_TRUE(cfirst == first);
    EXPECT_TRUE(first != clast);
    EXPECT_TRUE(clast != first);
    EXPECT_TRUE(first < clast);
    EXPECT_TRUE(cfirst < last);
    EXPECT_TRUE(last > cfirst);
    EXPECT_TRUE(clast > first);
    EXPECT_TRUE(first <= cfirst);
    EXPECT_TRUE(cfirst <= first);
    EXPECT_TRUE(last >= clast);
    EXPECT_TRUE(clast >= last);
    EXPECT_EQ(last - cfirst, 5);
    EXPECT_EQ(clast - first, 5);
    EXPECT_EQ(actual.cend() - actual.begin(), 10);
    EXPECT_EQ(actual.end() - actual.cbegin(), 10);
}

// An empty range has no effect, including on an array that has never allocated.
TYPED_TEST(VLAErase, EmptyRangeHasNoEffect)
{
    Array<TypeParam> empty{std::allocator<TypeParam>{}};
    // Each result is stored first: EXPECT_EQ does not order the evaluation of its arguments.
    const auto empty_result = empty.erase(empty.cbegin(), empty.cend());
    EXPECT_EQ(empty_result, empty.end());
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.capacity(), 0U);

    auto       subjects = make_pair_of<TypeParam>(10U, 10U);
    auto&      actual   = subjects.first;
    const auto middle   = actual.erase(actual.cbegin() + 4, actual.cbegin() + 4);
    EXPECT_EQ(middle, actual.begin() + 4);
    const auto end = actual.erase(actual.cend(), actual.cend());
    EXPECT_EQ(end, actual.end());
    expect_sequence(actual, subjects.second);
}

TYPED_TEST(VLAErase, EraseEverythingKeepsCapacityAndMaxSize)
{
    Array<TypeParam> actual(13U, TypeParam{1}, 20U, std::allocator<TypeParam>{});
    const auto       capacity = actual.capacity();
    // Store the result first: EXPECT_EQ could otherwise evaluate end() before the erase.
    const auto result = actual.erase(actual.cbegin(), actual.cend());
    EXPECT_EQ(result, actual.end());
    EXPECT_TRUE(actual.empty());
    EXPECT_EQ(actual.capacity(), capacity);
    EXPECT_EQ(actual.max_size(), 20U);
    actual.push_back(TypeParam{0});
    expect_sequence(actual, std::vector<TypeParam>{TypeParam{0}});
}

// The returned iterator supports the usual erase-while-iterating loop.
TYPED_TEST(VLAErase, EraseWhileIterating)
{
    auto  subjects = make_pair_of<TypeParam>(23U, 23U);
    auto& actual   = subjects.first;
    auto& expected = subjects.second;
    for (auto it = actual.begin(); it != actual.end();)
    {
        it = (*it == TypeParam{1}) ? actual.erase(it) : it + 1;
    }
    for (auto it = expected.begin(); it != expected.end();)
    {
        it = (*it == TypeParam{1}) ? expected.erase(it) : it + 1;
    }
    expect_sequence(actual, expected);
}

TYPED_TEST(VLAErase, IteratorsBeforeTheErasedRangeRemainValid)
{
    auto       subjects = make_pair_of<TypeParam>(12U, 12U);
    auto&      actual   = subjects.first;
    const auto before   = actual.begin() + 2;
    const auto value    = static_cast<TypeParam>(*before);
    (void) actual.erase(actual.cbegin() + 5, actual.cbegin() + 9);
    EXPECT_EQ(static_cast<TypeParam>(*before), value);
    EXPECT_EQ(before, actual.begin() + 2);
}

TEST(VLAErase, NonTrivialElements)
{
    const std::vector<std::string> source{"zero",
                                          "one",
                                          "a string long enough to need its own heap allocation",
                                          "three",
                                          "another string long enough to need its own heap allocation",
                                          "five"};
    Array<std::string>             actual(source.begin(), source.end(), std::allocator<std::string>{});
    std::vector<std::string>       expected(source);
    (void) actual.erase(actual.cbegin() + 1);
    (void) expected.erase(expected.cbegin() + 1);
    expect_sequence(actual, expected);
    (void) actual.erase(actual.cbegin() + 1, actual.cbegin() + 3);
    (void) expected.erase(expected.cbegin() + 1, expected.cbegin() + 3);
    expect_sequence(actual, expected);
}

TEST(VLAErase, MoveOnlyElements)
{
    Array<std::unique_ptr<int>> actual{std::allocator<std::unique_ptr<int>>{}};
    for (int i = 0; i < 5; ++i)
    {
        actual.push_back(std::make_unique<int>(i));
    }
    const auto result = actual.erase(actual.cbegin() + 1, actual.cbegin() + 3);
    ASSERT_EQ(actual.size(), 3U);
    EXPECT_EQ(result, actual.begin() + 1);
    EXPECT_EQ(*actual[0], 0);
    EXPECT_EQ(*actual[1], 3);
    EXPECT_EQ(*actual[2], 4);
}

// Erase cannot fail for elements whose move assignment and destructor cannot.
struct ThrowingMove
{
    ThrowingMove()                               = default;
    ThrowingMove(const ThrowingMove&)            = default;
    ThrowingMove& operator=(const ThrowingMove&) = default;
    ThrowingMove& operator=(ThrowingMove&&) noexcept(false)
    {
        return *this;
    }
};
static_assert(noexcept(std::declval<Array<int>&>().erase(std::declval<Array<int>::const_iterator>())),
              "erase of nothrow elements is noexcept");
static_assert(noexcept(std::declval<Array<bool>&>().erase(std::declval<Array<bool>::const_iterator>(),
                                                          std::declval<Array<bool>::const_iterator>())),
              "erase of bits is noexcept");
static_assert(
    !noexcept(std::declval<Array<ThrowingMove>&>().erase(std::declval<Array<ThrowingMove>::const_iterator>())),
    "erase propagates a throwing move assignment");

}  // namespace
