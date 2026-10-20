/// @file
/// Extensions and utilities for types found in the standard `memory` header to better integrate with pmr types.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PMR_MEMORY_H_INCLUDED
#define CETL_PMR_MEMORY_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include <memory>
#include <utility>
#include <cstddef>  // for std::max_align_t

namespace cetl
{
namespace pmr
{

/// RAII helper for MemoryResourceType::allocate() and MemoryResourceType::deallocate(). This type is designed to work
/// with std::unique_ptr.
///
/// @tparam MemoryResourceType The memory resource type.
///
/// Example usage:
///
/// @snippet{trimleft} example_06_memory_resource_deleter.cpp example_usage
/// (@ref example_06_memory_resource_deleter "See full example here...")
///
template <typename MemoryResourceType>
class memory_resource_deleter final
{
private:
    struct memory_resource_data
    {
        MemoryResourceType* mem_resource;
        std::size_t         mem_size;
        std::size_t         mem_align;
    };

public:
    /// Designated constructor.
    /// @param mem_resource The memory resource to use for deallocation.
    /// @param mem_size     The size of the memory to deallocate.
    /// @param mem_align    The alignment of the memory to deallocate.
    memory_resource_deleter(MemoryResourceType* mem_resource,
                            std::size_t         mem_size,
                            std::size_t         mem_align = alignof(std::max_align_t)) noexcept
        : data_{mem_resource, mem_size, mem_align}
    {
    }

    /// To support unique_ptr move semantics.
    /// @param rhs The deleter to move from.
    memory_resource_deleter(memory_resource_deleter&& rhs) noexcept
        : data_{nullptr, 0, 0}
    {
        swap(std::move(rhs));
    }

    /// To support unique_ptr move semantics.
    /// @param rhs The deleter to move from.
    /// @return Reference to this.
    memory_resource_deleter& operator=(memory_resource_deleter&& rhs) noexcept
    {
        swap(std::move(rhs));
        return *this;
    }

    ~memory_resource_deleter() = default;

    memory_resource_deleter() noexcept                                 = delete;
    memory_resource_deleter(const memory_resource_deleter&)            = delete;
    memory_resource_deleter& operator=(const memory_resource_deleter&) = delete;

    /// Functor called by smart-pointer to deallocate memory.
    /// @param p The memory to deallocate.
    void operator()(void* p) noexcept
    {
        CETL_DEBUG_ASSERT(nullptr == p || nullptr != data_.mem_resource, "mem_resource was null in deleter?");
        if (nullptr != data_.mem_resource)
        {
            data_.mem_resource->deallocate(p, data_.mem_size, data_.mem_align);
        }
    }

    /// Size of the memory this deleter will or did deallocate.
    /// @return Size in bytes.
    std::size_t size() const noexcept
    {
        return data_.mem_size;
    }

    /// Alignment of the memory this deleter will or did deallocate.
    /// @return Alignment.
    std::size_t alignment() const noexcept
    {
        return data_.mem_align;
    }

    /// The memory resource this deleter will or did use to deallocate memory.
    /// @return The memory resource or nullptr if this deleter was moved from and is now invalid.
    MemoryResourceType* resource() const noexcept
    {
        return data_.mem_resource;
    }

private:
    void swap(memory_resource_deleter&& rhs) noexcept
    {
        CETL_DEBUG_ASSERT(nullptr != rhs.data_.mem_resource, "Moving from resource deleter with null memory resource.");
        data_.mem_resource = nullptr;
        std::swap(data_, rhs.data_);
    }

    memory_resource_data data_;
};

/// RAII helper for cetl::pf17::pmr::polymorphic_allocator and std::pmr::polymorphic_allocator. This type is designed
/// to work with std::unique_ptr but is still cumbersome to use. Use with cetl::pmr::factory for the best and safest
/// experience. Remember, be safe, use the cetl::pmr::factory.
///
/// @note
/// See cetl::pmr::factory for an example of how to use this type.
///
/// @tparam PolymorphicDeallocator    The type of the polymorphic allocator to use for deallocation.
///
template <typename PolymorphicDeallocator>
class polymorphic_deleter final
{
public:
    /// The allocator type this deleter uses to, uh... delete memory?
    using allocator = PolymorphicDeallocator;

    /// The type this deleter, you know...deletes? (It is in the name of the class and all)
    using value_type = typename PolymorphicDeallocator::value_type;

    /// While this object is simply a functor and could be used with other smart pointers the
    /// design is optimized for and tested with this std::unique_ptr type.
    using unique_ptr = std::unique_ptr<value_type, cetl::pmr::polymorphic_deleter<PolymorphicDeallocator>>;

    polymorphic_deleter() noexcept = delete;

