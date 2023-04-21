/// @file
/// Defines the C++17 std::pmr::memory_resource abstract type and several other types within the std::pmr namespace.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETL_PF17_PMR_MEMORY_RESOURCE_H_INCLUDED
#define CETL_PF17_PMR_MEMORY_RESOURCE_H_INCLUDED

#include <algorithm>  // for std::max
#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <tuple>
#include <utility>  // for std::pair

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#    include "cetl/pf17/byte.hpp"
#endif

namespace cetl
{
namespace pf17
{
namespace pmr
{

//  +--[mem.res]--------------------------------------------------------------+

/// Interface to a class that manages memory resources.
/// This implementation Adheres to memory_resource as defined by the C++17 specification. It does not
/// incorporate changes made to this type in C++20.
class memory_resource
{
public:
    /// Implicitly defined default constructor.
    memory_resource() = default;

    /// Implicitly defined copy constructor.
    /// @param  rhs  The object to copy from.
    memory_resource(const memory_resource& rhs) = default;

    /// Implicitly defined destructor.
    virtual ~memory_resource() = default;

    /// Implicitly defined assignment operator.
    /// @param rhs The object to assign from.
    /// @return A reference to the assigned-to object (i.e. `*this`).
    memory_resource& operator=(const memory_resource& rhs) = default;

    /// Allocate memory.
    /// Allocates at least `size_bytes` of memory aligned to `alignment` by calling the template method `do_allocate`.
    ///
    /// @par Example
    /// Because C++14 does not provide portable, low-level memory allocators that support arbitrary alignment the
    /// ability for CETL to support over-alignment is limited. A possible implementation is proposed here to
    /// demonstrate:
    /// @snippet{trimleft} example_03_memory_resource.cpp do_allocate
    ///
    /// @param size_bytes   The number of bytes to allocate. Implementations may allocate additional bytes but shall
    ///                     not allocate fewer. Callers should always assume that a successful call has allocated
    ///                     exactly the requested number of bytes.
    /// @param alignment    The alignment of the allocated memory. CETL will treat an inability to properly align
    ///                     memory as an allocation failure.
    /// @return The allocated memory or std::nullptr if exceptions are disabled and an allocation failure occurs.
    /// @throws std::bad_alloc Throws if any allocation errors occur. Implementations shall not leak memory even if
    ///                        the allocation fails.
    void* allocate(std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        return do_allocate(size_bytes, alignment);
    }

    /// Deallocate memory previous allocated by this class.
    /// Deallocates memory by calling the template method `do_deallocate`. While the specification sets a
    /// precondition that p was returned by a call to cetl::pf17::pmr::memory_resource::allocate with the same
    /// size_bytes and alignment it does not require any errors to be raised if any of the inputs are invalid.
    ///
    /// @par Example
    /// See cetl::pf17::pmr::memory_resource::allocate for the first part of this example and a discussion of the
    /// limitations of CETL's implementations.
    /// @snippet{trimleft} example_03_memory_resource.cpp do_deallocate
    ///
    /// @param p            A pointer returned by cetl::pf17::pmr::memory_resource::allocate.
    /// @param size_bytes   The size passed into cetl::pf17::pmr::memory_resource::allocate.
    /// @param alignment    The alignment passed into cetl::pf17::pmr::memory_resource::allocate.
    /// @throws nothing (but is not `noexcept`).
    void deallocate(void* p, std::size_t size_bytes, std::size_t alignment = alignof(std::max_align_t))
    {
        return do_deallocate(p, size_bytes, alignment);
    }

    /// Compares two memory_resource implementations by calling template method `do_is_equal`.
    /// @param rhs The memory_resource to compare.
    /// @return `true` if memory allocated from this object can be deallocated by `rhs` and vice-versa.
    bool is_equal(const memory_resource& rhs) const noexcept
    {
        return do_is_equal(rhs);
    }

private:
    /// Template method to implement in terms of cetl::pf17::pmr::memory_resource::allocate.
    /// @param size_bytes See cetl::pf17::pmr::memory_resource::allocate.
    /// @param alignment See cetl::pf17::pmr::memory_resource::allocate.
    /// @return See cetl::pf17::pmr::memory_resource::allocate.
    /// @throws See cetl::pf17::pmr::memory_resource::allocate.
    virtual void* do_allocate(std::size_t size_bytes, std::size_t alignment) = 0;

