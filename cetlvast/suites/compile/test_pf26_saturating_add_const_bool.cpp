/// @file
/// Compile test that ensures cv-qualified bool is rejected by the saturation arithmetic functions as plain bool is.
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
    // const bool is still bool: no matching function.
    return cetl::pf26::saturating_add<const bool>(true, true) ? 0 : 1;
#else
    return cetl::pf26::saturating_add<const int>(1, 1) - 2;
#endif
}
