/// @file
/// Unit tests for classes that use
/// cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words BMRD

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"
#include "cetl/pf17/byte.hpp"

// +-------------------------------------------------------------------------------------------------------------------+
// | TYPES UNDER TEST
// +-------------------------------------------------------------------------------------------------------------------+
#include "cetl/pf17/array_memory_resource.hpp"
#include "cetl/pf17/buffer_memory_resource.hpp"

namespace cetlvast
{
// +-------------------------------------------------------------------------------------------------------------------+
// | TEST FIXTURES
// +-------------------------------------------------------------------------------------------------------------------+

// Buffer Specification is used to define test subjects for users of
// cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate and to provide helpers for consistent construction and test
// specializations.
template <typename T, typename TSize, typename TUpstream, typename TUpstreamSizeMax, typename ShallBeMoveConstructable>
struct BufferSpec
{
    using TestSubjectType            = T;
    using TestSubjectSize            = TSize;
    using TestSubjectUpstreamType    = TUpstream;
    using TestSubjectUpstreamSizeMax = TUpstreamSizeMax;
    using TestSubjectMustMove        = ShallBeMoveConstructable;

    BufferSpec()  = default;
    ~BufferSpec() = default;

    BufferSpec(const BufferSpec&)            = delete;
    BufferSpec(BufferSpec&&)                 = delete;
    BufferSpec& operator=(const BufferSpec&) = delete;
    BufferSpec& operator=(BufferSpec&&)      = delete;

    // clang-format off

    // +---------------------------------------------------------------------------------------------------------------+
    // | make_test_subject
    // |    Invoke in tests as `TypeParam::make_test_subject(*TestFixture::factory);`. When the spec is for a type that
    // | takes an external buffer one will be provided by the instance in `TestFixture::factory`. The reference to the
    // | test subject and any external buffer remain valid for the remainder of the ::testing::Test. Both are deleted
    // | in `::testing::TestFixture::TearDown()`.
    // +---------------------------------------------------------------------------------------------------------------+

    template <typename UBufferSpec, typename ...Args>
    static typename std::enable_if_t<
        std::is_constructible<typename UBufferSpec::TestSubjectType, void*, std::size_t, typename UBufferSpec::TestSubjectUpstreamType*, std::size_t>::value,
        typename UBufferSpec::TestSubjectType&>

    make_test_subject(UBufferSpec& spec, Args&&... args)
    {
        spec.subjects_.emplace_back(new typename UBufferSpec::TestSubjectType(static_cast<void*>(spec.new_external_array()), TestSubjectSize::value, std::forward<Args>(args)...));
        return *spec.subjects_.back();
    }

    template <typename UBufferSpec, typename ...Args>
    static typename std::enable_if_t<
        std::is_default_constructible<typename UBufferSpec::TestSubjectType>::value,
        typename UBufferSpec::TestSubjectType&>

    make_test_subject(UBufferSpec& spec, Args&&... args)
    {
        spec.subjects_.emplace_back(new typename UBufferSpec::TestSubjectType(std::forward<Args>(args)...));
        return *spec.subjects_.back();
    }

    // +---------------------------------------------------------------------------------------------------------------+
    // | move_if
    // |    Move-constructs a new test subject or returns `nullptr` if the type is not move constructable.
    // |    ```
    // |    typename TypeParam::TestSubjectType* test_subject_moved_to_perhaps = TypeParam::move_if(*TestFixture::factory, std::move(test_subject));
    // |
    // |    if (!test_subject_moved_to_perhaps)
    // |    {
    // |        GTEST_SKIP() << "Not a moveable type.";
    // |    }
    // |    typename TypeParam::TestSubjectType& test_subject_moved_to = *test_subject_moved_to_perhaps;
    // |    ```
    // +--------------------------------------------------------------------------------------------------------------+
    template<typename UBufferSpec>
    static typename std::enable_if_t<std::is_move_constructible<typename UBufferSpec::TestSubjectType>::value, typename UBufferSpec::TestSubjectType*>

    move_if(UBufferSpec& spec, typename UBufferSpec::TestSubjectType&& move_from)
    {
        static_assert(std::is_nothrow_move_constructible<typename UBufferSpec::TestSubjectType>::value, "Move constructable type should be noexcept");
        spec.subjects_.emplace_back(new typename UBufferSpec::TestSubjectType(std::move(move_from)));
        return spec.subjects_.back().get();
    }

    template<typename UBufferSpec>
    static typename std::enable_if_t<!std::is_move_constructible<typename UBufferSpec::TestSubjectType>::value, typename UBufferSpec::TestSubjectType*>

    move_if(UBufferSpec& spec, typename UBufferSpec::TestSubjectType&& move_from)
    {
        static_assert(!UBufferSpec::TestSubjectMustMove::value,
                      "Test declared a type must be move constructable but SFINAE failed for "
                      "std::is_move_constructible");
        (void) spec;
        (void) move_from;
        return nullptr;
    }
    // clang-format on

private:
    cetl::pf17::byte* new_external_array()
    {
        externals_.emplace_back(new std::array<cetl::pf17::byte, TestSubjectSize::value>());
        return externals_.back()->data();
    }

