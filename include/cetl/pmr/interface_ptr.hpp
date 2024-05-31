/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PMR_INTERFACE_PTR_H_INCLUDED
#define CETL_PMR_INTERFACE_PTR_H_INCLUDED

#include "cetl/pmr/function.hpp"

#include <memory>
#include <functional>

namespace cetl
{
namespace pmr
{

/// RAII helper for cetl::pf17::pmr::polymorphic_allocator and std::pmr::polymorphic_allocator.
/// Use with cetl::pmr::InterfaceFactory for the best and safest
/// experience. Remember, be safe, use the cetl::pmr::InterfaceFactory.
///
/// @note
/// See cetl::pmr::InterfaceFactory for an example of how to use this type.
///
/// @tparam Interface The interface type of the polymorphic allocator to use for deallocation.
///
template <typename Interface>
class PmrInterfaceDeleter final
{
public:

    /// Constructs a Concrete type-erased deleter for the given interface type.
    ///
    /// @tparam PmrAllocator The type of the polymorphic allocator to use for deallocation.
    ///
    template <typename PmrAllocator>
    PmrInterfaceDeleter(PmrAllocator alloc, std::size_t obj_count)
        : deleter_{[alloc, obj_count](Interface* ptr) mutable {
            using Concrete = typename PmrAllocator::value_type;

            auto* concrete_ptr = static_cast<Concrete*>(ptr);
            concrete_ptr->~Concrete();
            alloc.deallocate(concrete_ptr, obj_count);
        }}
    {
    }

    /// Functor called by smart-pointer to deallocate and deconstruct objects.
    ///
    /// @param ptr The object to deconstruct and deallocate.
    ///
    void operator()(Interface* ptr) noexcept
    {
        deleter_(ptr);
    }

    // Below convertor constructor is only possible with enabled PMR at `function`.
    // For now, we decided to comment it out, so that `InterfacePtr` always stays
    // within 24-bytes small object optimization, namely without extra memory allocation
    // just for the sake of "advanced" deleter (actually chain of casters down to original `Concrete` pointer).
    //
    //    template <typename Down, typename = std::enable_if_t<std::is_base_of<Interface, Down>::value>>
    //    PmrInterfaceDeleter(const PmrInterfaceDeleter<Down, Pmr>& other)
    //        : deleter_{other.get_memory_resource(), [other](Interface* ptr) {
    //                       // Delegate to the down class deleter.
    //                       // The down-casting is assumed to be safe because the caller
    //                       // guarantees that *ptr is of type Down.
    //                       // This is a possible deviation from AUTOSAR M5-2-3; whether the
    //                       // type is polymorphic or not is irrelevant in this context.
    //                       other.deleter_(static_cast<Down*>(ptr));
    //                   }}
    //    {
    //    }
    //
    //    Pmr* get_memory_resource() const noexcept
    //    {
    //        return deleter_.get_memory_resource();
    //    }
    //
    // private:
    //    template <typename Down, typename PmrT>
    //    friend class PmrInterfaceDeleter;

private:
    cetl::pmr::function<void(Interface*), 24> deleter_;

};  // PmrInterfaceDeleter

template <typename Interface>
using InterfacePtr = std::unique_ptr<Interface, PmrInterfaceDeleter<Interface>>;

/// Interface Factory helper for creating objects with polymorphic allocators using proper RAII semantics.
/// Uses the cetl::pmr::PmrInterfaceDeleter type to ensure proper type-erased deallocation.
///
/// Example usage:
///
/// @snippet{trimleft} example_07_polymorphic_alloc_deleter.cpp example_usage_2
/// (@ref example_07_polymorphic_alloc_deleter "See full example here...")
///
class InterfaceFactory final
{
public:
    ~InterfaceFactory() = delete;
    InterfaceFactory()  = delete;

    template <typename Interface, typename PmrAllocator, typename... Args>
    CETL_NODISCARD static InterfacePtr<Interface> make_unique(PmrAllocator alloc, Args&&... args)
    {
        // Allocate memory for the concrete object.
        // Then try to construct it in-place - it could potentially throw,
        // so RAII will deallocate the memory BUT won't try to destroy the uninitialized object!
        //
        ConcreteRaii<PmrAllocator> concrete_raii{alloc};
        if (auto* const concrete_ptr = concrete_raii.get())
        {
            concrete_raii.construct(std::forward<Args>(args)...);
        }

        // Everything is good, so now we can move ownership of the concrete object to the interface smart pointer.
        //
        return InterfacePtr<Interface>{concrete_raii.release(), PmrInterfaceDeleter<Interface>{alloc, 1}};
    }

private:
    /// Helper RAII class for temporal management of allocated/initialized memory of a Concrete object.
    /// In use together with `InterfacePtr` to ensure proper deallocation in case of exceptions.
    ///
    template <typename PmrAllocator>
    class ConcreteRaii final
    {
        using Concrete = typename PmrAllocator::value_type;

    public:
        ConcreteRaii(PmrAllocator& pmr_allocator)
            : concrete_{pmr_allocator.allocate(1)}
            , constructed_{false}
            , pmr_allocator_{pmr_allocator}
        {
        }
        ~ConcreteRaii()
        {
            if (nullptr != concrete_)
            {
                if (constructed_)
                {
                    concrete_->~Concrete();
                }

                pmr_allocator_.deallocate(concrete_, 1);
            }
        }
        ConcreteRaii(const ConcreteRaii&)                = delete;
        ConcreteRaii(ConcreteRaii&&) noexcept            = delete;
        ConcreteRaii& operator=(const ConcreteRaii&)     = delete;
        ConcreteRaii& operator=(ConcreteRaii&&) noexcept = delete;

        template <typename... Args>
        void construct(Args&&... args)
        {
            CETL_DEBUG_ASSERT(!constructed_, "");

            if (nullptr != concrete_)
            {
                pmr_allocator_.construct(concrete_, std::forward<Args>(args)...);
                constructed_ = true;
            }
        }

        Concrete* get() const noexcept
        {
            return concrete_;
        }

        Concrete* release()
        {
            constructed_ = false;
            Concrete* result{nullptr};
            std::swap(result, concrete_);
            return result;
        }

    private:
        Concrete*     concrete_;
        bool          constructed_;
        PmrAllocator& pmr_allocator_;

    };  // ConcreteRaii

};  // InterfaceFactory

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_INTERFACE_PTR_H_INCLUDED
