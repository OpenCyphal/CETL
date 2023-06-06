/// @file
/// Includes cetl::VariableLengthArray type and non-member functions.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// cSpell:ignore cend cbegin rnext rend lnext lbegin pocca pocma

#ifndef CETL_VARIABLE_LENGTH_ARRAY_HPP_INCLUDED
#define CETL_VARIABLE_LENGTH_ARRAY_HPP_INCLUDED

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <limits>
#include <memory>
#include <type_traits>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

static_assert(
    __cplusplus >= 201402L,
    "Unsupported language: ISO C14, C++14, or a newer version of either is required to use the built-in VLA type");

namespace cetl
{

// Common template implementation for VariableLengthArray and its specializations. This is not intended to be used
// directly.
template <typename BaseValueType, typename BaseAllocatorType>
class VariableLengthArrayBase
{
public:
    ///
    /// STL-like declaration of the container's storage type.
    ///
    using value_type = BaseValueType;

    ///
    /// Per the std::uses_allocator protocol.
    ///
    using allocator_type = typename std::allocator_traits<BaseAllocatorType>::template rebind_alloc<value_type>;

    ///
    /// STL-like declaration of the container's difference type.
    ///
    using difference_type = std::ptrdiff_t;

    ///
    /// STL-like declaration of the container's size type.
    ///
    using size_type = std::size_t;

    constexpr VariableLengthArrayBase(
        const allocator_type& alloc,
        value_type*           data,
        size_type             initial_capacity,
        size_type             size,
        size_type             max_size_max) noexcept(std::is_nothrow_copy_constructible<allocator_type>::value)
        : alloc_(alloc)
        , data_(data)
        , capacity_(initial_capacity)
        , size_(size)
        , max_size_max_(max_size_max)
    {
    }

    constexpr VariableLengthArrayBase(const VariableLengthArrayBase& rhs, const allocator_type& rhs_alloc) noexcept(
        std::is_nothrow_copy_constructible<allocator_type>::value)
        : alloc_(std::allocator_traits<allocator_type>::select_on_container_copy_construction(rhs_alloc))
        , data_(nullptr)
        , capacity_(0)
        , size_(0)
        , max_size_max_(rhs.max_size_max_)
    {
    }

    constexpr VariableLengthArrayBase(VariableLengthArrayBase&& rhs ) noexcept
        : alloc_(std::move(rhs.alloc_))
        , data_(std::move(rhs.data_))
        , capacity_(rhs.capacity_)
        , size_(rhs.size_)
        , max_size_max_(rhs.max_size_max_)
    {
        static_assert(std::is_nothrow_move_constructible<allocator_type>::value,
                      "Allocator must be nothrow move constructible.");
        rhs.size_     = 0;
        rhs.capacity_ = 0;
        rhs.data_     = nullptr;
    }

private:
    // +----------------------------------------------------------------------+
    // | TYPE HELPERS
    // +----------------------------------------------------------------------+
    // adapted from https://en.cppreference.com/w/cpp/experimental/is_detected
    // implements a C++14-compatible detection idiom.

    template <typename...>
    using _void_t = void;

    template <class AlwaysVoid, template <class...> class Op, class... Args>
    struct detector
    {
        using value_t = std::false_type;
    };

    template <template <class...> class Op, class... Args>
    struct detector<_void_t<Op<Args...>>, Op, Args...>
    {
        using value_t = std::true_type;
    };

    template <template <class...> class Op, class... Args>
    using is_detected = typename detector<void, Op, Args...>::value_t;

    // +----------------------------------------------------------------------+
    /// Check if one type can be trivially coped from another.
    template <typename DstType, typename SrcType>
    struct is_trivially_copyable_from
        : std::integral_constant<bool,
                                 std::is_trivially_copyable<DstType>::value && std::is_same<DstType, SrcType>::value>
    {};

    // +----------------------------------------------------------------------+
    // If allocator propagates on move assignment or is always equal. This
    // is used to implement parts of the propagate_on_container_move_assignment
    // named requirement for allocators.
    // See https://en.cppreference.com/w/cpp/named_req/Allocator
    template <typename UAlloc>
    struct is_pocma_or_is_always_equal
        : public std::integral_constant<
              bool,
              (std::allocator_traits<std::remove_reference_t<UAlloc>>::propagate_on_container_move_assignment::value ||
               std::allocator_traits<std::remove_reference_t<UAlloc>>::is_always_equal::value)>
    {};

    // +----------------------------------------------------------------------+
    // If allocator propagates on copy assignment or is always equal. This
    // is used to implement parts of the propagate_on_container_copy_assignment
    // named requirement for allocators.
    // See https://en.cppreference.com/w/cpp/named_req/Allocator
    template <typename UAlloc>
    struct is_pocca_or_is_always_equal
        : public std::integral_constant<
              bool,
              (std::allocator_traits<std::remove_reference_t<UAlloc>>::propagate_on_container_copy_assignment::value ||
               std::allocator_traits<std::remove_reference_t<UAlloc>>::is_always_equal::value)>
    {};

protected:
    // +----------------------------------------------------------------------+
    // | Allocator move assignment
    // +----------------------------------------------------------------------+
    template <typename UAlloc>
    constexpr bool move_assign_alloc(
        UAlloc&& rhs,
        typename std::enable_if<std::allocator_traits<
            std::remove_reference_t<UAlloc>>::propagate_on_container_move_assignment::value>::type* =
            nullptr) noexcept(is_pocma_or_is_always_equal<UAlloc>::value)
    {
        static_assert(std::is_nothrow_move_assignable<allocator_type>::value,
                      "The C++ standard requires anything adhering to propagate_on_container_move_assignment to copy "
                      "without throwing.");
        alloc_ = std::forward<UAlloc>(rhs);
        return true;
    }

    template <typename UAlloc>
    constexpr bool move_assign_alloc(
        UAlloc&& rhs,
        typename std::enable_if<!std::allocator_traits<
            std::remove_reference_t<UAlloc>>::propagate_on_container_move_assignment::value>::type* =
            nullptr) noexcept(is_pocma_or_is_always_equal<UAlloc>::value)
    {
        // Cannot move assign so we have to do everything manually.
        (void) rhs;
        return false;
    }

    // +----------------------------------------------------------------------+
    // | Allocator copy assignment
    // +----------------------------------------------------------------------+
    template <typename UAlloc>
    constexpr bool copy_assign_alloc(
        const UAlloc& rhs,
        typename std::enable_if<std::allocator_traits<UAlloc>::propagate_on_container_copy_assignment::value>::type* =
            nullptr) noexcept
    {
        static_assert(std::is_nothrow_copy_assignable<allocator_type>::value,
                      "The C++ standard requires anything adhering to propagate_on_container_copy_assignment to copy "
                      "without throwing.");
        alloc_ = rhs;
        return true;
    }

    template <typename UAlloc>
    constexpr bool copy_assign_alloc(
        const UAlloc& rhs,
        typename std::enable_if<!std::allocator_traits<UAlloc>::propagate_on_container_copy_assignment::value>::type* =
            nullptr) noexcept
    {
        (void) rhs;
        return false;
    }

    // +----------------------------------------------------------------------+
    // | reallocation support
    // +----------------------------------------------------------------------+

    template <typename U>
    using reallocate_operation =
        decltype(std::declval<U>().reallocate(std::declval<value_type*>(), std::size_t(), std::size_t()));

    template <typename UAllocator>
    static constexpr typename std::enable_if<is_detected<reallocate_operation, UAllocator>::value, value_type>::type*
    reallocate(value_type* data, UAllocator& alloc, std::size_t old_object_count, std::size_t new_object_count)
    {
        return alloc.reallocate(data, old_object_count, new_object_count);
    }

    template <typename UAllocator>
    static constexpr typename std::enable_if<!is_detected<reallocate_operation, UAllocator>::value, value_type>::type*
    reallocate(value_type* data, UAllocator& alloc, std::size_t old_object_count, std::size_t new_object_count)
    {
        (void) data;
        (void) alloc;
        (void) old_object_count;
        (void) new_object_count;
        return nullptr;
    }

