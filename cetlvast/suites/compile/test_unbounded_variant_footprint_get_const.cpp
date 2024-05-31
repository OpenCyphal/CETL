/// @file
/// Compile test that ensures it's impossible get "bigger" value than `Footprint` of const `unbounded_variant`.
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

    const ub_var test{static_cast<uint8_t>(0)};

#ifndef CETLVAST_COMPILETEST_PRECHECK

    // Verify at `cetl::detail::base_storage::check_footprint` (called from `base_access::get_ptr() const`).
    // ```
    // static_assert(sizeof(ValueType) <= Footprint,
    //               "Cannot contain the requested type since the footprint is too small");
    // ```
    return cetl::get<uint16_t>(test);

#else

    return cetl::get<uint8_t>(test);

#endif
}
