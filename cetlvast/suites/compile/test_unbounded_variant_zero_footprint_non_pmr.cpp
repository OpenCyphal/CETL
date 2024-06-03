/// @file
/// Compile test that ensures it's impossible to use non-PMR `unbounded_variant` with zero footprint.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/unbounded_variant.hpp"

#include <cstdint>

int main()
{
#ifndef CETLVAST_COMPILETEST_PRECHECK

    cetl::unbounded_variant<0> test{};

#else

    cetl::unbounded_variant<1> test{};

#endif

    return 0;
}
