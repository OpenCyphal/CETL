/// @file
/// Demonstration of memory alignment when using the cetl::pmr::UnsynchronizedArrayMemoryResource in
/// cetl/pmr/array_memory_resource.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include <iostream>

#include <gtest/gtest.h>

//![example_delegate]
#include "cetl/pf17/memory_resource.hpp"
#include "cetl/pmr/array_memory_resource.hpp"
#include "cetl/pf17/byte.hpp"

namespace cetl
{

namespace pmr
{

/// Implementation of cetl::pmr::memory_resource that implements cetl::pf17::pmr::memory_resource using
/// cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate and cetl::pf17::pmr::memory_resource as the upstream memory
/// resource.
class UnsynchronizedArrayMemoryResource final : public cetl::pf17::pmr::memory_resource
{
public:
    using MemoryResourceType = cetl::pf17::pmr::memory_resource;

    /// See cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate for details..
    UnsynchronizedArrayMemoryResource(void*               buffer,
                                      std::size_t         buffer_size_bytes,
                                      MemoryResourceType* upstream = cetl::pf17::pmr::null_memory_resource())
        : delegate_{buffer,
                    buffer_size_bytes,
                    upstream,
                    cetl::pf17::pmr::deviant::memory_resource_traits<MemoryResourceType>::max_size(*upstream)}
    {
    }

    ~UnsynchronizedArrayMemoryResource()                                                   = default;
    UnsynchronizedArrayMemoryResource(const UnsynchronizedArrayMemoryResource&)            = delete;
    UnsynchronizedArrayMemoryResource& operator=(const UnsynchronizedArrayMemoryResource&) = delete;
    UnsynchronizedArrayMemoryResource(UnsynchronizedArrayMemoryResource&&)                 = delete;
    UnsynchronizedArrayMemoryResource& operator=(UnsynchronizedArrayMemoryResource&&)      = delete;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        return delegate_.allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
    {
        delegate_.deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const MemoryResourceType& other) const noexcept override
    {
        return (this == &other);
    }

    std::size_t do_max_size() const noexcept override
    {
        return delegate_.max_size();
    }

    void* do_reallocate(void* p, std::size_t old_size_bytes, std::size_t new_size_bytes, std::size_t alignment) override
    {
        return delegate_.reallocate(p, old_size_bytes, new_size_bytes, alignment);
    }

    cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate<MemoryResourceType> delegate_;
};
}  // namespace pmr
}  // namespace cetl
//![example_delegate]

TEST(example_05_array_memory_resource_alignment, example_0)
{
    static_assert(alignof(std::max_align_t) < 128, "Wow, what hardware are you running on?");
    //![example_0]
    constexpr std::size_t                        BufferSizeBytes = 64;
    cetl::pf17::byte                             buffer[BufferSizeBytes];
    cetl::pmr::UnsynchronizedArrayMemoryResource resource{buffer, BufferSizeBytes};

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
    constexpr std::size_t                        BufferSizeBytes = 64 + 128;
    cetl::pf17::byte                             buffer[BufferSizeBytes];
    cetl::pmr::UnsynchronizedArrayMemoryResource resource{buffer, BufferSizeBytes};

    void* r = resource.allocate(64, 128);

    std::cout << "Over-aligned address at: " << r << std::endl;

    resource.deallocate(r, 64, 128);
    //![example_1]
}
