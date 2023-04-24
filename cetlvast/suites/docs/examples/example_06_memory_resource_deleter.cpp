/// @file
/// Example of using the cetl::pmr::MemoryResourceDeleter from cetl/pmr/memory.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

//![example_include]
#include "cetl/pf17/cetlpf.hpp"
#include "cetl/pmr/memory.hpp"
//![example_include]

#include <vector>
#include <algorithm>
#include <iostream>

int main()
{
    //![example_usage]
    // Let's say you wanted to store a bunch of buffers in a heap so you can get the largest one quickly.
    // You could do something like this:

    using MemoryResourcePointer = std::unique_ptr<void, cetl::pmr::MemoryResourceDeleter>;

    struct ByteBuffer
    {
        MemoryResourcePointer data;
        std::size_t           size;
    };

    cetl::pmr::memory_resource* resource = cetl::pmr::new_delete_resource();
    auto                        buffer_0 = ByteBuffer{{resource->allocate(256), {resource, 256}}, 256};
    auto                        buffer_1 = ByteBuffer{{resource->allocate(512), {resource, 512}}, 512};
    auto                        buffer_2 = ByteBuffer{{resource->allocate(1024), {resource, 1024}}, 1024};
    std::vector<ByteBuffer>     buffers;
    buffers.push_back(std::move(buffer_0));
    buffers.push_back(std::move(buffer_1));
    buffers.push_back(std::move(buffer_2));

    std::make_heap(buffers.begin(), buffers.end(), [](const auto& lhs, const auto& rhs) {
        return lhs.size < rhs.size;
    });

    auto& largest_buffer = buffers.front();

    std::cout << "Largest buffer size: " << largest_buffer.size << std::endl;

    // Now as long as the vector "buffers" is in scope, the buffers will be valid. When the vector is destroyed,
    // the buffers will be freed using the correct memory resource.

    //![example_usage]
    return 0;
}
