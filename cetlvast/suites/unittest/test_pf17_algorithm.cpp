/// @file
/// Unit tests for cetl/pf17/algorithm.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "cetlvast/helpers_gtest.hpp"
#include "cetl/pf17/algorithm.hpp"
#include "cetl/pf17/cetlpf.hpp"

#include <functional>
#include <string>

namespace
{

// +----------------------------------------------------------------------+
// | Constant expressions
// +----------------------------------------------------------------------+
static_assert(cetl::pf17::clamp(5, 1, 3) == 3, "above hi");
static_assert(cetl::pf17::clamp(-5, 1, 3) == 1, "below lo");
static_assert(cetl::pf17::clamp(2, 1, 3) == 2, "inside");
static_assert(cetl::pf17::clamp(1, 1, 3) == 1, "at lo");
static_assert(cetl::pf17::clamp(3, 1, 3) == 3, "at hi");
static_assert(cetl::pf17::clamp(2, 2, 2) == 2, "degenerate range");
static_assert(cetl::pf17::clamp(5, 3, 1, std::greater<int>{}) == 3, "reversed comparison, above");
static_assert(cetl::pf17::clamp(0, 3, 1, std::greater<int>{}) == 1, "reversed comparison, below");
static_assert(cetl::pf17::clamp(2, 3, 1, std::greater<int>{}) == 2, "reversed comparison, inside");
static_assert(cetl::clamp(5, 1, 3) == 3, "cetlpf alias");

// +----------------------------------------------------------------------+
// | Reference semantics
// +----------------------------------------------------------------------+
TEST(PF17AlgorithmTest, ClampReturnsReferenceToArgument)
{
    const int v     = 2;
    const int lo    = 1;
    const int hi    = 3;
    const int below = 0;
    const int above = 9;
    EXPECT_EQ(&v, &cetl::pf17::clamp(v, lo, hi));
    EXPECT_EQ(&lo, &cetl::pf17::clamp(below, lo, hi));
    EXPECT_EQ(&hi, &cetl::pf17::clamp(above, lo, hi));
    // At a bound, v itself is returned (it is not less than lo and hi is not less than it).
    EXPECT_EQ(&lo, &cetl::pf17::clamp(lo, lo, hi));
    EXPECT_NE(&lo, &cetl::pf17::clamp(v, v, hi));
    EXPECT_EQ(&v, &cetl::pf17::clamp(v, v, hi));
}

TEST(PF17AlgorithmTest, ClampWithComparator)
{
    const auto        by_length = [](const std::string& a, const std::string& b) { return a.size() < b.size(); };
    const std::string lo{"ab"};
    const std::string hi{"abcd"};
    const std::string too_long{"abcdef"};
    const std::string too_short{"a"};
    const std::string inside{"abc"};
    EXPECT_EQ(hi, cetl::pf17::clamp(too_long, lo, hi, by_length));
    EXPECT_EQ(lo, cetl::pf17::clamp(too_short, lo, hi, by_length));
    EXPECT_EQ(inside, cetl::pf17::clamp(inside, lo, hi, by_length));
}

TEST(PF17AlgorithmTest, ClampFloatingPoint)
{
    EXPECT_DOUBLE_EQ(1.5, cetl::pf17::clamp(1.5, 1.0, 2.0));
    EXPECT_DOUBLE_EQ(1.0, cetl::pf17::clamp(-1.5, 1.0, 2.0));
    EXPECT_DOUBLE_EQ(2.0, cetl::pf17::clamp(100.0, 1.0, 2.0));
}

// +----------------------------------------------------------------------+
// | Precondition
// +----------------------------------------------------------------------+
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

static void TestClampWithInvertedBounds()
{
    flush_coverage_on_death();
    const int  lo     = 3;
    const int  hi     = 1;
    const int  v      = 2;
    const int& result = cetl::pf17::clamp(v, lo, hi);
    (void) result;
}

TEST(DeathTestPF17Algorithm, ClampWithInvertedBounds)
{
    EXPECT_DEATH(TestClampWithInvertedBounds(), "CDE_algorithm_001");
}

#endif  // CETL_ENABLE_DEBUG_ASSERT

}  // namespace
