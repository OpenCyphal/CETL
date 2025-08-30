/// @file
/// Example of using CETL memory_resource types.
///
/// This file demonstrates using cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// SPDX-License-Identifier: MIT
///
#include "cetl/pmr/o1heap_memory_resource_delegate.hpp"

#include <gtest/gtest.h>

namespace cetl
{
namespace pf17
{

TEST(example_11_memory_resource_01heap, main)
{
    //! [main]
    // We'll use the aligned-storage helper class to defined properly aligned storage for the 01-heap areana. As this
    // is a large chunk we'll make it static to keep it off the stack.
    static cetl::pmr::O1HeapAlignedStorage<0x100000> large_buffer{};

    // Now we can use our test subject to allocate and deallocate memory.
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{large_buffer};

    // Note that, until https://github.com/pavel-kirienko/o1heap/issues/13 is fixed the alignment doesn't have
    // any effect on allocation requests.
    void* mem = test_subject.allocate(8);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(mem, 8);
    //! [main]
}

}  // namespace pf17
}  // namespace cetl
