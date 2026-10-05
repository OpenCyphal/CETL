/// @file
/// Compiled with -fexceptions even when the calling polyfill tests use -fno-exceptions.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "test_pf17_memory_exception.hpp"

#if !defined(__cpp_exceptions)
#    error "The exception injection helper must be compiled with exceptions enabled."
#endif

namespace cetlvast
{
namespace pf17_memory_test
{
void throw_exception()
{
    throw 206;
}
}  // namespace pf17_memory_test
}  // namespace cetlvast
