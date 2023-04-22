/// @file
/// Unit tests for cetl::pmr::MemoryResourceManager
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers.hpp"

#include "cetl/pf17/cetlpf.hpp"

#include "cetl/pmr/memory_resource_manager.hpp"
#include "cetl/pmr/array_memory_resource.hpp"

#include <vector>

TEST(MemoryResourceManagerTest, TestDefault)
{
    cetl::byte buffer[256];
    cetl::pmr::UnsynchronizedArrayMemoryResource buffer_resource{buffer, 256};
    cetl::pmr::MemoryResourceManager test_subject{&buffer_resource};
    cetl::pmr::MemoryResourcePointer ptr = test_subject.raii_allocate(8);
    cetl::pmr::MemoryResourcePointer ptr_move_ctr{std::move(ptr)};
    ASSERT_TRUE(static_cast<bool>(ptr_move_ctr));
    ptr_move_ctr.reset();
    ASSERT_FALSE(static_cast<bool>(ptr_move_ctr));
}

TEST(MemoryResourceManagerTest, TestResourceContainer)
{
    cetl::pmr::MemoryResourceManager test_subject{cetl::pmr::new_delete_resource()};
    std::vector<cetl::pmr::MemoryResourcePointer> pointers{};
    for(std::size_t i = 0; i < 256; ++i)
    {
        pointers.push_back(test_subject.raii_allocate((i + 1) * 2));
    }
    ASSERT_EQ(256, pointers.size());
    std::vector<cetl::pmr::MemoryResourcePointer> move_to_pointers{std::move(pointers)};
    ASSERT_EQ(256, move_to_pointers.size());
}
