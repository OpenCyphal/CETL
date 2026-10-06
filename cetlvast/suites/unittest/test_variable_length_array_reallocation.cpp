/// @file
/// Tests that VLA only byte-relocates trivially copyable storage through allocator reallocate.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/memory_resource.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <type_traits>

namespace
{
// Scenario inputs. Counts, payloads, and sentinels are independent choices; the relationships checked below
// ensure that changing them preserves relocation, first/middle/final failure injection, and recognizable values.
constexpr std::size_t ElementCount        = 3;
constexpr std::size_t InitialCapacity     = 16;
constexpr std::size_t ReserveCapacity     = 128;
constexpr int         FirstElementValue   = 10;
constexpr int         FillValue           = 42;
constexpr int         MovedFromValue      = -1;
constexpr int         FailureDisabled     = -1;
constexpr std::size_t PackedTailBits      = 1;
constexpr std::size_t PackedPatternPeriod = 3;

// VLA packs eight bools per storage byte. Tie the packed scenario to ElementCount storage bytes, leaving a
// partial final byte and one spare byte before shrink. The shared reserve target must grow both array kinds.
constexpr std::size_t BitsPerByte = 8;
static_assert(std::numeric_limits<unsigned char>::digits == BitsPerByte, "VLA bool storage requires octets.");
static_assert(ElementCount >= 3, "Relocation needs first, middle, and final failure positions.");
static_assert(ElementCount < std::numeric_limits<std::size_t>::max() / BitsPerByte,
              "Packed capacities and the spare byte must not overflow.");
static_assert(PackedTailBits > 0 && PackedTailBits < BitsPerByte, "Packed storage needs a partial final byte.");
constexpr std::size_t PackedElementCount    = (ElementCount - 1) * BitsPerByte + PackedTailBits;
constexpr std::size_t PackedShrinkCapacity  = ElementCount * BitsPerByte;
constexpr std::size_t PackedInitialCapacity = PackedShrinkCapacity + BitsPerByte;
static_assert(ElementCount < InitialCapacity, "Ordinary elements need spare capacity before shrinking.");
static_assert(PackedElementCount < PackedShrinkCapacity && PackedShrinkCapacity < PackedInitialCapacity,
              "Packed shrink must retain a partial byte and release at least one storage byte.");
static_assert(ReserveCapacity > InitialCapacity && ReserveCapacity > PackedInitialCapacity,
              "Reserve must grow both ordinary and packed storage.");
static_assert(ReserveCapacity % BitsPerByte == 0, "The shared reserve target must occupy whole packed storage bytes.");
static_assert(PackedPatternPeriod > 1 && PackedPatternPeriod < BitsPerByte && BitsPerByte % PackedPatternPeriod != 0,
              "The packed pattern must mix true/false values and change phase across byte boundaries.");

// Fill an allocation exactly, then append one more element to force implicit growth. The later resize derives
// its target from the resulting runtime capacity, avoiding a dependency on VLA's geometric growth policy.
constexpr std::size_t AppendInitialCapacity = ElementCount;
constexpr std::size_t AppendedElementCount  = AppendInitialCapacity + 1;
constexpr std::size_t ResizeExtraElements   = ElementCount;
static_assert(ElementCount < static_cast<std::size_t>(std::numeric_limits<int>::max()),
              "Element indices and failure countdowns must fit in int.");
static_assert(FirstElementValue <= std::numeric_limits<int>::max() - static_cast<int>(AppendedElementCount - 1),
              "The generated payload sequence must not overflow int.");
constexpr int LastElementValue = FirstElementValue + static_cast<int>(AppendedElementCount - 1);
static_assert(FillValue < FirstElementValue || FillValue > LastElementValue,
              "Fill values must be distinguishable from the original element sequence.");
static_assert((MovedFromValue < FirstElementValue || MovedFromValue > LastElementValue) && MovedFromValue != FillValue,
              "Moved-from values must be distinguishable from original and fill values.");
static_assert(FailureDisabled < 0, "The disabled sentinel must not select a construction failure position.");

// Generates the recognizable payload sequence used for setup and verification, including the extra appended element.
constexpr int element_value(std::size_t index) noexcept
{
    return FirstElementValue + static_cast<int>(index);
}

// Repeats a mixed bit pattern whose phase changes at each byte boundary, exposing incorrect packed-byte copies.
constexpr bool packed_value(std::size_t index) noexcept
{
    return index % PackedPatternPeriod == 0;
}

// Makes successful reallocation deterministic: allocate a different block before releasing the original.
// Counters distinguish reallocation from VLA's allocate/construct/destroy fallback. Recorded sizes and alignments
// verify ownership on every release; failure and decline switches exercise both kinds of fallback failure.
class MovingResource final : public cetl::pf17::pmr::memory_resource
{
public:
    std::map<void*, std::pair<std::size_t, std::size_t>> blocks;
    std::size_t                                          allocations   = 0;
    std::size_t                                          deallocations = 0;
    std::size_t                                          reallocations = 0;
    bool                                                 decline       = false;
    bool                                                 fail          = false;

