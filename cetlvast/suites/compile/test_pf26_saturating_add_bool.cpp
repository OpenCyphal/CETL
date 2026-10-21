/// @file
/// Compile test that ensures bool is not accepted by the saturation arithmetic functions, as in C++26.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf26/numeric.hpp"

int main()
{
#ifndef CETLVAST_COMPILETEST_PRECHECK
    // bool is not a signed or unsigned integer type: no matching function.
    return cetl::pf26::saturating_add(true, true) ? 0 : 1;
#else
    return cetl::pf26::saturating_add(1, 1) - 2;
#endif
}
