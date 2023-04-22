/// @file
/// Defines a safe smart pointer idiom when obtaining memory directly from a memory_resource.
///
/// TODO: examples
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

#include <utility>

namespace cetl
{
namespace pmr
{

class MemoryResourcePointer final
{
private:
    struct Data
    {
        void*            mem;
        std::size_t      mem_size;
        std::size_t      mem_align;
        memory_resource* mem_resource;
    };

    // only MemoryResourceManager objects are allowed to create pointers directly.
    friend class MemoryResourceManager;

    MemoryResourcePointer(void* mem, std::size_t mem_size, std::size_t mem_align, memory_resource* mem_resource)
        : data_{mem, mem_size, mem_align, mem_resource}
    {
        CETL_DEBUG_ASSERT(nullptr != data_.mem || nullptr != data_.mem_resource,
                          "null memory resource passed into resource pointer with non-null memory. This is a leak!");
    }

public:
    ~MemoryResourcePointer()
    {
        reset();
    }

    MemoryResourcePointer(const MemoryResourcePointer&)            = delete;
    MemoryResourcePointer& operator=(const MemoryResourcePointer&) = delete;
    MemoryResourcePointer& operator=(MemoryResourcePointer&&)      = delete;

    MemoryResourcePointer(MemoryResourcePointer&& rhs) noexcept
        : data_{nullptr, 0, 0, nullptr}
    {
        data_.mem_resource = rhs.data_.mem_resource;
        std::swap(data_, rhs.data_);
    }

    /// Don't say we didn't warn you.
    /// @return The previously held resource. The caller is now responsible for deallocating this memory using a
    ///         a resource equal to resource().
    void* release() noexcept
    {
        Data old_data = std::exchange(data_, Data{nullptr, 0, 0, data_.mem_resource});
        return old_data.mem;
    }

    memory_resource* resource() const noexcept
    {
        return data_.mem_resource;
    }

    void swap(MemoryResourcePointer& rhs) noexcept
    {
        std::swap(data_, rhs.data_);
    }

    void* get() const noexcept
    {
        return data_.mem;
    }

    explicit operator bool() const noexcept
    {
        return (nullptr != data_.mem);
    }

    void* operator*() const noexcept
    {
        return get();
    }

    void reset()
    {
        Data old_data = std::exchange(data_, Data{nullptr, 0, 0, data_.mem_resource});
        old_data.mem_resource->deallocate(old_data.mem, old_data.mem_size, old_data.mem_align);
    }

private:
    Data data_;
};

bool operator==(const MemoryResourcePointer& lhs, const MemoryResourcePointer& rhs)
{
    // of course, this will never be true but it allows us to meet the container contract.
    return (lhs.get() == rhs.get());
}

bool operator!=(const MemoryResourcePointer& lhs, const MemoryResourcePointer& rhs)
{
    return !(lhs == rhs);
}

bool operator>(const MemoryResourcePointer& lhs, const MemoryResourcePointer& rhs)
{
    return (lhs.get() > rhs.get());
}

bool operator>=(const MemoryResourcePointer& lhs, const MemoryResourcePointer& rhs)
{
    return (lhs.get() >= rhs.get());
}

bool operator<(const MemoryResourcePointer& lhs, const MemoryResourcePointer& rhs)
{
    return (lhs.get() < rhs.get());
}

bool operator<=(const MemoryResourcePointer& lhs, const MemoryResourcePointer& rhs)
{
    return (lhs.get() <= rhs.get());
}

/// Decorator for a std::memory_resource or cetl::pf17::pmr::memory_resource that vends a
/// cetl::pmr::MemoryResourcePointer from a raii_allocate method.
class MemoryResourceManager : public memory_resource
{
public:
    explicit MemoryResourceManager(memory_resource* resource)
        : resource_{resource}
    {
        CETL_DEBUG_ASSERT(nullptr != resource, "null resource passed into MemoryResourceManager.");
    }

    MemoryResourcePointer raii_allocate(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        return MemoryResourcePointer{resource_->allocate(size_bytes, alignment), size_bytes, alignment, resource_};
    }

    memory_resource* resource() const noexcept
    {
        return resource_;
    }

protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        return resource_->allocate(size_bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        return resource_->deallocate(p, size_bytes, alignment);
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return resource_->is_equal(rhs);
    }

private:
    memory_resource* resource_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_MEMORY_RESOURCE_PTR_H_INCLUDED
