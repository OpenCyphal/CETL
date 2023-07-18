/// @file
/// Defines memory_resource types for that are backed by simple contiguous blocks of memory and use cetl::pf17 types.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PF17_PMR_BUFFER_MEMORY_RESOURCE_H_INCLUDED
#define CETL_PF17_PMR_BUFFER_MEMORY_RESOURCE_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include "cetl/pf17/memory_resource.hpp"
#include "cetl/pf17/byte.hpp"
#include "cetl/pmr/buffer_memory_resource_delegate.hpp"

namespace cetl
{
namespace pf17
{
namespace pmr
{

/// Implementation of cetl::pf17::pmr::memory_resource that uses
/// cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate as the internal implementation.
///
/// @par Examples
/// Using this class with STL containers:
/// @snippet{trimleft} example_04_buffer_memory_resource.cpp example_a
/// Creating a small-buffer optimization using this class:
/// @snippet{trimleft} example_04_buffer_memory_resource.cpp example_b
/// Using two std::pmr::UnsynchronizedBufferMemoryResourceDelegate instances with std::vector:
/// @snippet{trimleft} example_04_buffer_memory_resource.cpp example_c
/// (@ref example_04_buffer_memory_resource "See full example here...")
///
class UnsynchronizedBufferMemoryResource final : public cetl::pf17::pmr::memory_resource
{
public:
    /// See cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate for details.
    /// @param buffer The buffer to provide to cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate.
    /// @param buffer_size_bytes The size of the buffer to provide to
    /// cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate
    /// @param upstream The upstream memory resource to provide to
    /// cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate.
    ///                 This cannot be null.
    /// @param upstream_max_size_bytes The maximum size of the upstream memory resource.
    UnsynchronizedBufferMemoryResource(void*                             buffer,
                                       std::size_t                       buffer_size_bytes,
                                       cetl::pf17::pmr::memory_resource* upstream,
                                       std::size_t                       upstream_max_size_bytes) noexcept
        : delegate_{buffer, buffer_size_bytes, upstream, upstream_max_size_bytes}
    {
    }

    /// Constructor version that uses cetl::pf17::pmr::null_memory_resource as the upstream resource.
    /// @param buffer The buffer to provide to cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate.
    /// @param buffer_size_bytes The size of the buffer to provide to
    /// cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate
    UnsynchronizedBufferMemoryResource(void* buffer, std::size_t buffer_size_bytes) noexcept
        : delegate_{buffer, buffer_size_bytes, cetl::pf17::pmr::null_memory_resource(), 0}
    {
    }

    ~UnsynchronizedBufferMemoryResource() override                                           = default;
    UnsynchronizedBufferMemoryResource(const UnsynchronizedBufferMemoryResource&)            = delete;
    UnsynchronizedBufferMemoryResource& operator=(const UnsynchronizedBufferMemoryResource&) = delete;
    UnsynchronizedBufferMemoryResource(UnsynchronizedBufferMemoryResource&&) noexcept        = default;
    UnsynchronizedBufferMemoryResource& operator=(UnsynchronizedBufferMemoryResource&&)      = delete;

    // +-----------------------------------------------------------------------+
    // | cetl::UnsynchronizedBufferMemoryResourceDelegate
    // +-----------------------------------------------------------------------+
    /// @copydoc cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate::data()
    void* data() noexcept
    {
        return delegate_.data();
    }

    /// @copydoc cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate::data()
    const void* data() const noexcept
    {
        return delegate_.data();
    }

    /// @copydoc cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate::size()
    std::size_t size() const noexcept
    {
        return delegate_.size();
    }

private:
    // +-----------------------------------------------------------------------+
    // | cetl::pf17::pmr::memory_resource
    // +-----------------------------------------------------------------------+

    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        return delegate_.allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
    {
        delegate_.deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const cetl::pf17::pmr::memory_resource& other) const noexcept override
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

    // +-----------------------------------------------------------------------+
    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource> delegate_;
};

}  // namespace pmr
}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_PMR_BUFFER_MEMORY_RESOURCE_H_INCLUDED
