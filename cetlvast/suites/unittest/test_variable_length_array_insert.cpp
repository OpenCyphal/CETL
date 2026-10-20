/// @file
/// insert(), checked against std::vector.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <forward_list>
#include <iterator>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

namespace
{
template <typename T>
using Array = cetl::variable_length_array<T, std::allocator<T>>;

template <typename T>
class VLAInsert : public ::testing::Test
{};
using ElementTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLAInsert, ElementTypes, );

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

// An irregular pattern, so a misplaced element changes the sequence.
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

// Every insertion position of arrays that end mid-byte, on a byte boundary, and past one in the bool specialization,
// for each overload and several counts.
TYPED_TEST(VLAInsert, AllOverloadsMatchVector)
{
    for (const std::size_t size : {0U, 1U, 7U, 8U, 9U, 17U})
    {
        for (std::size_t at = 0; at <= size; ++at)
        {
            for (const std::size_t count : {0U, 1U, 3U, 8U, 9U})
            {
                std::vector<TypeParam> source;
                for (std::size_t i = 0; i < count; ++i)
                {
                    source.push_back(static_cast<TypeParam>(1U - static_cast<unsigned>(pattern<TypeParam>(i + 3U))));
                }
                const auto offset = static_cast<std::ptrdiff_t>(at);
                for (int overload = 0; overload < 6; ++overload)
                {
                    SCOPED_TRACE(::testing::Message()
                                 << "size=" << size << " at=" << at << " count=" << count << " overload=" << overload);
                    auto  subjects = make_pair_of<TypeParam>(size);
                    auto& actual   = subjects.first;
                    auto& expected = subjects.second;
                    // Each insert's result is stored before it is compared with begin(): the order in which an
                    // expression's operands are evaluated is unspecified, and insert() may reallocate.
                    typename Array<TypeParam>::iterator       result{};
                    typename std::vector<TypeParam>::iterator oracle{};
                    switch (overload)
                    {
                    case 0:  // count copies
                        result = actual.insert(actual.cbegin() + offset, count, TypeParam{1});
                        oracle = expected.insert(expected.cbegin() + offset, count, TypeParam{1});
                        break;
                    case 1:  // random-access range
                        result = actual.insert(actual.cbegin() + offset, source.begin(), source.end());
                        oracle = expected.insert(expected.cbegin() + offset, source.begin(), source.end());
                        break;
                    case 2:  // forward range
                    {
                        const std::forward_list<TypeParam> list(source.begin(), source.end());
                        result = actual.insert(actual.cbegin() + offset, list.begin(), list.end());
                        oracle = expected.insert(expected.cbegin() + offset, list.begin(), list.end());
                        break;
                    }
                    case 3:  // single-pass range
                    {
                        std::ostringstream text;
                        for (const TypeParam value : source)
                        {
                            text << static_cast<int>(value) << ' ';
                        }
                        std::istringstream actual_input(text.str());
                        std::istringstream expected_input(text.str());
                        result = actual.insert(actual.cbegin() + offset,
                                               std::istream_iterator<int>(actual_input),
                                               std::istream_iterator<int>());
                        oracle = expected.insert(expected.cbegin() + offset,
                                                 std::istream_iterator<int>(expected_input),
                                                 std::istream_iterator<int>());
                        break;
                    }
                    case 4:  // one copy
                    {
                        const TypeParam value{0};
                        result = actual.insert(actual.cbegin() + offset, value);
                        oracle = expected.insert(expected.cbegin() + offset, value);
                        break;
                    }
                    default:  // initializer list
                        result = actual.insert(actual.cbegin() + offset, {TypeParam{1}, TypeParam{0}, TypeParam{1}});
                        oracle =
                            expected.insert(expected.cbegin() + offset, {TypeParam{1}, TypeParam{0}, TypeParam{1}});
                        break;
                    }
                    EXPECT_EQ(result - actual.begin(), oracle - expected.begin());
                    expect_sequence(actual, expected);
                    EXPECT_GE(actual.capacity(), actual.size());

                    // The array must remain usable, including the packed-bit bookkeeping at the new end.
                    actual.push_back(TypeParam{1});
                    expected.push_back(TypeParam{1});
                    expect_sequence(actual, expected);
                }
            }
        }
    }
}

