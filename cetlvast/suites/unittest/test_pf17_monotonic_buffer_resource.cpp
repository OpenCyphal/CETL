/// @file
/// Unit tests for cetl::pf17::pmr::monotonic_buffer_resource
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers.hpp"

#include "cetl/pf17/sys/memory_resource.hpp"

#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <memory_resource>
#endif

// +----------------------------------------------------------------------+
// | Test Suite :: TestMonotonicBufferResource
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
// clang-format on7

TYPED_TEST_SUITE(TestMonotonicBufferResource, MonotonicBufferResourceTypes, );

// +----------------------------------------------------------------------+

TYPED_TEST(TestMonotonicBufferResource, TestDefaultConstruction)
{
    TypeParam subject{};
    void*     memory = subject.allocate(1024);
    ASSERT_NE(nullptr, memory);
    subject.release();
}
