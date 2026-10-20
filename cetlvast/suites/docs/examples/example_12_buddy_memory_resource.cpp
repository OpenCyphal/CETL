/// @file
/// Buddy allocation with vector-style operations and no payload copying during in-place growth.
/// This teaching resource uses the existing reallocate extension for trivially copyable data.
/// Copy assignment can reuse the same mechanism when its destination needs more capacity.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include "cetl/pf17/memory_resource.hpp"
#include "cetl/variable_length_array.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

#include <gtest/gtest.h>

namespace
{
// A deliberately small, unsynchronized binary buddy arena. Bookkeeping is separate from payload
// storage, so splitting/merging cannot overwrite an element. The resource must outlive its users.
// Allocation searches a fixed tree; this example is not a constant-time production allocator.
class buddy_memory_resource final : public cetl::pf17::pmr::memory_resource
{
public:
    enum : std::size_t
    {
        arena_bytes         = 1024,
        minimum_block_bytes = 16,
        leaf_count          = arena_bytes / minimum_block_bytes
    };

    struct statistics
    {
        std::size_t allocations              = 0;
        std::size_t deallocations            = 0;
        std::size_t reallocations            = 0;
        std::size_t successful_reallocations = 0;
        std::size_t growth_merges            = 0;
        std::size_t reserved_bytes           = 0;
        std::size_t peak_reserved_bytes      = 0;
    };

    buddy_memory_resource()                                        = default;
    buddy_memory_resource(const buddy_memory_resource&)            = delete;
    buddy_memory_resource& operator=(const buddy_memory_resource&) = delete;

    const statistics& stats() const noexcept
    {
        return stats_;
    }

private:
    enum class state : unsigned char
    {
        free,
        split,
        allocated
    };
    static constexpr std::size_t no_node = std::numeric_limits<std::size_t>::max();

    static std::size_t block_size(std::size_t bytes, std::size_t alignment) noexcept
    {
        if (bytes == 0 || bytes > arena_bytes || alignment == 0 || (alignment & (alignment - 1U)) != 0 ||
            alignment > alignof(std::max_align_t))
        {
            return 0;
        }
        std::size_t result = minimum_block_bytes;
        while (result < std::max(bytes, alignment))
        {
            result *= 2U;
        }
        return result;
    }

    // Nodes at one level correspond to consecutive blocks of the same size.
    static std::size_t node_at(std::size_t offset, std::size_t bytes) noexcept
    {
        return (arena_bytes / bytes - 1U) + offset / bytes;
    }

    std::size_t take(std::size_t node, std::size_t bytes, std::size_t wanted) noexcept
    {
        if (nodes_[node] == state::allocated || bytes < wanted)
        {
            return no_node;
        }
        if (bytes == wanted)
        {
            if (nodes_[node] != state::free)
            {
                return no_node;
            }
            nodes_[node] = state::allocated;
            return node;
        }
        if (nodes_[node] == state::free)
        {
            nodes_[node]           = state::split;
            nodes_[2U * node + 1U] = state::free;
            nodes_[2U * node + 2U] = state::free;
        }
        const auto left = take(2U * node + 1U, bytes / 2U, wanted);
        return left != no_node ? left : take(2U * node + 2U, bytes / 2U, wanted);
    }

    void add_reserved(std::size_t bytes) noexcept
    {
        stats_.reserved_bytes += bytes;
        stats_.peak_reserved_bytes = std::max(stats_.peak_reserved_bytes, stats_.reserved_bytes);
    }

    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        const auto block = block_size(bytes, alignment);
        const auto node  = block == 0 ? no_node : take(0, arena_bytes, block);
        if (node == no_node)
        {
#if defined(__cpp_exceptions)
            throw std::bad_alloc();
#else
            return nullptr;
#endif
        }
        ++stats_.allocations;
        add_reserved(block);
        return arena_.data() + (node - (arena_bytes / block - 1U)) * block;
    }

    void do_deallocate(void* pointer, std::size_t bytes, std::size_t alignment) override
    {
        if (pointer == nullptr)
        {
            return;
        }
        const auto block = block_size(bytes, alignment);
        // As with memory_resource, the pointer, byte count, and alignment must match the live allocation.
        const auto offset = static_cast<std::size_t>(static_cast<unsigned char*>(pointer) - arena_.data());
        auto       node   = node_at(offset, block);
        nodes_[node]      = state::free;
        while (node != 0)
        {
            const auto parent = (node - 1U) / 2U;
            if (nodes_[2U * parent + 1U] != state::free || nodes_[2U * parent + 2U] != state::free)
            {
                break;
            }
            nodes_[parent] = state::free;
            node           = parent;
        }
        ++stats_.deallocations;
        stats_.reserved_bytes -= block;
    }

