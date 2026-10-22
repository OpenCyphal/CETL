/// @file
/// Death tests for cetl/pf23/expected.hpp: debug assertions on unchecked access, and value() without exceptions.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest.hpp"
#include "cetl/pf23/expected.hpp"

namespace
{
/// Uses a result, which a cast to void does not do for GCC's warn_unused_result (CETL_NODISCARD in C++14).
template <typename T>
void consume(const T&) noexcept
{
}

// +----------------------------------------------------------------------+
// | DEBUG ASSERT TESTS
// +----------------------------------------------------------------------+
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

struct widget
{
    int value;
};

using ex  = cetl::pf23::expected<widget, int>;
using vex = cetl::pf23::expected<void, int>;

void DereferenceError()
{
    flush_coverage_on_death();
    ex error{cetl::pf23::unexpect, 1};
    (void) (*error).value;
}

void ArrowError()
{
    flush_coverage_on_death();
    const ex error{cetl::pf23::unexpect, 1};
    (void) error->value;
}

void ErrorOfValue()
{
    flush_coverage_on_death();
    const ex value{widget{1}};
    consume(value.error());
}

void RvalueErrorOfValue()
{
    flush_coverage_on_death();
    consume(ex{widget{1}}.error());
}

void DereferenceVoidError()
{
    flush_coverage_on_death();
    const vex error{cetl::pf23::unexpect, 1};
    *error;
}

void ErrorOfVoidValue()
{
    flush_coverage_on_death();
    const vex value{};
    consume(value.error());
}

TEST(DeathTestExpectedAssertions, DereferenceError)
{
    EXPECT_DEATH(DereferenceError(), "CDE_expected_004");
}

TEST(DeathTestExpectedAssertions, ArrowError)
{
    EXPECT_DEATH(ArrowError(), "CDE_expected_001");
}

TEST(DeathTestExpectedAssertions, ErrorOfValue)
{
    EXPECT_DEATH(ErrorOfValue(), "CDE_expected_007");
}

TEST(DeathTestExpectedAssertions, RvalueErrorOfValue)
{
    EXPECT_DEATH(RvalueErrorOfValue(), "CDE_expected_010");
}

TEST(DeathTestExpectedAssertions, DereferenceVoidError)
{
    EXPECT_DEATH(DereferenceVoidError(), "CDE_expected_011");
}

TEST(DeathTestExpectedAssertions, ErrorOfVoidValue)
{
    EXPECT_DEATH(ErrorOfVoidValue(), "CDE_expected_012");
}

#endif  // CETL_ENABLE_DEBUG_ASSERT

// +----------------------------------------------------------------------+
// | NO EXCEPTIONS
// +----------------------------------------------------------------------+
#if !defined(__cpp_exceptions)

void ValueOfError()
{
    flush_coverage_on_death();
    const cetl::pf23::expected<int, int> error{cetl::pf23::unexpect, 1};
    consume(error.value());
}

void ValueOfVoidError()
{
    flush_coverage_on_death();
    cetl::pf23::expected<void, int> error{cetl::pf23::unexpect, 1};
    std::move(error).value();
}

TEST(DeathTestExpectedNoExceptions, ValueOfErrorTerminates)
{
    EXPECT_DEATH(ValueOfError(), "");
}

TEST(DeathTestExpectedNoExceptions, ValueOfVoidErrorTerminates)
{
    EXPECT_DEATH(ValueOfVoidError(), "");
}

#endif  // !__cpp_exceptions

TEST(DeathTestExpected, Placeholder)
{
    // Keeps the suite non-empty in configurations where neither group above applies.
    SUCCEED();
}

}  // namespace
