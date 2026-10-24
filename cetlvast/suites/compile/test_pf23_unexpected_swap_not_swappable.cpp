/// @file
/// Compile test that ensures unexpected::swap() cannot be called with an error type that is not swappable.
///
/// [expected.un.swap] makes is_swappable_v<E> a Mandates of the member swap, which this polyfill and libc++ enforce
/// with a static_assert; libstdc++ makes it a constraint instead. Either way the call does not compile.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf23/expected.hpp"

namespace
{
struct pinned
{
    explicit pinned(const int v) noexcept
        : value{v}
    {
    }
    pinned(const pinned&)            = delete;
    pinned(pinned&&)                 = delete;
    pinned& operator=(const pinned&) = delete;
    pinned& operator=(pinned&&)      = delete;
    ~pinned()                        = default;
    int value;
};
}  // namespace

int main()
{
    cetl::pf23::unexpected<pinned> a{1};
    cetl::pf23::unexpected<pinned> b{2};
#ifndef CETLVAST_COMPILETEST_PRECHECK
    a.swap(b);
#endif
    return a.error().value + b.error().value - 3;
}
