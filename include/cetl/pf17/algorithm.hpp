/// @file
/// Defines C++17 entities from the \c algorithm header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_ALGORITHM_HPP_INCLUDED
#define CETL_PF17_ALGORITHM_HPP_INCLUDED

#include <functional>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

namespace cetl
{
namespace pf17
{

/// Polyfill for std::clamp with a user-provided comparison (C++17, [alg.clamp]).
/// Returns `lo` if `v` is less than `lo`, `hi` if `hi` is less than `v`, otherwise `v`, where "less" is `comp`.
/// The result is a reference to one of the arguments, so none of them may be a temporary that outlives the call.
///
/// @pre `!comp(hi, lo)` (CDE_algorithm_001).
template <typename T, typename Compare>
CETL_NODISCARD constexpr const T& clamp(const T& v, const T& lo, const T& hi, Compare comp)
{
    CETL_DEBUG_ASSERT(!comp(hi, lo), "CDE_algorithm_001: clamp requires that hi is not less than lo.");
    return comp(v, lo) ? lo : (comp(hi, v) ? hi : v);
}

/// Polyfill for std::clamp using `operator<` (C++17, [alg.clamp]).
/// @pre `!(hi < lo)` (CDE_algorithm_001).
template <typename T>
CETL_NODISCARD constexpr const T& clamp(const T& v, const T& lo, const T& hi)
{
    // Qualified: an unqualified call would also find std::clamp through ADL on std::less (and on std types T).
    return cetl::pf17::clamp(v, lo, hi, std::less<>{});
}

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_ALGORITHM_HPP_INCLUDED
