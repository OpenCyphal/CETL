/// @file
/// Count-value construction and its overload resolution against the other VLA constructors.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

namespace
{
template <typename T>
using Array = cetl::variable_length_array<T, std::allocator<T>>;

template <typename T>
class VLAConstruct : public ::testing::Test
{};
using ElementTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLAConstruct, ElementTypes, );

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

// The storage a count-value constructor obtains: exactly count elements, or whole bytes of packed bits.
template <typename T>
std::size_t exact_capacity(const std::size_t count)
{
    return std::is_same<T, bool>::value ? ((count + 7U) / 8U) * 8U : count;
}

enum UnscopedCount : unsigned
{
    Three = 3U
};

TYPED_TEST(VLAConstruct, CountValueMatchesVector)
{
    const std::allocator<TypeParam> alloc{};
    // Cross byte boundaries and leave a partial final byte in the bool specialization.
    for (const std::size_t count : {0U, 1U, 7U, 8U, 9U, 17U})
    {
        for (const int source : {0, 1})
        {
            SCOPED_TRACE(::testing::Message() << "count=" << count << " source=" << source);
            const auto                   value = static_cast<TypeParam>(source);
            const Array<TypeParam>       actual(count, value, alloc);
            const std::vector<TypeParam> expected(count, value, alloc);
            expect_sequence(actual, expected);
            EXPECT_EQ(actual.capacity(), exact_capacity<TypeParam>(count));
            EXPECT_EQ(actual.max_size(), Array<TypeParam>(alloc).max_size());
        }
    }
}

TYPED_TEST(VLAConstruct, CountValueWithMaxSize)
{
    const std::allocator<TypeParam> alloc{};
    const auto                      value = static_cast<TypeParam>(1);
    const Array<TypeParam>          actual(5U, value, 10U, alloc);
    expect_sequence(actual, std::vector<TypeParam>(5U, value));
    EXPECT_EQ(actual.max_size(), 10U);

    const Array<TypeParam> full(10U, value, 10U, alloc);
    expect_sequence(full, std::vector<TypeParam>(10U, value));
    EXPECT_EQ(full.max_size(), 10U);

    // Construction keeps the bound for later growth.
    Array<TypeParam> bounded(10U, value, 10U, alloc);
#if defined(__cpp_exceptions)
    EXPECT_THROW(bounded.push_back(value), std::length_error);
#else
    bounded.push_back(value);
#endif
    EXPECT_EQ(bounded.size(), 10U);
}

TYPED_TEST(VLAConstruct, CountBeyondMaxSize)
{
    const std::allocator<TypeParam> alloc{};
    const auto                      value = static_cast<TypeParam>(1);
#if defined(__cpp_exceptions)
    EXPECT_THROW((Array<TypeParam>(11U, value, 10U, alloc)), std::length_error);
#else
    // Without exceptions the count is clamped to max_size(), which the caller detects by checking size().
    const Array<TypeParam> clamped(11U, value, 10U, alloc);
    expect_sequence(clamped, std::vector<TypeParam>(10U, value));
#endif
}

TYPED_TEST(VLAConstruct, CountAcceptsTypesConvertibleToSizeType)
{
    const std::allocator<TypeParam> alloc{};
    const auto                      value = static_cast<TypeParam>(1);
    const std::vector<TypeParam>    expected(3U, value);
    expect_sequence(Array<TypeParam>(3, value, alloc), expected);
    expect_sequence(Array<TypeParam>(static_cast<unsigned char>(3), value, alloc), expected);
    expect_sequence(Array<TypeParam>(std::size_t{3}, value, alloc), expected);
    expect_sequence(Array<TypeParam>(std::uint64_t{3}, value, alloc), expected);
    expect_sequence(Array<TypeParam>(Three, value, alloc), expected);
    expect_sequence(Array<TypeParam>(Three, value, 10, alloc), expected);
}

// The count-value constructors must not change which constructor the existing argument lists select.
TYPED_TEST(VLAConstruct, OverloadResolution)
{
    const std::allocator<TypeParam> alloc{};
    const auto                      value = static_cast<TypeParam>(1);

    // (n, alloc) is still the max_size_max constructor, unlike std::vector's (count, alloc).
    const Array<TypeParam> bounded(5, alloc);
    EXPECT_TRUE(bounded.empty());
    EXPECT_EQ(bounded.max_size(), 5U);

    // A braced first argument is still an initializer list, even when an int literal max_size_max converts better
    // to value_type than to size_type.
    const Array<TypeParam> list({value}, 5, alloc);
    expect_sequence(list, std::vector<TypeParam>{value});
    EXPECT_EQ(list.max_size(), 5U);
    const Array<TypeParam> braced_list{{value}, 5, alloc};
    expect_sequence(braced_list, std::vector<TypeParam>{value});
    EXPECT_EQ(braced_list.max_size(), 5U);

    // Braced initialization falls back to count-value when the list cannot be an initializer list of elements.
    const Array<TypeParam> braced_count{5, value, alloc};
    expect_sequence(braced_count, std::vector<TypeParam>(5U, value));
    const Array<TypeParam> braced_count_bounded{5, value, 10, alloc};
    expect_sequence(braced_count_bounded, std::vector<TypeParam>(5U, value));
    EXPECT_EQ(braced_count_bounded.max_size(), 10U);

    // Iterators still select the range constructors.
    const std::array<TypeParam, 3> source{{value, TypeParam{}, value}};
    expect_sequence(Array<TypeParam>(source.begin(), source.end(), alloc), source);
    const Array<TypeParam> range(source.begin(), source.end(), 10, alloc);
    expect_sequence(range, source);
    EXPECT_EQ(range.max_size(), 10U);
}

