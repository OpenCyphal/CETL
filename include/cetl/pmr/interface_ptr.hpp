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
/// Use with cetl::pmr::interface_factory for the best and safest
/// experience. Remember, be safe, use the cetl::pmr::interface_factory.
///
/// @note
/// See cetl::pmr::interface_factory for an example of how to use this type.
///
/// @tparam Interface The interface type of the polymorphic allocator to use for deallocation.
///
template <typename Interface>
class pmr_interface_deleter final
{
public:
    /// Constructs empty no-operation deleter.
    ///
    /// Useful for initially empty `interface_ptr` instance without deleter attached.
    ///
    pmr_interface_deleter() = default;

    /// Constructs a concrete type-erased deleter for the given interface type.
    ///
    /// @tparam PmrAllocator The type of the polymorphic allocator to use for deallocation.
    ///
    template <typename PmrAllocator>
    pmr_interface_deleter(PmrAllocator alloc, std::size_t obj_count)
        : deleter_{[alloc, obj_count](Interface* ptr) mutable {
            using concrete = typename PmrAllocator::value_type;

            auto* concrete_ptr = static_cast<concrete*>(ptr);
#if defined(__GNUG__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
            // https://cplusplus.github.io/LWG/issue3036 (deprecates this function)
            // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2875r4.pdf un-deprecated this function
            // whoops!
            alloc.destroy(concrete_ptr);
#if defined(__GNUG__)
#    pragma GCC diagnostic pop
#endif
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
        CETL_DEBUG_ASSERT((nullptr != ptr) == static_cast<bool>(deleter_), "Empty deleter is fine for null ptr!");

        if ((nullptr != ptr) && static_cast<bool>(deleter_))
        {
            deleter_(ptr);
        }
    }

    // Below convertor constructor is only possible with enabled PMR at `function`.
    // For now, we decided to comment it out, so that `interface_ptr` always stays
    // within 24-bytes small object optimization, namely without extra memory allocation
    // just for the sake of "advanced" deleter (actually chain of casters down to original `concrete` pointer).
    //
    // NOTE: It is possible to avoid PMR if we define a certain static limit on the maximum number of
    // down-conversions performed on an interface ptr. One issue is that exceeding that limit is a
    // runtime error, so it will have to be handled correctly somehow. It could be that std::abort()
    // is a sensible choice here, but probably not. The way to implement this solution is to keep a
    // stack of the following convertors, where each convertor reverses one up-conversion:
    //
    //          using caster_t = void* (*) (void*);
    //          /// Creates a function that performs a safe type conversion on a type-erased void* pointer.
    //          template <typename From, typename To>
    //          static caster_t make_caster() noexcept
    //          { return [](void* const ptr) -> void* { return static_cast<To*>(static_cast<From*>(ptr)); }; }
    //
    //    template <typename Down, typename = std::enable_if_t<std::is_base_of<Interface, Down>::value>>
    //    pmr_interface_deleter(const pmr_interface_deleter<Down, Pmr>& other)
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
    //    friend class pmr_interface_deleter;

private:
    cetl::pmr::function<void(Interface*), 24> deleter_;

};  // pmr_interface_deleter

template <typename Interface>
using interface_ptr = std::unique_ptr<Interface, pmr_interface_deleter<Interface>>;

/// Interface factory helper for creating objects with polymorphic allocators using proper RAII semantics.
/// Uses the cetl::pmr::pmr_interface_deleter type to ensure proper type-erased deallocation.
///
/// Example usage:
///
/// @snippet{trimleft} example_07_polymorphic_alloc_deleter.cpp example_usage_2
///
/// As shown in the next example, objects can maintain tight control of their lifecycle by befriending the allocators
/// used to create cetl::pmr::interface_ptr objects.
///
/// @snippet{trimleft} example_07_polymorphic_alloc_deleter.cpp example_usage_3
/// (@ref example_07_polymorphic_alloc_deleter "See full example here...")
///
class interface_factory final
{
public:
    ~interface_factory() = delete;
    interface_factory()  = delete;

    template <typename Interface, typename PmrAllocator, typename... Args>
    CETL_NODISCARD static interface_ptr<Interface> make_unique(PmrAllocator alloc, Args&&... args)
    {
        // Allocate memory for the concrete object.
        // Then try to construct it in-place - it could potentially throw,
        // so RAII will deallocate the memory BUT won't try to destroy the uninitialized object!
        //
        concrete_raii<PmrAllocator> raii{alloc};
        if (auto* const concrete_ptr = raii.get())
        {
            raii.construct(std::forward<Args>(args)...);
        }

        // Everything is good, so now we can move ownership of the concrete object to the interface smart pointer.
        //
        return interface_ptr<Interface>{raii.release(), pmr_interface_deleter<Interface>{alloc, 1}};
    }

private:
    /// Helper RAII class for temporal management of allocated/initialized memory of a concrete object.
    /// In use together with `interface_ptr` to ensure proper deallocation in case of exceptions.
    ///
    template <typename PmrAllocator>
    class concrete_raii final
    {
        using concrete = typename PmrAllocator::value_type;

    public:
        explicit concrete_raii(PmrAllocator& pmr_allocator)
            : concrete_{pmr_allocator.allocate(1)}
            , constructed_{false}
            , pmr_allocator_{pmr_allocator}
        {
        }
        ~concrete_raii()
        {
            if (nullptr != concrete_)
            {
                if (constructed_)
                {
#if defined(__GNUG__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#endif
                    // https://cplusplus.github.io/LWG/issue3036 (deprecates this function)
                    // https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2024/p2875r4.pdf un-deprecated this function
                    // whoops!
                    pmr_allocator_.destroy(concrete_);
                }
#if defined(__GNUG__)
#    pragma GCC diagnostic pop
#endif
                pmr_allocator_.deallocate(concrete_, 1);
            }
        }
        concrete_raii(const concrete_raii&)                = delete;
        concrete_raii(concrete_raii&&) noexcept            = delete;
        concrete_raii& operator=(const concrete_raii&)     = delete;
        concrete_raii& operator=(concrete_raii&&) noexcept = delete;

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

        concrete* get() const noexcept
        {
            return concrete_;
        }

        concrete* release()
        {
            constructed_ = false;
            concrete* result{nullptr};
            std::swap(result, concrete_);
            return result;
        }

    private:
        concrete*     concrete_;
        bool          constructed_;
        PmrAllocator& pmr_allocator_;

    };  // concrete_raii

};  // interface_factory

/// @deprecated Renamed to cetl::pmr::pmr_interface_deleter; this alias will be removed in a future major version.
template <typename Interface>
using PmrInterfaceDeleter [[deprecated("renamed to cetl::pmr::pmr_interface_deleter")]] =
    pmr_interface_deleter<Interface>;

/// @deprecated Renamed to cetl::pmr::interface_ptr; this alias will be removed in a future major version.
template <typename Interface>
using InterfacePtr [[deprecated("renamed to cetl::pmr::interface_ptr")]] = interface_ptr<Interface>;

/// @deprecated Renamed to cetl::pmr::interface_factory; this alias will be removed in a future major version.
using InterfaceFactory [[deprecated("renamed to cetl::pmr::interface_factory")]] = interface_factory;

}  // namespace pmr
}  // namespace cetl

#endif  // CETL_PMR_INTERFACE_PTR_H_INCLUDED