    // +----------------------------------------------------------------------+
    // | destruction support
    // +----------------------------------------------------------------------+
    ///
    /// If trivially destructible then we don't have to call the destructors and this is an no-op.
    ///
    template <typename U>
    static constexpr void fast_destroy(
        U* const        src,
        const size_type src_size_count,
        typename std::enable_if_t<std::is_trivially_destructible<U>::value>* = nullptr) noexcept
    {
        (void) src;
        (void) src_size_count;
    }

    ///
    /// If not trivially destructible then we invoke each destructor.
    ///
    template <typename U>
    static constexpr void fast_destroy(U* const        src,
                                       const size_type src_size_count,
                                       typename std::enable_if_t<!std::is_trivially_destructible<U>::value>* =
                                           nullptr) noexcept(std::is_nothrow_destructible<U>::value)
    {
        size_type dtor_iterator = src_size_count;
        while (dtor_iterator > 0)
        {
            src[--dtor_iterator].~U();
        }
    }
    // +----------------------------------------------------------------------+
    // | deallocation support
    // +----------------------------------------------------------------------+
    ///
    /// Invokes fast_destroy then deallocates the memory.
    ///
    template <typename U>
    static constexpr void fast_deallocate(
        U* const        src,
        const size_type src_size_count,
        const size_type src_capacity_count,
        allocator_type& alloc,
        typename std::enable_if_t<std::is_trivially_destructible<U>::value>* = nullptr)
    {
        fast_destroy(src, src_size_count);
        alloc.deallocate(src, src_capacity_count);
    }

    ///
    /// Invokes fast_destroy then deallocates the memory.
    ///
    template <typename U>
    static constexpr void fast_deallocate(
        U* const        src,
        const size_type src_size_count,
        const size_type src_capacity_count,
        allocator_type& alloc,
        typename std::enable_if_t<!std::is_trivially_destructible<U>::value>* = nullptr)
    {
        fast_destroy(src, src_size_count);
        alloc.deallocate(src, src_capacity_count);
    }

    // +----------------------------------------------------------------------+
    // | COPY ASSIGNMENT
    // +----------------------------------------------------------------------+
    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename DstType, typename SrcType>
    static constexpr size_type fast_copy_assign(
        DstType* const       dst,
        size_type            dst_capacity_count,
        const SrcType* const src,
        size_type            src_len_count,
        typename std::enable_if_t<is_trivially_copyable_from<DstType, SrcType>::value>* = nullptr) noexcept
    {
        if (nullptr == dst || nullptr == src)
        {
            return 0;
        }
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        if (max_copy_size > 0)
        {
            (void) std::memcpy(dst, src, max_copy_size * sizeof(DstType));
        }
        return max_copy_size;
    }

    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename DstType, typename SrcType>
    static constexpr size_type fast_copy_assign(
        DstType* const       dst,
        size_type            dst_capacity_count,
        const SrcType* const src,
        size_type            src_len_count,
        typename std::enable_if_t<!is_trivially_copyable_from<DstType, SrcType>::value>* =
            nullptr) noexcept(noexcept(std::allocator_traits<allocator_type>::
                                           construct(std::declval<std::add_lvalue_reference_t<allocator_type>>(),
                                                     std::declval<std::add_pointer_t<DstType>>(),
                                                     std::declval<std::add_lvalue_reference_t<const SrcType>>())))
    {
        if (nullptr == dst || nullptr == src)
        {
            return 0;
        }
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        for (size_type i = 0; i < max_copy_size; ++i)
        {
            dst[i] = src[i];
        }
        return max_copy_size;
    }

    // +----------------------------------------------------------------------+
    // | COPY CONSTRUCTION
    // +----------------------------------------------------------------------+

    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename DstType, typename SrcType>
    static constexpr size_type fast_copy_construct(
        DstType* const       dst,
        size_type            dst_capacity_count,
        const SrcType* const src,
        size_type            src_len_count,
        allocator_type&      alloc,
        typename std::enable_if_t<is_trivially_copyable_from<DstType, SrcType>::value>* = nullptr) noexcept
    {
        (void) alloc;
        // for trivial copyable assignment is the same as construction:
        return fast_copy_assign(dst, dst_capacity_count, src, src_len_count);
    }

    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename DstType, typename SrcType>
    static constexpr size_type fast_copy_construct(
        DstType* const       dst,
        size_type            dst_capacity_count,
        const SrcType* const src,
        size_type            src_len_count,
        allocator_type&      alloc,
        typename std::enable_if_t<!is_trivially_copyable_from<DstType, SrcType>::value>* =
            nullptr) noexcept(noexcept(std::allocator_traits<allocator_type>::
                                           construct(std::declval<std::add_lvalue_reference_t<allocator_type>>(),
                                                     std::declval<std::add_pointer_t<DstType>>(),
                                                     std::declval<std::add_lvalue_reference_t<const SrcType>>())))
    {
        if (nullptr == dst || nullptr == src)
        {
            return 0;
        }
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        for (size_type i = 0; i < max_copy_size; ++i)
        {
            std::allocator_traits<allocator_type>::construct(alloc, &dst[i], src[i]);
        }
        return max_copy_size;
    }

    // +----------------------------------------------------------------------+
    // | FORWARD CONSTRUCTION
    // +----------------------------------------------------------------------+
    template <typename DstType, typename SrcType>
    static constexpr size_type fast_forward_construct(
        DstType* const  dst,
        size_type       dst_capacity_count,
        SrcType* const  src,
        size_type       src_len_count,
        allocator_type& alloc,
        typename std::enable_if_t<is_trivially_copyable_from<DstType, SrcType>::value>* = nullptr)
    {
        (void) alloc;
        // for trivial copyable assignment move is the same as copy:
        return fast_copy_assign(dst, dst_capacity_count, src, src_len_count);
    }

    template <typename DstType, typename SrcType>
    static constexpr size_type fast_forward_construct(
        DstType* const  dst,
        size_type       dst_capacity_count,
        SrcType* const  src,
        size_type       src_len_count,
        allocator_type& alloc,
        typename std::enable_if_t<!is_trivially_copyable_from<DstType, SrcType>::value>* = nullptr)
    {
        if (nullptr == dst || nullptr == src)
        {
            return 0;
        }
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        for (size_type i = 0; i < max_copy_size; ++i)
        {
            std::allocator_traits<allocator_type>::construct(alloc, &dst[i], std::move_if_noexcept(src[i]));
        }
        return max_copy_size;
    }

    // +----------------------------------------------------------------------+
    // | FORWARD ASSIGNMENT
    // +----------------------------------------------------------------------+
    template <typename DstType, typename SrcType>
    static constexpr size_type fast_forward_assign(
        DstType* const dst,
        size_type      dst_capacity_count,
        SrcType* const src,
        size_type      src_len_count,
        typename std::enable_if_t<is_trivially_copyable_from<DstType, SrcType>::value>* = nullptr)
    {
        // for trivial copyable move and copy is the same.
        return fast_copy_assign(dst, dst_capacity_count, src, src_len_count);
    }

    template <typename DstType, typename SrcType>
    static constexpr size_type fast_forward_assign(
        DstType* const dst,
        size_type      dst_capacity_count,
        SrcType* const src,
        size_type      src_len_count,
        typename std::enable_if_t<!is_trivially_copyable_from<DstType, SrcType>::value>* = nullptr)
    {
        if (nullptr == dst || nullptr == src)
        {
            return 0;
        }
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        for (size_type i = 0; i < max_copy_size; ++i)
        {
            dst[i] = std::move_if_noexcept(src[i]);
        }
        return max_copy_size;
    }

