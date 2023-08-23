/// @file
/// CETL VerificAtion SuiTe – Google test helpers that includes memory_resource.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words soccc

#ifndef CETLVAST_HELPERS_GTEST_MEMORY_RESOURCE_H_INCLUDED
#define CETLVAST_HELPERS_GTEST_MEMORY_RESOURCE_H_INCLUDED

#include "cetlvast/helpers_gtest.hpp"

#include "cetl/pf17/memory_resource.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"
#if (__cplusplus >= CETL_CPP_STANDARD_17)
// As of xcode 14, Apple clang doesn't support PMR yet. It's available under
// an experimental namespace but it isn't fully integrated into Apple's
// standard library.
#    include <memory_resource>
#endif

namespace cetlvast
{

/// PF17 memory resource that does _not_ implement realloc.
class MaxAlignNewDeleteResourceWithoutRealloc : public cetl::pf17::pmr::memory_resource
{
protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        if (alignment > alignof(std::max_align_t))
        {
#if defined(__cpp_exceptions)
            throw std::bad_alloc();
#endif
            return nullptr;
        }
        return ::operator new(size_bytes);
    }

    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        (void) size_bytes;
        (void) alignment;
        ::operator delete(p);
    }

    void* do_reallocate(void*       ptr,
                        std::size_t old_size_bytes,
                        std::size_t new_size_bytes,
                        std::size_t alignment) override
    {
        (void) ptr;
        (void) old_size_bytes;
        (void) new_size_bytes;
        (void) alignment;
        return nullptr;
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return (&rhs == this);
    }
};

