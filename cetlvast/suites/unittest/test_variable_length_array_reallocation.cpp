/// @file
/// Tests that VLA only byte-relocates trivially copyable storage through allocator reallocate.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/memory_resource.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <type_traits>
#include <vector>

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
constexpr std::size_t PackedElementCount    = ((ElementCount - 1) * BitsPerByte) + PackedTailBits;
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

    MovingResource() = default;
    // Allocators retain this resource's address; its allocation ledger must never be copied or transferred.
    MovingResource(const MovingResource&)            = delete;
    MovingResource(MovingResource&&)                 = delete;
    MovingResource& operator=(const MovingResource&) = delete;
    MovingResource& operator=(MovingResource&&)      = delete;

    ~MovingResource() override
    {
        EXPECT_TRUE(blocks.empty());
        EXPECT_EQ(allocations, deallocations);
    }

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        EXPECT_LE(alignment, alignof(std::max_align_t));
        // NOLINTNEXTLINE(hicpp-no-malloc, cppcoreguidelines-no-malloc)
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
            // NOLINTNEXTLINE(hicpp-no-malloc, cppcoreguidelines-no-malloc, cppcoreguidelines-owning-memory)
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
    // These scenarios exercise construction, not assignment. Keep destruction trivial for the trait regression.
    SelfReference& operator=(const SelfReference&) = delete;
    SelfReference& operator=(SelfReference&&)      = delete;
    ~SelfReference()                               = default;
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

    Lifetimes() = default;
    // Elements retain this recorder by reference; its identity and lifetime counts must remain stable.
    Lifetimes(const Lifetimes&)            = delete;
    Lifetimes(Lifetimes&&)                 = delete;
    Lifetimes& operator=(const Lifetimes&) = delete;
    Lifetimes& operator=(Lifetimes&&)      = delete;

    void construct(const void* address)
    {
#if defined(__cpp_exceptions)
        if (remaining == 0)
        {
            // NOLINTNEXTLINE(hicpp-exception-baseclass)
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
    MoveOnly(const MoveOnly&) = delete;
    MoveOnly(MoveOnly&& rhs)
        : MoveOnly(rhs.state, rhs.value)
    {
        ++state.moves;
        rhs.value = MovedFromValue;
    }
    // Relocation constructs new objects. Assignment is intentionally unsupported, as it cannot rebind state.
    MoveOnly& operator=(const MoveOnly&) = delete;
    MoveOnly& operator=(MoveOnly&&)      = delete;
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
    // Copy construction enables move_if_noexcept's fallback; assignment remains disabled as in the base.
    CopyFallback& operator=(const CopyFallback&) = delete;
    CopyFallback& operator=(CopyFallback&&)      = delete;
    ~CopyFallback()                              = default;
};
static_assert(!std::is_copy_constructible<MoveOnly>::value, "Exercise the move-only fallback.");
static_assert(!std::is_nothrow_move_constructible<CopyFallback>::value, "Exercise copying during relocation.");

// Explicit pf17 allocator keeps the reallocate-capable path under test in C++17 and newer too.
template <typename T>
using Array = cetl::variable_length_array<T, cetl::pf17::pmr::polymorphic_allocator<T>>;

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
    cetl::variable_length_array<SelfReference, std::allocator<SelfReference>> array{std::allocator<SelfReference>{}};
    array.reserve(InitialCapacity);
    for (std::size_t i = 0; i < ElementCount; ++i)
    {
        array.emplace_back(element_value(i));
    }
    relocate(array);
    expect_elements(array);
}

// Allocation failure must retain the original buffer and values; debug builds diagnose a failed shrink.
// Without debug asserts, shrink suppresses bad_alloc; reserve propagates it when exceptions are enabled.
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
#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT
    if (GetParam())
    {
        EXPECT_DEATH(relocate(array), "CDE_vla_01[56]");
    }
    else
