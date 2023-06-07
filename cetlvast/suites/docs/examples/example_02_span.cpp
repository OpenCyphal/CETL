/// @file
/// Example of using the dynamic-extent specialization of cetl::span.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf20/span.hpp"
#include <iostream>
#include <string>
#include <algorithm>

#include <gtest/gtest.h>

TEST(example_02_span, dynamic_span)
{
//! [example_02_span_dynamic]
    std::string greeting{"Hello Dynamic World."};
    // Works just like a std::span...
    cetl::pf20::span<const char> dynamic{greeting.c_str(), 13};
    auto print = [](const char c) { std::cout << c; };

    // Print just the characters in the span...
    std::for_each(dynamic.begin(), dynamic.end(), print);
    std::cout << std::endl;

    // or...
    std::string substring{dynamic.begin(), dynamic.size()};
    std::cout << substring << std::endl;
//! [example_02_span_dynamic]
}


//! [example_02_span_static_pt1]
template<typename T, std::size_t Extent>
std::ostream& operator<<(std::ostream& os, const cetl::pf20::span<T, Extent>& sp)
{
    std::for_each(sp.begin(), sp.end(), [&os](const char c) { os << c; });
    return os;
}
//! [example_02_span_static_pt1]


TEST(example_02_span, static_span)
{
//! [example_02_span_static_pt2]
    constexpr const char* greeting = "Hello Static World";
    std::cout << cetl::pf20::span<const char, 12>{greeting, 12} << std::endl;
//! [example_02_span_static_pt2]
}