TYPED_TEST(VLAInsert, MutableIteratorPositions)
{
    auto  subjects = make_pair_of<TypeParam>(5U);
    auto& actual   = subjects.first;
    auto& expected = subjects.second;
    (void) actual.insert(actual.begin() + 2, TypeParam{1});
    (void) expected.insert(expected.begin() + 2, TypeParam{1});
    (void) actual.insert(actual.end(), 2U, TypeParam{0});
    (void) expected.insert(expected.end(), 2U, TypeParam{0});
    expect_sequence(actual, expected);
}

// Nothing to insert leaves an array that has never allocated without storage.
TYPED_TEST(VLAInsert, EmptyInsertionsHaveNoEffect)
{
    Array<TypeParam>             empty{std::allocator<TypeParam>{}};
    const std::vector<TypeParam> none;
    const auto                   position = empty.insert(empty.cbegin(), 0U, TypeParam{1});
    EXPECT_TRUE(position == empty.end());
    const auto position2 = empty.insert(empty.cbegin(), none.begin(), none.end());
    EXPECT_TRUE(position2 == empty.end());
    const auto position3 = empty.insert(empty.cbegin(), {});
    EXPECT_TRUE(position3 == empty.end());
    EXPECT_TRUE(empty.empty());
    EXPECT_EQ(empty.capacity(), 0U);

    auto       subjects  = make_pair_of<TypeParam>(6U);
    auto&      actual    = subjects.first;
    const auto position4 = actual.insert(actual.cbegin() + 3, 0U, TypeParam{1});
    EXPECT_TRUE(position4 == actual.begin() + 3);
    expect_sequence(actual, subjects.second);
}

// Repeated insertion grows capacity geometrically, as push_back() does, rather than by the inserted count.
TYPED_TEST(VLAInsert, RepeatedInsertionGrowsGeometrically)
{
    Array<TypeParam> actual{std::allocator<TypeParam>{}};
    std::size_t      growths  = 0;
    std::size_t      capacity = actual.capacity();
    for (std::size_t i = 0; i < 200U; ++i)
    {
        (void) actual.insert(actual.cbegin(), pattern<TypeParam>(i));
        if (actual.capacity() != capacity)
        {
            ++growths;
            capacity = actual.capacity();
        }
    }
    EXPECT_EQ(actual.size(), 200U);
    EXPECT_LT(growths, 20U);
}

TYPED_TEST(VLAInsert, MaxSizeIsEnforced)
{
    Array<TypeParam>             actual(5U, TypeParam{1}, 8U, std::allocator<TypeParam>{});
    const std::vector<TypeParam> four(4U, TypeParam{0});
#if defined(__cpp_exceptions)
    EXPECT_THROW((void) actual.insert(actual.cbegin(), 4U, TypeParam{0}), std::length_error);
    EXPECT_THROW((void) actual.insert(actual.cbegin(), four.begin(), four.end()), std::length_error);
    expect_sequence(actual, std::vector<TypeParam>(5U, TypeParam{1}));

    // A single-pass range is only found to be too long while appending; the appended elements are removed.
    std::istringstream input("0 0 0 0");
    EXPECT_THROW((void) actual.insert(actual.cbegin(), std::istream_iterator<int>(input), std::istream_iterator<int>()),
                 std::length_error);
    expect_sequence(actual, std::vector<TypeParam>(5U, TypeParam{1}));
#else
    // Without exceptions a sized insertion that does not fit has no effect...
    const auto position5 = actual.insert(actual.cbegin() + 1, 4U, TypeParam{0});
    EXPECT_TRUE(position5 == actual.begin() + 1);
    const auto position6 = actual.insert(actual.cbegin() + 1, four.begin(), four.end());
    EXPECT_TRUE(position6 == actual.begin() + 1);
    expect_sequence(actual, std::vector<TypeParam>(5U, TypeParam{1}));

    // ...while a single-pass range keeps the prefix that fits, as assign(first, last) does.
    std::istringstream input("0 0 0 0");
    (void) actual.insert(actual.cbegin(), std::istream_iterator<int>(input), std::istream_iterator<int>());
    expect_sequence(actual,
                    std::vector<TypeParam>{TypeParam{0},
                                           TypeParam{0},
                                           TypeParam{0},
                                           TypeParam{1},
                                           TypeParam{1},
                                           TypeParam{1},
                                           TypeParam{1},
                                           TypeParam{1}});
#endif
    // Exactly reaching max_size() is allowed.
    (void) actual.insert(actual.cend(), 8U - actual.size(), TypeParam{0});
    EXPECT_EQ(actual.size(), 8U);
}

