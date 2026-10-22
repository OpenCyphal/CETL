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
// +----------------------------------------------------------------------------------------------------------------+
// | Integer traits shared by the integer-comparison functions and by cetl/pf26/numeric.hpp.
// |
// | The standard traits are not enough on their own: GCC's 128-bit extended integers are not reported by
// | std::is_integral, std::is_signed, std::make_unsigned or std::numeric_limits in its strict -std=c++NN modes, and
// | cv-qualifiers must be ignored so that explicit template arguments such as `cmp_less<const char, char>` are still
// | rejected.
// +----------------------------------------------------------------------------------------------------------------+
#if defined(__SIZEOF_INT128__)
__extension__ typedef __int128          int128_type;
__extension__ typedef unsigned __int128 uint128_type;
template <typename T>
using is_int128 =
    std::integral_constant<bool, std::is_same<T, int128_type>::value || std::is_same<T, uint128_type>::value>;
template <typename T>
using is_signed_int128 = std::is_same<T, int128_type>;
#else
template <typename T>
using is_int128 = std::false_type;
template <typename T>
using is_signed_int128 = std::false_type;
#endif

/// `bool` and the character types: integral, but excluded from the integer-comparison functions and from saturation
/// arithmetic by the standard.
template <typename T>
struct is_bool_or_character
    : std::integral_constant<bool,
                             std::is_same<T, bool>::value || std::is_same<T, char>::value ||
                                 std::is_same<T, wchar_t>::value || std::is_same<T, char16_t>::value ||
                                 std::is_same<T, char32_t>::value>
{};
#if defined(__cpp_char8_t)
template <>
struct is_bool_or_character<char8_t> : std::true_type
{};
#endif

/// True for the standard and extended integer types of [basic.fundamental], ignoring cv-qualifiers.
template <typename T>
using is_standard_or_extended_integer = std::integral_constant<bool,
                                                               (std::is_integral<std::remove_cv_t<T>>::value &&
                                                                !is_bool_or_character<std::remove_cv_t<T>>::value) ||
                                                                   is_int128<std::remove_cv_t<T>>::value>;

/// True for the signed types among the standard and extended integer types.
template <typename T>
using is_signed_integer =
    std::integral_constant<bool,
                           is_standard_or_extended_integer<T>::value && (std::is_signed<std::remove_cv_t<T>>::value ||
                                                                         is_signed_int128<std::remove_cv_t<T>>::value)>;

template <typename T, bool = is_int128<T>::value>
struct make_unsigned_integer
{
    using type = std::make_unsigned_t<T>;
};
#if defined(__SIZEOF_INT128__)
template <typename T>
struct make_unsigned_integer<T, true>
{
    using type = uint128_type;
};
#endif
/// The unsigned counterpart of a standard or extended integer type.
template <typename T>
using make_unsigned_integer_t = typename make_unsigned_integer<std::remove_cv_t<T>>::type;

/// The limits of a standard or extended integer type. std::numeric_limits is used where it is specialized for T.
template <typename T, bool = std::numeric_limits<T>::is_specialized>
struct integer_limits
{
    static constexpr T min() noexcept
    {
        return std::numeric_limits<T>::min();
    }
    static constexpr T max() noexcept
    {
        return std::numeric_limits<T>::max();
    }
};
/// Extended integers that the standard library does not describe (GCC's __int128 in strict modes) are two's
/// complement, so their limits follow from their width.
template <typename T>
struct integer_limits<T, false>
{
    using unsigned_type = make_unsigned_integer_t<T>;
    static constexpr T max() noexcept
    {
        return max_impl(is_signed_integer<T>{});
    }
    static constexpr T min() noexcept
    {
        return min_impl(is_signed_integer<T>{});
    }

private:
    static constexpr T max_impl(std::true_type /*signed*/) noexcept
    {
        return static_cast<T>(static_cast<unsigned_type>(~unsigned_type{0}) >> 1);
    }
    static constexpr T max_impl(std::false_type /*unsigned*/) noexcept
    {
        return static_cast<T>(~T{0});
    }
    static constexpr T min_impl(std::true_type /*signed*/) noexcept
    {
        return static_cast<T>(-max() - 1);
    }
    static constexpr T min_impl(std::false_type /*unsigned*/) noexcept
    {
        return T{0};
    }
};

/// Selects the comparison strategy: 0 = same signedness, 1 = T signed and U unsigned, 2 = T unsigned and U signed.
template <typename T, typename U>
using cmp_case_t = std::integral_constant<int,
                                          (is_signed_integer<T>::value == is_signed_integer<U>::value) ? 0
                                          : is_signed_integer<T>::value                                ? 1
                                                                                                       : 2>;

template <typename T, typename U>
constexpr bool cmp_equal_impl(T t, U u, std::integral_constant<int, 0>) noexcept
{
    return t == u;
}
template <typename T, typename U>
constexpr bool cmp_equal_impl(T t, U u, std::integral_constant<int, 1>) noexcept
{
    return (t < 0) ? false : (static_cast<make_unsigned_integer_t<T>>(t) == u);
}
template <typename T, typename U>
constexpr bool cmp_equal_impl(T t, U u, std::integral_constant<int, 2>) noexcept
{
    return (u < 0) ? false : (t == static_cast<make_unsigned_integer_t<U>>(u));
}

template <typename T, typename U>
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 0>) noexcept
{
    return t < u;
}
template <typename T, typename U>
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 1>) noexcept
{
    return (t < 0) ? true : (static_cast<make_unsigned_integer_t<T>>(t) < u);
}
template <typename T, typename U>
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 2>) noexcept
{
    return (u < 0) ? false : (t < static_cast<make_unsigned_integer_t<U>>(u));
}

template <typename T, typename U>
using are_comparable_integers =
    std::integral_constant<bool,
                           is_standard_or_extended_integer<T>::value && is_standard_or_extended_integer<U>::value>;
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
    static_assert(detail::are_comparable_integers<R, T>::value,
                  "in_range requires standard or extended integer types (not bool or character types).");
    return cmp_greater_equal(t, detail::integer_limits<std::remove_cv_t<R>>::min()) &&
           cmp_less_equal(t, detail::integer_limits<std::remove_cv_t<R>>::max());
}

}  // namespace pf20
}  // namespace cetl

#endif  // CETL_PF20_UTILITY_HPP_INCLUDED
