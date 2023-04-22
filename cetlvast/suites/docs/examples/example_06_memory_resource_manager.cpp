/// @file
/// Example of using the cetl::pmr::MemoryResourceManager from cetl/pmr/memory_resource_manager.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

//![example_include]
#include "cetl/pf17/cetlpf.hpp"
#include "cetl/pmr/memory_resource_manager.hpp"
//![example_include]

#include <unordered_map>

int main()
{
    //![example_usage]
    // Let's say you wanted to store a bunch of buffers in a map, but you wanted to use a custom memory resource.
    // Use std::pmr::MemoryResourceManager to decorate you memory resource and then use the raii_allocate method to
    // allocate memory for your objects. The MemoryResourcePointer returned from raii_allocate will automatically
    // free the memory when it goes out of scope and it can be used as a key in a map, set, or other container.

    struct ByteBuffer
    {
        cetl::byte* data;
        std::size_t size;
    };

    cetl::pmr::MemoryResourceManager                                 resource{cetl::pmr::new_delete_resource()};
    std::unordered_map<cetl::pmr::MemoryResourcePointer, ByteBuffer> object_map{};

    auto buffer_0                   = resource.raii_allocate(256);
    auto buffer_1                   = resource.raii_allocate(512);
    auto buffer_2                   = resource.raii_allocate(1024);
    object_map[std::move(buffer_0)] = ByteBuffer{static_cast<cetl::byte*>(buffer_0.get()), buffer_0.size()};
    object_map[std::move(buffer_1)] = ByteBuffer{static_cast<cetl::byte*>(buffer_1.get()), buffer_1.size()};
    object_map[std::move(buffer_2)] = ByteBuffer{static_cast<cetl::byte*>(buffer_2.get()), buffer_2.size()};

    // Now as long as the MemoryResourcePointer is in scope, the buffers will be valid. When the map is destroyed,
    // the buffers will be freed using the correct memory resource.

    //![example_usage]
    return 0;
}
