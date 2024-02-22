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
#include <cetl/pf17/type_traits.hpp>
#include <cetl/pf17/attribute.hpp>

#include <tuple>
#include <limits>
#include <cassert>
#include <algorithm>
#include <exception>  // We need this even if exceptions are disabled for std::terminate.
#include <type_traits>

namespace cetl
{
namespace pf17
{
/// Implementation of \ref std::variant_npos.
constexpr std::size_t variant_npos = std::numeric_limits<std::size_t>::max();

/// Implementation of \ref std::monostate.
struct monostate
{};
// clang-format off
inline constexpr bool operator==(const monostate, const monostate) noexcept { return true;  }
inline constexpr bool operator!=(const monostate, const monostate) noexcept { return false; }
inline constexpr bool operator< (const monostate, const monostate) noexcept { return false; }
inline constexpr bool operator> (const monostate, const monostate) noexcept { return false; }
inline constexpr bool operator<=(const monostate, const monostate) noexcept { return true;  }
inline constexpr bool operator>=(const monostate, const monostate) noexcept { return true;  }
// clang-format on

namespace detail
{
namespace var
{
template <std::size_t N, typename... Ts>
using nth_type = std::tuple_element_t<N, std::tuple<Ts...>>;

/// An internal helper used to keep the list of the variant types and query their properties.
template <typename... Ts>
struct types final
{
    template <template <typename> class F>
    static constexpr bool all_satisfy = conjunction_v<F<Ts>...>;

    static constexpr bool trivially_destructible       = all_satisfy<std::is_trivially_destructible>;
    static constexpr bool trivially_copy_constructible = all_satisfy<std::is_trivially_copy_constructible>;
    static constexpr bool trivially_move_constructible = all_satisfy<std::is_trivially_move_constructible>;
    static constexpr bool trivially_copy_assignable    = all_satisfy<std::is_trivially_copy_assignable>;
    static constexpr bool trivially_move_assignable    = all_satisfy<std::is_trivially_move_assignable>;
    static constexpr bool nothrow_move_constructible   = all_satisfy<std::is_nothrow_move_constructible>;

