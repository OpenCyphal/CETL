/// @file
/// CETL VerificAtion SuiTe – Google test helpers.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETLVAST_HELPERS_GTEST_H_INCLUDED
#define CETLVAST_HELPERS_GTEST_H_INCLUDED

#include "cetlvast/helpers.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

/// Workaround limitation in googletest to enable coverage data from the
/// forked or cloned processes used by google death-tests.
/// Add a call to this function at the start of the death test. For example:
/// ```
/// static void TestThatThisDies()
/// {
///     flush_coverage_on_death();
///     this_should_cause_an_abort("Ack!");
/// }
///
/// TEST(DeathTestFoo, TestThatThisDies)
/// {
///     EXPECT_DEATH(TestThatThisDies(), "Ack!");
/// }
/// ```
extern "C" void flush_coverage_on_death();

#else

/// No-op.
inline void flush_coverage_on_death() {}

#endif

namespace cetlvast
{
/// Used for typed tests to tag that the CETL version of a fixture should be used.
struct CETLTag final
{
    CETLTag() = delete;
};

/// Used for typed tests to tag that the STL version of a figure is available and should be used.
struct STLTag final
{
    STLTag() = delete;
};

/// Used for typed tests to tag that the given test should be skipped for this type.
struct SkipTag final
{
    SkipTag() = delete;
};
}  // namespace cetlvast

#endif  // CETLVAST_HELPERS_GTEST_H_INCLUDED
