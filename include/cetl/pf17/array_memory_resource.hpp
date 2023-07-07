/// @file
/// Defines memory_resource types for that are backed by simple contiguous blocks of memory and use cetl::pf17 types.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PF17_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED
#define CETL_PF17_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include "cetl/pf17/memory_resource.hpp"
#include "cetl/pf17/byte.hpp"
#include "cetl/pmr/buffer_memory_resource.hpp"

#include <array>

namespace cetl
{
namespace pf17
{
namespace pmr
{

/// Implementation of cetl::pf17::pmr::memory_resource that uses
/// cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate as the internal implementation.
///
/// @par Over-Alignment
/// This class supports over-alignment but you will need to over-provision the backing array to support this feature.
/// For example, if the buffer is too small to support the requested alignment then the allocation will fail as this
/// example demonstrates:
/// @snippet{trimleft} example_05_array_memory_resource.cpp example_0
/// By over-provisioning the buffer the same alignment will succeed:
/// @snippet{trimleft} example_05_array_memory_resource.cpp example_1
/// (@ref example_05_array_memory_resource "See full example here...")
///
template <std::size_t array_size>
class UnsynchronizedArrayMemoryResource final : public cetl::pf17::pmr::memory_resource
{
public:
    UnsynchronizedArrayMemoryResource(cetl::pf17::pmr::memory_resource* upstream,
                                      std::size_t                       upstream_max_size_bytes) noexcept
        : array_{}
        , delegate_{array_.data(), sizeof(cetl::pf17::byte) * array_size, upstream, upstream_max_size_bytes}
    {
    }

    UnsynchronizedArrayMemoryResource() noexcept
        : array_{}
        , delegate_{array_.data(), sizeof(cetl::pf17::byte) * array_size, cetl::pf17::pmr::null_memory_resource(), 0}
    {
    }

    ~UnsynchronizedArrayMemoryResource() override                                          = default;
    UnsynchronizedArrayMemoryResource(const UnsynchronizedArrayMemoryResource&)            = delete;
    UnsynchronizedArrayMemoryResource& operator=(const UnsynchronizedArrayMemoryResource&) = delete;
    UnsynchronizedArrayMemoryResource(UnsynchronizedArrayMemoryResource&&)                 = delete;
    UnsynchronizedArrayMemoryResource& operator=(UnsynchronizedArrayMemoryResource&&)      = delete;

    cetl::pf17::byte* data() noexcept
    {
        return array_.data();
    }

    const cetl::pf17::byte* data() const noexcept
    {
        return array_.data();
    }

    std::size_t size() const noexcept
    {
        return array_.size();
    }

private:
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

    std::array<cetl::pf17::byte, array_size>                                                array_;
    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource> delegate_;
};
}  // namespace pmr
}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED
