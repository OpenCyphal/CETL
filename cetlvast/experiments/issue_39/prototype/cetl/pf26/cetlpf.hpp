/// Prototype of cetl/pf26/cetlpf.hpp: selects std (new names), std (pre-rename names), or the CETL polyfill.
#ifndef CETL_PF26_H_INCLUDED
#define CETL_PF26_H_INCLUDED

#include "cetl/cetl.hpp"
#include <numeric>  // defines __cpp_lib_saturation_arithmetic where the library provides the feature

#if defined(__cpp_lib_saturation_arithmetic) && (__cpp_lib_saturation_arithmetic >= 202603L) && !defined(CETL_DOXYGEN)
#    define CETL_PF26_SATURATION_SOURCE "std (P4052R0 names)"
namespace cetl
{
using std::saturating_add;
using std::saturating_sub;
using std::saturating_mul;
using std::saturating_div;
using std::saturating_cast;
}  // namespace cetl
#elif defined(__cpp_lib_saturation_arithmetic) && (__cpp_lib_saturation_arithmetic >= 202311L) && !defined(CETL_DOXYGEN)
#    define CETL_PF26_SATURATION_SOURCE "std (P0543R3 names, adapted)"
namespace cetl
{
// Libraries shipped between P0543R3 and P4052R0 (libstdc++ 14-15, libc++ 18-22) only have the old names.
template <typename T>
CETL_NODISCARD constexpr T saturating_add(T x, T y) noexcept
{
    return std::add_sat(x, y);
}
template <typename T>
CETL_NODISCARD constexpr T saturating_sub(T x, T y) noexcept
{
    return std::sub_sat(x, y);
}
template <typename T>
CETL_NODISCARD constexpr T saturating_mul(T x, T y) noexcept
{
    return std::mul_sat(x, y);
}
template <typename T>
CETL_NODISCARD constexpr T saturating_div(T x, T y) noexcept
{
    return std::div_sat(x, y);
}
template <typename T, typename U>
CETL_NODISCARD constexpr T saturating_cast(U x) noexcept
{
    return std::saturate_cast<T>(x);
}
}  // namespace cetl
#else
#    define CETL_PF26_SATURATION_SOURCE "cetl::pf26 polyfill"
#    include "cetl/pf26/numeric.hpp"
namespace cetl
{
using cetl::pf26::saturating_add;
using cetl::pf26::saturating_sub;
using cetl::pf26::saturating_mul;
using cetl::pf26::saturating_div;
using cetl::pf26::saturating_cast;
}  // namespace cetl
#endif

#endif  // CETL_PF26_H_INCLUDED
