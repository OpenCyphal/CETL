/// @file
/// CETL polyfill header for C++23 types
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
/// The types are selected by the library feature-test macro rather than by `__cplusplus`, because the standard
/// library's support lags the language mode: if `__cpp_lib_expected >= 202211L` (std::expected with the monadic
/// operations) the standard types are used, otherwise cetl::pf23 provides them.
///
/// For example, a parser that reports why it failed without using exceptions:
///
/// @snippet{trimleft} example_14_expected.cpp example_14_expected_parse
///
/// and the monadic operations that chain such steps without checking each result by hand:
///
/// @snippet{trimleft} example_14_expected.cpp example_14_expected_chain
///
/// (@ref example_14_expected "See full example here...")
///

#ifndef CETL_PF23_H_INCLUDED
#define CETL_PF23_H_INCLUDED

/// @namespace cetl::pf23 This namespace contains C++23 polyfill (pf) types.
///     The types within this namespace adhere to the C++23 specification and should be drop-in replaceable
///     with said standard types where they are available. CETL polyfill types may implement a sub-set of
///     the required functionality but they will not implement non-compliant functionality.
#include "cetl/cetl.hpp"
#include "cetl/pf20/cetlpf.hpp"

#if (__cplusplus >= CETL_CPP_STANDARD_20)
#    include <version>  // Defines __cpp_lib_expected where the standard library provides the feature.
#endif

#if defined(__cpp_lib_expected) && (__cpp_lib_expected >= 202211L) && !defined(CETL_DOXYGEN)
#    include <expected>

namespace cetl
{
// expected
using std::expected;
using std::unexpected;
using std::bad_expected_access;
using std::unexpect_t;
using std::unexpect;
}  // namespace cetl

#else
#    include "cetl/pf23/expected.hpp"

namespace cetl
{
// expected
using cetl::pf23::expected;
using cetl::pf23::unexpected;
#    if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)
using cetl::pf23::bad_expected_access;
#    endif
using cetl::pf23::unexpect_t;
using cetl::pf23::unexpect;
}  // namespace cetl

#endif

#endif  // CETL_PF23_H_INCLUDED