    // +----------------------------------------------------------------------+
    // | COPY ASSIGN IMPLEMENTATION
    // +----------------------------------------------------------------------+
    constexpr void copy_assign_from(const VariableLengthArrayBase& rhs, const size_type rhs_max_size)
    {
        if (this == &rhs)
        {
            return;
        }
        max_size_max_ = rhs.max_size_max_;
        if ((std::allocator_traits<allocator_type>::is_always_equal::value || alloc_ == rhs.alloc_ ||
             !std::allocator_traits<allocator_type>::propagate_on_container_copy_assignment::value) &&
            rhs.size_ <= capacity_)
        {
            // First off, we're here because the incoming data will fit within our existing capacity.

            // Next we are here because either the allocators are equal or we are not going to adopt the incoming
            // allocator.

            // If we are not adopting the incoming allocator then the next statement will be a no-op.
            copy_assign_alloc(rhs.alloc_);

            // Let's go ahead and assign-copy what we can.
            const std::size_t overlap = fast_copy_assign(data_, size_, rhs.data_, rhs.size_);
            if (rhs.size_ <= size_)
            {
                // if there's less elements in rhs than in this container, destroy the remaining elements.
                fast_destroy(&data_[overlap], size_ - overlap);
            }
            else
            {
                // If there's more elements in rhs than in this container. Construct the remaining elements.
                fast_copy_construct(&data_[overlap],
                                    capacity_ - overlap,
                                    &rhs.data_[overlap],
                                    rhs.size_ - overlap,
                                    alloc_);
            }
            size_ = rhs.size_;
        }
        else
        {
            // If the incoming data doesn't fit in our capacity we have to resize. It doesn't make any sense, until
            // we support realloc here (TODO: support realloc here), to move the existing data with the resize since
            // we're going to be copying over it right away. That means the most efficient procedure at this point
            // is to simply discard everything on the lhs, reserve enough memory for the rhs, and copy everything
            // over in one go.

            fast_deallocate(data_, size_, capacity_, alloc_);
            data_                    = nullptr;
            capacity_                = 0;
            size_                    = 0;
            const size_type new_size = rhs.size_;

            // Also note that it doesn't matter if the allocators are equal or not since we just delete first using the
            // current allocator, either adopt or don't adopt the rhs allocator (based on SFINAE of copy_assign_alloc),
            // then allocate using the new (or not) allocator. If they are equal then this is all the same thing.
            // If they are not then we did it right anyway. All hail TMP!
            copy_assign_alloc(rhs.alloc_);
            reserve(new_size, rhs_max_size);
            size_ = fast_copy_construct(data_, capacity_, rhs.data_, new_size, alloc_);
        }
    }

    // +----------------------------------------------------------------------+
    // | MOVE ASSIGN IMPLEMENTATION
    // +----------------------------------------------------------------------+

    ///
    /// This version of move assign is an optimization. It compiles in the simplest possible runtime for moving from
    /// one container to the other. All other implementations require additional checks and branches to perform the
    /// move.
    template <typename UAlloc>
    constexpr void move_assign_from(
        VariableLengthArrayBase&& rhs,
        const size_type           rhs_max_size,
        typename std::enable_if<is_pocma_or_is_always_equal<UAlloc>::value>::type* = nullptr) noexcept
    {
        (void) rhs_max_size;

        if (this == &rhs)
        {
            return;
        }

        fast_deallocate(data_, size_, capacity_, alloc_);
        move_assign_alloc(rhs.alloc_);

        max_size_max_ = rhs.max_size_max_;
        capacity_     = rhs.capacity_;
        size_         = rhs.size_;
        data_         = rhs.data_;
        rhs.size_     = 0;
        rhs.data_     = nullptr;
        rhs.capacity_ = 0;
    }

    template <typename UAlloc>
    constexpr void move_assign_from(
        VariableLengthArrayBase&& rhs,
        const size_type           rhs_max_size,
        typename std::enable_if<!is_pocma_or_is_always_equal<UAlloc>::value>::type* = nullptr)
    {
        if (this == &rhs)
        {
            return;
        }

        // can't move the allocator and they aren't always the same. we have to
        // move assign into new memory.
        if (alloc_ == rhs.alloc_)
        {
            // Even though the two allocators aren't always the same they are this time. We can simply take ownership
            // of the incoming memory even if we don't adopt the rhs allocator (SFINAE takes care of that in the
            // move_assign_alloc method).
            fast_deallocate(data_, size_, capacity_, alloc_);
            move_assign_alloc(rhs.alloc_);

            max_size_max_ = rhs.max_size_max_;
            capacity_     = rhs.capacity_;
            size_         = rhs.size_;
            data_         = rhs.data_;
            rhs.size_     = 0;
            rhs.data_     = nullptr;
            rhs.capacity_ = 0;
        }
        else
        {
            if (rhs.size_ <= capacity_)
            {
                // We're here because the incoming data will fit within our existing capacity and because we can't
                // just move the memory from the rhs allocator since that allocator is neither equal to our own nor are
                // we adopting it.

                // if POCMA is false then this is a no-op.
                move_assign_alloc(rhs.alloc_);

                // Let's go ahead and move-assign what we can.
                const std::size_t overlap = fast_forward_assign(data_, size_, rhs.data_, rhs.size_);
                if (rhs.size_ <= size_)
                {
                    // if there's fewer elements in rhs than in this container, destroy the remaining elements.
                    fast_destroy(&data_[overlap], size_ - overlap);
                }
                else
                {
                    // If there's more elements in rhs than in this container. Construct the remaining elements.
                    fast_forward_construct(&data_[overlap],
                                           capacity_ - overlap,
                                           &rhs.data_[overlap],
                                           rhs.size_ - overlap,
                                           alloc_);
                }
                size_ = rhs.size_;
            }
            else
            {
                // Ah well. We have to resize our buffer anyway so there's no point in trying to save any of it.
                // Good-bye my sweet princes, my children, my items.
                // Fear not the shade, the dark deallocator, ere it looms large upon ye.
                // It is but the gateway to reallocation and reuse – the circle of our bits.
                // Transmutations that warm us.
                // Residual heat, a validation of our agency.
                fast_deallocate(data_, size_, capacity_, alloc_);
                data_     = nullptr;
                capacity_ = 0;
                size_     = 0;
                // Adieu! Adieu.
                // ...

                const size_type new_size = rhs.size_;
                move_assign_alloc(rhs.alloc_);
                reserve(new_size, rhs_max_size);
                size_ = fast_forward_construct(data_, capacity_, rhs.data_, new_size, alloc_);
                rhs.resize(0, rhs_max_size);
            }
        }
    }

    // +----------------------------------------------------------------------+
    // | Common Implementation
    // +----------------------------------------------------------------------+
    ///
    /// Ensure enough memory is allocated to store at least the `desired_capacity` number of elements.
    ///
    /// @param  desired_capacity The number of elements to allocate, but not initialize, memory for.
    /// @param  max_size         The maximum number of elements that can be stored in this container.
    ///
    constexpr void reserve(const size_type desired_capacity, const size_type max_size)
    {
        if (desired_capacity <= capacity_)
        {
            // nothing to do. Why are you here? Go away.
            return;
        }
        size_type clamped_capacity = desired_capacity;
        if (clamped_capacity > max_size)
        {
#if __cpp_exceptions
            throw std::length_error("Requested capacity exceeds maximum size.");
#else
            // deviation from the standard: instead of undefined behaviour we clamp the capacity to the maximum size
            // when exceptions are disabled.
            clamped_capacity = max_size;
#endif
        }

        const size_type no_shrink_capacity = std::max(clamped_capacity, size_);

        value_type* new_data = reallocate(data_, alloc_, capacity_, no_shrink_capacity);

        if (new_data != nullptr)
        {
            data_     = new_data;
            capacity_ = no_shrink_capacity;
        }
        else
        {
            // The allocator was unable to extend the reserved area for the same memory pointer.
            // We need to allocate a new block of memory and copy the old data into it.
            new_data = std::allocator_traits<allocator_type>::allocate(alloc_, no_shrink_capacity);
            if (nullptr != new_data)
            {
                fast_forward_construct(new_data, no_shrink_capacity, data_, size_, alloc_);
                fast_deallocate(data_, size_, capacity_, alloc_);
                data_     = new_data;
                capacity_ = no_shrink_capacity;
            }
        }
    }

    template <typename... Args>
    constexpr void resize(const size_type new_size, const size_type max_size, Args&&... args)
    {
        if (new_size == size_)
        {
            return;
        }

        if (new_size > size_)
        {
            // grow
            if (new_size > capacity_)
            {
                reserve(new_size, max_size);
            }
            for (std::size_t i = size_; i < new_size; ++i)
            {
                alloc_.construct(&data_[i], std::forward<Args>(args)...);
            }
        }
        else
        {
            // shrink
            fast_destroy(&data_[new_size], size_ - new_size);
        }
        size_ = new_size;
    }

