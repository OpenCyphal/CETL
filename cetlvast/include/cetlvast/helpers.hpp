/// @file
/// CETL VerificAtion SuiTe – Test suite helpers.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETLVAST_HELPERS_H_INCLUDED
#define CETLVAST_HELPERS_H_INCLUDED

#include <type_traits>
#include <utility>
#include <memory>

#if defined(__clang__)
#    if __has_feature(cxx_rtti)
#        define CETLVAST_RTTI_ENABLED
#    endif
#elif defined(__GNUC__)
#    if defined(__GXX_RTTI)
#        define CETLVAST_RTTI_ENABLED
#    endif
#elif defined(_MSC_VER)
#    if defined(_CPPRTTI)
#        define CETLVAST_RTTI_ENABLED
#    endif
#endif

namespace cetlvast
{

template <typename T>
constexpr bool is_power_of_two(const T& value)
{
    return (value && !static_cast<bool>(value & (value - 1)));
}

template <typename T>
constexpr bool is_aligned(T* object, std::size_t alignment)
{
    void*       object_ptr_variable = object;
    std::size_t sizeof_t_variable   = sizeof(T);
    return (nullptr != std::align(alignment, sizeof(T), object_ptr_variable, sizeof_t_variable));
}

template <typename T>
constexpr bool is_aligned(T* object)
{
    return is_aligned<T>(object, alignof(T));
}

}  // namespace cetlvast

#endif  // CETLVAST_HELPERS_H_INCLUDED
