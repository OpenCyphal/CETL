/// @file
/// Includes cetl::variable_length_array type and non-member functions.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// cSpell:ignore cend cbegin crbegin crend rbegin rnext rend lnext lbegin pocca pocma urvo

#ifndef CETL_VARIABLE_LENGTH_ARRAY_HPP_INCLUDED
#define CETL_VARIABLE_LENGTH_ARRAY_HPP_INCLUDED

#include <algorithm>
#include <cstring>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <memory>
#include <type_traits>
#if __cplusplus >= 202002L
#    include <compare>
#    include <concepts>
#endif

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

static_assert(
    __cplusplus >= 201402L,
    "Unsupported language: ISO C14, C++14, or a newer version of either is required to use the built-in VLA type");

namespace cetl
{
namespace detail
{

// True for types whose iterator_category is an input category, as std::vector requires of its range overloads.
template <typename T, typename = void>
struct is_input_iterator : std::false_type
{};
template <typename T>
struct is_input_iterator<T,
                         std::enable_if_t<std::is_base_of<std::input_iterator_tag,
                                                          typename std::iterator_traits<T>::iterator_category>::value>>
    : std::true_type
{};

// Selects iterator-range overloads as std::vector does: only for input iterators.
template <typename InputIt>
using enable_if_input_iterator_t = std::enable_if_t<is_input_iterator<InputIt>::value>;

// Selects the count-value constructors for counts that convert to SizeType, as std::vector's size_type parameter
// accepts. The count's type is deduced so a braced first argument cannot match it: VLA({x}, max_size_max, alloc)
// would otherwise be ambiguous with the initializer-list constructor whenever max_size_max converts better to
// value_type than to size_type, as an int literal does for VLA<int>. Input iterators are excluded so (first, last)
// always selects the range constructor, as it does for std::vector, even when the iterator converts to SizeType.
template <typename Count, typename SizeType>
using enable_if_count_t =
    std::enable_if_t<std::is_convertible<Count, SizeType>::value && !is_input_iterator<Count>::value>;

#if defined(__cpp_lib_three_way_comparison) && (__cpp_lib_three_way_comparison >= 201907L)
// The standard's exposition-only boolean-testable: a result that converts implicitly to bool, and whose negation does
// too, so it can be used in a condition the way bool can.
template <typename T>
concept BooleanTestableImpl = std::convertible_to<T, bool>;
template <typename T>
concept BooleanTestable = BooleanTestableImpl<T> && requires(T&& t) {
    { !std::forward<T>(t) } -> BooleanTestableImpl;
};

// The standard's exposition-only synth-three-way, which std::vector's operator<=> uses: an element's own operator<=>
// if it has one, otherwise a weak ordering derived from operator<. Elements without a boolean-testable operator< do
// not satisfy the constraint, so the array's operator<=> drops out of overload resolution instead of failing to
// compile, and is available exactly when std::vector's is.
struct synth_three_way
{
    template <typename T>
        requires requires(const T& lhs, const T& rhs) {
            { lhs < rhs } -> BooleanTestable;
            { rhs < lhs } -> BooleanTestable;
        }
    constexpr auto operator()(const T& lhs, const T& rhs) const
    {
        if constexpr (std::three_way_comparable<T>)
        {
            return lhs <=> rhs;
        }
        else
        {
            if (lhs < rhs)
            {
                return std::weak_ordering::less;
            }
            if (rhs < lhs)
            {
                return std::weak_ordering::greater;
            }
            return std::weak_ordering::equivalent;
        }
    }
};
#endif

// Common template implementation for variable_length_array and its specializations. This is not intended to be used
// directly.
template <typename BaseValueType, typename BaseAllocatorType>
class variable_length_array_base
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

    // Assignment is implemented by copy_assign_from/move_assign_from because it needs the derived class's max_size().
    // Copy construction is implemented by the derived classes via base(const base&, const allocator_type&).
    variable_length_array_base(const variable_length_array_base&)            = delete;
    variable_length_array_base& operator=(const variable_length_array_base&) = delete;
    variable_length_array_base& operator=(variable_length_array_base&&)      = delete;

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
    /// Check if an array of one type can be trivially coped to another. This
    /// is different from std::is_trivially_assignable in that it ensures a
    /// memcpy of arrays of the types will line up based on the object representation
    /// and alignment of the types involved.
    template <typename DstType, typename SrcType>
    struct is_array_of_type_trivially_copyable
        : std::integral_constant<bool,
                                 std::is_trivially_copyable<DstType>::value &&
                                     std::is_trivially_assignable<SrcType, DstType>::value &&
                                     (sizeof(DstType) == sizeof(SrcType)) && (alignof(DstType) == alignof(SrcType))>
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

protected:
    // +----------------------------------------------------------------------+
    // | Allocator move assignment
    // +----------------------------------------------------------------------+
    template <typename UAlloc>
    constexpr bool move_assign_alloc(
        UAlloc&& rhs,
        typename std::enable_if_t<
            std::allocator_traits<std::remove_reference_t<UAlloc>>::propagate_on_container_move_assignment::value>* =
            nullptr) noexcept(is_pocma_or_is_always_equal<UAlloc>::value)
    {
        static_assert(std::is_nothrow_move_assignable<allocator_type>::value,
                      "The C++ standard requires anything adhering to propagate_on_container_move_assignment to move "
                      "assign without throwing.");
        alloc_ = std::forward<UAlloc>(rhs);
        return true;
    }

    template <typename UAlloc>
    constexpr bool move_assign_alloc(
        UAlloc&& rhs,
        typename std::enable_if_t<
            !std::allocator_traits<std::remove_reference_t<UAlloc>>::propagate_on_container_move_assignment::value>* =
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
        typename std::enable_if_t<std::allocator_traits<UAlloc>::propagate_on_container_copy_assignment::value>* =
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
        typename std::enable_if_t<!std::allocator_traits<UAlloc>::propagate_on_container_copy_assignment::value>* =
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

    // Reallocation may move the block by copying bytes, without invoking element constructors or destructors.
    // Only trivially copyable storage can use this extension; other types use allocate_and_forward instead.
    // Apply the same type-level rule to empty buffers. Packed bool remains eligible through its byte storage.
    template <typename UAllocator>
    static constexpr typename std::enable_if_t<std::is_trivially_copyable<value_type>::value &&
                                                   is_detected<reallocate_operation, UAllocator>::value,
                                               value_type>*
    reallocate(value_type* data, UAllocator& alloc, std::size_t old_object_count, std::size_t new_object_count)
    {
        return alloc.reallocate(data, old_object_count, new_object_count);
    }

    template <typename UAllocator>
    static constexpr typename std::enable_if_t<!std::is_trivially_copyable<value_type>::value ||
                                                   !is_detected<reallocate_operation, UAllocator>::value,
                                               value_type>*
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
        allocator_type& alloc,
        typename std::enable_if_t<std::is_trivially_destructible<U>::value>* = nullptr) noexcept
    {
        (void) src;
        (void) src_size_count;
        (void) alloc;
    }

    ///
    /// If not trivially destructible then we invoke each destructor.
    ///
    template <typename U>
    static constexpr void fast_destroy(U* const        src,
                                       const size_type src_size_count,
                                       allocator_type& alloc,
                                       typename std::enable_if_t<!std::is_trivially_destructible<U>::value>* =
                                           nullptr) noexcept(std::is_nothrow_destructible<U>::value)
    {
        size_type dtor_iterator = src_size_count;
        while (dtor_iterator > 0)
        {
            std::allocator_traits<allocator_type>::destroy(alloc, std::addressof(src[--dtor_iterator]));
        }
    }

    ///
    /// Destroys every element but keeps the allocation, leaving this container empty with its capacity intact.
    ///
    constexpr void destroy_all() noexcept(std::is_nothrow_destructible<value_type>::value)
    {
        fast_destroy(data_, size_, alloc_);
        size_ = 0;
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
        fast_destroy(src, src_size_count, alloc);
        // An array that never allocated holds no storage; deallocate(nullptr, 0) violates the allocator contract.
        if (src != nullptr)
        {
            alloc.deallocate(src, src_capacity_count);
        }
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
        fast_destroy(src, src_size_count, alloc);
        // An array that never allocated holds no storage; deallocate(nullptr, 0) violates the allocator contract.
        if (src != nullptr)
        {
            alloc.deallocate(src, src_capacity_count);
        }
    }

    // +----------------------------------------------------------------------+
    // | COPY ASSIGNMENT
    // +----------------------------------------------------------------------+
    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename InputIt>
    static constexpr size_type fast_copy_assign(value_type* dst, size_type dst_capacity_count, InputIt src)
    {
        if (nullptr == dst)
        {
            return 0;
        }
        (void) std::fill_n(dst, dst_capacity_count, src);
        return dst_capacity_count;
    }

    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename InputIt>
    static constexpr size_type fast_copy_assign(value_type* dst,
                                                size_type   dst_capacity_count,
                                                InputIt     src,
                                                size_type   src_len_count)
    {
        if (nullptr == dst || nullptr == src)
        {
            return 0;
        }
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        (void) std::copy_n(src, max_copy_size, dst);
        return max_copy_size;
    }

    // Construct a contiguous range, recording only completed constructions. The caller supplies the construction
    // operation so copy, move, default, and fill construction retain their allocator hooks and relocation policy.
    // On failure, destroy the completed prefix; allocation ownership remains with the caller.
    template <typename Construct>
    static void construct_n(value_type* dst, size_type count, allocator_type& alloc, Construct construct)
    {
        size_type constructed = 0;
#if defined(__cpp_exceptions)
        try
        {
#else
        (void) alloc;
#endif
            for (; constructed < count; ++constructed)
            {
                construct(dst + constructed, constructed);
            }
#if defined(__cpp_exceptions)
        } catch (...)
        {
            while (constructed > 0)
            {
                std::allocator_traits<allocator_type>::destroy(alloc, dst + --constructed);
            }
            throw;
        }
#endif
    }

    // +----------------------------------------------------------------------+
    // | COPY CONSTRUCTION
    // +----------------------------------------------------------------------+

    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename InputIt>
    static constexpr size_type fast_copy_construct(
        value_type* const dst,
        size_type         dst_capacity_count,
        const InputIt     src,
        size_type         src_len_count,
        allocator_type&   alloc,
        typename std::enable_if_t<
            is_array_of_type_trivially_copyable<std::add_pointer_t<value_type>, InputIt>::value>* = nullptr) noexcept
    {
        (void) alloc;
        // for trivial copyable assignment is the same as construction:
        return fast_copy_assign(dst, dst_capacity_count, src, src_len_count);
    }

    ///
    /// Copy from src to dst.
    /// @return the number of elements copied.
    ///
    template <typename InputIt>
    static constexpr size_type fast_copy_construct(
        value_type* const dst,
        size_type         dst_capacity_count,
        const InputIt     src,
        size_type         src_len_count,
        allocator_type&   alloc,
        typename std::enable_if_t<
            !is_array_of_type_trivially_copyable<std::add_pointer_t<value_type>, InputIt>::value>* = nullptr)
    {
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        construct_n(dst, max_copy_size, alloc, [&alloc, &src](value_type* address, size_type& i) {
            std::allocator_traits<allocator_type>::construct(alloc, address, src[static_cast<difference_type>(i)]);
        });
        return max_copy_size;
    }

    // +----------------------------------------------------------------------+
    // | FORWARD CONSTRUCTION
    // +----------------------------------------------------------------------+
    template <typename InputIt>
    static constexpr size_type fast_forward_construct(
        value_type* const dst,
        size_type         dst_capacity_count,
        InputIt           src,
        size_type         src_len_count,
        allocator_type&   alloc,
        typename std::enable_if_t<
            is_array_of_type_trivially_copyable<std::add_pointer_t<value_type>, InputIt>::value>* = nullptr)
    {
        (void) alloc;
        // for trivial copyable assignment move is the same as copy:
        return fast_copy_assign(dst, dst_capacity_count, src, src_len_count);
    }

    template <typename InputIt>
    static constexpr size_type fast_forward_construct(
        value_type* const dst,
        size_type         dst_capacity_count,
        InputIt           src,
        size_type         src_len_count,
        allocator_type&   alloc,
        typename std::enable_if_t<
            !is_array_of_type_trivially_copyable<std::add_pointer_t<value_type>, InputIt>::value>* = nullptr)
    {
        const size_type max_copy_size = std::min(dst_capacity_count, src_len_count);
        construct_n(dst, max_copy_size, alloc, [&alloc, &src](value_type* address, size_type& i) {
            std::allocator_traits<allocator_type>::construct(alloc, address, std::move_if_noexcept(src[i]));
        });
        return max_copy_size;
    }

    // Construction rollback destroys objects; this helper separately releases an abandoned allocation.
    static value_type* allocate_and_forward(size_type capacity, value_type* src, size_type count, allocator_type& alloc)
    {
        value_type* const result = std::allocator_traits<allocator_type>::allocate(alloc, capacity);
        if (result != nullptr)
        {
#if defined(__cpp_exceptions)
            try
            {
#endif
                fast_forward_construct(result, capacity, src, count, alloc);
#if defined(__cpp_exceptions)
            } catch (...)
            {
                std::allocator_traits<allocator_type>::deallocate(alloc, result, capacity);
                throw;
            }
#endif
        }
        return result;
    }