    /// Template method to implement in terms of cetl::pf17::pmr::memory_resource::deallocate.
    /// @param p See cetl::pf17::pmr::memory_resource::deallocate.
    /// @param size_bytes See cetl::pf17::pmr::memory_resource::deallocate.
    /// @param alignment See cetl::pf17::pmr::memory_resource::deallocate.
    /// @throws See cetl::pf17::pmr::memory_resource::deallocate.
    virtual void do_deallocate(void* p, std::size_t size_bytes, std::size_t alignment) = 0;

    /// Template method to implement in terms of cetl::pf17::pmr::memory_resource::is_equal.
    /// @param rhs See cetl::pf17::pmr::memory_resource::is_equal.
    /// @return See cetl::pf17::pmr::memory_resource::is_equal
    virtual bool do_is_equal(const memory_resource& rhs) const noexcept = 0;
};

inline bool operator==(const memory_resource& lhs, const memory_resource& rhs) noexcept
{
    return &lhs == &rhs || lhs.is_equal(rhs);
}

inline bool operator!=(const memory_resource& lhs, const memory_resource& rhs) noexcept
{
    return !(lhs == rhs);
}

//  +--[mem.res.global]-------------------------------------------------------+

/// Adheres to the null_memory_resource specification.
/// Note that the C++ specification [dcl.inline] allows non-static inline functions in headers to define function-scoped
/// static variables since 1, the inline specifier has no effect on the linkage of the function and 2, the ODR requires
/// that inline functions with external linkage have the same memory address in all translation units. Furthermore
/// the specification states "A static local variable in an inline function with external or module linkage always
/// refers to the same object."
///
/// @return Pointer to a static memory_resource object (i.e. a singleton).
inline memory_resource* null_memory_resource() noexcept
{
    class cetl_null_memory_resource_impl : public memory_resource
    {
    protected:
        void* do_allocate(std::size_t size_bytes, std::size_t alignment) override
        {
            (void) size_bytes;
            (void) alignment;
#if __cpp_exceptions
            throw std::bad_alloc();
#endif
            // This is not defined by the specification but should be reasonably
            // expected given the behaviour of the new operator when exceptions
            // are disabled.
            // Section 6.7.5.5.2 of ISO/IEC 14882 states:
            //
            // > An allocation function that has a non-throwing exception specification (14.5) indicates
            // > failure by returning a null pointer value.
            //
            // We interpret this this to mean that, when all exceptions are disabled, every method implicitly
            // has a non-throwing exception specification.
            return nullptr;
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
    };

    static cetl_null_memory_resource_impl singleton;
    return &singleton;
}

//  +--[mem.poly.allocator.class]---------------------------------------------+
/// @tparam T
template <typename T>
class polymorphic_allocator
{
    /// @brief See https://en.cppreference.com/w/cpp/memory/uses_allocator#Uses-allocator_construction for this
    /// protocol.

    template <typename U, typename... Args>
    struct is_leading_allocator_constructible
    {
        static constexpr bool value =
            std::uses_allocator<U, polymorphic_allocator<T>>::value &&
            std::is_constructible<U, std::allocator_arg_t, const polymorphic_allocator<T>&, Args...>::value;
    };
    template <typename U, typename... Args>
    struct is_trailing_allocator_constructible
    {
        static constexpr bool value = std::uses_allocator<U, polymorphic_allocator<T>>::value &&
                                      std::is_constructible<U, Args..., const polymorphic_allocator<T>&>::value;
    };

    template <typename U, typename... Args>
    struct is_not_allocator_constructable
    {
        static constexpr bool value = !std::uses_allocator<U, polymorphic_allocator<U>>::value;
    };

    template <typename U, typename Enable = void>
    struct EnableIfNotPair
    {
        using return_type = void;
    };

    template <typename U>
    struct EnableIfNotPair<
        U,
        typename std::enable_if<
            std::is_base_of<std::pair<typename U::first_type, typename U::second_type>, U>::value>::type>
    {};

    template <typename PairType, typename... PTArgsT>
    typename std::enable_if<is_not_allocator_constructable<PairType, PTArgsT...>::value, std::tuple<PTArgsT&&...>>::type
    make_pair_member_args(std::tuple<PTArgsT...>& pmArgs)
    {
        return {std::move(pmArgs)};
    }

    template <typename PairType, typename... PTArgsT>
    typename std::enable_if<is_leading_allocator_constructible<PairType, PTArgsT...>::value,
                            std::tuple<std::allocator_arg_t, polymorphic_allocator, PTArgsT&&...>>::type
    make_pair_member_args(std::tuple<PTArgsT...>& pmArgs)
    {
        // https://cplusplus.github.io/LWG/issue2969
        return std::tuple_cat(std::make_tuple(std::allocator_arg, *this), std::move(pmArgs));
    }

    template <typename PairType, typename... PTArgsT>
    typename std::enable_if<is_trailing_allocator_constructible<PairType, PTArgsT...>::value,
                            std::tuple<PTArgsT&&..., polymorphic_allocator>>::type
    make_pair_member_args(std::tuple<PTArgsT...>& pmArgs)
    {
        // https://cplusplus.github.io/LWG/issue2969
        return std::tuple_cat(std::move(pmArgs), std::make_tuple(*this));
    }

    template <typename U, typename... UArgsT>
    typename std::enable_if<is_not_allocator_constructable<U, UArgsT...>::value>::type construct_not_pair_impl(
        U* p,
        UArgsT&&... uArgs)
    {
        new (p) U(std::forward<UArgsT>(uArgs)...);
    }

    template <typename U, typename... UArgsT>
    typename std::enable_if<is_leading_allocator_constructible<U, UArgsT...>::value>::type construct_not_pair_impl(
        U* p,
        UArgsT&&... uArgs)
    {
        // https://cplusplus.github.io/LWG/issue2969
        new (p) U(std::allocator_arg, *this, std::forward<UArgsT>(uArgs)...);
    }

    template <typename U, typename... UArgsT>
    typename std::enable_if<is_trailing_allocator_constructible<U, UArgsT...>::value>::type construct_not_pair_impl(
        U* p,
        UArgsT&&... uArgs)
    {
        // https://cplusplus.github.io/LWG/issue2969
        new (p) U(std::forward<UArgsT>(uArgs)..., *this);
    }

public:
    using value_type = T;

    /// @brief CETL diverges from the c++ standard here.
    /// By default, the C++ standard provides a new_delete_resource. Because C++14 did not provide standardized support
    /// for arbitrary alignment of memory allocations it would inflate CETL significantly to support this functionality.
    /// Instead, CETL declines to provide default resources and requires explicit resources are provided
    /// in types that would otherwise use a silent default.
    ///
    polymorphic_allocator() = delete;

    polymorphic_allocator(memory_resource* r)
        : memory_resource_(r)
    {
        CETL_DEBUG_ASSERT(nullptr != r,
                          "Passing a null memory_resource to polymorphic_allocator is undefined per the C++ "
                          "specification.");
    }

    polymorphic_allocator(const polymorphic_allocator&)            = default;
    polymorphic_allocator& operator=(const polymorphic_allocator&) = delete;

    template <class U>
    polymorphic_allocator(const polymorphic_allocator<U>& rhs) noexcept
        : memory_resource_(rhs.memory_resource_)
    {
    }

    T* allocate(std::size_t object_count)
    {
        if (std::numeric_limits<std::size_t>::max() / sizeof(T) < object_count)
        {
#if __cpp_exceptions
            // Per the specification, if memory needed to create object_count objects exceeds
            // the ability to address this memory using std::size_t then throw bad_array_new_length;
            throw std::bad_array_new_length();
#else
            return nullptr;
#endif
        }
        return static_cast<T*>(memory_resource_->allocate(object_count * sizeof(T), alignof(T)));
    }

    void deallocate(T* p, size_t object_count)
    {
        memory_resource_->deallocate(p, sizeof(T) * object_count, alignof(T));
    }

    memory_resource* resource() const
    {
        return memory_resource_;
    }

    /// Constructs an object U in the storage p with the given arguments following the uses-allocator protocol
    /// for constructors.
    ///
    /// See std::uses_allocator_construction_args for a C++20 helper and [allocator.uses.construction] in the C++
    /// specification for a discussion of this protocol.
    ///
    /// This signature participates in overload resolution only if U is not an std::pair and if U does not
    /// use allocator construction.
    ///
    /// This method only throws if the constructor of U throws.
    ///
    /// @tparam U       The type to construct.
    /// @tparam ...Args The type of arguments to forward to the constructor of U.
    /// @param p        The memory to construct an instance of U within.
    /// @param ...args  The argument to forward to the constructor of U.
    /// @return void The type is constructed in-place if U is not an std::pair.
    template <typename U, typename... Args>
    typename EnableIfNotPair<U>::return_type construct(U* p, Args&&... args)
    {
        construct_not_pair_impl(p, args...);
    }

    template <class FirstType, class SecondType, class... FirstTypeConstructorArgs, class... SecondTypeConstructorArgs>
    void construct(std::pair<FirstType, SecondType>* p,
                   std::piecewise_construct_t,
                   std::tuple<FirstTypeConstructorArgs...>  x,
                   std::tuple<SecondTypeConstructorArgs...> y)
    {
        new (p) std::pair<FirstType, SecondType>{std::piecewise_construct,
                                                 make_pair_member_args<FirstType, FirstTypeConstructorArgs...>(x),
                                                 make_pair_member_args<SecondType, SecondTypeConstructorArgs...>(y)};
    }

    template <class FirstType, class SecondType>
    void construct(std::pair<FirstType, SecondType>* p)
    {
        construct(p, std::piecewise_construct, std::tuple<>(), std::tuple<>());
    }

    template <class FirstType, class SecondType, class FirstTypeArgType, class SecondTypeArgType>
    void construct(std::pair<FirstType, SecondType>* p, FirstTypeArgType&& x, SecondTypeArgType&& y)
    {
        construct(p,
                  std::piecewise_construct,
                  std::forward_as_tuple(std::forward<FirstTypeArgType>(x)),
                  std::forward_as_tuple(std::forward<SecondTypeArgType>(y)));
    }

    template <class FirstType, class SecondType, class CopyFromFirstType, class CopyFromSecondType>
    void construct(std::pair<FirstType, SecondType>*                       p,
                   const std::pair<CopyFromFirstType, CopyFromSecondType>& copyFrom)
    {
        construct(p,
                  std::piecewise_construct,
                  std::forward_as_tuple(copyFrom.first),
                  std::forward_as_tuple(copyFrom.second));
    }

    template <class FirstType, class SecondType, class MoveFromFirstType, class MoveFromSecondType>
    void construct(std::pair<FirstType, FirstType>* p, std::pair<MoveFromFirstType, MoveFromSecondType>&& moveFrom)
    {
        construct(p,
                  std::piecewise_construct,
                  std::forward_as_tuple(std::forward<MoveFromFirstType>(moveFrom.first)),
                  std::forward_as_tuple(std::forward<MoveFromSecondType>(moveFrom.second)));
    }

    // https://cplusplus.github.io/LWG/issue3036 (deprecates this function)
    template <typename U>
    void destroy(U* p)
    {
        if (nullptr != p)
        {
            p->~U();
        }
    }

    polymorphic_allocator select_on_container_copy_construction() const
    {
        return polymorphic_allocator(resource());
    }

private:
    memory_resource* memory_resource_;
};

template <class LHT, class RHT>
bool operator==(const polymorphic_allocator<LHT>& lhs, const polymorphic_allocator<RHT>& rhs) noexcept
{
    return *lhs.resource() == *rhs.resource();
}

template <class LHT, class RHT>
bool operator!=(const polymorphic_allocator<LHT>& lhs, const polymorphic_allocator<RHT>& rhs) noexcept
{
    return !(lhs == rhs);
}

//  +--[mem.res.monotonic.buffer]---------------------------------------------+

namespace deviant
{

/// Adheres to the C++17 std::pmr::monotonic_buffer_resource specification except that it does not provide
/// default upstream constructors and therefore has no dependencies to types defined in
/// cetl/pf17/sys/memory_resource.hpp.
class basic_monotonic_buffer_resource : public memory_resource
{
public:
    basic_monotonic_buffer_resource(void* buffer, size_t buffer_size, memory_resource* upstream)
        : first_buffer_control_{buffer, buffer_size, 0, buffer_size, nullptr}
        , upstream_{upstream}
        , current_buffer_{&first_buffer_control_}
    {
    }

    basic_monotonic_buffer_resource(std::size_t initial_size, memory_resource* upstream)
        : basic_monotonic_buffer_resource(nullptr, initial_size, upstream)
    {
    }

    explicit basic_monotonic_buffer_resource(memory_resource* upstream)
        : basic_monotonic_buffer_resource(0, upstream)
    {
    }

    virtual ~basic_monotonic_buffer_resource()
    {
        release();
    }

    basic_monotonic_buffer_resource(const basic_monotonic_buffer_resource&)            = delete;
    basic_monotonic_buffer_resource& operator=(const basic_monotonic_buffer_resource&) = delete;

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
}  // namespace deviant
}  // namespace pmr
}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_PMR_MEMORY_RESOURCE_H_INCLUDED