    ///
    /// Deallocate or reallocate memory such that not more than `size()` elements can be stored in this object.
    ///
    constexpr void shrink_to_fit()
    {
        if (size_ == capacity_)
        {
            // already shrunk
            return;
        }

        // Special case where we are shrinking to empty
        if (size_ == 0)
        {
            alloc_.deallocate(data_, capacity_);
            data_     = nullptr;
            capacity_ = 0;
            return;
        }

        value_type* new_data = reallocate(data_, alloc_, capacity_, size_);

        if (nullptr != new_data)
        {
            // our allocator did the work for us, yay!
            data_     = new_data;
            capacity_ = size_;
            return;
        }

        // Allocate only enough to store what we have.
        value_type* minimized_data = alloc_.allocate(size_);

        if (minimized_data == nullptr)
        {
            return;
        }
        else
        {
            if (minimized_data != data_)
            {
                fast_forward_construct(minimized_data, size_, data_, size_, alloc_);
                fast_deallocate(data_, size_, capacity_, alloc_);
            }  // else the allocator was able to simply shrink the reserved area for the same memory pointer.
            data_     = minimized_data;
            capacity_ = size_;
        }
    }

    /// Grows the capacity of the array ensures amortized, constant-time expansion.
    constexpr bool grow(const size_type max_size)
    {
        // Simple geometric progression of capacity growth.
        const size_type capacity_before = capacity_;

        const size_type half_capacity = capacity_before / 2U;

        // That is, instead of a capacity sequence of 1, 2, 3, 4, 6, 9 we start from zero as 2, 4, 6, 9. The first
        // opportunity for reusing previously freed memory comes when increasing to 19 from 13 since E(n-1) == 21.
        const size_type new_capacity = std::min(max_size, capacity_before + ((half_capacity <= 1) ? 2 : half_capacity));

        reserve(new_capacity, max_size);

        return capacity_ > capacity_before;
    }

    // +----------------------------------------------------------------------+
    // | DATA MEMBERS
    // +----------------------------------------------------------------------+
    allocator_type alloc_;
    value_type*    data_;
    size_type      capacity_;
    size_type      size_;
    size_type      max_size_max_;
};

// +-------------------------------------------------------------------------------------------------------------------+
// | VariableLengthArray
// +-------------------------------------------------------------------------------------------------------------------+

///
/// Minimal, generic container for storing Cyphal variable-length arrays. While this type shares similarities with
/// std::vector it has several important properties that are different.
/// First, the `max_size_max` maximum bound allows the array to enforce a maximum size whether or not the allocator
/// properly implements the std::allocator_traits::max_size protocol (std::pmr::polymorphic_allocator, for example).
/// This allows use of an allocator that is backed by statically allocated memory:
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_exact_fit
///
/// Even if cetl::VariableLengthArray::reserve() is not used the implementation will still use less memory then its
/// STL counterpart:
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_tight_fit_0
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_tight_fit_1
///
/// Finally, when exceptions are disabled the cetl::VariableLengthArray will not exhibit undefined behaviour like
/// std::vector does:
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_no_exceptions
/// (@ref example_08_variable_length_array_vs_vector "See full example here...")
///
/// @tparam  T           The type of elements in the array.
/// @tparam Allocator    The allocator type to use for all allocations.
///
template <typename T, typename Allocator>
class VariableLengthArray : protected VariableLengthArrayBase<T, Allocator>
{
protected:
    using Base = VariableLengthArrayBase<T, Allocator>;
    using Base::data_;
    using Base::size_;
    using Base::capacity_;
    using Base::max_size_max_;
    using Base::alloc_;

public:
    ///
    /// STL-like declaration of the container's storage type.
    ///
    using value_type = typename Base::value_type;

    ///
    /// STL-like declaration of pointer type.
    ///
    using pointer = typename std::add_pointer_t<value_type>;

    ///
    /// STL-like declaration of the iterator type.
    ///
    using iterator = typename std::add_pointer_t<value_type>;

    ///
    /// STL-like declaration of the const iterator type.
    ///
    using const_iterator = typename std::add_pointer_t<typename std::add_const_t<value_type>>;

    ///
    /// STL-like declaration of the container's difference type.
    ///
    using difference_type = typename Base::difference_type;

    ///
    /// STL-like declaration of the container's size type.
    ///
    using size_type = typename Base::size_type;

    ///
    /// Per the std::uses_allocator protocol.
    ///
    using allocator_type = typename Base::allocator_type;

    ///
    /// STL-like declaration of reference type.
    ///
    using reference = typename std::add_lvalue_reference_t<value_type>;

    ///
    /// STL-like declaration of constant-reference type.
    ///
    using const_reference = typename std::add_const_t<std::add_lvalue_reference_t<value_type>>;

    /// Required constructor for std::uses_allocator protocol.
    /// @param alloc Allocator to use for all allocations.
    explicit VariableLengthArray(const allocator_type& alloc) noexcept
        : Base(alloc, nullptr, 0, 0, std::numeric_limits<size_type>::max())
    {
    }

    /// Required constructor for std::uses_allocator protocol.
    /// @param alloc Allocator to use for all allocations.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    /// cetl::VariableLengthArray::max_size() will return
    /// `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    explicit VariableLengthArray(const allocator_type& alloc, size_type max_size_max) noexcept
        : Base(alloc, nullptr, 0, 0, max_size_max)
    {
    }

    /// Initializer syntax constructor.
    /// @param l Initializer list of elements to copy into the array.
    /// @param alloc Allocator to use for all allocations.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    /// cetl::VariableLengthArray::max_size() will return
    /// `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    VariableLengthArray(std::initializer_list<value_type> l,
                        const allocator_type&             alloc,
                        size_type                         max_size_max = std::numeric_limits<size_type>::max())
        : Base(alloc, nullptr, 0, 0, max_size_max)
    {
        reserve(l.size());
        size_ = Base::fast_copy_construct(data_, capacity_, l.begin(), l.size(), alloc_);
    }

    /// Range constructor.
    /// @tparam InputIt The type of the range's iterators.
    /// @param first    The beginning of the range.
    /// @param last     The end of the range.
    /// @param alloc    Allocator to use for all allocations.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    /// cetl::VariableLengthArray::max_size() will return `std::min(max_size_max,
    /// std::allocator_traits<allocator_type>::max_size(alloc))`
    template <class InputIt>
    VariableLengthArray(InputIt               first,
                        InputIt               last,
                        const allocator_type& alloc,
                        size_type             max_size_max = std::numeric_limits<size_type>::max())
        : Base(alloc, nullptr, 0, 0, max_size_max)
    {
        if (last >= first)
        {
            reserve(static_cast<std::size_t>(last - first));
            if (nullptr != data_)
            {
                size_ =
                    Base::fast_copy_construct(data_, capacity_, first, static_cast<std::size_t>(last - first), alloc_);
            }
        }
    }

    //
    // Rule of Five.
    //
    VariableLengthArray(const VariableLengthArray& rhs, const allocator_type& alloc)
        : Base(rhs, alloc)
    {
        Base::reserve(rhs.size(), rhs.max_size());
        size_ = Base::fast_copy_construct(data_, capacity_, rhs.data_, rhs.size_, alloc_);
    }

    VariableLengthArray(const VariableLengthArray& rhs)
        : VariableLengthArray(rhs, rhs.alloc_)
    {
    }

    VariableLengthArray& operator=(const VariableLengthArray& rhs)
    {
        Base::copy_assign_from(rhs, rhs.max_size());
        return *this;
    }

    VariableLengthArray(VariableLengthArray&& rhs) noexcept
        : Base(std::move(rhs))
    {
    }

    VariableLengthArray& operator=(VariableLengthArray&& rhs) noexcept(
        std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value ||
        std::allocator_traits<allocator_type>::is_always_equal::value)
    {
        Base::template move_assign_from<allocator_type>(std::move(rhs), rhs.max_size());
        return *this;
    }

