/// @file
/// Unit tests for cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
// cSpell: words CDE_ubmrd

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest.hpp"
#include "cetlvast/helpers_gtest_memory_resource.hpp"
#include "cetl/pf17/byte.hpp"

#include "cetl/pmr/buffer_memory_resource.hpp"

using ::testing::Return;
using ::testing::Expectation;

constexpr std::size_t   TestBufferSize = 0x100000;
static cetl::pf17::byte large_buffer[TestBufferSize];

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestNullBuffer)
{
    cetlvast::MockPf17MemoryResource                                                        mock_upstream{};
    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource> test_subject{nullptr,
                                                                                                         10,
                                                                                                         &mock_upstream,
                                                                                                         0};
#if defined(__cpp_exceptions)
    ASSERT_THROW(test_subject.allocate(1, 1), std::bad_alloc);
#else
    ASSERT_EQ(nullptr, test_subject.allocate(200));
#endif
    // deallocate must be null-safe so this should have no ill effect.
    test_subject.deallocate(nullptr, 1, 1);
}

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestLargeBuffer)
{
    cetlvast::MockPf17MemoryResource                                                        mock_upstream{};
    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource> test_subject{large_buffer,
                                                                                                         TestBufferSize,
                                                                                                         &mock_upstream,
                                                                                                         0};
    void* mem = test_subject.allocate(TestBufferSize);
    ASSERT_NE(nullptr, mem);
    ASSERT_EQ(TestBufferSize * sizeof(cetl::pf17::byte), test_subject.max_size());
    test_subject.deallocate(mem, TestBufferSize);
}

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestLocalReallocate)
{
    cetlvast::MockMemoryResource                                                        mock_upstream{};
    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetlvast::MockMemoryResource> test_subject{large_buffer,
                                                                                                     TestBufferSize,
                                                                                                     &mock_upstream,
                                                                                                     0};
    void*                                                                               mem = test_subject.allocate(1);
    ASSERT_NE(nullptr, mem);
#if defined(__cpp_exceptions)
    ASSERT_THROW(test_subject.allocate(200), std::bad_alloc);
#else
    ASSERT_EQ(nullptr, test_subject.allocate(200));
#endif
    void* reallocated_mem = test_subject.reallocate(mem, 1, 200);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(reallocated_mem, 200);
}

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestUpstreamSpilloverOnReallocate)
{
    cetlvast::MockPf17MemoryResource mock_upstream{};
    cetl::pf17::byte                 buffer[10];
    cetl::pf17::byte                 upstream_buffer[20];

    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>
        test_subject_upstream{upstream_buffer, sizeof(cetl::pf17::byte) * 20, &mock_upstream, 0};

    ON_CALL(mock_upstream, do_allocate)
        .WillByDefault([&test_subject_upstream](std::size_t size_bytes, std::size_t alignment) {
            return test_subject_upstream.allocate(size_bytes, alignment);
        });
    ON_CALL(mock_upstream, do_reallocate)
        .WillByDefault([&test_subject_upstream](void*       p,
                                                std::size_t old_size_bytes,
                                                std::size_t new_size_bytes,
                                                std::size_t new_alignment) {
            return test_subject_upstream.reallocate(p, old_size_bytes, new_size_bytes, new_alignment);
        });
    ON_CALL(mock_upstream, do_deallocate)
        .WillByDefault([&test_subject_upstream](void* p, std::size_t size_bytes, std::size_t alignment) {
            return test_subject_upstream.deallocate(p, size_bytes, alignment);
        });

    EXPECT_CALL(mock_upstream, do_allocate(15, 2)).Times(1);
    EXPECT_CALL(mock_upstream, do_reallocate(upstream_buffer, 15, 20, 1)).Times(1);
    EXPECT_CALL(mock_upstream, do_deallocate(upstream_buffer, 20, ::testing::_)).Times(1);

    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>
          test_subject{buffer, sizeof(cetl::pf17::byte) * 10, &mock_upstream, 20};
    void* mem = test_subject.allocate(15, 2);
    ASSERT_NE(nullptr, mem);
    void* reallocated_mem = test_subject.reallocate(mem, 15, 20, 1);
    ASSERT_NE(nullptr, reallocated_mem);
    test_subject.deallocate(reallocated_mem, 20);
}

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestUpstreamSpilloverOnReallocateNoUpstreamRealloc)
{
    cetlvast::MockMemoryResource mock_upstream{};
    cetl::pf17::byte             buffer[10];
    cetl::pf17::byte             upstream_buffer[20];

    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetlvast::MockMemoryResource>
        test_subject_upstream{upstream_buffer, sizeof(cetl::pf17::byte) * 20, &mock_upstream, 0};

    ON_CALL(mock_upstream, allocate)
        .WillByDefault([&test_subject_upstream](std::size_t size_bytes, std::size_t alignment) {
            return test_subject_upstream.allocate(size_bytes, alignment);
        });
    ON_CALL(mock_upstream, deallocate)
        .WillByDefault([&test_subject_upstream](void* p, std::size_t size_bytes, std::size_t alignment) {
            return test_subject_upstream.deallocate(p, size_bytes, alignment);
        });

    EXPECT_CALL(mock_upstream, allocate(15, 2)).Times(1);
    EXPECT_CALL(mock_upstream, deallocate(upstream_buffer, 15, ::testing::_)).Times(1);

    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetlvast::MockMemoryResource>
          test_subject{buffer, sizeof(cetl::pf17::byte) * 10, &mock_upstream, 20};
    void* mem = test_subject.allocate(15, 2);
    ASSERT_NE(nullptr, mem);
    void* reallocated_mem = test_subject.reallocate(mem, 15, 20, 1);
    ASSERT_EQ(nullptr, reallocated_mem);
    test_subject.deallocate(mem, 15);
}

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestUpstreamSpillover)
{
    cetlvast::MockPf17MemoryResource mock_upstream{};
    cetl::pf17::byte                 buffer[10];
    cetl::pf17::byte                 upstream_buffer[20];
    Expectation allocate = EXPECT_CALL(mock_upstream, do_allocate(6, 1)).Times(1).WillOnce(Return(upstream_buffer));
    EXPECT_CALL(mock_upstream, do_deallocate(upstream_buffer, 6, testing::_)).Times(1).After(allocate);

    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>
        test_subject{buffer, sizeof(cetl::pf17::byte) * 10, &mock_upstream, sizeof(cetl::pf17::byte) * 20};

    void* mem = test_subject.allocate(5, 1);
    ASSERT_NE(nullptr, mem);
    void* upstream_mem = test_subject.allocate(6, 1);
    ASSERT_NE(nullptr, upstream_mem);
    ASSERT_NE(upstream_mem, mem);
    test_subject.deallocate(mem, 5);
    mem = nullptr;
    mem = test_subject.allocate(4, 1);
    ASSERT_NE(nullptr, mem);
    ASSERT_NE(upstream_mem, mem);
    test_subject.deallocate(upstream_mem, 6);
    test_subject.deallocate(mem, 4);
}

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestMaxSize)
{
    cetlvast::MockPf17MemoryResource mock_upstream{};
    cetl::pf17::byte                 buffer[1];
    constexpr std::size_t            max_size_max      = std::numeric_limits<std::size_t>::max();
    constexpr std::size_t            max_size_expected = std::numeric_limits<std::ptrdiff_t>::max();
    ASSERT_EQ(max_size_expected,
              cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>(buffer,
                                                                                                      1,
                                                                                                      &mock_upstream,
                                                                                                      max_size_max)
                  .max_size());
    ASSERT_EQ(max_size_expected,
              cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>(buffer,
                                                                                                      max_size_max,
                                                                                                      &mock_upstream,
                                                                                                      1)
                  .max_size());
    ASSERT_EQ(max_size_expected,
              cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>(buffer,
                                                                                                      max_size_max,
                                                                                                      &mock_upstream,
                                                                                                      max_size_max)
                  .max_size());
}

