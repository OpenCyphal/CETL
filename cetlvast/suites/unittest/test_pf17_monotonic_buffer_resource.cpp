/// @file
/// Unit tests for cetl::pf17::pmr::monotonic_buffer_resource
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetlvast/helpers_gtest_memory_resource.hpp"
#include "cetl/pf17/cetlpf.hpp"

using ::testing::_;
using ::testing::Return;
using ::testing::Sequence;
using ::testing::Ge;

// +----------------------------------------------------------------------+
// | TEST SUITE :: TestMonotonicBufferResource
// +----------------------------------------------------------------------+
template <typename T>
class TestMonotonicBufferResource : public ::testing::Test
{};

// clang-format off
using MonotonicBufferResourceTypes = ::testing::Types<
      cetl::pf17::pmr::monotonic_buffer_resource
#if (__cplusplus >= CETL_CPP_STANDARD_17)
    , std::pmr::monotonic_buffer_resource
#endif
>;
// clang-format on

TYPED_TEST_SUITE(TestMonotonicBufferResource, MonotonicBufferResourceTypes, );

// +----------------------------------------------------------------------+

/// Test that the default constructor provides a viable, upstream, memory resource.
TYPED_TEST(TestMonotonicBufferResource, TestDefaultConstruction)
{
    TypeParam subject{};
    void*     memory = subject.allocate(1024);
    ASSERT_NE(nullptr, memory);
    subject.release();
}

// +----------------------------------------------------------------------+

/// Ensure deallocate has no effect on the monotonic resource.
TYPED_TEST(TestMonotonicBufferResource, TestDeallocateHasNoEffect)
{
    static_assert(alignof(cetl::byte) <= alignof(std::max_align_t),
                  "Assumptions about the alignment of cetl::byte are wrong");
    constexpr std::size_t                   size_bytes = 1024;
    std::array<cetl::byte, size_bytes>      buffer{};
    TypeParam subject{buffer.data(), buffer.size(), cetlvast::MRH::null_memory_resource<TypeParam>()};
    ASSERT_NE(nullptr, subject.upstream_resource());
    void* memory = subject.allocate(size_bytes / 2);
    ASSERT_NE(nullptr, memory);
    // first we try to allocate more then the buffer can hold. This should
    // cause the resource to try using the null_memory_resource, which will
    // throw an exception or return null (depending on __cpp_exceptions).
    // TODO: test helper that is expect-throw-if-except-else-nullptr-if-cetl-else-dont-do-test.
#if __cpp_exceptions
    EXPECT_THROW((void) subject.allocate(size_bytes), std::bad_alloc);
#elif (__cplusplus == CETL_CPP_STANDARD_14)
    ASSERT_EQ(nullptr, subject.allocate(size_bytes));
#else
    GTEST_SKIP() << "C++17 pmr does not support defined out of memory behaviour without exceptions.";
#endif
    // now we deallocate the memory we allocated. This should have no effect.
    subject.deallocate(memory, size_bytes / 2);
#if __cpp_exceptions
    EXPECT_THROW((void) subject.allocate(size_bytes), std::bad_alloc);
#else
    ASSERT_EQ(nullptr, subject.allocate(size_bytes));
#endif
    // finally, we call release which should reset the resource.
    subject.release();
    // now we should be able to allocate again.
    ASSERT_NE(nullptr, subject.allocate(size_bytes / 2));
}

// +----------------------------------------------------------------------+

/// Test that the upstream is used only after the internal buffer is exhausted.
TYPED_TEST(TestMonotonicBufferResource, TestAllocationOrder)
{
    static_assert(alignof(cetl::byte) <= alignof(std::max_align_t),
                  "Assumptions about the alignment of cetl::byte are wrong");
    constexpr std::size_t                            size_bytes = 1024;
    std::array<cetl::byte, size_bytes>               buffer{};
    std::array<cetl::byte, size_bytes * 2>           upstream_buffer{};
    cetlvast::MRH::MockMemoryResourceType<TypeParam> mock{};
    Sequence                                         s1;
    EXPECT_CALL(mock, do_allocate(Ge(size_bytes), _)).Times(1).InSequence(s1).WillOnce(Return(upstream_buffer.data()));
    EXPECT_CALL(mock, do_deallocate(upstream_buffer.data(), Ge(size_bytes), _)).Times(1).InSequence(s1);

    TypeParam subject{buffer.data(), buffer.size(), &mock};
    ASSERT_EQ(&mock, subject.upstream_resource());
    void* memory = subject.allocate(size_bytes / 2);
    ASSERT_NE(nullptr, memory);
    void* upstream_memory = subject.allocate(size_bytes);
    ASSERT_NE(nullptr, upstream_memory);
}

// +----------------------------------------------------------------------+

/// Verify fix for issue #45
TYPED_TEST(TestMonotonicBufferResource, TestIssue45)
{
    constexpr std::size_t                   size_bytes = 1024;
    std::array<cetl::byte, size_bytes>      buffer{};
    TypeParam subject{buffer.data(), buffer.size(), cetlvast::MRH::null_memory_resource<TypeParam>()};
    for(std::size_t i = 0; i < 1024 / (2 * alignof(cetl::byte)); ++i)
    {
        void*     memory0 = subject.allocate(1, alignof(cetl::byte));
        ASSERT_NE(nullptr, memory0);
        void*     memory1 = subject.allocate(1, alignof(cetl::byte));
        ASSERT_NE(memory0, memory1);
        ASSERT_LT(memory0, memory1);
        std::ptrdiff_t range = static_cast<unsigned char*>(memory1) - static_cast<unsigned char*>(memory0);
        ASSERT_GE(range, sizeof(cetl::byte));
    }
    subject.release();
}
