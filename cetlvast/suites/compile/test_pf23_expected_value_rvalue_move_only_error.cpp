/// @file
/// Compile test that ensures value() on an rvalue still requires a copy constructible error type, as in C++23.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf23/expected.hpp"

#include <memory>

int main()
{
    cetl::pf23::expected<int, std::unique_ptr<int>> ex{1};
#ifndef CETLVAST_COMPILETEST_PRECHECK
    // [expected.object.obs] mandates is_copy_constructible_v<E> for value() && as well, because the error is put into
    // a bad_expected_access; libc++ and libstdc++ enforce it the same way.
    return std::move(ex).value() - 1;
#else
    return *std::move(ex) - 1;
#endif
}