    // +----------------------------------------------------------------------+
    // | FORWARD ASSIGNMENT
    // +----------------------------------------------------------------------+
    template <typename InputIt>
    static constexpr size_type fast_forward_assign(
        value_type* const dst,
        size_type         dst_capacity_count,
        InputIt           src,
        size_type         src_len_count,
        typename std::enable_if_t<
            is_array_of_type_trivially_copyable<std::add_pointer_t<value_type>, InputIt>::value>* = nullptr)
    {
        // for trivial copyable move and copy is the same.
        return fast_copy_assign(dst, dst_capacity_count, src, src_len_count);
    }

    template <typename InputIt>
    static constexpr size_type fast_forward_assign(
        value_type* const dst,
        size_type         dst_capacity_count,
        InputIt           src,
        size_type         src_len_count,
        typename std::enable_if_t<
            !is_array_of_type_trivially_copyable<std::add_pointer_t<value_type>, InputIt>::value>* = nullptr)
    {
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
    constexpr void copy_assign_from(const variable_length_array_base& rhs, const size_type rhs_max_size)
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
                fast_destroy(&data_[overlap], size_ - overlap, alloc_);
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
        variable_length_array_base&& rhs,
        const size_type              rhs_max_size,
        typename std::enable_if_t<is_pocma_or_is_always_equal<UAlloc>::value>* = nullptr) noexcept
    {
        (void) rhs_max_size;

        if (this == &rhs)
        {
            return;
        }

        fast_deallocate(data_, size_, capacity_, alloc_);
        move_assign_alloc(std::move(rhs.alloc_));

        max_size_max_ = rhs.max_size_max_;
        capacity_     = rhs.capacity_;
        size_         = rhs.size_;
        data_         = rhs.data_;
        rhs.size_     = 0;
        rhs.data_     = nullptr;
        rhs.capacity_ = 0;
    }

    template <typename UAlloc>
    constexpr void move_assign_from(variable_length_array_base&& rhs,
                                    const size_type              rhs_max_size,
                                    typename std::enable_if_t<!is_pocma_or_is_always_equal<UAlloc>::value>* = nullptr)
    {
        if (this == &rhs)
        {
            return;
        }

        // Every branch adopts the source's bound, as copy_assign_from does. The unequal-allocator branches then size
        // the destination against rhs_max_size, which is now also this container's limit.
        max_size_max_ = rhs.max_size_max_;

        // can't move the allocator and they aren't always the same. we have to
        // move assign into new memory.
        if (alloc_ == rhs.alloc_)
        {
            // This overload never propagates the allocator but the two allocators compare equal this time so our
            // alloc_ can deallocate the incoming memory. We simply take ownership of it and keep alloc_.
            fast_deallocate(data_, size_, capacity_, alloc_);

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

                // Let's go ahead and move-assign what we can.
                const std::size_t overlap = fast_forward_assign(data_, size_, rhs.data_, rhs.size_);
                if (rhs.size_ <= size_)
                {
                    // if there's fewer elements in rhs than in this container, destroy the remaining elements.
                    fast_destroy(&data_[overlap], size_ - overlap, alloc_);
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
                reserve(new_size, rhs_max_size);
                size_ = fast_forward_construct(data_, capacity_, rhs.data_, new_size, alloc_);
            }
            // Empty the source but retain its allocation for reuse; callers can explicitly shrink it. The sizes differ
            // only when exceptions are disabled and the replacement allocation was declined. Nothing was transferred
            // then, so the source is left unchanged.
            if (size_ == rhs.size_)
            {
                rhs.destroy_all();
            }
        }
    }

    // +----------------------------------------------------------------------+
    // | Common Implementation
    // +----------------------------------------------------------------------+
    // Range assignment converts elements implicitly, as std::vector does. A conversion the caller requested (int into
    // std::uint8_t, for example) must not surface as a -Wconversion diagnostic located in this header.
#if defined(__GNUC__)
#    pragma GCC diagnostic push
#    pragma GCC diagnostic ignored "-Wconversion"
#    pragma GCC diagnostic ignored "-Wsign-conversion"
#    pragma GCC diagnostic ignored "-Wfloat-conversion"
#    pragma GCC diagnostic ignored "-Wdouble-promotion"
#    pragma GCC diagnostic ignored "-Wfloat-equal"
#endif
    template <typename Target, typename Source>
    static void assign_element(Target&& target, Source&& source)
    {
        std::forward<Target>(target) = std::forward<Source>(source);
    }

    // Use the public element interface so ordinary elements and packed bool share the same single-pass algorithm.
    // Commit each appended element before advancing the iterator: construction or iterator failures must leave
    // every live element represented in size(). emplace_back enforces max_size(). Returns the number of elements
    // assigned; the caller destroys any unused suffix.
    template <typename Array, typename InputIt>
    static size_type assign_from_input_range(Array& array, InputIt first, InputIt last)
    {
        const size_type size_before = array.size();
        size_type       assigned    = 0;
        for (; first != last; ++assigned, (void) ++first)
        {
            if (assigned < size_before)
            {
                assign_element(array[assigned], *first);
            }
            else
            {
                array.emplace_back(*first);
                if (array.size() == assigned)
                {
                    // With exceptions disabled, emplace_back stops at max_size() or when growth is declined.
                    break;
                }
            }
        }
        return assigned;
    }
#if defined(__GNUC__)
#    pragma GCC diagnostic pop
#endif

    // Forward ranges can be measured without consuming their elements. Do not subtract equal null pointers.
    template <typename ForwardIt>
    static size_type assignment_range_size(ForwardIt first, ForwardIt last, const size_type max_size)
    {
        const size_type count = first == last ? 0 : static_cast<size_type>(std::distance(first, last));
        if (count > max_size)
        {
#if defined(__cpp_exceptions)
            throw std::length_error("Assigned range exceeds maximum size.");
#else
            return max_size;
#endif
        }
        return count;
    }

    // Replacement assignment does not need to relocate any old elements. In particular, forward-iterator
    // assignment must support elements constructible/assignable from *first even if they cannot be copied or moved.
    // Release insufficient storage before allocating, as in copy_assign_from, to support bounded resources.
    void prepare_assign_storage(const size_type count)
    {
        if (count > capacity_)
        {
            fast_deallocate(data_, size_, capacity_, alloc_);
            data_     = nullptr;
            size_     = 0;
            capacity_ = 0;
            data_     = std::allocator_traits<allocator_type>::allocate(alloc_, count);
            if (data_ != nullptr)
            {
                capacity_ = count;
            }
#if defined(__cpp_exceptions)
            else
            {
                throw std::bad_alloc();
            }
#endif
        }
    }

    /// Destroys the elements from `new_size` onward, as resize() and copy assignment do.
    void destroy_suffix(const size_type new_size) noexcept(std::is_nothrow_destructible<value_type>::value)
    {
        if (size_ > new_size)
        {
            fast_destroy(data_ + new_size, size_ - new_size, alloc_);
            size_ = new_size;
        }
    }

    // std::copy_n lets a trivially copyable random-access source become a single memmove.
    template <typename RandomIt>
    RandomIt assign_prefix(RandomIt first, const size_type count, std::random_access_iterator_tag)
    {
        (void) std::copy_n(first, count, data_);
        return first + static_cast<typename std::iterator_traits<RandomIt>::difference_type>(count);
    }

    template <typename ForwardIt>
    ForwardIt assign_prefix(ForwardIt first, const size_type count, std::forward_iterator_tag)
    {
        for (size_type i = 0; i < count; ++i, (void) ++first)
        {
            assign_element(data_[i], *first);
        }
        return first;
    }

    template <typename ForwardIt>
    void assign_from_forward_range(ForwardIt first, ForwardIt last, const size_type max_size)
    {
        const size_type count = assignment_range_size(first, last, max_size);
        prepare_assign_storage(count);
        const size_type available = std::min(count, capacity_);
        const size_type overlap   = std::min(size_, available);
        first = assign_prefix(first, overlap, typename std::iterator_traits<ForwardIt>::iterator_category{});
        construct_suffix(first, available);
        destroy_suffix(available);
    }

    // Constructs elements from first, committing each one, until size() reaches `available`.
    template <typename ForwardIt>
    void construct_suffix(ForwardIt first, const size_type available)
    {
        for (; size_ < available; (void) ++first)
        {
            std::allocator_traits<allocator_type>::construct(alloc_, data_ + size_, *first);
            ++size_;
        }
    }

