/// @file
/// Demonstration of memory alignment when using the cetl::pmr::UnsynchronizedArrayMemoryResource in
/// cetl/pmr/array_memory_resource.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

//![example_include]
#include "cetl/pf17/memory_resource.hpp"
namespace cetl
{
using byte = pf17::byte;
namespace pmr
{
using memory_resource = pf17::pmr::memory_resource;
}  // namespace pmr
}  // namespace cetl
#include "cetl/pmr/array_memory_resource.hpp"
//![example_include]

#include <iostream>

int main()
{
    {
        //![example_0]
        constexpr std::size_t                        BufferSizeBytes = 64;
        cetl::byte                                   buffer[BufferSizeBytes];
        cetl::pmr::UnsynchronizedArrayMemoryResource resource{buffer, BufferSizeBytes};

        // let's say we have a buffer that must be aligned to a 128-byte (1024-bit) boundary. If we tried to use
        // UnsynchronizedArrayMemoryResource with a 64-byte buffer, on a typical system, the allocation would fail.

        static_assert(alignof(std::max_align_t) < 128, "Wow, what hardware are you running on?");

        void* r = nullptr;
#if __cpp_exceptions
        try
        {
#endif
            r = resource.allocate(64, 128);
#if __cpp_exceptions
        } catch (const std::bad_alloc&)
        {
        }
#endif
        std::cout << "Over-aligned attempt failed: " << r << std::endl;

        //![example_0]
    }
    {
        //![example_1]
        // By over-provisioning the buffer you can now get the alignment you want:
        constexpr std::size_t                        BufferSizeBytes = 64 + 128;
        cetl::byte                                   buffer[BufferSizeBytes];
        cetl::pmr::UnsynchronizedArrayMemoryResource resource{buffer, BufferSizeBytes};

        void* r = resource.allocate(64, 128);

        std::cout << "Over-aligned address at: " << r << std::endl;

        resource.deallocate(r, 64, 128);
        //![example_1]
    }

    return 0;
}
