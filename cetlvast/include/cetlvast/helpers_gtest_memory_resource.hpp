/// @file
/// CETL VerificAtion SuiTe – Google test helpers that includes memory_resource.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETLVAST_HELPERS_GTEST_MEMORY_RESOURCE_H_INCLUDED
#define CETLVAST_HELPERS_GTEST_MEMORY_RESOURCE_H_INCLUDED

#include "cetlvast/helpers_gtest.hpp"

#include "cetl/pf17/memory_resource.hpp"
#include "cetl/pf17/sys/memory_resource.hpp"
#if (__cplusplus >= CETL_CPP_STANDARD_17)
#    include <memory_resource>
#endif

namespace cetlvast
{

/// Pf17 Mock of memory_resource.
class MockPf17MemoryResource : public cetl::pf17::pmr::memory_resource
{
public:
    MOCK_METHOD(void*, do_allocate, (std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(void, do_deallocate, (void* p, std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(bool, do_is_equal, (const cetl::pf17::pmr::memory_resource& rhs), (const, noexcept));

    static constexpr bool ReturnsNullWhenFNoExceptions = true;

    static cetl::pf17::pmr::memory_resource* get()
    {
        return cetl::pf17::pmr::null_memory_resource();
    }
};

#if (__cplusplus >= CETL_CPP_STANDARD_17)
/// Std Mock of memory_resource.
class MockStdMemoryResource : public std::pmr::memory_resource
{
public:
    MOCK_METHOD(void*, do_allocate, (std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(void, do_deallocate, (void* p, std::size_t size_bytes, std::size_t alignment));
    MOCK_METHOD(bool, do_is_equal, (const std::pmr::memory_resource& rhs), (const, noexcept));

    static constexpr bool ReturnsNullWhenFNoExceptions = false;

    static std::pmr::memory_resource* get()
    {
        return std::pmr::null_memory_resource();
    }
};
#endif

// +----------------------------------------------------------------------+

/// Memory Resource Helper (MRH)
/// Support for parameterized tests that use both std::pmr::memory_resource and cetl::pf17::pmr::memory_resource.
struct MRH final
{
    template <typename T, typename... Args>
    static std::enable_if_t<std::is_base_of<cetl::pf17::pmr::memory_resource, T>::value, T> construct(Args&&... args)
    {
        return T(std::forward<Args>(args)...);
    }

    template <typename T>
    static std::enable_if_t<std::is_base_of<cetl::pf17::pmr::memory_resource, T>::value,
                            cetl::pf17::pmr::memory_resource*>
    null_memory_resource()
    {
        return cetl::pf17::pmr::null_memory_resource();
    }

    template <typename T>
    static std::enable_if_t<std::is_base_of<cetl::pf17::pmr::memory_resource, T>::value, MockPf17MemoryResource>
    mock_memory_resource()
    {
        return MockPf17MemoryResource();
    }

#if (__cplusplus >= CETL_CPP_STANDARD_17)
    template <typename T, typename... Args>
    static std::enable_if_t<std::is_base_of<std::pmr::memory_resource, T>::value, T> construct(Args&&... args)
    {
        return T(std::forward<Args>(args)...);
    }

    template <typename T>
    static std::enable_if_t<std::is_base_of<std::pmr::memory_resource, T>::value, std::pmr::memory_resource*>
    null_memory_resource()
    {
        return std::pmr::null_memory_resource();
    }

    template <typename T>
    static std::enable_if_t<std::is_base_of<std::pmr::memory_resource, T>::value, MockStdMemoryResource>
    mock_memory_resource()
    {
        return MockStdMemoryResource();
    }

#endif
    MRH() = delete;
};

// +----------------------------------------------------------------------+

}  // namespace cetlvast

#endif  // CETLVAST_HELPERS_GTEST_MEMORY_RESOURCE_H_INCLUDED