    // Range construction of an empty array, like std::vector's, only constructs elements: it must not require them
    // to be assignable, as assign_from_forward_range does even when nothing overlaps.
    template <typename ForwardIt>
    void construct_from_forward_range(ForwardIt first, ForwardIt last, const size_type max_size)
    {
        prepare_assign_storage(assignment_range_size(first, last, max_size));
        construct_suffix(first, capacity_);
    }

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
#if defined(__cpp_exceptions)
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
            // Reallocation is unavailable, unsafe for this element type, or declined by the allocator.
            // Allocate replacement storage and relocate the elements with the appropriate construction policy.
            new_data = allocate_and_forward(no_shrink_capacity, data_, size_, alloc_);
            if (nullptr != new_data)
            {
                fast_deallocate(data_, size_, capacity_, alloc_);
                data_     = new_data;
                capacity_ = no_shrink_capacity;
            }
#if defined(__cpp_exceptions)
            // GCOVR_EXCL_START
            else
            {
                // protect against invalid standard libraries that do not throw as required.
                throw std::bad_alloc();
            }
            // GCOVR_EXCL_STOP
#endif
        }
    }

    template <typename... Args>
    constexpr void resize(size_type new_size, const size_type max_size, Args&&... args)
    {
        if (new_size == size_)
        {
            return;
        }

        if (new_size > size_)
        {
            // grow
            // Spare capacity can exceed max_size() after assignment adopts a smaller bound, so check it explicitly.
            if (new_size > max_size)
            {
#if defined(__cpp_exceptions)
                throw std::length_error("Requested size exceeds maximum size.");
#else
                new_size = max_size;
                if (new_size <= size_)
                {
                    return;
                }
#endif
            }
            if (new_size > capacity_)
            {
                reserve(new_size, max_size);
#if !defined(__cpp_exceptions)
                if (capacity_ != new_size)
                {
                    new_size = capacity_;
                }
#endif
            }
            construct_n(data_ == nullptr ? nullptr : data_ + size_,
                        new_size - size_,
                        alloc_,
                        [this, &args...](value_type* address, size_type&) {
                            std::allocator_traits<allocator_type>::construct(alloc_,
                                                                             address,
                                                                             std::forward<Args>(args)...);
                        });
        }
        else
        {
            // shrink
            fast_destroy(&data_[new_size], size_ - new_size, alloc_);
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
        value_type* minimized_data = allocate_and_forward(size_, data_, size_, alloc_);

        if (minimized_data == nullptr)
        {
            return;
        }
        else
        {
            if (minimized_data != data_)
            {
                fast_deallocate(data_, size_, capacity_, alloc_);
            }  // else the allocator was able to simply shrink the reserved area for the same memory pointer.
            data_     = minimized_data;
            capacity_ = size_;
        }
    }

    /// The capacity that grow() requests. It is not greater than capacity() when max_size prevents growth.
    constexpr size_type grown_capacity(const size_type max_size) const noexcept
    {
        // Simple geometric progression of capacity growth.
        const size_type half_capacity = capacity_ / 2U;

        // That is, instead of a capacity sequence of 1, 2, 3, 4, 6, 9 we start from zero as 2, 4, 6, 9. The first
        // opportunity for reusing previously freed memory comes when increasing to 19 from 13 since E(n-1) == 21.
        return std::min(max_size, capacity_ + ((half_capacity <= 1) ? 2 : half_capacity));
    }

    /// Grows the capacity of the array ensures amortized, constant-time expansion.
    constexpr bool grow(const size_type max_size)
    {
        const size_type capacity_before = capacity_;

        reserve(grown_capacity(max_size), max_size);

        return capacity_ > capacity_before;
    }

    /// Grows the capacity as grow() does and constructs one more element from `args` at the end of the array.
    /// The arguments may refer to elements of this array, as in `a.emplace_back(a[0])`, so they are consumed before
    /// the old storage is released.
    /// @return false, with the array unchanged, if max_size prevents growth or, with exceptions disabled, the
    ///         allocator returned no memory.
    /// @throw * Any exception thrown by the allocator or by constructing an element. The array is then unchanged.
    template <typename... Args>
    bool grow_and_emplace_back(const size_type max_size, Args&&... args)
    {
        if (grown_capacity(max_size) <= capacity_)
        {
            return false;
        }
        return grow_and_emplace_back_impl(reallocates_elements{}, max_size, std::forward<Args>(args)...);
    }

    /// Whether reserve() may relocate elements through the allocator's reallocate extension (see reallocate()).
    using reallocates_elements = std::integral_constant<bool,
                                                        std::is_trivially_copyable<value_type>::value &&
                                                            is_detected<reallocate_operation, allocator_type>::value>;

    /// Whether `value` is one of the elements in [data(), data() + size()).
    bool refers_to_element(const value_type& value) const noexcept
    {
        // std::less orders pointers that are not into the same array, which the built-in operators do not.
        const std::less<const value_type*> less;
        const value_type*                  address = std::addressof(value);
        return (data_ != nullptr) && !less(address, data_) && less(address, data_ + size_);
    }

    /// Allocator reallocation can release the old block before the new element is constructed, and arbitrary
    /// constructor arguments may refer into it. Construct the element in new storage before relocating the old ones
    /// instead, forgoing reallocation for this append.
    template <typename... Args>
    bool grow_and_emplace_back_impl(std::true_type, const size_type max_size, Args&&... args)
    {
        return grow_and_emplace_back_impl(std::false_type{}, max_size, std::forward<Args>(args)...);
    }

    /// Without arguments nothing can refer into the old block, so grow as usual and construct the element in place.
    bool grow_and_emplace_back_impl(std::true_type, const size_type max_size)
    {
        if (!grow(max_size))
        {
            return false;
        }
        std::allocator_traits<allocator_type>::construct(alloc_, data_ + size_);
        ++size_;
        return true;
    }

    /// Whether `Arg&&` binds to a value_type that is not volatile, and so can be checked for aliasing.
    template <typename Arg>
    using is_element_argument = std::is_same<std::remove_const_t<std::remove_reference_t<Arg>>, value_type>;

    /// A single element-typed argument, as from push_back(), can be checked for aliasing. Either way the element is
    /// constructed once, through the allocator, in its final storage. Volatile arguments take the general path.
    template <typename Arg, std::enable_if_t<is_element_argument<Arg>::value, int> = 0>
    bool grow_and_emplace_back_impl(std::true_type, const size_type max_size, Arg&& arg)
    {
        if (refers_to_element(arg))
        {
            // Reallocation may release the element, so construct from it in new storage before relocating.
            return grow_and_emplace_back_impl(std::false_type{}, max_size, std::forward<Arg>(arg));
        }
        if (!grow(max_size))
        {
            return false;
        }
        std::allocator_traits<allocator_type>::construct(alloc_, data_ + size_, std::forward<Arg>(arg));
        ++size_;
        return true;
    }

    /// Constructs the new element in replacement storage while the old storage is intact, then relocates the old
    /// elements in front of it, as libstdc++'s `vector::_M_realloc_insert` does.
    template <typename... Args>
    bool grow_and_emplace_back_impl(std::false_type, const size_type max_size, Args&&... args)
    {
        const size_type   new_capacity = grown_capacity(max_size);
        value_type* const new_data     = std::allocator_traits<allocator_type>::allocate(alloc_, new_capacity);
        if (new_data == nullptr)
        {
#if defined(__cpp_exceptions)
            // GCOVR_EXCL_START
            // protect against invalid standard libraries that do not throw as required.
            throw std::bad_alloc();
            // GCOVR_EXCL_STOP
#else
            return false;
#endif
        }
        value_type* const element = new_data + size_;
#if defined(__cpp_exceptions)
        try
        {
#endif
            std::allocator_traits<allocator_type>::construct(alloc_, element, std::forward<Args>(args)...);
#if defined(__cpp_exceptions)
        } catch (...)
        {
            std::allocator_traits<allocator_type>::deallocate(alloc_, new_data, new_capacity);
            throw;
        }
        try
        {
#endif
            // On failure this destroys the elements it relocated, leaving the old ones valid.
            (void) fast_forward_construct(new_data, new_capacity, data_, size_, alloc_);
#if defined(__cpp_exceptions)
        } catch (...)
        {
            std::allocator_traits<allocator_type>::destroy(alloc_, element);
            std::allocator_traits<allocator_type>::deallocate(alloc_, new_data, new_capacity);
            throw;
        }
#endif
        fast_deallocate(data_, size_, capacity_, alloc_);
        data_     = new_data;
        capacity_ = new_capacity;
        ++size_;
        return true;
    }

    // +----------------------------------------------------------------------+
    // | CONSTRUCTORS
    // +----------------------------------------------------------------------+

    constexpr variable_length_array_base(
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

    /// The allocator is used as given. select_on_container_copy_construction is the business of the non-extended
    /// copy constructor only (see the derived classes).
    constexpr variable_length_array_base(const variable_length_array_base& rhs, const allocator_type& alloc) noexcept(
        std::is_nothrow_copy_constructible<allocator_type>::value)
        : alloc_(alloc)
        , data_(nullptr)
        , capacity_(0)
        , size_(0)
        , max_size_max_(rhs.max_size_max_)
    {
    }

    constexpr variable_length_array_base(variable_length_array_base&& rhs) noexcept
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

    /// Allocator-extended move construction where the allocators are always equal: the storage is simply adopted.
    /// Note that propagate_on_container_move_assignment has no bearing on construction.
    template <typename UAlloc>
    constexpr variable_length_array_base(
        variable_length_array_base&& rhs,
        const UAlloc&                alloc,
        typename std::enable_if_t<std::allocator_traits<UAlloc>::is_always_equal::value>* = nullptr) noexcept
        : alloc_(alloc)
        , data_(std::move(rhs.data_))
        , capacity_(rhs.capacity_)
        , size_(rhs.size_)
        , max_size_max_(rhs.max_size_max_)
    {
        static_assert(std::is_nothrow_copy_constructible<UAlloc>::value,
                      "Allocator must be nothrow copy constructible.");
        rhs.size_     = 0;
        rhs.capacity_ = 0;
        rhs.data_     = nullptr;
    }

    /// Allocator-extended move construction where the allocators may be unequal. If they are equal at runtime the
    /// storage is adopted, otherwise storage is obtained from the given allocator and the elements are moved into
    /// it, leaving the source empty with its original capacity still allocated from the source allocator. The latter
    /// can throw so this overload is not noexcept.
    template <typename UAlloc>
    constexpr variable_length_array_base(
        variable_length_array_base&& rhs,
        const UAlloc&                alloc,
        typename std::enable_if_t<!std::allocator_traits<UAlloc>::is_always_equal::value>* = nullptr)
        : alloc_(alloc)
        , data_{nullptr}
        , capacity_(0)
        , size_(0)
        , max_size_max_(rhs.max_size_max_)
    {
        static_assert(std::is_nothrow_copy_constructible<UAlloc>::value,
                      "Allocator must be nothrow copy constructible.");
        if (alloc_ == rhs.alloc_)
        {
            // The allocators may not always be equal, but they are this time.
            data_         = std::move(rhs.data_);
            capacity_     = rhs.capacity_;
            size_         = rhs.size_;
            rhs.size_     = 0;
            rhs.capacity_ = 0;
            rhs.data_     = nullptr;
        }
        else
        {
            // The allocators are not equal, so we need to move the data over
            // manually.
            if (rhs.size_ > 0)
            {
                data_ = allocate_and_forward(rhs.size_, rhs.data_, rhs.size_, alloc_);
                if (data_ == nullptr)
                {
#if defined(__cpp_exceptions)
                    throw std::bad_alloc();
#else
                    return;
#endif
                }
            }
            capacity_ = rhs.size_;
            size_     = rhs.size_;
            // Destroy the relocated elements only after success, retaining the allocation for source reuse.
            rhs.destroy_all();
        }
    }

    ~variable_length_array_base()
    {
        // This also runs when a derived constructor throws after acquiring storage.
        if (data_ != nullptr)
        {
            fast_deallocate(data_, size_, capacity_, alloc_);
        }
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

}  // namespace detail

// +-------------------------------------------------------------------------------------------------------------------+
// | variable_length_array
// +-------------------------------------------------------------------------------------------------------------------+

///
/// Minimal, generic container for storing Cyphal variable-length arrays. While this type shares similarities with
/// std::vector it has several important properties that are different.
/// First, the `max_size_max` maximum bound allows the array to enforce a maximum size whether or not the allocator
/// properly implements the std::allocator_traits::max_size protocol (std::pmr::polymorphic_allocator, for example).
/// This allows use of an allocator that is backed by statically allocated memory:
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_exact_fit
///
/// Even if cetl::variable_length_array::reserve() is not used the implementation will still use less memory then its
/// STL counterpart:
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_tight_fit_0
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_tight_fit_1
///
/// Finally, when exceptions are disabled the cetl::variable_length_array will not exhibit undefined behaviour like
/// std::vector does:
/// @snippet{trimleft} example_08_variable_length_array_vs_vector.cpp example_no_exceptions
/// (@ref example_08_variable_length_array_vs_vector "See full example here...")
///
/// The allocator's optional `reallocate` extension is used only for trivially copyable storage. Non-trivial
/// elements require replacement storage during capacity changes, even if the allocator could resize in place.
/// With a resource that supports only one outstanding allocation, reserve sufficient capacity before adding elements.
/// If such a resource backs the source of an unequal-allocator move, the empty source still holds that allocation
/// afterward (see the table below); call `shrink_to_fit()` on the source before allocating from the resource again.
///
/// After successful move construction or move assignment between distinct arrays, the source is empty. If its
/// storage is transferred, its capacity becomes zero. If unequal allocators require element relocation, the source
/// retains its original allocation and capacity for reuse. Call `shrink_to_fit()` on the empty source to release
/// that storage explicitly; this requires no replacement allocation and is a no-op if capacity is already zero.
/// Because the source keeps that storage, its allocator and any memory resource behind it must remain valid until
/// the source is destroyed or shrunk, as for a moved-from `std::vector`.
/// Retaining source capacity does not copy its spare capacity into the destination of an unequal-allocator move
/// construction, which obtains only the storage required for the elements (rounded to bytes for packed bool).
/// Every copy and move, whatever the allocators, also gives the destination the source's `max_size_max` bound.
///
/// In the table below, `C` is the source capacity before a successful move between distinct arrays. The source size
/// becomes zero in every row. The same rules apply to packed `variable_length_array<bool>` and empty reserved sources.
/// Self-move assignment leaves the array unchanged. Allocator propagation affects assignment only.
///
// clang-format off
/// | Move operation | Allocator condition | Destination storage operations | Source capacity afterward |
/// | --- | --- | --- | --- |
/// | Construction: `VLA dst(std::move(src))` | Any | Adopt source storage; move-construct allocator | 0 |
/// | Construction: `VLA dst(std::move(src), alloc)` | Equal, including `is_always_equal` | Adopt source storage | 0 |
/// | Construction: `VLA dst(std::move(src), alloc)` | Unequal | Allocate storage and relocate elements | `C` (retained) |
/// | Assignment: `dst = std::move(src)` | `propagate_on_container_move_assignment` is true | Release old storage; adopt source allocator and storage | 0 |
/// | Assignment: `dst = std::move(src)` | No propagation; equal, including `is_always_equal` | Release old storage; adopt source storage | 0 |
/// | Assignment: `dst = std::move(src)` | No propagation; unequal; destination capacity sufficient | Move elements into existing storage | `C` (retained) |
/// | Assignment: `dst = std::move(src)` | No propagation; unequal; destination capacity insufficient | Release old storage; allocate replacement storage and relocate elements | `C` (retained) |
// clang-format on
///
/// With exceptions enabled, failed construction destroys every successfully constructed destination element and
/// releases abandoned storage. Failed `reserve()` or `shrink_to_fit()` preserves the original values when relocation
/// copies elements. If a throwing move-only element is relocated, source values may already have been moved from;
/// the container retains ownership and valid size/capacity bookkeeping, but those values are not restored.
///
/// Failed construction during `resize()` destroys the new suffix and retains the original size. Capacity acquired
/// before the failure may remain. Assignment provides valid lifetime and ownership bookkeeping after failure;
/// previously assigned values need not be restored, and replacement of the old buffer may leave an empty array.
/// Move assignment with non-propagating, unequal allocators releases an insufficient destination buffer before
/// allocating its replacement, avoiding simultaneous ownership of the old and new destination buffers. If replacement
/// allocation or element construction throws, the destination is valid but empty. If the existing capacity suffices, a
/// throwing element assignment may leave destination values partially updated. Throwing element moves may also modify
/// source values; the successful-move postconditions above do not apply to failed operations. Element destruction and
/// allocator deallocation must not throw. With exceptions disabled, no rollback is provided, except that an
/// unequal-allocator move construction or move assignment that cannot obtain destination storage leaves the
/// destination empty and the source unchanged. Such a failure is detected by the source remaining non-empty.
///
/// @tparam  T           The type of elements in the array.
/// @tparam Allocator    The allocator type to use for all allocations.
///
template <typename T, typename Allocator>
class variable_length_array : protected detail::variable_length_array_base<T, Allocator>
{
protected:
    using base = detail::variable_length_array_base<T, Allocator>;
    using base::data_;
    using base::size_;
    using base::capacity_;
    using base::max_size_max_;
    using base::alloc_;

public:
    ///
    /// STL-like declaration of the container's storage type.
    ///
    using value_type = typename base::value_type;

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
    /// STL-like declaration of the reverse iterator type.
    ///
    using reverse_iterator = std::reverse_iterator<iterator>;

    ///
    /// STL-like declaration of the const reverse iterator type.
    ///
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    ///
    /// STL-like declaration of the container's difference type.
    ///
    using difference_type = typename base::difference_type;

    ///
    /// STL-like declaration of the container's size type.
    ///
    using size_type = typename base::size_type;

    ///
    /// Per the std::uses_allocator protocol.
    ///
    using allocator_type = typename base::allocator_type;

    ///
    /// STL-like declaration of reference type.
    ///
    using reference = typename std::add_lvalue_reference_t<value_type>;

    ///
    /// STL-like declaration of constant-reference type.
    ///
    using const_reference = typename std::add_lvalue_reference_t<std::add_const_t<value_type>>;

    // +----------------------------------------------------------------------+
    // | CONSTRUCTORS
    // +----------------------------------------------------------------------+
    /// Required constructor for std::uses_allocator protocol.
    /// @param alloc Allocator to use for all allocations.
    explicit variable_length_array(const allocator_type& alloc) noexcept
        : base(alloc, nullptr, 0, 0, std::numeric_limits<size_type>::max())
    {
    }

    /// Required constructor for std::uses_allocator protocol.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     cetl::variable_length_array::max_size() will return
    ///                     `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    /// @param alloc Allocator to use for all allocations.
    explicit variable_length_array(size_type max_size_max, const allocator_type& alloc) noexcept
        : base(alloc, nullptr, 0, 0, max_size_max)
    {
    }

    /// Initializer syntax constructor with maximum max size.
    /// @param l Initializer list of elements to copy into the array.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     cetl::variable_length_array::max_size() will return
    ///                     `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    /// @param alloc Allocator to use for all allocations.
    variable_length_array(std::initializer_list<value_type> l, size_type max_size_max, const allocator_type& alloc)
        : base(alloc, nullptr, 0, 0, max_size_max)
    {
        reserve(l.size());
        size_ = base::fast_copy_construct(data_, capacity_, l.begin(), l.size(), alloc_);
    }

    /// Initializer syntax constructor.
    /// @param l Initializer list of elements to copy into the array.
    /// @param alloc Allocator to use for all allocations.
    variable_length_array(std::initializer_list<value_type> l, const allocator_type& alloc)
        : variable_length_array(l, std::numeric_limits<size_type>::max(), alloc)
    {
    }

    /// Count-value constructor with maximum max size. Constructs `count` copies of `value`, as
    /// `std::vector(count, value, alloc)` does, allocating storage for exactly `count` elements.
    /// The count may be any type that converts to size_type, but not a braced list:
    /// `variable_length_array({x}, n, alloc)` selects the initializer-list constructor. There is no `(count, alloc)`
    /// overload, because `variable_length_array(n, alloc)` sets max_size_max.
    /// With exceptions disabled, the array is truncated to max_size() or to the storage the allocator provides, which
    /// the caller can detect by comparing size() with `count`.
    /// @tparam Count       The type of `count`.
    /// @param count        The number of elements to construct.
    /// @param value        The value to copy into each element.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     cetl::variable_length_array::max_size() will return
    ///                     `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    /// @param alloc        Allocator to use for all allocations.
    /// @throw std::length_error if `count` is greater than max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory to hold `count` elements.
    template <typename Count, typename = detail::enable_if_count_t<Count, size_type>>
    variable_length_array(Count count, const value_type& value, size_type max_size_max, const allocator_type& alloc)
        : base(alloc, nullptr, 0, 0, max_size_max)
    {
        // Elements constructed before a failure are destroyed by resize(); the base destructor releases the storage.
        base::resize(static_cast<size_type>(count), max_size(), value);
    }

    /// Count-value constructor. Constructs `count` copies of `value`, as `std::vector(count, value, alloc)` does.
    /// Has the same guarantees as the constructor that also takes max_size_max.
    /// @tparam Count The type of `count`.
    /// @param count  The number of elements to construct.
    /// @param value  The value to copy into each element.
    /// @param alloc  Allocator to use for all allocations.
    /// @throw std::length_error if `count` is greater than max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory to hold `count` elements.
    template <typename Count, typename = detail::enable_if_count_t<Count, size_type>>
    variable_length_array(Count count, const value_type& value, const allocator_type& alloc)
        : variable_length_array(count, value, std::numeric_limits<size_type>::max(), alloc)
    {
    }

    /// Range constructor with maximum max size.
    /// @tparam InputIt The type of the range's iterators.
    /// @param first    The beginning of the range.
    /// @param last     The end of the range.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     cetl::variable_length_array::max_size() will return `std::min(max_size_max,
    ///                     std::allocator_traits<allocator_type>::max_size(alloc))`
    /// @param alloc    Allocator to use for all allocations.
    template <class InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    variable_length_array(InputIt first, InputIt last, size_type max_size_max, const allocator_type& alloc)
        : base(alloc, nullptr, 0, 0, max_size_max)
    {
        // Accepts the iterators assign(first, last) accepts, but only constructs elements. Elements constructed before
        // a failure are destroyed by the base destructor.
        construct_range(first, last, typename std::iterator_traits<InputIt>::iterator_category{});
    }

    /// Range constructor.
    /// @tparam InputIt The type of the range's iterators.
    /// @param first    The beginning of the range.
    /// @param last     The end of the range.
    /// @param alloc    Allocator to use for all allocations.
    template <class InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    variable_length_array(InputIt first, InputIt last, const allocator_type& alloc)
        : variable_length_array(first, last, std::numeric_limits<size_type>::max(), alloc)
    {
    }

    // +----------------------------------------------------------------------+
    // | RULE OF FIVE
    // +----------------------------------------------------------------------+
    variable_length_array(const variable_length_array& rhs, const allocator_type& alloc)
        : base(rhs, alloc)
    {
        base::reserve(rhs.size(), rhs.max_size());
        size_ = base::fast_copy_construct(data_, capacity_, rhs.data_, rhs.size_, alloc_);
    }

    variable_length_array(const variable_length_array& rhs)
        : variable_length_array(rhs,
                                std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                                    rhs.alloc_))
    {
    }

    variable_length_array& operator=(const variable_length_array& rhs)
    {
        base::copy_assign_from(rhs, rhs.max_size());
        return *this;
    }

    variable_length_array(variable_length_array&& rhs, const allocator_type& alloc) noexcept(
        std::allocator_traits<allocator_type>::is_always_equal::value)
        : base(std::move(rhs), alloc)
    {
    }

    variable_length_array(variable_length_array&& rhs) noexcept
        : base(std::move(rhs))
    {
    }

    variable_length_array& operator=(variable_length_array&& rhs) noexcept(
        std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value ||
        std::allocator_traits<allocator_type>::is_always_equal::value)
    {
        base::template move_assign_from<allocator_type>(std::move(rhs), rhs.max_size());
        return *this;
    }

    /// Replace the contents with the initializer list, keeping the allocator and max_size().
    /// Equivalent to assign(values), with the same guarantees.
    /// @param values The elements to copy into the array.
    /// @return `*this`
    /// @throw std::length_error if `values` has more elements than max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory to hold `values`.
    variable_length_array& operator=(std::initializer_list<value_type> values)
    {
        assign(values);
        return *this;
    }

    ~variable_length_array() = default;

    // +----------------------------------------------------------------------+
    // | COMPARATORS
    // +----------------------------------------------------------------------+
    constexpr bool operator==(const variable_length_array& rhs) const noexcept
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

    constexpr bool operator!=(const variable_length_array& rhs) const noexcept
    {
        return !(operator==(rhs));
    }

#if defined(__cpp_lib_three_way_comparison) && (__cpp_lib_three_way_comparison >= 201907L)
    /// Compares the elements lexicographically, as std::vector's operator<=> does: the first pair that is not
    /// equivalent decides, and otherwise the shorter array is less. Elements are compared with their own operator<=>,
    /// or through operator< when they have none, and the result has the matching ordering category. <, <=, > and >=
    /// are rewritten in terms of this operator, as for std::vector, so partially ordered elements such as a NaN give
    /// the same answers as std::vector does in C++20.
    template <typename U = value_type>
    constexpr auto operator<=>(const variable_length_array& rhs) const
        -> decltype(detail::synth_three_way{}(std::declval<const U&>(), std::declval<const U&>()))
    {
        return std::lexicographical_compare_three_way(cbegin(),
                                                      cend(),
                                                      rhs.cbegin(),
                                                      rhs.cend(),
                                                      detail::synth_three_way{});
    }
#else
    /// Compares the elements lexicographically, as std::vector's operator< does before C++20: the first unequal pair
    /// decides, and otherwise the shorter array is less. Requires value_type to be comparable with operator<. In C++20
    /// these four operators are replaced by operator<=>, as they are for std::vector.
    constexpr bool operator<(const variable_length_array& rhs) const
    {
        return std::lexicographical_compare(cbegin(), cend(), rhs.cbegin(), rhs.cend());
    }

    /// Lexicographic comparison; see operator<.
    constexpr bool operator>(const variable_length_array& rhs) const
    {
        return rhs < *this;
    }

    /// Lexicographic comparison; see operator<.
    constexpr bool operator<=(const variable_length_array& rhs) const
    {
        return !(rhs < *this);
    }

    /// Lexicographic comparison; see operator<.
    constexpr bool operator>=(const variable_length_array& rhs) const
    {
        return !(*this < rhs);
    }
#endif

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
    /// A reverse iterator to the last element, or rend() if the array is empty. Like the forward iterators, reverse
    /// iterators are invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }
    constexpr const_reverse_iterator rbegin() const noexcept
    {
        return crbegin();
    }
    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }

    ///
    /// A reverse iterator to the position before the first element.
    ///
    constexpr reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }
    constexpr const_reverse_iterator rend() const noexcept
    {
        return crend();
    }
    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
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

