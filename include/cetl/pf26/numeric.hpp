/// @file
/// Defines the C++26 saturation arithmetic functions of the \c numeric header ([numeric.sat]) for C++14 and newer.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// The functions follow ISO/IEC 14882:2026 as renamed by P4052R0: `saturating_add`, `saturating_sub`,
/// `saturating_mul`, `saturating_div` and `saturating_cast`. As in the standard, the operands share one type that
/// is never promoted, mixed operand types fail deduction, and `bool` and the character types are excluded.
///
/// Where the compiler provides `__builtin_add_overflow` and friends (GCC 5 and newer, Clang) they are used;
/// otherwise a portable implementation that never forms an overflowing expression is used. Both are constant
/// expressions in C++14.

#ifndef CETL_PF26_NUMERIC_HPP_INCLUDED
#define CETL_PF26_NUMERIC_HPP_INCLUDED

#include <limits>
#include <type_traits>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif
#include "cetl/pf20/utility.hpp"

/// @def CETL_PF26_DETAIL_OVERFLOW_BUILTINS
/// 1 when the compiler's overflow-checking builtins implement the saturating operations, 0 when the portable
/// implementation does. Not a configuration knob; exposed only so that tests can report which path is active.
#if defined(__has_builtin)
#    if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_sub_overflow) && \
        __has_builtin(__builtin_mul_overflow)
#        define CETL_PF26_DETAIL_OVERFLOW_BUILTINS 1
#    endif
#elif defined(__GNUC__) && (__GNUC__ >= 5)
#    define CETL_PF26_DETAIL_OVERFLOW_BUILTINS 1
#endif
#ifndef CETL_PF26_DETAIL_OVERFLOW_BUILTINS
#    define CETL_PF26_DETAIL_OVERFLOW_BUILTINS 0
#endif