    ~VariableLengthArray()
    {
        if (nullptr != data_)
        {
            // While deallocation is null-safe, we don't know if the allocator
            // was move and is now in an invalid state.
            Base::fast_deallocate(data_, size_, capacity_, alloc_);
        }
    }

    constexpr bool operator==(const VariableLengthArray& rhs) const noexcept
    {
        if (data_ == rhs.data_)
        {
            return true;
        }
        if (size_ != rhs.size_)
        {
            return false;
        }
        return std::equal(data_, data_ + size_, rhs.data_, rhs.data_ + rhs.size_);
    }

    constexpr bool operator!=(const VariableLengthArray& rhs) const noexcept
    {
        return !(operator==(rhs));
    }

    // +----------------------------------------------------------------------+
    // | ELEMENT ACCESS
    // +----------------------------------------------------------------------+
    ///
    /// Provides direct, unsafe access to the internal data buffer. This pointer
    /// is invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr const_iterator cbegin() const noexcept
    {
        return data_;
    }

    ///
    /// Pointer to memory location after the last, valid element. This pointer
    /// is invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr const_iterator cend() const noexcept
    {
        if (nullptr == data_)
        {
            return nullptr;
        }
        else
        {
            return &data_[size_];
        }
    }

    ///
    /// Provides direct, unsafe access to the internal data buffer. This pointer
    /// is invalidated by calls to `shrink_to_fit` and `reserve`.
    /// @return An iterator to the first element in the array.
    ///
    constexpr iterator begin() noexcept
    {
        return data_;
    }
    constexpr const_iterator begin() const noexcept
    {
        return cbegin();
    }

    ///
    /// Pointer to memory location after the last, valid element. This pointer
    /// is invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr iterator end() noexcept
    {
        if (nullptr == data_)
        {
            return nullptr;
        }
        else
        {
            return &data_[size_];
        }
    }
    constexpr const_iterator end() const noexcept
    {
        return cend();
    }

    ///
    /// Provides direct, unsafe access to the internal data buffer. This pointer
    /// is invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr value_type* data() noexcept
    {
        return data_;
    }

    ///
    /// Provides direct, unsafe access to the internal data buffer. This pointer
    /// is invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr const value_type* data() const noexcept
    {
        return data_;
    }

    ///
    /// Direct, const access to an element. If `pos` is > `size`
    /// the behavior is undefined.
    ///
    /// The returned reference is valid while this object is unless
    /// `reserve` or `shrink_to_fit` is called.
    ///
    constexpr const value_type& operator[](size_type pos) const noexcept
    {
        return data_[pos];
    }

    ///
    /// Direct access to an element. If `pos` is > `size`
    /// the behavior is undefined.
    ///
    /// The returned reference is valid while this object is unless
    /// `reserve` or `shrink_to_fit` is called.
    ///
    /// @param pos The index of the element to access.
    /// @return A reference to the element at `pos`.
    ///
    constexpr value_type& operator[](size_type pos) noexcept
    {
        return data_[pos];
    }

    ///
    /// STL-like access to a copy of the internal allocator.
    ///
    constexpr allocator_type get_allocator() const noexcept
    {
        return alloc_;
    }

#if __cpp_exceptions || defined(CETL_DOXYGEN)

    // *************************************************************************
    // we refuse to implement these with exceptions disabled since there is
    // no good use for then in that context.
    // *************************************************************************

    /// Returns a reference to the element at specified location pos, with bounds checking.
    ///
    /// @note
    /// This function is only available if exceptions are enabled since there is no
    /// valid way to implement it without exceptions.
    ///
    /// @param  pos Position of the element to return.
    /// @return Reference to the requested element.
    constexpr reference at(size_type pos)
    {
        if (pos >= size())
        {
            throw std::out_of_range("at position argument is outside of container size.");
        }
        return this->operator[][pos];
    }

    /// Returns a const reference to the element at specified location pos, with bounds checking.
    ///
    /// @note
    /// This function is only available if exceptions are enabled since there is no
    /// valid way to implement it without exceptions.
    ///
    /// @param  pos Position of the element to return.
    /// @return Const reference to the requested element.
    constexpr const_reference at(size_type pos) const
    {
        if (pos >= size())
        {
            throw std::out_of_range("at position argument is outside of container size.");
        }
        return this->operator[][pos];
    }

#endif

    // +----------------------------------------------------------------------+
    // | CAPACITY
    // +----------------------------------------------------------------------+

    /// Query if the container has any elements in it at all.
    /// @return `true` if the container is empty, `false` otherwise.
    ///
    constexpr bool empty() const noexcept
    {
        return (size_ == 0);
    }

    /// Reduce the amount of memory held by this object to the minimum required
    /// based on size. This method may not actually deallocate any memory if
    /// if there is not enough memory to allocate a smaller buffer before
    /// moving the existing elements and freeing the larger buffer.
    /// @throws if any items throw while being moved.
    void shrink_to_fit()
    {
#if __cpp_exceptions
        try
        {
#endif
            Base::shrink_to_fit();
#if __cpp_exceptions
        } catch (const std::bad_alloc&)
        {
            // per-spec. Any exceptions thrown have no effects. We simply don't
            // shrink.
        }
#endif
    }

    ///
    /// The number of elements that can be stored in the array without further
    /// allocations. This number will only grow through calls to `reserve`
    /// and can shrink through calls to `shrink_to_fit`. This value shall
    /// never exceed `max_size`.
    /// @return The current capacity of this object.
    ///
    constexpr size_type capacity() const noexcept
    {
        return capacity_;
    }

    ///
    /// The current number of elements in the array. This number increases with each
    /// successful call to `push_back` and decreases with each call to
    /// `pop_back` (when size is > 0).
    ///
    constexpr size_type size() const noexcept
    {
        return size_;
    }

    ///
    /// Ensure enough memory is allocated to store at least the `desired_capacity` number of elements.
    ///
    /// @param  desired_capacity The number of elements to allocate, but not initialize, memory for.
    ///
    void reserve(const size_type desired_capacity)
    {
        Base::reserve(desired_capacity, max_size());
    }

    /// Returns the, theoretical, maximum number of elements that can be stored in this container.
    /// It does not take into account the current state of the allocator. That is, if the allocator
    /// is out of memory this method will still return the maximum number of elements that could
    /// be stored if the allocator had enough memory, however, it will always return the maximum
    /// size passed into the constructor if that value is less than the allocator's max_size.
    ///
    /// @return The maximum number of elements that could be stored in this container.
    constexpr size_type max_size() const noexcept
    {
        const size_type max_diff = std::numeric_limits<ptrdiff_t>::max() / sizeof(value_type);
        return std::min(max_size_max_, std::min(max_diff, std::allocator_traits<allocator_type>::max_size(alloc_)));
    }

    // +----------------------------------------------------------------------+
    // | MODIFIERS
    // +----------------------------------------------------------------------+
    constexpr void clear() noexcept(std::is_nothrow_destructible<value_type>::value)
    {
        Base::fast_destroy(data_, size_);
        size_ = 0;
    }

    ///
    /// Allocate a new element on to the back of the array and copy value into it. Grows size by 1 and
    /// may grow capacity.
    ///
    /// If exceptions are disabled the caller must check before and after to see if the size grew to determine success.
    /// If using exceptions this method throws `std::length_error` if the size of this collection is at capacity
    /// or `std::bad_alloc` if the allocator failed to provide enough memory.
    ///
    /// If exceptions are disabled use the following logic:
    ///
    ///     const size_t size_before = my_array.size();
    ///     my_array.push_back(element);
    ///     if (size_before == my_array.size())
    ///     {
    ///         // failure
    ///         if (size_before == my_array.max_size())
    ///         {
    ///             // length_error: you probably should have checked this first.
    ///         }
    ///         else
    ///         {
    ///             // bad_alloc: out of memory
    ///         }
    ///     } // else, success.
    ///
    constexpr void push_back(const value_type& value)
    {
        if (!ensure_size_plus_one())
        {
            return;
        }

        if (nullptr == push_back_impl(value))
        {
#if __cpp_exceptions
            throw std::length_error("size is at capacity. Use reserve to grow the capacity.");
#endif
        }
    }

