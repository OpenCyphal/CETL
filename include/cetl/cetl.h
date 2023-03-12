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

//
// @note CETL_ENABLE_DEBUG_ASSERT
// Define as 1 to enable library assertions. Enabling this in production code is
// <em>strongly</em> discouraged.
//
#ifndef CETL_ENABLE_DEBUG_ASSERT
#    define CETL_ENABLE_DEBUG_ASSERT 1
#endif

#if (CETL_ENABLE_DEBUG_ASSERT)
#    ifdef NDEBUG
#        error "Cannot enable CETL debug assert in non-debug builds."
#    else
#        include <cassert>
#        define CETL_DEBUG_ASSERT(c, m) assert(((void) m, c))
#    endif
#else
#    define CETL_DEBUG_ASSERT(c, m) ((void) m)
#endif  // CETL_ENABLE_DEBUG_ASSERT

// For example: https://godbolt.org/z/Thsn8qf1a
// We define these in a common header since we might encounter odd values on some compilers that we'll have to
// provide special cases for.

#define CETL_CPP_STANDARD_14 201402L
#define CETL_CPP_STANDARD_17 201703L
#define CETL_CPP_STANDARD_20 202002L

#endif  // CETL_H_INCLUDED
