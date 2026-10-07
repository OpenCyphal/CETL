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
// Span multiple packed bytes and leave both a partial byte and spare source capacity.
constexpr std::size_t BitsPerByte    = 8;
constexpr std::size_t ElementCount   = BitsPerByte + 3;
constexpr std::size_t SourceCapacity = BitsPerByte * 8;
constexpr std::size_t SmallCapacity  = BitsPerByte;
static_assert(ElementCount > SmallCapacity && ElementCount < SourceCapacity, "Exercise both assignment branches.");
static_assert(ElementCount % BitsPerByte != 0, "Exercise partial-byte bookkeeping.");

// Tracks allocation ownership and sizes independently of the container, checking for leaks at scope exit.
struct AllocationState
{
    std::map<void*, std::size_t> allocations;
    std::size_t                  allocation_count   = 0;
    std::size_t                  deallocation_count = 0;

    AllocationState()                                  = default;
    AllocationState(const AllocationState&)            = delete;
    AllocationState(AllocationState&&)                 = delete;
    AllocationState& operator=(const AllocationState&) = delete;
    AllocationState& operator=(AllocationState&&)      = delete;

    ~AllocationState()
    {
        EXPECT_TRUE(allocations.empty());
        EXPECT_EQ(allocation_count, deallocation_count);
    }
};

// Unequal state identities prevent buffer transfer; allocation records detect ownership and size mismatches.
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

    TrackingAllocator(const TrackingAllocator&)            = default;
    TrackingAllocator(TrackingAllocator&&)                 = default;
    TrackingAllocator& operator=(const TrackingAllocator&) = default;
    TrackingAllocator& operator=(TrackingAllocator&&)      = default;
    ~TrackingAllocator()                                   = default;

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

// Run the same ownership and reuse contract against ordinary storage and packed bits.
template <typename T>
class VLAMoveCapacityTests : public ::testing::Test
{
protected:
    using Subject   = cetl::VariableLengthArray<T, TrackingAllocator<T>>;
    using Allocator = typename Subject::allocator_type;

    static std::size_t storage_bytes(std::size_t capacity)
    {
        return std::is_same<T, bool>::value ? (capacity + BitsPerByte - 1) / BitsPerByte : capacity * sizeof(T);
    }

    // Retention must preserve the original allocation and allocator, permit filling it without allocation,
    // and allow the caller to release it explicitly. Repeated shrinking must not deallocate twice.
    static void check_retained_source(Subject& source, AllocationState& state, void* allocation)
    {
        ASSERT_TRUE(source.empty());
        ASSERT_EQ(source.capacity(), SourceCapacity);
        EXPECT_EQ(source.get_allocator(), Allocator{state});
        ASSERT_EQ(state.allocations.size(), 1U);
        EXPECT_EQ(state.allocations.begin()->first, allocation);
        EXPECT_EQ(state.allocations.begin()->second, storage_bytes(SourceCapacity));
        EXPECT_EQ(state.deallocation_count, 0U);
        const auto allocation_count = state.allocation_count;
        for (std::size_t i = 0; i < SourceCapacity; ++i)
        {
            source.emplace_back(i % 2);
            ASSERT_EQ(source.size(), i + 1);
            EXPECT_EQ(source.back(), i % 2);
        }
        EXPECT_EQ(state.allocation_count, allocation_count);
        EXPECT_EQ(state.deallocation_count, 0U);
        source.clear();
        source.shrink_to_fit();
        EXPECT_EQ(source.capacity(), 0U);
        EXPECT_TRUE(state.allocations.empty());
        EXPECT_EQ(state.deallocation_count, 1U);
        source.shrink_to_fit();
        EXPECT_EQ(state.deallocation_count, 1U);
        EXPECT_EQ(state.allocation_count, allocation_count);
    }

    // Preserve payload and packed-bit size across the transfer, including the partial final byte.
    static void check_values(const Subject& destination)
    {
        ASSERT_EQ(destination.size(), ElementCount);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            EXPECT_EQ(destination[i], i % 2);
        }
    }
};

using ValueTypes = ::testing::Types<int, bool>;
TYPED_TEST_SUITE(VLAMoveCapacityTests, ValueTypes, );

// Relocation retains the source allocation while the destination acquires only enough storage for its elements.
TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorPreservesSourceCapacity)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    AllocationState source_state;
    AllocationState destination_state;
    Subject         source{Allocator{source_state}};
    source.reserve(SourceCapacity);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        source.emplace_back(i % 2);
    }
    void* const allocation = source_state.allocations.begin()->first;

    Subject destination{std::move(source), Allocator{destination_state}};
    TestFixture::check_retained_source(source, source_state, allocation);
    TestFixture::check_values(destination);
    ASSERT_EQ(destination_state.allocations.size(), 1U);
    ASSERT_EQ(destination.capacity(),
              (std::is_same<TypeParam, bool>::value ? ((ElementCount + BitsPerByte - 1) / BitsPerByte) * BitsPerByte
                                                    : ElementCount));
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
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        EXPECT_EQ(destination[i], i % 2);
    }
    EXPECT_EQ(destination_state.allocation_count, 2U);
    EXPECT_EQ(destination_state.deallocation_count, 1U);
    ASSERT_EQ(destination_state.allocations.size(), 1U);
    EXPECT_EQ(TestFixture::storage_bytes(destination.capacity()), destination_state.allocations.begin()->second);

    source.emplace_back(1);
    EXPECT_EQ(source.size(), 1U);
    EXPECT_EQ(source.back(), 1);
}

