/// @file
/// Defines a function compatible with C++17 std::launder.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PF17_NEW_H_INCLUDED
#define CETL_PF17_NEW_H_INCLUDED

#include <new>
#include <type_traits>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

namespace cetl
{
namespace pf17
{

/// Returns a pointer to the object of type @p T that is located at the address @p p holds.
///
/// Since C++17 (P0137R1) a pointer obtained by casting the address of some storage does not point to an object that
/// was later created in that storage with placement new; it has to be passed through std::launder first. This is
/// std::launder where the standard library provides it. Before C++17 the cast pointer already points to the object, so
/// this returns @p p unchanged.
///
/// @tparam T   The type of the object. Must not be a function type or (cv-qualified) `void`.
/// @param  p   The address of an object of type @p T that is within its lifetime.
/// @return A pointer to that object.
template <typename T>
CETL_NODISCARD constexpr T* launder(T* const p) noexcept
{
    static_assert(!std::is_function<T>::value, "launder of a pointer to function is ill-formed.");
    static_assert(!std::is_void<std::remove_cv_t<T>>::value, "launder of a pointer to void is ill-formed.");
#if defined(__cpp_lib_launder)
    return std::launder(p);
#else
    return p;
#endif
}

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_NEW_H_INCLUDED
