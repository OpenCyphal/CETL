/// @file
/// Defines the C++17 std::variant type and several related entities.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_VARIANT_HPP_INCLUDED
#define CETL_PF17_VARIANT_HPP_INCLUDED

#include <cetl/_helpers.hpp>
#include <cetl/pf17/utility.hpp>
#include <cetl/pf17/attribute.hpp>

#include <tuple>
#include <limits>
#include <algorithm>
#include <exception>  // We need this even if exceptions are disabled for std::terminate.
#include <type_traits>

namespace cetl
{
namespace pf17
{
/// Implementation of \ref std::variant_npos.
constexpr std::size_t variant_npos = std::numeric_limits<std::size_t>::max();

namespace detail
{
namespace var
{
template <std::size_t N, typename... Ts>
using nth_type = std::tuple_element_t<N, std::tuple<Ts...>>;

/// index_of<> fails with a missing type error if the type is not found in the sequence.
/// If there is more than one matching type, the index of the first occurrence is selected.
template <typename T, typename... Ts>
struct index_of_impl;
template <typename T, typename... Ts>
struct index_of_impl<T, T, Ts...>
{
    static constexpr std::size_t value = 0;
};
template <typename T, typename U, typename... Ts>
struct index_of_impl<T, U, Ts...>
{
    static constexpr std::size_t value = 1 + index_of_impl<T, Ts...>::value;
};
template <typename T, typename... Ts>
constexpr std::size_t index_of = index_of_impl<T, Ts...>::value;

static_assert(0 == index_of<int, int>, "");
static_assert(0 == index_of<int, int, double, char>, "");
static_assert(1 == index_of<double, int, double, char>, "");
static_assert(2 == index_of<char, int, double, char>, "");

/// This type manages the storage arena for the variant and keeps track of the currently active type.
/// It expressly ignores copy/move/destruction/assignment; this must be managed externally.
template <typename... Ts>
class arena final
{
public:
    template <std::size_t N, typename... Args>
    explicit arena(const in_place_index_t<N>, Args&&... args)
    {
        construct<N>(std::forward<Args>(args)...);
    }
    template <typename T, typename... Args>
    explicit arena(const in_place_type_t<T>, Args&&... args)
        : arena(in_place_index<index_of<T, Ts...>>, std::forward<Args>(args)...)
    {
    }
    arena()  // #0 must be default-constructible, otherwise this type is not default-constructible either.
        : arena(in_place_index<0>)
    {
    }

    /// Constructs the new value in the arena; the caller is responsible for destroying the old value beforehand.
    template <std::size_t N, typename... Args>
    auto& construct(Args&&... args)
    {
        using T = nth_type<N, Ts...>;
        m_index = N;
        m_dtor  = [](void* const data) { reinterpret_cast<T*>(data)->~T(); };
        return *new (m_data) T(std::forward<Args>(args)...);
    }
    template <typename T, typename... Args>
    auto& construct(Args&&... args)
    {
        return construct<index_of<T, Ts...>>(std::forward<Args>(args)...);
    }

    /// Destroys the value and makes this arena valueless.
    void destroy() noexcept
    {
        m_dtor(m_data);
        m_dtor  = nullptr;
        m_index = variant_npos;
    }

    /// Returns variant_npos if valueless.
    CETL_NODISCARD std::size_t index() const noexcept
    {
        return m_index;
    }

    // clang-format off
    template <typename T>       T&  as()      &  { return *reinterpret_cast<T*>(m_data); }
    template <typename T> const T&  as() const&  { return *reinterpret_cast<const T*>(m_data); }
    template <typename T>       T&& as()      && { return std::move(*reinterpret_cast<T*>(m_data)); }
    template <typename T> const T&& as() const&& { return std::move(*reinterpret_cast<const T*>(m_data)); }

    template <std::size_t N>       auto&  as()      &  { return as<nth_type<N, Ts...>>(); }
    template <std::size_t N> const auto&  as() const&  { return as<nth_type<N, Ts...>>(); }
    template <std::size_t N>       auto&& as()      && { return as<nth_type<N, Ts...>>(); }
    template <std::size_t N> const auto&& as() const&& { return as<nth_type<N, Ts...>>(); }
    // clang-format on

private:
    alignas(std::max({alignof(Ts)...})) unsigned char m_data[std::max({sizeof(Ts)...})];
    void (*m_dtor)(void*);  // We could have used visitation instead but this approach does not require runtime search.
    std::size_t m_index;
};
static_assert(std::is_trivially_copyable<arena<int, double>>::value, "");
static_assert(std::is_trivially_copy_constructible<arena<int, double>>::value, "");
static_assert(std::is_trivially_move_constructible<arena<int, double>>::value, "");
static_assert(std::is_trivially_copy_assignable<arena<int, double>>::value, "");
static_assert(std::is_trivially_move_assignable<arena<int, double>>::value, "");
static_assert(std::is_trivially_destructible<arena<int, double>>::value, "");

}  // namespace var
}  // namespace detail

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_VARIANT_HPP_INCLUDED
