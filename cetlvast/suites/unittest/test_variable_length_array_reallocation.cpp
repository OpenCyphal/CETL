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
#include <map>
#include <set>
#include <type_traits>

namespace
{
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
        : value(42)
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
    int                   remaining   = -1;

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
        rhs.value = -1;
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
    template <typename T>
    void relocate(T& array)
    {
        if (GetParam())
        {
            array.shrink_to_fit();
        }
        else
        {
            array.reserve(128);
        }
    }

    template <typename T>
    void expect_elements(const T& array)
    {
        ASSERT_EQ(array.size(), 3U);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i].value, static_cast<int>(10 + i));
            EXPECT_EQ(array[i].self, &array[i].value);
        }
        EXPECT_EQ(array.capacity(), GetParam() ? 3U : 128U);
    }
};

// The minimal reproducer: byte relocation corrupts all three self pointers even though destruction is trivial.
TEST_P(VLAReallocation, TrivialDestructionDoesNotPermitByteRelocation)
{
    MovingResource       resource;
    Array<SelfReference> array{typename Array<SelfReference>::allocator_type{&resource}};
    array.reserve(16);
    for (int i = 10; i < 13; ++i)
    {
        array.emplace_back(i);
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
    array.reserve(16);
    for (int i = 10; i < 13; ++i)
    {
        array.emplace_back(state, i);
    }
    relocate(array);
    expect_elements(array);
    EXPECT_EQ(resource.reallocations, 0U);
    EXPECT_EQ(state.moves, 3U);
    EXPECT_EQ(state.constructed, 6U);
    EXPECT_EQ(state.destroyed, 3U);
    EXPECT_EQ(state.live.size(), 3U);
}

// Copyable elements with potentially throwing moves must preserve VLA's move_if_noexcept policy.
TEST_P(VLAReallocation, CopyFallbackUsesConstructors)
{
    MovingResource      resource;
    Lifetimes           state;
    Array<CopyFallback> array{typename Array<CopyFallback>::allocator_type{&resource}};
    array.reserve(16);
    for (int i = 10; i < 13; ++i)
    {
        array.emplace_back(state, i);
    }
    relocate(array);
    expect_elements(array);
    EXPECT_EQ(resource.reallocations, 0U);
    EXPECT_EQ(state.moves, 0U);
    EXPECT_EQ(state.constructed, 6U);
    EXPECT_EQ(state.destroyed, 3U);
}

// Trivial storage must still use successful moving reallocation, while a declined request must fall back safely.
TEST_P(VLAReallocation, TrivialStorageRetainsReallocationAndDeclineFallback)
{
    for (const bool decline : {false, true})
    {
        SCOPED_TRACE(decline);
        MovingResource resource;
        Array<int>     array{typename Array<int>::allocator_type{&resource}};
        array.reserve(16);
        for (int i = 10; i < 13; ++i)
        {
            array.push_back(i);
        }
        resource.decline = decline;
        const auto calls = resource.reallocations;
        relocate(array);
        EXPECT_EQ(resource.reallocations, calls + 1);
        ASSERT_EQ(array.size(), 3U);
        EXPECT_EQ(array.capacity(), GetParam() ? 3U : 128U);
        EXPECT_EQ(array[0], 10);
        EXPECT_EQ(array[1], 11);
        EXPECT_EQ(array[2], 12);
        EXPECT_EQ(resource.blocks.size(), 1U);
    }
}

// Eligibility depends on copying the representation, not on how default construction initializes it.
TEST_P(VLAReallocation, NonTrivialDefaultConstructionStillAllowsReallocation)
{
    MovingResource     resource;
    Array<TrivialCopy> array{typename Array<TrivialCopy>::allocator_type{&resource}};
    array.reserve(16);
    array.resize(3);
    const auto calls = resource.reallocations;
    relocate(array);
    EXPECT_EQ(resource.reallocations, calls + 1);
    ASSERT_EQ(array.size(), 3U);
    EXPECT_EQ(array.capacity(), GetParam() ? 3U : 128U);
    for (const auto& element : array)
    {
        EXPECT_EQ(element.value, 42);
    }
}

// Packed bool uses byte storage, so moving reallocation remains valid across partial-byte boundaries.
TEST_P(VLAReallocation, PackedBoolRetainsReallocation)
{
    MovingResource resource;
    Array<bool>    array{typename Array<bool>::allocator_type{&resource}};
    array.reserve(64);
    for (int i = 0; i < 17; ++i)
    {
        array.push_back(i % 3 == 0);
    }
    const auto calls = resource.reallocations;
    relocate(array);
    EXPECT_EQ(resource.reallocations, calls + 1);
    ASSERT_EQ(array.size(), 17U);
    EXPECT_EQ(array.capacity(), GetParam() ? 24U : 128U);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(array[i], i % 3 == 0);
    }
    EXPECT_EQ(resource.blocks.size(), 1U);
}

