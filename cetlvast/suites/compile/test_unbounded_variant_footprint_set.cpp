/// @file
/// Compile test that ensures it's impossible set "bigger" value than `Footprint` of `unbounded_variant`.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/unbounded_variant.hpp"

#include <cstdint>

namespace cetl
{
template <>
constexpr type_id type_id_value<uint8_t>{};

template <>
constexpr type_id type_id_value<uint16_t>{};

}  // namespace cetl

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
