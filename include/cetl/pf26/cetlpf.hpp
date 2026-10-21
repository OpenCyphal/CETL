/// @file
/// CETL polyfill header for C++26 types
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// @warning
/// This header violates AUTOSAR-14 M7-3-6, A17-0-1, and A16-0-1 (and possibly other rules). Don't use CETL polyfill
/// headers in AUTOSAR code.
///
/// @warning
/// polyfill headers cannot be used if CETL_H_ERASE is defined.
///
/// The saturation arithmetic functions are selected by the library feature-test macro rather than by `__cplusplus`,
/// because the standard library's support lags the language mode and because the functions were renamed late:
///
/// - `__cpp_lib_saturation_arithmetic >= 202603L`: the standard's `std::saturating_add` family is used directly.
/// - `__cpp_lib_saturation_arithmetic >= 202311L`: the standard library only has the earlier P0543R3 names
///   (`std::add_sat`, `std::saturate_cast`, ...). CETL forwards to them under the final names, so user code is
///   written once.
/// - otherwise: cetl::pf26 provides the functions.
///
/// For example, a counter that must stop at its maximum rather than wrap:
///
/// @snippet{trimleft} example_13_saturating_arithmetic.cpp example_13_saturating_add
///
/// and a narrowing conversion that clamps instead of truncating:
///
/// @snippet{trimleft} example_13_saturating_arithmetic.cpp example_13_saturating_cast
///
/// (@ref example_13_saturating_arithmetic "See full example here...")
///

#ifndef CETL_PF26_H_INCLUDED
#define CETL_PF26_H_INCLUDED

/// @namespace cetl::pf26 This namespace contains C++26 polyfill (pf) types.
///     The types within this namespace adhere to the C++26 specification and should be drop-in replaceable
///     with said standard types where they are available. CETL polyfill types may implement a sub-set of
///     the required functionality but they will not implement non-compliant functionality.
#include "cetl/cetl.hpp"
#include "cetl/pf20/cetlpf.hpp"

#include <numeric>  // Defines __cpp_lib_saturation_arithmetic where the standard library provides the feature.

#if defined(__cpp_lib_saturation_arithmetic) && (__cpp_lib_saturation_arithmetic >= 202603L) && !defined(CETL_DOXYGEN)

namespace cetl
{
using std::saturating_add;
using std::saturating_sub;
using std::saturating_mul;
using std::saturating_div;
using std::saturating_cast;
}  // namespace cetl

#elif defined(__cpp_lib_saturation_arithmetic) && (__cpp_lib_saturation_arithmetic >= 202311L) && !defined(CETL_DOXYGEN)

namespace cetl
{
// The standard library implements P0543R3 but not the P4052R0 renaming. The standard's constraints apply.
template <typename T>
CETL_NODISCARD constexpr T saturating_add(T x, T y) noexcept
{
    return std::add_sat(x, y);
}
template <typename T>
CETL_NODISCARD constexpr T saturating_sub(T x, T y) noexcept
{
    return std::sub_sat(x, y);
}
template <typename T>
CETL_NODISCARD constexpr T saturating_mul(T x, T y) noexcept
{
    return std::mul_sat(x, y);
}
template <typename T>
CETL_NODISCARD constexpr T saturating_div(T x, T y) noexcept
{
    return std::div_sat(x, y);
}
template <typename T, typename U>
CETL_NODISCARD constexpr T saturating_cast(U x) noexcept
{
    return std::saturate_cast<T>(x);
}
}  // namespace cetl

#else
#    include "cetl/pf26/numeric.hpp"

namespace cetl
{
using cetl::pf26::saturating_add;
using cetl::pf26::saturating_sub;
using cetl::pf26::saturating_mul;
using cetl::pf26::saturating_div;
using cetl::pf26::saturating_cast;
}  // namespace cetl

#endif

#endif  // CETL_PF26_H_INCLUDED