#endif
    {
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
        auto *const old_data = array.data();
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
        auto const* old_data = array.data();
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

int value_of(const int element) noexcept
{
    return element;
}

int value_of(const SelfReference& element) noexcept
{
    EXPECT_EQ(element.self, &element.value);
    return element.value;
}

// Trivially copyable but not movable, so relocating an appended element must not need a move constructor.
struct CopyOnly
{
    int value;

    explicit CopyOnly(int v) noexcept
        : value(v)
    {
    }
    CopyOnly(const CopyOnly&)            = default;
    CopyOnly(CopyOnly&&)                 = delete;
    CopyOnly& operator=(const CopyOnly&) = default;
    CopyOnly& operator=(CopyOnly&&)      = delete;
    ~CopyOnly()                          = default;
};
static_assert(!std::is_move_constructible<CopyOnly>::value, "Exercise growth without a move constructor.");

int value_of(const CopyOnly& element) noexcept
{
    return element.value;
}

// Appending an element of a full array must read it before the old storage is released (issue #245). Trivially
// copyable elements grow through reallocate, or allocate when it is declined; other elements always allocate.
template <typename T>
void expect_appended_elements_survive_growth(const bool decline)
{
    MovingResource resource;
    resource.decline = decline;
    Array<T> array{typename Array<T>::allocator_type{&resource}};
    array.reserve(AppendInitialCapacity);
    for (std::size_t i = 0; i < AppendInitialCapacity; ++i)
    {
        array.emplace_back(element_value(i));
    }
    std::vector<int> expected;
    for (const auto& element : array)
    {
        expected.push_back(value_of(element));
    }
    // Each round fills the spare capacity, then appends the first element, then the last, each time from full storage.
    for (std::size_t round = 0; round < 3; ++round)
    {
        for (const bool emplace : {false, true})
        {
            while (array.size() < array.capacity())
            {
                array.emplace_back(element_value(array.size()));
                expected.push_back(element_value(expected.size()));
            }
            const T* const data_before   = array.data();
            const auto     reallocations = resource.reallocations;
            if (emplace)
            {
                array.emplace_back(array.back());
                expected.push_back(expected.back());
            }
            else
            {
                array.push_back(static_cast<const T&>(array.front()));
                expected.push_back(expected.front());
            }
            EXPECT_NE(array.data(), data_before);
            // An argument that is an element is constructed from in new storage, never through reallocate.
            EXPECT_EQ(resource.reallocations, reallocations);
        }
    }
    ASSERT_EQ(array.size(), expected.size());
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(value_of(array[i]), expected[i]) << "index=" << i;
    }
    EXPECT_EQ(resource.blocks.size(), 1U);
}

TEST(VLAReallocationGrowth, AppendedElementsSurviveGrowth)
{
    for (const bool decline : {false, true})
    {
        SCOPED_TRACE(decline);
        expect_appended_elements_survive_growth<int>(decline);
        expect_appended_elements_survive_growth<CopyOnly>(decline);
        expect_appended_elements_survive_growth<SelfReference>(decline);
    }
}

// An allocator out of memory makes appending to a full array throw bad_alloc or, with exceptions disabled, return
// without effect. Either way the original storage and elements remain.
template <typename T>
void expect_failed_append_preserves_storage()
{
    MovingResource resource;
    Array<T>       array{typename Array<T>::allocator_type{&resource}};
    array.reserve(AppendInitialCapacity);
    for (std::size_t i = 0; i < AppendInitialCapacity; ++i)
    {
        array.emplace_back(element_value(i));
    }
    const T* const data_before = array.data();
    resource.fail              = true;
#if defined(__cpp_exceptions)
    EXPECT_THROW(array.push_back(static_cast<const T&>(array.front())), std::bad_alloc);
    EXPECT_THROW(array.emplace_back(array.back()), std::bad_alloc);
#else
    array.push_back(static_cast<const T&>(array.front()));
    array.emplace_back(array.back());
#endif
    EXPECT_EQ(array.data(), data_before);
    EXPECT_EQ(array.capacity(), AppendInitialCapacity);
    ASSERT_EQ(array.size(), AppendInitialCapacity);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(value_of(array[i]), element_value(i));
    }
    resource.fail = false;
    array.emplace_back(array.front());
    ASSERT_EQ(array.size(), AppendInitialCapacity + 1U);
    EXPECT_EQ(value_of(array.back()), element_value(0));
}

