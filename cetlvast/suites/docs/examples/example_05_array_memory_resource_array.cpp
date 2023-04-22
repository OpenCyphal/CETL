/// @file
/// Example of using the cetl::pmr::UnsynchronizedArrayMemoryResource in cetl/pmr/array_memory_resource.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

//![example_include]
#include "cetl/pf17/cetlpf.hpp"
#include "cetl/pmr/array_memory_resource.hpp"
//![example_include]

#include <vector>
#include <iostream>

//![example_setup]
struct Message
{
    Message(cetl::pmr::polymorphic_allocator<std::uint64_t>& allocator)
        : data{allocator}
    {
    }
    std::vector<std::uint64_t, cetl::pmr::polymorphic_allocator<std::uint64_t>> data;
};

// Let's say we have a data structure that contains a Message with variable-length data in it.
// We can use UnsynchronizedArrayMemoryResource to allocate a buffer large enough to hold all of this data at once
// but if there is less data the std::vector in the message will return the size() of that data (i.e. where an
// std::array would not).
static constexpr std::size_t SmallMessageSizeBytes = 64 * 8;
static cetl::byte            small_message_buffer_[SmallMessageSizeBytes];

//![example_setup]

int main()
{
    {
//![example_a]
        cetl::pmr::UnsynchronizedArrayMemoryResource    aResource{small_message_buffer_, SmallMessageSizeBytes};
        cetl::pmr::polymorphic_allocator<std::uint64_t> aAlloc{&aResource};
        Message                                         a{aAlloc};

        // The big "gotcha" when using UnsynchronizedArrayMemoryResource with STL containers is that you must reserve
        // the size needed before you insert data into them. This is because UnsynchronizedArrayMemoryResource only
        // allows one allocation at a time and vector, for example, will have two allocations outstanding as it
        // geometrically grows it's capacity when inserting new data beyond the current capacity.
        const std::size_t item_cout = SmallMessageSizeBytes / sizeof(decltype(aAlloc)::value_type);
        a.data.reserve(item_cout);

        std::cout << "BEFORE -> data size = " << a.data.size() << ", data capacity : " << a.data.capacity()
                  << std::endl;

        for (std::size_t i = 0; i < item_cout; ++i)
        {
            a.data.push_back(i);
        }

        std::cout << "AFTER  -> data size = " << a.data.size() << ", data capacity : " << a.data.capacity()
                  << std::endl;
//![example_a]
    }
    {
//![example_b]
        // BUT WAIT! THERE'S MORE! The UnsynchronizedArrayMemoryResource both slices and dices! That is, you can provide
        // an upstream allocator to turn this into a "small buffer optimization" resource where the internal allocation
        // is the small buffer and the upstream allocator becomes the larger allocator.
        cetl::pmr::UnsynchronizedArrayMemoryResource    bResource{small_message_buffer_,
                                                               SmallMessageSizeBytes,
                                                               cetl::pmr::new_delete_resource()};
        cetl::pmr::polymorphic_allocator<std::uint64_t> bAlloc{&bResource};
        Message                                         b{bAlloc};

        // This time we won't reserve which should cause vector to do multiple allocations. We'll also insert
        // a bunch more items then there space in the small message buffer.
        std::cout << "BEFORE -> data size = " << b.data.size() << ", data capacity : " << b.data.capacity()
                  << std::endl;

        const std::size_t item_cout = (SmallMessageSizeBytes / sizeof(decltype(bAlloc)::value_type)) * 100;
        for (std::size_t i = 0; i < item_cout; ++i)
        {
            b.data.push_back(i);
        }

        std::cout << "AFTER  -> data size = " << b.data.size() << ", data capacity : " << b.data.capacity()
                  << std::endl;
//![example_b]
    }
    {
//![example_c]
        // One more example: by using another UnsynchronizedArrayMemoryResource as an upstream for another
        // UnsynchronizedArrayMemoryResource with the same-sized buffer you can use vector push_back without reserve up
        // to the size of these buffers.
        static cetl::byte                               upstream_buffer[SmallMessageSizeBytes];
        cetl::pmr::UnsynchronizedArrayMemoryResource    cUpstreamResource{&upstream_buffer, SmallMessageSizeBytes};
        cetl::pmr::UnsynchronizedArrayMemoryResource    cResource{small_message_buffer_,
                                                               SmallMessageSizeBytes,
                                                               &cUpstreamResource};
        cetl::pmr::polymorphic_allocator<std::uint64_t> cAlloc{&cResource};
        Message                                         c{cAlloc};

        // This time we won't reserve which should cause vector to do multiple allocations. We'll also insert
        // a bunch more items then there space in the small message buffer.
        std::cout << "BEFORE -> data size = " << c.data.size() << ", data capacity : " << c.data.capacity()
                  << std::endl;

        const std::size_t item_cout = SmallMessageSizeBytes / sizeof(decltype(cAlloc)::value_type);
        for (std::size_t i = 0; i < item_cout; ++i)
        {
            c.data.push_back(i);
        }

        std::cout << "AFTER  -> data size = " << c.data.size() << ", data capacity : " << c.data.capacity()
                  << std::endl;
//![example_c]
    }
    return 0;
}
