/// @file
/// Unit tests for span.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest.hpp"

#if defined(__clang__)
#    pragma clang diagnostic push
#    pragma clang diagnostic ignored "-Wkeyword-macro"
#endif

#define private public
#define protected public
#include "cetl/variable_length_array.hpp"

#if defined(__clang__)
#    pragma clang diagnostic pop
#endif

namespace
{

// +----------------------------------------------------------------------+
// | DEBUG ASSERT TESTS
// +----------------------------------------------------------------------+
#if CETL_ENABLE_DEBUG_ASSERT

static void TestBoolSpecLastByteBitFillTooLarge()
{
    flush_coverage_on_death();
    cetl::VariableLengthArray<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    vla.push_back(true);
    if (vla.size() == 1)
    {
        vla.last_byte_bit_fill_ = 9;
        (void)vla.size();
    }
}

TEST(DeathTestVLAAssertions, TestBoolSpecLastByteBitFillTooLarge)
{
    EXPECT_DEATH(TestBoolSpecLastByteBitFillTooLarge(), "CDE_vla_001");
}


// +----------------------------------------------------------------------+


static void TestBoolSpecSizeGreaterThanCapacity()
{
    flush_coverage_on_death();
    cetl::VariableLengthArray<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    vla.push_back(true);
    if (vla.size() == 1)
    {
        vla.size_ = vla.capacity_ + 1;
        (void)vla.size();
    }
}

TEST(DeathTestVLAAssertions, TestBoolSpecSizeGreaterThanCapacity)
{
    EXPECT_DEATH(TestBoolSpecSizeGreaterThanCapacity(), "CDE_vla_002");
}


// +----------------------------------------------------------------------+


static void TestBoolSpecLastByteFillZeroWhenSizeIs()
{
    flush_coverage_on_death();
    cetl::VariableLengthArray<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    vla.push_back(true);
    if (vla.size() == 1)
    {
        vla.last_byte_bit_fill_ = 0;
        (void)vla.size();
    }
}

TEST(DeathTestVLAAssertions, TestBoolSpecLastByteFillZeroWhenSizeIs)
{
    EXPECT_DEATH(TestBoolSpecLastByteFillZeroWhenSizeIs(), "CDE_vla_003");
}


#endif // CETL_ENABLE_DEBUG_ASSERT
} // namespace
