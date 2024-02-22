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
    static constexpr bool trivially_copyable           = all_satisfy<std::is_trivially_copyable>;
    static constexpr bool trivially_copy_assignable    = all_satisfy<std::is_trivially_copy_assignable>;
    static constexpr bool trivially_move_assignable    = all_satisfy<std::is_trivially_move_assignable>;
    static constexpr bool trivially_copy_constructible = all_satisfy<std::is_trivially_copy_constructible>;
    static constexpr bool trivially_move_constructible = all_satisfy<std::is_trivially_move_constructible>;

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

/// This type manages the storage arena for the variant and keeps track of the currently active type.
/// It expressly ignores copy/move/destruction/assignment; this must be managed externally.
/// We support the valueless semantics even if __cpp_exceptions is not defined for the sake of ABI compatibility
/// because there may be another translation unit that uses exceptions.
template <typename... Ts>
class arena final
{
public:
    template <std::size_t N, typename T = nth_type<N, Ts...>, typename... Args>
    explicit arena(const in_place_index_t<N>, Args&&... args)  // NOLINT(*-pro-type-member-init)
        : m_index(N)
        , m_dtor([](void* const data) { reinterpret_cast<T*>(data)->~T(); })
    {
        new (m_data) T(std::forward<Args>(args)...);
    }
    template <typename T, typename... Args>
    explicit arena(const in_place_type_t<T>, Args&&... args)
        : arena(in_place_index<index_of<T, Ts...>>, std::forward<Args>(args)...)
    {
    }
    arena()  // Type #0 must be default-constructible, otherwise this type is not default-constructible either.
        : arena(in_place_index<0>)
    {
    }

    /// Constructs a new value in the arena. The old value will be destroyed beforehand by calling destroy().
    /// If the constructor throws, the arena will be left valueless.
    template <std::size_t N, typename T = nth_type<N, Ts...>, typename... Args>
    auto& construct(Args&&... args)
    {
        destroy();                                                    // First, make it valueless...
        auto* const p = new (m_data) T(std::forward<Args>(args)...);  // If constructor throws, stay valueless.
        m_index       = N;                                            // If constructor succeeds, become non-valueless.
        m_dtor        = [](void* const data) { reinterpret_cast<T*>(data)->~T(); };
        return *p;
    }
    template <typename T, typename... Args>
    auto& construct(Args&&... args)
    {
        return construct<index_of<T, Ts...>>(std::forward<Args>(args)...);
    }

    /// Destroys the value and makes this arena valueless. Does nothing if the arena is already valueless.
    /// If all Ts are trivially destructible, there is no need to invoke this function upon destruction of the arena.
    void destroy() noexcept
    {
        if (!is_valueless())
        {
            m_index = variant_npos;
            assert(m_dtor != nullptr);
            m_dtor(m_data);
            m_dtor = nullptr;
        }
        assert(is_valueless());
    }

    /// Returns variant_npos if valueless.
    CETL_NODISCARD std::size_t index() const noexcept
    {
        return m_index;
    }

    CETL_NODISCARD bool is_valueless() const noexcept
    {
        return m_index == variant_npos;
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
    // We could have used visitation instead but the closure approach does not require runtime search.
    void        (*m_dtor)(void*);
    std::size_t m_index;
};
static_assert(std::is_trivially_copyable<arena<int, double>>::value, "");
static_assert(std::is_trivially_copy_constructible<arena<int, double>>::value, "");
static_assert(std::is_trivially_move_constructible<arena<int, double>>::value, "");
static_assert(std::is_trivially_copy_assignable<arena<int, double>>::value, "");
static_assert(std::is_trivially_move_assignable<arena<int, double>>::value, "");
static_assert(std::is_trivially_destructible<arena<int, double>>::value, "");

/// DESTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_destructible>
struct base_destruction;
/// Trivially destructible case.
template <typename... Ts>
struct base_destruction<types<Ts...>, true>
{
    arena<Ts...> m_arena;
};
/// Non-trivially destructible case.
template <typename... Ts>
struct base_destruction<types<Ts...>, false>
{
    base_destruction()                                   = default;
    base_destruction(const base_destruction&)            = default;
    base_destruction(base_destruction&&)                 = default;
    base_destruction& operator=(const base_destruction&) = default;
    base_destruction& operator=(base_destruction&&)      = default;
    ~base_destruction() noexcept
    {
        m_arena.destroy();
    }
    arena<Ts...> m_arena;
};

/// COPY CONSTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_copy_constructible>
struct base_copy_construction;
/// Trivially copy constructible case.
template <typename... Ts>
struct base_copy_construction<types<Ts...>, true> : base_destruction<types<Ts...>>
{
    // Copies constructed by merely copying the bytes of the source.
};
/// Non-trivially copy constructible case.
template <typename... Ts>
struct base_copy_construction<types<Ts...>, false> : base_destruction<types<Ts...>>
{
    base_copy_construction()                                         = default;
    base_copy_construction(const base_copy_construction&)            = default;
    base_copy_construction(base_copy_construction&&)                 = default;
    base_copy_construction& operator=(const base_copy_construction&) = default;
    base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                               = default;
};

}  // namespace var
}  // namespace detail

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_VARIANT_HPP_INCLUDED
