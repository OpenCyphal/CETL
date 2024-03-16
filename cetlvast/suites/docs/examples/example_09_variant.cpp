/// @file
/// Example of using cetl::variant.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf17/variant.hpp"
#include <string>
#include <algorithm>
#include <cassert>
#include <iostream>
#include <string>

#include <gtest/gtest.h>

TEST(example_09_variant, basic_usage)
{
    //! [example_09_basic_usage]
    /// This example is taken directly from the [cppreference.com](https://en.cppreference.com/w/cpp/utility/variant)
    /// documentation.
    cetl::pf17::variant<int, float> v, w;
    v     = 42;  // v contains int
    int i = cetl::pf17::get<int>(v);
    assert(42 == i);  // succeeds
    w = cetl::pf17::get<int>(v);
    w = cetl::pf17::get<0>(v);  // same effect as the previous line
    w = v;                      // same effect as the previous line

    //  cetl::pf17::get<double>(v); // error: no double in [int, float]
    //  cetl::pf17::get<3>(v);      // error: valid index values are 0 and 1

    float* result = cetl::pf17::get_if<float>(&w);  // w contains int, not float: will return null NOLINT(*-use-auto)
    std::cout << "get_if<float>(&w) => " << result << '\n';

    using namespace std::literals;

    cetl::pf17::variant<std::string> x("abc");
    // converting constructors work when unambiguous
    x = "def";  // converting assignment also works when unambiguous

    cetl::pf17::variant<std::string, void const*> y("abc");
    // casts to void const * when passed a char const *
    assert(cetl::pf17::holds_alternative<void const*>(y));  // succeeds
    y = "xyz"s;
    assert(cetl::pf17::holds_alternative<std::string>(y));  // succeeds
    //! [example_09_basic_usage]
}
