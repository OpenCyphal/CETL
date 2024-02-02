/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_UTILITY_HPP_INCLUDED
#define CETL_UTILITY_HPP_INCLUDED

namespace cetl
{
namespace pf17
{

/// Polyfill for std::in_place_t.
struct in_place_t
{
    explicit in_place_t() = default;
};

/// Polyfill for std::in_place.
inline constexpr in_place_t in_place{};

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_UTILITY_HPP_INCLUDED
