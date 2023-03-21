/// @file
/// Unit tests for cetl::pf17::pmr::polymorphic_allocator defined in memory_resource.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers.hpp"

#include "cetl/pf17/memory_resource.hpp"
#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <memory_resource>
#endif

// +----------------------------------------------------------------------+
// | Test Helper(s)
// +----------------------------------------------------------------------+

/// Stateless memory resource that uses new and delete.
/// While this is similar to the C++17 std::pmr::new_delete_resource it only uses new overrides available in C++14.
/// This means it cannot over-align memory and will throw/return-null if an alignment request >
/// alignof(std::max_align_t) is made.
class MaxAlignMemoryResource : public cetl::pf17::pmr::memory_resource
{
protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        if (alignment > alignof(std::max_align_t))
        {
#if __cpp_exceptions
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

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
#ifdef CETLVAST_RTTI_ENABLED
        // with RTTI we can just always be equal if this is a MaxAlignMemoryResource since
        // it uses the same heap.
        const MaxAlignMemoryResource* p = dynamic_cast<const MaxAlignMemoryResource*>(&rhs);
        return (nullptr != p);
#else
        // without RTTI we can only know that we're equal if we're the same instance.
        return (&rhs == this);
#endif
    }
};

/// Helper to encapsulate the required protocol for allocating, constructing, destroying, and deallocating objects using
/// a polymorphic_allocator and a unique_ptr. This could be considered a generic factory pattern.
template <typename T, typename ByteAllocator>
struct ObjectConstructionProtocol
{
    using value_type     = T;
    using allocator_type = ByteAllocator;
    using byte_type      = typename std::allocator_traits<allocator_type>::value_type;

    static_assert(
#if (__cplusplus >= CETL_CPP_STANDARD_17)
        std::is_same<byte_type, std::byte>::value or
#endif
            std::is_same<byte_type, cetl::pf17::byte>::value,
        "This test expects the ByteAllocator to use a known byte type.");

    static_assert(cetlvast::is_power_of_two(sizeof(byte_type)), "We only handle byte types that are powers of two.");

    ObjectConstructionProtocol(allocator_type& allocator, std::size_t allocated_size_bytes)
        : alloc_(allocator)
        , allocated_size_bytes_(allocated_size_bytes)
    {
    }

    void operator()(value_type* p)
    {
        if (p)
        {
            p->~value_type();
        }
        alloc_.get().deallocate(reinterpret_cast<byte_type*>(p), allocated_size_bytes_);
    }

    template <typename... TArgs>
    static std::unique_ptr<value_type, ObjectConstructionProtocol<value_type, allocator_type>> make_unique(
        allocator_type& alloc,
        TArgs&&... args)
    {
        std::size_t allocate_size_bytes = sizeof(value_type) / sizeof(byte_type);
        value_type* p                   = reinterpret_cast<value_type*>(alloc.allocate(allocate_size_bytes));
        alloc.construct(p, args...);
        return {p, ObjectConstructionProtocol<value_type, allocator_type>{alloc, allocate_size_bytes}};
    }

private:
    std::reference_wrapper<allocator_type> alloc_;
    std::size_t                            allocated_size_bytes_;
};

// +----------------------------------------------------------------------+
// | Test Fixture(s)
// +----------------------------------------------------------------------+
template <typename Alloc>
struct LeadingAllocType
{
    using allocator_type = Alloc;

    LeadingAllocType()
        : alloc_()
        , data_(0)
    {
    }

    LeadingAllocType(std::allocator_arg_t, const Alloc& alloc)
        : alloc_(alloc)
        , data_(0)
    {
    }

    LeadingAllocType(std::allocator_arg_t, const Alloc& alloc, int data)
        : alloc_(alloc)
        , data_(data)
    {
    }

    operator int() const
    {
        return data_;
    }

private:
    Alloc alloc_;
    int   data_;
};

template <typename Alloc>
struct TrailingAllocType
{
    using allocator_type = Alloc;

    TrailingAllocType()
        : alloc_()
        , data_(0)
    {
    }

    TrailingAllocType(const Alloc& alloc)
        : alloc_(alloc)
        , data_(0)
    {
    }

    TrailingAllocType(int data, const Alloc& alloc)
        : alloc_(alloc)
        , data_(data)
    {
    }

