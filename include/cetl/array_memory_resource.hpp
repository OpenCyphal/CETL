/// @file
/// Defines memory_resource types specific to CETL but adhering to the C++17 std::pmr::memory_resource interface
/// contract that are backed by simple contiguous blocks of memory.
///
/// If including this file you will need to either first include cetl/pf17/cetlpf.hpp or provide the memory_resource
/// definition you want this class to use.
///
/// TODO: examples
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_ARRAY_MEMORY_RESOURCE_H_INCLUDED
#define CETL_ARRAY_MEMORY_RESOURCE_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

namespace cetl
{
namespace pmr
{

/// std::pmr::memory_resource that supports a single allocation request within a single, contiguous block of memory.
/// Without any memory barriers or other synchronization primitives this is one of the simplest possible implementations
/// of the std::prm::memory_resource interface with the one feature of supporting an, optional, upstream allocator
/// that this class defers to if its array is already in use.
class UnsynchronizedArrayMemoryResource : public memory_resource
{
public:
    UnsynchronizedArrayMemoryResource(void* buffer, std::size_t buffer_size_bytes, memory_resource* upstream = nullptr)
        : upstream_{upstream}
        , buffer_{buffer}
        , buffer_size_bytes_{buffer_size_bytes}
        , in_use_{nullptr}
    {
    }

    ~UnsynchronizedArrayMemoryResource()                                                   = default;
    UnsynchronizedArrayMemoryResource(const UnsynchronizedArrayMemoryResource&)            = delete;
    UnsynchronizedArrayMemoryResource& operator=(const UnsynchronizedArrayMemoryResource&) = delete;
    UnsynchronizedArrayMemoryResource(UnsynchronizedArrayMemoryResource&&)                 = delete;
    UnsynchronizedArrayMemoryResource& operator=(UnsynchronizedArrayMemoryResource&&)      = delete;

    //  +--[public methods]---------------------------------------------------+
    memory_resource* upstream_resource() const
    {
        return upstream_;
    }

protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        void* result = nullptr;
        if (!in_use_ && nullptr != buffer_ && size_bytes <= buffer_size_bytes_)
        {
            void*       storage_ptr  = buffer_;
            std::size_t storage_size = buffer_size_bytes_;
            result                   = std::align(alignment, size_bytes, storage_ptr, storage_size);
            if (nullptr != result)
            {
                in_use_ = result;
            }
            // else we've asked for a memory alignment we can't support within the
            // static array for the given length.
        }
        if (nullptr == result && upstream_)
        {
            result = upstream_->allocate(size_bytes, alignment);
        }

#if __cpp_exceptions
        if (nullptr == result)
        {
            throw std::bad_alloc();
        }
#endif
        return result;
    }

    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        if (p == in_use_)
        {
            in_use_ = nullptr;
        }
        else
        {
            upstream_->deallocate(p, size_bytes, alignment);
        }
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return (this == &rhs);
    }

private:
    memory_resource*  upstream_;
    void*             buffer_;
    const std::size_t buffer_size_bytes_;
    void*             in_use_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_ARRAY_MEMORY_RESOURCE_H_INCLUDED
