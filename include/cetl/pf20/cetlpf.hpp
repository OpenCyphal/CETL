/// @file
/// CETL polyfill header for C++20 types
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
/// CETL polyfill headers will provide the CETL type as an aliased standard type in the std namespace when compiling
/// using older C++ libraries and will automatically provide types from the standard library when compiling
/// using newer C++ libraries. This allows for causal use of CETL as a true polyfill library but does violate certain
/// coding standards. As such, for more critical software we recommend not using these headers but including the
/// types you use from `cetl::pf20` directly.
///
/// For example, using the polyfill header an program like this can be written:
///
/// @include example_01_polyfill.cpp
///
/// In that example the code automatically switches to using the C++ 20 version of span when compiling
/// with that standard enabled. For more critical code you will need to do something like this:
///
/// @include example_02_polyfill.cpp
///
/// In that example the span type can be easily redefined but it will not be automatically redefined.
/// In these cases something like this might also be in order:
///
/// ```
/// static_assert(__cplusplus < CETL_CPP_STANDARD_20, "Don't use CETL if you are compiling for C++20.");
/// ```
///

#ifndef CETL_PF20_H_INCLUDED
#define CETL_PF20_H_INCLUDED

/// @namespace cetl::pf20 This namespace contains C++20 polyfill (pf) types.
///     The types within this namespace adhere to the C++20 specification and should be drop-in replaceable
///     with said standard types where they are available. CETL polyfill types may implement a sub-set of
///     the required functionality but they will not implement non-compliant functionality.
#include "cetl/cetl.hpp"
#include "cetl/pf17/cetlpf.hpp"

#if (__cplusplus >= CETL_CPP_STANDARD_20 && !defined(CETL_DOXYGEN))
#    include <span>

namespace cetl
{
constexpr std::size_t dynamic_extent = std::dynamic_extent;

template <typename T, std::size_t Extent = dynamic_extent>
using span = std::span<T, Extent>;
}  // namespace cetl
#else
#    include "cetl/pf20/span.hpp"

namespace cetl
{
constexpr std::size_t dynamic_extent = cetl::pf20::dynamic_extent;

template <typename T, std::size_t Extent = dynamic_extent>
using span = cetl::pf20::span<T, Extent>;

}  // namespace cetl
#endif

#endif  // CETL_PF20_H_INCLUDED