// When the element type is size_type, (count, value) and (first, last) arguments have the same type. As for
// std::vector, integers select count-value and pointers select the range constructor.
TEST(VLAConstruct, SizeTypeElements)
{
    const std::allocator<std::size_t> alloc{};
    expect_sequence(Array<std::size_t>(3, 4, alloc), std::vector<std::size_t>(3, 4));
    expect_sequence(Array<std::size_t>(std::size_t{3}, std::size_t{4}, alloc), std::vector<std::size_t>(3, 4));
    expect_sequence(Array<std::size_t>(3, 4, 10, alloc), std::vector<std::size_t>(3, 4));
    expect_sequence(Array<std::size_t>{3, 4, alloc}, std::vector<std::size_t>(3, 4));

    const std::array<std::size_t, 2> source{{3, 4}};
    expect_sequence(Array<std::size_t>(source.data(), source.data() + source.size(), alloc), source);

    const Array<std::size_t> list({3}, 4, alloc);
    expect_sequence(list, std::vector<std::size_t>{3});
    EXPECT_EQ(list.max_size(), 4U);
}

// An input iterator that also converts to size_type, over a sequence of Reference values produced from its position.
// Without excluding input iterators from the count, VLA<SelfIterator>(first, last, alloc) is ambiguous between the
// range and count-value constructors.
template <typename Reference>
struct ConvertibleIterator
{
    using iterator_category = std::input_iterator_tag;
    using value_type        = Reference;
    using difference_type   = std::ptrdiff_t;
    using pointer           = const Reference*;
    using reference         = Reference;

    std::size_t position;

    Reference operator*() const
    {
        return Reference(position);
    }
    ConvertibleIterator& operator++()
    {
        ++position;
        return *this;
    }
    ConvertibleIterator operator++(int)
    {
        const ConvertibleIterator old = *this;
        ++position;
        return old;
    }
    bool operator==(const ConvertibleIterator& rhs) const
    {
        return position == rhs.position;
    }
    bool operator!=(const ConvertibleIterator& rhs) const
    {
        return position != rhs.position;
    }
    operator std::size_t() const
    {
        return position;
    }
};

// An element type that is itself such an iterator, as the review of #63 described.
struct SelfIterator : ConvertibleIterator<SelfIterator>
{
    explicit SelfIterator(const std::size_t at)
        : ConvertibleIterator<SelfIterator>{at}
    {
    }
};

// Odd positions yield true.
struct Bit
{
    explicit Bit(const std::size_t at)
        : value((at % 2U) != 0U)
    {
    }
    operator bool() const
    {
        return value;
    }
    bool value;
};

// As for std::vector, (first, last) always selects the range constructor, even when the iterator converts to a count.
TEST(VLAConstruct, IteratorsConvertibleToSizeTypeSelectRange)
{
    const std::allocator<SelfIterator> alloc{};
    const SelfIterator                 first{1};
    const SelfIterator                 last{4};
    const std::vector<SelfIterator>    expected(first, last, alloc);
    ASSERT_EQ(expected.size(), 3U);
    expect_sequence(Array<SelfIterator>(first, last, alloc), expected);
    const Array<SelfIterator> bounded(first, last, 10, alloc);
    expect_sequence(bounded, expected);
    EXPECT_EQ(bounded.max_size(), 10U);

    const std::allocator<bool>     bool_alloc{};
    const ConvertibleIterator<Bit> bit_first{1};
    const ConvertibleIterator<Bit> bit_last{4};
    const std::vector<bool>        bits(bit_first, bit_last, bool_alloc);
    expect_sequence(Array<bool>(bit_first, bit_last, bool_alloc), bits);
    expect_sequence(Array<bool>(bit_first, bit_last, 10, bool_alloc), bits);
}

TEST(VLAConstruct, PointerElements)
{
    const std::allocator<int*> alloc{};
    std::array<int, 2>         targets{{1, 2}};
    std::array<int*, 2>        source{{&targets[0], &targets[1]}};
    expect_sequence(Array<int*>(source.begin(), source.end(), alloc), source);
    expect_sequence(Array<int*>(2, &targets[1], alloc), std::vector<int*>(2, &targets[1]));
    expect_sequence(Array<int*>(2, nullptr, alloc), std::vector<int*>(2, nullptr));
}

TEST(VLAConstruct, NonTrivialElements)
{
    const std::allocator<std::string> alloc{};
    // The value converts to the element type, as it would for std::vector.
    const Array<std::string> actual(4, "a string long enough to need its own heap allocation", alloc);
    expect_sequence(actual, std::vector<std::string>(4, "a string long enough to need its own heap allocation", alloc));
    EXPECT_EQ(actual.capacity(), 4U);
}

static_assert(std::is_constructible<Array<int>, int, int, const std::allocator<int>&>::value,
              "count-value construction");
static_assert(std::is_constructible<Array<int>, int, int, int, const std::allocator<int>&>::value,
              "count-value construction with max_size_max");
static_assert(!std::is_constructible<Array<int>, int*, int, const std::allocator<int>&>::value,
              "a count must convert to size_type");
static_assert(!std::is_constructible<Array<int>, int, int*, const std::allocator<int>&>::value,
              "the value must convert to value_type");

}  // namespace
