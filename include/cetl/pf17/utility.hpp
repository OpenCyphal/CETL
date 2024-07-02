/// @file
/// Defines C++17 entities from the \c utility header.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_UTILITY_HPP_INCLUDED
#define CETL_PF17_UTILITY_HPP_INCLUDED

#include <cstddef>
#include <utility>
#include <type_traits>

namespace cetl
{
namespace pf17
{
/// Polyfill for std::in_place_t.
struct in_place_t
{
    explicit in_place_t() = default;
};

// --------------------------------------------------------------------------------------------

/// Polyfill for std::in_place.
constexpr in_place_t in_place{};

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::in_place_type_t.
template <typename T>
struct in_place_type_t
{
    explicit in_place_type_t() = default;
};

/// Implementation of \ref std::in_place_type.
template <typename T>
constexpr in_place_type_t<T> in_place_type{};

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::in_place_index_t.
template <std::size_t I>
struct in_place_index_t
{
    explicit in_place_index_t() = default;
};

/// Implementation of \ref std::in_place_index.
template <std::size_t I>
constexpr in_place_index_t<I> in_place_index{};

// --------------------------------------------------------------------------------------------

// Non-standard extensions for internal use.
// We keep the definitions here to make it clear they are designed to detect the above-defined types, not std:: ones.
namespace detail
{
template <typename>
struct is_in_place_type_impl : std::false_type
{};
template <typename T>
struct is_in_place_type_impl<in_place_type_t<T>> : std::true_type
{};
/// A SFINAE helper for detecting \ref in_place_type_t.
template <typename T>
struct is_in_place_type : is_in_place_type_impl<std::decay_t<T>>
{};
static_assert(is_in_place_type<decltype(in_place_type<int>)>::value, "self-test failure");
static_assert(!is_in_place_type<decltype(in_place_index<0>)>::value, "self-test failure");

// --------------------------------------------------------------------------------------------

template <typename>
struct is_in_place_index_impl : std::false_type
{};
template <std::size_t I>
struct is_in_place_index_impl<in_place_index_t<I>> : std::true_type
{};
/// A SFINAE helper for detecting \ref in_place_index_t.
template <typename T>
struct is_in_place_index : is_in_place_index_impl<std::decay_t<T>>
{};
static_assert(is_in_place_index<decltype(in_place_index<0>)>::value, "self-test failure");
static_assert(!is_in_place_index<decltype(in_place_type<int>)>::value, "self-test failure");
}  // namespace detail

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_UTILITY_HPP_INCLUDED
