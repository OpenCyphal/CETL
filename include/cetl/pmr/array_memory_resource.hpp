/// @file
/// Defines memory_resource types specific to CETL but adhering to the C++17 std::pmr::memory_resource interface
/// contract that are backed by simple contiguous blocks of memory.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED
#define CETL_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include <algorithm>
#include <cstddef>
#include <limits>
#include <memory>  // for std::align

namespace cetl
{
namespace pmr
{

/// Memory resource that supports a single allocation request within a single, contiguous block of memory.
/// Without any memory barriers or other synchronization primitives this is one of the simplest possible implementations
/// of a std::pmr::memory_resource with only one feature of supporting an, optional, upstream allocator.
///
/// This is a delegate class that isn't useful on its own but is used to implement other memory resource types. There
/// are two ways to use this class:
///
/// @par PolyFill
/// If you are using cetl/pf17/cetlpf.hpp then include this header just before that one and use
/// cetl::pmr::UnsynchronizedArrayMemoryResource as defined in that header. When compiling for C++17 this type will
/// implement std::pmr::memory_resource and when compiling for C++14 this type will implement
/// cetl::pf17::pmr::memory_resource.
///
/// @par Delegate Class
/// Manually build an implementation of memory_resource that uses this class as a delegate. For example:
///
/// @snippet{trimleft} example_05_array_memory_resource_alignment.cpp example_delegate
///
/// Also note that this class supports over-alignment. For example:
///
/// @snippet{trimleft} example_05_array_memory_resource_alignment.cpp example_0
///
/// In that example the buffer is too small to support the requested alignment. In that case you can do this:
///
/// @snippet{trimleft} example_05_array_memory_resource_alignment.cpp example_1
///
/// @tparam MemoryResourceType The type of the upstream memory resource to use.
template <typename UpstreamMemoryResourceType>
class UnsynchronizedArrayMemoryResourceDelegate final
{
private:
    /// Saturating add of two max size values clamped to the maximum value for the pointer difference type
    /// for the current architecture.
    static constexpr std::size_t calculate_max_size_bytes(std::size_t max_size_left, std::size_t max_size_right)
    {
        static_assert(std::numeric_limits<std::ptrdiff_t>::max() >= 0,
                      "We don't know what it means to have a negative maximum pointer diff? Serious, what gives?");

        constexpr const std::size_t max_diff_as_size =
            static_cast<std::size_t>(std::numeric_limits<std::ptrdiff_t>::max());
        const std::size_t left_clamped  = std::min(max_size_left, max_diff_as_size);
        const std::size_t right_clamped = std::min(max_size_right, max_diff_as_size);
        if (right_clamped > (max_diff_as_size - left_clamped))
        {
            return max_diff_as_size;
        }
        else
        {
            return left_clamped + right_clamped;
        }
    }

public:
    /// Designated constructor that initializes the object with a fixed buffer and an optional upstream memory resource.
    /// @param buffer                   The buffer that is used to satisfy allocation requests.
    /// @param buffer_size_bytes        The size, in bytes, of the buffer.
    /// @param upstream                 An optional upstream memory resource to use if the buffer is already in use.
    /// @param upstream_max_size_bytes  The maximum size of the upstream buffer.
    UnsynchronizedArrayMemoryResourceDelegate(void*                       buffer,
                                              std::size_t                 buffer_size_bytes,
                                              UpstreamMemoryResourceType* upstream,
                                              std::size_t                 upstream_max_size_bytes) noexcept
        : upstream_{upstream}
        , buffer_{buffer}
        , buffer_size_bytes_{buffer_size_bytes}
        , max_size_bytes_{calculate_max_size_bytes(buffer_size_bytes, upstream_max_size_bytes)}
        , in_use_{nullptr}
    {
        CETL_DEBUG_ASSERT(nullptr != upstream,
                          "Upstream memory resource cannot be null. Use std::pmr::null_memory_resource or "
                          "cetl::pmr::null_memory_resource if you don't want an upstream memory resource.");
    }

    ~UnsynchronizedArrayMemoryResourceDelegate()                                                           = default;
    UnsynchronizedArrayMemoryResourceDelegate(const UnsynchronizedArrayMemoryResourceDelegate&)            = delete;
    UnsynchronizedArrayMemoryResourceDelegate& operator=(const UnsynchronizedArrayMemoryResourceDelegate&) = delete;
    UnsynchronizedArrayMemoryResourceDelegate(UnsynchronizedArrayMemoryResourceDelegate&&)                 = delete;
    UnsynchronizedArrayMemoryResourceDelegate& operator=(UnsynchronizedArrayMemoryResourceDelegate&&)      = delete;

    //  +--[public methods]---------------------------------------------------+
    constexpr UpstreamMemoryResourceType* upstream_resource() const
    {
        return upstream_;
    }

    constexpr void* allocate(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        void* result = nullptr;
        if (!in_use_)
        {
            result = allocate_internal_buffer(size_bytes, alignment);
        }
        if (nullptr != result)
        {
            in_use_ = result;
        }
        else if (upstream_)
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

    constexpr void* reallocate(void* p, std::size_t old_size_bytes, std::size_t new_size_bytes, std::size_t new_align)
    {
        (void) old_size_bytes;
        CETL_DEBUG_ASSERT(nullptr == p || in_use_ == p || nullptr != upstream_,
                          "Unknown pointer passed into reallocate.");
        if (p == in_use_)
        {
            return allocate_internal_buffer(new_size_bytes, new_align);
        }
        else
        {
            return nullptr;
        }
        return nullptr;
    }

    constexpr void deallocate(void* p, std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        CETL_DEBUG_ASSERT(nullptr == p || in_use_ == p || nullptr != upstream_,
                          "Unknown pointer passed into deallocate.");
        if (p == in_use_)
        {
            in_use_ = nullptr;
        }
        else if (nullptr != upstream_)
        {
            upstream_->deallocate(p, size_bytes, alignment);
        }
    }

    std::size_t max_size() const noexcept
    {
        return max_size_bytes_;
    }

private:
    constexpr void* allocate_internal_buffer(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        void* result = nullptr;
        if (nullptr != buffer_ && size_bytes <= buffer_size_bytes_)
        {
            void*       storage_ptr  = buffer_;
            std::size_t storage_size = buffer_size_bytes_;
            result                   = std::align(alignment, size_bytes, storage_ptr, storage_size);
        }
        return result;
    }

    UpstreamMemoryResourceType* upstream_;
    void*                       buffer_;
    const std::size_t           buffer_size_bytes_;
    const std::size_t           max_size_bytes_;
    void*                       in_use_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED
