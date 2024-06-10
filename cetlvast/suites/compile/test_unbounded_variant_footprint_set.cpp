/// @file
/// Compile test that ensures it's impossible set "bigger" value than `Footprint` of `unbounded_variant`.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/unbounded_variant.hpp"
#include "cetlvast/helpers_rtti.hpp"

#include <cstdint>

int main()
{
    using ub_var = cetl::unbounded_variant<sizeof(uint8_t)>;

    ub_var test{};

#ifndef CETLVAST_COMPILETEST_PRECHECK

    // Verify at `cetl::detail::base_storage::check_footprint` (called from `base_access::make_handlers()`).
    // ```
    // static_assert(sizeof(Tp) <= Footprint, "Enlarge the footprint");
    // ```
    test = static_cast<uint16_t>(1);

#else

    test = static_cast<uint8_t>(1);

#endif

    return 0;
}
