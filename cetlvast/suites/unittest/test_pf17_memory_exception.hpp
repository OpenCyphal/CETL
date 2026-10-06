/// @file
/// Exception injection from an exception-enabled translation unit into embedded-profile tests.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#ifndef CETLVAST_TEST_PF17_MEMORY_EXCEPTION_HPP_INCLUDED
#define CETLVAST_TEST_PF17_MEMORY_EXCEPTION_HPP_INCLUDED

namespace cetlvast
{
namespace pf17_memory_test
{
/// Injects an uncaught exception into an exception-disabled caller.
/// This function never returns normally.
/// @throws int Always throws the value 206 from this exception-enabled translation unit.
[[noreturn]] void throw_exception();
}  // namespace pf17_memory_test
}  // namespace cetlvast

#endif  // CETLVAST_TEST_PF17_MEMORY_EXCEPTION_HPP_INCLUDED