    ~MovingResource() override
    {
        EXPECT_TRUE(blocks.empty());
        EXPECT_EQ(allocations, deallocations);
    }

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        EXPECT_LE(alignment, alignof(std::max_align_t));
        void* const result = fail ? nullptr : std::malloc(bytes);
        if (result == nullptr)
        {
#if defined(__cpp_exceptions)
            throw std::bad_alloc{};
#else
            return nullptr;
#endif
        }
        EXPECT_TRUE(blocks.emplace(result, std::make_pair(bytes, alignment)).second);
        ++allocations;
        return result;
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override
    {
        if (pointer != nullptr)
        {
            const auto found = blocks.find(pointer);
            ASSERT_NE(found, blocks.end());
            EXPECT_EQ(found->second.first, bytes);
            EXPECT_EQ(found->second.second, alignment);
            blocks.erase(found);
            ++deallocations;
            std::free(pointer);
        }
    }

    void* do_reallocate(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) override
    {
        ++reallocations;
        // Declining reallocation leaves allocation failure to allocate(), isolating VLA's fallback contract.
        if (decline || fail || new_bytes == 0)
        {
            return nullptr;
        }
        void* const result = do_allocate(new_bytes, alignment);
        if (result != nullptr)
        {
            if (pointer != nullptr)
            {
                std::memcpy(result, pointer, std::min(old_bytes, new_bytes));
            }
            do_deallocate(pointer, old_bytes, alignment);
        }
        return result;
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return &rhs == this;
    }
};

// A byte copy leaves self pointing at the old allocation. Its trivial destructor specifically guards against
// mistakenly using is_trivially_destructible instead of is_trivially_copyable to select reallocate.
struct SelfReference
{
    int        value;
    const int* self;

    explicit SelfReference(int v) noexcept
        : value(v)
        , self(&value)
    {
    }
    SelfReference(const SelfReference& rhs) noexcept
        : SelfReference(rhs.value)
    {
    }
    SelfReference(SelfReference&& rhs) noexcept
        : SelfReference(rhs.value)
    {
    }
};
static_assert(std::is_trivially_destructible<SelfReference>::value, "Exercise trivial destruction.");
static_assert(!std::is_trivially_copyable<SelfReference>::value, "Require element-wise relocation.");

// A user-provided default constructor does not prevent safe byte relocation. This guards against an overly
// restrictive is_trivial or is_trivially_constructible gate that would unnecessarily disable the optimization.
struct TrivialCopy
{
    int value;
    TrivialCopy() noexcept
        : value(FillValue)
    {
    }
};
static_assert(std::is_trivially_copyable<TrivialCopy>::value, "Permit byte relocation.");
static_assert(!std::is_trivially_constructible<TrivialCopy>::value, "Exercise non-trivial default construction.");

// Distinguishes injected constructor failures from allocation failures, which shrink_to_fit suppresses.
struct ConstructionFailure
{};

// Records actual object addresses and exact lifetime counts, including any partially constructed replacement.
// A negative remaining count disables failure; zero fails the next construction before its lifetime begins.
struct Lifetimes
{
    std::set<const void*> live;
    std::size_t           constructed = 0;
    std::size_t           destroyed   = 0;
    std::size_t           moves       = 0;
    int                   remaining   = FailureDisabled;

