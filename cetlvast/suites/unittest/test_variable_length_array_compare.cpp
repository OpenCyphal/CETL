/// @file
/// Equality and lexicographic ordering, checked against std::vector.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <cstddef>
#include <limits>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>
#if __cplusplus >= 202002L
#    include <compare>
#endif

namespace
{
template <typename T>
using Array = cetl::VariableLengthArray<T, std::allocator<T>>;

template <typename T>
class VLACompare : public ::testing::Test
{};
using ElementTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLACompare, ElementTypes, );

// Sequences that differ at the first, a middle, and the last element, share prefixes of every length, and cross
// packed-byte boundaries: every bit pattern of up to five elements, then longer runs with one element changed.
template <typename T>
std::vector<std::vector<T>> make_sequences()
{
    std::vector<std::vector<T>> sequences;
    for (std::size_t length = 0; length <= 5U; ++length)
    {
        for (std::size_t bits = 0; bits < (std::size_t{1} << length); ++bits)
        {
            std::vector<T> sequence;
            for (std::size_t i = 0; i < length; ++i)
            {
                sequence.push_back(static_cast<T>((bits >> i) & 1U));
            }
            sequences.push_back(sequence);
        }
    }
    for (const std::size_t length : {7U, 8U, 9U, 16U, 17U})
    {
        for (const std::size_t changed : {std::size_t{0}, length / 2U, length - 1U})
        {
            for (const int fill : {0, 1})
            {
                std::vector<T> sequence(length, static_cast<T>(fill));
                sequence[changed] = static_cast<T>(1 - fill);
                sequences.push_back(sequence);
            }
        }
        sequences.emplace_back(length, static_cast<T>(1));
    }
    return sequences;
}

template <typename T>
Array<T> to_array(const std::vector<T>& source)
{
    return Array<T>(source.begin(), source.end(), std::allocator<T>{});
}

TYPED_TEST(VLACompare, AllOperatorsMatchVector)
{
    const auto sequences = make_sequences<TypeParam>();
    for (const auto& left : sequences)
    {
        const Array<TypeParam> lhs = to_array(left);
        for (const auto& right : sequences)
        {
            const Array<TypeParam> rhs = to_array(right);
            SCOPED_TRACE(::testing::Message() << "left size=" << left.size() << " right size=" << right.size());
            EXPECT_EQ(lhs == rhs, left == right);
            EXPECT_EQ(lhs != rhs, left != right);
            EXPECT_EQ(lhs < rhs, left < right);
            EXPECT_EQ(lhs > rhs, left > right);
            EXPECT_EQ(lhs <= rhs, left <= right);
            EXPECT_EQ(lhs >= rhs, left >= right);
        }
    }
}

TYPED_TEST(VLACompare, ShorterPrefixIsLess)
{
    const Array<TypeParam> empty{std::allocator<TypeParam>{}};
    const Array<TypeParam> prefix(8U, TypeParam{1}, std::allocator<TypeParam>{});
    const Array<TypeParam> longer(9U, TypeParam{1}, std::allocator<TypeParam>{});
    EXPECT_FALSE(empty < empty);
    EXPECT_TRUE(empty <= empty);
    EXPECT_TRUE(empty < prefix);
    EXPECT_TRUE(prefix < longer);
    EXPECT_TRUE(longer > prefix);
    EXPECT_TRUE(prefix <= longer);
    EXPECT_FALSE(prefix >= longer);
}

// Only the elements take part; neither max_size() nor the allocation does.
TYPED_TEST(VLACompare, BoundsAndCapacityDoNotAffectOrder)
{
    const std::allocator<TypeParam> alloc{};
    Array<TypeParam>                bounded(3U, TypeParam{1}, 3U, alloc);
    Array<TypeParam>                roomy(3U, TypeParam{1}, alloc);
    roomy.reserve(64U);
    EXPECT_TRUE(bounded == roomy);
    EXPECT_FALSE(bounded < roomy);
    EXPECT_FALSE(roomy < bounded);
    EXPECT_TRUE(bounded <= roomy);
    EXPECT_TRUE(bounded >= roomy);
}