    void* do_reallocate(void* pointer, std::size_t old_bytes, std::size_t new_bytes, std::size_t alignment) override
    {
        ++stats_.reallocations;
        const auto wanted = block_size(new_bytes, alignment);
        if (pointer == nullptr || wanted == 0)
        {
            return nullptr;  // Decline; the old allocation, if any, remains owned by the caller.
        }
        const auto old_block = block_size(old_bytes, alignment);
        const auto offset    = static_cast<std::size_t>(static_cast<unsigned char*>(pointer) - arena_.data());
        auto       node      = node_at(offset, old_block);

        // Preflight every merge before changing anything. A right child would move the allocation's
        // beginning; an occupied buddy prevents merging. No allocate/memcpy/free fallback occurs here.
        auto candidate = node;
        for (auto bytes = old_block; bytes < wanted; bytes *= 2U)
        {
            if (candidate == 0 || candidate % 2U == 0 || nodes_[candidate + 1U] != state::free)
            {
                return nullptr;
            }
            candidate = (candidate - 1U) / 2U;
        }
        auto current = old_block;
        while (current < wanted)
        {
            nodes_[node] = state::free;
            node         = (node - 1U) / 2U;
            nodes_[node] = state::allocated;
            current *= 2U;
            ++stats_.growth_merges;
        }
        while (current > wanted)
        {
            nodes_[node]           = state::split;
            nodes_[2U * node + 2U] = state::free;
            node                   = 2U * node + 1U;
            nodes_[node]           = state::allocated;
            current /= 2U;
        }
        stats_.reserved_bytes -= old_block;
        add_reserved(wanted);
        ++stats_.successful_reallocations;
        return pointer;
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return this == &rhs;
    }
    std::size_t do_max_size() const noexcept override
    {
        return arena_bytes;
    }

    alignas(std::max_align_t) std::array<unsigned char, arena_bytes> arena_{};
    std::array<state, 2U * leaf_count - 1U> nodes_{};
    statistics                              stats_{};
};

using allocator = cetl::pf17::pmr::polymorphic_allocator<char>;
using array     = cetl::variable_length_array<char, allocator>;

// Identical application code for std::vector and CETL. Explicit reservations make the comparison
// independent of their geometric growth factors. Every byte already stored is checked after growth.
template <typename Container>
void fill_in_stages(Container& values)
{
    for (std::size_t target = 16; target <= 256; target *= 2U)
    {
        values.reserve(target);
        for (char value : values)
        {
            EXPECT_EQ(value, 'x');
        }
        while (values.size() < target)
        {
            values.push_back('x');
        }
    }
}

TEST(example_12_buddy_memory_resource, same_operations_as_vector)
{
    //! [buddy_vector_comparison]
    buddy_memory_resource cetl_resource;
    buddy_memory_resource vector_resource;
    {
        array                        values{allocator{&cetl_resource}};
        std::vector<char, allocator> standard_values{allocator{&vector_resource}};
        fill_in_stages(values);
        fill_in_stages(standard_values);
        EXPECT_TRUE(std::equal(values.begin(), values.end(), standard_values.begin()));

        // CETL grows the first 16-byte block through 32, 64, 128, and 256 bytes by merging buddies.
        // The resource never copies payload bytes. std::vector never calls the reallocate extension.
        EXPECT_EQ(cetl_resource.stats().allocations, 1U);
        EXPECT_EQ(cetl_resource.stats().deallocations, 0U);
        EXPECT_EQ(cetl_resource.stats().growth_merges, 4U);
        EXPECT_EQ(cetl_resource.stats().peak_reserved_bytes, 256U);
        EXPECT_EQ(vector_resource.stats().reallocations, 0U);
        // A vector implementation may reserve more than requested, even skipping subsequent growth stages.
        // Report its measured cost instead of prescribing its capacities. libc++ uses 5 allocations / 384 bytes.
        EXPECT_GE(vector_resource.stats().allocations, 1U);
        EXPECT_GE(vector_resource.stats().peak_reserved_bytes, 256U);
        std::cout << "CETL/vector successful allocations: " << cetl_resource.stats().allocations << "/"
                  << vector_resource.stats().allocations
                  << "; peak arena bytes: " << cetl_resource.stats().peak_reserved_bytes << "/"
                  << vector_resource.stats().peak_reserved_bytes << '\n';
    }
    EXPECT_EQ(cetl_resource.stats().reserved_bytes, 0U);
    EXPECT_EQ(vector_resource.stats().reserved_bytes, 0U);
    //! [buddy_vector_comparison]
}

