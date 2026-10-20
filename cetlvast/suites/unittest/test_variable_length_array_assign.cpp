/// @file
/// Range and initializer-list assignment, including single-pass sources and bounded storage.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <forward_list>
#include <iterator>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace
{
template <typename T>
using Array = cetl::variable_length_array<T, std::allocator<T>>;

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
        // Fill to the limit first so a rejected range must leave contents and capacity untouched, not cleared.
        const std::array<int, 9> before{{0, 1, 1, 0, 1, 0, 0, 1, 1}};
        actual.assign(before.begin(), before.begin() + static_cast<std::ptrdiff_t>(limit));
        const auto capacity = actual.capacity();
        EXPECT_THROW(actual.assign(source.begin(), source.end()), std::length_error);
        expect_sequence(actual,
                        std::vector<TypeParam>(before.begin(), before.begin() + static_cast<std::ptrdiff_t>(limit)));
        EXPECT_EQ(actual.capacity(), capacity);
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
    actual.assign({0});
    const auto capacity = actual.capacity();
    EXPECT_THROW(actual.assign({1, 0}), std::length_error);
    expect_sequence(actual, std::vector<TypeParam>{0});
    EXPECT_EQ(actual.capacity(), capacity);
#else
    actual.assign({1, 0});
    expect_sequence(actual, std::vector<TypeParam>{1});
#endif
}

TYPED_TEST(VLAAssign, InitializerListOperatorMatchesVector)
{
    Array<TypeParam>       actual{std::allocator<TypeParam>{}};
    std::vector<TypeParam> expected;
    // operator= returns *this so assignments can be chained.
    EXPECT_EQ(&(actual = {1, 0, 1, 1, 0, 0, 1, 0, 1}), &actual);
    expected = {1, 0, 1, 1, 0, 0, 1, 0, 1};
    expect_sequence(actual, expected);
    const auto capacity = actual.capacity();
    actual              = {0, 1};
    expected            = {0, 1};
    expect_sequence(actual, expected);
    EXPECT_EQ(actual.capacity(), capacity);
    // An empty braced list must select the initializer-list overload and clear the array.
    actual = {};
    EXPECT_TRUE(actual.empty());
    EXPECT_EQ(actual.capacity(), capacity);
    actual   = {1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1};
    expected = {1, 1, 0, 1, 0, 0, 1, 1, 0, 1, 0, 1, 1, 0, 0, 1, 1};
    expect_sequence(actual, expected);
    actual.push_back(TypeParam{});
    expected.push_back(TypeParam{});
    expect_sequence(actual, expected);
}

TYPED_TEST(VLAAssign, InitializerListOperatorHonorsLogicalMaximum)
{
    Array<TypeParam> actual{1, std::allocator<TypeParam>{}};
#if defined(__cpp_exceptions)
    actual              = {0};
    const auto capacity = actual.capacity();
    EXPECT_THROW((actual = {1, 0}), std::length_error);
    expect_sequence(actual, std::vector<TypeParam>{0});
    EXPECT_EQ(actual.capacity(), capacity);
#else
    actual = {1, 0};
    expect_sequence(actual, std::vector<TypeParam>{1});
#endif
    EXPECT_EQ(actual.max_size(), 1U);
}

struct AllocationState
{
    bool        decline = false;
    std::size_t calls   = 0;
    std::size_t live    = 0;
    std::size_t peak    = 0;
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
        if (state->decline)
        {
            return nullptr;
        }
        state->peak = std::max(state->peak, ++state->live);
        return std::allocator<T>{}.allocate(count);
    }
    void deallocate(T* pointer, std::size_t count) noexcept
    {
        // Allocators may require a pointer previously returned by allocate(); arrays without storage must not call.
        EXPECT_NE(pointer, nullptr);
        --state->live;
        std::allocator<T>{}.deallocate(pointer, count);
    }
};