// An allocator that counts its calls and can decline to allocate by returning null.
struct AllocationState
{
    bool        decline = false;
    std::size_t calls   = 0;
};

template <typename T>
struct DecliningAllocator
{
    using value_type = T;
    AllocationState* state;
    explicit DecliningAllocator(AllocationState& s) noexcept
        : state(&s)
    {
    }
    template <typename U>
    DecliningAllocator(const DecliningAllocator<U>& other) noexcept
        : state(other.state)
    {
    }
    T* allocate(const std::size_t count)
    {
        ++state->calls;
        return state->decline ? nullptr : std::allocator<T>{}.allocate(count);
    }
    void deallocate(T* const pointer, const std::size_t count) noexcept
    {
        std::allocator<T>{}.deallocate(pointer, count);
    }
};

// Inserting within the existing capacity never allocates.
TYPED_TEST(VLAInsert, InsertingWithinCapacityDoesNotAllocate)
{
    AllocationState state;
    using Subject = cetl::variable_length_array<TypeParam, DecliningAllocator<TypeParam>>;
    Subject actual{typename Subject::allocator_type{state}};
    actual.reserve(40U);
    const auto                   calls = state.calls;
    const std::vector<TypeParam> source(5U, TypeParam{1});
    (void) actual.insert(actual.cbegin(), 3U, TypeParam{0});
    (void) actual.insert(actual.cbegin() + 1, source.begin(), source.end());
    (void) actual.insert(actual.cend(), {TypeParam{1}, TypeParam{0}});
    EXPECT_EQ(actual.size(), 10U);
    EXPECT_EQ(state.calls, calls);
}

// When the allocator declines to grow, an insertion has no effect: std::bad_alloc with exceptions, and an unchanged
// size() without them.
TYPED_TEST(VLAInsert, DeclinedGrowthHasNoEffect)
{
    AllocationState state;
    using Subject = cetl::variable_length_array<TypeParam, DecliningAllocator<TypeParam>>;
    Subject actual{typename Subject::allocator_type{state}};
    actual.push_back(TypeParam{1});
    actual.shrink_to_fit();
    const auto                   capacity = actual.capacity();
    const std::vector<TypeParam> source(64U, TypeParam{0});
    state.decline = true;
#if defined(__cpp_exceptions)
    EXPECT_THROW((void) actual.insert(actual.cbegin(), 64U, TypeParam{0}), std::bad_alloc);
    EXPECT_THROW((void) actual.insert(actual.cbegin(), source.begin(), source.end()), std::bad_alloc);
#else
    const auto position7 = actual.insert(actual.cbegin(), 64U, TypeParam{0});
    EXPECT_TRUE(position7 == actual.begin());
    const auto position8 = actual.insert(actual.cbegin(), source.begin(), source.end());
    EXPECT_TRUE(position8 == actual.begin());
#endif
    expect_sequence(actual, std::vector<TypeParam>{TypeParam{1}});
    EXPECT_EQ(actual.capacity(), capacity);
}

