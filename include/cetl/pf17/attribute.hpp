/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_ATTRIBUTE_HPP_INCLUDED
#define CETL_PF17_ATTRIBUTE_HPP_INCLUDED

/// CETL_NODISCARD
#ifndef CETL_NODISCARD
#    if __cplusplus >= 201703L
#        define CETL_NODISCARD [[nodiscard]]
#    else
#        if defined(__GNUC__) || defined(__clang__)
#            define CETL_NODISCARD __attribute__((warn_unused_result))
#        else
#            define CETL_NODISCARD
#        endif
#    endif
#endif

/// CETL_MAYBE_UNUSED
#ifndef CETL_MAYBE_UNUSED
#    if __cplusplus >= 201703L
#        define CETL_MAYBE_UNUSED [[maybe_unused]]
#    else
#        if defined(__GNUC__) || defined(__clang__)
#            define CETL_MAYBE_UNUSED __attribute__((unused))
#        else
#            define CETL_MAYBE_UNUSED
#        endif
#    endif
#endif

#endif  // CETL_PF17_ATTRIBUTE_HPP_INCLUDED