    void construct(const void* address)
    {
#if defined(__cpp_exceptions)
        if (remaining == 0)
        {
            throw ConstructionFailure{};
        }
#endif
        if (remaining > 0)
        {
            --remaining;
        }
        EXPECT_TRUE(live.insert(address).second);
        ++constructed;
    }

    ~Lifetimes()
    {
        EXPECT_TRUE(live.empty());
        EXPECT_EQ(constructed, destroyed);
    }
};

// Move-only, address-sensitive element with a potentially throwing move. Successful moves change the source value
// so the failure tests can distinguish ownership guarantees from the stronger copy-fallback value guarantee.
struct MoveOnly
{
    Lifetimes& state;
    int        value;
    const int* self;

    MoveOnly(Lifetimes& s, int v)
        : state(s)
        , value(v)
        , self(&value)
    {
        state.construct(this);
    }
    MoveOnly(MoveOnly&& rhs)
        : MoveOnly(rhs.state, rhs.value)
    {
        ++state.moves;
        rhs.value = MovedFromValue;
    }
    ~MoveOnly()
    {
        EXPECT_EQ(state.live.erase(this), 1U);
        ++state.destroyed;
    }
};

// Allows move_if_noexcept to copy instead. Both copying and moving use the same injected construction failure.
struct CopyFallback : MoveOnly
{
    using MoveOnly::MoveOnly;
    CopyFallback(const CopyFallback& rhs)
        : MoveOnly(rhs.state, rhs.value)
    {
    }
    CopyFallback(CopyFallback&&) = default;
};
static_assert(!std::is_copy_constructible<MoveOnly>::value, "Exercise the move-only fallback.");
static_assert(!std::is_nothrow_move_constructible<CopyFallback>::value, "Exercise copying during relocation.");

// Explicit pf17 allocator keeps the reallocate-capable path under test in C++17 and newer too.
template <typename T>
using Array = cetl::VariableLengthArray<T, cetl::pf17::pmr::polymorphic_allocator<T>>;

// Every scenario runs once for reserve growth and once for shrink_to_fit; setup always leaves spare capacity.
class VLAReallocation : public ::testing::TestWithParam<bool>
{
protected:
    // Runs the capacity change selected by the test parameter: false grows the reservation to ReserveCapacity,
    // while true requests shrink-to-fit. Setup provides spare capacity below that target so either operation attempts
    // relocation. Exceptions propagate to the caller so failure tests can check the operation's contract.
    template <typename T>
    void relocate(T& array)
    {
        if (GetParam())
        {
            array.shrink_to_fit();
        }
        else
        {
            array.reserve(ReserveCapacity);
        }
    }

    // Checks successful relocation of ElementCount address-sensitive elements initialized by element_value().
    // Each self pointer must refer to its element's current value member, and capacity must match the requested
    // operation: ElementCount after shrinking or ReserveCapacity after reserving.
    template <typename T>
    void expect_elements(const T& array)
    {
        ASSERT_EQ(array.size(), ElementCount);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i].value, element_value(i));
            EXPECT_EQ(array[i].self, &array[i].value);
        }
        EXPECT_EQ(array.capacity(), GetParam() ? ElementCount : ReserveCapacity);
    }
};