#if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)

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

    /// Returns a reference to the first element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Reference to the first element in the array.
    constexpr reference front()
    {
        CETL_DEBUG_ASSERT(size() > 0, "CDE_vla_004: Calling front() on an empty array is undefined.");
        return data_[0];
    }

    /// Returns a const reference to the first element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Constant reference to the first element in the array.
    constexpr const_reference front() const
    {
        CETL_DEBUG_ASSERT(size() > 0, "CDE_vla_005: Calling front() on an empty array is undefined.");
        return data_[0];
    }

    /// Returns a reference to the last element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Reference to the last element in the array.
    constexpr reference back()
    {
        // a, perhaps naive, attempt to constrain undefined behaviour considering
        // calling front() on an empty container will return a reference to undefined
        // memory close to the VLA object where as data_[size_t{0} - 1] will reference a
        // random but very distant memory address.
        const size_type current_size = size();
        if (current_size == 0)
        {
            return front();
        }
        return data_[current_size - 1];
    }

    /// Returns a const reference to the last element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Constant reference to the last element in the array.
    constexpr const_reference back() const
    {
        const size_type current_size = size();
        if (current_size == 0)
        {
            return front();
        }
        return data_[current_size - 1];
    }

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
#if defined(__cpp_exceptions)
        try
        {
#endif
            base::shrink_to_fit();
#if defined(__cpp_exceptions)
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
        base::reserve(desired_capacity, max_size());
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
    ///
    /// Destroys all elements in the list but does not release any capacity.
    ///
    constexpr void clear() noexcept(std::is_nothrow_destructible<value_type>::value)
    {
        base::destroy_all();
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
        emplace_back(value);
    }

    ///
    /// Allocate a new element on to the back of the array and move value into it. Grows size by 1 and
    /// may grow capacity.
    ///
    /// See variable_length_array::push_back(value_type&) for full documentation.
    ///
    /// @throw std::length_error if the size of this collection is at capacity.
    /// @throw std::bad_alloc if memory was needed and none could be allocated.
    ///
    constexpr void push_back(value_type&& value)
    {
        emplace_back(std::move(value));
    }

    ///
    /// Remove and destroy the last item in the array. This reduces the array size by 1 unless
    /// the array is already empty.
    ///
    constexpr void pop_back() noexcept(std::is_nothrow_destructible<value_type>::value)
    {
        if (size_ > 0)
        {
            std::allocator_traits<allocator_type>::destroy(alloc_, std::addressof(data_[--size_]));
        }
    }

    /// Removes the element at `pos`, as std::vector::erase does. Has the same guarantees as erase(first, last).
    /// @param pos  The element to remove. It must be dereferenceable, so it cannot be end().
    /// @return An iterator to the element that followed the removed one, which is end() if it was the last.
    iterator erase(const_iterator pos) noexcept(std::is_nothrow_move_assignable<value_type>::value &&
                                                std::is_nothrow_destructible<value_type>::value)
    {
        CETL_DEBUG_ASSERT((cbegin() <= pos) && (pos < cend()), "CDE_vla_009: erase() position is not dereferenceable.");
        return erase(pos, pos + 1);
    }

    /// Removes the elements in [first, last), as std::vector::erase does. Later elements are move-assigned toward the
    /// front and the vacated elements at the end are destroyed. Capacity and max_size() are unchanged, and iterators
    /// before `first` remain valid. An empty range has no effect.
    /// @param first The first element to remove.
    /// @param last  One past the last element to remove. [first, last) must be a valid range of this array.
    /// @return An iterator to the element that followed the removed range, which is end() if the range reached it.
    /// @throw * Any exception thrown by value_type's move assignment. The array then keeps its size and every element
    ///        is valid, but elements from `first` onward may have been assigned or moved from.
    iterator erase(const_iterator first,
                   const_iterator last) noexcept(std::is_nothrow_move_assignable<value_type>::value &&
                                                 std::is_nothrow_destructible<value_type>::value)
    {
        CETL_DEBUG_ASSERT((cbegin() <= first) && (first <= last) && (last <= cend()),
                          "CDE_vla_010: erase() range is not within the array.");
        if (data_ == nullptr)
        {
            // Without storage the array is empty and the range must be too; skip arithmetic on null pointers.
            return end();
        }
        const iterator  position = data_ + (first - cbegin());
        const size_type count    = static_cast<size_type>(last - first);
        if (count > 0)
        {
            (void) std::move(position + count, end(), position);
            base::destroy_suffix(size_ - count);
        }
        return position;
    }

    /// Inserts a copy of `value` before `pos`, as std::vector::insert does. Has the same guarantees as
    /// insert(pos, count, value); `value` may be an element of this array.
    /// @return An iterator to the inserted element.
    iterator insert(const_iterator pos, const value_type& value)
    {
        return insert(pos, 1U, value);
    }

    /// Inserts `value` before `pos` by moving it, as std::vector::insert does. Has the same guarantees as
    /// insert(pos, count, value). As for any rvalue argument, `value` must not refer to an element of this array.
    /// @return An iterator to the inserted element.
    iterator insert(const_iterator pos, value_type&& value)
    {
        const size_type offset = insert_offset(pos);
        if (!reserve_for_insert(1U))
        {
            return iterator_at(offset);
        }
        return insert_appended(offset, [this, &value]() { append_reserved(std::move(value)); });
    }

    /// Inserts `count` copies of `value` before `pos`, as std::vector::insert does. `value` may be an element of this
    /// array. Iterators at or after `pos` are invalidated, and all iterators are if capacity grows; capacity grows
    /// geometrically, as for push_back(), so repeated insertion takes amortized constant time per element moved.
    ///
    /// The new elements are constructed at the end and rotated into place, so value_type must be move-assignable and
    /// swappable, as std::vector requires. If constructing a new element throws, the elements constructed so far are
    /// destroyed and the array is unchanged, although capacity acquired for the insertion may remain. If moving the
    /// existing elements throws, every element remains valid but their order is unspecified.
    ///
    /// With exceptions disabled, an insertion that would exceed max_size(), or for which the allocator does not provide
    /// storage, has no effect; detect it by comparing size() before and after.
    /// @param pos   The element before which to insert; may be end().
    /// @param count The number of copies to insert.
    /// @param value The value to copy.
    /// @return An iterator to the first inserted element, or `pos` if nothing was inserted.
    /// @throw std::length_error if size() + `count` would exceed max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory.
    iterator insert(const_iterator pos, const size_type count, const value_type& value)
    {
        const size_type offset = insert_offset(pos);
        if (base::refers_to_element(value))
        {
            // Growing may relocate the element value refers to before it is copied, so copy it first.
            const value_type copy(value);
            return insert_copies(offset, count, copy);
        }
        return insert_copies(offset, count, value);
    }

    /// Inserts the elements of [first, last) before `pos`, in order, as std::vector::insert does. The range must not
    /// refer to elements of this array. Forward ranges are measured first and have the same guarantees as
    /// insert(pos, count, value). Single-pass ranges are appended one element at a time and then rotated into place; if
    /// that fails with exceptions enabled, the appended elements are destroyed and the array is otherwise unchanged.
    /// With exceptions disabled, a single-pass range stops at max_size() or at failed growth and keeps the inserted
    /// prefix, as assign(first, last) does.
    /// @return An iterator to the first inserted element, or `pos` if the range is empty.
    /// @throw std::length_error if the inserted elements would exceed max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory.
    template <typename InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        return insert_range(insert_offset(pos),
                            first,
                            last,
                            typename std::iterator_traits<InputIt>::iterator_category{});
    }

    /// Inserts the elements of `values` before `pos`. Has the same guarantees as insert(pos, first, last).
    /// @return An iterator to the first inserted element, or `pos` if `values` is empty.
    iterator insert(const_iterator pos, std::initializer_list<value_type> values)
    {
        return insert(pos, values.begin(), values.end());
    }

    /// Like push_back but constructs the object directly in uninitialized memory. As with std::vector, the arguments
    /// may refer to elements of this array, even when the call grows the storage.
    /// @throw throw std::length_error if there was not enough storage for an additional element.
    ///        If exceptions are disabled then the caller must check the array size before and
    ///        after calling the method to determine if it succeeded.
    template <class... Args>
    void emplace_back(Args&&... args)
    {
        // Spare capacity can exceed max_size() after assignment adopts a smaller bound, so check both.
        if ((size_ < capacity_) && (size_ < max_size()))
        {
            std::allocator_traits<allocator_type>::construct(alloc_, data_ + size_, std::forward<Args>(args)...);
            ++size_;
        }
        else if (!base::grow_and_emplace_back(max_size(), std::forward<Args>(args)...))
        {
#if defined(__cpp_exceptions)
            throw std::length_error("size is at capacity and we cannot grow the capacity.");
#endif
        }
    }

    /// Resizes internal storage to count elements default initializing any added elements over the
    /// current size() and deleting any elements under the current size(). If size() == `count` then
    /// this method has no effect.
    ///
    /// @param count    The new size to set for this container.
    /// @throw * If exceptions are enabled then any exceptions that `value_type` constructors or destructors
    ///        throw will escape this call
    /// @throw std::length_error if the size requested is greater than `max_size()`.
    /// @throw std::bad_alloc if the container cannot obtain enough memory to size up to `count`.
    constexpr void resize(size_type count)
    {
        base::resize(count, max_size());
    }

    /// Resizes internal storage to count elements copy-initializing any added elements over the
    /// current size() and deleting any elements under the current size(). If size() == `count` then
    /// this method has no effect.
    ///
    /// @param count    The new size to set for this container.
    /// @param value    The value to copy into any new elements created by the operation.
    /// @throw * If exceptions are enabled then any exceptions that `value_type` constructors or destructors
    ///        throw will escape this call
    /// @throw std::length_error if the size requested is greater than `max_size()`.
    /// @throw std::bad_alloc if the container cannot obtain enough memory to size up to `count`.
    constexpr void resize(size_type count, const value_type& value)
    {
        base::resize(count, max_size(), value);
    }

    /// Set count elements to the given value. Grow the array if needed else
    /// set the size to count without reducing capacity.
    /// Like assign(first, last), insufficient storage is released before replacement storage is allocated, so
    /// growth needs no relocation; allocation failure or a throwing copy then leaves the array empty or partial.
    /// @param count    Number of elements, starting from 0, to set.
    /// @param value    The value to set. It must not refer to an element of this array.
    /// @throw std::length_error if the size requested is greater than `max_size()`.
    /// @throw std::bad_alloc if the container cannot obtain enough memory to size up to `count`.
    constexpr void assign(size_type count, const value_type& value)
    {
        if (count > max_size())
        {
#if defined(__cpp_exceptions)
            throw std::length_error("Requested count exceeds maximum size.");
#else
            count = max_size();
#endif
        }
        base::prepare_assign_storage(count);
        const size_type size_before = size_;
        // Within capacity, resize() only constructs the new suffix.
        resize(std::min(count, capacity_), value);
        const size_type back_fill_count = (size_ > size_before) ? size_before : size_;
        base::fast_copy_assign(data_, back_fill_count, value);
    }

    /// Replace the contents with the elements in [first, last), reusing existing storage where possible.
    /// Single-pass input iterators are traversed once; forward ranges are measured before assignment.
    /// Move iterators move from the source elements. Forward ranges require no relocation of existing elements.
    /// The source range must not refer to elements of this array.
    ///
    /// Forward ranges are checked against max_size() before anything changes. They release insufficient storage and
    /// then allocate storage for the whole range at once; if that allocation fails the array is left empty.
    /// Single-pass ranges grow one element at a time and detect an excessive length only on reaching max_size(), after
    /// the first max_size() elements have been assigned. If any element operation, iterator operation, allocation, or
    /// single-pass length check fails, the array remains valid but may be partially updated.
    ///
    /// With exceptions disabled, forward ranges are truncated to max_size() or left empty by failed allocation, which
    /// the caller can detect by comparing size() with std::distance(first, last). Single-pass ranges stop at max_size()
    /// or at failed growth and keep the assigned prefix; the element that could not be stored is consumed. That cannot
    /// be distinguished from a source that ended at the same length, so bound single-pass sources by the remaining
    /// max_size() and reserve() capacity beforehand when truncation must be detected.
    /// @throw std::length_error if the source range exceeds max_size().
    /// @throw std::bad_alloc if storage cannot be obtained: for the whole forward range, or for the next element of a
    ///                       single-pass range.
    template <typename InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    void assign(InputIt first, InputIt last)
    {
        assign_range_impl(first, last, typename std::iterator_traits<InputIt>::iterator_category{});
    }

    /// Replace the contents with the initializer list. Has the same guarantees as assign(first, last).
    void assign(std::initializer_list<value_type> values)
    {
        assign(values.begin(), values.end());
    }

