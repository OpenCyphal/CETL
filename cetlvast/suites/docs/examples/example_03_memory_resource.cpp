/// @file
/// Example of using CETL memory_resource types.
///
/// This file implements two cetl::pf17::pmr::memory_resource specializations to demonstrate the
/// utility and requirements of the type.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
#include "cetl/pf17/sys/memory_resource.hpp"
#include <memory_resource>
#include "cetl/pf17/byte.hpp"
#include <iostream>
#include <memory>
#include <algorithm>
#include <new>
#include <cstring>

namespace cetl
{
namespace pf17
{

/// Implements a memory resource that over-allocates memory from an upstream memory resource to support
/// over-aligning allocations without C++17 or platform-specific system calls.
class OverAlignedMemoryResource : public pmr::memory_resource
{
    /// A control-block, of sorts.
    /// The MemoryBlock provides a map between the system aligned memory returned by the
    /// upstream memory_resource to the over-aligned region provided to the caller
    /// of this specialization.
    struct MemoryBlock
    {
        void*        aligned_memory{nullptr};
        std::size_t  aligned_memory_size_bytes{0};
        std::size_t  memory_block_size_bytes;
        MemoryBlock* next{nullptr};
    };

    /// We use a single unique_ptr as the head of a linked list of MemoryBlocks.
    /// When the OverAlignedMemoryResource is deleted this deleter will deconstruct all memory blocks.
    struct MemoryBlockDeleter
    {
        MemoryBlockDeleter(pmr::memory_resource* upstream)
            : upstream_(upstream)
        {
        }

        void operator()(MemoryBlock* cb)
        {
            CETL_DEBUG_ASSERT(nullptr != upstream_, "null memory_resource stored in MemoryBlockDeleter!");
            MemoryBlock* n = cb;
            while (n != nullptr)
            {
                MemoryBlock*      nn                      = n->next;
                const std::size_t memory_block_size_bytes = n->memory_block_size_bytes;
                n->~MemoryBlock();
                upstream_->deallocate(n, memory_block_size_bytes);
                n = nn;
            }
        };

    private:
        pmr::memory_resource* upstream_;
    };

    /// A helper type definition for the root block smart pointer.
    using MemoryBlockPointer = std::unique_ptr<MemoryBlock, MemoryBlockDeleter>;

public:
    /// Required constructor.
    ///
    /// While STL prefers using std::pmr::new_delete_resource as a default thus allowing
    /// for default constructors, CETL does not provide this API since it cannot be properly
    /// implemented using C++14 without using non-standard and platform-specific APIs.
    /// @param upstream The memory_resource this class uses to allocate default-aligned memory.
    OverAlignedMemoryResource(pmr::memory_resource* upstream = cetl::pf17::pmr::new_delete_resource())
        : head_(nullptr, MemoryBlockDeleter(upstream))
        , tail_(nullptr)
        , upstream_(upstream)
    {
    }

    // Remember, this is a polymorphic type and relies on virtual methods.
    virtual ~OverAlignedMemoryResource() = default;

    // we could implement the move constructors but this is just a example so we'll
    // keep it simple. The copy constructors should not be implemented since that would
    // require an additional layer of abstraction needed to share the internal
    // memory blocks.
    OverAlignedMemoryResource(const OverAlignedMemoryResource&)            = delete;
    OverAlignedMemoryResource& operator=(const OverAlignedMemoryResource&) = delete;
    OverAlignedMemoryResource(OverAlignedMemoryResource&& rhs)             = delete;
    OverAlignedMemoryResource& operator=(OverAlignedMemoryResource&& rhs)  = delete;

protected:
    //! [do_allocate]
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        // The standard specifies a pre-condition that alignment is a power of two.
        CETL_DEBUG_ASSERT(alignment && !(alignment & (alignment - 1)), "Alignment must be a power of 2.");

        // This class has a precondition that the memory_resource pointer is not-null. We use a raw pointer
        // since the C++17 standard uses a similar pattern for an "upstream" allocator for the
        // std::pmr::monotonic_buffer_resource.
        if (nullptr == upstream_)
        {
#if __cpp_exceptions
            throw std::bad_alloc();
#endif
            return nullptr;
        }

        // Optimization here: if we are not over-aligning then just use the upstream allocator.
        if (alignment <= alignof(std::max_align_t))
        {
            return upstream_->allocate(size_bytes, alignment);
        }

        // This method will demonstrate an implementation of do_allocate that handles the alignment parameter using
        // only portable APIs available in C++14. We'll over-allocate from the system to store a MemoryBlock and
        // to ensure we can locate a starting pointer within the memory that is aligned to the requested power of 2.

        // We'll allocate the size of the MemoryBlock and also enforce that the size plus the padding added by the
        // compiler provides an alignof(std::max_align_t) aligned start to our aligned memory area.
        const std::size_t control_block_size_bytes =
            sizeof(MemoryBlock) + (sizeof(MemoryBlock) % alignof(std::max_align_t));

