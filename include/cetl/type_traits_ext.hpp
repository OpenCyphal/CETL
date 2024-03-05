/// @file
/// Type traits extensions not found in C++ standards.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_TYPE_TRAITS_EXTENSIONS_HPP_INCLUDED
#define CETL_TYPE_TRAITS_EXTENSIONS_HPP_INCLUDED

#include <algorithm>
#include <utility>
#include <limits>
#include <type_traits>

namespace cetl
{
namespace type_traits_ext
{
namespace detail
{
// The predicate is an unary function but we declare it as variadic for composability with generic tools
// like partial application. A variadic argument accepts an unary template, but not the other way around.
template <template <typename...> class, std::size_t, typename...>
struct find_impl;
template <template <typename...> class Predicate, std::size_t Ix>
struct find_impl<Predicate, Ix>
{
    static constexpr std::size_t first_match_index = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t match_count       = 0;
};
template <template <typename...> class Predicate, std::size_t Ix, typename Head, typename... Tail>
struct find_impl<Predicate, Ix, Head, Tail...> : find_impl<Predicate, Ix + 1, Tail...>
{
    using base                                     = find_impl<Predicate, Ix + 1, Tail...>;
    static constexpr std::size_t first_match_index = (Predicate<Head>::value ? Ix : base::first_match_index);
    static constexpr std::size_t match_count       = (Predicate<Head>::value ? 1 : 0) + base::match_count;
};
}  // namespace detail

/// Index of the first type T in the typelist for which Predicate<T>::value is true.
template <template <typename...> class Predicate, typename... Ts>
struct find : std::integral_constant<std::size_t, detail::find_impl<Predicate, 0, Ts...>::first_match_index>
{};
template <template <typename...> class Predicate, typename... Ts>
static constexpr std::size_t find_v = find<Predicate, Ts...>::value;

/// Number of types in the typelist for which Predicate<T>::value is true.
template <template <typename...> class Predicate, typename... Ts>
struct count : std::integral_constant<std::size_t, detail::find_impl<Predicate, 0, Ts...>::match_count>
{};
template <template <typename...> class Predicate, typename... Ts>
static constexpr std::size_t count_v = count<Predicate, Ts...>::value;

// --------------------------------------------------------------------------------------------

/// Partially apply a template by holding the Left type arguments fixed.
/// Does not work with non-type template parameters.
template <template <typename...> class F, typename... Left>
struct partial
{
    template <typename... Right>
    using type = F<Left..., Right...>;
};

}  // namespace type_traits_ext
}  // namespace cetl

#endif  // CETL_TYPE_TRAITS_EXTENSIONS_HPP_INCLUDED
