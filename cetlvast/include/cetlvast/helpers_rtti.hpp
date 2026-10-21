/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETLVAST_HELPERS_RTTI_HPP
#define CETLVAST_HELPERS_RTTI_HPP

#include "cetl/rtti.hpp"

#include <complex>
#include <functional>
#include <string>
#include <vector>

namespace cetl
{

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// A0672C3A-C6D2-4BF5-990A-1A4601264D60
template <>
constexpr type_id type_id_getter<std::string>() noexcept
{
    return {0xA0, 0x67, 0x2C, 0x3A, 0xC6, 0xD2, 0x4B, 0xF5, 0x99, 0x0A, 0x1A, 0x46, 0x01, 0x26, 0x4D, 0x60};
}
// 473A0E53-86AB-4426-9F32-732D519F940D
template <>
constexpr type_id type_id_getter<std::complex<double>>() noexcept
{
    return {0x47, 0x3A, 0x0E, 0x53, 0x86, 0xAB, 0x44, 0x26, 0x9F, 0x32, 0x73, 0x2D, 0x51, 0x9F, 0x94, 0x0D};
}
// D30E9194-8ECB-4831-9B31-F73C031DBFFB
template <>
constexpr type_id type_id_getter<std::function<const char*()>>() noexcept
{
    return {0xD3, 0x0E, 0x91, 0x94, 0x8E, 0xCB, 0x48, 0x31, 0x9B, 0x31, 0xF7, 0x3C, 0x03, 0x1D, 0xBF, 0xFB};
}
// 63E796F8-AAFC-4E61-B545-99CE28B796FD
template <>
constexpr type_id type_id_getter<std::vector<char>>() noexcept
{
    return {0x63, 0xE7, 0x96, 0xF8, 0xAA, 0xFC, 0x4E, 0x61, 0xB5, 0x45, 0x99, 0xCE, 0x28, 0xB7, 0x96, 0xFD};
}

// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

}  // namespace cetl

#endif  // CETLVAST_HELPERS_RTTI_HPP