private:
    // +----------------------------------------------------------------------+

    template <typename InputIt>
    void assign_range_impl(InputIt first, InputIt last, std::input_iterator_tag)
    {
        base::destroy_suffix(base::assign_from_input_range(*this, first, last));
    }

    template <typename ForwardIt>
    void assign_range_impl(ForwardIt first, ForwardIt last, std::forward_iterator_tag)
    {
        base::assign_from_forward_range(first, last, max_size());
    }

    size_type insert_offset(const const_iterator pos) const noexcept
    {
        CETL_DEBUG_ASSERT((cbegin() <= pos) && (pos <= cend()),
                          "CDE_vla_013: insert() position is not within the array.");
        // An array without storage is empty, so the only position is its beginning; avoid subtracting null pointers.
        return (data_ == nullptr) ? 0U : static_cast<size_type>(pos - cbegin());
    }

    iterator iterator_at(const size_type offset) noexcept
    {
        return (data_ == nullptr) ? data_ : (data_ + offset);
    }

    // Makes room for `count` more elements, growing geometrically as push_back() does so that repeated insertion stays
    // amortized. Returns false, with exceptions disabled, when they would exceed max_size() or the allocator declines;
    // the array is then unchanged.
    bool reserve_for_insert(const size_type count)
    {
        const size_type limit = max_size();
        if (count > (limit - std::min(size_, limit)))
        {
#if defined(__cpp_exceptions)
            throw std::length_error("insert() would exceed max_size().");
#else
            return false;
#endif
        }
        const size_type required = size_ + count;
        if (required > capacity_)
        {
            const size_type geometric = capacity_ + std::max<size_type>(capacity_ / 2U, 2U);
            base::reserve(std::min(limit, std::max(required, geometric)), limit);
        }
        return capacity_ >= required;
    }

    // Constructs one element at the end of storage that reserve_for_insert() provided, committing it to size().
    template <typename... Args>
    void append_reserved(Args&&... args)
    {
        std::allocator_traits<allocator_type>::construct(alloc_, data_ + size_, std::forward<Args>(args)...);
        ++size_;
    }

    // Runs `append`, which commits each element it adds to size(), then rotates the added elements to `offset`. If
    // appending throws, the added elements are destroyed and the array is unchanged except, possibly, its capacity.
    template <typename Append>
    iterator insert_appended(const size_type offset, Append append)
    {
        const size_type old_size = size_;
#if defined(__cpp_exceptions)
        try
        {
#endif
            append();
#if defined(__cpp_exceptions)
        } catch (...)
        {
            base::destroy_suffix(old_size);
            throw;
        }
#endif
        if (data_ == nullptr)
        {
            // Nothing was added to an array that never allocated.
            return data_;
        }
        (void) std::rotate(data_ + offset, data_ + old_size, data_ + size_);
        return data_ + offset;
    }

    iterator insert_copies(const size_type offset, const size_type count, const value_type& value)
    {
        if ((count == 0U) || !reserve_for_insert(count))
        {
            return iterator_at(offset);
        }
        return insert_appended(offset, [this, count, &value]() {
            for (size_type i = 0; i < count; ++i)
            {
                append_reserved(value);
            }
        });
    }

    template <typename ForwardIt>
    iterator insert_range(const size_type offset, ForwardIt first, ForwardIt last, std::forward_iterator_tag)
    {
        // Do not subtract equal null pointers.
        const size_type count = (first == last) ? 0U : static_cast<size_type>(std::distance(first, last));
        if ((count == 0U) || !reserve_for_insert(count))
        {
            return iterator_at(offset);
        }
        return insert_appended(offset, [this, &first, &last]() {
            for (; first != last; (void) ++first)
            {
                append_reserved(*first);
            }
        });
    }

    template <typename InputIt>
    iterator insert_range(const size_type offset, InputIt first, InputIt last, std::input_iterator_tag)
    {
        return insert_appended(offset, [this, &first, &last]() {
            for (; first != last; (void) ++first)
            {
                const size_type size_before = size_;
                emplace_back(*first);
                if (size_ == size_before)
                {
                    // With exceptions disabled, emplace_back stops at max_size() or when growth is declined.
                    break;
                }
            }
        });
    }

    // Range construction requires elements to be constructible from *first, as std::vector's does, not assignable.
    template <typename InputIt>
    void construct_range(InputIt first, InputIt last, std::input_iterator_tag)
    {
        for (; first != last; (void) ++first)
        {
            const size_type size_before = size_;
            emplace_back(*first);
            if (size_ == size_before)
            {
                // With exceptions disabled, emplace_back stops at max_size() or when growth is declined.
                break;
            }
        }
    }

    template <typename ForwardIt>
    void construct_range(ForwardIt first, ForwardIt last, std::forward_iterator_tag)
    {
        base::construct_from_forward_range(first, last, max_size());
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
class variable_length_array<bool, Allocator> : protected detail::variable_length_array_base<unsigned char, Allocator>
{
protected:
    using storage = unsigned char;
    static_assert(sizeof(storage) == 1, "Unsigned char != 1 byte is not implemented (contributions welcome).");
    using base = detail::variable_length_array_base<storage, Allocator>;
    using base::data_;
    using base::size_;
    using base::capacity_;
    using base::max_size_max_;
    using base::alloc_;

public:
    using value_type      = bool;
    using difference_type = typename base::difference_type;
    using size_type       = typename base::size_type;
    using allocator_type  = typename base::allocator_type;

    ///
    /// Returns true iff the bit at the specified position is set and the argument is within the range.
    ///
    static constexpr bool test(const variable_length_array& container, const size_type pos) noexcept
    {
        const auto idx = std::make_pair(pos / 8U, pos % 8U);
        return (pos < container.size()) && ((container.data_[idx.first] & (1U << idx.second)) != 0);
    }

    ///
    /// Sets the bit at the specified position to the specified value. Does nothing if position is out of range.
    ///
    static constexpr void set(variable_length_array& container, const size_type pos, const bool value = true) noexcept
    {
        if (pos < container.size())
        {
            const auto idx = std::make_pair(pos / 8U, pos % 8U);
            if (value)
            {
                container.data_[idx.first] = static_cast<storage>(container.data_[idx.first] | (1U << idx.second));
            }
            else
            {
                container.data_[idx.first] = static_cast<storage>(container.data_[idx.first] & (~(1U << idx.second)));
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

        /// Exchanges the bits two proxies refer to, as std::vector<bool>'s reference swap does. Dereferencing an
        /// iterator gives a temporary proxy, which std::swap cannot take, so without this, algorithms that swap
        /// elements through std::iter_swap (std::reverse, std::rotate, std::sort and others) would not compile on bits.
        friend void swap(reference lhs, reference rhs) noexcept
        {
            const bool lhs_value = lhs;
            lhs                  = static_cast<bool>(rhs);
            rhs                  = lhs_value;
        }

    private:
        friend class variable_length_array;

        reference(variable_length_array& array, const size_type index) noexcept
            : array_(array)
            , index_(index)
        {
        }

        // The reference does not need to be copyable because URVO will elide the copy.
        variable_length_array& array_;
        const size_type        index_;
    };
    using const_reference = bool;

    /// Exchanges two bits through their proxies, as std::vector<bool>::swap(reference, reference) does.
    static void swap(reference lhs, reference rhs) noexcept
    {
        const bool lhs_value = lhs;
        lhs                  = static_cast<bool>(rhs);
        rhs                  = lhs_value;
    }

private:
    template <typename A>
    class iterator_impl final
    {
    public:
        using iterator_category = std::random_access_iterator_tag;
        using value_type        = typename A::value_type;
        using difference_type   = typename A::difference_type;
        using reference =
            std::conditional_t<std::is_const<A>::value, typename A::const_reference, typename A::reference>;
        using pointer = void;

        iterator_impl() noexcept = default;

        /// A mutable iterator converts to a const_iterator, as a standard container's iterator does.
        template <typename B,
                  typename = std::enable_if_t<std::is_same<const B, A>::value && !std::is_same<B, A>::value>>
        iterator_impl(const iterator_impl<B>& other) noexcept  // NOLINT(*-explicit-constructor)
            : array_(other.array_)
            , index_(other.index_)
        {
        }

        iterator_impl& operator++() noexcept
        {
            ++index_;
            return *this;
        }
        iterator_impl operator++(int) noexcept
        {
            const iterator_impl tmp(*this);
            ++index_;
            return tmp;
        }
        iterator_impl& operator--() noexcept
        {
            --index_;
            return *this;
        }
        iterator_impl operator--(int) noexcept
        {
            iterator_impl tmp(*this);
            --index_;
            return tmp;
        }

        iterator_impl& operator+=(const difference_type n) noexcept
        {
            index_ = static_cast<size_type>(static_cast<difference_type>(index_) + n);
            return *this;
        }
        iterator_impl& operator-=(const difference_type n) noexcept
        {
            index_ = static_cast<size_type>(static_cast<difference_type>(index_) - n);
            return *this;
        }
        iterator_impl operator+(const difference_type n) const noexcept
        {
            return iterator_impl(*array_, static_cast<size_type>(static_cast<difference_type>(index_) + n));
        }
        iterator_impl operator-(const difference_type n) const noexcept
        {
            return iterator_impl(*array_, static_cast<size_type>(static_cast<difference_type>(index_) - n));
        }
        // The iterator-to-iterator operators are hidden friends rather than members, so an iterator can convert to a
        // const_iterator on either side: bits.begin() != bits.cend() and bits.cend() - bits.begin() both compile.
        friend difference_type operator-(const iterator_impl& lhs, const iterator_impl& rhs) noexcept
        {
            return static_cast<difference_type>(lhs.index_) - static_cast<difference_type>(rhs.index_);
        }

        ///
        /// The return type may be either a reference or const_reference depending on whether this iterator is const
        /// or mutable.
        ///
        auto operator*() -> decltype(std::declval<A>()[0])
        {
            return this->operator[](0);
        }
        reference operator*() const
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
        reference operator[](const difference_type n) const
        {
            return array_->operator[](static_cast<size_type>(static_cast<difference_type>(index_) + n));
        }

        friend bool operator==(const iterator_impl& lhs, const iterator_impl& rhs) noexcept
        {
            return lhs.array_ == rhs.array_ && lhs.index_ == rhs.index_;
        }
        friend bool operator!=(const iterator_impl& lhs, const iterator_impl& rhs) noexcept
        {
            return !(lhs == rhs);
        }
        friend bool operator<(const iterator_impl& lhs, const iterator_impl& rhs) noexcept
        {
            return lhs.index_ < rhs.index_;
        }
        friend bool operator>(const iterator_impl& lhs, const iterator_impl& rhs) noexcept
        {
            return lhs.index_ > rhs.index_;
        }
        friend bool operator<=(const iterator_impl& lhs, const iterator_impl& rhs) noexcept
        {
            return lhs.index_ <= rhs.index_;
        }
        friend bool operator>=(const iterator_impl& lhs, const iterator_impl& rhs) noexcept
        {
            return lhs.index_ >= rhs.index_;
        }

    private:
        friend class variable_length_array;
        template <typename>
        friend class iterator_impl;

        iterator_impl(A& array, const size_type index) noexcept
            : array_(&array)
            , index_(index)
        {
        }

        A*        array_ = nullptr;
        size_type index_ = 0;
    };

public:
    using iterator               = iterator_impl<variable_length_array>;
    using const_iterator         = iterator_impl<const variable_length_array>;
    using reverse_iterator       = std::reverse_iterator<iterator>;
    using const_reverse_iterator = std::reverse_iterator<const_iterator>;

    // +----------------------------------------------------------------------+
    // | CONSTRUCTORS
    // +----------------------------------------------------------------------+

    /// Required constructor for std::uses_allocator protocol with maximum max size.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    ///                     cetl::variable_length_array::max_size() will return
    /// @param alloc Allocator to use for all allocations.
    explicit constexpr variable_length_array(size_type max_size_max, const allocator_type& alloc) noexcept
        : base(alloc, nullptr, 0, 0, max_size_max)
        , last_byte_bit_fill_{0}
    {
    }

    /// Required constructor for std::uses_allocator protocol.
    /// @param alloc Allocator to use for all allocations.
    explicit constexpr variable_length_array(const allocator_type& alloc) noexcept
        : variable_length_array(numeric_limits_max_bits(), alloc)
    {
    }

    /// Initializer syntax constructor with maximum max size.
    /// @param l Initializer list of elements to copy into the array.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     cetl::variable_length_array::max_size() will return
    ///                     `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    /// @param alloc Allocator to use for all allocations.
    variable_length_array(std::initializer_list<bool> l, size_type max_size_max, const allocator_type& alloc)
        : base(alloc, nullptr, 0, 0, max_size_max)
        , last_byte_bit_fill_{0}
    {
        reserve_bits(l.size(), max_size());
        for (auto list_item : l)
        {
            emplace_back(list_item);
        }
    }

    /// Initializer syntax constructor.
    /// @param l Initializer list of elements to copy into the array.
    /// @param alloc Allocator to use for all allocations.
    variable_length_array(std::initializer_list<bool> l, const allocator_type& alloc)
        : variable_length_array(l, numeric_limits_max_bits(), alloc)
    {
    }

    /// Count-value constructor with maximum max size. Constructs `count` copies of `value`, as
    /// `std::vector<bool>(count, value, alloc)` does, allocating the bytes needed for exactly `count` bits.
    /// The count may be any type that converts to size_type, but not a braced list:
    /// `variable_length_array({x}, n, alloc)` selects the initializer-list constructor. There is no `(count, alloc)`
    /// overload, because `variable_length_array(n, alloc)` sets max_size_max.
    /// With exceptions disabled, the array is truncated to max_size() or to the storage the allocator provides, which
    /// the caller can detect by comparing size() with `count`.
    /// @tparam Count       The type of `count`.
    /// @param count        The number of bits to construct.
    /// @param value        The value of each bit.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     cetl::variable_length_array::max_size() will return
    ///                     `std::min(max_size_max, std::allocator_traits<allocator_type>::max_size(alloc))`
    /// @param alloc        Allocator to use for all allocations.
    /// @throw std::length_error if `count` is greater than max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory to hold `count` bits.
    template <typename Count, typename = detail::enable_if_count_t<Count, size_type>>
    variable_length_array(Count count, const bool value, size_type max_size_max, const allocator_type& alloc)
        : base(alloc, nullptr, 0, 0, max_size_max)
        , last_byte_bit_fill_{0}
    {
        resize(static_cast<size_type>(count), value);
    }

    /// Count-value constructor. Constructs `count` copies of `value`, as `std::vector<bool>(count, value, alloc)`
    /// does. Has the same guarantees as the constructor that also takes max_size_max.
    /// @tparam Count The type of `count`.
    /// @param count  The number of bits to construct.
    /// @param value  The value of each bit.
    /// @param alloc  Allocator to use for all allocations.
    /// @throw std::length_error if `count` is greater than max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory to hold `count` bits.
    template <typename Count, typename = detail::enable_if_count_t<Count, size_type>>
    variable_length_array(Count count, const bool value, const allocator_type& alloc)
        : variable_length_array(count, value, numeric_limits_max_bits(), alloc)
    {
    }

    /// Range constructor with maximum max size.
    /// @tparam InputIt The type of the range's iterators.
    /// @param first    The beginning of the range.
    /// @param last     The end of the range.
    /// @param max_size_max Clamping value for the maximum size of this array. That is,
    ///                     cetl::variable_length_array::max_size() will return `std::min(max_size_max,
    ///                     std::allocator_traits<allocator_type>::max_size(alloc))`
    /// @param alloc    Allocator to use for all allocations.
    template <class InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    variable_length_array(InputIt first, InputIt last, size_type max_size_max, const allocator_type& alloc)
        : base(alloc, nullptr, 0, 0, max_size_max)
        , last_byte_bit_fill_{0}
    {
        assign(first, last);
    }

    /// Range constructor.
    /// @tparam InputIt The type of the range's iterators.
    /// @param first    The beginning of the range.
    /// @param last     The end of the range.
    /// @param alloc    Allocator to use for all allocations.
    template <class InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    variable_length_array(InputIt first, InputIt last, const allocator_type& alloc)
        : variable_length_array(first, last, numeric_limits_max_bits(), alloc)
    {
    }

    // +----------------------------------------------------------------------+
    // | RULE OF FIVE
    // +----------------------------------------------------------------------+
    variable_length_array(const variable_length_array& rhs, const allocator_type& alloc)
        : base(rhs, alloc)
        , last_byte_bit_fill_{rhs.last_byte_bit_fill_}
    {
        reserve_bits(rhs.size(), rhs.max_size());
        size_ = base::fast_copy_construct(data_, capacity_, rhs.data_, rhs.size_, alloc_);
    }

    variable_length_array(const variable_length_array& rhs)
        : variable_length_array(rhs,
                                std::allocator_traits<allocator_type>::select_on_container_copy_construction(
                                    rhs.alloc_))
    {
    }

    variable_length_array& operator=(const variable_length_array& rhs)
    {
#if defined(__cpp_exceptions)
        try
        {
#endif
            base::copy_assign_from(rhs, rhs.max_size());
#if defined(__cpp_exceptions)
        } catch (...)
        {
            if (size_ == 0)
            {
                last_byte_bit_fill_ = 0;
            }
            throw;
        }
#endif
        last_byte_bit_fill_ = rhs.last_byte_bit_fill_;
        return *this;
    }

    variable_length_array(variable_length_array&& rhs) noexcept
        : base(std::move(rhs))
        , last_byte_bit_fill_{rhs.last_byte_bit_fill_}
    {
        rhs.last_byte_bit_fill_ = 0;
    }

    variable_length_array(variable_length_array&& rhs, const allocator_type& alloc) noexcept(
        std::allocator_traits<allocator_type>::is_always_equal::value)
        : base(std::move(rhs), alloc)
        , last_byte_bit_fill_{0}
    {
        take_last_byte_bit_fill(rhs);
    }

    variable_length_array& operator=(variable_length_array&& rhs) noexcept(
        std::allocator_traits<allocator_type>::propagate_on_container_move_assignment::value ||
        std::allocator_traits<allocator_type>::is_always_equal::value)
    {
        if (this != &rhs)
        {
            move_assign_bits(rhs);
        }
        return *this;
    }

    /// Replace the contents with the initializer list, keeping the allocator and max_size().
    /// Equivalent to assign(values), with the same guarantees.
    /// @param values The bits to copy into the array.
    /// @return `*this`
    /// @throw std::length_error if `values` has more elements than max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory to hold `values`.
    variable_length_array& operator=(std::initializer_list<bool> values)
    {
        assign(values);
        return *this;
    }

    ~variable_length_array() = default;

    // +----------------------------------------------------------------------+
    // | COMPARATORS
    // +----------------------------------------------------------------------+
    constexpr bool operator==(const variable_length_array& rhs) const noexcept
    {
        if (data_ == rhs.data_)
        {
            return true;
        }
        if (size_ != rhs.size_ || last_byte_bit_fill_ != rhs.last_byte_bit_fill_)
        {
            return false;
        }
        if (size_ > 1 && !std::equal(data_, data_ + size_ - 1, rhs.data_, rhs.data_ + rhs.size_ - 1))
        {
            return false;
        }
        if (size_ == 0)
        {
            return true;
        }
        const storage last_byte_mask = static_cast<storage>((1U << (last_byte_bit_fill_ + 1U)) - 1U);
        return (data_[size_ - 1] & last_byte_mask) == (rhs.data_[size_ - 1] & last_byte_mask);
    }

    constexpr bool operator!=(const variable_length_array& rhs) const noexcept
    {
        return !(operator==(rhs));
    }

#if defined(__cpp_lib_three_way_comparison) && (__cpp_lib_three_way_comparison >= 201907L)
    /// Compares the bits lexicographically, as std::vector<bool>'s operator<=> does: at the first differing bit, false
    /// is less than true, and otherwise the shorter array is less. <, <=, > and >= are rewritten in terms of this.
    constexpr std::strong_ordering operator<=>(const variable_length_array& rhs) const noexcept
    {
        const size_type index = first_difference(rhs);
        return (index < std::min(size(), rhs.size())) ? (test(*this, index) <=> test(rhs, index))
                                                      : (size() <=> rhs.size());
    }
#else
    /// Compares the bits lexicographically, as std::vector<bool>'s operator< does: at the first differing bit, false is
    /// less than true, and otherwise the shorter array is less.
    constexpr bool operator<(const variable_length_array& rhs) const noexcept
    {
        const size_type index = first_difference(rhs);
        return (index < std::min(size(), rhs.size())) ? test(rhs, index) : (size() < rhs.size());
    }

    /// Lexicographic comparison; see operator<.
    constexpr bool operator>(const variable_length_array& rhs) const noexcept
    {
        return rhs < *this;
    }

    /// Lexicographic comparison; see operator<.
    constexpr bool operator<=(const variable_length_array& rhs) const noexcept
    {
        return !(rhs < *this);
    }

    /// Lexicographic comparison; see operator<.
    constexpr bool operator>=(const variable_length_array& rhs) const noexcept
    {
        return !(*this < rhs);
    }
#endif

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
    /// Reverse iterators over the bits, from the last bit back to the first. Like the forward iterators, they are
    /// invalidated by calls to `shrink_to_fit` and `reserve`.
    ///
    constexpr reverse_iterator rbegin() noexcept
    {
        return reverse_iterator(end());
    }
    constexpr const_reverse_iterator rbegin() const noexcept
    {
        return crbegin();
    }
    constexpr const_reverse_iterator crbegin() const noexcept
    {
        return const_reverse_iterator(cend());
    }
    constexpr reverse_iterator rend() noexcept
    {
        return reverse_iterator(begin());
    }
    constexpr const_reverse_iterator rend() const noexcept
    {
        return crend();
    }
    constexpr const_reverse_iterator crend() const noexcept
    {
        return const_reverse_iterator(cbegin());
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

#if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)

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

    /// Returns a reference to the first element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Reference to the first element in the array.
    constexpr reference front()
    {
        CETL_DEBUG_ASSERT(size() > 0, "CDE_vla_006: Calling front() on an empty array is undefined.");
        return this->operator[](0);
    }

    /// Returns a const reference to the first element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Constant reference to the first element in the array.
    constexpr const_reference front() const
    {
        CETL_DEBUG_ASSERT(size() > 0, "CDE_vla_007: Calling front() on an empty array is undefined.");
        return this->operator[](0);
    }

    /// Returns a reference to the last element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Reference to the last element in the array.
    constexpr reference back()
    {
        // a, perhaps naive, attempt to constrain undefined behaviour considering
        // calling front() on an empty container will return a reference to undefined
        // memory close to the VLA object where as data_[size_t{0} - 1] will reference a
        // random but very distant memory address.
        const size_type current_size = size();
        if (current_size == 0)
        {
            return front();
        }
        return this->operator[](current_size - 1);
    }

    /// Returns a const reference to the last element in the array.
    /// Calling this method on an empty array is undefined.
    /// @return Constant reference to the last element in the array.
    constexpr const_reference back() const
    {
        const size_type current_size = size();
        if (current_size == 0)
        {
            return front();
        }
        return this->operator[](current_size - 1);
    }
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

    /// Returns the, theoretical, maximum number of elements that can be stored in this container.
    /// It does not take into account the current state of the allocator. That is, if the allocator
    /// is out of memory this method will still return the maximum number of elements that could
    /// be stored if the allocator had enough memory, however, it will always return the maximum
    /// size passed into the constructor if that value is less than the allocator's max_size.
    /// @return The maximum number of elements that could be stored in this container.
    constexpr size_type max_size() const noexcept
    {
        // We can't simply take the maximum possible bytes and convert that to bits because
        // it could overflow size_type.  Instead we calculate the lesser of the maximum bytes that
        // could be allocated and the maximum bytes that can be represented and convert that into
        // bits.
        const size_type max_diff_bytes = std::numeric_limits<ptrdiff_t>::max() / sizeof(storage);
        const size_type max_alloc_bytes =
            std::min(max_diff_bytes, std::allocator_traits<allocator_type>::max_size(alloc_));
        const size_type max_bytes = std::min(max_alloc_bytes, numeric_limits_max_bits() / 8U);
        return std::min(max_size_max_bits(), max_bytes * 8U);
    }

    /// Reduce the amount of memory held by this object to the minimum required
    /// based on size. This method may not actually deallocate any memory if
    /// if there is not enough memory to allocate a smaller buffer before
    /// moving the existing elements and freeing the larger buffer.
    /// @throws if any items throw while being moved.
    void shrink_to_fit()
    {
#if defined(__cpp_exceptions)
        try
        {
#endif
            base::shrink_to_fit();
#if defined(__cpp_exceptions)
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
        reserve_bits(desired_capacity, max_size());
    }

    // +----------------------------------------------------------------------+
    // | MODIFIERS
    // +----------------------------------------------------------------------+
    ///
    /// Destroys all elements in the list but does not release any capacity.
    ///
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
#if defined(__cpp_exceptions)
            throw std::length_error("max_size is reached, the array cannot grow further");
#endif
            return;
        }
        if (!emplace_back_impl(value))
        {
#if defined(__cpp_exceptions)
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
            --last_byte_bit_fill_;
        }
        else if (size_ > 1)
        {
            --size_;
            last_byte_bit_fill_ = 7;
        }
        else
        {
            size_               = 0;
            last_byte_bit_fill_ = 0;
        }
    }

    /// Removes the bit at `pos`, as std::vector<bool>::erase does. Has the same guarantees as erase(first, last).
    /// @param pos  The bit to remove. It must be dereferenceable, so it cannot be end().
    /// @return An iterator to the bit that followed the removed one, which is end() if it was the last.
    iterator erase(const_iterator pos) noexcept
    {
        CETL_DEBUG_ASSERT((pos.array_ == this) && (pos.index_ < size()),
                          "CDE_vla_011: erase() position is not dereferenceable.");
        return erase(pos, pos + 1);
    }

    /// Removes the bits in [first, last), as std::vector<bool>::erase does. Later bits move toward the front.
    /// Capacity and max_size() are unchanged, and iterators before `first` remain valid. An empty range has no effect.
    /// @param first The first bit to remove.
    /// @param last  One past the last bit to remove. [first, last) must be a valid range of this array.
    /// @return An iterator to the bit that followed the removed range, which is end() if the range reached it.
    iterator erase(const_iterator first, const_iterator last) noexcept
    {
        CETL_DEBUG_ASSERT((first.array_ == this) && (last.array_ == this) && (first.index_ <= last.index_) &&
                              (last.index_ <= size()),
                          "CDE_vla_012: erase() range is not within the array.");
        const size_type count = last.index_ - first.index_;
        if (count > 0)
        {
            const size_type new_size = size() - count;
            for (size_type index = first.index_; index < new_size; ++index)
            {
                set(*this, index, test(*this, index + count));
            }
            set_size_bits(new_size);
        }
        return iterator(*this, first.index_);
    }

    /// Inserts a bit with `value` before `pos`, as std::vector<bool>::insert does. Has the same guarantees as
    /// insert(pos, count, value).
    /// @return An iterator to the inserted bit.
    iterator insert(const_iterator pos, const bool value)
    {
        return insert(pos, 1U, value);
    }

    /// Inserts `count` bits with `value` before `pos`, as std::vector<bool>::insert does. Iterators at or after `pos`
    /// are invalidated, and all iterators are if capacity grows; capacity grows geometrically, as for push_back().
    /// With exceptions disabled, an insertion that would exceed max_size(), or for which the allocator does not provide
    /// storage, has no effect; detect it by comparing size() before and after.
    /// @return An iterator to the first inserted bit, or `pos` if nothing was inserted.
    /// @throw std::length_error if size() + `count` would exceed max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory.
    iterator insert(const_iterator pos, const size_type count, const bool value)
    {
        const size_type offset = insert_offset(pos);
        if ((count == 0U) || !reserve_for_insert(count))
        {
            return iterator(*this, offset);
        }
        const size_type old_size = size();
        resize(old_size + count, value);
        return rotate_into_place(offset, old_size);
    }

    /// Inserts the bits of [first, last) before `pos`, in order, as std::vector<bool>::insert does. The range must not
    /// refer to this array. Forward ranges are measured and storage is obtained first; single-pass ranges grow one
    /// bit at a time. If reading or converting a source element throws, or a single-pass range exceeds max_size(), the
    /// appended bits are removed and the array is unchanged except, possibly, its capacity. With exceptions disabled,
    /// a forward range that does not fit has no effect, while a single-pass range stops at max_size() or at failed
    /// growth and keeps the inserted prefix, as assign(first, last) does.
    /// @return An iterator to the first inserted bit, or `pos` if the range is empty.
    /// @throw std::length_error if the inserted bits would exceed max_size().
    /// @throw std::bad_alloc if the container cannot obtain enough memory.
    template <typename InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    iterator insert(const_iterator pos, InputIt first, InputIt last)
    {
        const size_type offset = insert_offset(pos);
        if (!reserve_range_for_insert(first, last, typename std::iterator_traits<InputIt>::iterator_category{}))
        {
            return iterator(*this, offset);
        }
        const size_type old_size = size();
#if defined(__cpp_exceptions)
        try
        {
#endif
            for (; first != last; (void) ++first)
            {
                const size_type size_before = size();
                emplace_back(*first);
                if (size() == size_before)
                {
                    // With exceptions disabled, emplace_back stops at max_size() or when growth is declined.
                    break;
                }
            }
#if defined(__cpp_exceptions)
        } catch (...)
        {
            set_size_bits(old_size);
            throw;
        }
#endif
        return rotate_into_place(offset, old_size);
    }

    /// Inserts the bits of `values` before `pos`. Has the same guarantees as insert(pos, first, last).
    /// @return An iterator to the first inserted bit, or `pos` if `values` is empty.
    iterator insert(const_iterator pos, std::initializer_list<bool> values)
    {
        return insert(pos, values.begin(), values.end());
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
#if defined(__cpp_exceptions)
            throw std::length_error("max_size is reached, the array cannot grow further");
#endif
            return;
        }
        if (!emplace_back_impl(value))
        {
#if defined(__cpp_exceptions)
            throw std::length_error("max_size is reached, the array cannot grow further");
#endif
        }
    }

    /// Resizes internal storage to count elements default initializing any added elements over the
    /// current size() and deleting any elements under the current size(). If size() == `count` then
    /// this method has no effect.
    ///
    /// @param count    The new size, in bits, to set for this container.
    /// @throw std::length_error if the size requested is greater than `max_size()`.
    /// @throw std::bad_alloc if the container cannot obtain enough memory to size up to `count`.
    constexpr void resize(size_type count)
    {
        resize(count, false);
    }

    /// Resizes internal storage to count elements copy-initializing any added elements over the
    /// current size() and deleting any elements under the current size(). If size() == `count` then
    /// this method has no effect.
    ///
    /// @param count    The new size, in bits, to set for this container.
    /// @param value    The value to copy into any new elements created by the operation.
    /// @throw std::length_error if the size requested is greater than `max_size()`.
    /// @throw std::bad_alloc if the container cannot obtain enough memory to size up to `count`.
    constexpr void resize(size_type count, const bool value)
    {
        size_type clamped_count = count;
        if (clamped_count > max_size())
        {
#if defined(__cpp_exceptions)
            throw std::length_error("Requested count exceeds maximum size.");
#else
            // deviation from the standard: instead of undefined behaviour we clamp the count to the maximum size
            // when exceptions are disabled.
            clamped_count = max_size();
#endif
        }

        const std::size_t current_count = size_bits();
        if (clamped_count != current_count)
        {
            std::size_t byte_sized = bits2bytes(clamped_count);
            if (byte_sized > capacity_)
            {
                reserve_bits(clamped_count, max_size());
                if (byte_sized > capacity_)
                {
                    // With exceptions disabled the allocator declined growth; fill only the storage we have.
                    clamped_count = capacity_bits();
                    byte_sized    = capacity_;
                }
            }
            if (byte_sized == 0)
            {
                size_               = 0;
                last_byte_bit_fill_ = 0;
            }
            else
            {
                storage bit_sized = (clamped_count - 1) % 8U;
                if (byte_sized > size_)
                {
                    // Go ahead and just set all bits in the last bytes since we use masks
                    // when we do comparisons.
                    (void) std::memset(&data_[size_], (value) ? 0xFF : 0, byte_sized - size_);
                }
                if (size_ > 0)
                {
                    const storage     existing_byte  = data_[size_ - 1];
                    const std::size_t bit_size_delta = 8U - (last_byte_bit_fill_ + 1U);
                    const storage existing_bits_mask = static_cast<storage>((1U << (last_byte_bit_fill_ + 1U)) - 1U);
                    if (value)
                    {
                        const storage new_bits =
                            static_cast<storage>(((1U << bit_size_delta) - 1U) << (last_byte_bit_fill_ + 1U));
                        data_[size_ - 1] = (existing_byte & existing_bits_mask) | new_bits;
                    }
                    else
                    {
                        data_[size_ - 1] = existing_byte & existing_bits_mask;
                    }
                }
                size_               = byte_sized;
                last_byte_bit_fill_ = bit_sized;
            }
            (void) value;
        }
        // else no change
    }

    /// Set count elements to the given value. Grow the array if needed else
    /// set the size to count without reducing capacity.
    /// Like assign(first, last), insufficient storage is released before replacement storage is allocated.
    /// @param count    Number of elements, starting from 0, to set.
    /// @param value    The value to set.
    /// @throw std::length_error if the size requested is greater than `max_size()`.
    /// @throw std::bad_alloc if the container cannot obtain enough memory to size up to `count`.
    constexpr void assign(size_type count, const bool& value)
    {
        if (count > max_size())
        {
#if defined(__cpp_exceptions)
            throw std::length_error("Requested count exceeds maximum size.");
#else
            count = max_size();
#endif
        }
        if (count > capacity())
        {
            // As in assign(first, last), release insufficient storage before allocating its replacement.
            clear();
            base::prepare_assign_storage(bits2bytes(count));
        }
        const size_type size_before = size_;
        resize(std::min(count, capacity()), value);
        const size_type bytes_to_overwrite = (size_ > size_before) ? size_before : size_;
        // With no bytes to overwrite data_ may be null, and memset(nullptr, v, 0) is undefined.
        if (bytes_to_overwrite > 0)
        {
            (void) std::memset(data_, (value) ? 0xFF : 0, bytes_to_overwrite);
        }
    }

    /// Replace the contents with the bits in [first, last), reusing existing storage where possible.
    /// Single-pass input iterators are traversed once; forward ranges are measured before assignment.
    /// The source range must not refer to elements of this array.
    ///
    /// Forward ranges are checked against max_size() before anything changes. They release insufficient storage and
    /// then allocate storage for the whole range at once; if that allocation fails the array is left empty.
    /// Single-pass ranges grow one bit at a time and detect an excessive length only on reaching max_size(), after
    /// the first max_size() bits have been assigned. If any iterator operation, value conversion, allocation, or
    /// single-pass length check fails, the array remains valid but may be partially updated.
    ///
    /// With exceptions disabled, forward ranges are truncated to max_size() or left empty by failed allocation, which
    /// the caller can detect by comparing size() with std::distance(first, last). Single-pass ranges stop at max_size()
    /// or at failed growth and keep the assigned prefix; the bit that could not be stored is consumed. That cannot be
    /// distinguished from a source that ended at the same length, so bound single-pass sources by the remaining
    /// max_size() and reserve() capacity beforehand when truncation must be detected.
    /// @throw std::length_error if the source range exceeds max_size().
    /// @throw std::bad_alloc if storage cannot be obtained: for the whole forward range, or for the next bit of a
    ///                       single-pass range.
    template <typename InputIt, typename = detail::enable_if_input_iterator_t<InputIt>>
    void assign(InputIt first, InputIt last)
    {
        assign_range_impl(first, last, typename std::iterator_traits<InputIt>::iterator_category{});
    }

    /// Replace the contents with the initializer list. Has the same guarantees as assign(first, last).
    void assign(std::initializer_list<bool> values)
    {
        assign(values.begin(), values.end());
    }

private:
    template <typename InputIt>
    void assign_range_impl(InputIt first, InputIt last, std::input_iterator_tag)
    {
        const size_type assigned = base::assign_from_input_range(*this, first, last);
        if (size() > assigned)
        {
            // Only shrinks; it cannot allocate.
            resize(assigned);
        }
    }

    template <typename ForwardIt>
    void assign_range_impl(ForwardIt first, ForwardIt last, std::forward_iterator_tag)
    {
        const size_type count = base::assignment_range_size(first, last, max_size());
        if (count > capacity())
        {
            clear();
            base::prepare_assign_storage(bits2bytes(count));
        }
        const size_type available = std::min(count, capacity());
        // Pack eight source values per byte store. size() is committed only at the end; if a source operation throws,
        // some bytes may already hold new bits while the old size() is kept.
        size_type index = 0;
        while (index < available)
        {
            unsigned       byte = 0U;
            const unsigned bits = static_cast<unsigned>(std::min<size_type>(8U, available - index));
            for (unsigned bit = 0U; bit < bits; ++bit, (void) ++first)
            {
                bool value = false;
                base::assign_element(value, *first);
                byte |= (value ? 1U : 0U) << bit;
            }
            data_[index / 8U] = static_cast<storage>(byte);
            index += bits;
        }
        set_size_bits(available);
    }

    void move_assign_bits(variable_length_array& rhs)
    {
#if defined(__cpp_exceptions)
        try
        {
#endif
            base::template move_assign_from<allocator_type>(std::move(rhs), rhs.max_size());
#if defined(__cpp_exceptions)
        } catch (...)
        {
            if (size_ == 0)
            {
                last_byte_bit_fill_ = 0;
            }
            throw;
        }
#endif
        take_last_byte_bit_fill(rhs);
    }

    // Takes the source's partial-byte bookkeeping after a move emptied it. With exceptions disabled a declined
    // allocation transfers nothing, leaving this array empty and the source unchanged.
    void take_last_byte_bit_fill(variable_length_array& rhs) noexcept
    {
        if (rhs.size_ == 0)
        {
            last_byte_bit_fill_     = rhs.last_byte_bit_fill_;
            rhs.last_byte_bit_fill_ = 0;
        }
        else
        {
            last_byte_bit_fill_ = 0;
        }
    }

    constexpr bool ensure_size_plus_one()
    {
        if (capacity_ > 0 && (last_byte_bit_fill_ < 7 || size_ < capacity_))
        {
            // we have at least one byte of capacity (first allocation)
            // and we have room in the last byte or we have room for another byte
            // but we haven't reached the max
            return size_bits() < max_size();
        }

        // base::grow works in storage bytes, but max_size() is in bits.
        return base::grow(bits2bytes(max_size()));
    }

    constexpr bool emplace_back_impl(bool value)
    {
        const size_type index = size_bits();
        if (size_ == 0 || last_byte_bit_fill_ == 7)
        {
            // we are using a bit in the next byte so we have to bump the size.
            if (size_ >= capacity_)
            {
                // nope. Can't grow.
                return false;
            }
            ++size_;
            last_byte_bit_fill_ = 0;
        }
        else
        {
            ++last_byte_bit_fill_;
        }
        set(*this, index, value);
        return true;
    }

    constexpr void reserve_bits(const size_type desired_capacity, const size_type max_size)
    {
        size_type clamped_capacity = desired_capacity;
        if (clamped_capacity > max_size)
        {
#if defined(__cpp_exceptions)
            throw std::length_error("Requested capacity exceeds maximum size.");
#else
            // deviation from the standard: instead of undefined behaviour we clamp the capacity to the maximum size
            // when exceptions are disabled.
            clamped_capacity = max_size;
#endif
        }
        base::reserve(bits2bytes(clamped_capacity), bits2bytes(max_size));
    }

    constexpr static size_type round8(const size_type value) noexcept
    {
        return (value + 7U) & ~static_cast<size_type>(7U);  // Careful, make sure the mask is long enough
    }
    constexpr static size_type bits2bytes(const size_type value) noexcept
    {
        return round8(value) / 8U;
    }

    /// Records a size of `bits` bits, which must fit in the current capacity. storage contents are unchanged.
    constexpr void set_size_bits(const size_type bits) noexcept
    {
        size_               = bits2bytes(bits);
        last_byte_bit_fill_ = static_cast<storage>((bits == 0U) ? 0U : ((bits - 1U) % 8U));
    }

    size_type insert_offset(const const_iterator pos) const noexcept
    {
        CETL_DEBUG_ASSERT((pos.array_ == this) && (pos.index_ <= size()),
                          "CDE_vla_014: insert() position is not within the array.");
        return pos.index_;
    }

    // Makes room for `count` more bits, growing geometrically as push_back() does. Returns false, with exceptions
    // disabled, when they would exceed max_size() or the allocator declines; the array is then unchanged.
    bool reserve_for_insert(const size_type count)
    {
        const size_type limit = max_size();
        if (count > (limit - std::min(size(), limit)))
        {
#if defined(__cpp_exceptions)
            throw std::length_error("insert() would exceed max_size().");
#else
            return false;
#endif
        }
        const size_type required = size() + count;
        if (required > capacity())
        {
            const size_type limit_bytes = bits2bytes(limit);
            const size_type geometric   = capacity_ + std::max<size_type>(capacity_ / 2U, 2U);
            base::reserve(std::min(limit_bytes, std::max(bits2bytes(required), geometric)), limit_bytes);
        }
        return capacity() >= required;
    }

    // A forward range is measured so its storage is obtained once, before any bit is added.
    template <typename ForwardIt>
    bool reserve_range_for_insert(ForwardIt first, ForwardIt last, std::forward_iterator_tag)
    {
        const size_type count = (first == last) ? 0U : static_cast<size_type>(std::distance(first, last));
        return (count == 0U) || reserve_for_insert(count);
    }

    // A single-pass range cannot be measured; it grows as it is appended.
    template <typename InputIt>
    bool reserve_range_for_insert(InputIt, InputIt, std::input_iterator_tag) noexcept
    {
        return true;
    }

    // Rotates the bits appended since `old_size` to `offset`, the second half of every insert.
    iterator rotate_into_place(const size_type offset, const size_type old_size)
    {
        (void) std::rotate(begin() + static_cast<difference_type>(offset),
                           begin() + static_cast<difference_type>(old_size),
                           end());
        return iterator(*this, offset);
    }

    /// The index of the first bit at which this array and rhs differ, or the length of the shorter array if they do
    /// not differ before it.
    constexpr size_type first_difference(const variable_length_array& rhs) const noexcept
    {
        const size_type common = std::min(size(), rhs.size());
        size_type       index  = 0;
        while ((index < common) && (test(*this, index) == test(rhs, index)))
        {
            ++index;
        }
        return index;
    }

    constexpr size_type size_bits() const noexcept
    {
        CETL_DEBUG_ASSERT(last_byte_bit_fill_ <= 7, "CDE_vla_001: last_byte_bit_fill_ is out of range.");
        CETL_DEBUG_ASSERT(size_ <= capacity_, "CDE_vla_002: size_ is out of range.");
        CETL_DEBUG_ASSERT(size_ != 0 || last_byte_bit_fill_ == 0,
                          "CDE_vla_003: last_byte_bit_fill_ should always be zero when size_ is.");
        return (size_ == 0) ? 0 : ((size_ - 1) * 8U) + (last_byte_bit_fill_ + 1U);
    }

    /// Note that the underlying size_ and capacity_ of detail::variable_length_array_base are in terms
    /// of bytes and then converted to bits in size_bits() and capacity_bits().  However
    /// bool VLA (unlike type T VLA) treats max_size_max_ in terms of bits and thus no conversion
    /// is needed.
    constexpr size_type max_size_max_bits() const noexcept
    {
        return max_size_max_;
    }

    constexpr size_type capacity_bits() const noexcept
    {
        return capacity_ * 8U;
    }

    constexpr static size_type numeric_limits_max_bits()
    {
        // Make sure it ends on a byte boundary because of the various maths in other places that
        // convert between bits and bytes
        const size_type max_bits = std::numeric_limits<size_type>::max() - (std::numeric_limits<size_type>::max() % 8U);

        // numeric_limits_max_bits() passes through round8() so we need to make sure it doesn't overflow
        static_assert(max_bits + 7U > max_bits, "Uh oh, overflow");

        return max_bits;
    }

    /// The number of bits that are valid in the last byte of the array. If size_ == 0 this value
    /// has no meaning, however, it should always be 0 if size_ == 0 such that, when size_ == 0, size_++
    /// will immediately be valid as 1-bit in the first byte. See size_bits() implementation for more
    /// details.
    storage last_byte_bit_fill_;
};

/// @deprecated Renamed to cetl::variable_length_array; this alias will be removed in a future major version.
template <typename T, typename Allocator>
using VariableLengthArray [[deprecated("renamed to cetl::variable_length_array")]] =
    variable_length_array<T, Allocator>;

}  // namespace cetl

#endif  // CETL_VARIABLE_LENGTH_ARRAY_HPP_INCLUDED
