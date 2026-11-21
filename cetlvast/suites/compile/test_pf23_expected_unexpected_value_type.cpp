/// @file
/// Compile test that ensures expected rejects a specialization of unexpected as its value type, as in C++23.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf23/expected.hpp"

int main()
{
#ifndef CETLVAST_COMPILETEST_PRECHECK
    const cetl::pf23::expected<cetl::pf23::unexpected<int>, int> ex{cetl::pf23::unexpect, 0};
#else
    const cetl::pf23::expected<int, int> ex{cetl::pf23::unexpect, 0};
#endif
    return ex.error();
}
