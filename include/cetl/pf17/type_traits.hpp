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

/// Implementation of \ref std::negation.
template <typename T>
struct negation : std::integral_constant<bool, !static_cast<bool>(T::value)>
{};

/// Implementation of \ref std::negation_v.
template <typename T>
constexpr bool negation_v = negation<T>::value;

// ---------------------------------------------------------------------------------------------------------------------

static_assert(conjunction_v<> == true, "");
static_assert(conjunction_v<std::true_type> == true, "");
static_assert(conjunction_v<std::false_type> == false, "");
static_assert(conjunction_v<std::true_type, std::true_type> == true, "");
static_assert(conjunction_v<std::true_type, std::false_type> == false, "");
static_assert(conjunction_v<std::false_type, std::true_type> == false, "");
static_assert(conjunction_v<std::false_type, std::false_type> == false, "");

static_assert(disjunction_v<> == false, "");
static_assert(disjunction_v<std::true_type> == true, "");
static_assert(disjunction_v<std::false_type> == false, "");
static_assert(disjunction_v<std::true_type, std::true_type> == true, "");
static_assert(disjunction_v<std::true_type, std::false_type> == true, "");
static_assert(disjunction_v<std::false_type, std::true_type> == true, "");
static_assert(disjunction_v<std::false_type, std::false_type> == false, "");

static_assert(negation_v<std::true_type> == false, "");
static_assert(negation_v<std::false_type> == true, "");

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_TYPE_TRAITS_HPP_INCLUDED
