/// @file
/// Example of using cetl::unbounded_variant.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/unbounded_variant.hpp"

#include <gmock/gmock.h>

//! [example_10_any_type_id]
namespace cetl
{
template <>
constexpr type_id type_id_value<bool> = {1};
template <>
constexpr type_id type_id_value<int> = {2};
template <>
constexpr type_id type_id_value<float> = {3};
template <>
constexpr type_id type_id_value<double> = {4};

}  // namespace cetl
//! [example_10_any_type_id]

TEST(example_10_unbounded_variant, basic_usage)
{
    //! [example_10_unbounded_variant_basic_usage]
    /// This example is inspired by the [cppreference.com](https://en.cppreference.com/w/cpp/utility/any)
    /// documentation, but instead of `any` it's called `unbounded_variant`.
    using ub_var = cetl::unbounded_variant<std::max(sizeof(int), sizeof(double))>;

    ub_var a = 1;
    EXPECT_THAT(cetl::get<int>(a), 1);

    a = 3.14;
    EXPECT_THAT(cetl::get<double>(a), 3.14);

    a = true;
    EXPECT_TRUE(cetl::get<bool>(a));

    // bad any cast
    //
    a = 1;
#if defined(__cpp_exceptions)
    // Workaround for GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
    // Should be used in the tests where exceptions are expected (see `EXPECT_THROW`).
    const auto sink = [](auto&&) {};

    EXPECT_THROW(sink(cetl::get<float>(a)), cetl::bad_unbounded_variant_access);
#else
    EXPECT_THAT(cetl::get_if<float>(&a), testing::IsNull());
#endif

    a = 2;
    EXPECT_TRUE(a.has_value());

    // reset
    //
    a.reset();
    EXPECT_FALSE(a.has_value());

    // pointer to contained data
    //
    a = 3;
    EXPECT_THAT(*cetl::get_if<int>(&a), 3);
    //! [example_10_unbounded_variant_basic_usage]
}