    /// Designated constructor that copies a given allocator and records the object count to use when deleting
    /// the smart pointer resources.
    /// @param alloc        The allocator to use when deleting the objects in the smart pointer.
    /// @param object_count The number of objects in the smart pointer that will be deleted.
    polymorphic_deleter(const PolymorphicDeallocator& alloc, std::size_t object_count) noexcept(
        std::is_nothrow_copy_constructible<PolymorphicDeallocator>::value)
        : alloc_{alloc}
        , obj_count_{object_count}
    {
    }

    ~polymorphic_deleter() = default;

    polymorphic_deleter(polymorphic_deleter&& rhs) noexcept(
        std::is_nothrow_move_constructible<PolymorphicDeallocator>::value)
        : alloc_{rhs.alloc_}
        , obj_count_{rhs.obj_count_}
    {
    }

    polymorphic_deleter& operator=(polymorphic_deleter&& rhs) noexcept(
        std::is_nothrow_move_assignable<PolymorphicDeallocator>::value)
    {
        alloc_     = rhs.alloc_;
        obj_count_ = rhs.obj_count_;
        return *this;
    }

    polymorphic_deleter(const polymorphic_deleter& rhs) noexcept(
        std::is_nothrow_copy_constructible<PolymorphicDeallocator>::value)
        : alloc_{rhs.alloc_}
        , obj_count_{rhs.obj_count_}
    {
    }
    polymorphic_deleter& operator=(const polymorphic_deleter& rhs) noexcept(
        std::is_nothrow_copy_assignable<PolymorphicDeallocator>::value)
    {
        alloc_     = rhs.alloc_;
        obj_count_ = rhs.obj_count_;
        return *this;
    }

    /// Functor called by smart-pointer to deallocate and deconstruct objects.
    /// @param p    The object to deconstruct and deallocate.
    void operator()(value_type* p) noexcept
    {
        if (nullptr != p)
        {
#if defined(__GNUG__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            // https://cplusplus.github.io/LWG/issue3036 (deprecates this function)
            // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2875r4.pdf un-deprecated this function
            // whoops!
            alloc_.destroy(p);
#if defined(__GNUG__)
#    pragma GCC diagnostic pop
#endif
        }
        alloc_.deallocate(p, obj_count_);
    }

private:
    PolymorphicDeallocator alloc_;
    std::size_t            obj_count_;
};

/// Factory helper for creating objects with polymorphic allocators using proper RAII semantics.
/// Uses the cetl::pmr::polymorphic_deleter type to ensure proper deallocation.
///
/// Example usage:
///
/// @snippet{trimleft} example_07_polymorphic_alloc_deleter.cpp example_usage_0
/// @snippet{trimleft} example_07_polymorphic_alloc_deleter.cpp example_usage_1
/// (@ref example_07_polymorphic_alloc_deleter "See full example here...")
///
class factory final
{
public:
    ~factory() = delete;
    factory()  = delete;

    template <typename RebindAllocatorType>
    using allocator_t = typename std::allocator_traits<RebindAllocatorType>::template rebind_alloc<
        typename RebindAllocatorType::value_type>;

    template <typename RebindAllocatorType>
    using unique_ptr_t = std::unique_ptr<typename allocator_t<RebindAllocatorType>::value_type,
                                         cetl::pmr::polymorphic_deleter<allocator_t<RebindAllocatorType>>>;

    // TODO: make_unique_array
    template <typename PolymorphicAllocatorTypeU, typename... Args>
    static auto make_unique(PolymorphicAllocatorTypeU& alloc, Args&&... args) ->
        typename factory::unique_ptr_t<PolymorphicAllocatorTypeU>
    {
        typename factory::unique_ptr_t<PolymorphicAllocatorTypeU>
            p{alloc.allocate(1), typename factory::unique_ptr_t<PolymorphicAllocatorTypeU>::deleter_type{alloc, 1}};
        if (nullptr != p)
        {
            alloc.construct(p.get(), std::forward<Args>(args)...);
        }
        return p;
    }
};

/// @deprecated Renamed to cetl::pmr::memory_resource_deleter; this alias will be removed in a future major version.
/// Before C++20 the old name cannot deduce its template argument (alias-template CTAD is C++20); name the argument or
/// use the new name.
template <typename MemoryResourceType>
using MemoryResourceDeleter [[deprecated("renamed to cetl::pmr::memory_resource_deleter")]] =
    memory_resource_deleter<MemoryResourceType>;

/// @deprecated Renamed to cetl::pmr::polymorphic_deleter; this alias will be removed in a future major version.
/// Before C++20 the old name cannot deduce its template argument (alias-template CTAD is C++20); name the argument or
/// use the new name.
template <typename PolymorphicDeallocator>
using PolymorphicDeleter [[deprecated("renamed to cetl::pmr::polymorphic_deleter")]] =
    polymorphic_deleter<PolymorphicDeallocator>;

/// @deprecated Renamed to cetl::pmr::factory; this alias will be removed in a future major version.
using Factory [[deprecated("renamed to cetl::pmr::factory")]] = factory;

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_MEMORY_H_INCLUDED
