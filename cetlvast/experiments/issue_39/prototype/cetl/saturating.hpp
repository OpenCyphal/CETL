/// Prototype of cetl/saturating.hpp: CETL-specific extensions on top of the C++26 saturation functions.
#ifndef CETL_SATURATING_HPP_INCLUDED
#define CETL_SATURATING_HPP_INCLUDED

#include "cetl/pf26/cetlpf.hpp"
#include "cetl/pf26/numeric.hpp"  // for the detail traits (would move to a shared detail header)
#include <cstddef>
#include <limits>
#include <type_traits>

namespace cetl
{
namespace detail
{
template <std::size_t Bits, typename T>
constexpr T bits_max() noexcept
{
    // max() >> (width - Bits) is the largest value of an N-bit integer with T's signedness.
    return static_cast<T>(std::numeric_limits<T>::max() >>
                          (std::numeric_limits<T>::digits + (std::is_signed<T>::value ? 1 : 0) - Bits));
}
template <std::size_t Bits, typename T>
constexpr T bits_min(std::true_type /*signed*/) noexcept
{
    return static_cast<T>(-bits_max<Bits, T>() - 1);
}
template <std::size_t Bits, typename T>
constexpr T bits_min(std::false_type /*unsigned*/) noexcept
{
    return static_cast<T>(0);
}
}  // namespace detail

/// Saturate an integer to the range of an N-bit integer of the same signedness (DSDL `saturated intN`/`uintN`).
template <std::size_t Bits, typename T, pf26::detail::enable_if_sat_integer_t<T> = 0>
CETL_NODISCARD constexpr T saturate_to_bits(T x) noexcept
{
    static_assert((Bits > 0) && (Bits <= std::numeric_limits<T>::digits + (std::is_signed<T>::value ? 1 : 0)),
                  "Bits must be in [1, bit width of T]");
    return (x > detail::bits_max<Bits, T>())                        ? detail::bits_max<Bits, T>()
           : (x < detail::bits_min<Bits, T>(std::is_signed<T>{})) ? detail::bits_min<Bits, T>(std::is_signed<T>{})
                                                                  : x;
}

/// A thin wrapper (cf. Rust std::num::Saturating<T>) whose operators saturate instead of wrapping/overflowing.
/// Binary operators are defined only between two saturating<T> of the same T; construction from exactly T is
/// implicit, construction from any other integer type is explicit and saturates.
template <typename T>
class saturating final
{
    static_assert(pf26::detail::is_signed_or_unsigned_integer<T>::value, "T must be a signed or unsigned integer type");

public:
    using value_type = T;

    constexpr saturating() noexcept = default;

    template <typename U, std::enable_if_t<std::is_same<U, T>::value, int> = 0>
    constexpr saturating(U v) noexcept  // NOLINT(google-explicit-constructor) implicit from exactly T by design
        : value_{v}
    {
    }
    template <typename U,
              std::enable_if_t<!std::is_same<U, T>::value && pf26::detail::is_signed_or_unsigned_integer<U>::value, int> = 0>
    constexpr explicit saturating(U v) noexcept
        : value_{saturating_cast<T>(v)}
    {
    }

    CETL_NODISCARD constexpr T value() const noexcept
    {
        return value_;
    }
    CETL_NODISCARD constexpr explicit operator T() const noexcept
    {
        return value_;
    }

    constexpr saturating& operator+=(saturating o) noexcept
    {
        value_ = saturating_add(value_, o.value_);
        return *this;
    }
    constexpr saturating& operator-=(saturating o) noexcept
    {
        value_ = saturating_sub(value_, o.value_);
        return *this;
    }
    constexpr saturating& operator*=(saturating o) noexcept
    {
        value_ = saturating_mul(value_, o.value_);
        return *this;
    }
    constexpr saturating& operator/=(saturating o) noexcept
    {
        value_ = saturating_div(value_, o.value_);
        return *this;
    }
    constexpr saturating& operator++() noexcept
    {
        return *this += saturating{static_cast<T>(1)};
    }
    constexpr saturating& operator--() noexcept
    {
        return *this -= saturating{static_cast<T>(1)};
    }

    template <typename U = T, std::enable_if_t<std::is_signed<U>::value, int> = 0>
    CETL_NODISCARD constexpr saturating operator-() const noexcept
    {
        return saturating{saturating_sub(static_cast<T>(0), value_)};
    }

    CETL_NODISCARD friend constexpr saturating operator+(saturating a, saturating b) noexcept
    {
        return a += b;
    }
    CETL_NODISCARD friend constexpr saturating operator-(saturating a, saturating b) noexcept
    {
        return a -= b;
    }
    CETL_NODISCARD friend constexpr saturating operator*(saturating a, saturating b) noexcept
    {
        return a *= b;
    }
    CETL_NODISCARD friend constexpr saturating operator/(saturating a, saturating b) noexcept
    {
        return a /= b;
    }
    CETL_NODISCARD friend constexpr bool operator==(saturating a, saturating b) noexcept
    {
        return a.value_ == b.value_;
    }
    CETL_NODISCARD friend constexpr bool operator!=(saturating a, saturating b) noexcept
    {
        return a.value_ != b.value_;
    }
    CETL_NODISCARD friend constexpr bool operator<(saturating a, saturating b) noexcept
    {
        return a.value_ < b.value_;
    }

private:
    T value_{};
};

}  // namespace cetl

#endif  // CETL_SATURATING_HPP_INCLUDED