TYPED_TEST(VLAAssign, ReusesStorageAndRetainsAllocatorAndMaximum)
{
    AllocationState state;
    using Subject = cetl::variable_length_array<TypeParam, Allocator<TypeParam>>;
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
    actual = {1, 1, 0};
    expect_sequence(actual, std::vector<TypeParam>{1, 1, 0});
    actual = {};
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
        using Subject = cetl::variable_length_array<TypeParam, Allocator<TypeParam>>;
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
    using Subject = cetl::variable_length_array<TypeParam, Allocator<TypeParam>>;
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

TYPED_TEST(VLAAssign, EveryAssignOverloadReleasesBeforeGrowing)
{
    // A bounded resource may permit only one outstanding block; no assign overload may hold two.
    AllocationState state;
    using Subject = cetl::variable_length_array<TypeParam, Allocator<TypeParam>>;
    Subject     actual{typename Subject::allocator_type{state}};
    std::size_t count = 4;
    for (int overload = 0; overload < 3; ++overload)
    {
        SCOPED_TRACE(overload);
        count *= 4;
        actual.assign(count / 4, TypeParam{});
        const std::vector<int> source(count, 1);
        if (overload == 0)
        {
            actual.assign(count, TypeParam{1});
        }
        else if (overload == 1)
        {
            actual.assign(source.begin(), source.end());
        }
        else
        {
            const std::forward_list<int> list(source.begin(), source.end());
            actual.assign(list.begin(), list.end());
        }
        expect_sequence(actual, std::vector<TypeParam>(count, TypeParam{1}));
        EXPECT_EQ(state.peak, 1U);
    }
}

TYPED_TEST(VLAAssign, GrowthBeyondInheritedMaximumIsRejected)
{
    // Assignment adopts the source's smaller bound while keeping this array's larger capacity.
    Array<TypeParam> actual{std::allocator<TypeParam>{}};
    actual.reserve(16);
    Array<TypeParam> bounded{3, std::allocator<TypeParam>{}};
    bounded.push_back(TypeParam{1});
    actual = bounded;
    ASSERT_EQ(actual.max_size(), 3U);
    ASSERT_GE(actual.capacity(), 16U);
    actual.push_back(TypeParam{1});
    actual.push_back(TypeParam{1});
    std::istringstream input{"1 1 1 1 1"};
#if defined(__cpp_exceptions)
    EXPECT_THROW(actual.push_back(TypeParam{1}), std::length_error);
    EXPECT_THROW(actual.resize(8), std::length_error);
    EXPECT_THROW(actual.assign(7, TypeParam{1}), std::length_error);
    EXPECT_THROW(actual.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{}), std::length_error);
#else
    actual.push_back(TypeParam{1});
    actual.resize(8);
    actual.assign(7, TypeParam{1});
    actual.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{});
#endif
    expect_sequence(actual, std::vector<TypeParam>(3, TypeParam{1}));
}

TYPED_TEST(VLAAssign, DeclinedFillGrowthStaysWithinStorage)
{
    AllocationState state;
    using Subject = cetl::variable_length_array<TypeParam, Allocator<TypeParam>>;
    Subject actual{typename Subject::allocator_type{state}};
    actual.push_back(TypeParam{1});
    const auto capacity = actual.capacity();
    state.decline       = true;
#if defined(__cpp_exceptions)
    EXPECT_THROW(actual.resize(200, TypeParam{1}), std::bad_alloc);
    expect_sequence(actual, std::vector<TypeParam>(1, TypeParam{1}));
    // Fill assignment releases insufficient storage before allocating, as range assignment does.
    EXPECT_THROW(actual.assign(200, TypeParam{1}), std::bad_alloc);
#else
    actual.resize(200, TypeParam{1});
    expect_sequence(actual, std::vector<TypeParam>(capacity, TypeParam{1}));
    actual.assign(200, TypeParam{1});
#endif
    EXPECT_TRUE(actual.empty());
    EXPECT_EQ(actual.capacity(), 0U);
    (void) capacity;
}

TEST(VLAAssignObjects, ImplicitConversionsCompileCleanly)
{
    // The CETL warning set (-Wconversion -Wsign-conversion -Werror) must accept what std::vector::assign accepts.
    const std::vector<int> source{1, 2, 300};
    Array<std::uint8_t>    narrow{std::allocator<std::uint8_t>{}};
    narrow.assign(source.begin(), source.end());
    expect_sequence(narrow, std::vector<std::uint8_t>(source.begin(), source.end()));
    const std::forward_list<int> list(source.begin(), source.end());
    narrow.assign(list.begin(), list.end());
    expect_sequence(narrow, std::vector<std::uint8_t>(source.begin(), source.end()));
    std::istringstream input{"1 2 300"};
    narrow.assign(std::istream_iterator<int>{input}, std::istream_iterator<int>{});
    expect_sequence(narrow, std::vector<std::uint8_t>(source.begin(), source.end()));
    const std::array<double, 3> real{{0.0, 0.5, 2.0}};
    Array<float>                single{std::allocator<float>{}};
    single.assign(real.begin(), real.end());
    expect_sequence(single, std::vector<float>(real.begin(), real.end()));
    Array<bool> bits{std::allocator<bool>{}};
    bits.assign(real.begin(), real.end());
    expect_sequence(bits, std::vector<bool>(real.begin(), real.end()));
}

