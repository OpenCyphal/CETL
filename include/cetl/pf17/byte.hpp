/// @file
/// Defines a type compatible with C++17 std::byte.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PF17_BYTE_H_INCLUDED
#define CETL_PF17_BYTE_H_INCLUDED

#include <cstddef>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

namespace cetl
{
namespace pf17
{

/// A non-character type that implements the concept of a byte defined by the C++17 specification.
enum class byte : unsigned char
{
};

/// TODO [support.types.byteops] non-member operations

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_BYTE_H_INCLUDED
