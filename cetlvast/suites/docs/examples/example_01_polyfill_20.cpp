/// @file
/// Example of using the CETL C++20 polyfill headers.
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

TEST(example_01_polyfill_20, span_static)
{
    //! [example_01_polyfill_20_span]
    std::string            greeting{"Hello Dynamic World."};

    // the cetl/pf20/cetlpf.hpp header will automatically define cetl::span
    // based on the C++ standard available to the compiler. If < 20 then
    // it will be the CETL version, if >= 20 then it will be the std version.
    cetl::span<const char> dynamic{greeting.c_str(), 13};
    auto                   print = [](const char c) { std::cout << c; };

    // Print just the characters in the span...
    std::for_each(dynamic.begin(), dynamic.end(), print);
    std::cout << std::endl;

    // or...
    std::string substring{dynamic.begin(), dynamic.end()};
    std::cout << substring << std::endl;
    //! [example_01_polyfill_20_span]
}

namespace
{
//! [example_01_polyfill_20_span_not_pf_pt1]
// Define these in a header or something...
constexpr std::size_t my_dynamic_extent = cetl::pf20::dynamic_extent;

template <typename T, std::size_t Extent = my_dynamic_extent>
using my_span = cetl::pf20::span<T, Extent>;

//! [example_01_polyfill_20_span_not_pf_pt1]
}  // namespace

TEST(example_01_polyfill_20, example_01_polyfill_20_span_dynamic)
{
    //! [example_01_polyfill_20_span_not_pf_pt2]
    std::string         greeting{"Hello Dynamic World."};

    // now use my_span instead of cetl::span and you only have one place
    // to change to std::span when you upgrade your compiler.
    my_span<const char> dynamic{greeting.c_str(), 13};
    auto                print = [](const char c) { std::cout << c; };

    // Print just the characters in the span...
    std::for_each(dynamic.begin(), dynamic.end(), print);
    std::cout << std::endl;

    // or...
    std::string substring{dynamic.begin(), dynamic.end()};
    std::cout << substring << std::endl;

    //! [example_01_polyfill_20_span_not_pf_pt2]
}
