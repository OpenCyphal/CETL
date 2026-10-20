/// @file
/// Checks that the deprecated pre-snake_case names still exist and name the same types as their replacements.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/array_memory_resource.hpp"
#include "cetl/pf17/buffer_memory_resource.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"
#include "cetl/pmr/buffer_memory_resource_delegate.hpp"
#include "cetl/pmr/interface_ptr.hpp"
#include "cetl/pmr/memory.hpp"
#include "cetl/pmr/o1heap_memory_resource_delegate.hpp"

#include <type_traits>

// Every use of an old name below is deliberate.
#if defined(__GNUC__)
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif

namespace
{

using pf17_alloc = cetl::pf17::pmr::polymorphic_allocator<int>;

static_assert(
    std::is_same<cetl::VariableLengthArray<int, pf17_alloc>, cetl::variable_length_array<int, pf17_alloc>>::value,
    "");
static_assert(std::is_same<cetl::VariableLengthArray<bool, cetl::pf17::pmr::polymorphic_allocator<bool>>,
                           cetl::variable_length_array<bool, cetl::pf17::pmr::polymorphic_allocator<bool>>>::value,
              "");

static_assert(std::is_same<cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<16>,
                           cetl::pf17::pmr::unsynchronized_array_memory_resource<16>>::value,
              "");
static_assert(std::is_same<cetl::pf17::pmr::UnsynchronizedBufferMemoryResource,
                           cetl::pf17::pmr::unsynchronized_buffer_memory_resource>::value,
              "");
static_assert(std::is_same<cetl::pf17::pmr::deviant::MaxAlignNewDeleteResource,
                           cetl::pf17::pmr::deviant::max_align_new_delete_resource>::value,
              "");

static_assert(
    std::is_same<cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate<cetl::pf17::pmr::memory_resource>,
                 cetl::pmr::unsynchronized_buffer_memory_resource_delegate<cetl::pf17::pmr::memory_resource>>::value,
    "");
static_assert(std::is_same<cetl::pmr::O1HeapAlignedStorage<1024>, cetl::pmr::o1heap_aligned_storage<1024>>::value, "");
static_assert(std::is_same<cetl::pmr::UnsynchronizedO1HeapMemoryResourceDelegate,
                           cetl::pmr::unsynchronized_o1heap_memory_resource_delegate>::value,
              "");

static_assert(std::is_same<cetl::pmr::MemoryResourceDeleter<cetl::pf17::pmr::memory_resource>,
                           cetl::pmr::memory_resource_deleter<cetl::pf17::pmr::memory_resource>>::value,
              "");
static_assert(
    std::is_same<cetl::pmr::PolymorphicDeleter<pf17_alloc>, cetl::pmr::polymorphic_deleter<pf17_alloc>>::value,
    "");
static_assert(std::is_same<cetl::pmr::Factory, cetl::pmr::factory>::value, "");

struct named
{
    virtual ~named()                   = default;
    virtual int value() const noexcept = 0;
};

struct concrete_named final : named
{
    explicit concrete_named(const int v)
        : v_{v}
    {
    }
    int value() const noexcept override
    {
        return v_;
    }
    int v_;
};

static_assert(std::is_same<cetl::pmr::PmrInterfaceDeleter<named>, cetl::pmr::pmr_interface_deleter<named>>::value, "");
static_assert(std::is_same<cetl::pmr::InterfacePtr<named>, cetl::pmr::interface_ptr<named>>::value, "");
static_assert(std::is_same<cetl::pmr::InterfaceFactory, cetl::pmr::interface_factory>::value, "");

TEST(TestDeprecatedNames, OldNamesAreUsable)
{
    cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<256> resource{};

    cetl::VariableLengthArray<int, pf17_alloc> vla{pf17_alloc{&resource}};
    vla.push_back(42);
    EXPECT_EQ(1U, vla.size());
    EXPECT_EQ(42, vla[0]);

    // The array resource above holds a single allocation, so the factories use the heap.
    pf17_alloc heap_alloc{cetl::pf17::pmr::new_delete_resource()};
    auto       made = cetl::pmr::Factory::make_unique(heap_alloc, 7);
    ASSERT_NE(nullptr, made);
    EXPECT_EQ(7, *made);

    cetl::pmr::InterfacePtr<named> iface = cetl::pmr::InterfaceFactory::make_unique<
        named>(cetl::pf17::pmr::polymorphic_allocator<concrete_named>{cetl::pf17::pmr::new_delete_resource()}, 3);
    ASSERT_NE(nullptr, iface);
    EXPECT_EQ(3, iface->value());
}

}  // namespace