namespace cetl
{
namespace pf26
{
namespace detail
{
/// The "signed or unsigned integer types" of [basic.fundamental]: the standard and extended integer types.
template <typename T>
using is_signed_or_unsigned_integer = pf20::detail::is_standard_or_extended_integer<T>;

template <typename T>
using enable_if_integer_t = std::enable_if_t<is_signed_or_unsigned_integer<T>::value, int>;

// +----------------------------------------------------------------------------------------------------------------+
// | Bounds returned on overflow. For signed types the operands of an overflowing operation determine the direction.
// +----------------------------------------------------------------------------------------------------------------+
template <typename T>
constexpr T add_saturated(T x, std::true_type /*signed*/) noexcept
{
    return (x < 0) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
}
template <typename T>
constexpr T add_saturated(T, std::false_type /*unsigned*/) noexcept
{
    return std::numeric_limits<T>::max();
}
template <typename T>
constexpr T sub_saturated(T x, std::true_type /*signed*/) noexcept
{
    return (x < 0) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
}
template <typename T>
constexpr T sub_saturated(T, std::false_type /*unsigned*/) noexcept
{
    return std::numeric_limits<T>::min();
}
template <typename T>
constexpr T mul_saturated(T x, T y, std::true_type /*signed*/) noexcept
{
    return ((x < 0) != (y < 0)) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
}
template <typename T>
constexpr T mul_saturated(T, T, std::false_type /*unsigned*/) noexcept
{
    return std::numeric_limits<T>::max();
}

// +----------------------------------------------------------------------------------------------------------------+
// | Portable overflow predicates. No expression below can itself overflow or divide by zero: the limit is moved to
// | the side of the comparison that keeps it in range, and divisors are paired with dividends of the sign that
// | cannot produce min / -1.
// +----------------------------------------------------------------------------------------------------------------+
template <typename T>
constexpr bool add_overflows(T x, T y, std::true_type /*signed*/) noexcept
{
    return (y > 0) ? (x > static_cast<T>(std::numeric_limits<T>::max() - y))
                   : (x < static_cast<T>(std::numeric_limits<T>::min() - y));
}
template <typename T>
constexpr bool add_overflows(T x, T y, std::false_type /*unsigned*/) noexcept
{
    return x > static_cast<T>(std::numeric_limits<T>::max() - y);
}
template <typename T>
constexpr bool sub_overflows(T x, T y, std::true_type /*signed*/) noexcept
{
    return (y < 0) ? (x > static_cast<T>(std::numeric_limits<T>::max() + y))
                   : (x < static_cast<T>(std::numeric_limits<T>::min() + y));
}
template <typename T>
constexpr bool sub_overflows(T x, T y, std::false_type /*unsigned*/) noexcept
{
    return x < y;
}
template <typename T>
constexpr bool mul_overflows(T x, T y, std::true_type /*signed*/) noexcept
{
    return ((x == 0) || (y == 0)) ? false
           : (x > 0) ? ((y > 0) ? (x > std::numeric_limits<T>::max() / y) : (y < std::numeric_limits<T>::min() / x))
                     : ((y > 0) ? (x < std::numeric_limits<T>::min() / y) : (y < std::numeric_limits<T>::max() / x));
}
template <typename T>
constexpr bool mul_overflows(T x, T y, std::false_type /*unsigned*/) noexcept
{
    return (y != 0) && (x > std::numeric_limits<T>::max() / y);
}
template <typename T>
constexpr bool is_min_over_minus_one(T x, T y, std::true_type /*signed*/) noexcept
{
    return (x == std::numeric_limits<T>::min()) && (y == static_cast<T>(-1));
}
template <typename T>
constexpr bool is_min_over_minus_one(T, T, std::false_type /*unsigned*/) noexcept
{
    return false;
}

// +----------------------------------------------------------------------------------------------------------------+
// | Portable implementations. These are always compiled (and unit-tested) even where the builtins are used.
// +----------------------------------------------------------------------------------------------------------------+
template <typename T>
constexpr T saturating_add_portable(T x, T y) noexcept
{
    return add_overflows(x, y, std::is_signed<T>{}) ? add_saturated(x, std::is_signed<T>{}) : static_cast<T>(x + y);
}
template <typename T>
constexpr T saturating_sub_portable(T x, T y) noexcept
{
    return sub_overflows(x, y, std::is_signed<T>{}) ? sub_saturated(x, std::is_signed<T>{}) : static_cast<T>(x - y);
}
template <typename T>
constexpr T saturating_mul_portable(T x, T y) noexcept
{
    return mul_overflows(x, y, std::is_signed<T>{}) ? mul_saturated(x, y, std::is_signed<T>{}) : static_cast<T>(x * y);
}

#if CETL_PF26_DETAIL_OVERFLOW_BUILTINS
template <typename T>
constexpr T saturating_add_builtin(T x, T y) noexcept
{
    T result{};
    return __builtin_add_overflow(x, y, &result) ? add_saturated(x, std::is_signed<T>{}) : result;
}
template <typename T>
constexpr T saturating_sub_builtin(T x, T y) noexcept
{
    T result{};
    return __builtin_sub_overflow(x, y, &result) ? sub_saturated(x, std::is_signed<T>{}) : result;
}
template <typename T>
constexpr T saturating_mul_builtin(T x, T y) noexcept
{
    T result{};
    return __builtin_mul_overflow(x, y, &result) ? mul_saturated(x, y, std::is_signed<T>{}) : result;
}
#endif
}  // namespace detail

/// Polyfill for std::saturating_add (C++26, [numeric.sat.func]).
/// Returns `x + y` if it is representable in `T`, otherwise the bound of `T` nearest to the mathematical result.
/// @tparam T A signed or unsigned integer type (not `bool` or a character type); never promoted.
template <typename T, detail::enable_if_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_add(T x, T y) noexcept
{
#if CETL_PF26_DETAIL_OVERFLOW_BUILTINS
    return detail::saturating_add_builtin(x, y);
#else
    return detail::saturating_add_portable(x, y);
#endif
}

/// Polyfill for std::saturating_sub (C++26, [numeric.sat.func]).
/// Returns `x - y` if it is representable in `T`, otherwise the bound of `T` nearest to the mathematical result.
/// @tparam T A signed or unsigned integer type (not `bool` or a character type); never promoted.
template <typename T, detail::enable_if_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_sub(T x, T y) noexcept
{
#if CETL_PF26_DETAIL_OVERFLOW_BUILTINS
    return detail::saturating_sub_builtin(x, y);
#else
    return detail::saturating_sub_portable(x, y);
#endif
}

/// Polyfill for std::saturating_mul (C++26, [numeric.sat.func]).
/// Returns `x * y` if it is representable in `T`, otherwise the bound of `T` nearest to the mathematical result.
/// @tparam T A signed or unsigned integer type (not `bool` or a character type); never promoted.
template <typename T, detail::enable_if_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_mul(T x, T y) noexcept
{
#if CETL_PF26_DETAIL_OVERFLOW_BUILTINS
    return detail::saturating_mul_builtin(x, y);
#else
    return detail::saturating_mul_portable(x, y);
#endif
}

/// Polyfill for std::saturating_div (C++26, [numeric.sat.func]).
/// Returns `x / y` (truncated toward zero) if it is representable in `T`; the only other case,
/// `numeric_limits<T>::min() / -1`, returns `numeric_limits<T>::max()`.
/// @tparam T A signed or unsigned integer type (not `bool` or a character type); never promoted.
/// @pre `y != 0` (CDE_numeric_001). As in the standard, a zero divisor is undefined behaviour and a call that
///      divides by zero is not a constant expression.
template <typename T, detail::enable_if_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_div(T x, T y) noexcept
{
    CETL_DEBUG_ASSERT(y != 0, "CDE_numeric_001: saturating_div requires a non-zero divisor.");
    return detail::is_min_over_minus_one(x, y, std::is_signed<T>{}) ? std::numeric_limits<T>::max()
                                                                    : static_cast<T>(x / y);
}

/// Polyfill for std::saturating_cast (C++26, [numeric.sat.cast]).
/// Returns `x` if it is representable in `T`, otherwise the bound of `T` nearest to `x`.
/// @tparam T The signed or unsigned integer type to convert to (not `bool` or a character type).
/// @tparam U The signed or unsigned integer type of the value (deduced).
template <typename T, typename U, detail::enable_if_integer_t<T> = 0, detail::enable_if_integer_t<U> = 0>
CETL_NODISCARD constexpr T saturating_cast(U x) noexcept
{
    return pf20::cmp_less(x, std::numeric_limits<T>::min())      ? std::numeric_limits<T>::min()
           : pf20::cmp_greater(x, std::numeric_limits<T>::max()) ? std::numeric_limits<T>::max()
                                                                 : static_cast<T>(x);
}

}  // namespace pf26
}  // namespace cetl

#endif  // CETL_PF26_NUMERIC_HPP_INCLUDED
