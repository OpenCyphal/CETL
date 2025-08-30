/// @file
/// Defines cetl::pmr::O1HeapMemoryResource which is a std::pmr::memory_resource implemented in terms of Pavel
/// Kirienko's [o1heap](https://github.com/pavel-kirienko/o1heap). If including this file you will need to either first
/// include cetl/pf17/cetlpf.hpp or provide the memory_resource definition you want this class to use.
/// You'll also need to provide an include path to o1heap.h and compile in o1heap.c when using this type.
///
/// @snippet{trimleft} example_11_memory_resource_01heap.cpp main
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PMR_O1HEAP_MEMORY_RESOURCE_DELEGATE_H_INCLUDED
#define CETL_PMR_O1HEAP_MEMORY_RESOURCE_DELEGATE_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include <array>

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
    static constexpr std::size_t arena_size = ((size_bytes + (alignment - 1)) / alignment) * alignment;

    static_assert(O1HEAP_ALIGNMENT >= alignof(std::max_align_t), "O1HEAP_ALIGNMENT is too small for this platform.");

    struct alignas(alignment) type
    {
        std::array<unsigned char, arena_size> data;
    } storage[1];
};

class UnsynchronizedO1HeapMemoryResourceDelegate
{
public:
    UnsynchronizedO1HeapMemoryResourceDelegate(void* buffer, std::size_t buffer_size_bytes)
        : o1heap_{o1heapInit(buffer, buffer_size_bytes)}
    {
#if defined CETL_ENABLE_DEBUG_ASSERT && CETL_ENABLE_DEBUG_ASSERT
        if (nullptr == o1heap_)
        {
            CETL_DEBUG_ASSERT(o1heapMinArenaSize <= buffer_size_bytes, "CDE_o1h_002: buffer_size_bytes is too small.");
            CETL_DEBUG_ASSERT(nullptr != o1heap_, "CDE_o1h_001: o1heapInit failed.");
        }
#endif
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

#if defined(__cpp_exceptions)
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
        return o1heapGetMaxAllocationSize(o1heap_);
    }

private:
    O1HeapInstance* o1heap_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_O1HEAP_MEMORY_RESOURCE_DELEGATE_H_INCLUDED