    ///
    /// Allocate a new element on to the back of the array and move value into it. Grows size by 1 and
    /// may grow capacity.
    ///
    /// See VariableLengthArray::push_back(value_type&) for full documentation.
    ///
    /// @throw std::length_error if the size of this collection is at capacity.
    /// @throw std::bad_alloc if memory was needed and none could be allocated.
    ///
    constexpr void push_back(value_type&& value)
    {
        if (!ensure_size_plus_one())
        {
#if __cpp_exceptions
            throw std::length_error("size is at capacity and we cannot grow the capacity.");
#endif
            return;
        }

        if (nullptr == push_back_impl(std::move(value)))
        {
#if __cpp_exceptions
            throw std::length_error("size is at capacity. Use reserve to grow the capacity.");
#endif
        }
    }

    ///
    /// Remove and destroy the last item in the array. This reduces the array size by 1 unless
    /// the array is already empty.
    ///
    constexpr void pop_back() noexcept(std::is_nothrow_destructible<value_type>::value)
    {
        if (size_ > 0)
        {
            data_[--size_].~value_type();
        }
    }

    ///
    /// Like push_back but constructs the object directly in uninitialized memory.
    /// @throw throw std::length_error if there was not enough storage for an additional element.
    ///        If exceptions are disabled then the caller must check the array size before and
    ///        after calling the method to determine if it succeeded.
    template <class... Args>
    void emplace_back(Args&&... args)
    {
        if (!ensure_size_plus_one())
        {
#if __cpp_exceptions
            throw std::length_error("size is at capacity and we cannot grow the capacity.");
#endif
            return;
        }

        std::allocator_traits<allocator_type>::construct(alloc_, &data_[size_], std::forward<Args>(args)...);
        size_++;
    }

private:
    // +----------------------------------------------------------------------+

    /// return true if there was or now is enough capacity to add one more element.
    constexpr bool ensure_size_plus_one()
    {
        if (size_ < capacity_)
        {
            return true;
        }

        return Base::grow(max_size());
    }

    constexpr pointer push_back_impl(value_type&& value) noexcept(std::is_nothrow_move_constructible<value_type>::value)
    {
        if (size_ < capacity_)
        {
            std::allocator_traits<allocator_type>::construct(alloc_, &data_[size_], std::move(value));
            return &data_[size_++];
        }
        else
        {
            return nullptr;
        }
    }

    constexpr pointer push_back_impl(const value_type& value) noexcept(
        std::is_nothrow_copy_constructible<value_type>::value)
    {
        if (size_ < capacity_)
        {
            std::allocator_traits<allocator_type>::construct(alloc_, &data_[size_], value);
            return &data_[size_++];
        }
        else
        {
            return nullptr;
        }
    }
};

// +-------------------------------------------------------------------------------------------------------------------+
// | SPECIALIZATIONS
// +-------------------------------------------------------------------------------------------------------------------+
///
/// A memory-optimized specialization for bool storing 8 bits per byte.
/// The internal bit ordering is little-endian.
/// @tparam Allocator The allocator type to use.
template <typename Allocator>
class VariableLengthArray<bool, Allocator> : protected VariableLengthArrayBase<unsigned char, Allocator>
{
protected:
    using Storage = unsigned char;
    static_assert(sizeof(Storage) == 1, "Unsigned char != 1 byte is not implemented (contributions welcome).");
    using Base = VariableLengthArrayBase<Storage, Allocator>;
    using Base::data_;
    using Base::size_;
    using Base::capacity_;
    using Base::max_size_max_;
    using Base::alloc_;

public:
    using value_type      = bool;
    using difference_type = typename Base::difference_type;
    using size_type       = typename Base::size_type;
    using allocator_type  = typename Base::allocator_type;

    ///
    /// Returns true iff the bit at the specified position is set and the argument is within the range.
    ///
    static constexpr bool test(const VariableLengthArray& container, const size_type pos) noexcept
    {
        const auto idx = std::make_pair(pos / 8U, pos % 8U);
        return (pos < container.size()) && ((container.data_[idx.first] & (1U << idx.second)) != 0);
    }

    ///
    /// Sets the bit at the specified position to the specified value. Does nothing if position is out of range.
    ///
    static constexpr void set(VariableLengthArray& container, const size_type pos, const bool value = true) noexcept
    {
        if (pos < container.size())
        {
            const auto idx = std::make_pair(pos / 8U, pos % 8U);
            if (value)
            {
                container.data_[idx.first] = static_cast<Storage>(container.data_[idx.first] | (1U << idx.second));
            }
            else
            {
                container.data_[idx.first] = static_cast<Storage>(container.data_[idx.first] & (~(1U << idx.second)));
            }
        }
    }

    class reference final
    {
    public:
        reference(const reference&) noexcept = default;
        reference(reference&&) noexcept      = default;
        ~reference() noexcept                = default;

        reference& operator=(const bool x)
        {
            set(array_, index_, x);
            return *this;
        }
        reference& operator=(const reference& x)
        {
            return this->operator=(static_cast<bool>(x));
        }
        reference& operator=(reference&& x)
        {
            return this->operator=(static_cast<bool>(x));
        }

        bool operator~() const
        {
            return !test(array_, index_);
        }
        operator bool() const
        {
            return test(array_, index_);
        }

        bool operator==(const reference& rhs) const
        {
            return static_cast<bool>(*this) == static_cast<bool>(rhs);
        }
        bool operator!=(const reference& rhs) const
        {
            return !((*this) == rhs);
        }

        void flip()
        {
            set(array_, index_, !test(array_, index_));
        }

    private:
        friend class VariableLengthArray;

        reference(VariableLengthArray& array, const size_type index) noexcept
            : array_(array)
            , index_(index)
        {
        }

        // The reference does not need to be copyable because URVO will elide the copy.
        VariableLengthArray& array_;
        const size_type      index_;
    };
    using const_reference = bool;

private:
    template <typename A>
    class IteratorImpl final
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = typename A::value_type;
        using difference_type   = typename A::difference_type;
        using reference         = typename A::reference;
        using const_reference   = typename A::const_reference;
        using pointer           = void;

        IteratorImpl() noexcept = default;

        IteratorImpl& operator++() noexcept
        {
            ++index_;
            return *this;
        }
        IteratorImpl operator++(int) noexcept
        {
            const IteratorImpl tmp(*this);
            ++index_;
            return tmp;
        }
        IteratorImpl& operator--() noexcept
        {
            --index_;
            return *this;
        }
        IteratorImpl operator--(int) noexcept
        {
            IteratorImpl tmp(*this);
            --index_;
            return tmp;
        }

        IteratorImpl& operator+=(const difference_type n) noexcept
        {
            index_ = static_cast<size_type>(static_cast<difference_type>(index_) + n);
            return *this;
        }
        IteratorImpl& operator-=(const difference_type n) noexcept
        {
            index_ = static_cast<size_type>(static_cast<difference_type>(index_) - n);
            return *this;
        }
        IteratorImpl operator+(const difference_type n) const noexcept
        {
            return IteratorImpl(*array_, static_cast<size_type>(static_cast<difference_type>(index_) + n));
        }
        IteratorImpl operator-(const difference_type n) const noexcept
        {
            return IteratorImpl(*array_, static_cast<size_type>(static_cast<difference_type>(index_) - n));
        }
        difference_type operator-(const IteratorImpl& other) const noexcept
        {
            return static_cast<difference_type>(index_) - static_cast<difference_type>(other.index_);
        }

        ///
        /// The return type may be either a reference or const_reference depending on whether this iterator is const
        /// or mutable.
        ///
        auto operator*() -> decltype(std::declval<A>()[0])
        {
            return this->operator[](0);
        }
        const_reference operator*() const
        {
            return this->operator[](0);
        }

        ///
        /// The return type may be either a reference or const_reference depending on whether this iterator is const
        /// or mutable.
        ///
        auto operator[](const difference_type n) -> decltype(std::declval<A>()[n])
        {
            return array_->operator[](static_cast<size_type>(static_cast<difference_type>(index_) + n));
        }
        const_reference operator[](const difference_type n) const
        {
            return array_->operator[](static_cast<size_type>(static_cast<difference_type>(index_) + n));
        }

