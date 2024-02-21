/// @file
/// Defines C++17 entities from the \c utility header.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_UTILITY_HPP_INCLUDED
#define CETL_PF17_UTILITY_HPP_INCLUDED

namespace cetl
{
namespace pf17
{

/// Polyfill for std::in_place_t.
struct in_place_t
{
    explicit in_place_t() = default;
};

/// Polyfill for std::in_place.
constexpr in_place_t in_place{};

/// Implementation of \ref std::in_place_type_t.
template <typename T>
struct in_place_type_t
{
    explicit in_place_type_t() = default;
};

/// Implementation of \ref std::in_place_type.
template <typename T>
constexpr in_place_type_t<T> in_place_type{};

/// Implementation of \ref std::in_place_index_t.
template <std::size_t I>
struct in_place_index_t
{
    explicit in_place_index_t() = default;
};

/// Implementation of \ref std::in_place_index.
template <std::size_t I>
constexpr in_place_index_t<I> in_place_index{};

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_UTILITY_HPP_INCLUDED