// The minimal reproducer: byte relocation corrupts every self pointer even though destruction is trivial.
TEST_P(VLAReallocation, TrivialDestructionDoesNotPermitByteRelocation)
{
    MovingResource       resource;
    Array<SelfReference> array{typename Array<SelfReference>::allocator_type{&resource}};
    array.reserve(InitialCapacity);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        array.emplace_back(element_value(i));
    }
    const auto allocations = resource.allocations;
    relocate(array);
    expect_elements(array);
    EXPECT_EQ(resource.reallocations, 0U);
    EXPECT_EQ(resource.allocations, allocations + 1);
    EXPECT_EQ(resource.blocks.size(), 1U);
}

// A move-only element must be move-constructed, then destroyed at its old address exactly once.
TEST_P(VLAReallocation, MoveOnlyElementsUseConstructors)
{
    MovingResource  resource;
    Lifetimes       state;
    Array<MoveOnly> array{typename Array<MoveOnly>::allocator_type{&resource}};
    array.reserve(InitialCapacity);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        array.emplace_back(state, element_value(i));
    }
    relocate(array);
    expect_elements(array);
    EXPECT_EQ(resource.reallocations, 0U);
    EXPECT_EQ(state.moves, ElementCount);
    EXPECT_EQ(state.constructed, ElementCount + ElementCount);
    EXPECT_EQ(state.destroyed, ElementCount);
    EXPECT_EQ(state.live.size(), ElementCount);
}

// Copyable elements with potentially throwing moves must preserve VLA's move_if_noexcept policy.
TEST_P(VLAReallocation, CopyFallbackUsesConstructors)
{
    MovingResource      resource;
    Lifetimes           state;
    Array<CopyFallback> array{typename Array<CopyFallback>::allocator_type{&resource}};
    array.reserve(InitialCapacity);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        array.emplace_back(state, element_value(i));
    }
    relocate(array);
    expect_elements(array);
    EXPECT_EQ(resource.reallocations, 0U);
    EXPECT_EQ(state.moves, 0U);
    EXPECT_EQ(state.constructed, ElementCount + ElementCount);
    EXPECT_EQ(state.destroyed, ElementCount);
}

// Trivial storage must still use successful moving reallocation, while a declined request must fall back safely.
TEST_P(VLAReallocation, TrivialStorageRetainsReallocationAndDeclineFallback)
{
    for (const bool decline : {false, true})
    {
        SCOPED_TRACE(decline);
        MovingResource resource;
        Array<int>     array{typename Array<int>::allocator_type{&resource}};
        array.reserve(InitialCapacity);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            array.push_back(element_value(i));
        }
        resource.decline = decline;
        const auto calls = resource.reallocations;
        relocate(array);
        EXPECT_EQ(resource.reallocations, calls + 1);
        ASSERT_EQ(array.size(), ElementCount);
        EXPECT_EQ(array.capacity(), GetParam() ? ElementCount : ReserveCapacity);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i], element_value(i));
        }
        EXPECT_EQ(resource.blocks.size(), 1U);
    }
}

// Eligibility depends on copying the representation, not on how default construction initializes it.
TEST_P(VLAReallocation, NonTrivialDefaultConstructionStillAllowsReallocation)
{
    MovingResource     resource;
    Array<TrivialCopy> array{typename Array<TrivialCopy>::allocator_type{&resource}};
    array.reserve(InitialCapacity);
    array.resize(ElementCount);
    const auto calls = resource.reallocations;
    relocate(array);
    EXPECT_EQ(resource.reallocations, calls + 1);
    ASSERT_EQ(array.size(), ElementCount);
    EXPECT_EQ(array.capacity(), GetParam() ? ElementCount : ReserveCapacity);
    for (const auto& element : array)
    {
        EXPECT_EQ(element.value, FillValue);
    }
}

