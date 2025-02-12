/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETLVAST_UNITTEST_OPTIONAL_TEST_PF17_OPTIONAL_H_INCLUDED
#define CETLVAST_UNITTEST_OPTIONAL_TEST_PF17_OPTIONAL_H_INCLUDED

#include <cetl/pf17/optional.hpp>
#include <cetl/pf17/utility.hpp>
#include <cetlvast/typelist.hpp>
#include <cetlvast/smf_policies.hpp>
#include <gtest/gtest.h>

/// A simple pair of types for testing where foo is implicitly convertible to bar but not vice versa.
template <typename>
struct bar;
template <typename Base>
struct foo final : Base
{
    foo()
        : value{0}
    {
    }
    explicit foo(const std::int64_t val) noexcept
        : value{val}
    {
    }
    explicit foo(const bar<Base>& val) noexcept;
    explicit foo(bar<Base>&& val) noexcept;
    foo(const std::initializer_list<std::int64_t> il)
        : value{static_cast<std::int64_t>(il.size())}
    {
    }
    std::int64_t value;
};
template <typename Base>
struct bar final : Base
{
    bar(const std::int64_t val) noexcept  // NOLINT(*-explicit-constructor)
        : value{val}
    {
    }
    bar(const foo<Base>& other) noexcept  // NOLINT(*-explicit-constructor)
        : value{other.value}
    {
    }
    bar(foo<Base>&& other) noexcept  // NOLINT(*-explicit-constructor)
        : value{other.value}
    {
        other.value = 0;  // Moving zeroes the source.
    }
    std::int64_t value;
};
template <typename Base>
foo<Base>::foo(const bar<Base>& val) noexcept
    : value{val.value}
{
}
template <typename Base>
foo<Base>::foo(bar<Base>&& val) noexcept
    : value{val.value}
{
    val.value = 0;  // Moving zeroes the source.
}

#endif  // CETLVAST_UNITTEST_OPTIONAL_TEST_PF17_OPTIONAL_H_INCLUDED
