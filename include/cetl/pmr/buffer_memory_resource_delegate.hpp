/// @file
/// Defines memory_resource types specific to CETL but adhering to the C++17 std::pmr::memory_resource interface
/// contract that are backed by simple contiguous blocks of memory.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words CDE_ubmrd

#ifndef CETL_PMR_BUFFER_MEMORY_RESOURCE_DELEGATE_H_INCLUDED
#define CETL_PMR_BUFFER_MEMORY_RESOURCE_DELEGATE_H_INCLUDED

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
/// @par Delegate Class
/// You will need an implementation of memory_resource that uses this class as a delegate since this class does not
/// directly rely on any C++17 nor CETL pf17 types. This allows you to use it with std::pmr::memory_resource or
/// cetl::pf17::pmr::memory_resource.
///
/// @par Over-Alignment
/// This class supports over-alignment but you will need to over-provision the backing array to support this feature.
/// For example, if the buffer is too small to support the requested alignment then the allocation will fail as this
/// example, using the delegate via cetl::pf17::pmr::UnsynchronizedArrayMemoryResource, demonstrates:
///
/// @snippet{trimleft} example_05_array_memory_resource.cpp example_0
/// By over-provisioning the buffer the same alignment will succeed:
/// @snippet{trimleft} example_05_array_memory_resource.cpp example_1
/// (@ref example_05_array_memory_resource "See full example here...")
///
/// @tparam MemoryResourceType The type of the upstream memory resource to use.
template <typename UpstreamMemoryResourceType>
class UnsynchronizedBufferMemoryResourceDelegate final
{
private:
    // adapted from https://en.cppreference.com/w/cpp/experimental/is_detected
    // implements a C++14-compatible detection idiom. This code may be copy-and-pasted in a few places within CETL types
    // to avoid creating dependency coupling.

    template <typename...>
    using _void_t = void;

    template <class AlwaysVoid, template <class...> class Op, class... Args>
    struct detector
    {
        using value_t = std::false_type;
    };

    template <template <class...> class Op, class... Args>
    struct detector<_void_t<Op<Args...>>, Op, Args...>
    {
        using value_t = std::true_type;
    };

    template <template <class...> class Op, class... Args>
    using is_detected = typename detector<void, Op, Args...>::value_t;

    template <typename U>
    using reallocate_operation =
        decltype(std::declval<U>().reallocate(nullptr, std::size_t(), std::size_t(), std::size_t()));

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

    template <typename UpstreamResourceType>
    constexpr typename std::enable_if_t<!is_detected<reallocate_operation, UpstreamResourceType>::value>*
    reallocate_internal(UpstreamResourceType& upstream,
                        void*                 data,
                        std::size_t           old_object_count,
                        std::size_t           new_object_count,
                        std::size_t           new_align)
    {
        (void) old_object_count;
        (void) upstream;
        void* result = nullptr;
        if (data == in_use_)
        {
            result  = allocate_internal_buffer(new_object_count, new_align);
            in_use_ = result;
        }
        return result;
    }

    template <typename UpstreamResourceType>
    constexpr typename std::enable_if_t<is_detected<reallocate_operation, UpstreamResourceType>::value>*
    reallocate_internal(UpstreamResourceType& upstream,
                        void*                 data,
                        std::size_t           old_object_count,
                        std::size_t           new_object_count,
                        std::size_t           new_align)
    {
        (void) old_object_count;
        void* result = nullptr;
        if (data == in_use_)
        {
            result  = allocate_internal_buffer(new_object_count, new_align);
            in_use_ = result;
        }
        else
        {
            result = upstream.reallocate(data, old_object_count, new_object_count, new_align);
        }
        return result;
    }

public:
    /// Designated constructor that initializes the object with a fixed buffer and an optional upstream memory resource.
    /// @param buffer                   The buffer that is used to satisfy allocation requests.
    /// @param buffer_size_bytes        The size, in bytes, of the buffer.
    /// @param upstream                 An optional upstream memory resource to use if the buffer is already in use.
    /// @param upstream_max_size_bytes  The maximum size of the upstream buffer.
    UnsynchronizedBufferMemoryResourceDelegate(void*                       buffer,
                                               std::size_t                 buffer_size_bytes,
                                               UpstreamMemoryResourceType* upstream,
                                               std::size_t                 upstream_max_size_bytes) noexcept
        : upstream_{upstream}
        , buffer_{buffer}
        , buffer_size_bytes_{buffer_size_bytes}
        , max_size_bytes_{calculate_max_size_bytes(buffer_size_bytes, upstream_max_size_bytes)}
        , upstream_max_size_bytes_{upstream_max_size_bytes}
        , in_use_{nullptr}
    {
        CETL_DEBUG_ASSERT(nullptr != upstream,
                          "CDE_ubmrd_001: Upstream memory resource cannot be null. Use std::pmr::null_memory_resource "
                          "or cetl::pmr::null_memory_resource if you don't want an upstream memory resource.");
        CETL_DEBUG_ASSERT(nullptr != buffer || buffer_size_bytes == 0,
                          "CDE_ubmrd_002: Nullptr buffer provided with a buffer size > 0.");
    }

    ~UnsynchronizedBufferMemoryResourceDelegate()                                                            = default;
    UnsynchronizedBufferMemoryResourceDelegate(const UnsynchronizedBufferMemoryResourceDelegate&)            = delete;
    UnsynchronizedBufferMemoryResourceDelegate& operator=(const UnsynchronizedBufferMemoryResourceDelegate&) = delete;
    UnsynchronizedBufferMemoryResourceDelegate(UnsynchronizedBufferMemoryResourceDelegate&&) noexcept        = default;
    UnsynchronizedBufferMemoryResourceDelegate& operator=(UnsynchronizedBufferMemoryResourceDelegate&&)      = delete;

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
            result  = allocate_internal_buffer(size_bytes, alignment);
            in_use_ = result;
        }
        if (nullptr == result && size_bytes <= upstream_max_size_bytes_)
        {
            result = upstream_->allocate(size_bytes, alignment);
        }

#if defined(__cpp_exceptions)
        if (nullptr == result)
        {
            throw std::bad_alloc();
        }
#endif
        return result;
    }

    constexpr void* reallocate(void*       p,
                               std::size_t old_size_bytes,
                               std::size_t new_size_bytes,
                               std::size_t new_align = alignof(std::max_align_t))
    {
        return reallocate_internal(*upstream_, p, old_size_bytes, new_size_bytes, new_align);
    }

    constexpr void deallocate(void* p, std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
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

    std::size_t max_size() const noexcept
    {
        return max_size_bytes_;
    }

    /// Direct access to the internal data. It is generally not safe to use this memory directly.
    void* data() noexcept
    {
        return buffer_;
    }

    /// Direct access to the internal data. It is generally not safe to use this memory directly.
    const void* data() const noexcept
    {
        return buffer_;
    }

    /// The size in bytes of the internal buffer.
    std::size_t size() const noexcept
    {
        return buffer_size_bytes_;
    }

private:
    constexpr void* allocate_internal_buffer(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        void* result = nullptr;
        if (size_bytes <= buffer_size_bytes_)
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
    const std::size_t           upstream_max_size_bytes_;
    void*                       in_use_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_BUFFER_MEMORY_RESOURCE_DELEGATE_H_INCLUDED
