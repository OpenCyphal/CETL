/// @file
/// Unit tests for variable_length_array debug assertions and shrink allocation failure.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetl/pf17/memory_resource.hpp"
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

// Reallocation is always declined so shrink must allocate a replacement. Failure is injected only after setup.
class ShrinkFailureResource final : public cetl::pf17::pmr::memory_resource
{
public:
    bool fail = false;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        EXPECT_LE(alignment, alignof(std::max_align_t));
        if (fail)
        {
#if defined(__cpp_exceptions)
            throw std::bad_alloc{};
#else
            return nullptr;
#endif
        }
        return std::allocator<unsigned char>{}.allocate(bytes);
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t) override
    {
        std::allocator<unsigned char>{}.deallocate(static_cast<unsigned char*>(pointer), bytes);
    }

    bool do_is_equal(const cetl::pf17::pmr::memory_resource& other) const noexcept override
    {
        return this == &other;
    }
};

template <typename T>
void TestShrinkAllocationFailure(const char* diagnostic)
{
    using Allocator = cetl::pf17::pmr::polymorphic_allocator<T>;
    ShrinkFailureResource                     resource;
    cetl::variable_length_array<T, Allocator> array{Allocator{&resource}};
    array.reserve(32);
    array.push_back(T{1});
    array.push_back(T{0});
    const auto old_capacity = array.capacity();
    const auto old_begin    = array.cbegin();
    resource.fail           = true;
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT
    EXPECT_DEATH(array.shrink_to_fit(), diagnostic);
#else
    (void) diagnostic;
    array.shrink_to_fit();
#endif
    EXPECT_EQ(array.cbegin(), old_begin);
    EXPECT_EQ(array.capacity(), old_capacity);
    ASSERT_EQ(array.size(), 2U);
    EXPECT_EQ(array[0], T{1});
    EXPECT_EQ(array[1], T{0});
    resource.fail = false;
    array.shrink_to_fit();
    EXPECT_LT(array.capacity(), old_capacity);
    ASSERT_EQ(array.size(), 2U);
    EXPECT_EQ(array[0], T{1});
    EXPECT_EQ(array[1], T{0});
}

// These tests also verify unchanged release behavior and a successful fallback after reallocate returns nullptr.
TEST(DeathTestVLAAssertions, TestShrinkAllocationFailure)
{
#if defined(__cpp_exceptions)
    TestShrinkAllocationFailure<int>("CDE_vla_016");
#else
    TestShrinkAllocationFailure<int>("CDE_vla_015");
#endif
}

TEST(DeathTestVLAAssertions, TestShrinkAllocationFailureBool)
{
#if defined(__cpp_exceptions)
    TestShrinkAllocationFailure<bool>("CDE_vla_017");
#else
    TestShrinkAllocationFailure<bool>("CDE_vla_015");
#endif
}

// +----------------------------------------------------------------------+
// | DEBUG ASSERT TESTS
// +----------------------------------------------------------------------+
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT

static void TestBoolSpecLastByteBitFillTooLarge()
{
    flush_coverage_on_death();
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
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
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
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
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
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
    cetl::variable_length_array<int, std::allocator<int> > vla{std::allocator<int>()};
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
    const cetl::variable_length_array<int, std::allocator<int> > vla{std::allocator<int>()};
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
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
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
    const cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
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
    cetl::variable_length_array<int, std::allocator<int> > vla{std::allocator<int>()};
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
    const cetl::variable_length_array<int, std::allocator<int> > vla{std::allocator<int>()};
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
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
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
    const cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    (void)reinterpret_cast<const decltype(vla)*>(&vla)->back();
}

TEST(DeathTestVLAAssertions, TestConstBackOnEmptyBool)
{
    EXPECT_DEATH(TestConstBackOnEmptyBool(), "CDE_vla_007");
}


// +----------------------------------------------------------------------+


static void TestEraseEnd()
{
    flush_coverage_on_death();
    cetl::variable_length_array<int, std::allocator<int> > vla{std::allocator<int>()};
    vla.push_back(1);
    (void)vla.erase(vla.cend());
}

TEST(DeathTestVLAAssertions, TestEraseEnd)
{
    EXPECT_DEATH(TestEraseEnd(), "CDE_vla_009");
}


// +----------------------------------------------------------------------+


static void TestEraseReversedRange()
{
    flush_coverage_on_death();
    cetl::variable_length_array<int, std::allocator<int> > vla{std::allocator<int>()};
    vla.push_back(1);
    vla.push_back(2);
    (void)vla.erase(vla.cbegin() + 1, vla.cbegin());
}

TEST(DeathTestVLAAssertions, TestEraseReversedRange)
{
    EXPECT_DEATH(TestEraseReversedRange(), "CDE_vla_010");
}


// +----------------------------------------------------------------------+


static void TestEraseEndBool()
{
    flush_coverage_on_death();
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    vla.push_back(true);
    (void)vla.erase(vla.cend());
}

TEST(DeathTestVLAAssertions, TestEraseEndBool)
{
    EXPECT_DEATH(TestEraseEndBool(), "CDE_vla_011");
}


// +----------------------------------------------------------------------+


static void TestEraseOtherArrayRangeBool()
{
    flush_coverage_on_death();
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    cetl::variable_length_array<bool, std::allocator<bool> > other{std::allocator<bool>()};
    vla.push_back(true);
    other.push_back(true);
    (void)vla.erase(other.cbegin(), other.cend());
}

TEST(DeathTestVLAAssertions, TestEraseOtherArrayRangeBool)
{
    EXPECT_DEATH(TestEraseOtherArrayRangeBool(), "CDE_vla_012");
}


// +----------------------------------------------------------------------+


static void TestInsertPastEnd()
{
    flush_coverage_on_death();
    cetl::variable_length_array<int, std::allocator<int> > vla{std::allocator<int>()};
    vla.reserve(4);
    vla.push_back(1);
    (void)vla.insert(vla.cend() + 1, 2);
}

TEST(DeathTestVLAAssertions, TestInsertPastEnd)
{
    EXPECT_DEATH(TestInsertPastEnd(), "CDE_vla_013");
}


// +----------------------------------------------------------------------+


static void TestInsertOtherArrayBool()
{
    flush_coverage_on_death();
    cetl::variable_length_array<bool, std::allocator<bool> > vla{std::allocator<bool>()};
    cetl::variable_length_array<bool, std::allocator<bool> > other{std::allocator<bool>()};
    other.push_back(true);
    (void)vla.insert(other.cbegin(), true);
}

TEST(DeathTestVLAAssertions, TestInsertOtherArrayBool)
{
    EXPECT_DEATH(TestInsertOtherArrayBool(), "CDE_vla_014");
}

#endif // CETL_ENABLE_DEBUG_ASSERT
} // namespace