    types()  = delete;
    ~types() = delete;
};
static_assert(types<int, float>::trivially_destructible, "");
static_assert(!types<int, types<>>::trivially_destructible, "");

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

/// DESTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_destructible>
struct base_destruction;
/// Trivially destructible case.
template <typename... Ts>
struct base_destruction<types<Ts...>, true>  // NOLINT(*-pro-type-member-init)
{
    /// If the constructor throws, the instance will be left valueless.
    template <std::size_t N, typename T = nth_type<N, Ts...>, typename... Args>
    auto& construct(Args&&... args)
    {
        destroy();
        auto* const p = new (m_data) T(std::forward<Args>(args)...);  // If constructor throws, stay valueless.
        m_index       = N;                                            // If constructor succeeds, become non-valueless.
        return *p;
    }
    void destroy()
    {
        m_index = variant_npos;  // The destructor is trivial.
    }
    alignas(std::max({alignof(Ts)...})) unsigned char m_data[std::max({sizeof(Ts)...})];
    std::size_t m_index = variant_npos;
};
/// Non-trivially destructible case.
template <typename... Ts>
struct base_destruction<types<Ts...>, false>  // NOLINT(*-pro-type-member-init)
{
    base_destruction()                                   = default;
    base_destruction(const base_destruction&)            = default;
    base_destruction(base_destruction&&)                 = default;
    base_destruction& operator=(const base_destruction&) = default;
    base_destruction& operator=(base_destruction&&)      = default;
    ~base_destruction() noexcept
    {
        m_dtor(m_data);
    }
    /// If the constructor throws, the instance will be left valueless.
    template <std::size_t N, typename T = nth_type<N, Ts...>, typename... Args>
    auto& construct(Args&&... args)
    {
        destroy();
        auto* const p = new (m_data) T(std::forward<Args>(args)...);  // If constructor throws, stay valueless.
        m_index       = N;                                            // If constructor succeeds, become non-valueless.
        m_dtor        = [](void* const data) { reinterpret_cast<T*>(data)->~T(); };
        return *p;
    }
    void destroy()
    {
        m_dtor(m_data);
        m_dtor  = [](void*) {};
        m_index = variant_npos;
    }
    alignas(std::max({alignof(Ts)...})) unsigned char m_data[std::max({sizeof(Ts)...})];
    std::size_t m_index = variant_npos;

private:
    void (*m_dtor)(void*) = [](void*) {};
};

/// COPY CONSTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_copy_constructible>
struct base_copy_construction;
/// Trivially copy constructible case.
template <typename... Ts>
struct base_copy_construction<types<Ts...>, true> : base_destruction<types<Ts...>>
{
    using base = base_destruction<types<Ts...>>;
    using base::construct;
};
/// Non-trivially copy constructible case.
template <typename... Ts>
struct base_copy_construction<types<Ts...>, false> : base_destruction<types<Ts...>>
{
    using base               = base_destruction<types<Ts...>>;
    base_copy_construction() = default;
    base_copy_construction(const base_copy_construction& other)
        : m_copy_ctor(other.m_copy_ctor)
    {
        m_copy_ctor(this->m_data, other.m_data);  // May throw. No-op if the other is valueless.
        this->m_index = other.m_index;
    }
    base_copy_construction(base_copy_construction&&)                 = default;
    base_copy_construction& operator=(const base_copy_construction&) = default;
    base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                               = default;
    /// If the constructor throws, the instance will be left valueless.
    template <std::size_t N, typename T = nth_type<N, Ts...>, typename... Args>
    auto& construct(Args&&... args)
    {
        m_copy_ctor = [](void*, const void*) {};  // Ground in case we become valueless.
        auto& res   = base::template construct<N>(std::forward<Args>(args)...);
        m_copy_ctor = [](void* const dst, const void* const src) { new (dst) T(*reinterpret_cast<const T*>(src)); };
        return res;
    }

private:
    void (*m_copy_ctor)(void*, const void*) = [](void*, const void*) {};
};

/// MOVE CONSTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_move_constructible>
struct base_move_construction;
/// Trivially move constructible case.
template <typename... Ts>
struct base_move_construction<types<Ts...>, true> : base_copy_construction<types<Ts...>>
{
    using base = base_copy_construction<types<Ts...>>;
    using base::construct;
};
/// Non-trivially move constructible case.
template <typename... Ts>
struct base_move_construction<types<Ts...>, false> : base_copy_construction<types<Ts...>>
{
    using base                                            = base_copy_construction<types<Ts...>>;
    base_move_construction()                              = default;
    base_move_construction(const base_move_construction&) = default;
    base_move_construction(base_move_construction&& other) noexcept(types<Ts...>::nothrow_move_constructible)
        : m_move_ctor(other.m_move_ctor)
    {
        m_move_ctor(this->m_data, other.m_data);  // May throw. No-op if the other is valueless.
        this->m_index = other.m_index;
    }
    base_move_construction& operator=(const base_move_construction&) = default;
    base_move_construction& operator=(base_move_construction&&)      = default;
    ~base_move_construction() noexcept                               = default;
    /// If the constructor throws, the instance will be left valueless.
    template <std::size_t N, typename T = nth_type<N, Ts...>, typename... Args>
    auto& construct(Args&&... args)
    {
        m_move_ctor = [](void*, void*) {};  // Ground in case we become valueless.
        auto& res   = base::template construct<N>(std::forward<Args>(args)...);
        m_move_ctor = [](void* const dst, void* const src) { new (dst) T(std::move(*reinterpret_cast<T*>(src))); };
        return res;
    }

private:
    void (*m_move_ctor)(void*, void*) = [](void*, void*) {};
};

}  // namespace var
}  // namespace detail

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_VARIANT_HPP_INCLUDED