/// Pf17 Mock of memory_resource.
class MockPf17MemoryResource : public cetl::pf17::pmr::memory_resource
{
public:
    MOCK_METHOD(void*, do_allocate, (std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(void, do_deallocate, (void* p, std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(bool, do_is_equal, (const cetl::pf17::pmr::memory_resource& rhs), (const, noexcept));
    MOCK_METHOD(void*,
                do_reallocate,
                (void* p, std::size_t old_size_bytes, std::size_t new_size_bytes, std::size_t new_alignment));

    static constexpr bool ReturnsNullWhenFNoExceptions = true;

    static cetl::pf17::pmr::memory_resource* get()
    {
        return cetl::pf17::pmr::null_memory_resource();
    }
};

/// No-type memory resource that looks like std::pmr::memory_resource.
class MockMemoryResource
{
public:
    MOCK_METHOD(void*, allocate, (std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(void, deallocate, (void* p, std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(bool, is_equal, (const MockMemoryResource& rhs), (const, noexcept));
};

#if (__cplusplus >= CETL_CPP_STANDARD_17)
/// Std Mock of memory_resource.
class MockStdMemoryResource : public std::pmr::memory_resource
{
public:
    MOCK_METHOD(void*, do_allocate, (std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(void, do_deallocate, (void* p, std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(bool, do_is_equal, (const std::pmr::memory_resource& rhs), (const, noexcept));

    static constexpr bool ReturnsNullWhenFNoExceptions = false;

    static std::pmr::memory_resource* get()
    {
        return std::pmr::null_memory_resource();
    }
};
#endif

// +----------------------------------------------------------------------+

/// Memory Resource Helper (MRH)
/// Support for parameterized tests that use both std::pmr::memory_resource and cetl::pf17::pmr::memory_resource.
/// For example:
/// ```
///    typename cetlvast::MRH::template MemoryResourceType<TypeParam>* resource =
//          cetlvast::MRH::template new_delete_resource_by_tag<TypeParam>();
/// ```
struct MRH final
{
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    template <typename TagType>
    using MemoryResourceType = std::conditional_t<std::is_same<cetlvast::CETLTag, TagType>::value,
                                                  cetl::pf17::pmr::memory_resource,
                                                  std::pmr::memory_resource>;

#else
    template <typename TagType>
    using MemoryResourceType = cetl::pf17::pmr::memory_resource;
#endif

#if (__cplusplus >= CETL_CPP_STANDARD_17)

    template <typename T>
    constexpr static std::enable_if_t<std::is_base_of<std::pmr::memory_resource, T>::value, std::pmr::memory_resource*>
    null_memory_resource() noexcept
    {
        return std::pmr::null_memory_resource();
    }

    template <typename T>
    using MockMemoryResourceType = std::conditional_t<
        std::is_base_of<cetl::pf17::pmr::memory_resource, T>::value,
        MockPf17MemoryResource,
        std::conditional_t<std::is_base_of<std::pmr::memory_resource, T>::value, MockStdMemoryResource, void>>;

#else
    template <typename T>
    using MockMemoryResourceType =
        std::conditional_t<std::is_base_of<cetl::pf17::pmr::memory_resource, T>::value, MockPf17MemoryResource, void>;
#endif

    template <typename T>
    constexpr static std::enable_if_t<std::is_base_of<cetl::pf17::pmr::memory_resource, T>::value,
                                      cetl::pf17::pmr::memory_resource*>
    null_memory_resource() noexcept
    {
        return cetl::pf17::pmr::null_memory_resource();
    }

    template <typename TagType>
    static typename std::enable_if_t<std::is_same<cetlvast::CETLTag, TagType>::value, cetl::pf17::pmr::memory_resource*>
    new_delete_resource_by_tag()
    {
        return cetl::pf17::pmr::new_delete_resource();
    }

#if (__cplusplus >= CETL_CPP_STANDARD_17)
    template <typename TagType>
    static typename std::enable_if_t<std::is_same<cetlvast::STLTag, TagType>::value, std::pmr::memory_resource*>
    new_delete_resource_by_tag()
    {
        return std::pmr::new_delete_resource();
    }
#endif

    MRH() = delete;
};

// +----------------------------------------------------------------------+
// | TYPED CONTAINER ALLOCATOR FACTORIES
// +----------------------------------------------------------------------+

/// Creates std::allocator instances
struct DefaultAllocatorFactory
{
    DefaultAllocatorFactory() = delete;

    template <typename ValueType>
    using allocator_type = std::allocator<ValueType>;

    template <typename ValueType>
    static allocator_type<ValueType> make_allocator()
    {
        return std::allocator<ValueType>{};
    }

    template <typename ValueType>
    static void reset()
    {
        // no-op
    }
};

// +---------------------------------------------------------------------------+

/// Creates cetl::pf17::pmr::polymorphic_allocator instances that use new and delete
struct PolymorphicAllocatorNewDeleteFactory
{
    PolymorphicAllocatorNewDeleteFactory() = delete;

    template <typename ValueType>
    using allocator_type = cetl::pf17::pmr::polymorphic_allocator<ValueType>;

    template <typename ValueType>
    static allocator_type<ValueType> make_allocator()
    {
        return allocator_type<ValueType>{cetl::pf17::pmr::new_delete_resource()};
    }

    template <typename ValueType>
    static void reset()
    {
        // no-op
    }
};

// +---------------------------------------------------------------------------+

/// Creates cetl::pf17::pmr::polymorphic_allocator instances that a monotonic buffer
/// with new and delete when the buffer is exhausted.
template <std::size_t MonotonicBufferSize = 4>
struct PolymorphicAllocatorNewDeleteBackedMonotonicFactory
{
    template <typename ValueType>
    using allocator_type = cetl::pf17::pmr::polymorphic_allocator<ValueType>;

    template <typename ValueType>
    struct MonotonicArray
    {
        std::array<ValueType, MonotonicBufferSize> storage;
        cetl::pf17::pmr::monotonic_buffer_resource resource{storage.data(),
                                                            storage.size(),
                                                            cetl::pf17::pmr::new_delete_resource()};
    };

    template <typename ValueType>
    static std::vector<std::unique_ptr<MonotonicArray<ValueType>>>& buffers()
    {
        static std::vector<std::unique_ptr<MonotonicArray<ValueType>>> buffers;
        return buffers;
    }

    template <typename ValueType>
    static allocator_type<ValueType> make_allocator()
    {
        auto& buffers_ref = buffers<ValueType>();
        buffers_ref.push_back(std::make_unique<MonotonicArray<ValueType>>());
        return allocator_type<ValueType>{&buffers_ref.back()->resource};
    }

    template <typename ValueType>
    static void reset()
    {
        buffers<ValueType>().clear();
    }
};

// +---------------------------------------------------------------------------+
// | TYPED CONTAINER ALLOCATOR PROTOCOL
// +---------------------------------------------------------------------------+

/// Pretty-typing for parameterized tests that use allocator factories.
///
/// Given an allocator factory type, creates a type for parameterized unit-tests that both defines the allocator type as
/// allocator_type and provides a static make_allocator() method that delegates to the allocator factory's
/// make_allocator method. A static reset() method is also provided which delegates to the allocator factory's reset
/// method.
template <typename AllocatorFactoryType, typename AllocatorValueType>
struct AllocatorTypeParamDef
{
    AllocatorTypeParamDef() = delete;

    using allocator_type    = typename AllocatorFactoryType::template allocator_type<AllocatorValueType>;
    using allocator_factory = AllocatorFactoryType;

    static constexpr allocator_type make_allocator()
    {
        return allocator_factory::template make_allocator<typename allocator_type::value_type>();
    }

    static constexpr void reset()
    {
        allocator_factory::template reset<typename allocator_type::value_type>();
    }
};

// +---------------------------------------------------------------------------+
// | INSTRUMENTED ALLOCATOR
// +---------------------------------------------------------------------------+
/// Used by InstrumentedNewDeleteAllocator to track allocations and deallocations.
struct InstrumentedAllocatorStatistics
{
private:
    InstrumentedAllocatorStatistics()
        : outstanding_allocated_memory{0}
        , allocations{0}
        , deallocations{0}
        , allocated_bytes{0}
        , deallocated_bytes{0}
        , last_allocation_size_bytes{0}
        , last_deallocation_size_bytes{0}
    {
    }

    InstrumentedAllocatorStatistics& operator=(const InstrumentedAllocatorStatistics&) = default;
    InstrumentedAllocatorStatistics& operator=(InstrumentedAllocatorStatistics&&)      = default;

public:
    InstrumentedAllocatorStatistics(const InstrumentedAllocatorStatistics&) = delete;
    InstrumentedAllocatorStatistics(InstrumentedAllocatorStatistics&&)      = delete;

    std::size_t outstanding_allocated_memory;
    std::size_t allocations;
    std::size_t deallocations;
    std::size_t allocated_bytes;
    std::size_t deallocated_bytes;
    std::size_t last_allocation_size_bytes;
    std::size_t last_deallocation_size_bytes;

    static ::testing::AssertionResult subtract_or_assert(std::size_t& lhs, const std::size_t rhs)
    {
        if (rhs > lhs)
        {
            return ::testing::AssertionFailure() << "Attempted to subtract " << rhs << " from " << lhs << std::endl;
        }
        lhs -= rhs;
        return ::testing::AssertionSuccess();
    }

    ::testing::AssertionResult record_deallocation(const std::size_t amount_bytes)
    {
        subtract_or_assert(outstanding_allocated_memory, amount_bytes)
            << "Attempted to deallocate " << deallocated_bytes << " bytes, but only " << outstanding_allocated_memory
            << " bytes were allocated." << std::endl;
        deallocations += 1;
        deallocated_bytes += amount_bytes;
        last_deallocation_size_bytes = amount_bytes;
        return ::testing::AssertionSuccess();
    }

    ::testing::AssertionResult record_allocation(const std::size_t amount_bytes)
    {
        outstanding_allocated_memory += amount_bytes;
        allocations += 1;
        allocated_bytes += amount_bytes;
        last_allocation_size_bytes = amount_bytes;
        return ::testing::AssertionSuccess();
    }

    static InstrumentedAllocatorStatistics& get()
    {
        static InstrumentedAllocatorStatistics stats;
        return stats;
    }

    static void reset()
    {
        get() = InstrumentedAllocatorStatistics{};
    }
};

/// Allocator that uses the standard heap but which can mimic the behavior of
/// polymorphic allocators. This allocator also collects statistics about
/// allocations and deallocations.
/// @tparam IsAlwaysEqual Mimic the is_always_equal property of polymorphic allocators.
/// @tparam IsEqual       Pretend to be equal if IsAlwaysEqual is false.
/// @tparam IsPropOnMove  Mimic the propagate_on_container_move_assignment property of polymorphic allocators.
/// @tparam IsPropOnCopy  Mimic the propagate_on_container_copy_assignment property of polymorphic allocators.
template <typename T,
          typename IsAlwaysEqual = std::true_type,
          typename IsEqual       = std::true_type,
          typename IsPropOnMove  = std::false_type,
          typename IsPropOnCopy  = std::false_type>
struct InstrumentedNewDeleteAllocator
{
    using value_type = T;
    using pointer    = T*;
    using size_type  = std::size_t;

    InstrumentedNewDeleteAllocator()
        : is_invalid{false}
        , was_from_soccc{false}
        , allocated_bytes{0}
    {
    }

    InstrumentedNewDeleteAllocator(const InstrumentedNewDeleteAllocator& rhs,
                                   bool                                  is_soccc) noexcept(IsPropOnCopy::value)
        : is_invalid{false}
        , was_from_soccc{is_soccc}
        , allocated_bytes{0}
    {
        (void) rhs;
    }

    InstrumentedNewDeleteAllocator(const InstrumentedNewDeleteAllocator& rhs) noexcept(IsPropOnCopy::value)
        : InstrumentedNewDeleteAllocator(rhs, rhs.was_from_soccc)
    {
    }

    InstrumentedNewDeleteAllocator(InstrumentedNewDeleteAllocator&& rhs) noexcept
        : is_invalid{rhs.is_invalid}
        , was_from_soccc{rhs.was_from_soccc}
        , allocated_bytes{rhs.allocated_bytes}
    {
        rhs.allocated_bytes = 0;
        rhs.is_invalid      = true;
    }

    ~InstrumentedNewDeleteAllocator() {}

    InstrumentedNewDeleteAllocator& operator=(const InstrumentedNewDeleteAllocator& rhs) noexcept(IsPropOnCopy::value)
    {
        EXPECT_FALSE(rhs.is_invalid) << "Attempted to copy from an invalid allocator." << std::endl;
        EXPECT_FALSE(is_invalid) << "Attempted to copy to an invalid allocator." << std::endl;
        if (!IsAlwaysEqual::value && !IsEqual::value)
        {
            EXPECT_EQ(allocated_bytes, 0) << "leaked " << allocated_bytes << " bytes in copy assignment." << std::endl;
        }
        allocated_bytes = rhs.allocated_bytes;
        return *this;
    }

    InstrumentedNewDeleteAllocator& operator=(InstrumentedNewDeleteAllocator&& rhs) noexcept(IsPropOnMove::value)
    {
        EXPECT_FALSE(rhs.is_invalid) << "Attempted to move from an invalid allocator." << std::endl;
        EXPECT_FALSE(is_invalid) << "Attempted to move to an invalid allocator." << std::endl;
        if (IsPropOnMove::value)
        {
            allocated_bytes += rhs.allocated_bytes;
        }
        else
        {
            EXPECT_TRUE(IsAlwaysEqual::value || IsEqual::value)
                << "Attempted to move from an allocator that is neither equal nor marked for propagation on move."
                << std::endl;
            EXPECT_EQ(allocated_bytes, 0) << "leaked " << allocated_bytes << " bytes in move assignment." << std::endl;
            allocated_bytes = rhs.allocated_bytes;
        }
        rhs.allocated_bytes = 0;
        rhs.is_invalid      = true;
        return *this;
    }

    bool operator==(const InstrumentedNewDeleteAllocator& rhs) const
    {
        (void) rhs;
        return (IsAlwaysEqual::value || IsEqual::value);
    }

    bool operator!=(const InstrumentedNewDeleteAllocator& rhs) const
    {
        (void) rhs;
        return (!IsAlwaysEqual::value && !IsEqual::value);
    }

    using is_always_equal                        = IsAlwaysEqual;
    using is_equal                               = IsEqual;
    using propagate_on_container_move_assignment = IsPropOnMove;
    using propagate_on_container_copy_assignment = IsPropOnCopy;

    InstrumentedNewDeleteAllocator select_on_container_copy_construction() const
    {
        return InstrumentedNewDeleteAllocator(*this, true);
    }

    template <class U>
    struct rebind
    {
        typedef InstrumentedNewDeleteAllocator<U, IsAlwaysEqual, IsEqual, IsPropOnMove, IsPropOnCopy> other;
    };

    pointer allocate(size_type n, const void* hint = nullptr)
    {
        EXPECT_FALSE(is_invalid) << "Attempted to allocate from an invalid allocator." << std::endl;
        (void) hint;
        const std::size_t bytes_to_allocate = (n * sizeof(T));
        InstrumentedAllocatorStatistics::get().record_allocation(bytes_to_allocate);
        allocated_bytes += bytes_to_allocate;
        return reinterpret_cast<pointer>(::operator new(bytes_to_allocate));
    }

    void deallocate(T* p, std::size_t n)
    {
        EXPECT_FALSE(is_invalid) << "Attempted to deallocate from an invalid allocator." << std::endl;
        const std::size_t bytes_to_deallocate = (n * sizeof(T));
        allocated_bytes -= bytes_to_deallocate;
        InstrumentedAllocatorStatistics::get().record_deallocation(bytes_to_deallocate);
        ::operator delete(p);
    }

    template <class U, class... Args>
    void construct(U* p, Args&&... args)
    {
        EXPECT_FALSE(is_invalid) << "Attempted to construct from an invalid allocator." << std::endl;
        ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
    }

    bool       is_invalid;
    const bool was_from_soccc;

private:
    std::size_t allocated_bytes;
};

}  // namespace cetlvast

#endif  // CETLVAST_HELPERS_GTEST_MEMORY_RESOURCE_H_INCLUDED
