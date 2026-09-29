/// @file
/// Tests allocation ownership and capacity when moving VLA with an allocator.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <cstddef>
#include <map>
#include <memory>
#include <type_traits>
#include <utility>

namespace
{
struct AllocationState
{
    std::map<void*, std::size_t> allocations;
    std::size_t                  allocation_count   = 0;
    std::size_t                  deallocation_count = 0;

    ~AllocationState()
    {
        EXPECT_TRUE(allocations.empty());
        EXPECT_EQ(allocation_count, deallocation_count);
    }
};

template <typename T>
struct TrackingAllocator
{
    using value_type                             = T;
    using is_always_equal                        = std::false_type;
    using propagate_on_container_move_assignment = std::false_type;

    explicit TrackingAllocator(AllocationState& state) noexcept
        : state_(&state)
    {
    }

    template <typename U>
    TrackingAllocator(const TrackingAllocator<U>& rhs) noexcept
        : state_(rhs.state_)
    {
    }

    T* allocate(std::size_t count)
    {
        T* const pointer = std::allocator<T>{}.allocate(count);
        EXPECT_TRUE(state_->allocations.emplace(pointer, count * sizeof(T)).second);
        ++state_->allocation_count;
        return pointer;
    }

    void deallocate(T* pointer, std::size_t count) noexcept
    {
        if (pointer != nullptr)
        {
            const auto allocation = state_->allocations.find(pointer);
            ASSERT_NE(allocation, state_->allocations.end()) << "Deallocation used the wrong allocator.";
            EXPECT_EQ(count * sizeof(T), allocation->second);
            // Free using the recorded size even if the container supplied the wrong count.
            std::allocator<T>{}.deallocate(pointer, allocation->second / sizeof(T));
            state_->allocations.erase(allocation);
            ++state_->deallocation_count;
        }
    }

    template <typename U>
    bool operator==(const TrackingAllocator<U>& rhs) const noexcept
    {
        return state_ == rhs.state_;
    }

    template <typename U>
    bool operator!=(const TrackingAllocator<U>& rhs) const noexcept
    {
        return !(*this == rhs);
    }

    AllocationState* state_;
};

template <typename T>
class VLAMoveCapacityTests : public ::testing::Test
{
protected:
    using Subject   = cetl::VariableLengthArray<T, TrackingAllocator<T>>;
    using Allocator = typename Subject::allocator_type;

    static std::size_t storage_bytes(std::size_t capacity)
    {
        return std::is_same<T, bool>::value ? (capacity + 7U) / 8U : capacity * sizeof(T);
    }
};

using ValueTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLAMoveCapacityTests, ValueTypes, );

TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorDropsUnusedCapacity)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    AllocationState source_state;
    AllocationState destination_state;
    Subject         source{Allocator{source_state}};
    source.reserve(64);
    source.emplace_back(1);
    source.emplace_back(0);
    source.emplace_back(1);

    Subject destination{std::move(source), Allocator{destination_state}};
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.capacity(), 0U);
    EXPECT_TRUE(source_state.allocations.empty());
    ASSERT_EQ(destination.size(), 3U);
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination[1], 0);
    EXPECT_EQ(destination[2], 1);
    ASSERT_EQ(destination_state.allocations.size(), 1U);
    ASSERT_EQ(destination.capacity(), (std::is_same<TypeParam, bool>::value ? 8U : 3U));
    ASSERT_EQ(TestFixture::storage_bytes(destination.capacity()), destination_state.allocations.begin()->second);

    // Fill the actual allocation, including spare bits in the bool specialization.
    const auto capacity = destination.capacity();
    while (destination.size() < capacity)
    {
        destination.emplace_back(0);
    }
    EXPECT_EQ(destination_state.allocation_count, 1U);
    destination.emplace_back(1);
    ASSERT_EQ(destination.size(), capacity + 1U);
    EXPECT_EQ(destination.back(), 1);
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination[1], 0);
    EXPECT_EQ(destination[2], 1);
    EXPECT_EQ(destination_state.allocation_count, 2U);
    EXPECT_EQ(destination_state.deallocation_count, 1U);
    ASSERT_EQ(destination_state.allocations.size(), 1U);
    EXPECT_EQ(TestFixture::storage_bytes(destination.capacity()), destination_state.allocations.begin()->second);

    source.emplace_back(1);
    EXPECT_EQ(source.size(), 1U);
    EXPECT_EQ(source.back(), 1);
}

TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorMovesEmptyReservedSource)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    AllocationState source_state;
    AllocationState destination_state;
    Subject         source{Allocator{source_state}};
    source.reserve(64);

    Subject destination{std::move(source), Allocator{destination_state}};
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.capacity(), 0U);
    EXPECT_TRUE(source_state.allocations.empty());
    EXPECT_TRUE(destination.empty());
    ASSERT_EQ(destination.capacity(), 0U);
    EXPECT_EQ(destination_state.allocation_count, 0U);

    destination.emplace_back(1);
    EXPECT_EQ(destination.size(), 1U);
    EXPECT_EQ(destination.back(), 1);
    EXPECT_EQ(destination_state.allocation_count, 1U);
    ASSERT_EQ(destination_state.allocations.size(), 1U);
    EXPECT_EQ(TestFixture::storage_bytes(destination.capacity()), destination_state.allocations.begin()->second);
}

TYPED_TEST(VLAMoveCapacityTests, EqualAllocatorTransfersUnusedCapacity)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    AllocationState state;
    Subject         source{Allocator{state}};
    source.reserve(64);
    source.emplace_back(1);
    const auto  capacity   = source.capacity();
    void* const allocation = state.allocations.begin()->first;

    Subject destination{std::move(source), Allocator{state}};
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.capacity(), 0U);
    ASSERT_EQ(destination.size(), 1U);
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination.capacity(), capacity);
    ASSERT_EQ(state.allocations.size(), 1U);
    EXPECT_EQ(state.allocations.begin()->first, allocation);
    EXPECT_EQ(state.allocation_count, 1U);
    EXPECT_EQ(state.deallocation_count, 0U);

    destination.emplace_back(0);
    EXPECT_EQ(destination.size(), 2U);
    EXPECT_EQ(destination.back(), 0);
    EXPECT_EQ(state.allocation_count, 1U);
}
}  // namespace
