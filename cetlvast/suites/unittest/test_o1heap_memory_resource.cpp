/// @file
/// Unit tests for cetl::pmr::O1heapResrouce
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers.hpp"

#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <memory_resource>
#    include <cstddef>
namespace cetl
{
using byte = ::std::byte;
namespace pmr
{
using memory_resource = ::std::pmr::memory_resource;
}  // namespace pmr
}  // namespace cetl
#else
#    include "cetl/pf17/memory_resource.hpp"
#    include "cetl/pf17/byte.hpp"
namespace cetl
{
using byte = ::cetl::pf17::byte;
namespace pmr
{
using memory_resource = ::cetl::pf17::pmr::memory_resource;
}  // namespace pmr
}  // namespace cetl
#endif

#include "cetl/pmr/o1heap_memory_resource.hpp"

constexpr std::size_t                                  TestBufferSize = 0x100000;
static cetl::pmr::O1HeapAlignedStorage<TestBufferSize> large_buffer{};

TEST(UnsynchronizedO1HeapMemoryResourceTest, TestDefault)
{
    cetl::pmr::UnsynchronizedO1HeapMemoryResource test_subject{large_buffer};
    void*                                         mem = test_subject.allocate(8);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(mem, 8);
}

TEST(UnsynchronizedO1HeapMemoryResourceTest, O1HeapAlignedStorageTest)
{
    cetl::pmr::O1HeapAlignedStorage<4096>         aligned_storage{};
    cetl::pmr::UnsynchronizedO1HeapMemoryResource test_subject{aligned_storage};
    void*                                         mem = test_subject.allocate(16);
    ASSERT_NE(nullptr, mem);
    test_subject.deallocate(mem, 16);
}
