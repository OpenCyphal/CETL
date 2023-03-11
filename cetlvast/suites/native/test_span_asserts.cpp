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

#endif  // CETL_ENABLE_DEBUG_ASSERT

}  //  namespace