TEST(example_12_buddy_memory_resource, push_back_grows_without_payload_relocation)
{
    buddy_memory_resource resource;
    array                 values{allocator{&resource}};
    values.push_back('x');
    // Retain an integer address rather than use a pointer invalidated by the reallocate contract.
    const auto initial_address = reinterpret_cast<std::uintptr_t>(values.data());
    for (std::size_t i = 1; i < 512; ++i)
    {
        values.push_back('x');
        ASSERT_EQ(values.size(), i + 1U);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(values.data()), initial_address);
    }
    EXPECT_EQ(resource.stats().allocations, 1U);
    EXPECT_EQ(resource.stats().deallocations, 0U);
    EXPECT_GT(resource.stats().growth_merges, 0U);
    EXPECT_TRUE(std::all_of(values.begin(), values.end(), [](char value) { return value == 'x'; }));
}

TEST(example_12_buddy_memory_resource, copy_assignment_combines_free_buddies)
{
    //! [buddy_copy_assignment]
    buddy_memory_resource destination_resource;
    buddy_memory_resource source_resource;
    {
        array destination{allocator{&destination_resource}};
        array source{allocator{&source_resource}};
        destination.resize(16, 'x');
        source.resize(64, 'y');
        const auto address = reinterpret_cast<std::uintptr_t>(destination.data());
        const auto before  = destination_resource.stats();

        destination = source;  // The same expression as std::vector assignment.

        // Merge 16 -> 32 -> 64 without a new allocation or copying the discarded destination bytes.
        // Assignment still copies the 64 source characters into the enlarged destination.
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(destination.data()), address);
        EXPECT_EQ(destination.capacity(), source.size());
        EXPECT_EQ(destination, source);
        EXPECT_EQ(source.size(), 64U);
        EXPECT_TRUE(std::all_of(source.begin(), source.end(), [](char value) { return value == 'y'; }));
        EXPECT_EQ(destination_resource.stats().allocations, before.allocations);
        EXPECT_EQ(destination_resource.stats().deallocations, before.deallocations);
        EXPECT_EQ(destination_resource.stats().successful_reallocations, before.successful_reallocations + 1U);
        EXPECT_EQ(destination_resource.stats().growth_merges, before.growth_merges + 2U);
        EXPECT_EQ(destination_resource.stats().peak_reserved_bytes, 64U);
        std::cout << "Copy assignment: two buddy merges, no replacement allocation, 64 peak destination bytes\n";
    }
    EXPECT_EQ(destination_resource.stats().reserved_bytes, 0U);
    EXPECT_EQ(source_resource.stats().reserved_bytes, 0U);
    //! [buddy_copy_assignment]
}

TEST(example_12_buddy_memory_resource, copy_assignment_releases_before_fallback_allocation)
{
    buddy_memory_resource destination_resource;
    buddy_memory_resource source_resource;
    {
        array destination{allocator{&destination_resource}};
        array source{allocator{&source_resource}};
        destination.resize(16, 'x');
        source.resize(64, 'y');
        auto*      blocker = destination_resource.allocate(16, 1);
        const auto before  = destination_resource.stats();

        destination = source;  // The occupied buddy prevents in-place growth; VLA handles replacement.

        EXPECT_EQ(destination, source);
        EXPECT_EQ(destination.capacity(), 64U);
        EXPECT_EQ(source.size(), 64U);
        EXPECT_EQ(destination_resource.stats().successful_reallocations, before.successful_reallocations);
        EXPECT_EQ(destination_resource.stats().allocations, before.allocations + 1U);
        EXPECT_EQ(destination_resource.stats().deallocations, before.deallocations + 1U);
        // 64 destination bytes + the 16-byte blocker, with no overlap between old and new destination buffers.
        EXPECT_EQ(destination_resource.stats().peak_reserved_bytes, 80U);
        destination_resource.deallocate(blocker, 16, 1);
    }
    EXPECT_EQ(destination_resource.stats().reserved_bytes, 0U);
    EXPECT_EQ(source_resource.stats().reserved_bytes, 0U);
}

TEST(example_12_buddy_memory_resource, blocked_growth_falls_back_in_the_container)
{
    buddy_memory_resource resource;
    array                 values{allocator{&resource}};
    values.reserve(16);
    values.resize(16, 'x');
    auto* blocker = resource.allocate(16, 1);
    values.reserve(32);
    EXPECT_EQ(resource.stats().successful_reallocations, 0U);
    EXPECT_EQ(resource.stats().allocations, 3U);  // Original, blocker, replacement.
    EXPECT_EQ(resource.stats().deallocations, 1U);
    EXPECT_EQ(values.size(), 16U);
    EXPECT_TRUE(std::all_of(values.begin(), values.end(), [](char value) { return value == 'x'; }));
    resource.deallocate(blocker, 16, 1);
}

