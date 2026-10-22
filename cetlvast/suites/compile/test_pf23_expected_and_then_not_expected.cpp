/// @file
/// Compile test that ensures and_then() rejects a callable that does not return an expected, as in C++23.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf23/expected.hpp"

int main()
{
    const cetl::pf23::expected<int, int> ex{1};
#ifndef CETLVAST_COMPILETEST_PRECHECK
    // and_then() mandates that the callable returns a specialization of expected.
    return ex.and_then([](const int x) { return x; }).value();
#else
    return ex.and_then([](const int x) { return cetl::pf23::expected<int, int>{x - 1}; }).value();
#endif
}
