/// @file
/// Covers edge cases not covered by other tests for allocator-aware features
/// of cetl::VariableLengthArray.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"

#include "gtest/gtest.h"
#include "gmock/gmock.h"

#include <vector>
#include <string>

// +---------------------------------------------------------------------------+
// | TEST FIXTURES
// +---------------------------------------------------------------------------+

TEST(VLAAllocatorAwarenessTest, UsesPMAForItems)
{
    std::string::value_type buffer[100];
    cetl::pf17::pmr::monotonic_buffer_resource resource{buffer, sizeof(buffer)};
    cetl::VariableLengthArray<std::string, cetl::pf17::pmr::polymorphic_allocator<std::string::value_type>> vla{&resource};
    vla.reserve(3);
    vla.emplace_back("Hello");
    vla.emplace_back(" ");
    vla.emplace_back("World");

    ASSERT_EQ(3, vla.size());

    // Verify that the strings created within the array are using the buffer.
    for(const std::string& item : vla)
    {
        ASSERT_GE(item.c_str(), buffer);
        ASSERT_LT(item.c_str(), buffer + sizeof(buffer));
    }
}
