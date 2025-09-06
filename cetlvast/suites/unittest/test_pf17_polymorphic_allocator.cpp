/// @file
/// Unit tests for cetl::pf17::pmr::polymorphic_allocator defined in memory_resource.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pf17/byte.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"

#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <memory_resource>
#endif

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
#if defined(__GNUG__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

            alloc_.get().destroy(p);
#if defined(__GNUG__)
#    pragma GCC diagnostic pop
#endif
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

    explicit TrailingAllocType(const Alloc& alloc)
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

    explicit NoAllocType(int data)
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
    static typename std::enable_if_t<std::is_constructible<U, std::pmr::memory_resource*>::value, U>
    make_default_allocator()
    {
        return U(std::pmr::new_delete_resource());
    }
#endif

    template <typename U>
    static typename std::enable_if_t<std::is_constructible<U, cetl::pf17::pmr::memory_resource*>::value, U>
    make_default_allocator()
    {
        return U(cetl::pf17::pmr::new_delete_resource());
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

// +----------------------------------------------------------------------+
// | Test Suite :: TestPolymorphicAllocatorMoveOnlyProtocols
// +----------------------------------------------------------------------+
///
/// Test suite to verify that cetl::pf17::pmr::polymorphic_allocator adheres
/// to all conventions/protocols required of it by the C++ specification when
/// using move-only types.
///
template <typename T>
class TestPolymorphicAllocatorMoveOnlyProtocols : public ::testing::Test
{
public:
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    template <typename U>
    using AllocatorType = std::conditional_t<std::is_same<cetlvast::CETLTag, T>::value,
                                             cetl::pf17::pmr::polymorphic_allocator<U>,
                                             std::pmr::polymorphic_allocator<U>>;

#else
    template <typename U>
    using AllocatorType = cetl::pf17::pmr::polymorphic_allocator<U>;

#endif
};

// clang-format off
using TestPolymorphicAllocatorMoveOnlyProtocolsTypes = ::testing::Types<
      cetlvast::CETLTag
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    , cetlvast::STLTag
#endif
>;
// clang-format on

TYPED_TEST_SUITE(TestPolymorphicAllocatorMoveOnlyProtocols, TestPolymorphicAllocatorMoveOnlyProtocolsTypes, );

namespace
{

struct OnlyMovable
{
    OnlyMovable()
        : value{nullptr}
    {
    }

    OnlyMovable(OnlyMovable&& rhs)
        : value(rhs.value)
    {
        rhs.value = nullptr;
    }

    OnlyMovable& operator=(OnlyMovable&& rhs)
    {
        value     = rhs.value;
        rhs.value = nullptr;
        return *this;
    }

    OnlyMovable(const OnlyMovable&)            = delete;
    OnlyMovable& operator=(const OnlyMovable&) = delete;

    OnlyMovable(int* value)
        : value(value)
    {
    }

    int* value;
};

}  // namespace

// +----------------------------------------------------------------------+

TYPED_TEST(TestPolymorphicAllocatorMoveOnlyProtocols, TestDefaultConstruction)
{
    typename cetlvast::MRH::template MemoryResourceType<TypeParam>* resource =
        cetlvast::MRH::template new_delete_resource_by_tag<TypeParam>();
    using AllocatorType = typename TestFixture::template AllocatorType<OnlyMovable>;
    AllocatorType subject{resource};
    OnlyMovable*  p = subject.allocate(1);
    ASSERT_FALSE(nullptr == p);
    subject.deallocate(p, 1);
}

// +----------------------------------------------------------------------+

TYPED_TEST(TestPolymorphicAllocatorMoveOnlyProtocols, TestEmplace)
{
    int                                                             test_data       = 0;
    int                                                             other_test_data = 0;
    typename cetlvast::MRH::template MemoryResourceType<TypeParam>* resource =
        cetlvast::MRH::template new_delete_resource_by_tag<TypeParam>();
    using AllocatorType = typename TestFixture::template AllocatorType<OnlyMovable>;
    AllocatorType subject{resource};
    OnlyMovable*  p = subject.allocate(1);
    ASSERT_FALSE(nullptr == p);
    subject.construct(p, &test_data);
    ASSERT_EQ(&test_data, p->value);
    OnlyMovable other{&other_test_data};
    // This is where Issue #41 was found
    subject.construct(p, std::move(other));
    ASSERT_EQ(&other_test_data, p->value);
    ASSERT_EQ(nullptr, other.value);
    subject.deallocate(p, 1);
}

// +----------------------------------------------------------------------+
// | DEATH TESTS
// +----------------------------------------------------------------------+

#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

static void TestNullResourceToCtor()
{
    flush_coverage_on_death();
    cetl::pf17::pmr::polymorphic_allocator<int> subject{nullptr};
    static_cast<void>(subject);
}

TEST(DeathTestPmrPolymorphicAllocator, TestNullResourceToCtor)
{
    EXPECT_DEATH(TestNullResourceToCtor(), "CDE_pmr_001");
}

static void TestNullPointerToPairConstruct()
{
    flush_coverage_on_death();
    cetl::pf17::pmr::polymorphic_allocator<std::pair<int, int>> subject{cetl::pf17::pmr::get_default_resource()};
    std::pair<int, int>*                                        p{nullptr};
    static_cast<void>(subject.construct(p));
}

TEST(DeathTestPmrPolymorphicAllocator, TestNullPointerToPairConstruct)
{
    EXPECT_DEATH(TestNullPointerToPairConstruct(), "CDE_pmr_005");
}

static void TestNullPointerToNoPairConstructNotAlloc()
{
    flush_coverage_on_death();
    cetl::pf17::pmr::polymorphic_allocator<int> subject{cetl::pf17::pmr::get_default_resource()};
    int*                                        p{nullptr};
    static_cast<void>(subject.construct(p));
}

TEST(DeathTestPmrPolymorphicAllocator, TestNullPointerToNoPairConstructNotAlloc)
{
    EXPECT_DEATH(TestNullPointerToNoPairConstructNotAlloc(), "CDE_pmr_002");
}

static void TestNullPointerToNoPairConstructAllocFirst()
{
    flush_coverage_on_death();
    cetl::pf17::pmr::polymorphic_allocator<int>                    subject{cetl::pf17::pmr::get_default_resource()};
    LeadingAllocType<cetl::pf17::pmr::polymorphic_allocator<int>>* p{nullptr};
    static_cast<void>(subject.construct(p, 1));
}

TEST(DeathTestPmrPolymorphicAllocator, TestNullPointerToNoPairConstructAllocFirst)
{
    EXPECT_DEATH(TestNullPointerToNoPairConstructAllocFirst(), "CDE_pmr_003");
}

static void TestNullPointerToNoPairConstructAllocLast()
{
    flush_coverage_on_death();
    cetl::pf17::pmr::polymorphic_allocator<int>                     subject{cetl::pf17::pmr::get_default_resource()};
    TrailingAllocType<cetl::pf17::pmr::polymorphic_allocator<int>>* p{nullptr};
    static_cast<void>(subject.construct(p, 1));
}

TEST(DeathTestPmrPolymorphicAllocator, TestNullPointerToNoPairConstructAllocLast)
{
    EXPECT_DEATH(TestNullPointerToNoPairConstructAllocLast(), "CDE_pmr_004");
}

#endif  // CETL_ENABLE_DEBUG_ASSERT