// Packed bool uses byte storage, so moving reallocation remains valid across partial-byte boundaries.
TEST_P(VLAReallocation, PackedBoolRetainsReallocation)
{
    MovingResource resource;
    Array<bool>    array{typename Array<bool>::allocator_type{&resource}};
    array.reserve(PackedInitialCapacity);
    for (std::size_t i = 0; i < PackedElementCount; ++i)
    {
        array.push_back(packed_value(i));
    }
    const auto calls = resource.reallocations;
    relocate(array);
    EXPECT_EQ(resource.reallocations, calls + 1);
    ASSERT_EQ(array.size(), PackedElementCount);
    EXPECT_EQ(array.capacity(), GetParam() ? PackedShrinkCapacity : ReserveCapacity);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(array[i], packed_value(i));
    }
    EXPECT_EQ(resource.blocks.size(), 1U);
}

// Allocators without the extension must remain well-formed and continue to construct non-trivial elements.
TEST_P(VLAReallocation, StandardAllocatorNeedsNoReallocateMember)
{
    cetl::VariableLengthArray<SelfReference, std::allocator<SelfReference>> array{std::allocator<SelfReference>{}};
    array.reserve(InitialCapacity);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        array.emplace_back(element_value(i));
    }
    relocate(array);
    expect_elements(array);
}

// Allocation failure must retain the original buffer and values; shrink suppresses bad_alloc, reserve propagates it.
// In embedded profiles both calls return with the unchanged container when the allocator returns nullptr.
TEST_P(VLAReallocation, AllocationFailurePreservesNonTrivialStorage)
{
    MovingResource       resource;
    Array<SelfReference> array{typename Array<SelfReference>::allocator_type{&resource}};
    array.reserve(InitialCapacity);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        array.emplace_back(element_value(i));
    }
    const auto old_data = array.data();
    resource.fail       = true;
#if defined(__cpp_exceptions)
    if (!GetParam())
    {
        EXPECT_THROW(relocate(array), std::bad_alloc);
    }
    else
#endif
    {
        relocate(array);
    }
    EXPECT_EQ(array.data(), old_data);
    EXPECT_EQ(array.capacity(), InitialCapacity);
    ASSERT_EQ(array.size(), ElementCount);
    EXPECT_EQ(resource.blocks.size(), 1U);
    EXPECT_EQ(resource.reallocations, 0U);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(array[i].value, element_value(i));
        EXPECT_EQ(array[i].self, &array[i].value);
    }
    resource.fail = false;
    relocate(array);
    expect_elements(array);
}

#if defined(__cpp_exceptions)
// Inject first/middle/final copy failures with a reallocate-capable allocator. Issue #37's rollback must release
// only the replacement allocation, destroy its completed prefix, and preserve every original value and address.
TEST_P(VLAReallocation, FailedCopyRelocationRollsBackReplacement)
{
    for (std::size_t fail_at = 0; fail_at < ElementCount; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        MovingResource      resource;
        Lifetimes           state;
        Array<CopyFallback> array{typename Array<CopyFallback>::allocator_type{&resource}};
        array.reserve(InitialCapacity);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            array.emplace_back(state, element_value(i));
        }
        const auto old_data = array.data();
        state.remaining     = static_cast<int>(fail_at);
        EXPECT_THROW(relocate(array), ConstructionFailure);
        EXPECT_EQ(array.data(), old_data);
        EXPECT_EQ(array.capacity(), InitialCapacity);
        ASSERT_EQ(array.size(), ElementCount);
        EXPECT_EQ(resource.reallocations, 0U);
        EXPECT_EQ(resource.blocks.size(), 1U);
        EXPECT_EQ(state.constructed, ElementCount + fail_at);
        EXPECT_EQ(state.destroyed, fail_at);
        EXPECT_EQ(state.live.size(), ElementCount);
        EXPECT_EQ(state.moves, 0U);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i].value, element_value(i));
            EXPECT_EQ(array[i].self, &array[i].value);
        }
        state.remaining = FailureDisabled;
        relocate(array);
        expect_elements(array);
    }
}

