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

    ub_var test{static_cast<uint8_t>(0)};

#ifndef CETLVAST_COMPILETEST_PRECHECK

    // Verify at `cetl::detail::base_storage::check_footprint` (called from `base_access::get_ptr()`).
    // ```
    // static_assert(sizeof(ValueType) <= Footprint,
    //               "Cannot contain the requested type since the footprint is too small");
    // ```
    return cetl::get<uint16_t&>(test);

#else

    return cetl::get<uint8_t&>(test);

#endif
}
