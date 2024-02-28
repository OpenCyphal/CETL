/// @file
/// Attribute polyfills for C++17.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_ATTRIBUTE_HPP_INCLUDED
#define CETL_PF17_ATTRIBUTE_HPP_INCLUDED

/// CETL_NODISCARD
#ifndef CETL_NODISCARD
#    if (__cplusplus >= 201703L) || defined(CETL_DOXYGEN)
/// A compatibility macros that expands to \c [[nodiscard]] if C++17 or later is used, otherwise it expands to
/// a compiler-specific alternative if one is known, otherwise it expands to nothing.
#        define CETL_NODISCARD [[nodiscard]]
#    else
#        if defined(__GNUC__) || defined(__clang__)
#            define CETL_NODISCARD __attribute__((warn_unused_result))
#        else
#            define CETL_NODISCARD
#        endif
#    endif
#endif

#endif  // CETL_PF17_ATTRIBUTE_HPP_INCLUDED
