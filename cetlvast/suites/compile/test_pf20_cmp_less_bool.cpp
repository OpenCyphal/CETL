/// @file
/// Compile test that ensures bool is not accepted by the integer comparison functions, as in C++20.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf20/utility.hpp"

int main()
{
#ifndef CETLVAST_COMPILETEST_PRECHECK
    // Mandated: bool is not a standard or extended integer type, so this fails a static assertion.
    return cetl::pf20::cmp_less(false, 1) ? 0 : 1;
#else
    return cetl::pf20::cmp_less(0, 1) ? 0 : 1;
#endif
}
