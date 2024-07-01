/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETLVAST_HELPERS_RTTI_HPP
#define CETLVAST_HELPERS_RTTI_HPP

#include "cetl/rtti.hpp"

#include <complex>
#include <cstdint>
#include <functional>
#include <string>
#include <vector>

namespace cetl
{

// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)

// 6B02B2B9-610B-414E-9304-E7FC5BC0D061
template <>
constexpr type_id type_id_getter<bool>() noexcept
{
    return {0x6B, 0x02, 0xB2, 0xB9, 0x61, 0x0B, 0x41, 0x4E, 0x93, 0x04, 0xE7, 0xFC, 0x5B, 0xC0, 0xD0, 0x61};
}
// AA3F7C4D-0E44-43CB-AB4C-2AE19E646F91
template <>
constexpr type_id type_id_getter<int>() noexcept
{
    return {0xAA, 0x3F, 0x7C, 0x4D, 0x0E, 0x44, 0x43, 0xCB, 0xAB, 0x4C, 0x2A, 0xE1, 0x9E, 0x64, 0x6F, 0x91};
}
// 42844900-45ED-41A0-AA63-D6A42B60B343
template <>
constexpr type_id type_id_getter<float>() noexcept
{
    return {0x42, 0x84, 0x49, 0x00, 0x45, 0xED, 0x41, 0xA0, 0xAA, 0x63, 0xD6, 0xA4, 0x2B, 0x60, 0xB3, 0x43};
}
// 6B5BE490-194C-4E2E-B8DE-3BB15CC52777
template <>
constexpr type_id type_id_getter<double>() noexcept
{
    return {0x6B, 0x5B, 0xE4, 0x90, 0x19, 0x4C, 0x4E, 0x2E, 0xB8, 0xDE, 0x3B, 0xB1, 0x5C, 0xC5, 0x27, 0x77};
}

// 05855903-D323-41C3-8C58-691E035507D8
template <>
constexpr type_id type_id_getter<char>() noexcept
{
    return {0x05, 0x85, 0x59, 0x03, 0xD3, 0x23, 0x41, 0xC3, 0x8C, 0x58, 0x69, 0x1E, 0x03, 0x55, 0x07, 0xD8};
}
// 6BC0579E-B665-480A-AFB0-45DB755A143E
template <>
constexpr type_id type_id_getter<std::uint8_t>() noexcept
{
    return {0x6B, 0xC0, 0x57, 0x9E, 0xB6, 0x65, 0x48, 0x0A, 0xAF, 0xB0, 0x45, 0xDB, 0x75, 0x5A, 0x14, 0x3E};
}
// 3C22EF31-63C0-4710-9AAE-966E89134C19
template <>
constexpr type_id type_id_getter<std::uint16_t>() noexcept
{
    return {0x3C, 0x22, 0xEF, 0x31, 0x63, 0xC0, 0x47, 0x10, 0x9A, 0xAE, 0x96, 0x6E, 0x89, 0x13, 0x4C, 0x19};
}
// 89A2F7BC-5BEA-47BF-96C4-CFFA3A2DBBB2
template <>
constexpr type_id type_id_getter<std::uint32_t>() noexcept
{
    return {0x89, 0xA2, 0xF7, 0xBC, 0x5B, 0xEA, 0x47, 0xBF, 0x96, 0xC4, 0xCF, 0xFA, 0x3A, 0x2D, 0xBB, 0xB2};
}
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