TEST(example_12_buddy_memory_resource, failed_multilevel_merge_is_atomic)
{
    buddy_memory_resource resource;
    auto*                 first   = resource.allocate(16, 1);
    auto*                 buddy   = resource.allocate(16, 1);
    auto*                 blocker = resource.allocate(32, 1);
    resource.deallocate(buddy, 16, 1);
    EXPECT_EQ(resource.reallocate(first, 16, 64, 1), nullptr);
    EXPECT_EQ(resource.stats().growth_merges, 0U);
    EXPECT_EQ(resource.stats().reserved_bytes, 48U);
    // The first merge was possible, but must not have consumed its free buddy on failure.
    auto* still_free = resource.allocate(16, 1);
    EXPECT_EQ(static_cast<unsigned char*>(still_free), static_cast<unsigned char*>(first) + 16);
    resource.deallocate(still_free, 16, 1);
    resource.deallocate(blocker, 32, 1);
    resource.deallocate(first, 16, 1);
    auto* whole = resource.allocate(buddy_memory_resource::arena_bytes);
    resource.deallocate(whole, buddy_memory_resource::arena_bytes);
    EXPECT_EQ(resource.stats().reserved_bytes, 0U);
}

TEST(example_12_buddy_memory_resource, upper_buddy_cannot_grow_downwards)
{
    buddy_memory_resource resource;
    auto*                 lower = resource.allocate(16, 1);
    auto*                 upper = resource.allocate(16, 1);
    resource.deallocate(lower, 16, 1);
    EXPECT_EQ(resource.reallocate(upper, 16, 32, 1), nullptr);
    resource.deallocate(upper, 16, 1);
}

TEST(example_12_buddy_memory_resource, shrink_releases_upper_buddies)
{
    buddy_memory_resource resource;
    auto*                 original = resource.allocate(64, 1);
    auto*                 smaller  = resource.reallocate(original, 64, 16, 1);
    ASSERT_NE(smaller, nullptr);
    EXPECT_EQ(resource.stats().reserved_bytes, 16U);
    auto* buddy = resource.allocate(16, 1);
    EXPECT_EQ(static_cast<unsigned char*>(buddy), static_cast<unsigned char*>(smaller) + 16);
    resource.deallocate(buddy, 16, 1);
    auto* restored = resource.reallocate(smaller, 16, 64, 1);
    ASSERT_NE(restored, nullptr);
    resource.deallocate(restored, 64, 1);
}

TEST(example_12_buddy_memory_resource, rounding_and_declined_requests_preserve_allocation)
{
    buddy_memory_resource resource;
    auto*                 p = resource.allocate(17);
    EXPECT_EQ(reinterpret_cast<std::uintptr_t>(p) % alignof(std::max_align_t), 0U);
    EXPECT_EQ(resource.stats().reserved_bytes, 32U);
    p = resource.reallocate(p, 17, 31);
    ASSERT_NE(p, nullptr);
    EXPECT_EQ(resource.stats().reserved_bytes, 32U);
    EXPECT_EQ(resource.reallocate(p, 31, 0), nullptr);
    EXPECT_EQ(resource.reallocate(p, 31, std::numeric_limits<std::size_t>::max()), nullptr);
    EXPECT_EQ(resource.reallocate(p, 31, 32, 2U * alignof(std::max_align_t)), nullptr);
    resource.deallocate(p, 31);
    EXPECT_EQ(resource.stats().reserved_bytes, 0U);
}

TEST(example_12_buddy_memory_resource, exhausted_arena_coalesces_after_scattered_frees)
{
    buddy_memory_resource                                resource;
    std::array<void*, buddy_memory_resource::leaf_count> blocks{};
    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        blocks[i] = resource.allocate(16, 1);
        ASSERT_NE(blocks[i], nullptr);
        std::fill_n(static_cast<unsigned char*>(blocks[i]), 16, static_cast<unsigned char>(i));
    }
#if defined(__cpp_exceptions)
    EXPECT_THROW(resource.allocate(16, 1), std::bad_alloc);
#else
    EXPECT_EQ(resource.allocate(16, 1), nullptr);
#endif
    EXPECT_EQ(resource.stats().reserved_bytes, buddy_memory_resource::arena_bytes);
    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        const auto* bytes = static_cast<const unsigned char*>(blocks[i]);
        for (std::size_t j = 0; j < 16; ++j)
        {
            EXPECT_EQ(bytes[j], static_cast<unsigned char>(i));
        }
    }
    for (std::size_t i = 0; i < blocks.size(); ++i)
    {
        resource.deallocate(blocks[(i * 17U) % blocks.size()], 16, 1);
    }
    auto* whole = resource.allocate(buddy_memory_resource::arena_bytes);
    ASSERT_NE(whole, nullptr);
    resource.deallocate(whole, buddy_memory_resource::arena_bytes);
    EXPECT_EQ(resource.stats().reserved_bytes, 0U);
}
}  // namespace
