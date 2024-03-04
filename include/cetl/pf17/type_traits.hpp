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
#include <type_traits>

namespace cetl
{
namespace pf17
{
namespace detail
{
namespace adl_swap_detail
{
// std::swap has to be used in a SFINAE context, so we need to rely on ADL here.
// This is a deviation from AUTOSAR M7-3-6.
using std::swap;

template <typename T, typename = void>
struct is_swappable : std::false_type
{};
template <typename T>
struct is_swappable<T, decltype(swap(std::declval<T&>(), std::declval<T&>()))> : std::true_type
{};

template <typename T, bool = is_swappable<T>::value>
struct is_nothrow_swappable;
template <typename T>
struct is_nothrow_swappable<T, false> : std::false_type
{};
template <typename T>
struct is_nothrow_swappable<T, true>
    : std::integral_constant<bool, noexcept(swap(std::declval<T&>(), std::declval<T&>()))>
{};
}  // namespace adl_swap_detail
}  // namespace detail

/// Implementation of \ref std::is_swappable.
template <typename T>
struct is_swappable : detail::adl_swap_detail::is_swappable<T>
{};

/// Implementation of \ref std::is_swappable_v.
template <typename T>
constexpr bool is_swappable_v = is_swappable<T>::value;

/// Implementation of \ref std::is_nothrow_swappable.
template <typename T>
struct is_nothrow_swappable : detail::adl_swap_detail::is_nothrow_swappable<T>
{};

/// Implementation of \ref std::is_nothrow_swappable_v.
template <typename T>
constexpr bool is_nothrow_swappable_v = is_nothrow_swappable<T>::value;

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::conjunction.
template <typename...>
struct conjunction : std::true_type
{};
template <typename A>
struct conjunction<A> : A
{};
template <typename A, typename... B>
struct conjunction<A, B...> : std::conditional_t<static_cast<bool>(A::value), conjunction<B...>, A>
{};

/// Implementation of \ref std::conjunction_v.
template <typename... Ts>
constexpr bool conjunction_v = conjunction<Ts...>::value;

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::disjunction.
template <typename...>
struct disjunction : std::false_type
{};
template <typename A>
struct disjunction<A> : A
{};
template <typename A, typename... B>
struct disjunction<A, B...> : std::conditional_t<static_cast<bool>(A::value), A, disjunction<B...>>
{};

/// Implementation of \ref std::disjunction_v.
template <typename... Ts>
constexpr bool disjunction_v = disjunction<Ts...>::value;

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::negation.
template <typename T>
struct negation : std::integral_constant<bool, !static_cast<bool>(T::value)>
{};

/// Implementation of \ref std::negation_v.
template <typename T>
constexpr bool negation_v = negation<T>::value;

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::void_t.
template <typename...>
using void_t = void;

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_TYPE_TRAITS_HPP_INCLUDED
