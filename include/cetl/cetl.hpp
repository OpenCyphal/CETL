/// @file
/// CETL common header.
///
/// @note
/// Keep this very spare. CETL's desire is to adapt to future C++ standards
/// and too many CETL-specific definitions makes it difficult for users to switch off of CETL in the
/// future.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_H_INCLUDED
#define CETL_H_INCLUDED

/// @def CETL_DEBUG_ASSERT
/// When `CETL_ENABLE_DEBUG_ASSERT` is defined and not 0 then this is redirected to
/// assert as included from `<cassert>`. Because assert does not support a failure message
/// we embed the `m` argument in a comma operator statement such that the compiler places the
/// both the text and the failure clause `c` in the text section of the binary which is normally
/// shown from standard error when an assert occurs.
///
/// When `CETL_ENABLE_DEBUG_ASSERT` is <em>not</em> defined or is 0 then these statements have no
/// effect and should not effect the resulting binary.
///
/// @warning
/// Define `CETL_ENABLE_DEBUG_ASSERT` as 1 to enable assertions within CETL code. Enabling this
/// in production code is <em>strongly</em> discouraged.
///
#if defined CETL_ENABLE_DEBUG_ASSERT && CETL_ENABLE_DEBUG_ASSERT
#    include <cassert>
#    define CETL_DEBUG_ASSERT(c, m) assert(((void) m, c))
#else
#    define CETL_DEBUG_ASSERT(c, m) ((void) m)
#endif  // CETL_ENABLE_DEBUG_ASSERT

// For example: https://godbolt.org/z/Thsn8qf1a
// We define these in a common header since we might encounter odd values on some compilers that we'll have to
// provide special cases for.

/// @defgroup CETL_CPP_STANDARD Guaranteed CETL c++ standard numbers
/// These macros are an AUTOSAR-14 Rule A16-0-1 violation but can be used to conditionally include headers which
/// is compliant with A16-0-1.
/// @{

/// @def CETL_CPP_STANDARD_14
/// Provides the proper value to test against `__cplusplus` for c++14.
/// ```
/// #if __cplusplus >= CETL_CPP_STANDARD_14
/// #include <something_from_cpp_14>
/// #endif
/// ```
#define CETL_CPP_STANDARD_14 201402L

/// @def CETL_CPP_STANDARD_17
/// Provides the proper value to test against `__cplusplus` for c++14.
/// ```
/// #if __cplusplus >= CETL_CPP_STANDARD_17
/// #include <something_from_cpp_14>
/// #include <something_from_cpp_17>
/// #endif
/// ```
#define CETL_CPP_STANDARD_17 201703L

/// @def CETL_CPP_STANDARD_20
/// Provides the proper value to test against `__cplusplus` for c++14.
/// ```
/// #if __cplusplus >= CETL_CPP_STANDARD_17
/// #include <something_from_cpp_14>
/// #include <something_from_cpp_17>
/// #include <something_from_cpp_20>
/// #endif
/// ```
#define CETL_CPP_STANDARD_20 202002L

/// @}
#endif  // CETL_H_INCLUDED