    operator int() const
    {
        return data_;
    }

private:
    Alloc alloc_;
    int   data_;
};

struct NoAllocType
{
    NoAllocType()
        : data_(0)
    {
    }

    NoAllocType(int data)
        : data_(data)
    {
    }

    operator int() const
    {
        return data_;
    }

private:
    int data_;
};

// +----------------------------------------------------------------------+
// | Test Suite :: TestPolymorphicAllocatorProtocols
// +----------------------------------------------------------------------+

///
/// Test suite to verify that cetl::pf17::pmr::polymorphic_allocator adheres
/// to all conventions/protocols required of it by the C++ specification.
///
template <typename T>
class TestPolymorphicAllocatorProtocols : public ::testing::Test
{
protected:
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    template <typename U>
    static typename std::enable_if<std::is_constructible<U, std::pmr::memory_resource*>::value, U>::type
    make_default_allocator()
    {
        return U(std::pmr::new_delete_resource());
    }
#endif

    template <typename U>
    static typename std::enable_if<std::is_constructible<U, cetl::pf17::pmr::memory_resource*>::value, U>::type
    make_default_allocator()
    {
        static MaxAlignMemoryResource default_resource{};
        return U(&default_resource);
    }
};

// clang-format off
using TestPolymorphicAllocatorTypes = ::testing::Types<
      cetl::pf17::pmr::polymorphic_allocator<cetl::pf17::byte>
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    , std::pmr::polymorphic_allocator<std::byte>
#endif
>;
// clang-format on

TYPED_TEST_SUITE(TestPolymorphicAllocatorProtocols, TestPolymorphicAllocatorTypes, );

// +----------------------------------------------------------------------+

TYPED_TEST(TestPolymorphicAllocatorProtocols, TestDefaultConstruction)
{
    TypeParam                       subject = TestFixture::template make_default_allocator<TypeParam>();
    typename TypeParam::value_type* p       = subject.allocate(1);
    ASSERT_FALSE(nullptr == p);
    subject.deallocate(p, 1);
}

// +----------------------------------------------------------------------+

TYPED_TEST(TestPolymorphicAllocatorProtocols, TestUsesAllocatorConstructionNoAllocator)
{
    const int test_value    = 0xAA;
    TypeParam subject       = TestFixture::template make_default_allocator<TypeParam>();
    auto      test_instance = ObjectConstructionProtocol<NoAllocType, TypeParam>::make_unique(subject, test_value);
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get(), alignof(std::max_align_t)));
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get()));
    ASSERT_EQ(test_value, *test_instance);
}

// +----------------------------------------------------------------------+

TYPED_TEST(TestPolymorphicAllocatorProtocols, TestUsesAllocatorConstructionLeading)
{
    const int test_value = 0xAA;
    TypeParam subject    = TestFixture::template make_default_allocator<TypeParam>();
    auto      test_instance =
        ObjectConstructionProtocol<LeadingAllocType<TypeParam>, TypeParam>::make_unique(subject, test_value);
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get(), alignof(std::max_align_t)));
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get()));
    ASSERT_EQ(test_value, *test_instance);
}

// +----------------------------------------------------------------------+

TYPED_TEST(TestPolymorphicAllocatorProtocols, TestUsesAllocatorConstructionTrailing)
{
    const int test_value = 0xAA;
    TypeParam subject    = TestFixture::template make_default_allocator<TypeParam>();
    auto      test_instance =
        ObjectConstructionProtocol<TrailingAllocType<TypeParam>, TypeParam>::make_unique(subject, test_value);
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get(), alignof(std::max_align_t)));
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get()));
    ASSERT_EQ(test_value, *test_instance);
}

// +----------------------------------------------------------------------+

TYPED_TEST(TestPolymorphicAllocatorProtocols, TestPairConstructionNoAllocator)
{
    const int test_value = 0xAA;
    TypeParam subject    = TestFixture::template make_default_allocator<TypeParam>();
    using PairType       = std::pair<NoAllocType, NoAllocType>;
    auto test_instance = ObjectConstructionProtocol<PairType, TypeParam>::make_unique(subject, test_value, test_value);
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get(), alignof(std::max_align_t)));
    ASSERT_TRUE(cetlvast::is_aligned(test_instance.get()));
    ASSERT_EQ(test_value, test_instance->first);
    ASSERT_EQ(test_value, test_instance->second);
}
