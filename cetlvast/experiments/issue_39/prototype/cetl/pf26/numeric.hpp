/// Prototype of cetl/pf26/numeric.hpp: C++14 implementation of C++26 saturation arithmetic ([numeric.sat]).
#ifndef CETL_PF26_NUMERIC_HPP_INCLUDED
#define CETL_PF26_NUMERIC_HPP_INCLUDED

#include <limits>
#include <type_traits>
#include "cetl/cetl.hpp"

#ifndef CETL_PF26_NO_OVERFLOW_BUILTINS
#    if defined(__has_builtin)
#        if __has_builtin(__builtin_add_overflow) && __has_builtin(__builtin_sub_overflow) && \
            __has_builtin(__builtin_mul_overflow)
#            define CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS 1
#        endif
#    elif defined(__GNUC__) && (__GNUC__ >= 5)
#        define CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS 1
#    endif
#endif
#ifndef CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS
#    define CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS 0
#endif

namespace cetl
{
namespace pf26
{
namespace detail
{
/// "signed or unsigned integer type" per [basic.fundamental]: integral types other than bool and the character types.
template <typename T>
struct is_signed_or_unsigned_integer
    : std::integral_constant<bool,
                             std::is_integral<T>::value && !std::is_same<T, bool>::value &&
                                 !std::is_same<T, char>::value && !std::is_same<T, wchar_t>::value &&
                                 !std::is_same<T, char16_t>::value && !std::is_same<T, char32_t>::value>
{};
#if defined(__cpp_char8_t)
template <>
struct is_signed_or_unsigned_integer<char8_t> : std::false_type
{};
#endif
template <typename T>
using enable_if_sat_integer_t = std::enable_if_t<is_signed_or_unsigned_integer<T>::value, int>;

// ---- C++20 std::cmp_less / cmp_greater, C++14 edition (tag dispatch instead of if constexpr) ----
template <typename T, typename U>
using cmp_case_t = std::integral_constant<int,
                                          (std::is_signed<T>::value == std::is_signed<U>::value) ? 0
                                          : std::is_signed<T>::value                             ? 1
                                                                                                 : 2>;
template <typename T, typename U>
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 0>) noexcept
{
    return t < u;
}
template <typename T, typename U>  // T signed, U unsigned
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 1>) noexcept
{
    return (t < 0) ? true : (static_cast<std::make_unsigned_t<T>>(t) < u);
}
template <typename T, typename U>  // T unsigned, U signed
constexpr bool cmp_less_impl(T t, U u, std::integral_constant<int, 2>) noexcept
{
    return (u < 0) ? false : (t < static_cast<std::make_unsigned_t<U>>(u));
}
template <typename T, typename U>
constexpr bool cmp_less(T t, U u) noexcept
{
    return cmp_less_impl(t, u, cmp_case_t<T, U>{});
}
template <typename T, typename U>
constexpr bool cmp_greater(T t, U u) noexcept
{
    return cmp_less(u, t);
}

// ---- portable, UB-free overflow predicates (used when the compiler has no __builtin_*_overflow) ----
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
constexpr bool sub_overflows(T x, T y, std::true_type) noexcept
{
    return (y < 0) ? (x > static_cast<T>(std::numeric_limits<T>::max() + y))
                   : (x < static_cast<T>(std::numeric_limits<T>::min() + y));
}
template <typename T>
constexpr bool sub_overflows(T x, T y, std::false_type) noexcept
{
    return x < y;
}
template <typename T>
constexpr bool mul_overflows(T x, T y, std::true_type) noexcept
{
    // Divisor sign is always paired so that min / -1 can never be formed here.
    return ((x == 0) || (y == 0)) ? false
           : (x > 0) ? ((y > 0) ? (x > std::numeric_limits<T>::max() / y) : (y < std::numeric_limits<T>::min() / x))
                     : ((y > 0) ? (x < std::numeric_limits<T>::min() / y) : (y < std::numeric_limits<T>::max() / x));
}
template <typename T>
constexpr bool mul_overflows(T x, T y, std::false_type) noexcept
{
    return (y != 0) && (x > std::numeric_limits<T>::max() / y);
}

// ---- which bound to return on overflow ----
template <typename T>
constexpr T add_bound(T x, std::true_type) noexcept
{
    return (x < 0) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
}
template <typename T>
constexpr T add_bound(T, std::false_type) noexcept
{
    return std::numeric_limits<T>::max();
}
template <typename T>
constexpr T sub_bound(T x, std::true_type) noexcept
{
    return (x < 0) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
}
template <typename T>
constexpr T sub_bound(T, std::false_type) noexcept
{
    return std::numeric_limits<T>::min();
}
template <typename T>
constexpr T mul_bound(T x, T y, std::true_type) noexcept
{
    return ((x < 0) != (y < 0)) ? std::numeric_limits<T>::min() : std::numeric_limits<T>::max();
}
template <typename T>
constexpr T mul_bound(T, T, std::false_type) noexcept
{
    return std::numeric_limits<T>::max();
}
template <typename T>
constexpr bool is_min_over_minus_one(T x, T y, std::true_type) noexcept
{
    return (x == std::numeric_limits<T>::min()) && (y == static_cast<T>(-1));
}
template <typename T>
constexpr bool is_min_over_minus_one(T, T, std::false_type) noexcept
{
    return false;
}
}  // namespace detail