// An empty reserved source keeps its allocation; the destination needs no storage at all.
TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorMovesEmptyReservedSource)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    AllocationState source_state;
    AllocationState destination_state;
    Subject         source{Allocator{source_state}};
    source.reserve(SourceCapacity);
    void* const allocation = source_state.allocations.begin()->first;

    Subject destination{std::move(source), Allocator{destination_state}};
    TestFixture::check_retained_source(source, source_state, allocation);
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

// Equal allocators transfer the entire allocation; shrinking the empty source cannot release destination storage.
TYPED_TEST(VLAMoveCapacityTests, EqualAllocatorTransfersUnusedCapacity)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;

    AllocationState state;
    Subject         source{Allocator{state}};
    source.reserve(SourceCapacity);
    source.emplace_back(1);
    const auto  capacity   = source.capacity();
    void* const allocation = state.allocations.begin()->first;

    Subject destination{std::move(source), Allocator{state}};
    source.shrink_to_fit();
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

// Exercise reuse of a sufficiently large destination and replacement of an insufficient one.
// Destination reuse covers both assignment into existing elements and construction of a new suffix.
TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorAssignmentPreservesSourceCapacity)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;
    for (const std::size_t destination_size : {SmallCapacity, SourceCapacity})
    {
        for (const std::size_t destination_capacity : {SmallCapacity, SourceCapacity})
        {
            if (destination_size > destination_capacity)
            {
                continue;
            }
            SCOPED_TRACE(::testing::Message() << "size=" << destination_size << " capacity=" << destination_capacity);
            AllocationState source_state;
            AllocationState destination_state;
            Subject         source{Allocator{source_state}};
            Subject         destination{Allocator{destination_state}};
            source.reserve(SourceCapacity);
            for (std::size_t i = 0; i < ElementCount; ++i)
            {
                source.emplace_back(i % 2);
            }
            destination.reserve(destination_capacity);
            destination.resize(destination_size);
            void* const allocation = source_state.allocations.begin()->first;
            const bool  replaces   = destination_capacity < ElementCount;

            destination = std::move(source);

            TestFixture::check_retained_source(source, source_state, allocation);
            TestFixture::check_values(destination);
            EXPECT_EQ(destination.get_allocator(), Allocator{destination_state});
            EXPECT_EQ(destination_state.allocation_count, replaces ? 2U : 1U);
            EXPECT_EQ(destination_state.deallocation_count, replaces ? 1U : 0U);
            ASSERT_EQ(destination_state.allocations.size(), 1U);
            EXPECT_EQ(TestFixture::storage_bytes(destination.capacity()),
                      destination_state.allocations.begin()->second);
        }
    }
}

// Assigning an empty reserved source clears the destination's elements while both buffers stay available.
TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorAssignmentFromEmptyReservedSource)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;
    AllocationState source_state;
    AllocationState destination_state;
    Subject         source{Allocator{source_state}};
    Subject         destination{Allocator{destination_state}};
    source.reserve(SourceCapacity);
    destination.reserve(SmallCapacity);
    destination.emplace_back(1);
    void* const allocation = source_state.allocations.begin()->first;

    destination = std::move(source);

    TestFixture::check_retained_source(source, source_state, allocation);
    EXPECT_TRUE(destination.empty());
    EXPECT_EQ(destination.capacity(), SmallCapacity);
    EXPECT_EQ(destination_state.allocation_count, 1U);
    EXPECT_EQ(destination_state.deallocation_count, 0U);
    destination.shrink_to_fit();
    EXPECT_TRUE(destination_state.allocations.empty());
}

// Equal-allocator assignment releases the old destination and transfers source ownership without allocating.
TYPED_TEST(VLAMoveCapacityTests, EqualAllocatorAssignmentTransfersUnusedCapacity)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;
    AllocationState state;
    Subject         source{Allocator{state}};
    Subject         destination{Allocator{state}};
    source.reserve(SourceCapacity);
    source.emplace_back(1);
    void* const allocation = state.allocations.begin()->first;
    destination.reserve(SmallCapacity);
    destination.emplace_back(0);

    destination = std::move(source);
    source.shrink_to_fit();

    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.capacity(), 0U);
    ASSERT_EQ(destination.size(), 1U);
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination.capacity(), SourceCapacity);
    ASSERT_EQ(state.allocations.size(), 1U);
    EXPECT_EQ(state.allocations.begin()->first, allocation);
    EXPECT_EQ(state.allocation_count, 2U);
    EXPECT_EQ(state.deallocation_count, 1U);
}
}  // namespace
