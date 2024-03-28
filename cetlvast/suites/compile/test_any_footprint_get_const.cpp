/// @file
/// Compile test that ensures it's impossible get "bigger" value than `Footprint` of const `any`.
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
}

int main()
{
    using any = cetl::any<sizeof(uint8_t)>;

    const any test{static_cast<uint8_t>(0)};

    // Verify at `cetl::detail::base_storage::get_ptr const`
    // ```
    // static_assert(sizeof(ValueType) <= Footprint,
    //               "Cannot contain the requested type since the footprint is too small");
    // ```
    return cetl::any_cast<uint16_t>(test);
}