// Without a copy alternative, a failed move can change source values but must retain valid lifetimes and storage.
// Clearing and repopulating after failure demonstrates that the surviving container remains reusable.
TEST_P(VLAReallocation, FailedMoveRelocationPreservesOwnership)
{
    for (std::size_t fail_at = 0; fail_at < ElementCount; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        MovingResource  resource;
        Lifetimes       state;
        Array<MoveOnly> array{typename Array<MoveOnly>::allocator_type{&resource}};
        array.reserve(InitialCapacity);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            array.emplace_back(state, element_value(i));
        }
        const auto old_data = array.data();
        state.remaining     = static_cast<int>(fail_at);
        EXPECT_THROW(relocate(array), ConstructionFailure);
        EXPECT_EQ(array.data(), old_data);
        EXPECT_EQ(array.capacity(), InitialCapacity);
        ASSERT_EQ(array.size(), ElementCount);
        EXPECT_EQ(resource.reallocations, 0U);
        EXPECT_EQ(resource.blocks.size(), 1U);
        EXPECT_EQ(state.constructed, ElementCount + fail_at);
        EXPECT_EQ(state.destroyed, fail_at);
        EXPECT_EQ(state.live.size(), ElementCount);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i].self, &array[i].value);
            EXPECT_EQ(state.live.count(&array[i]), 1U);
        }
        state.remaining = FailureDisabled;
        array.clear();
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            array.emplace_back(state, element_value(i));
        }
        relocate(array);
        expect_elements(array);
    }
}
#endif

// Reserve and shrink share the same eligibility rule but have different allocation-failure contracts.
INSTANTIATE_TEST_SUITE_P(ReserveAndShrink, VLAReallocation, ::testing::Bool());

// Empty non-trivial storage follows the conservative type-level gate. No-op reserve and shrink-to-empty must
// avoid invoking constructors or reallocate, and shrinking must release the original allocation completely.
TEST(VLAReallocationEmpty, ReserveAndShrinkWithoutLiveElements)
{
    MovingResource       resource;
    Array<SelfReference> array{typename Array<SelfReference>::allocator_type{&resource}};
    array.reserve(InitialCapacity);
    EXPECT_EQ(array.capacity(), InitialCapacity);
    const auto allocations = resource.allocations;
    array.reserve(InitialCapacity);
    EXPECT_EQ(resource.allocations, allocations);
    array.shrink_to_fit();
    EXPECT_TRUE(array.empty());
    EXPECT_EQ(array.capacity(), 0U);
    EXPECT_TRUE(resource.blocks.empty());
    EXPECT_EQ(resource.reallocations, 0U);
}

// Implicit reserve from append and resize must obey the same rule as an explicit capacity change.
// Existing elements retain their self pointers, and new elements are constructed with the requested values.
TEST(VLAReallocationGrowth, AppendAndResizePreserveExistingElements)
{
    MovingResource       resource;
    Array<SelfReference> array{typename Array<SelfReference>::allocator_type{&resource}};
    array.reserve(AppendInitialCapacity);
    for (std::size_t i = 0; i < AppendedElementCount; ++i)
    {
        array.emplace_back(element_value(i));
    }
    ASSERT_GT(array.capacity(), AppendInitialCapacity);
    const auto append_capacity = array.capacity();
    ASSERT_LE(append_capacity, std::numeric_limits<std::size_t>::max() - ResizeExtraElements);
    const auto resized_count = append_capacity + ResizeExtraElements;
    array.resize(resized_count, SelfReference{FillValue});
    ASSERT_EQ(array.size(), resized_count);
    EXPECT_GT(array.capacity(), append_capacity);
    EXPECT_EQ(resource.reallocations, 0U);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(array[i].value, i < AppendedElementCount ? element_value(i) : FillValue);
        EXPECT_EQ(array[i].self, &array[i].value);
    }
}
}  // namespace
