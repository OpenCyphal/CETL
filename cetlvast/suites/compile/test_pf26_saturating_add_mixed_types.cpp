/// @file
/// Compile test that ensures operands of different types are rejected rather than promoted, as in C++26.
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
    // Deduction of T from int and long conflicts, exactly as for std::min(1, 1L).
    return static_cast<int>(cetl::pf26::saturating_add(1, 1L)) - 2;
#else
    return static_cast<int>(cetl::pf26::saturating_add<long>(1, 1L)) - 2;
#endif
}
