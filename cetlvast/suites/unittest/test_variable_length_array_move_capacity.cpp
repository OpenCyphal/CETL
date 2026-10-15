/// @file
/// Tests allocation ownership and capacity when moving VLA with an allocator.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <cstddef>
#include <limits>
#include <map>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace
{
// Span multiple packed bytes and leave both a partial byte and spare source capacity.
constexpr std::size_t BitsPerByte = 8;
static_assert(std::numeric_limits<unsigned char>::digits == BitsPerByte, "VLA bool storage requires octets.");
constexpr std::size_t ElementCount   = BitsPerByte + 3;
constexpr std::size_t SourceCapacity = BitsPerByte * 8;
constexpr std::size_t SmallCapacity  = BitsPerByte;
static_assert(ElementCount > SmallCapacity && ElementCount < SourceCapacity, "Exercise both assignment branches.");
static_assert(ElementCount % BitsPerByte != 0, "Exercise partial-byte bookkeeping.");

// Tracks allocation ownership and sizes independently of the container, checking for leaks at scope exit.
struct AllocationState
{
    std::map<void*, std::size_t> allocations;
    std::size_t                  allocation_count    = 0;
    std::size_t                  deallocation_count  = 0;
    bool                         decline_allocations = false;

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
// Declined allocations return nullptr, which the container reports as std::bad_alloc when exceptions are enabled.
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
        if (state_->decline_allocations)
        {
            return nullptr;
        }
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
        return std::is_same<T, bool>::value ? cetlvast::align_size_up(capacity, BitsPerByte) / BitsPerByte
                                            : capacity * sizeof(T);
    }

    // Retention must preserve the original allocation and allocator, permit filling it without allocation,
    // and allow the caller to release it explicitly. Repeated shrinking must not deallocate twice. Releasing the
    // allocation through any allocator other than the source's fails in TrackingAllocator::deallocate.
    static void check_retained_source(Subject& source, AllocationState& state, void* allocation)
    {
        ASSERT_TRUE(source.empty());
        ASSERT_EQ(source.capacity(), SourceCapacity);
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

    // Write the payload that check_values expects, ending in a partial byte for packed bits.
    static void fill(Subject& array)
    {
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            array.emplace_back(i % 2);
        }
    }

    // Preserve payload and packed-bit size across the transfer, including the partial final byte. Elements appended
    // after the transfer may follow the payload, so the caller states the total size.
    static void check_values(const Subject& array, const std::size_t size = ElementCount)
    {
        ASSERT_EQ(array.size(), size);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            EXPECT_EQ(array[i], i % 2);
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
    TestFixture::fill(source);
    void* const allocation = source_state.allocations.begin()->first;

    Subject destination{std::move(source), Allocator{destination_state}};
    TestFixture::check_retained_source(source, source_state, allocation);
    TestFixture::check_values(destination);
    ASSERT_EQ(destination_state.allocations.size(), 1U);
    ASSERT_EQ(destination.capacity(),
              (std::is_same<TypeParam, bool>::value ? cetlvast::align_size_up(ElementCount, BitsPerByte)
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
    TestFixture::check_values(destination, capacity + 1U);
    EXPECT_EQ(destination.back(), 1);
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
    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.capacity(), 0U);
    ASSERT_EQ(destination.size(), 1U);
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination.capacity(), capacity);
    ASSERT_EQ(state.allocations.size(), 1U);
    EXPECT_EQ(state.allocations.begin()->first, allocation);
    EXPECT_EQ(state.allocation_count, 1U);
    EXPECT_EQ(state.deallocation_count, 0U);

    source.shrink_to_fit();
    EXPECT_EQ(source.capacity(), 0U);
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
    struct Shape
    {
        std::size_t size;
        std::size_t capacity;
    };
    // Each destination shape selects a different move_assign_from branch.
    for (const Shape shape : {
             Shape{SmallCapacity, SmallCapacity},    // Replace the insufficient buffer.
             Shape{SmallCapacity, SourceCapacity},   // Assign SmallCapacity elements, construct the rest.
             Shape{SourceCapacity, SourceCapacity},  // Assign ElementCount elements, destroy the rest.
         })
    {
        SCOPED_TRACE(::testing::Message() << "size=" << shape.size << " capacity=" << shape.capacity);
        AllocationState source_state;
        AllocationState destination_state;
        Subject         source{Allocator{source_state}};
        Subject         destination{Allocator{destination_state}};
        source.reserve(SourceCapacity);
        TestFixture::fill(source);
        destination.reserve(shape.capacity);
        destination.resize(shape.size);
        void* const allocation = source_state.allocations.begin()->first;
        const bool  replaces   = shape.capacity < ElementCount;

        destination = std::move(source);

        TestFixture::check_retained_source(source, source_state, allocation);
        TestFixture::check_values(destination);
        EXPECT_EQ(destination.get_allocator(), Allocator{destination_state});
        EXPECT_EQ(destination_state.allocation_count, replaces ? 2U : 1U);
        EXPECT_EQ(destination_state.deallocation_count, replaces ? 1U : 0U);
        ASSERT_EQ(destination_state.allocations.size(), 1U);
        EXPECT_EQ(TestFixture::storage_bytes(destination.capacity()), destination_state.allocations.begin()->second);
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

// Equal-allocator assignment releases the old destination and transfers source ownership without allocating;
// shrinking the empty source cannot release destination storage.
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

    EXPECT_TRUE(source.empty());
    EXPECT_EQ(source.capacity(), 0U);
    ASSERT_EQ(destination.size(), 1U);
    EXPECT_EQ(destination[0], 1);
    EXPECT_EQ(destination.capacity(), SourceCapacity);
    ASSERT_EQ(state.allocations.size(), 1U);
    EXPECT_EQ(state.allocations.begin()->first, allocation);
    EXPECT_EQ(state.allocation_count, 2U);
    EXPECT_EQ(state.deallocation_count, 1U);

    source.shrink_to_fit();
    EXPECT_EQ(source.capacity(), 0U);
    ASSERT_EQ(state.allocations.size(), 1U);
    EXPECT_EQ(state.allocations.begin()->first, allocation);
    EXPECT_EQ(state.allocation_count, 2U);
    EXPECT_EQ(state.deallocation_count, 1U);
}

// A declined destination allocation transfers nothing, so the source keeps its elements and storage. Exceptions
// report the failure; without them the empty destination and non-empty source report it. Either way a retry works.
TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorDeclinedConstructionLeavesSourceUnchanged)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;
    AllocationState source_state;
    AllocationState destination_state;
    Subject         source{Allocator{source_state}};
    source.reserve(SourceCapacity);
    TestFixture::fill(source);
    void* const allocation                = source_state.allocations.begin()->first;
    destination_state.decline_allocations = true;

#if defined(__cpp_exceptions)
    EXPECT_THROW((void) Subject(std::move(source), Allocator{destination_state}), std::bad_alloc);
#else
    {
        const Subject destination{std::move(source), Allocator{destination_state}};
        EXPECT_TRUE(destination.empty());
        EXPECT_EQ(destination.capacity(), 0U);
    }
#endif
    TestFixture::check_values(source);
    EXPECT_EQ(source.capacity(), SourceCapacity);
    ASSERT_EQ(source_state.allocations.size(), 1U);
    EXPECT_EQ(source_state.allocations.begin()->first, allocation);
    EXPECT_EQ(destination_state.allocation_count, 0U);

    destination_state.decline_allocations = false;
    const Subject destination{std::move(source), Allocator{destination_state}};
    TestFixture::check_values(destination);
    TestFixture::check_retained_source(source, source_state, allocation);
}

