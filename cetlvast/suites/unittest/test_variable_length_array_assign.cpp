/// @file
/// Range and initializer-list assignment, including single-pass sources and bounded storage.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <array>
#include <forward_list>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
template <typename T>
using Array = cetl::VariableLengthArray<T, std::allocator<T>>;

template <typename T>
class VLAAssign : public ::testing::Test
{};
using ElementTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLAAssign, ElementTypes, );

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

TYPED_TEST(VLAAssign, RangeReplacesGrowsShrinksAndClears)
{
    Array<TypeParam>          actual{std::allocator<TypeParam>{}};
    std::vector<TypeParam>    expected;
    const std::array<int, 19> source{{1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1}};
    // Cross byte boundaries and leave a partial final byte in the bool specialization.
    for (const std::size_t count : {0U, 1U, 9U, 19U, 8U, 3U, 0U, 17U})
    {
        SCOPED_TRACE(count);
        const auto last = source.begin() + static_cast<std::ptrdiff_t>(count);
        actual.assign(source.begin(), last);
        expected.assign(source.begin(), last);
        expect_sequence(actual, expected);
    }
    actual.push_back(TypeParam{});
    expected.push_back(TypeParam{});
    expect_sequence(actual, expected);
}

TYPED_TEST(VLAAssign, InitializerListAndIntegralOverloadResolution)
{
    Array<TypeParam> actual{std::allocator<TypeParam>{}};
    actual.assign({1, 0, 1, 1, 0, 0, 1, 0, 1});
    expect_sequence(actual, std::vector<TypeParam>{1, 0, 1, 1, 0, 0, 1, 0, 1});
    const auto capacity = actual.capacity();
    actual.assign({0, 1});
    expect_sequence(actual, std::vector<TypeParam>{0, 1});
    EXPECT_EQ(actual.capacity(), capacity);
    actual.assign({});
    EXPECT_TRUE(actual.empty());
    EXPECT_EQ(actual.capacity(), capacity);
    // Equal integral argument types must select count/value, never the iterator overload.
    actual.assign(3, 1);
    expect_sequence(actual, std::vector<TypeParam>(3, 1));
    actual.assign(short{2}, short{0});
    expect_sequence(actual, std::vector<TypeParam>(2, 0));
}

TYPED_TEST(VLAAssign, SinglePassAndForwardIterators)
{
    Array<TypeParam> actual{std::allocator<TypeParam>{}};
    for (const char* text : {"1 0 1 0 1 1 0 0 1 0 1", "0 1", "", "1 1 0"})
    {
        std::istringstream input{text};
        std::istringstream reference_input{text};
        actual.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{});
        std::vector<TypeParam> expected;
        expected.assign(std::istream_iterator<int>{reference_input}, std::istream_iterator<int>{});
        expect_sequence(actual, expected);
    }
    const std::forward_list<int> source{0, 1, 1, 0, 1, 0, 1, 1, 1};
    actual.assign(source.begin(), source.end());
    expect_sequence(actual, std::vector<TypeParam>(source.begin(), source.end()));
}

TYPED_TEST(VLAAssign, BoundedForwardAssignmentHonorsLogicalMaximum)
{
    for (const std::size_t limit : {0U, 1U, 9U})
    {
        SCOPED_TRACE(limit);
        Array<TypeParam> actual{limit, std::allocator<TypeParam>{}};
        // Packed bool has physical capacity past a non-byte-aligned logical bound.
        actual.reserve(limit);
        const std::array<int, 11> source{{1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1}};
#if defined(__cpp_exceptions)
        EXPECT_THROW(actual.assign(source.begin(), source.end()), std::length_error);
        EXPECT_TRUE(actual.empty());
#else
        actual.assign(source.begin(), source.end());
        expect_sequence(actual,
                        std::vector<TypeParam>(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(limit)));
#endif
        actual.assign({});
        EXPECT_TRUE(actual.empty());
    }
}

TYPED_TEST(VLAAssign, BoundedInputAssignmentStopsAtLogicalMaximum)
{
    for (const std::size_t limit : {0U, 1U, 9U})
    {
        SCOPED_TRACE(limit);
        Array<TypeParam> actual{limit, std::allocator<TypeParam>{}};
        actual.reserve(limit);
        std::istringstream input{"1 0 1 1 0 0 1 0 1 0 1"};
#if defined(__cpp_exceptions)
        EXPECT_THROW(actual.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{}), std::length_error);
#else
        actual.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{});