// The CETL warning set (-Wconversion -Wsign-conversion -Werror) must accept the conversions std::vector::insert does.
TEST(VLAInsert, ImplicitConversionsCompileCleanly)
{
    const std::vector<int>    source{1, 2, 300};
    Array<std::uint8_t>       actual{std::allocator<std::uint8_t>{}};
    std::vector<std::uint8_t> expected;
    (void) actual.insert(actual.cbegin(), source.begin(), source.end());
    (void) expected.insert(expected.cbegin(), source.begin(), source.end());
    std::istringstream actual_input("7 8");
    std::istringstream expected_input("7 8");
    (void) actual.insert(actual.cbegin() + 1, std::istream_iterator<int>(actual_input), std::istream_iterator<int>());
    (void) expected.insert(expected.cbegin() + 1,
                           std::istream_iterator<int>(expected_input),
                           std::istream_iterator<int>());
    expect_sequence(actual, expected);
}

// Integers select the count-value overload and a braced list the initializer-list overload, as for std::vector.
TEST(VLAInsert, OverloadResolution)
{
    Array<int>       actual{std::allocator<int>{}};
    std::vector<int> expected;
    (void) actual.insert(actual.cbegin(), 3, 4);
    (void) expected.insert(expected.cbegin(), 3, 4);
    (void) actual.insert(actual.cbegin() + 1, {5});
    (void) expected.insert(expected.cbegin() + 1, {5});
    (void) actual.insert(actual.cbegin(), {6, 7});
    (void) expected.insert(expected.cbegin(), {6, 7});
    expect_sequence(actual, expected);
}

// The value may be an element of the array, even when inserting reallocates and relocates it.
TEST(VLAInsert, ValueMayBeAnElement)
{
    const std::string long_text = "a string long enough to need its own heap allocation";
    for (const bool reallocate : {true, false})
    {
        SCOPED_TRACE(reallocate);
        Array<std::string> actual{std::allocator<std::string>{}};
        actual.push_back(long_text);
        actual.push_back("short");
        if (reallocate)
        {
            actual.shrink_to_fit();
        }
        else
        {
            actual.reserve(16U);
        }
        (void) actual.insert(actual.cbegin(), actual[0]);
        (void) actual.insert(actual.cbegin() + 1, 3U, actual.back());
        expect_sequence(actual, std::vector<std::string>{long_text, "short", "short", "short", long_text, "short"});
    }
}

TEST(VLAInsert, MoveOnlyElements)
{
    Array<std::unique_ptr<int>> actual{std::allocator<std::unique_ptr<int>>{}};
    actual.push_back(std::make_unique<int>(1));
    actual.push_back(std::make_unique<int>(4));
    (void) actual.insert(actual.cbegin() + 1, std::make_unique<int>(2));
    std::vector<std::unique_ptr<int>> more;
    more.push_back(std::make_unique<int>(3));
    (void) actual.insert(actual.cbegin() + 2,
                         std::make_move_iterator(more.begin()),
                         std::make_move_iterator(more.end()));
    ASSERT_EQ(actual.size(), 4U);
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        EXPECT_EQ(*actual[i], static_cast<int>(i + 1U));
    }
}

// Inserting a range only constructs elements from it, as std::vector requires: an element type that is constructible,
// but not assignable, from the source works.
struct FromInt
{
    explicit FromInt(const int v)
        : value(v)
    {
    }
    int value;
};

TEST(VLAInsert, ElementsNeedOnlyBeConstructibleFromTheSource)
{
    static_assert(!std::is_assignable<FromInt&, int>::value, "the test needs a type not assignable from int");
    const std::vector<int> source{2, 3};
    Array<FromInt>         actual{std::allocator<FromInt>{}};
    actual.emplace_back(1);
    actual.emplace_back(4);
    (void) actual.insert(actual.cbegin() + 1, source.begin(), source.end());
    ASSERT_EQ(actual.size(), 4U);
    for (std::size_t i = 0; i < actual.size(); ++i)
    {
        EXPECT_EQ(actual[i].value, static_cast<int>(i + 1U));
    }
}

}  // namespace
