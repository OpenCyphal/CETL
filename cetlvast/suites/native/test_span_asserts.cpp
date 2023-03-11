/// @file
/// Unit tests for span.h
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.h"
#include "cetlvast/helpers.h"
#include "cetl/span.h"

namespace
{
// +----------------------------------------------------------------------+
// | DEBUG ASSERT TESTS
// +----------------------------------------------------------------------+
#if CETL_ENABLE_DEBUG_ASSERT

static void TestStaticSpanWithWrongSize()
{
    const char* hello_world = "Hello World";
    (void) cetl::span<const char, 11>(hello_world, 10);
}

TEST(DeathTestSpanAssertions, TestStaticSpanWithWrongSize)
{
    EXPECT_DEATH(TestStaticSpanWithWrongSize(), "CDE_span_001");
}

// +----------------------------------------------------------------------+

static void TestStaticSpanWithWrongDistance()
{
    const char* hello_world = "Hello World";
    (void) cetl::span<const char, 10>(hello_world, &hello_world[11]);
}

TEST(DeathTestSpanAssertions, TestStaticSpanWithWrongDistance)
{
    EXPECT_DEATH(TestStaticSpanWithWrongDistance(), "CDE_span_002");
}

// +----------------------------------------------------------------------+

static void TestStaticSpanFromDynamicOfWrongSize()
{
    const char*            hello_world = "Hello World";
    cetl::span<const char> hello_span(hello_world, 11);
    (void) cetl::span<const char, 10>(hello_span);
}

TEST(DeathTestSpanAssertions, TestStaticSpanFromDynamicOfWrongSize)
{
    EXPECT_DEATH(TestStaticSpanFromDynamicOfWrongSize(), "CDE_span_003");
}

// +----------------------------------------------------------------------+

static void TestStaticSpanIndexPastEnd()
{
    const char*            hello_world = "Hello World";
    cetl::span<const char, 11> subject(hello_world, 11);
    (void)subject[11];
}

TEST(DeathTestSpanAssertions, TestStaticSpanIndexPastEnd)
{
    EXPECT_DEATH(TestStaticSpanIndexPastEnd(), "CDE_span_004");
}

// +----------------------------------------------------------------------+

static void TestStaticIndexOfNull()
{
    const char*            hello_world = nullptr;
    cetl::span<const char, 11> subject(hello_world, 11);
    (void)subject[0];
}

TEST(DeathTestSpanAssertions, TestStaticIndexOfNull)
{
    EXPECT_DEATH(TestStaticIndexOfNull(), "CDE_span_005");
}

// +----------------------------------------------------------------------+

static void TestStaticFrontOfZeroSize()
{
    const char*            hello_world = "Hello world";
    cetl::span<const char, 0> subject(hello_world, 0);
    (void)subject.front();
}

TEST(DeathTestSpanAssertions, TestStaticFrontOfZeroSize)
{
    EXPECT_DEATH(TestStaticFrontOfZeroSize(), "CDE_span_006");
}

// +----------------------------------------------------------------------+

static void TestStaticBackOfZeroSize()
{
    const char*            hello_world = "Hello world";
    cetl::span<const char, 0> subject(hello_world, 0);
    (void)subject.back();
}

TEST(DeathTestSpanAssertions, TestStaticBackOfZeroSize)
{
    EXPECT_DEATH(TestStaticBackOfZeroSize(), "CDE_span_007");
}

// +----------------------------------------------------------------------+

static void TestStaticSubviewFirstTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::span<const char, 10> subject(hello_world, 10);
    (void)subject.first(11);
}

TEST(DeathTestSpanAssertions, TestStaticSubviewFirstTooLarge)
{
    EXPECT_DEATH(TestStaticSubviewFirstTooLarge(), "CDE_span_008");
}

// +----------------------------------------------------------------------+

static void TestStaticSubviewLastTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::span<const char, 10> subject(hello_world, 10);
    (void)subject.last(11);
}

TEST(DeathTestSpanAssertions, TestStaticSubviewLastTooLarge)
{
    EXPECT_DEATH(TestStaticSubviewLastTooLarge(), "CDE_span_009");
}

// +----------------------------------------------------------------------+

static void TestStaticSubspanOffsetTooLarge()
{
    const char*            hello_world = "Hello world";
    cetl::span<const char, 10> subject(hello_world, 10);
    (void)subject.subspan(11, cetl::dynamic_extent);
}

TEST(DeathTestSpanAssertions, TestStaticSubspanOffsetTooLarge)
{
    EXPECT_DEATH(TestStaticSubspanOffsetTooLarge(), "CDE_span_010");
}

// +----------------------------------------------------------------------+

static void TestStaticSubspanCountIsWrong()
{
    const char*            hello_world = "Hello world";
    cetl::span<const char, 10> subject(hello_world, 10);
    (void)subject.subspan(10, 1);
}

TEST(DeathTestSpanAssertions, TestStaticSubspanCountIsWrong)
{
    EXPECT_DEATH(TestStaticSubspanCountIsWrong(), "CDE_span_011");
}





#endif  // CETL_ENABLE_DEBUG_ASSERT

}  //  namespace
