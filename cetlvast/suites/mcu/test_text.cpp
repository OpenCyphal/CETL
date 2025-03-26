/// @file
/// Unit tests for cetl::pmr::O1heapResource
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include "cetl/cetl.hpp"
#include "cetl/visit_helpers.hpp"
#include "cetl/pf20/cetlpf.hpp"
#include "cetl/variable_length_array.hpp"
#include "cetl/pf17/array_memory_resource.hpp"

namespace
{
constexpr std::size_t DummyBssBufferSizeBytes{1024U};

template <typename Allocator>
cetl::optional<cetl::VariableLengthArray<cetl::byte, Allocator>> create_dummy_data(const Allocator& alloc)
{
    return cetl::VariableLengthArray<cetl::byte, Allocator>{alloc};
}

cetl::pmr::memory_resource* make_bss_memory_resource()
{
    static cetl::pf17::pmr::UnsynchronizedArrayMemoryResource<DummyBssBufferSizeBytes>
        memory{cetl::pmr::null_memory_resource(), 0};
    return &memory;
}

constexpr auto make_variant()
{
    return cetl::variant<int, float, unsigned long>{3};
}

};  // namespace


// This is all just to ensure stuff compiles. We don't have on-target testing defined yet.
int main()
{
    cetl::pmr::polymorphic_allocator<cetl::byte> alloc{make_bss_memory_resource()};
    auto                                         dummy = create_dummy_data(alloc);
    auto                                         vdumb = make_variant();

    int result = 0;
    const auto visitor = cetl::make_overloaded(
        [&result](int i){ result = i; },
        [](float){},
        [](unsigned long){}
    );
    cetl::visit(visitor, vdumb);

    return result;
}
