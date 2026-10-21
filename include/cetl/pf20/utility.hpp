/// @file
/// Defines C++20 entities from the \c utility header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF20_UTILITY_HPP_INCLUDED
#define CETL_PF20_UTILITY_HPP_INCLUDED

#include <limits>
#include <type_traits>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

namespace cetl
{
namespace pf20
{
namespace detail
{
/// True for the standard and extended integer types of [basic.fundamental]. False for `bool` and the character
/// types, which the standard excludes from the integer-comparison functions and from saturation arithmetic.
template <typename T>
struct is_standard_or_extended_integer
    : std::integral_constant<bool,
                             std::is_integral<T>::value && !std::is_same<T, bool>::value &&
                                 !std::is_same<T, char>::value && !std::is_same<T, wchar_t>::value &&
                                 !std::is_same<T, char16_t>::value && !std::is_same<T, char32_t>::value>
{};
#if defined(__cpp_char8_t)
template <>
struct is_standard_or_extended_integer<char8_t> : std::false_type
{};
#endif

/// Selects the comparison strategy: 0 = same signedness, 1 = T signed and U unsigned, 2 = T unsigned and U signed.
template <typename T, typename U>
using cmp_case_t = std::integral_constant<int,
                                          (std::is_signed<T>::value == std::is_signed<U>::value) ? 0
                                          : std::is_signed<T>::value                             ? 1
                                                                                                 : 2>;

template <typename T, typename U>
constexpr bool cmp_equal_impl(T t, U u, std::integral_constant<int, 0>) noexcept
{
    return t == u;
}
template <typename T, typename U>
constexpr bool cmp_equal_impl(T t, U u, std::integral_constant<int, 1>) noexcept
{
    return (t < 0) ? false : (static_cast<std::make_unsigned_t<T>>(t) == u);
}
template <typename T, typename U>
constexpr bool cmp_equal_impl(T t, U u, std::integral_constant<int, 2>) noexcept
{
    return (u < 0) ? false : (t == static_cast<std::make_unsigned_t<U>>(u));
}

template <typename T, typename U>
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 0>) noexcept
{
    return t < u;
}
template <typename T, typename U>
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 1>) noexcept
{
    return (t < 0) ? true : (static_cast<std::make_unsigned_t<T>>(t) < u);
}
template <typename T, typename U>
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 2>) noexcept
{
    return (u < 0) ? false : (t < static_cast<std::make_unsigned_t<U>>(u));
}

template <typename T, typename U>
struct are_comparable_integers
    : std::integral_constant<bool,
                             is_standard_or_extended_integer<T>::value && is_standard_or_extended_integer<U>::value>
{};
}  // namespace detail

/// Polyfill for std::cmp_equal (C++20, [utility.intcmp]): `t == u` with the mathematical values, so that
/// negative values never compare equal to unsigned values.
/// @pre T and U are standard or extended integer types (mandated by static assertion).
template <typename T, typename U>
CETL_NODISCARD constexpr bool cmp_equal(T t, U u) noexcept
{
    static_assert(detail::are_comparable_integers<T, U>::value,
                  "cmp_equal requires standard or extended integer types (not bool or character types).");
    return detail::cmp_equal_impl(t, u, detail::cmp_case_t<T, U>{});
}

/// Polyfill for std::cmp_not_equal (C++20, [utility.intcmp]).
template <typename T, typename U>
CETL_NODISCARD constexpr bool cmp_not_equal(T t, U u) noexcept
{
    return !cmp_equal(t, u);
}

/// Polyfill for std::cmp_less (C++20, [utility.intcmp]): `t < u` with the mathematical values, so that
/// `cmp_less(-1, 1U)` is true where `-1 < 1U` is false.
/// @pre T and U are standard or extended integer types (mandated by static assertion).
template <typename T, typename U>
CETL_NODISCARD constexpr bool cmp_less(T t, U u) noexcept
{
    static_assert(detail::are_comparable_integers<T, U>::value,
                  "cmp_less requires standard or extended integer types (not bool or character types).");
    return detail::cmp_less_impl(t, u, detail::cmp_case_t<T, U>{});
}

/// Polyfill for std::cmp_greater (C++20, [utility.intcmp]).
template <typename T, typename U>
CETL_NODISCARD constexpr bool cmp_greater(T t, U u) noexcept
{
    return cmp_less(u, t);
}

/// Polyfill for std::cmp_less_equal (C++20, [utility.intcmp]).
template <typename T, typename U>
CETL_NODISCARD constexpr bool cmp_less_equal(T t, U u) noexcept
{
    return !cmp_less(u, t);
}

/// Polyfill for std::cmp_greater_equal (C++20, [utility.intcmp]).
template <typename T, typename U>
CETL_NODISCARD constexpr bool cmp_greater_equal(T t, U u) noexcept
{
    return !cmp_less(t, u);
}

/// Polyfill for std::in_range (C++20, [utility.intcmp]): true if the value of `t` is representable in `R`.
/// @pre R and T are standard or extended integer types (mandated by static assertion).
template <typename R, typename T>
CETL_NODISCARD constexpr bool in_range(T t) noexcept
{
    static_assert(detail::are_comparable_integers<std::remove_cv_t<R>, T>::value,
                  "in_range requires standard or extended integer types (not bool or character types).");
    return cmp_greater_equal(t, std::numeric_limits<R>::min()) && cmp_less_equal(t, std::numeric_limits<R>::max());
}

}  // namespace pf20
}  // namespace cetl

#endif  // CETL_PF20_UTILITY_HPP_INCLUDED
