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
#include <utility>

namespace cetl
{
namespace pf17
{
namespace detail
{
namespace mem
{
/// Internal construction/rollback engine. The caller supplies construction and nonthrowing destruction operations,
/// allowing allocator-aware callers to customize object lifetime management without changing the public algorithm.
/// Construct must create one object at the supplied address or throw without leaving a live object there.
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
            construct(std::addressof(*current), *first);
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

/// Polyfill for the sequential, three-argument overload of std::uninitialized_move.
/// Move-constructs objects from [first, last) into uninitialized storage starting at destination and returns the end
/// of the constructed range. Execution-policy overloads and uninitialized_move_n are not provided.
///
/// InputIt must meet the input iterator requirements. ForwardIt must meet the nonthrowing forward iterator requirements
/// for uninitialized-memory algorithms: its assignment, comparison, increment, and indirection must not
/// throw, and indirection must yield a reference to its value_type. The destination must provide sufficient, suitably
/// aligned uninitialized storage, and the source and destination ranges must not overlap.
///
/// If construction or an input iterator operation throws, all objects constructed by this call are destroyed in an
/// unspecified order before the exception is rethrown. Destruction must not throw. Source elements may already have
/// been moved from; their original values are not restored. This algorithm always uses move semantics, even when
/// copying could avoid a throwing move constructor.
///
/// The caller owns the storage: this function neither allocates nor deallocates it. On success, the caller is also
/// responsible for destroying the constructed objects.
template <typename InputIt, typename ForwardIt>
ForwardIt uninitialized_move(InputIt first, InputIt last, ForwardIt destination)
{
    using value_type = typename std::iterator_traits<ForwardIt>::value_type;
    return detail::mem::uninitialized_construct(
        first,
        last,
        destination,
        [](value_type* address, auto&& source) { ::new (static_cast<void*>(address)) value_type(std::move(source)); },
        [](value_type* address) { address->~value_type(); });
}

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_MEMORY_HPP_INCLUDED