        // Now we adjust our allocation to account for over-alignment. For under-alignment we just waste memory.
        const std::size_t over_alignment = (alignment > alignof(std::max_align_t)) ? alignment : 0;
        const std::size_t upstream_size  = control_block_size_bytes + over_alignment + size_bytes;

        byte* const max_aligned_memory =
            static_cast<byte*>(upstream_->allocate(upstream_size, alignof(std::max_align_t)));

        if (nullptr == max_aligned_memory)
        {
            // we don't have to throw here because, if exceptions are enabled, the upstream call would have thrown.
            return nullptr;
        }

        // Time to setup RAII for our raw memory. We can reuse the MemoryBlockDeleter as long as we haven't yet
        // set the next_block pointer.
        MemoryBlockPointer cb(new (max_aligned_memory) MemoryBlock{nullptr, size_bytes, upstream_size, nullptr},
                              MemoryBlockDeleter(upstream_));

        // We expect all malloc implementations to return memory aligned to std::max_align_t.
        CETL_DEBUG_ASSERT((reinterpret_cast<std::uintptr_t>(max_aligned_memory) % alignof(std::max_align_t)) == 0,
                          "The upstream allocator must provide alignof(std::max_align_t) aligned memory!?");

        // Now we give the area after the MemoryBlock to std::align to do it's thing.
        void*       aligned_memory          = &static_cast<byte*>(max_aligned_memory)[control_block_size_bytes];
        std::size_t max_aligned_memory_size = upstream_size - control_block_size_bytes;
        aligned_memory = std::align(alignment, size_bytes, aligned_memory, max_aligned_memory_size);

        // If we get here then something about our expectations for over-allocation memory was wrong. Given this
        // is just example code we haven't rigorously proven that it will always be correct. UMMV.
        if (nullptr == aligned_memory || max_aligned_memory_size < size_bytes)
        {
#if __cpp_exceptions
            throw std::bad_alloc();
#endif
            return nullptr;
        }

        // One last sanity check but this failing would mean std::align was broken so we don't expect
        // this ever fail given a correctly implemented C++14 library.
        CETL_DEBUG_ASSERT((reinterpret_cast<std::uintptr_t>(aligned_memory) % static_cast<std::uintptr_t>(alignment)) ==
                              0,
                          "Internal alignment math was incorrect and did not result in a properly aligned memory block "
                          "block.");

        // All that's left is the linked-list business.
        cb->aligned_memory = aligned_memory;

        if (nullptr == head_.get())
        {
            CETL_DEBUG_ASSERT(nullptr == tail_, "Tail must be null when head is null.");
            // This is the first. We establish the head of our linked list of
            // allocations here.
            head_.swap(cb);
            tail_ = head_.get();
        }
        else if (nullptr == tail_)
        {
            head_->next = cb.release();
            tail_       = head_->next;
        }
        else
        {
            tail_->next = cb.release();
            tail_       = tail_->next;
        }

        // We return the aligned region to the call. They must use this object to de_allocate or the behaviour
        // is undefined.
        return aligned_memory;
    }
    //! [do_allocate]
    //! [do_deallocate]
    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        // The standard does not actually specify what to do about size_bytes nor alignment in the deallocate method.
        // It does require that this method does not throw and, since there's no return value,
        // this means any failures to find and deallocate memory are silent.
        // We use alignment to detect which allocator was used and dispatch the deallocate call accordingly.
        if (alignment <= alignof(std::max_align_t))
        {
            upstream_->deallocate(p, size_bytes, alignment);
            return;
        }
        MemoryBlock* previous;

        MemoryBlock* cb = find(p, previous);

        // In a debug mode you might want to assert on conditions that shouldn't occur
        // in a healthy program.
        // Here we maintain the standard behaviour of C that deallocating nullptr is
        // not an error. We therefore only abort if p is not null AND we did not find a
        // control block OR p was null AND we somehow did find a control block
        // (that would be weird).
        CETL_DEBUG_ASSERT((p && cb) || (!p && !cb), "Unknown pointer provided to do_deallocate.");

        // Here we assert that, if we are deallocating a valid block, the size requested
        // by the call to do_allocate is the same size provided to this method.
        CETL_DEBUG_ASSERT(!p || cb->aligned_memory_size_bytes == size_bytes,
                          "Control Block size did not match size argument for do_deallocate.");

        if (nullptr != cb)
        {
            if (nullptr == previous)
            {
                // there was no previous block. This is the head we are deleting.
                CETL_DEBUG_ASSERT(cb == head_.get(), "find logic is incorrect");
                (void) head_.release();
                head_.reset(cb->next);
            }
            else
            {
                previous->next = cb->next;
            }
            if (cb == tail_)
            {
                tail_ = cb->next;
            }
            const std::size_t memory_block_size_bytes = cb->memory_block_size_bytes;
            cb->~MemoryBlock();
            upstream_->deallocate(cb, memory_block_size_bytes);
        }
    }
    //! [do_deallocate]
    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        // As our heap is stored in a unique pointer we cannot share it with
        // another instance of this class or any other memory_resource. As such,
        // Simple pointer comparison should suffice.
        return (&rhs == this);
    }

