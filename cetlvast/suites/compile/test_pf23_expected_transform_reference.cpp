/// @file
/// Compile test that ensures transform() rejects a callable that returns a reference, as in C++23.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf23/expected.hpp"

namespace
{
int storage = 0;
}

int main()
{
    const cetl::pf23::expected<int, int> ex{1};
#ifndef CETLVAST_COMPILETEST_PRECHECK
    // The new value type would be int&, which is not a valid value type for expected.
    return ex.transform([](const int) -> int& { return storage; }).value();
#else
    return ex.transform([](const int) -> int { return storage; }).value();
#endif
}
