/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_HELPERS_HPP_INCLUDED
#define CETL_HELPERS_HPP_INCLUDED

namespace cetl::detail
{
template <bool Copyable, bool Movable>
struct enable_copy_move_construction;
template <>
struct enable_copy_move_construction<true, true>
{
    enable_copy_move_construction()                                                = default;
    enable_copy_move_construction(const enable_copy_move_construction&)            = default;
    enable_copy_move_construction(enable_copy_move_construction&&)                 = default;
    enable_copy_move_construction& operator=(const enable_copy_move_construction&) = default;
    enable_copy_move_construction& operator=(enable_copy_move_construction&&)      = default;
};
template <>
struct enable_copy_move_construction<true, false>
{
    enable_copy_move_construction()                                                = default;
    enable_copy_move_construction(const enable_copy_move_construction&)            = default;
    enable_copy_move_construction(enable_copy_move_construction&&)                 = delete;
    enable_copy_move_construction& operator=(const enable_copy_move_construction&) = default;
    enable_copy_move_construction& operator=(enable_copy_move_construction&&)      = default;
};
template <>
struct enable_copy_move_construction<false, true>
{
    enable_copy_move_construction()                                                = default;
    enable_copy_move_construction(const enable_copy_move_construction&)            = delete;
    enable_copy_move_construction(enable_copy_move_construction&&)                 = default;
    enable_copy_move_construction& operator=(const enable_copy_move_construction&) = default;
    enable_copy_move_construction& operator=(enable_copy_move_construction&&)      = default;
};
template <>
struct enable_copy_move_construction<false, false>
{
    enable_copy_move_construction()                                                = default;
    enable_copy_move_construction(const enable_copy_move_construction&)            = delete;
    enable_copy_move_construction(enable_copy_move_construction&&)                 = delete;
    enable_copy_move_construction& operator=(const enable_copy_move_construction&) = default;
    enable_copy_move_construction& operator=(enable_copy_move_construction&&)      = default;
};

template <bool Copyable, bool Movable>
struct enable_copy_move_assignment;
template <>
struct enable_copy_move_assignment<true, true>
{
    enable_copy_move_assignment()                                              = default;
    enable_copy_move_assignment(const enable_copy_move_assignment&)            = default;
    enable_copy_move_assignment(enable_copy_move_assignment&&)                 = default;
    enable_copy_move_assignment& operator=(const enable_copy_move_assignment&) = default;
    enable_copy_move_assignment& operator=(enable_copy_move_assignment&&)      = default;
};
template <>
struct enable_copy_move_assignment<true, false>
{
    enable_copy_move_assignment()                                              = default;
    enable_copy_move_assignment(const enable_copy_move_assignment&)            = default;
    enable_copy_move_assignment(enable_copy_move_assignment&&)                 = default;
    enable_copy_move_assignment& operator=(const enable_copy_move_assignment&) = default;
    enable_copy_move_assignment& operator=(enable_copy_move_assignment&&)      = delete;
};
template <>
struct enable_copy_move_assignment<false, true>
{
    enable_copy_move_assignment()                                              = default;
    enable_copy_move_assignment(const enable_copy_move_assignment&)            = default;
    enable_copy_move_assignment(enable_copy_move_assignment&&)                 = default;
    enable_copy_move_assignment& operator=(const enable_copy_move_assignment&) = delete;
    enable_copy_move_assignment& operator=(enable_copy_move_assignment&&)      = default;
};
template <>
struct enable_copy_move_assignment<false, false>
{
    enable_copy_move_assignment()                                              = default;
    enable_copy_move_assignment(const enable_copy_move_assignment&)            = default;
    enable_copy_move_assignment(enable_copy_move_assignment&&)                 = default;
    enable_copy_move_assignment& operator=(const enable_copy_move_assignment&) = delete;
    enable_copy_move_assignment& operator=(enable_copy_move_assignment&&)      = delete;
};

}  // namespace cetl::detail

#endif  // CETL_HELPERS_HPP_INCLUDED
