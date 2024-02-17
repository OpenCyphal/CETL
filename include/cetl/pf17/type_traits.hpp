/// @file
/// Type traits backported from C++17.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_TYPE_TRAITS_HPP_INCLUDED
#define CETL_PF17_TYPE_TRAITS_HPP_INCLUDED

#include <algorithm>
#include <utility>

namespace cetl
{
namespace pf17
{
namespace detail
{
namespace type_traits
{
/// std::is_nothrow_swappable
using std::swap;  // This has to be visible for ADL.
template <typename T>
constexpr bool is_nothrow_swappable = noexcept(swap(std::declval<T&>(), std::declval<T&>()));
}  // namespace type_traits
}  // namespace detail

/// A reimplementation of \ref std::is_nothrow_swappable_v that works with C++14.
template <typename T>
constexpr bool is_nothrow_swappable_v = detail::type_traits::is_nothrow_swappable<T>;

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_TYPE_TRAITS_HPP_INCLUDED
