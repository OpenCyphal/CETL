/// @file
/// Compile test that ensures it's impossible set "bigger" value than `Footprint` of `any`.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/any.hpp"

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
    using any = cetl::any<sizeof(uint8_t)>;

    any test{};

#ifndef CETLVAST_COMPILETEST_PRECHECK

    // Verify at `cetl::detail::base_storage::make_handlers`
    // ```
    // static_assert(sizeof(Tp) <= Footprint, "Enlarge the footprint");
    // ```
    test = static_cast<uint16_t>(1);

#else

    test = static_cast<uint8_t>(1);

#endif

    return 0;
}
