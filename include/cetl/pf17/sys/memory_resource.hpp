/// @file
/// Extends the cetl::pf17::pmr namespace with system memory resources. This is the only header in CETL that
/// relies on malloc/free which is why it is separate from cetl/pf17/memory_resource.hpp.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PF17_PMR_SYS_MEMORY_RESOURCE_H_INCLUDED
#define CETL_PF17_PMR_SYS_MEMORY_RESOURCE_H_INCLUDED

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <atomic>
#include <algorithm>

//  +--[mem.res.global]-------------------------------------------------------+
#ifndef CETL_H_ERASE
#    include "cetl/pf17/memory_resource.hpp"
#endif

namespace cetl
{
namespace pf17
{
namespace pmr
{
namespace deviant
{

class MaxAlignNewDeleteResource : public memory_resource
{
protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        if (alignment > alignof(std::max_align_t))
        {
#if __cpp_exceptions
            throw std::bad_alloc();
#endif
            return nullptr;
        }
        return ::operator new(size_bytes);
    }

    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        (void) size_bytes;
        (void) alignment;
        ::operator delete(p);
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return (&rhs == this);
    };
};

}  // namespace deviant

/// @namespace cetl::pf17::pmr::_detail Users should not utilize anything inside of this namespace directly. These
/// are quasi-private implementation details that are necessarily visible for header-only implementations.
namespace _detail
{

inline deviant::MaxAlignNewDeleteResource* get_max_align_new_delete_resource_singleton() noexcept
{
    static deviant::MaxAlignNewDeleteResource singleton{};
    return &singleton;
}

inline std::atomic<memory_resource*>& get_new_delete_resource_singleton() noexcept
{
    static std::atomic<memory_resource*> singleton{get_max_align_new_delete_resource_singleton()};
    return singleton;
}

}  // namespace _detail

//  +--[mem.res.global]-------------------------------------------------------+

inline memory_resource* new_delete_resource() noexcept
{
    return _detail::get_new_delete_resource_singleton().load();
}

//  +-------------------------------------------------------------------------+
namespace deviant
{

/// Replace the memory_resource returned from cetl::pf17::pmr::new_delete_resource().
/// Because C++14 does not provide a portable way to obtain over-aligned memory from the system, which means
/// the default "new_delete" implementation used by CETL cannot allocate over-aligned memory, this deviation from
/// the C++17 specification is provided to allow replacement of this implementation by the user.
///
/// @note
/// This implementation uses std::atomic::exchange to avoid data races with cetl::pf17::pmr::new_delete_resource().
///
/// @param r    If nullptr then the implementation returned from cetl::pf17::pmr::new_delete_resource() is set to
///             a static-duration instance of cetl::pf17::pmr::MaxAlignNewDeleteResource otherwise
///             cetl::pf17::pmr::new_delete_resource() is set to r.
/// @return The previous value returned by cetl::pf17::pmr::new_delete_resource().
constexpr memory_resource* set_new_delete_resource(memory_resource* r) noexcept
{
    if (nullptr == r)
    {
        return _detail::get_new_delete_resource_singleton().exchange(
            _detail::get_max_align_new_delete_resource_singleton());
    }
    else
    {
        return _detail::get_new_delete_resource_singleton().exchange(r);
    }
}

}  // namespace deviant

namespace _detail
{
inline std::atomic<memory_resource*>& get_default_resource_singleton() noexcept
{
    static std::atomic<memory_resource*> singleton{new_delete_resource()};
    return singleton;
}

}  // namespace _detail

//  +--[mem.res.global]-------------------------------------------------------+

inline memory_resource* set_default_resource(memory_resource* r) noexcept
{
    if (nullptr == r)
    {
        return _detail::get_default_resource_singleton().exchange(new_delete_resource());
    }
    else
    {
        return _detail::get_default_resource_singleton().exchange(r);
    }
}

inline memory_resource* get_default_resource() noexcept
{
    return _detail::get_default_resource_singleton().load();
}

//  +--[mem.res.monotonic.buffer]---------------------------------------------+

class monotonic_buffer_resource : public memory_resource
{
public:
    monotonic_buffer_resource(void* buffer, size_t buffer_size, memory_resource* upstream)
        : first_buffer_control_{buffer, buffer_size, 0, buffer_size, nullptr}
        , upstream_{upstream}
        , current_buffer_{&first_buffer_control_}
    {
    }