// Assignment releases an insufficient destination buffer before a declined replacement allocation (#225).
TYPED_TEST(VLAMoveCapacityTests, UnequalAllocatorDeclinedAssignmentLeavesSourceUnchanged)
{
    using Subject   = typename TestFixture::Subject;
    using Allocator = typename TestFixture::Allocator;
    AllocationState source_state;
    AllocationState destination_state;
    Subject         source{Allocator{source_state}};
    Subject         destination{Allocator{destination_state}};
    source.reserve(SourceCapacity);
    TestFixture::fill(source);
    destination.reserve(SmallCapacity);
    destination.emplace_back(1);
    void* const allocation                = source_state.allocations.begin()->first;
    destination_state.decline_allocations = true;

#if defined(__cpp_exceptions)
    EXPECT_THROW(destination = std::move(source), std::bad_alloc);
#else
    destination = std::move(source);
#endif
    EXPECT_TRUE(destination.empty());
    EXPECT_EQ(destination.capacity(), 0U);
    EXPECT_TRUE(destination_state.allocations.empty());
    TestFixture::check_values(source);
    EXPECT_EQ(source.capacity(), SourceCapacity);
    ASSERT_EQ(source_state.allocations.size(), 1U);
    EXPECT_EQ(source_state.allocations.begin()->first, allocation);

    destination_state.decline_allocations = false;
    destination                           = std::move(source);
    TestFixture::check_values(destination);
    TestFixture::check_retained_source(source, source_state, allocation);
}

// Element type without a default constructor; the live count shows that moved-from source elements are destroyed.
struct NonDefaultConstructible
{
    explicit NonDefaultConstructible(const int v) noexcept
        : value{v}
    {
        ++live;
    }
    NonDefaultConstructible(const NonDefaultConstructible& rhs) noexcept
        : value{rhs.value}
    {
        ++live;
    }
    NonDefaultConstructible& operator=(const NonDefaultConstructible&) = default;
    ~NonDefaultConstructible()
    {
        --live;
    }

    int                value;
    static std::size_t live;
};
std::size_t NonDefaultConstructible::live = 0;
static_assert(!std::is_default_constructible<NonDefaultConstructible>::value, "Exercise #223.");

// Move assignment without allocator propagation must not require default construction, whichever branch runs (#223).
TEST(VLAMoveCapacityNonDefaultConstructibleTest, Assignment)
{
    using Subject   = cetl::VariableLengthArray<NonDefaultConstructible, TrackingAllocator<NonDefaultConstructible>>;
    using Allocator = Subject::allocator_type;
    for (const bool equal : {true, false})
    {
        for (const std::size_t destination_capacity : {SmallCapacity, SourceCapacity})
        {
            SCOPED_TRACE(::testing::Message() << "equal=" << equal << " capacity=" << destination_capacity);
            AllocationState source_state;
            AllocationState other_state;
            {
                Subject source{Allocator{source_state}};
                Subject destination{Allocator{equal ? source_state : other_state}};
                source.reserve(SourceCapacity);
                for (std::size_t i = 0; i < ElementCount; ++i)
                {
                    source.emplace_back(static_cast<int>(i));
                }
                destination.reserve(destination_capacity);
                destination.emplace_back(-1);

                destination = std::move(source);

                EXPECT_TRUE(source.empty());
                EXPECT_EQ(source.capacity(), equal ? 0U : SourceCapacity);
                ASSERT_EQ(destination.size(), ElementCount);
                for (std::size_t i = 0; i < ElementCount; ++i)
                {
                    EXPECT_EQ(destination[i].value, static_cast<int>(i));
                }
                EXPECT_EQ(NonDefaultConstructible::live, ElementCount);
            }
            EXPECT_EQ(NonDefaultConstructible::live, 0U);
        }
    }
}
}  // namespace