TEST(VLAReallocationGrowth, FailedAllocationOnAppendPreservesStorage)
{
    expect_failed_append_preserves_storage<int>();
    expect_failed_append_preserves_storage<CopyOnly>();
    expect_failed_append_preserves_storage<SelfReference>();
}

struct HookCounts
{
    std::size_t              constructs = 0;
    std::size_t              destroys   = 0;
    std::vector<const void*> addresses;
};

// A reallocate-capable allocator that counts its construct and destroy hooks.
template <typename T>
class HookCountingAllocator : public cetl::pf17::pmr::polymorphic_allocator<T>
{
public:
    using Base = cetl::pf17::pmr::polymorphic_allocator<T>;

    HookCountingAllocator(cetl::pf17::pmr::memory_resource* resource, HookCounts* counts) noexcept
        : Base(resource)
        , counts_(counts)
    {
    }

    template <typename U>
    HookCountingAllocator(const HookCountingAllocator<U>& other) noexcept  // NOLINT(*-explicit-constructor)
        : Base(other.resource())
        , counts_(other.counts())
    {
    }

    template <typename U, typename... Args>
    void construct(U* pointer, Args&&... args)
    {
        ++counts_->constructs;
        counts_->addresses.push_back(pointer);
        Base::construct(pointer, std::forward<Args>(args)...);
    }

    template <typename U>
    void destroy(U* pointer)
    {
        ++counts_->destroys;
        pointer->~U();
    }

    HookCounts* counts() const noexcept
    {
        return counts_;
    }

private:
    HookCounts* counts_;
};

// Each append constructs its element once, through the allocator, at its final address, whether or not the argument
// is an element that reallocate releases. A failed append constructs nothing.
TEST(VLAReallocationGrowth, AppendConstructsOnceInPlaceThroughAllocator)
{
    for (const bool decline : {false, true})
    {
        for (const bool aliased : {false, true})
        {
            SCOPED_TRACE(::testing::Message() << "decline=" << decline << " aliased=" << aliased);
            MovingResource resource;
            resource.decline = decline;
            HookCounts                                                 counts;
            cetl::variable_length_array<int, HookCountingAllocator<int>> array{
                HookCountingAllocator<int>{&resource, &counts}};
            array.reserve(AppendInitialCapacity);
            for (std::size_t i = 0; i < AppendInitialCapacity; ++i)
            {
                array.push_back(element_value(i));
            }
            ASSERT_EQ(counts.constructs, AppendInitialCapacity);
            const int  value         = aliased ? array.front() : element_value(AppendInitialCapacity);
            const auto reallocations = resource.reallocations;
            counts.addresses.clear();
            if (aliased)
            {
                array.push_back(static_cast<const int&>(array.front()));
            }
            else
            {
                array.push_back(value);
            }
            // An aliased argument is constructed from in new storage, never through reallocate. Without reallocate,
            // each old element is also relocated through the allocator.
            const bool relocates = aliased || decline;
            EXPECT_EQ(resource.reallocations, reallocations + (aliased ? 0U : 1U));
            EXPECT_EQ(counts.constructs, AppendInitialCapacity + 1U + (relocates ? AppendInitialCapacity : 0U));
            const void* const appended = &array.back();
            EXPECT_EQ(std::count(counts.addresses.begin(), counts.addresses.end(), appended), 1);
            EXPECT_EQ(counts.destroys, 0U);
            EXPECT_EQ(array.back(), value);

            while (array.size() < array.capacity())
            {
                array.push_back(element_value(array.size()));
            }
            const auto constructs = counts.constructs;
            const auto size       = array.size();
            resource.fail         = true;
#if defined(__cpp_exceptions)
            if (aliased)
            {
                EXPECT_THROW(array.emplace_back(array.back()), std::bad_alloc);
            }
            else
            {
                EXPECT_THROW(array.emplace_back(value), std::bad_alloc);
            }
#else
            if (aliased)
            {
                array.emplace_back(array.back());
            }
            else
            {
                array.emplace_back(value);
            }
#endif
            resource.fail = false;
            EXPECT_EQ(array.size(), size);
            EXPECT_EQ(counts.constructs, constructs);
            EXPECT_EQ(counts.destroys, 0U);
        }
    }
}

