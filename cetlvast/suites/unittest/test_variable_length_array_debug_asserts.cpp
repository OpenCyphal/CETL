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
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

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
    if (vla.size() == 0)
    {
        vla.last_byte_bit_fill_ = 1;
        (void)vla.size();
    }
}

TEST(DeathTestVLAAssertions, TestBoolSpecLastByteFillZeroWhenSizeIs)
{
    EXPECT_DEATH(TestBoolSpecLastByteFillZeroWhenSizeIs(), "CDE_vla_003");
}

// +----------------------------------------------------------------------+


static void TestFrontOnEmpty()
{
    flush_coverage_on_death();
    cetl::VariableLengthArray<int, std::allocator<int> > vla{std::allocator<int>()};
    (void)vla.front();
}

TEST(DeathTestVLAAssertions, TestFrontOnEmpty)
{
    EXPECT_DEATH(TestFrontOnEmpty(), "CDE_vla_004");
}


// +----------------------------------------------------------------------+


static void TestConstFrontOnEmpty()
{
    flush_coverage_on_death();
    const cetl::VariableLengthArray<int, std::allocator<int> > vla{std::allocator<int>()};
    (void)reinterpret_cast<const decltype(vla)*>(&vla)->front();
}

TEST(DeathTestVLAAssertions, TestConstFrontOnEmpty)
{
    EXPECT_DEATH(TestConstFrontOnEmpty(), "CDE_vla_005");
}


// +----------------------------------------------------------------------+


static void TestFrontOnEmptyBool()
{
    flush_coverage_on_death();
    cetl::VariableLengthArray<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    (void)vla.front();
}

TEST(DeathTestVLAAssertions, TestFrontOnEmptyBool)
{
    EXPECT_DEATH(TestFrontOnEmptyBool(), "CDE_vla_006");
}


// +----------------------------------------------------------------------+


static void TestConstFrontOnEmptyBool()
{
    flush_coverage_on_death();
    const cetl::VariableLengthArray<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    (void)reinterpret_cast<const decltype(vla)*>(&vla)->front();
}

TEST(DeathTestVLAAssertions, TestConstFrontOnEmptyBool)
{
    EXPECT_DEATH(TestConstFrontOnEmptyBool(), "CDE_vla_007");
}


// +----------------------------------------------------------------------+


static void TestBackOnEmpty()
{
    flush_coverage_on_death();
    cetl::VariableLengthArray<int, std::allocator<int> > vla{std::allocator<int>()};
    (void)vla.back();
}

TEST(DeathTestVLAAssertions, TestBackOnEmpty)
{
    EXPECT_DEATH(TestBackOnEmpty(), "CDE_vla_004");
}


// +----------------------------------------------------------------------+


static void TestConstBackOnEmpty()
{
    flush_coverage_on_death();
    const cetl::VariableLengthArray<int, std::allocator<int> > vla{std::allocator<int>()};
    (void)reinterpret_cast<const decltype(vla)*>(&vla)->back();
}

TEST(DeathTestVLAAssertions, TestConstBackOnEmpty)
{
    EXPECT_DEATH(TestConstBackOnEmpty(), "CDE_vla_005");
}


// +----------------------------------------------------------------------+


static void TestBackOnEmptyBool()
{
    flush_coverage_on_death();
    cetl::VariableLengthArray<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    (void)vla.back();
}

TEST(DeathTestVLAAssertions, TestBackOnEmptyBool)
{
    EXPECT_DEATH(TestBackOnEmptyBool(), "CDE_vla_006");
}


// +----------------------------------------------------------------------+


static void TestConstBackOnEmptyBool()
{
    flush_coverage_on_death();
    const cetl::VariableLengthArray<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    (void)reinterpret_cast<const decltype(vla)*>(&vla)->back();
}

TEST(DeathTestVLAAssertions, TestConstBackOnEmptyBool)
{
    EXPECT_DEATH(TestConstBackOnEmptyBool(), "CDE_vla_007");
}

#endif // CETL_ENABLE_DEBUG_ASSERT
} // namespace
