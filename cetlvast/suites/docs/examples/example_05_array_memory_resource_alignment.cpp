/// @file
/// Demonstration of memory alignment when using the cetl::pmr::UnsynchronizedArrayMemoryResource in
/// cetl/pf17/cetlpf.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include <iostream>

#include <gtest/gtest.h>

//![example_include]
#include "cetl/pmr/array_memory_resource.hpp"
#include "cetl/pf17/cetlpf.hpp"
//![example_include]

TEST(example_05_array_memory_resource_alignment, example_0)
{
    static_assert(alignof(std::max_align_t) < 128, "Wow, what hardware are you running on?");
    //![example_0]
    constexpr std::size_t                                                    BufferSizeBytes = 64;
    cetl::pf17::byte                                                         buffer[BufferSizeBytes];
    cetl::pmr::UnsynchronizedArrayMemoryResource<cetl::pmr::memory_resource> resource{buffer,
                                                                                      BufferSizeBytes,
                                                                                      cetl::pmr::null_memory_resource(),
                                                                                      0};

    // let's say we have a buffer that must be aligned to a 128-byte (1024-bit) boundary. If we tried to use
    // UnsynchronizedArrayMemoryResource with a 64-byte buffer, on a typical system, the allocation would fail.

    void* r = nullptr;
#if __cpp_exceptions
    try
    {
#endif
        r = resource.allocate(64, 128);
#if __cpp_exceptions
    } catch (const std::bad_alloc&)
    {
        // This is expected.
    }
#endif
    std::cout << "Over-aligned attempt failed: " << r << std::endl;

    //![example_0]
}
TEST(example_05_array_memory_resource_alignment, example_1)
{
    //![example_1]
    // By over-provisioning the buffer you can now get the alignment you want:
    constexpr std::size_t                                                    BufferSizeBytes = 64 + 128;
    cetl::pf17::byte                                                         buffer[BufferSizeBytes];
    cetl::pmr::UnsynchronizedArrayMemoryResource<cetl::pmr::memory_resource> resource{buffer,
                                                                                      BufferSizeBytes,
                                                                                      cetl::pmr::null_memory_resource(),
                                                                                      0};

    void* r = resource.allocate(64, 128);

    std::cout << "Over-aligned address at: " << r << std::endl;

    resource.deallocate(r, 64, 128);
    //![example_1]
}
