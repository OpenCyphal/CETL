/// @file
/// Example of using the CETL C++26 polyfill header for saturation arithmetic.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf26/cetlpf.hpp"
#include "cetl/pf17/cetlpf.hpp"

#include <chrono>
#include <cstdint>
#include <iostream>
#include <limits>

#include <gtest/gtest.h>

TEST(example_13_saturating_arithmetic, saturating_add)
{
    //! [example_13_saturating_add]
    // A drop counter that must stop at its maximum instead of wrapping back to zero.
    // The cetl/pf26/cetlpf.hpp header provides cetl::saturating_add from the standard library when it has it
    // (under either the 2023 or the final 2026 names) and from cetl::pf26 otherwise.
    std::uint16_t dropped = 65530U;
    for (int i = 0; i < 10; ++i)
    {
        dropped = cetl::saturating_add(dropped, std::uint16_t{1});
    }
    std::cout << "dropped: " << dropped << std::endl;  // 65535, not 4
    //! [example_13_saturating_add]
    ASSERT_EQ(65535U, dropped);
}

TEST(example_13_saturating_arithmetic, saturating_cast)
{
    //! [example_13_saturating_cast]
    // Uptime in seconds is a 32-bit field on the wire (for example uavcan.node.Heartbeat). A static_cast would
    // wrap after 136 years; saturating_cast pins the value at the field's maximum instead.
    const std::chrono::seconds uptime{INT64_C(5000000000)};
    const std::uint32_t        field = cetl::saturating_cast<std::uint32_t>(uptime.count());
    std::cout << "uptime field: " << field << std::endl;  // 4294967295
    //! [example_13_saturating_cast]
    ASSERT_EQ(std::numeric_limits<std::uint32_t>::max(), field);
}

TEST(example_13_saturating_arithmetic, operands_share_one_type)
{
    //! [example_13_one_type]
    // As with std::min and std::max, both operands share one type and nothing is promoted. Name the type when the
    // operands differ; otherwise the call does not compile.
    const std::int8_t a   = 100;
    const std::int8_t sum = cetl::saturating_add<std::int8_t>(a, 100);  // 127
    //! [example_13_one_type]
    ASSERT_EQ(127, sum);
}

TEST(example_13_saturating_arithmetic, clamp_and_in_range)
{
    //! [example_13_clamp]
    // A value destined for a 10-bit signed field (DSDL `saturated int10`) has no matching fundamental type, so it is
    // clamped to the field's range with std::clamp, which cetl/pf17/cetlpf.hpp provides for C++14...
    const std::int16_t raw   = 0x5555;
    const std::int16_t field = cetl::clamp(raw, std::int16_t{-512}, std::int16_t{511});  // 511
    // ...and std::in_range (cetl/pf20/cetlpf.hpp) answers "would saturating_cast change this value?" without
    // performing the conversion.
    const bool fits = cetl::in_range<std::uint8_t>(raw);  // false
    //! [example_13_clamp]
    ASSERT_EQ(511, field);
    ASSERT_FALSE(fits);
}