#endif
        const std::array<int, 11> source{{1, 0, 1, 1, 0, 0, 1, 0, 1, 0, 1}};
        expect_sequence(actual,
                        std::vector<TypeParam>(source.begin(), source.begin() + static_cast<std::ptrdiff_t>(limit)));
    }
}

TYPED_TEST(VLAAssign, InitializerListHonorsLogicalMaximum)
{
    Array<TypeParam> actual{1, std::allocator<TypeParam>{}};
#if defined(__cpp_exceptions)
    EXPECT_THROW(actual.assign({1, 0}), std::length_error);
    EXPECT_TRUE(actual.empty());
#else
    actual.assign({1, 0});
    expect_sequence(actual, std::vector<TypeParam>{1});
#endif
}

struct AllocationState
{
    bool        decline = false;
    std::size_t calls   = 0;
};

template <typename T>
struct Allocator
{
    using value_type = T;
    AllocationState* state;
    explicit Allocator(AllocationState& s) noexcept
        : state(&s)
    {
    }
    template <typename U>
    Allocator(const Allocator<U>& other) noexcept
        : state(other.state)
    {
    }
    T* allocate(std::size_t count)
    {
        ++state->calls;
        return state->decline ? nullptr : std::allocator<T>{}.allocate(count);
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    {
        std::allocator<T>{}.deallocate(pointer, count);
    }
};

TYPED_TEST(VLAAssign, ReusesStorageAndRetainsAllocatorAndMaximum)
{
    AllocationState state;
    using Subject = cetl::VariableLengthArray<TypeParam, Allocator<TypeParam>>;
    Subject actual{23, typename Subject::allocator_type{state}};
    actual.reserve(19);
    const auto capacity = actual.capacity();
    const auto calls    = state.calls;
    state.decline       = true;
    const std::forward_list<int> source{1, 0, 1, 1, 0, 0, 1, 0, 1};
    actual.assign(source.begin(), source.end());
    expect_sequence(actual, std::vector<TypeParam>(source.begin(), source.end()));
    actual.assign({0, 1});
    expect_sequence(actual, std::vector<TypeParam>{0, 1});
    actual.assign({});
    EXPECT_TRUE(actual.empty());
    EXPECT_EQ(actual.capacity(), capacity);
    EXPECT_EQ(state.calls, calls);
    EXPECT_EQ(actual.get_allocator().state, &state);
    EXPECT_EQ(actual.max_size(), 23U);
}

TYPED_TEST(VLAAssign, DeclinedGrowthKeepsAssignedPrefixAndCanRetry)
{
    for (const bool reserve : {false, true})
    {
        AllocationState state;
        using Subject = cetl::VariableLengthArray<TypeParam, Allocator<TypeParam>>;
        Subject actual{typename Subject::allocator_type{state}};
        if (reserve)
        {
            actual.reserve(8);
            actual.assign({0, 0});
        }
        const auto capacity = actual.capacity();
        state.decline       = true;
        std::vector<int> source(capacity + 1, 1);
        std::string      text;
        for (const auto item : source)
        {
            text += std::to_string(item) + " ";
        }
        std::istringstream input{text};
#if defined(__cpp_exceptions)
        EXPECT_THROW(actual.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{}), std::bad_alloc);
#else
        actual.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{});
#endif
        expect_sequence(actual, std::vector<TypeParam>(capacity, 1));
        EXPECT_EQ(actual.capacity(), capacity);
        state.decline = false;
        actual.assign(source.begin(), source.end());
        expect_sequence(actual, std::vector<TypeParam>(source.size(), 1));
    }
}

TYPED_TEST(VLAAssign, ForwardAssignmentAllocatesOnceAndRecoversFromFailure)
{
    AllocationState state;
    using Subject = cetl::VariableLengthArray<TypeParam, Allocator<TypeParam>>;
    Subject actual{typename Subject::allocator_type{state}};
    actual.assign({0, 0});
    state.calls   = 0;
    state.decline = true;
    const std::forward_list<int> source{1, 0, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 0, 1, 0, 0, 1};
#if defined(__cpp_exceptions)
    EXPECT_THROW(actual.assign(source.begin(), source.end()), std::bad_alloc);
#else
    actual.assign(source.begin(), source.end());
#endif
    EXPECT_TRUE(actual.empty());
    EXPECT_EQ(actual.capacity(), 0U);
    EXPECT_EQ(state.calls, 1U);
    state.decline = false;
    state.calls   = 0;
    actual.assign(source.begin(), source.end());
    expect_sequence(actual, std::vector<TypeParam>(source.begin(), source.end()));
    EXPECT_EQ(state.calls, 1U);
}

