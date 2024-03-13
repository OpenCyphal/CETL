/// @file
/// Defines the C++17 `std::any` type and several related entities.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_ANY_HPP_INCLUDED
#define CETL_PF17_ANY_HPP_INCLUDED

#include <cetl/pf17/attribute.hpp>

#include <typeinfo>   // We need this for `std::bad_cast`.
#include <exception>  // We need this even if exceptions are disabled for `std::terminate`.

namespace cetl  // NOLINT(*-concat-nested-namespaces) TODO: Fix this when C++17.
{
namespace pf17
{
class any;

#if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)

/// A polyfill for `std::bad_any_cast`.
/// This is only available if exceptions are enabled (`__cpp_exceptions` is defined).
class bad_any_cast : public std::bad_cast
{
public:
    bad_any_cast() noexcept                               = default;
    bad_any_cast(const bad_any_cast&) noexcept            = default;
    bad_any_cast(bad_any_cast&&) noexcept                 = default;
    bad_any_cast& operator=(const bad_any_cast&) noexcept = default;
    bad_any_cast& operator=(bad_any_cast&&) noexcept      = default;
    ~bad_any_cast() noexcept override                     = default;

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "bad_any_cast";
    }
};

#endif  // defined(__cpp_exceptions) || defined(CETL_DOXYGEN)

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_ANY_HPP_INCLUDED