// Allocators without the extension must remain well-formed and continue to construct non-trivial elements.
TEST_P(VLAReallocation, StandardAllocatorNeedsNoReallocateMember)
{
    cetl::VariableLengthArray<SelfReference, std::allocator<SelfReference>> array{std::allocator<SelfReference>{}};
    array.reserve(16);
    for (int i = 10; i < 13; ++i)
    {
        array.emplace_back(i);
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
    array.reserve(16);
    for (int i = 10; i < 13; ++i)
    {
        array.emplace_back(i);
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
    EXPECT_EQ(array.capacity(), 16U);
    ASSERT_EQ(array.size(), 3U);
    EXPECT_EQ(resource.blocks.size(), 1U);
    EXPECT_EQ(resource.reallocations, 0U);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(array[i].value, static_cast<int>(10 + i));
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
    for (int fail_at = 0; fail_at < 3; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        MovingResource      resource;
        Lifetimes           state;
        Array<CopyFallback> array{typename Array<CopyFallback>::allocator_type{&resource}};
        array.reserve(16);
        for (int i = 10; i < 13; ++i)
        {
            array.emplace_back(state, i);
        }
        const auto old_data = array.data();
        state.remaining     = fail_at;
        EXPECT_THROW(relocate(array), ConstructionFailure);
        EXPECT_EQ(array.data(), old_data);
        EXPECT_EQ(array.capacity(), 16U);
        ASSERT_EQ(array.size(), 3U);
        EXPECT_EQ(resource.reallocations, 0U);
        EXPECT_EQ(resource.blocks.size(), 1U);
        EXPECT_EQ(state.constructed, 3U + static_cast<std::size_t>(fail_at));
        EXPECT_EQ(state.destroyed, static_cast<std::size_t>(fail_at));
        EXPECT_EQ(state.live.size(), 3U);
        EXPECT_EQ(state.moves, 0U);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i].value, static_cast<int>(10 + i));
            EXPECT_EQ(array[i].self, &array[i].value);
        }
        state.remaining = -1;
        relocate(array);
        expect_elements(array);
    }
}

// Without a copy alternative, a failed move can change source values but must retain valid lifetimes and storage.
// Clearing and repopulating after failure demonstrates that the surviving container remains reusable.
TEST_P(VLAReallocation, FailedMoveRelocationPreservesOwnership)
{
    for (int fail_at = 0; fail_at < 3; ++fail_at)
    {
        SCOPED_TRACE(fail_at);
        MovingResource  resource;
        Lifetimes       state;
        Array<MoveOnly> array{typename Array<MoveOnly>::allocator_type{&resource}};
        array.reserve(16);
        for (int i = 10; i < 13; ++i)
        {
            array.emplace_back(state, i);
        }
        const auto old_data = array.data();
        state.remaining     = fail_at;
        EXPECT_THROW(relocate(array), ConstructionFailure);
        EXPECT_EQ(array.data(), old_data);
        EXPECT_EQ(array.capacity(), 16U);
        ASSERT_EQ(array.size(), 3U);
        EXPECT_EQ(resource.reallocations, 0U);
        EXPECT_EQ(resource.blocks.size(), 1U);
        EXPECT_EQ(state.constructed, 3U + static_cast<std::size_t>(fail_at));
        EXPECT_EQ(state.destroyed, static_cast<std::size_t>(fail_at));
        EXPECT_EQ(state.live.size(), 3U);
        for (std::size_t i = 0; i < array.size(); ++i)
        {
            EXPECT_EQ(array[i].self, &array[i].value);
            EXPECT_EQ(state.live.count(&array[i]), 1U);
        }
        state.remaining = -1;
        array.clear();
        for (int i = 10; i < 13; ++i)
        {
            array.emplace_back(state, i);
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
    array.reserve(16);
    EXPECT_EQ(array.capacity(), 16U);
    const auto allocations = resource.allocations;
    array.reserve(16);
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
    array.reserve(1);
    array.emplace_back(10);
    array.emplace_back(11);
    array.resize(12, SelfReference{42});
    ASSERT_EQ(array.size(), 12U);
    EXPECT_EQ(resource.reallocations, 0U);
    for (std::size_t i = 0; i < array.size(); ++i)
    {
        EXPECT_EQ(array[i].value, i < 2 ? static_cast<int>(10 + i) : 42);
        EXPECT_EQ(array[i].self, &array[i].value);
    }
}
}  // namespace
