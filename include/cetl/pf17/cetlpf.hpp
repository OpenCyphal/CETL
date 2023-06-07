/// @file
/// CETL polyfill header for C++17 types
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// @warning
/// This header violates AUTOSAR-14 M7-3-6, A17-0-1, and A16-0-1 (and possibly other rules). Don't use CETL polyfill
/// headers in AUTOSAR code.
///
/// @warning
/// polyfill headers cannot be used if CETL_H_ERASE is defined.
///
/// CETL polyfill headers will provide the CETL type as an aliased standard type in the std namespace when compiling
/// using older C++ libraries and will automatically provide types from the standard library when compiling
/// using newer C++ libraries. This allows for causal use of CETL as a true polyfill library but does violate certain
/// coding standards. As such, for more critical software we recommend not using these headers but including the
/// types you use from `cetl::pf17` directly.
///
/// For example, (TODO: polyfill 17 examples.)
///

#ifndef CETL_PF17_H_INCLUDED
#define CETL_PF17_H_INCLUDED

#include "cetl/cetl.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"

/// @namespace cetl::pf17 This namespace contains C++17 polyfill (pf) types.
///     The types within this namespace adhere to the C++17 specification and should be drop-in replaceable
///     with said standard types where they are available. CETL polyfill types may implement a sub-set of
///     the required functionality but they will not implement non-compliant functionality.
///
/// @namespace cetl::pf17::pmr          CETL polyfill types for the standard Polymorphic Memory Resource (PMR)
///                                     namespace.
/// @namespace cetl::pf17::pmr::deviant Types or methods that deviate from the C++17 specification.
///
#if (__cplusplus >= CETL_CPP_STANDARD_17 && !defined(CETL_DOXYGEN))
#    include <memory_resource>
#    include <cstddef>

namespace cetl
{

using byte = std::byte;

namespace pmr
{

using memory_resource = std::pmr::memory_resource;

inline memory_resource* null_memory_resource() noexcept
{
    return std::pmr::null_memory_resource();
}

template <typename T>
using polymorphic_allocator = std::pmr::polymorphic_allocator<T>;

inline memory_resource* new_delete_resource() noexcept
{
    return std::pmr::new_delete_resource();
}

}  // namespace pmr
}  // namespace cetl

#else
#    include "cetl/pf17/byte.hpp"

namespace cetl
{

using byte = cetl::pf17::byte;

namespace pmr
{
using memory_resource = cetl::pf17::pmr::memory_resource;

inline memory_resource* null_memory_resource() noexcept
{
    return cetl::pf17::pmr::null_memory_resource();
}

template <typename T>
using polymorphic_allocator = cetl::pf17::pmr::polymorphic_allocator<T>;

inline memory_resource* new_delete_resource() noexcept
{
    return cetl::pf17::pmr::new_delete_resource();
}

}  // namespace pmr
}  // namespace cetl
#endif

#if defined(CETL_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED) || defined(CETL_DOXYGEN)

namespace cetl
{
namespace pmr
{

/// Automatic implementation of cetl::pmr::memory_resource that uses
/// cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate as the internal implementation.
/// @tparam UpstreamResourceType The type of any upstream memory resource provided.
///
/// To use this polyfill type, include the header file `cetl/pmr/array_memory_resource.hpp`
/// before including this header. For example:
///
/// @snippet{trimleft} example_05_array_memory_resource_alignment.cpp example_include
///
/// (See cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate for more examples)
///
template <typename UpstreamResourceType>
class UnsynchronizedArrayMemoryResource final : public cetl::pmr::memory_resource
{
public:
    /// See cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate for details.
    /// @param buffer The buffer to provide to cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate.
    /// @param buffer_size_bytes The size of the buffer to provide to
    /// cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate
    /// @param upstream The upstream memory resource to provide to cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate.
    ///                 This cannot be null. Use cetl::pmr::null_memory_resource() if you don't want upstream memory.
    /// @param upstream_max_size_bytes The maximum size of the upstream memory resource.
    UnsynchronizedArrayMemoryResource(void*                 buffer,
                                      std::size_t           buffer_size_bytes,
                                      UpstreamResourceType* upstream,
                                      std::size_t           upstream_max_size_bytes) noexcept
        : delegate_{buffer, buffer_size_bytes, upstream, upstream_max_size_bytes}
    {
    }

    ~UnsynchronizedArrayMemoryResource() override                                          = default;
    UnsynchronizedArrayMemoryResource(const UnsynchronizedArrayMemoryResource&)            = delete;
    UnsynchronizedArrayMemoryResource& operator=(const UnsynchronizedArrayMemoryResource&) = delete;
    UnsynchronizedArrayMemoryResource(UnsynchronizedArrayMemoryResource&&)                 = delete;
    UnsynchronizedArrayMemoryResource& operator=(UnsynchronizedArrayMemoryResource&&)      = delete;

private:
    void* do_allocate(std::size_t bytes, std::size_t alignment) override
    {
        return delegate_.allocate(bytes, alignment);
    }

    void do_deallocate(void* p, std::size_t bytes, std::size_t alignment) override
    {
        delegate_.deallocate(p, bytes, alignment);
    }

    bool do_is_equal(const cetl::pmr::memory_resource& other) const noexcept override
    {
        return (this == &other);
    }

#    if (__cplusplus < CETL_CPP_STANDARD_17 || defined(CETL_DOXYGEN))
    std::size_t do_max_size() const noexcept override
    {
        return delegate_.max_size();
    }

    void* do_reallocate(void* p, std::size_t old_size_bytes, std::size_t new_size_bytes, std::size_t alignment) override
    {
        return delegate_.reallocate(p, old_size_bytes, new_size_bytes, alignment);
    }
#    else
public:
    // These aren't visible through the std::memory_resource interface but they
    // are still useable if you have access to the full type.
    std::size_t max_size() const noexcept
    {
        return delegate_.max_size();
    }

    void* reallocate(void* p, std::size_t old_size_bytes, std::size_t new_size_bytes, std::size_t alignment)
    {
        return delegate_.reallocate(p, old_size_bytes, new_size_bytes, alignment);
    }

private:
#    endif

    cetl::pmr::UnsynchronizedArrayMemoryResourceDelegate<UpstreamResourceType> delegate_;
};

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_ARRAY_MEMORY_RESOURCE_H_INCLUDED

#endif  // CETL_PF17_H_INCLUDED
