/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PMR_INTERFACE_PTR_H_INCLUDED
#define CETL_PMR_INTERFACE_PTR_H_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include "cetl/pmr/function.hpp"

#include <memory>
#include <functional>

namespace cetl
{
namespace pmr
{

template <typename Interface>
class PmrInterfaceDeleter final
{
public:
    template <typename PmrAllocator>
    PmrInterfaceDeleter(PmrAllocator alloc, std::size_t obj_count)
        : deleter_{alloc.resource(), [alloc, obj_count](Interface* ptr) mutable {
                       using Concrete     = typename PmrAllocator::value_type;
                       auto* concrete_ptr = static_cast<Concrete*>(ptr);

                       concrete_ptr->~Concrete();
                       alloc.deallocate(concrete_ptr, obj_count);
                   }}
    {
    }

    template <typename Down, typename = std::enable_if_t<std::is_base_of<Interface, Down>::value>>
    PmrInterfaceDeleter(const PmrInterfaceDeleter<Down>& other)
        : deleter_{other.get_memory_resource(), [other](Interface* ptr) {
                       // Delegate to the down class deleter.
                       other.deleter_(static_cast<Down*>(ptr));
                   }}
    {
    }

    memory_resource* get_memory_resource() const noexcept
    {
        return deleter_.get_memory_resource();
    }

    void operator()(Interface* ptr) noexcept
    {
        deleter_(ptr);
    }

private:
    template <typename Down>
    friend class PmrInterfaceDeleter;

    cetl::pmr::function<void(Interface*), 24, true /*IsPmr*/> deleter_;

};  // PmrInterfaceDeleter

template <typename Interface>
using InterfacePtr = std::unique_ptr<Interface, PmrInterfaceDeleter<Interface>>;

class InterfaceFactory final
{
public:
    ~InterfaceFactory() = delete;
    InterfaceFactory()  = delete;

    template <typename Interface, typename PmrAllocator, typename... Args>
    static InterfacePtr<Interface> make_unique(PmrAllocator alloc, Args&&... args)
    {
        using Concrete = typename PmrAllocator::value_type;

        InterfacePtr<Concrete> concrete_ptr{alloc.allocate(1), PmrInterfaceDeleter<Concrete>{alloc, 1}};
        if (nullptr != concrete_ptr)
        {
            alloc.construct(concrete_ptr.get(), std::forward<Args>(args)...);
        }

        return concrete_ptr;
    }

};  // InterfaceFactory

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_INTERFACE_PTR_H_INCLUDED
