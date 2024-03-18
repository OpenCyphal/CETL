/// @file
/// CETL polyfill header for C++17 types
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
/// types you use from `cetl::pf17` directly.
///
/// For example, (TODO: polyfill 17 examples.)
///

#ifndef CETL_PF17_H_INCLUDED
#define CETL_PF17_H_INCLUDED

#include "cetl/cetl.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"

/// @namespace cetl::pf17 This namespace contains C++17 polyfill (pf) types.
///     The types within this namespace adhere to the C++17 specification and should be drop-in replaceable
///     with said standard types where they are available. CETL polyfill types may implement a sub-set of
///     the required functionality but they will not implement non-compliant functionality.
///
/// @namespace cetl::pf17::pmr          CETL polyfill types for the standard Polymorphic Memory Resource (PMR)
///                                     namespace.
/// @namespace cetl::pf17::pmr::deviant Types or methods that deviate from the C++17 specification.
///
#if (__cplusplus >= CETL_CPP_STANDARD_17 && !defined(CETL_DOXYGEN))
#    include <memory_resource>
#    include <cstddef>
#    include <optional>
#    include <variant>
#    include <any>

namespace cetl
{
using byte = std::byte;

// pmr
namespace pmr
{
using memory_resource = std::pmr::memory_resource;
inline memory_resource* null_memory_resource() noexcept
{
    return std::pmr::null_memory_resource();
}
template <typename T>
using polymorphic_allocator = std::pmr::polymorphic_allocator<T>;
inline memory_resource* new_delete_resource() noexcept
{
    return std::pmr::new_delete_resource();
}
}  // namespace pmr

// utility
using std::in_place;
using std::in_place_t;
using std::in_place_type;
using std::in_place_type_t;
using std::in_place_index;
using std::in_place_index_t;

// optional
using std::optional;
using std::nullopt_t;
using std::nullopt;
using std::make_optional;
using std::bad_optional_access;

// type_traits
using std::is_swappable;
using std::is_nothrow_swappable;
using std::is_swappable_v;
using std::is_nothrow_swappable_v;
using std::conjunction;
using std::conjunction_v;
using std::disjunction;
using std::disjunction_v;
using std::negation;
using std::negation_v;
using std::void_t;

// variant
using std::variant;
using std::monostate;
using std::bad_variant_access;
using std::variant_size;
using std::variant_size_v;
using std::variant_alternative;
using std::variant_alternative_t;
using std::variant_npos;
using std::get;
using std::get_if;
using std::visit;
using std::holds_alternative;

// any
using std::bad_any_cast;

}  // namespace cetl

#else
#    include "cetl/pf17/any.hpp"
#    include "cetl/pf17/byte.hpp"
#    include "cetl/pf17/utility.hpp"
#    include "cetl/pf17/optional.hpp"
#    include "cetl/pf17/variant.hpp"

namespace cetl
{

using byte = cetl::pf17::byte;

// pmr
namespace pmr
{
using memory_resource = cetl::pf17::pmr::memory_resource;
inline memory_resource* null_memory_resource() noexcept
{
    return cetl::pf17::pmr::null_memory_resource();
}
template <typename T>
using polymorphic_allocator = cetl::pf17::pmr::polymorphic_allocator<T>;
inline memory_resource* new_delete_resource() noexcept
{
    return cetl::pf17::pmr::new_delete_resource();
}
}  // namespace pmr

// utility
using cetl::pf17::in_place;
using cetl::pf17::in_place_t;
using cetl::pf17::in_place_type;
using cetl::pf17::in_place_type_t;
using cetl::pf17::in_place_index;
using cetl::pf17::in_place_index_t;

// optional
using cetl::pf17::optional;
using cetl::pf17::nullopt_t;
using cetl::pf17::nullopt;
using cetl::pf17::make_optional;
#    if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)
using cetl::pf17::bad_optional_access;
#    endif

// type_traits
using cetl::pf17::is_swappable;
using cetl::pf17::is_nothrow_swappable;
using cetl::pf17::is_swappable_v;
using cetl::pf17::is_nothrow_swappable_v;
using cetl::pf17::conjunction;
using cetl::pf17::conjunction_v;
using cetl::pf17::disjunction;
using cetl::pf17::disjunction_v;
using cetl::pf17::negation;
using cetl::pf17::negation_v;
using cetl::pf17::void_t;

// variant
using cetl::pf17::variant;
using cetl::pf17::monostate;
using cetl::pf17::variant_size;
using cetl::pf17::variant_size_v;
using cetl::pf17::variant_alternative;
using cetl::pf17::variant_alternative_t;
using cetl::pf17::variant_npos;
using cetl::pf17::get;
using cetl::pf17::get_if;
using cetl::pf17::visit;
using cetl::pf17::holds_alternative;
#    if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)
using cetl::pf17::bad_variant_access;
#    endif

// any
#    if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)
using cetl::pf17::bad_any_cast;
#    endif

}  // namespace cetl
#endif

#endif  // CETL_PF17_H_INCLUDED
