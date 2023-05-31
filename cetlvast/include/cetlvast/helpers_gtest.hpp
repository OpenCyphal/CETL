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

#if CETL_ENABLE_DEBUG_ASSERT

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

#endif  // CETLVAST_HELPERS_GTEST_H_INCLUDED