    std::vector<std::unique_ptr<std::array<cetl::pf17::byte, TestSubjectSize::value>>> externals_{};
    std::vector<std::unique_ptr<TestSubjectType>>                                      subjects_{};
};
// +-------------------------------------------------------------------------------------------------------------------+
// | TEST DEFINITIONS
// +-------------------------------------------------------------------------------------------------------------------+
// clang-format off
using MyTypes = ::testing::Types<

//                  | Test Subject Type                                       | Test Subject Max Size                     | Test Subject Upstream Type        | Test Subject Upstream Size Max        | Move Constructable |
/* 00 */  BufferSpec< cetl::pf17::pmr::UnsynchronizedBufferMemoryResource,      std::integral_constant<std::size_t, 10>,    cetl::pf17::pmr::memory_resource,   std::integral_constant<std::size_t, 0>, std::true_type      >
/* 01 */, BufferSpec< cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<10>,   std::integral_constant<std::size_t, 10>,    cetl::pf17::pmr::memory_resource,   std::integral_constant<std::size_t, 0>, std::false_type     >
>;

// clang-format on
// +-------------------------------------------------------------------------------------------------------------------+
}  // namespace cetlvast

// +-------------------------------------------------------------------------------------------------------------------+
// | TEST OBJECT
// +-------------------------------------------------------------------------------------------------------------------+
template <typename T>
class UnsynchronizedBMRDUsers : public ::testing::Test
{
protected:
    void SetUp() override
    {
        factory.reset(new T());
    }
    void TearDown() override
    {
        factory.reset();
    }

    std::unique_ptr<T> factory;
};

TYPED_TEST_SUITE(UnsynchronizedBMRDUsers, cetlvast::MyTypes, );

// +-------------------------------------------------------------------------------------------------------------------+
// | TEST CASES
// +-------------------------------------------------------------------------------------------------------------------+
TYPED_TEST(UnsynchronizedBMRDUsers, TestDataAccess)
{
    typename TypeParam::TestSubjectType& test_subject0 = TypeParam::make_test_subject(*TestFixture::factory);
    ASSERT_EQ(TypeParam::TestSubjectSize::value, test_subject0.size());
    ASSERT_TRUE(nullptr != test_subject0.data());

    const typename TypeParam::TestSubjectType& test_subject1 = TypeParam::make_test_subject(*TestFixture::factory);
    ASSERT_EQ(TypeParam::TestSubjectSize::value, test_subject1.size());
    ASSERT_TRUE(nullptr != test_subject1.data());
}

TYPED_TEST(UnsynchronizedBMRDUsers, TestAllocateDeallocate)
{
    typename TypeParam::TestSubjectType& test_subject = TypeParam::make_test_subject(*TestFixture::factory);
    ASSERT_GE(TypeParam::TestSubjectSize::value, 4)
        << "This test expects at least 4 bytes available in the test subject.";
    void* result = test_subject.allocate(1, 2);
    ASSERT_NE(nullptr, result);
#if defined(__cpp_exceptions)
    ASSERT_THROW(test_subject.allocate(1), std::bad_alloc);
#else
    ASSERT_EQ(nullptr, test_subject.allocate(1));
#endif
    test_subject.deallocate(result, 1, 2);
}

TYPED_TEST(UnsynchronizedBMRDUsers, TestIsEqual)
{
    typename TypeParam::TestSubjectType& test_subject = TypeParam::make_test_subject(*TestFixture::factory);
    ASSERT_TRUE(test_subject.is_equal(test_subject));

    typename TypeParam::TestSubjectType& test_subject_other = TypeParam::make_test_subject(*TestFixture::factory);
    ASSERT_FALSE(test_subject.is_equal(test_subject_other));
}

TYPED_TEST(UnsynchronizedBMRDUsers, TestMoveConstruct)
{
    typename TypeParam::TestSubjectType& test_subject  = TypeParam::make_test_subject(*TestFixture::factory);
    void*                                buffer_before = test_subject.data();
    void*                                allocation    = test_subject.allocate(1);
    ASSERT_NE(nullptr, allocation);
    *static_cast<cetl::pf17::byte*>(allocation) = static_cast<cetl::pf17::byte>('A');

    typename TypeParam::TestSubjectType* test_subject_moved_to_perhaps =
        TypeParam::move_if(*TestFixture::factory, std::move(test_subject));

    if (!test_subject_moved_to_perhaps)
    {
        GTEST_SKIP() << "Not a moveable type.";
    }

    typename TypeParam::TestSubjectType& test_subject_moved_to = *test_subject_moved_to_perhaps;

    ASSERT_EQ(static_cast<cetl::pf17::byte>('A'), *static_cast<cetl::pf17::byte*>(test_subject_moved_to.data()));
    ASSERT_EQ(buffer_before, test_subject_moved_to.data());

#if defined(__cpp_exceptions)
    ASSERT_THROW(test_subject_moved_to.allocate(1), std::bad_alloc);
#else
    ASSERT_EQ(nullptr, test_subject_moved_to.allocate(1));
#endif

    test_subject_moved_to.deallocate(allocation, 1);
}

TYPED_TEST(UnsynchronizedBMRDUsers, TestMaxSize)
{
    cetlvast::MockPf17MemoryResource     mock_upstream{};
    std::size_t                          mock_upstream_max_size = 10;
    typename TypeParam::TestSubjectType& test_subject =
        TypeParam::make_test_subject(*TestFixture::factory, &mock_upstream, mock_upstream_max_size);
    ASSERT_EQ(TypeParam::TestSubjectSize::value + mock_upstream_max_size, test_subject.max_size());
}

TYPED_TEST(UnsynchronizedBMRDUsers, TestReallocate)
{
    typename TypeParam::TestSubjectType& test_subject = TypeParam::make_test_subject(*TestFixture::factory);
    void* allocation = test_subject.allocate(test_subject.size(), 1);
    ASSERT_NE(nullptr, allocation);
    void* reallocation = test_subject.reallocate(allocation, test_subject.size(), 1);
    ASSERT_NE(nullptr, reallocation);
    test_subject.deallocate(reallocation, 1);
    void* new_allocation = test_subject.allocate(2);
    ASSERT_NE(nullptr, new_allocation);
}