TEST(VLACompare, IntOrderUsesElementValues)
{
    const std::vector<std::vector<int>> sequences{{}, {-1}, {0}, {2}, {0, -1}, {0, 2}, {2, -1}, {-1, 2, 0}};
    for (const auto& left : sequences)
    {
        for (const auto& right : sequences)
        {
            EXPECT_EQ(to_array(left) < to_array(right), left < right);
            EXPECT_EQ(to_array(left) >= to_array(right), left >= right);
        }
    }
}

// Unordered elements such as NaN are where std::vector's C++17 and C++20 operators disagree. Before C++20, <= is
// !(rhs < lhs) and an unordered pair counts as equivalent, so {NaN, 1} < {NaN, 2}. From C++20 the operators come from
// operator<=>, which stops at the first unordered pair. The array must match std::vector in each standard.
TEST(VLACompare, UnorderedElementsMatchVector)
{
    const double                           nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<std::vector<double>> sequences{{1.0, nan}, {1.0, 2.0}, {nan, 1.0}, {nan, 2.0}, {nan}, {}};
    for (const auto& left : sequences)
    {
        for (const auto& right : sequences)
        {
            const Array<double> lhs = to_array(left);
            const Array<double> rhs = to_array(right);
            EXPECT_EQ(lhs < rhs, left < right);
            EXPECT_EQ(lhs > rhs, left > right);
            EXPECT_EQ(lhs <= rhs, left <= right);
            EXPECT_EQ(lhs >= rhs, left >= right);
        }
    }
}

// Ordering needs only the element's operator<, as std::vector's does.
struct OnlyLess
{
    int  value;
    bool operator<(const OnlyLess& rhs) const
    {
        return value < rhs.value;
    }
};

TEST(VLACompare, ElementsNeedOnlyLessThan)
{
    const std::allocator<OnlyLess> alloc{};
    const Array<OnlyLess>          small{{OnlyLess{1}, OnlyLess{2}}, alloc};
    const Array<OnlyLess>          large{{OnlyLess{1}, OnlyLess{3}}, alloc};
    EXPECT_TRUE(small < large);
    EXPECT_TRUE(large > small);
    EXPECT_TRUE(small <= large);
    EXPECT_FALSE(small >= large);
}

// An element type without operator< can still be stored; the operators are only instantiated when used.
struct Unordered
{
    int value;
};

TEST(VLACompare, UnorderedElementTypesRemainUsable)
{
    Array<Unordered> array{std::allocator<Unordered>{}};
    array.push_back(Unordered{4});
    ASSERT_EQ(array.size(), 1U);
    EXPECT_EQ(array[0].value, 4);
}

#if defined(__cpp_lib_three_way_comparison) && (__cpp_lib_three_way_comparison >= 201907L)
// From C++20 the ordering comes from operator<=>, whose category follows the element's, as for std::vector.
TEST(VLACompare, ThreeWayComparisonMatchesVector)
{
    const auto sequences = make_sequences<int>();
    for (const auto& left : sequences)
    {
        for (const auto& right : sequences)
        {
            EXPECT_EQ(to_array(left) <=> to_array(right), left <=> right);
        }
    }
    const auto bits = make_sequences<bool>();
    for (const auto& left : bits)
    {
        for (const auto& right : bits)
        {
            EXPECT_EQ(to_array(left) <=> to_array(right), left <=> right);
        }
    }
    // Ordinary doubles give less, greater and equivalent partial orderings, and -0.0 is equivalent to 0.0.
    const std::vector<std::vector<double>> doubles{{}, {1.0}, {2.0}, {1.0, 2.0}, {2.0, 1.0}, {-0.0}, {0.0}, {0.0, 1.0}};
    for (const auto& left : doubles)
    {
        for (const auto& right : doubles)
        {
            EXPECT_EQ(to_array(left) <=> to_array(right), left <=> right);
        }
    }
    const double nan = std::numeric_limits<double>::quiet_NaN();
    EXPECT_EQ(to_array(std::vector<double>{nan, 1.0}) <=> to_array(std::vector<double>{nan, 2.0}),
              std::partial_ordering::unordered);
    EXPECT_EQ(Array<OnlyLess>({OnlyLess{1}}, std::allocator<OnlyLess>{}) <=>
                  Array<OnlyLess>({OnlyLess{2}}, std::allocator<OnlyLess>{}),
              std::weak_ordering::less);
}

