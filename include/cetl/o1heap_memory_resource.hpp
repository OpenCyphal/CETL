/// @file
/// Defines cetl::pmr::O1HeapMemoryResource which is a std::pmr::memory_resource implemented in terms of Pavel
/// Kirienko's [o1heap](https://github.com/pavel-kirienko/o1heap). If including this file you will need to either first
/// include cetl/pf17/cetlpf.hpp or provide the memory_resource definition you want this class to use.
/// You'll also need to provide an include path to o1heap.h and compile in o1heap.c where use use this type.
///
/// TODO: examples
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_O1HEAP_MEMORY_RESOURCE_H_INCLUDED
#define CETL_O1HEAP_MEMORY_RESOURCE_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include <type_traits>  // for aligned_storage

#include "o1heap.h"

namespace cetl
{
namespace pmr
{

template <std::size_t StorageSizeBytes>
struct O1HeapAlignedStorage
{
    static constexpr std::size_t size_bytes = StorageSizeBytes;
    static constexpr std::size_t alignment = O1HEAP_ALIGNMENT;

    static_assert(O1HEAP_ALIGNMENT >= alignof(std::max_align_t), "O1HEAP_ALIGNMENT is too small for this platform.");

    typename std::aligned_storage<sizeof(byte), alignment>::type storage[size_bytes];
};

class UnsynchronizedO1HeapMemoryResource : public memory_resource
{
public:
    UnsynchronizedO1HeapMemoryResource(void* buffer, std::size_t buffer_size_bytes)
        : o1heap_{o1heapInit(buffer, buffer_size_bytes)}
    {
        // TODO: https://github.com/pavel-kirienko/o1heap/issues/17
        CETL_DEBUG_ASSERT(nullptr != o1heap_, "o1heapInit failed.");
    }

    template <typename AlignedStorageType>
    UnsynchronizedO1HeapMemoryResource(AlignedStorageType& aligned_storage)
        : UnsynchronizedO1HeapMemoryResource(aligned_storage.storage, AlignedStorageType::size_bytes)
    {
    }

    ~UnsynchronizedO1HeapMemoryResource()                                                    = default;
    UnsynchronizedO1HeapMemoryResource(const UnsynchronizedO1HeapMemoryResource&)            = delete;
    UnsynchronizedO1HeapMemoryResource& operator=(const UnsynchronizedO1HeapMemoryResource&) = delete;
    UnsynchronizedO1HeapMemoryResource(UnsynchronizedO1HeapMemoryResource&&)                 = delete;
    UnsynchronizedO1HeapMemoryResource& operator=(UnsynchronizedO1HeapMemoryResource&&)      = delete;

protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        (void) alignment;
        // TODO: https://github.com/pavel-kirienko/o1heap/issues/13
        void* result = o1heapAllocate(o1heap_, size_bytes);

#if __cpp_exceptions
        if (nullptr == result)
        {
            throw std::bad_alloc();
        }
#endif
        return result;
    }

    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        // TODO https://github.com/pavel-kirienko/o1heap/issues/13
        (void) alignment;
        (void) size_bytes;
        o1heapFree(o1heap_, p);
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return (this == &rhs);
    }

private:
    O1HeapInstance* o1heap_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_O1HEAP_MEMORY_RESOURCE_H_INCLUDED
