/// @file
/// Defines cetl::pmr::O1HeapMemoryResource which is a std::pmr::memory_resource implemented in terms of Pavel
/// Kirienko's [o1heap](https://github.com/pavel-kirienko/o1heap). If including this file you will need to either first
/// include cetl/pf17/cetlpf.hpp or provide the memory_resource definition you want this class to use.
/// You'll also need to provide an include path to o1heap.h and compile in o1heap.c when using this type.
///
/// TODO: examples
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PMR_O1HEAP_MEMORY_RESOURCE_H_INCLUDED
#define CETL_PMR_O1HEAP_MEMORY_RESOURCE_H_INCLUDED

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
    static constexpr std::size_t alignment  = O1HEAP_ALIGNMENT;

    static_assert(O1HEAP_ALIGNMENT >= alignof(std::max_align_t), "O1HEAP_ALIGNMENT is too small for this platform.");

    typename std::aligned_storage<sizeof(unsigned char), alignment>::type storage[size_bytes];
};

class UnsynchronizedO1HeapMemoryResourceDelegate
{
public:
    UnsynchronizedO1HeapMemoryResourceDelegate(void* buffer, std::size_t buffer_size_bytes)
        : o1heap_{o1heapInit(buffer, buffer_size_bytes)}
        , max_size_bytes_{buffer_size_bytes}
    {
        // TODO: https://github.com/pavel-kirienko/o1heap/issues/17
        CETL_DEBUG_ASSERT(nullptr != o1heap_, "CDE_o1h_001: o1heapInit failed.");
    }

    template <typename AlignedStorageType>
    explicit UnsynchronizedO1HeapMemoryResourceDelegate(AlignedStorageType& aligned_storage)
        : UnsynchronizedO1HeapMemoryResourceDelegate(aligned_storage.storage, AlignedStorageType::size_bytes)
    {
    }

    ~UnsynchronizedO1HeapMemoryResourceDelegate()                                                            = default;
    UnsynchronizedO1HeapMemoryResourceDelegate(const UnsynchronizedO1HeapMemoryResourceDelegate&)            = delete;
    UnsynchronizedO1HeapMemoryResourceDelegate& operator=(const UnsynchronizedO1HeapMemoryResourceDelegate&) = delete;
    UnsynchronizedO1HeapMemoryResourceDelegate(UnsynchronizedO1HeapMemoryResourceDelegate&&)                 = delete;
    UnsynchronizedO1HeapMemoryResourceDelegate& operator=(UnsynchronizedO1HeapMemoryResourceDelegate&&)      = delete;

    void* allocate(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
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

    void deallocate(void* p, std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        // TODO https://github.com/pavel-kirienko/o1heap/issues/13
        (void) alignment;
        (void) size_bytes;
        o1heapFree(o1heap_, p);
    }

    std::size_t max_size() const noexcept
    {
        // TODO: https://github.com/pavel-kirienko/o1heap/issues/18
        return max_size_bytes_;
    }

private:
    O1HeapInstance* o1heap_;
    // TODO: remove when https://github.com/pavel-kirienko/o1heap/issues/18 is fixed.
    std::size_t max_size_bytes_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_O1HEAP_MEMORY_RESOURCE_H_INCLUDED