// std::decay drops volatile, but a volatile argument cannot be checked for aliasing through a const reference. It
// takes the general path, which constructs from it in new storage.
TEST(VLAReallocationGrowth, VolatileArgumentAppends)
{
    MovingResource resource;
    Array<int>     array{typename Array<int>::allocator_type{&resource}};
    array.reserve(AppendInitialCapacity);
    for (std::size_t i = 0; i < AppendInitialCapacity; ++i)
    {
        array.push_back(element_value(i));
    }
    volatile int value = FillValue;
    array.emplace_back(value);
    ASSERT_EQ(array.size(), AppendInitialCapacity + 1U);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(array[i], i < AppendInitialCapacity ? element_value(i) : FillValue);
    }
}

// Trivially copyable, but its converting constructor records where it ran, as a constructor that stores `this` does.
struct Located
{
    const void* where;
    int         value;

    explicit Located(int v) noexcept
        : where(this)
        , value(v)
    {
    }
};
static_assert(std::is_trivially_copyable<Located>::value, "Exercise the reallocate path.");

// Constructor arguments other than an element may refer anywhere, so the element is constructed in its new storage
// rather than in a temporary that is then moved there. An append without arguments needs no such care.
TEST(VLAReallocationGrowth, EmplaceConstructsInFinalStorage)
{
    MovingResource resource;
    Array<Located> array{typename Array<Located>::allocator_type{&resource}};
    for (std::size_t i = 0; i < AppendedElementCount * 4U; ++i)
    {
        const bool full = array.size() == array.capacity();
        array.emplace_back(element_value(i));
        ASSERT_EQ(array.size(), i + 1U);
        EXPECT_EQ(array.back().where, &array.back()) << "full=" << full;
        EXPECT_EQ(array.back().value, element_value(i));
    }

    Array<TrivialCopy> defaults{typename Array<TrivialCopy>::allocator_type{&resource}};
    defaults.reserve(AppendInitialCapacity);
    defaults.resize(AppendInitialCapacity);
    const auto reallocations = resource.reallocations;
    defaults.emplace_back();
    EXPECT_EQ(resource.reallocations, reallocations + 1U);
    ASSERT_EQ(defaults.size(), AppendInitialCapacity + 1U);
    for (const auto& element : defaults)
    {
        EXPECT_EQ(element.value, FillValue);
    }
}

#if defined(__cpp_exceptions)
// The appended element is constructed first and the old elements are then copied around it. A failure at any of
// those constructions must destroy what was built, release the replacement storage, and keep the original elements.
TEST(VLAReallocationGrowth, FailedAppendRollsBackReplacement)
{
    for (std::size_t fail_at = 0; fail_at <= ElementCount; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        MovingResource      resource;
        Lifetimes           state;
        Array<CopyFallback> array{typename Array<CopyFallback>::allocator_type{&resource}};
        array.reserve(ElementCount);
        for (std::size_t i = 0; i < ElementCount; ++i)
        {
            array.emplace_back(state, element_value(i));
        }
        const auto* const old_data = array.data();
        state.remaining            = static_cast<int>(fail_at);
        EXPECT_THROW(array.push_back(static_cast<const CopyFallback&>(array.front())), ConstructionFailure);
        EXPECT_EQ(array.data(), old_data);
        EXPECT_EQ(array.capacity(), ElementCount);
        ASSERT_EQ(array.size(), ElementCount);
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
        array.emplace_back(array.front());
        ASSERT_EQ(array.size(), ElementCount + 1U);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i].value, element_value(i < ElementCount ? i : 0U));
            EXPECT_EQ(array[i].self, &array[i].value);
        }
        EXPECT_EQ(resource.blocks.size(), 1U);
    }
}
#endif
}  // namespace
