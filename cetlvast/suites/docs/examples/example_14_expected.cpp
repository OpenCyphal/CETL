/// @file
/// Example of using the CETL C++23 polyfill header for expected.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf23/cetlpf.hpp"

#include <cstdint>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

namespace
{
//! [example_14_expected_parse]
// The cetl/pf23/cetlpf.hpp header provides cetl::expected from the standard library when it has it and from
// cetl::pf23 otherwise. Either way a function can return its result or the reason it has none, without exceptions.
enum class parse_error
{
    empty,
    not_a_number,
    out_of_range,
};

cetl::expected<std::uint16_t, parse_error> parse_node_id(const std::string& text)
{
    if (text.empty())
    {
        return cetl::unexpected<parse_error>(parse_error::empty);
    }
    std::uint32_t value = 0;
    for (const char c : text)
    {
        if ((c < '0') || (c > '9'))
        {
            return cetl::unexpected<parse_error>(parse_error::not_a_number);
        }
        value = (value * 10U) + static_cast<std::uint32_t>(c - '0');
        if (value > 0x7FFFU)
        {
            return cetl::unexpected<parse_error>(parse_error::out_of_range);
        }
    }
    return static_cast<std::uint16_t>(value);
}
//! [example_14_expected_parse]

const char* describe(const parse_error error)
{
    switch (error)
    {
    case parse_error::empty:
        return "empty";
    case parse_error::not_a_number:
        return "not a number";
    case parse_error::out_of_range:
        return "out of range";
    }
    return "unknown";
}

}  // namespace

TEST(example_14_expected, parse)
{
    const auto good = parse_node_id("42");
    const auto bad  = parse_node_id("forty-two");
    if (good)
    {
        std::cout << "node-ID: " << *good << std::endl;  // 42
    }
    if (!bad)
    {
        std::cout << "error: " << describe(bad.error()) << std::endl;  // not a number
    }
    ASSERT_EQ(42U, good.value_or(0));
    ASSERT_EQ(parse_error::not_a_number, bad.error());
}

TEST(example_14_expected, chain)
{
    //! [example_14_expected_chain]
    // and_then() runs the next step only if the previous one succeeded, transform() maps a value, and
    // transform_error() maps an error, so the steps compose without checking each result by hand.
    const auto checked = [](const std::uint16_t id) -> cetl::expected<std::uint16_t, parse_error> {
        if (id == 0U)
        {
            return cetl::unexpected<parse_error>(parse_error::out_of_range);  // zero is reserved here
        }
        return id;
    };
    const auto port = parse_node_id("125")
                          .and_then(checked)
                          .transform([](const std::uint16_t id) { return static_cast<std::uint32_t>(id) + 1000U; })
                          .transform_error([](const parse_error e) { return std::string(describe(e)); });
    std::cout << "port: " << port.value_or(0) << std::endl;  // 1125
    //! [example_14_expected_chain]
    ASSERT_EQ(1125U, *port);
    const auto rejected = parse_node_id("0").and_then(checked);
    ASSERT_EQ(parse_error::out_of_range, rejected.error());
}