private:
    MemoryBlock* find(void* p, MemoryBlock*& out_previous)
    {
        // this is, of course, really naive and O(n). To implement this class for heavy loads
        // some sort of logarithmic search algorithm should be supported.
        out_previous    = nullptr;
        MemoryBlock* cb = head_.get();
        while (cb != nullptr)
        {
            if (cb->aligned_memory == p)
            {
                break;
            }
            out_previous = cb;
            cb           = cb->next;
        }
        return cb;
    }

    MemoryBlockPointer head_;
    MemoryBlock*       tail_;
    memory_resource*   upstream_;
};

// +--------------------------------------------------------------------------+
// everything after this point is just RAII and fake machinery used to build
// an executable example. You don't need to look at it to understand what
// this file is trying to demonstrate.
class FakeDmaTransfer
{
public:
    FakeDmaTransfer(byte* buffer, std::size_t buffer_size)
        : buffer_{buffer}
        , buffer_size_{buffer_size}
        , make_believe_progress_{0}
    {
    }

    bool is_complete() const
    {
        const std::size_t run_until = std::min(buffer_size_, make_believe_progress_ + (buffer_size_ / 12));
        for (; make_believe_progress_ < run_until; ++make_believe_progress_)
        {
            buffer_[make_believe_progress_] = static_cast<byte>(make_believe_progress_ % sizeof(byte));
        }

        return (make_believe_progress_ >= buffer_size_);
    }

private:
    byte*               buffer_;
    std::size_t         buffer_size_;
    mutable std::size_t make_believe_progress_;
};

template <typename T>
struct MemoryResourceDeleter
{
    MemoryResourceDeleter(pmr::memory_resource* resource, std::size_t allocation_size, std::size_t buffer_alignment)
        : resource_(resource)
        , allocation_size_(allocation_size)
        , buffer_alignment_(buffer_alignment)
    {
    }

    void operator()(T* p)
    {
        CETL_DEBUG_ASSERT(nullptr != resource_, "null memory_resource stored in MemoryResourceDeleter!");
        resource_->deallocate(p, allocation_size_, buffer_alignment_);
    };

private:
    pmr::memory_resource* resource_;
    std::size_t           allocation_size_;
    std::size_t           buffer_alignment_;
};

template <typename T>
std::unique_ptr<T, MemoryResourceDeleter<T>> allocate_buffer(pmr::memory_resource& allocator,
                                                             std::size_t           buffer_size,
                                                             std::size_t           buffer_alignment)
{
    return std::unique_ptr<T, MemoryResourceDeleter<T>>{static_cast<T*>(
                                                            allocator.allocate(buffer_size, buffer_alignment)),
                                                        MemoryResourceDeleter<T>{&allocator,
                                                                                 buffer_size,
                                                                                 buffer_alignment}};
}

// +--------------------------------------------------------------------------+
}  // namespace pf17
}  // namespace cetl

int main()
{
    cetl::pf17::OverAlignedMemoryResource over_aligned_new_delete_resource{};

    // let's pretend we have dma that must be aligned to a 128-byte (1024-bit) boundary:
    std::cout << "About to allocate a big ol' buffer." << std::endl;
    auto buffer = cetl::pf17::allocate_buffer<cetl::pf17::byte>(over_aligned_new_delete_resource, 0x100000, 128);
    cetl::pf17::FakeDmaTransfer transfer(buffer.get(), 0x100000);
    std::cout << "About to pretend we're waiting on hardware." << std::endl;
    while (!transfer.is_complete())
    {
        std::cout << "Fake waiting..." << std::endl;
    }
    std::cout << "Our fake DMA transfer is complete!" << std::endl;

    // Just to prove a point, we can also use this as a regular old memory resource using the CETL deviant
    // set_new_delete_resource() method to transparently provide our new implementation to the program:
    cetl::pf17::pmr::deviant::set_new_delete_resource(&over_aligned_new_delete_resource);

    auto string_buffer =
        cetl::pf17::allocate_buffer<char>(*cetl::pf17::pmr::new_delete_resource(), sizeof(char) * 12, alignof(char));
    strncpy(string_buffer.get(), "hello world", 12);
    std::cout << string_buffer.get() << std::endl;

    // Do remember that memory_resource does not construct and delete objects. That is the job of allocators like
    // cetl::pf17::pmr::polymorphic_allocator. Because this example only used trivially constructable, copyable, and
    // destructible types we didn't have to build that machinery.
    return 0;
}
