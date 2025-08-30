/// @file
/// Unit tests for cetl::pmr::O1heapResource
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest.hpp"  // NOLINT(clangd-unused-includes)

#include "cetl/pmr/o1heap_memory_resource_delegate.hpp"

#include <vector>
#include <new>  // for std::bad_alloc

namespace cetl
{
namespace test
{

class O1HeapMemoryResourceTest : public ::testing::Test
{
    static constexpr std::size_t TestBufferSize = 0x100000;

public:
    cetl::pmr::O1HeapAlignedStorage<TestBufferSize> large_buffer{};
};

TEST_F(O1HeapMemoryResourceTest, TestDefault)
{
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{large_buffer};
    void*                                                 mem = test_subject.allocate(8);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(mem, 8);
}

TEST_F(O1HeapMemoryResourceTest, O1HeapAlignedStorageTest)
{
    cetl::pmr::O1HeapAlignedStorage<4096>                 aligned_storage{};
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{aligned_storage};
    void*                                                 mem = test_subject.allocate(16);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(mem, 16);
}

TEST_F(O1HeapMemoryResourceTest, TestAllocationFailureThrowsBadAlloc)
{
    // Use a small buffer to easily exhaust memory
    cetl::pmr::O1HeapAlignedStorage<1024>                 small_buffer{};
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{small_buffer};

    // Allocate all available memory to force the next allocation to fail
    std::vector<void*> allocations;

#if defined(__cpp_exceptions)
    try
    {
        // Keep allocating until we exhaust the heap
        while (true)
        {
            void* mem = test_subject.allocate(64);  // Allocate in reasonable chunks
            allocations.push_back(mem);
        }
        // If we get here, allocation didn't fail as expected
        FAIL() << "Expected std::bad_alloc to be thrown when heap was exhausted";
    } catch (const std::bad_alloc&)
    {
        // This is expected when the heap is exhausted - this tests line 77
        SUCCEED() << "std::bad_alloc was thrown as expected when heap was exhausted";
    }
#else
    // When exceptions are disabled, allocate until we get nullptr
    void* mem = nullptr;
    do
    {
        mem = test_subject.allocate(64);
        if (mem != nullptr)
        {
            allocations.push_back(mem);
        }
    } while (mem != nullptr);

    // We should have gotten at least some allocations before failing
    EXPECT_GT(allocations.size(), 0u) << "Should have been able to allocate some memory before exhaustion";

    // Try one more allocation to ensure it returns nullptr
    void* final_alloc = test_subject.allocate(64);
    EXPECT_EQ(nullptr, final_alloc) << "Allocation should return nullptr when heap is exhausted";
#endif

    // Clean up allocations
    for (void* mem : allocations)
    {
        test_subject.deallocate(mem, 64);
    }
}

}  // namespace test
}  // namespace cetl

// +----------------------------------------------------------------------+
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

static void TestNullBufferInCtor()
{
    flush_coverage_on_death();
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{nullptr, 0xFFFFFFFF};
}

TEST(DeathTestUnsynchronizedO1HeapMemoryResourceAssertions, TestNullBufferInCtor)
{
    EXPECT_DEATH(TestNullBufferInCtor(), "CDE_o1h_001");
}

static void TestArenaSizeTooSmall()
{
    flush_coverage_on_death();
    std::array<unsigned char, 1> small_storage{};
    CETL_DEBUG_ASSERT(small_storage.size() < o1heapMinArenaSize, "Test setup error");
    cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate test_subject{small_storage.data(), small_storage.size()};
}

TEST(DeathTestUnsynchronizedO1HeapMemoryResourceAssertions, TestArenaSizeTooSmall)
{
    EXPECT_DEATH(TestArenaSizeTooSmall(), "CDE_o1h_002");
}
#endif
