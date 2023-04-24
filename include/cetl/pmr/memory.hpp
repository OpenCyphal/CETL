/// @file
/// Extensions and utilities for types found in the standard `memory` header to better integrate with pmr types.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_MEMORY_RESOURCE_PTR_H_INCLUDED
#define CETL_MEMORY_RESOURCE_PTR_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include <memory>
#include <utility>

namespace cetl
{
namespace pmr
{

class MemoryResourceDeleter final
{
private:
    struct MemoryResourceData
    {
        memory_resource* mem_resource;
        std::size_t      mem_size;
        std::size_t      mem_align;
    };

public:
    MemoryResourceDeleter(memory_resource* mem_resource, std::size_t mem_size, std::size_t mem_align) noexcept
        : data_{mem_resource, mem_size, mem_align}
    {
    }

    MemoryResourceDeleter(memory_resource* mem_resource, std::size_t mem_size) noexcept
        : MemoryResourceDeleter(mem_resource, mem_size, alignof(std::max_align_t))
    {
    }

    MemoryResourceDeleter(MemoryResourceDeleter&& rhs) noexcept
        : data_{nullptr, 0, 0}
    {
        swap(std::move(rhs));
    }

    MemoryResourceDeleter& operator=(MemoryResourceDeleter&& rhs) noexcept
    {
        swap(std::move(rhs));
        return *this;
    }

    ~MemoryResourceDeleter() = default;

    MemoryResourceDeleter() noexcept                               = delete;
    MemoryResourceDeleter(const MemoryResourceDeleter&)            = delete;
    MemoryResourceDeleter& operator=(const MemoryResourceDeleter&) = delete;

    void operator()(void* p) noexcept
    {
        CETL_DEBUG_ASSERT(nullptr == p || nullptr != data_.mem_resource, "mem_resource was null in deleter?");
        if (nullptr != data_.mem_resource)
        {
            data_.mem_resource->deallocate(p, data_.mem_size, data_.mem_align);
        }
    }

    std::size_t size() const noexcept
    {
        return data_.mem_size;
    }

    std::size_t alignment() const noexcept
    {
        return data_.mem_align;
    }

    memory_resource* resource() const noexcept
    {
        return data_.mem_resource;
    }

private:
    void swap(MemoryResourceDeleter&& rhs) noexcept
    {
        CETL_DEBUG_ASSERT(nullptr != rhs.data_.mem_resource, "Moving from resource deleter with null memory resource.");
        data_.mem_resource = nullptr;
        std::swap(data_, rhs.data_);
    }

    MemoryResourceData data_;
};

template <typename PolymorphicAllocatorType>
class PolymorphicAllocatorDeleter final
{
public:
    using allocator  = PolymorphicAllocatorType;
    using value_type = typename PolymorphicAllocatorType::value_type;
    using unique_ptr =
        std::unique_ptr<value_type,
                        cetl::pmr::PolymorphicAllocatorDeleter<cetl::pmr::polymorphic_allocator<value_type>>>;

    PolymorphicAllocatorDeleter() noexcept = delete;

    PolymorphicAllocatorDeleter(const PolymorphicAllocatorType& alloc, std::size_t mem_size) noexcept(
        std::is_nothrow_copy_constructible_v<PolymorphicAllocatorType>)
        : alloc_{alloc}
        , obj_count_{mem_size}
    {
    }

    ~PolymorphicAllocatorDeleter() = default;

    PolymorphicAllocatorDeleter(PolymorphicAllocatorDeleter&& rhs) noexcept(
        std::is_nothrow_move_constructible_v<PolymorphicAllocatorType>)
        : alloc_{rhs.alloc_}
        , obj_count_{rhs.obj_count_}
    {
    }

    PolymorphicAllocatorDeleter& operator=(PolymorphicAllocatorDeleter&& rhs) noexcept(
        std::is_nothrow_move_assignable_v<PolymorphicAllocatorType>)
    {
        alloc_     = rhs.alloc_;
        obj_count_ = rhs.obj_count_;
        return *this;
    }

    PolymorphicAllocatorDeleter(const PolymorphicAllocatorDeleter& rhs) noexcept(
        std::is_nothrow_copy_constructible_v<PolymorphicAllocatorType>)
        : alloc_{rhs.alloc_}
        , obj_count_{rhs.obj_count_}
    {
    }
    PolymorphicAllocatorDeleter& operator=(const PolymorphicAllocatorDeleter& rhs) noexcept(
        std::is_nothrow_copy_assignable_v<PolymorphicAllocatorType>)
    {
        alloc_     = rhs.alloc_;
        obj_count_ = rhs.obj_count_;
        return *this;
    }

    void operator()(value_type* p) noexcept
    {
        if (nullptr != p)
        {
            p->~value_type();
        }
        alloc_.deallocate(p, obj_count_);
    }

    // TODO: make_unique_array
    template <typename... Args>
    static unique_ptr make_unique(PolymorphicAllocatorType& alloc, Args&&... args)
    {
        unique_ptr p{alloc.allocate(1), PolymorphicAllocatorDeleter{alloc, 1}};
        if (nullptr != p)
        {
            alloc.construct(p.get(), std::forward<Args>(args)...);
        }
        return std::move(p);
    }

private:
    PolymorphicAllocatorType alloc_;
    std::size_t              obj_count_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_MEMORY_RESOURCE_PTR_H_INCLUDED
