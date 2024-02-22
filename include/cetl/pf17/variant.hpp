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
#include <array>
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

#if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)
/// Implementation of \ref std::bad_variant_access.
/// This is only available if exceptions are enabled (__cpp_exceptions is defined).
class bad_variant_access : public std::exception
{
public:
    const char* what() const noexcept override
    {
        return "bad_variant_access";
    }
};
#endif

namespace detail
{
namespace var
{
template <std::size_t N, typename... Ts>
using nth_type = std::tuple_element_t<N, std::tuple<Ts...>>;

template <typename>
struct chronomorphize_impl;
template <std::size_t... Is>
struct chronomorphize_impl<std::index_sequence<Is...>>
{
    template <typename F, typename... Args>
    static decltype(auto) lookup(F&& fun, const std::size_t index, Args&&... ar)
    {
        using R = std::common_type_t<decltype(std::forward<F>(fun)(std::integral_constant<std::size_t, Is>{},
                                                                   std::forward<Args>(ar)...))...>;
        static const std::array<R (*)(F&&, Args&&... ar), sizeof...(Is)> lut = {
            [](F&& fn, Args&&... ar) -> R {
                return std::forward<F>(fn)(std::integral_constant<std::size_t, Is>{}, std::forward<Args>(ar)...);
            }...,
        };
        return lut.at(index)(std::forward<F>(fun), std::forward<Args>(ar)...);
    }
};
/// Invokes the specified fun as follows and forwards its result to the caller (argument forwarding not shown):
/// @code
/// fun(std::integral_constant<std::size_t, index>{}, ar...)
/// @endcode
/// In other words, this function moves the `index` value from the run time to the compile time domain (hence the name).
/// `N` specifies the maximum value for the index, which also informs the size of the scanned lookup table.
/// If `index>=N`, behaves like `std::array<...,N>::at(index)`.
/// The time complexity is constant.
template <std::size_t N, typename F, typename... Args>
decltype(auto) chronomorphize(F&& fun, const std::size_t index, Args&&... ar)
{
    return chronomorphize_impl<std::make_index_sequence<N>>::lookup(std::forward<F>(fun),
                                                                    index,
                                                                    std::forward<Args>(ar)...);
}

/// An internal helper used to keep the list of the variant types and query their properties.
template <typename... Ts>
struct types final
{
    template <template <typename> class F>
    static constexpr bool all_satisfy = conjunction_v<F<Ts>...>;

    static_assert(sizeof...(Ts) > 0, "");
    static_assert(conjunction_v<negation<std::is_array<Ts>>...>, "");
    static_assert(conjunction_v<negation<std::is_reference<Ts>>...>, "");
    static_assert(conjunction_v<negation<std::is_void<Ts>>...>, "");

    static constexpr bool copy_constructible = all_satisfy<std::is_copy_constructible>;
    static constexpr bool move_constructible = all_satisfy<std::is_move_constructible>;
    static constexpr bool copy_assignable    = all_satisfy<std::is_copy_assignable>;
    static constexpr bool move_assignable    = all_satisfy<std::is_move_assignable>;

    static constexpr bool trivially_destructible = all_satisfy<std::is_trivially_destructible>;

    static constexpr bool trivially_copy_constructible = all_satisfy<std::is_trivially_copy_constructible>;
    static constexpr bool trivially_move_constructible = all_satisfy<std::is_trivially_move_constructible>;
    static constexpr bool trivially_copy_assignable    = all_satisfy<std::is_trivially_copy_assignable>;
    static constexpr bool trivially_move_assignable    = all_satisfy<std::is_trivially_move_assignable>;

    static constexpr bool nothrow_move_constructible = all_satisfy<std::is_nothrow_move_constructible>;
    static constexpr bool nothrow_move_assignable    = all_satisfy<std::is_nothrow_move_assignable>;