TEST(UnsynchronizedBufferMemoryResourceDelegateTest, TestAllocateAllocateDeallocate)
{
    cetlvast::MockPf17MemoryResource mock_upstream{};
    cetl::pf17::byte                 buffer[10];
    cetl::pf17::byte                 upstream_buffer[20];
    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>
        test_subject_upstream{upstream_buffer, sizeof(cetl::pf17::byte) * 20, &mock_upstream, 0};

    ON_CALL(mock_upstream, do_allocate)
        .WillByDefault([&test_subject_upstream](std::size_t size_bytes, std::size_t alignment) {
            return test_subject_upstream.allocate(size_bytes, alignment);
        });
    ON_CALL(mock_upstream, do_deallocate)
        .WillByDefault([&test_subject_upstream](void* p, std::size_t size_bytes, std::size_t alignment) {
            return test_subject_upstream.deallocate(p, size_bytes, alignment);
        });

    EXPECT_CALL(mock_upstream, do_allocate(20, ::testing::_)).Times(3);
    EXPECT_CALL(mock_upstream, do_deallocate(upstream_buffer, 20, ::testing::_)).Times(2);

    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>
          test_subject{buffer, sizeof(cetl::pf17::byte) * 10, &mock_upstream, test_subject_upstream.max_size()};
    void* internal = test_subject.allocate(10, 1);
    ASSERT_NE(nullptr, internal);
    ASSERT_EQ(buffer, internal);
    void* upstream = test_subject.allocate(20, 1);
    ASSERT_NE(nullptr, upstream);
    ASSERT_NE(upstream, internal);
    ASSERT_EQ(upstream, upstream_buffer);
#if defined(__cpp_exceptions)
    ASSERT_THROW(test_subject.allocate(20, 1), std::bad_alloc);
#else
    ASSERT_EQ(nullptr, test_subject.allocate(20, 1));
#endif
    test_subject.deallocate(upstream, 20, 1);
    upstream = nullptr;
    upstream = test_subject.allocate(20, 1);
    ASSERT_NE(nullptr, upstream);
    ASSERT_EQ(upstream, upstream_buffer);
    test_subject.deallocate(upstream, 20, 1);
    test_subject.deallocate(internal, 10, 1);
    internal = nullptr;
    internal = test_subject.allocate(10, 1);
    ASSERT_NE(nullptr, internal);
    ASSERT_EQ(buffer, internal);
}

// +----------------------------------------------------------------------+
// | ☠️ DEATH TESTS ☠️
// +----------------------------------------------------------------------+
#if CETL_ENABLE_DEBUG_ASSERT

static void TestNullBufferInCtor()
{
    flush_coverage_on_death();
    cetl::pf17::byte small_buffer[255];
    cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>
        test_subject{small_buffer, sizeof(cetl::pf17::byte) * 255, nullptr, 0};
}

TEST(DeathTestUnsynchronizedBufferMemoryResourceDelegateAssertions, TestNullBufferInCtor)
{
    EXPECT_DEATH(TestNullBufferInCtor(), "CDE_ubmrd_001");
}

#endif