// An iterator whose namespace deletes the comma operator, as libc++'s operator_hijacker does.
template <typename Category>
struct CommaHijacker
{
    using iterator_category = Category;
    using value_type        = int;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const int*;
    using reference         = const int&;

    const int* data;
    reference  operator*() const
    {
        return *data;
    }
    CommaHijacker& operator++()
    {
        ++data;
        return *this;
    }
    CommaHijacker operator++(int)
    {
        const auto old = *this;
        ++data;
        return old;
    }
    bool operator==(const CommaHijacker& rhs) const
    {
        return data == rhs.data;
    }
    bool operator!=(const CommaHijacker& rhs) const
    {
        return !(*this == rhs);
    }
    template <typename T>
    friend void operator,(const CommaHijacker&, T&&) = delete;
};

template <typename Category>
void check_comma_hijacker()
{
    const std::array<int, 9>      source{{1, 0, 1, 1, 0, 0, 1, 0, 1}};
    const CommaHijacker<Category> first{source.data()};
    const CommaHijacker<Category> last{source.data() + source.size()};
    Array<int>                    numbers{first, last, std::allocator<int>{}};
    numbers.assign(first, last);
    expect_sequence(numbers, source);
    Array<bool> bits{first, last, std::allocator<bool>{}};
    bits.assign(first, last);
    expect_sequence(bits, std::vector<bool>(source.begin(), source.end()));
}

TEST(VLAAssignObjects, OverloadedCommaOperatorIsNotUsed)
{
    check_comma_hijacker<std::input_iterator_tag>();
    check_comma_hijacker<std::forward_iterator_tag>();
}

TEST(VLAAssignObjects, RangeConstructorsAcceptAssignableRanges)
{
    const std::forward_list<int> list{1, 0, 1};
    const Array<int>             numbers{list.begin(), list.end(), std::allocator<int>{}};
    expect_sequence(numbers, std::vector<int>(list.begin(), list.end()));
    const Array<bool> bits{list.begin(), list.end(), std::allocator<bool>{}};
    expect_sequence(bits, std::vector<bool>(list.begin(), list.end()));
    std::istringstream input{"4 5 6"};
    const Array<int>   streamed{std::istream_iterator<int>{input}, std::istream_iterator<int>{}, std::allocator<int>{}};
    expect_sequence(streamed, std::vector<int>{4, 5, 6});
}
// Copy-constructible but not assignable, like a type with a const member. std::vector's range constructor accepts it.
struct NoAssign
{
    const std::string text;
    explicit NoAssign(const int value)
        : text(std::to_string(value))
    {
    }
    NoAssign(const NoAssign&)            = default;
    NoAssign& operator=(const NoAssign&) = delete;
};

TEST(VLAAssignObjects, RangeConstructorsDoNotRequireAssignableElements)
{
    const NoAssign           objects[] = {NoAssign{1}, NoAssign{2}, NoAssign{3}};
    const Array<NoAssign>    copied{std::begin(objects), std::end(objects), std::allocator<NoAssign>{}};
    const std::array<int, 3> values{{1, 2, 3}};
    const CommaHijacker<std::forward_iterator_tag> forward_first{values.data()};
    const CommaHijacker<std::forward_iterator_tag> forward_last{values.data() + values.size()};
    const Array<NoAssign>                          forward{forward_first, forward_last, std::allocator<NoAssign>{}};
    const CommaHijacker<std::input_iterator_tag>   input_first{values.data()};
    const CommaHijacker<std::input_iterator_tag>   input_last{values.data() + values.size()};
    const Array<NoAssign>                          single_pass{input_first, input_last, std::allocator<NoAssign>{}};
    for (const Array<NoAssign>* actual : {&copied, &forward, &single_pass})
    {
        ASSERT_EQ(actual->size(), 3U);
        for (std::size_t i = 0; i < 3; ++i)
        {
            EXPECT_EQ((*actual)[i].text, std::to_string(i + 1));
        }
    }
}

// Integers select the count-value constructor instead, as for std::vector (see
// test_variable_length_array_construct.cpp), so use arguments that are neither iterators nor counts.
static_assert(!std::is_constructible<Array<int>, std::nullptr_t, std::nullptr_t, std::allocator<int>>::value,
              "Non-iterators must not select the range constructor.");
static_assert(!std::is_constructible<Array<bool>, std::nullptr_t, std::nullptr_t, std::allocator<bool>>::value,
              "Non-iterators must not select the range constructor.");

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