// Forward-iterator assignment only requires construction and assignment from the source reference type.
// It must not instantiate relocation or default construction of the destination element type.
struct Immovable
{
    int value;
    Immovable(int v)
        : value(v)
    {
    }
    Immovable(const Immovable&)            = delete;
    Immovable(Immovable&&)                 = delete;
    Immovable& operator=(const Immovable&) = delete;
    Immovable& operator=(Immovable&&)      = delete;
    Immovable& operator=(int v)
    {
        value = v;
        return *this;
    }
};

TEST(VLAAssignObjects, ForwardAssignmentDoesNotRequireMovableElements)
{
    Array<Immovable>         actual{std::allocator<Immovable>{}};
    std::vector<Immovable>   expected;
    const std::array<int, 5> source{{1, 2, 3, 4, 5}};
    for (const std::size_t count : {2U, 5U, 3U, 0U})
    {
        const auto last = source.begin() + static_cast<std::ptrdiff_t>(count);
        actual.assign(source.begin(), last);
        expected.assign(source.begin(), last);
        ASSERT_EQ(actual.size(), expected.size());
        for (std::size_t i = 0; i < count; ++i)
        {
            EXPECT_EQ(actual[i].value, expected[i].value);
        }
    }
}

TEST(VLAAssignObjects, NontrivialValuesAndEmptyNullPointerRange)
{
    Array<std::string> actual{std::allocator<std::string>{}};
    const std::string* empty = nullptr;
    actual.assign(empty, empty);
    actual.assign({"one", "two", "three"});
    const std::forward_list<std::string> source{"four", "five"};
    actual.assign(source.begin(), source.end());
    expect_sequence(actual, std::vector<std::string>{"four", "five"});
    actual.assign(empty, empty);
    EXPECT_TRUE(actual.empty());
}

TEST(VLAAssignObjects, MoveIteratorsSupportMoveOnlyElements)
{
    using Value = std::unique_ptr<int>;
    Array<Value> actual{std::allocator<Value>{}};
    actual.emplace_back(new int{99});
    for (const std::size_t count : {5U, 2U, 0U})
    {
        std::vector<Value> source;
        for (std::size_t i = 0; i < count; ++i)
        {
            source.emplace_back(new int{static_cast<int>(i)});
        }
        actual.assign(std::make_move_iterator(source.begin()), std::make_move_iterator(source.end()));
        ASSERT_EQ(actual.size(), count);
        for (std::size_t i = 0; i < count; ++i)
        {
            ASSERT_NE(actual[i], nullptr);
            EXPECT_EQ(*actual[i], static_cast<int>(i));
            EXPECT_EQ(source[i], nullptr);
        }
    }
}

TEST(VLAAssignObjects, PackedBoolProxySources)
{
    Array<bool> source{{true, false, true, false, false, true, false, true, true}, std::allocator<bool>{}};
    Array<bool> actual{std::allocator<bool>{}};
    actual.assign(source.begin(), source.end());
    expect_sequence(actual, source);
    std::vector<bool> other{false, true, false};
    actual.assign(other.begin(), other.end());
    expect_sequence(actual, other);
}

template <typename A, typename It, typename = void>
struct CanAssignRange : std::false_type
{};
template <typename A, typename It>
struct CanAssignRange<A, It, decltype(std::declval<A&>().assign(std::declval<It>(), std::declval<It>()))>
    : std::true_type
{};
static_assert(CanAssignRange<Array<int>, int*>::value, "Pointers must be accepted.");
static_assert(CanAssignRange<Array<bool>, std::istream_iterator<int>>::value, "Input iterators must be accepted.");
static_assert(!CanAssignRange<Array<int>, std::back_insert_iterator<std::vector<int>>>::value,
              "Output-only iterators must be excluded from overload resolution.");
static_assert(!CanAssignRange<Array<bool>, std::back_insert_iterator<std::vector<bool>>>::value,
              "Output-only iterators must be excluded from overload resolution.");
}  // namespace