        bool operator==(const IteratorImpl& other) const noexcept
        {
            return array_ == other.array_ && index_ == other.index_;
        }
        bool operator!=(const IteratorImpl& other) const noexcept
        {
            return !((*this) == other);
        }
        bool operator<(const IteratorImpl& other) const noexcept
        {
            return index_ < other.index_;
        }
        bool operator>(const IteratorImpl& other) const noexcept
        {
            return index_ > other.index_;
        }
        bool operator<=(const IteratorImpl& other) const noexcept
        {
            return index_ <= other.index_;
        }
        bool operator>=(const IteratorImpl& other) const noexcept
        {
            return index_ >= other.index_;
        }

    private:
        friend class VariableLengthArray;

        IteratorImpl(A& array, const size_type index) noexcept
            : array_(&array)
            , index_(index)
        {
        }

        A*        array_ = nullptr;
        size_type index_ = 0;
    };

public:
    using iterator       = IteratorImpl<VariableLengthArray>;
    using const_iterator = IteratorImpl<const VariableLengthArray>;

    /// Required constructor for std::uses_allocator protocol.
    /// @param alloc Allocator to use for all allocations.
    explicit constexpr VariableLengthArray(const allocator_type& alloc) noexcept
        : Base(alloc, nullptr, 0, 0, std::numeric_limits<size_type>::max())
        , last_byte_bit_fill_{0}
    {
    }

    /// Required constructor for std::uses_allocator protocol.
    /// @param alloc Allocator to use for all allocations.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    /// cetl::VariableLengthArray::max_size() will return
    /// `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    explicit constexpr VariableLengthArray(const allocator_type& alloc, size_type max_size_max) noexcept
        : Base(alloc, nullptr, 0, 0, max_size_max)
        , last_byte_bit_fill_{0}
    {
    }

    /// Initializer syntax constructor.
    /// @param l Initializer list of elements to copy into the array.
    /// @param alloc Allocator to use for all allocations.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    /// cetl::VariableLengthArray::max_size() will return
    /// `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    VariableLengthArray(std::initializer_list<bool> l,
                        const allocator_type&       alloc,
                        size_type                   max_size_max = std::numeric_limits<size_type>::max())
        : Base(alloc, nullptr, 0, 0, max_size_max)
        , last_byte_bit_fill_{0}
    {
        Base::reserve(bits2bytes(l.size()), max_size());
        for (auto list_item : l)
        {
            emplace_back_impl(list_item);
        }
    }

    /// Range constructor.
    /// @tparam InputIt The type of the range's iterators.
    /// @param first    The beginning of the range.
    /// @param last     The end of the range.
    /// @param length   The number of elements to copy from the range.
    /// @param alloc    Allocator to use for all allocations.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    /// cetl::VariableLengthArray::max_size() will return `std::min(max_size_max,
    /// std::allocator_traits<allocator_type>::max_size(alloc))`
    template <class InputIt>
    VariableLengthArray(InputIt               first,
                        InputIt               last,
                        const size_type       length,
                        const allocator_type& alloc        = std::declval<allocator_type>(),
                        size_type             max_size_max = std::numeric_limits<size_type>::max())
        : Base(alloc, nullptr, 0, 0, max_size_max)
        , last_byte_bit_fill_{0}
    {
        if (last >= first)
        {
            Base::reserve(bits2bytes(length), max_size());
            for (size_t inserted = 0; first != last && inserted < length; ++first)
            {
                emplace_back_impl(*first);
            }
        }
    }

    //
    // Rule of Five.
    //
    VariableLengthArray(const VariableLengthArray& rhs, const allocator_type& alloc)
        : Base(rhs, alloc)
        , last_byte_bit_fill_{rhs.last_byte_bit_fill_}
    {
        Base::reserve(rhs.size(), rhs.max_size());
        size_ = Base::fast_copy_construct(data_, capacity_, rhs.data_, rhs.size_, alloc_);
    }

    VariableLengthArray(const VariableLengthArray& rhs)
        : VariableLengthArray(rhs, rhs.alloc_)
    {
    }

    VariableLengthArray& operator=(const VariableLengthArray& rhs)
    {
        Base::copy_assign_from(rhs, rhs.max_size());
        last_byte_bit_fill_ = rhs.last_byte_bit_fill_;
        return *this;
    }

    VariableLengthArray(VariableLengthArray&& rhs) noexcept
        : Base(std::move(rhs))
        , last_byte_bit_fill_{rhs.last_byte_bit_fill_}
    {
        rhs.last_byte_bit_fill_ = 0;
    }

    VariableLengthArray& operator=(VariableLengthArray&& rhs) noexcept(
        std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value ||
        std::allocator_traits<allocator_type>::is_always_equal::value)
    {
        Base::template move_assign_from<allocator_type>(std::move(rhs), rhs.max_size());
        last_byte_bit_fill_     = rhs.last_byte_bit_fill_;
        rhs.last_byte_bit_fill_ = 0;
        return *this;
    }

    ~VariableLengthArray()
    {
        if (nullptr != data_)
        {
            // While deallocation is null-safe, we don't know if the allocator
            // was move and is now in an invalid state.
            Base::fast_deallocate(data_, size_, capacity_, alloc_);
        }
    }

    constexpr bool operator==(const VariableLengthArray& rhs) const noexcept
    {
        if (data_ == rhs.data_)
        {
            return true;
        }
        if (size_ != rhs.size_ || last_byte_bit_fill_ != rhs.last_byte_bit_fill_)
        {
            return false;
        }
        if (size_ > 1 && !std::equal(data_, data_ + size_ - 2, rhs.data_, rhs.data_ + rhs.size_ - 2))
        {
            return false;
        }
        if (size_ == 0)
        {
            return true;
        }
        const Storage last_byte_mask = static_cast<Storage>((1 << last_byte_bit_fill_) - 1U);
        return (data_[size_ - 1] & last_byte_mask) == (rhs.data_[size_ - 1] & last_byte_mask);
    }

    constexpr bool operator!=(const VariableLengthArray& rhs) const noexcept
    {
        return !(operator==(rhs));
    }

    // +----------------------------------------------------------------------+
    // | ELEMENT ACCESS
    // +----------------------------------------------------------------------+

    ///
    /// The returned iterators are invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr const_iterator cbegin() const noexcept
    {
        return const_iterator(*this, 0);
    }
    constexpr const_iterator cend() const noexcept
    {
        return const_iterator(*this, size());
    }
    constexpr iterator begin() noexcept
    {
        return iterator(*this, 0);
    }
    constexpr iterator end() noexcept
    {
        return iterator(*this, size());
    }
    constexpr const_iterator begin() const noexcept
    {
        return cbegin();
    }
    constexpr const_iterator end() const noexcept
    {
        return cend();
    }

    ///
    /// This is an alias for `test`.
    ///
    constexpr const_reference operator[](const size_type pos) const noexcept
    {
        return test(*this, pos);
    }

    constexpr reference operator[](const size_type pos) noexcept
    {
        return reference(*this, pos);
    }

    ///
    /// STL-like access to a copy of the internal allocator.
    ///
    constexpr allocator_type get_allocator() const noexcept
    {
        return alloc_;
    }

#if __cpp_exceptions || defined(CETL_DOXYGEN)

    // *************************************************************************
    // we refuse to implement these with exceptions disabled since there is
    // no good use for then in that context.
    // *************************************************************************

    /// Returns a reference to the element at specified location pos, with bounds checking.
    ///
    /// @note
    /// This function is only available if exceptions are enabled since there is no
    /// valid way to implement it without exceptions.
    ///
    /// @param  pos Position of the element to return.
    /// @return Reference to the requested element.
    constexpr reference at(size_type pos)
    {
        if (pos >= size())
        {
            throw std::out_of_range("at position argument is outside of container size.");
        }
        return this->operator[](pos);
    }

    /// Returns a const reference to the element at specified location pos, with bounds checking.
    ///
    /// @note
    /// This function is only available if exceptions are enabled since there is no
    /// valid way to implement it without exceptions.
    ///
    /// @param  pos Position of the element to return.
    /// @return Const reference to the requested element.
    constexpr const_reference at(size_type pos) const
    {
        if (pos >= size())
        {
            throw std::out_of_range("at position argument is outside of container size.");
        }
        return this->operator[](pos);
    }