static_assert(std::is_same<decltype(std::declval<const Array<int>&>() <=> std::declval<const Array<int>&>()),
                           std::strong_ordering>::value,
              "int elements order strongly");
static_assert(std::is_same<decltype(std::declval<const Array<bool>&>() <=> std::declval<const Array<bool>&>()),
                           std::strong_ordering>::value,
              "bits order strongly");
static_assert(std::is_same<decltype(std::declval<const Array<double>&>() <=> std::declval<const Array<double>&>()),
                           std::partial_ordering>::value,
              "floating-point elements order partially");
static_assert(std::is_same<decltype(std::declval<const Array<OnlyLess>&>() <=> std::declval<const Array<OnlyLess>&>()),
                           std::weak_ordering>::value,
              "elements with only operator< order weakly, as through synth-three-way");
static_assert(std::three_way_comparable<Array<int>>, "arrays of ordered elements are three-way comparable");
static_assert(!std::three_way_comparable<Array<Unordered>>, "arrays of unordered elements are not");

// operator< must give a boolean-testable result, as synth-three-way requires. These element types have an operator<
// that compiles but does not, so the array has no operator<=>, the same as std::vector, and asking does not fail to
// compile.
struct VoidLess
{
    void operator<(const VoidLess&) const {}
};
struct ExplicitBool
{
    explicit operator bool() const
    {
        return false;
    }
};
struct ExplicitBoolLess
{
    ExplicitBool operator<(const ExplicitBoolLess&) const
    {
        return {};
    }
};
// Boolean-testable also requires the negation of the result to convert to bool. This result converts implicitly but
// cannot be negated usefully.
struct VoidNegation
{
    operator bool() const
    {
        return false;
    }
    void operator!() const {}
};
struct VoidNegationLess
{
    VoidNegation operator<(const VoidNegationLess&) const
    {
        return {};
    }
};
template <typename A>
concept HasThreeWay = requires(const A& lhs, const A& rhs) { lhs <=> rhs; };
static_assert(!HasThreeWay<Array<VoidLess>>, "a void operator< does not order the array");
static_assert(!HasThreeWay<Array<ExplicitBoolLess>>, "an explicit-bool operator< does not order the array");
static_assert(HasThreeWay<Array<VoidLess>> == HasThreeWay<std::vector<VoidLess>>, "matches std::vector");
static_assert(HasThreeWay<Array<ExplicitBoolLess>> == HasThreeWay<std::vector<ExplicitBoolLess>>,
              "matches std::vector");
static_assert(!HasThreeWay<Array<VoidNegationLess>>, "a result whose negation is not boolean does not order the array");
static_assert(HasThreeWay<Array<VoidNegationLess>> == HasThreeWay<std::vector<VoidNegationLess>>,
              "matches std::vector");
static_assert(HasThreeWay<Array<OnlyLess>> == HasThreeWay<std::vector<OnlyLess>>, "matches std::vector");
#endif

static_assert(noexcept(std::declval<const Array<bool>&>() < std::declval<const Array<bool>&>()),
              "packed bits compare without throwing");
static_assert(noexcept(std::declval<const Array<bool>&>() >= std::declval<const Array<bool>&>()),
              "packed bits compare without throwing");

}  // namespace