    types()  = delete;
    ~types() = delete;
};
static_assert(types<int, float>::trivially_destructible, "");
static_assert(!types<int, types<char>>::trivially_destructible, "");

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

inline void bad_access_unless(const bool condition)
{
    if (!condition)
    {
#if __cpp_exceptions
        throw bad_variant_access{};
#else
        std::terminate();
#endif
    }
}

/// STORAGE
template <typename... Ts>
struct storage  // NOLINT(*-pro-type-member-init)
{
    /// If the constructor throws, the instance will be left valueless.
    template <std::size_t N, typename T = nth_type<N, Ts...>, typename... Args>
    auto& construct(Args&&... args)
    {
        destroy();
        auto* const p = new (this->m_data) T(std::forward<Args>(args)...);  // If constructor throws, stay valueless.
        this->m_index = N;  // If constructor succeeds, become non-valueless.
        return *p;
    }
    void destroy()
    {
        if (this->m_index != variant_npos)
        {
            if (!types<Ts...>::trivially_destructible)  // A decent compiler will know what to do.
            {
                this->visit([](auto& val) { val.~decltype(val)(); });
            }
            this->m_index = variant_npos;
        }
    }
    /// The ordinary visit().
    template <typename F>
    decltype(auto) visit(F&& fun)
    {
        return visit_index([this, &fun](const auto index) { return fun(this->get<index.value>()); });
    }
    /// This is like the ordinary visit but the argument type is integral_constant<size_t, index> instead of T.
    template <typename F>
    decltype(auto) visit_index(F&& fun)
    {
        bad_access_unless(m_index != variant_npos);
        return chronomorphize<sizeof...(Ts)>(
            [this, &fun](const auto index) -> decltype(auto) {
                assert(index.value == m_index);
                return fun(index);
            },
            m_index);
    }
    CETL_NODISCARD bool is_valueless() const noexcept
    {
        return m_index == variant_npos;
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD auto& get() & noexcept
    {
        bad_access_unless(N == m_index);
        return *reinterpret_cast<T*>(m_data);
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD const auto& get() const& noexcept
    {
        bad_access_unless(N == m_index);
        return *reinterpret_cast<const T*>(m_data);
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD auto&& get() && noexcept
    {
        bad_access_unless(N == m_index);
        return std::move(*reinterpret_cast<T*>(m_data));
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD const auto&& get() const&& noexcept
    {
        bad_access_unless(N == m_index);
        return std::move(*reinterpret_cast<const T*>(m_data));
    }
    // The address of the stored alternative equals the address of the first byte of the storage.
    // This means taking the address of the variant is equivalent to taking the address of the active alternative.
    alignas(std::max({alignof(Ts)...})) unsigned char m_data[std::max({sizeof(Ts)...})];
    std::size_t m_index = variant_npos;
};

/// DESTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_destructible>
struct base_destruction;
/// Trivially destructible case.
template <typename... Ts>
struct base_destruction<types<Ts...>, true> : storage<Ts...>  // NOLINT(*-pro-type-member-init)
{};
/// Non-trivially destructible case.
template <typename... Ts>
struct base_destruction<types<Ts...>, false> : storage<Ts...>  // NOLINT(*-pro-type-member-init)
{
    base_destruction()                                   = default;
    base_destruction(const base_destruction&)            = default;
    base_destruction(base_destruction&&)                 = default;
    base_destruction& operator=(const base_destruction&) = default;
    base_destruction& operator=(base_destruction&&)      = default;
    ~base_destruction() noexcept
    {
        this->destroy();
    }
};

/// COPY CONSTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_copy_constructible>
struct base_copy_construction;
/// Trivially copy constructible case.
template <typename... Ts>
struct base_copy_construction<types<Ts...>, true> : base_destruction<types<Ts...>>
{};
/// Non-trivially copy constructible case.
template <typename... Ts>
struct base_copy_construction<types<Ts...>, false> : base_destruction<types<Ts...>>
{
    base_copy_construction() = default;
    base_copy_construction(const base_copy_construction& other)
    {
        if (!other.is_valueless())
        {
            other.visit([this, &other](auto& val) { new (this->m_data) decltype(val)(val); });
            this->m_index = other.m_index;
        }
    }
    base_copy_construction(base_copy_construction&&)                 = default;
    base_copy_construction& operator=(const base_copy_construction&) = default;
    base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                               = default;
};

/// MOVE CONSTRUCTION POLICY
template <typename Seq, bool = Seq::trivially_move_constructible>
struct base_move_construction;
/// Trivially move constructible case.
template <typename... Ts>
struct base_move_construction<types<Ts...>, true> : base_copy_construction<types<Ts...>>
{};
/// Non-trivially move constructible case.
template <typename... Ts>
struct base_move_construction<types<Ts...>, false> : base_copy_construction<types<Ts...>>
{
    base_move_construction()                              = default;
    base_move_construction(const base_move_construction&) = default;
    base_move_construction(base_move_construction&& other) noexcept(types<Ts...>::nothrow_move_constructible)
    {
        if (!other.is_valueless())
        {
            other.visit([this, &other](auto& val) { new (this->m_data) decltype(val)(std::move(val)); });
            this->m_index = other.m_index;
        }
    }
    base_move_construction& operator=(const base_move_construction&) = default;
    base_move_construction& operator=(base_move_construction&&)      = default;
    ~base_move_construction() noexcept                               = default;
};

/// COPY ASSIGNMENT POLICY
template <typename Seq,
          bool = Seq::trivially_copy_assignable && Seq::trivially_copy_constructible && Seq::trivially_destructible>
struct base_copy_assignment;
/// Trivially copy assignable case.
template <typename... Ts>
struct base_copy_assignment<types<Ts...>, true> : base_move_construction<types<Ts...>>
{};
/// Non-trivially copy assignable case.
template <typename... Ts>
struct base_copy_assignment<types<Ts...>, false> : base_move_construction<types<Ts...>>
{
    base_copy_assignment()                            = default;
    base_copy_assignment(const base_copy_assignment&) = default;
    base_copy_assignment(base_copy_assignment&&)      = default;
    base_copy_assignment& operator=(const base_copy_assignment& other)
    {
        if ((!this->is_valueless()) && (this->m_index == other.m_index))  // Invoke copy assignment.
        {
            assert(!other.is_valueless());
            // If an exception is thrown, *this does not become valueless:
            // the value depends on the exception safety guarantee of the alternative's copy assignment.
            other.visit_index([this, &other](const auto index) {
                assert((index.value == other.m_index) && (index.value == this->m_index));
                this->template get<index.value>() = other.template get<index.value>();
            });
        }
        else if (!other.is_valueless())  // Invoke copy constructor.
        {
            // Here, this may or may not be valueless; either way we don't care about its state as it
            // needs to be replaced. If an exception is thrown, *this becomes valueless inside construct().
            other.visit_index([this, &other](const auto index) {
                assert(index.value == other.m_index);
                this->construct<index.value>(other.template get<index.value>());
            });
        }
        else
        {
            assert(other.is_valueless());
            this->destroy();  // This is a no-op if this is already valueless.
        }
        return *this;
    }
    base_copy_assignment& operator=(base_copy_assignment&&) = default;
    ~base_copy_assignment() noexcept                        = default;
};

/// MOVE ASSIGNMENT POLICY
template <typename Seq,
          bool = Seq::trivially_move_assignable && Seq::trivially_move_constructible && Seq::trivially_destructible>
struct base_move_assignment;
/// Trivially move assignable case.
template <typename... Ts>
struct base_move_assignment<types<Ts...>, true> : base_copy_assignment<types<Ts...>>
{};
/// Non-trivially move assignable case.
template <typename... Ts>
struct base_move_assignment<types<Ts...>, false> : base_copy_assignment<types<Ts...>>
{
    using tys                                                    = types<Ts...>;
    base_move_assignment()                                       = default;
    base_move_assignment(const base_move_assignment&)            = default;
    base_move_assignment(base_move_assignment&&)                 = default;
    base_move_assignment& operator=(const base_move_assignment&) = default;
    base_move_assignment& operator=(base_move_assignment&& other) noexcept(tys::nothrow_move_constructible &&
                                                                           tys::nothrow_move_assignable)
    {
        if ((!this->is_valueless()) && (this->m_index == other.m_index))  // Invoke move assignment.
        {
            assert(!other.is_valueless());
            // If an exception is thrown, *this does not become valueless:
            // the value depends on the exception safety guarantee of the alternative's move assignment.
            other.visit_index([this, &other](const auto index) {
                assert((index.value == other.m_index) && (index.value == this->m_index));
                this->template get<index.value>() = std::move(other.template get<index.value>());
            });
        }
        else if (!other.is_valueless())  // Invoke move constructor.
        {
            // Here, this may or may not be valueless; either way we don't care about its state as it
            // needs to be replaced. If an exception is thrown, *this becomes valueless inside construct().
            other.visit_index([this, &other](const auto index) {
                assert(index.value == other.m_index);
                this->construct<index.value>(std::move(other.template get<index.value>()));
            });
        }
        else
        {
            assert(other.is_valueless());
            this->destroy();  // This is a no-op if this is already valueless.
        }
        return *this;
    }
    ~base_move_assignment() noexcept = default;
};

/// A helper for special member function enablement.
template <typename L>
struct base_smf_enabler
    : public cetl::detail::enable_copy_move_construction<L::copy_constructible, L::move_constructible>,
      public cetl::detail::enable_copy_move_assignment<L::copy_constructible && L::copy_assignable,
                                                       L::move_constructible && L::move_assignable>
{};

}  // namespace var
}  // namespace detail

/// An implementation of \ref std::variant.
template <typename... Ts>
class variant : private detail::var::base_move_assignment<detail::var::types<Ts...>>,
                private detail::var::base_smf_enabler<detail::var::types<Ts...>>
{
    using base = detail::var::base_move_assignment<detail::var::types<Ts...>>;
};

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_VARIANT_HPP_INCLUDED