#endif

    // +----------------------------------------------------------------------+
    // | CAPACITY
    // +----------------------------------------------------------------------+

    /// Query if the container has any elements in it at all.
    /// @return `true` if the container is empty, `false` otherwise.
    ///
    constexpr bool empty() const noexcept
    {
        return (last_byte_bit_fill_ == 0);
    }

    /// Returns the, theoretical, maximum number of elements that can be stored in this container.
    /// It does not take into account the current state of the allocator. That is, if the allocator
    /// is out of memory this method will still return the maximum number of elements that could
    /// be stored if the allocator had enough memory, however, it will always return the maximum
    /// size passed into the constructor if that value is less than the allocator's max_size.
    ///
    /// @return The maximum number of elements that could be stored in this container.
    constexpr size_type max_size() const noexcept
    {
        const size_type max_diff = std::numeric_limits<ptrdiff_t>::max() / sizeof(bool);
        const size_type max_size_bytes =
            std::min(max_size_max_, std::min(max_diff, std::allocator_traits<allocator_type>::max_size(alloc_)));
        return max_size_bytes * 8;
    }

    /// Reduce the amount of memory held by this object to the minimum required
    /// based on size. This method may not actually deallocate any memory if
    /// if there is not enough memory to allocate a smaller buffer before
    /// moving the existing elements and freeing the larger buffer.
    /// @throws if any items throw while being moved.
    void shrink_to_fit()
    {
#if __cpp_exceptions
        try
        {
#endif
            Base::shrink_to_fit();
#if __cpp_exceptions
        } catch (const std::bad_alloc&)
        {
            // per-spec. Any exceptions thrown have no effects. We simply don't
            // shrink.
        }
#endif
    }

    ///
    /// The number of elements that can be stored in the array without further
    /// allocations. This number will only grow through calls to `reserve`
    /// and can shrink through calls to `shrink_to_fit`. This value shall
    /// never exceed `max_size`.
    ///
    constexpr size_type capacity() const noexcept
    {
        return capacity_bits();
    }

    ///
    /// The current number of elements in the array. This number increases with each
    /// successful call to `push_back` and decreases with each call to
    /// `pop_back` (when size is > 0).
    ///
    constexpr size_type size() const noexcept
    {
        return size_bits();
    }

    ///
    /// Ensure enough memory is allocated to store at least the `desired_capacity` number of elements.
    ///
    /// @param  desired_capacity The number of elements to allocate, but not initialize, memory for.
    ///
    void reserve(const size_type desired_capacity)
    {
        Base::reserve(bits2bytes(desired_capacity), max_size());
    }

    // +----------------------------------------------------------------------+
    // | MODIFIERS
    // +----------------------------------------------------------------------+
    constexpr void clear() noexcept
    {
        size_               = 0;
        last_byte_bit_fill_ = 0;
    }

    ///
    /// Construct a new element on to the back of the array. Grows size by 1 and may grow capacity.
    ///
    /// If exceptions are disabled the caller must check before and after to see if the size grew to determine success.
    /// If using exceptions_before = my_array.size();
    ///     my_array.push_back();
    ///     if (size_before = this method throws `std::length_error` if the size of this collection is at capacity
    /// or `std::bad_alloc` if the allocator failed to provide enough memory.
    ///
    /// If exceptions are disabled use the following logic:
    ///
    ///     const size_t siz= my_array.size())
    ///     {
    ///         // failure
    ///         if (size_before == my_array.max_size())
    ///         {
    ///             // length_error: you probably should have checked this first.
    ///         }
    ///         else
    ///         {
    ///             // bad_alloc: out of memory
    ///         }
    ///     } // else, success.
    ///
    /// @throw std::length_error if the size of this collection is at capacity.
    /// @throw std::bad_alloc if memory was needed and none could be allocated.
    ///
    constexpr void push_back(bool value)
    {
        if (!ensure_size_plus_one())
        {
#if __cpp_exceptions
            throw std::length_error("max_size is reached, the array cannot grow further");
#endif
            return;
        }
        if (!emplace_back_impl(value))
        {
#if __cpp_exceptions
            throw std::length_error("max_size is reached, the array cannot grow further");
#endif
        }
    }

    ///
    /// Remove and destroy the last item in the array. This reduces the array size by 1 unless
    /// the array is already empty.
    ///
    constexpr void pop_back() noexcept
    {
        if (last_byte_bit_fill_ > 0)
        {
            last_byte_bit_fill_--;
        }
        if (last_byte_bit_fill_ == 0 && size_ > 1)
        {
            --size_;
            last_byte_bit_fill_ = 8;
        }
    }

    ///
    /// Like push_back but constructs the object directly in uninitialized memory.
    /// @throw throw std::length_error if there was not enough storage for an additional element.
    ///        If exceptions are disabled then the caller must check the array size before and
    ///        after calling the method to determine if it succeeded.
    void emplace_back(bool value)
    {
        if (!ensure_size_plus_one())
        {
#if __cpp_exceptions
            throw std::length_error("max_size is reached, the array cannot grow further");
#endif
            return;
        }
        if (!emplace_back_impl(value))
        {
#if __cpp_exceptions
            throw std::length_error("max_size is reached, the array cannot grow further");
#endif
        }
    }

private:
    constexpr bool ensure_size_plus_one()
    {
        if (capacity_ > 0 && (last_byte_bit_fill_ < 8 || size_ < capacity_))
        {
            // we have at least one byte of capacity (first allocation)
            // and we have room in the last byte or we have room for another byte
            return true;
        }

        return Base::grow(max_size());
    }

    constexpr bool emplace_back_impl(bool value)
    {
        const size_type index = size_bits();
        if (last_byte_bit_fill_ == 0 || last_byte_bit_fill_ == 8)
        {
            // we are using a bit in the next byte so we have to bump the size.
            if (size_ >= capacity_)
            {
                // nope. Can't grow.
                return false;
            }
            // last_byte_bit_fill_ == 0 is a special condition meaning there are
            // no bits set at all. In fact empty() == (last_byte_bit_fill_ == 0).
            // After the array gets to this branch last_byte_bit_fill_ will always
            // be > 0 as long as there is at least one bit in the array. That is,
            // it will cycle from 1 to 8 and back to 1 as our byte count, size_,
            // increases.

            // zero the next byte we're about to start using so comparisons
            // don't use uninitialized bits.
            data_[size_] = 0;
            ++size_;
            last_byte_bit_fill_ = 1;
        }
        else
        {
            ++last_byte_bit_fill_;
        }
        set(*this, index, value);
        return true;
    }

    constexpr static size_type round8(const size_type value) noexcept
    {
        return (value + 7U) & ~7U;
    }
    constexpr static size_type bits2bytes(const size_type value) noexcept
    {
        return round8(value) / 8U;
    }

    constexpr size_type size_bits() const noexcept
    {
        CETL_DEBUG_ASSERT(last_byte_bit_fill_ <= 8, "CDE_vla_001: last_byte_bit_fill_ is out of range.");
        CETL_DEBUG_ASSERT(size_ <= capacity_, "CDE_vla_002: size_ is out of range.");
        CETL_DEBUG_ASSERT(size_ == 0 || last_byte_bit_fill_ > 0,
                          "CDE_vla_003: last_byte_bit_fill_ cannot be zero unless size_ is.");
        return (size_ == 0) ? 0 : ((size_ - 1) * 8U) + last_byte_bit_fill_;
    }

    constexpr size_type capacity_bits() const noexcept
    {
        return capacity_ * 8U;
    }

    /// The number of bits that are valid in the last byte of the array.
    Storage last_byte_bit_fill_;
};

}  // namespace cetl

#endif  // CETL_VARIABLE_LENGTH_ARRAY_HPP_INCLUDED