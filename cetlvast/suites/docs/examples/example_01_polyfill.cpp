/// @file
/// Example of using one of the CETL polyfill headers.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf20/cetlpf.hpp"
#include <iostream>
#include <string>
#include <algorithm>
#include <type_traits>

#include <gtest/gtest.h>

TEST(example_01_polyfill, main)
{
//! [main]
    std::string greeting{"Hello Dynamic World."};
    cetl::span<const char> dynamic{greeting.c_str(), 13};
    auto print = [](const char c) { std::cout << c; };

    // Print just the characters in the span...
    std::for_each(dynamic.begin(), dynamic.end(), print);
    std::cout << std::endl;

    // or...
    std::string substring{dynamic.begin(), dynamic.end()};
    std::cout << substring << std::endl;
//! [main]
}
