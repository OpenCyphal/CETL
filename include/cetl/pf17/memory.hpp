/// @file
/// Defines C++17 entities from the \c memory header.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_MEMORY_HPP_INCLUDED
#define CETL_PF17_MEMORY_HPP_INCLUDED

#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

namespace cetl
{
namespace pf17
{
namespace detail
{
namespace mem
{
// Dispatch at the construction site so a prvalue is never materialized as a callback argument.
template <typename T, typename InputIt>
void move_construct_at(T* address, InputIt& source, std::true_type)
{
    ::new (static_cast<void*>(address)) T(std::move(*source));
}

template <typename T, typename InputIt>
void move_construct_at(T* address, InputIt& source, std::false_type)
{
    ::new (static_cast<void*>(address)) T(*source);
}

/// Constructs a range using caller-supplied lifetime operations and rolls back on exceptions.
/// The callbacks allow allocator-aware lifetime management. This helper neither dereferences the source iterator
/// nor allocates or deallocates storage.
///
/// The source range must be valid and the destination must provide sufficient, suitably aligned uninitialized
/// storage. If `construct` throws, it must leave no live object at the supplied destination address.
///
/// @tparam InputIt    Source input iterator type.
/// @tparam ForwardIt  Destination forward iterator type with nonthrowing iterator operations.
/// @tparam Construct  Callable accepting a destination pointer and an InputIt reference.
/// @tparam Destroy    Nonthrowing callable accepting a pointer to a constructed destination object.
/// @param first       Beginning of the source range.
/// @param last        End of the source range.
/// @param destination Beginning of the destination storage.
/// @param construct   Operation that constructs one object without advancing the supplied source iterator.
/// @param destroy     Operation that destroys one successfully constructed object without releasing its storage.
/// @return Iterator immediately past the constructed destination range, or `destination` if the source range is empty.
/// @throws ... With exceptions enabled, propagates any exception from `construct` or a source iterator operation
///             after invoking `destroy` exactly once for each successfully constructed destination object.
template <typename InputIt, typename ForwardIt, typename Construct, typename Destroy>
ForwardIt uninitialized_construct(InputIt   first,
                                  InputIt   last,
                                  ForwardIt destination,
                                  Construct construct,
                                  Destroy   destroy)
{
    ForwardIt current = destination;
#if defined(__cpp_exceptions)
    try
    {
#else
    (void) destroy;
#endif
        // Record each successful construction before advancing the potentially throwing input iterator.
        for (; first != last; (void) ++current, ++first)
        {
            construct(std::addressof(*current), first);
        }
        return current;
#if defined(__cpp_exceptions)
    } catch (...)
    {
        for (; destination != current; ++destination)
        {
            destroy(std::addressof(*destination));
        }
        throw;
    }
#endif
}
}  // namespace mem
}  // namespace detail

// --------------------------------------------------------------------------------------------------------------------

/// Polyfill for std::uninitialized_move.
/// Implements the sequential, three-argument overload. Constructs objects from `[first, last)` in uninitialized
/// storage starting at `destination`. Execution-policy overloads and `uninitialized_move_n` are not provided.
///
/// Lvalue sources are moved from, even when copying could avoid a throwing move constructor. Other source expressions
/// retain their value category, as specified by [LWG 3918](https://cplusplus.github.io/LWG/issue3918).
/// The destination value type must be constructible from `std::move(*first)` for lvalue source expressions, or from
/// `*first` otherwise, under the active C++ language version's initialization rules.
///
/// The source range must be valid and must not overlap the destination range. The destination must provide sufficient,
/// suitably aligned uninitialized storage for every source element. Destruction of destination objects must not throw.
/// The caller owns the destination storage; this function neither allocates nor deallocates it. On success, all
/// constructed objects remain alive and the caller is responsible for destroying them.
///
/// When dereferencing the source produces a prvalue of the destination type, C++17 and newer require no copy/move
/// constructor for destination construction. In C++14, the selected copy/move constructor must be accessible and
/// not deleted, and callers must not assume that its invocation will be elided.
///
/// Source elements may already have been moved from when an exception occurs; their original values are not restored.
/// When compiled with exceptions disabled, no exception rollback is provided.
///
/// Complexity is linear in the number of source elements, with one destination construction per element on success.
///
/// @tparam InputIt    Source iterator type meeting the input iterator requirements.
/// @tparam ForwardIt  Destination iterator type meeting the forward iterator requirements. Assignment, comparison,
///                    increment, and indirection through valid iterators must not throw, and indirection must yield
///                    a reference to the iterator's value_type.
/// @param first       Beginning of the source range.
/// @param last        End of the source range.
/// @param destination Beginning of the destination storage.
/// @return Iterator immediately past the constructed destination range, or `destination` if the source range is empty.
/// @throws ... With exceptions enabled, propagates any exception from construction or a source iterator operation.
///             Every destination object successfully constructed by this call is destroyed exactly once before
///             propagation, in an unspecified order. The object whose constructor failed is not destroyed.
template <typename InputIt, typename ForwardIt>
ForwardIt uninitialized_move(InputIt first, InputIt last, ForwardIt destination)
{
    using value_type = typename std::iterator_traits<ForwardIt>::value_type;
    return detail::mem::uninitialized_construct(
        first,
        last,
        destination,
        [](value_type* address, InputIt& source) {
            detail::mem::move_construct_at(address, source, std::is_lvalue_reference<decltype(*source)>{});
        },
        [](value_type* address) { address->~value_type(); });
}

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_MEMORY_HPP_INCLUDED
