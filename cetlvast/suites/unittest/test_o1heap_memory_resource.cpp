/// @file
/// Unit tests for cetl::pmr::O1heapResource
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include "cetl/pmr/o1heap_memory_resource.hpp"

constexpr std::size_t                                  TestBufferSize = 0x100000;
static cetl::pmr::O1HeapAlignedStorage<TestBufferSize> large_buffer{};

TEST(UnsynchronizedO1HeapMemoryResourceTest, TestDefault)
{
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{large_buffer};
    void*                                                 mem = test_subject.allocate(8);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(mem, 8);
}

TEST(UnsynchronizedO1HeapMemoryResourceTest, O1HeapAlignedStorageTest)
{
    cetl::pmr::O1HeapAlignedStorage<4096>                 aligned_storage{};
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{aligned_storage};
    void*                                                 mem = test_subject.allocate(16);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(mem, 16);
}

// +----------------------------------------------------------------------+

static void TestNullBufferInCtor()
{
    flush_coverage_on_death();
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{nullptr, 0};
}

TEST(DeathTestUnsynchronizedO1HeapMemoryResourceAssertions, TestNullBufferInCtor)
{
    EXPECT_DEATH(TestNullBufferInCtor(), "CDE_o1h_001");
}