/// Polyfill for std::saturating_add ([numeric.sat], C++26).
template <typename T, detail::enable_if_sat_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_add(T x, T y) noexcept
{
#if CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS
    T r{};
    return __builtin_add_overflow(x, y, &r) ? detail::add_bound(x, std::is_signed<T>{}) : r;
#else
    return detail::add_overflows(x, y, std::is_signed<T>{}) ? detail::add_bound(x, std::is_signed<T>{})
                                                            : static_cast<T>(x + y);
#endif
}

/// Polyfill for std::saturating_sub.
template <typename T, detail::enable_if_sat_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_sub(T x, T y) noexcept
{
#if CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS
    T r{};
    return __builtin_sub_overflow(x, y, &r) ? detail::sub_bound(x, std::is_signed<T>{}) : r;
#else
    return detail::sub_overflows(x, y, std::is_signed<T>{}) ? detail::sub_bound(x, std::is_signed<T>{})
                                                            : static_cast<T>(x - y);
#endif
}

/// Polyfill for std::saturating_mul.
template <typename T, detail::enable_if_sat_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_mul(T x, T y) noexcept
{
#if CETL_PF26_DETAIL_HAS_OVERFLOW_BUILTINS
    T r{};
    return __builtin_mul_overflow(x, y, &r) ? detail::mul_bound(x, y, std::is_signed<T>{}) : r;
#else
    return detail::mul_overflows(x, y, std::is_signed<T>{}) ? detail::mul_bound(x, y, std::is_signed<T>{})
                                                            : static_cast<T>(x * y);
#endif
}

/// Polyfill for std::saturating_div. Precondition: y != 0 (as in the standard; checked by CETL_DEBUG_ASSERT).
template <typename T, detail::enable_if_sat_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturating_div(T x, T y) noexcept
{
    return CETL_DEBUG_ASSERT(y != 0, "saturating_div: division by zero"),
           detail::is_min_over_minus_one(x, y, std::is_signed<T>{}) ? std::numeric_limits<T>::max()
                                                                    : static_cast<T>(x / y);
}

/// Polyfill for std::saturating_cast.
template <typename T, typename U, detail::enable_if_sat_integer_t<T> = 0, detail::enable_if_sat_integer_t<U> = 0>
CETL_NODISCARD constexpr T saturating_cast(U x) noexcept
{
    return detail::cmp_less(x, std::numeric_limits<T>::min())      ? std::numeric_limits<T>::min()
           : detail::cmp_greater(x, std::numeric_limits<T>::max()) ? std::numeric_limits<T>::max()
                                                                   : static_cast<T>(x);
}

}  // namespace pf26
}  // namespace cetl

#endif  // CETL_PF26_NUMERIC_HPP_INCLUDED