    monotonic_buffer_resource(std::size_t initial_size, memory_resource* upstream)
        : monotonic_buffer_resource(nullptr, initial_size, upstream)
    {
    }

    explicit monotonic_buffer_resource(memory_resource* upstream)
        : monotonic_buffer_resource(0, upstream)
    {
    }

    monotonic_buffer_resource()
        : monotonic_buffer_resource(get_default_resource())
    {
    }

    explicit monotonic_buffer_resource(size_t initial_size)
        : monotonic_buffer_resource(initial_size, get_default_resource())
    {
    }

    monotonic_buffer_resource(void* buffer, size_t buffer_size)
        : monotonic_buffer_resource(buffer, buffer_size, get_default_resource())
    {
    }

    virtual ~monotonic_buffer_resource()
    {
        release();
    }

    monotonic_buffer_resource(const monotonic_buffer_resource&)            = delete;
    monotonic_buffer_resource& operator=(const monotonic_buffer_resource&) = delete;

    //  +--[public methods]---------------------------------------------------+
    void release()
    {
        BufferControl* head = current_buffer_;
        while (head != &first_buffer_control_)
        {
            BufferControl* previous = head->previous;
            upstream_->deallocate(head, sizeof(BufferControl) + head->buffer_size, head->buffer_align);
            head = previous;
        }
        current_buffer_                             = &first_buffer_control_;
        first_buffer_control_.remaining_buffer_size = first_buffer_control_.buffer_size;
    }

    memory_resource* upstream_resource() const
    {
        return upstream_;
    }

protected:
    void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
    {
        void* result = nullptr;
        do
        {
            if (current_buffer_->buffer && current_buffer_->remaining_buffer_size >= size_bytes)
            {
                void*       buffer      = current_buffer_->buffer;
                std::size_t buffer_size = current_buffer_->remaining_buffer_size;
                result                  = std::align(alignment, size_bytes, buffer, buffer_size);
                if (result)
                {
                    current_buffer_->remaining_buffer_size = buffer_size - size_bytes;
                    break;
                }
            }
            const std::size_t previous_buffer_size =
                std::max({static_cast<std::size_t>(4), current_buffer_->buffer_size, size_bytes + alignment});

            // Simple geometric progression of buffer size growth.
            const std::size_t next_buffer_size = previous_buffer_size + (previous_buffer_size / 2U);

            void* raw = upstream_->allocate(sizeof(BufferControl) + next_buffer_size, alignment);
            if (nullptr == raw)
            {
                // out-of-memory with no exceptions.
                break;
            }
            void* buffer = &reinterpret_cast<BufferControl*>(raw)[1];
            current_buffer_ =
                new (raw) BufferControl{buffer, next_buffer_size, alignment, next_buffer_size, current_buffer_};
        } while (true);
        return result;
    }

    void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) override
    {
        (void) p;
        (void) size_bytes;
        (void) alignment;
    }

    bool do_is_equal(const memory_resource& rhs) const noexcept override
    {
        return (&rhs == this);
    }

private:
    struct BufferControl
    {
        void*          buffer;
        std::size_t    buffer_size;
        std::size_t    buffer_align;
        std::size_t    remaining_buffer_size;
        BufferControl* previous;
    };

    BufferControl    first_buffer_control_;
    memory_resource* upstream_;
    BufferControl*   current_buffer_;
};

}  // namespace pmr
}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_PMR_SYS_MEMORY_RESOURCE_H_INCLUDED
