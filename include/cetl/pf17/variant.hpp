/// @file
/// Defines the C++17 std::variant type and several related entities.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_VARIANT_HPP_INCLUDED
#define CETL_PF17_VARIANT_HPP_INCLUDED

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
        // The lookup table can be made constexpr starting with C++17. Perhaps we should use some kind of conditional
        // macro here like CETL_CONSTEXPR_17 that expands into constexpr if C++17 is available.
        static const std::array<R (*)(F&&, Args&&...), sizeof...(Is)> lut = {
            [](F&& fn, Args&&... ar) -> R {
                return std::forward<F>(fn)(std::integral_constant<std::size_t, Is>{}, std::forward<Args>(ar)...);
            }...,
        };
        return lut.at(index)(std::forward<F>(fun), std::forward<Args>(ar)...);
    }
};
/// Invokes the specified fun as follows and forwards its result to the caller (argument forwarding not shown):
/// @code
/// fun(std::integral_constant<std::size_t, index>{}, extra_args...)
/// @endcode
/// In other words, this function moves the `index` value from the run time to the compile time domain (hence the name).
/// `N` specifies the maximum value for the index, which also informs the size of the lookup table.
/// If `index>=N`, behaves like `std::array<...,N>::at(index)`.
/// The time complexity is constant.
template <std::size_t N, typename F, typename... Args>
decltype(auto) chronomorphize(F&& fun, const std::size_t index, Args&&... extra_args)
{
    return chronomorphize_impl<std::make_index_sequence<N>>::lookup(std::forward<F>(fun),
                                                                    index,
                                                                    std::forward<Args>(extra_args)...);
}

/// This has to be an old-style enum because it is used as a template parameter.
enum smf_availability
{
    smf_deleted,
    smf_trivial,
    smf_nontrivial,
};

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

    static constexpr smf_availability avail_copy_ctor =
        all_satisfy<std::is_copy_constructible>
            ? (all_satisfy<std::is_trivially_copy_constructible> ? smf_trivial : smf_nontrivial)
            : smf_deleted;

    static constexpr smf_availability avail_move_ctor =
        all_satisfy<std::is_move_constructible>
            ? (all_satisfy<std::is_trivially_move_constructible> ? smf_trivial : smf_nontrivial)
            : smf_deleted;

    static constexpr smf_availability avail_copy_assign =
        all_satisfy<std::is_copy_assignable>
            ? (all_satisfy<std::is_trivially_copy_assignable> ? smf_trivial : smf_nontrivial)
            : smf_deleted;

    static constexpr smf_availability avail_move_assign =
        all_satisfy<std::is_move_assignable>
            ? (all_satisfy<std::is_trivially_move_assignable> ? smf_trivial : smf_nontrivial)
            : smf_deleted;

    static constexpr smf_availability avail_dtor =
        all_satisfy<std::is_destructible> ? (all_satisfy<std::is_trivially_destructible> ? smf_trivial : smf_nontrivial)
                                          : smf_deleted;

    static constexpr bool nothrow_move_constructible = all_satisfy<std::is_nothrow_move_constructible>;
    static constexpr bool nothrow_move_assignable    = all_satisfy<std::is_nothrow_move_assignable>;

    types()  = delete;
    ~types() = delete;
};

/// True iff all SMF availability values are trivial.
template <int... Vs>
constexpr bool smf_all_trivial = conjunction_v<std::integral_constant<bool, (smf_trivial == Vs)>...>;
static_assert(smf_all_trivial<smf_trivial, smf_trivial, smf_trivial>, "");
static_assert(!smf_all_trivial<smf_trivial, smf_nontrivial, smf_trivial>, "");
static_assert(!smf_all_trivial<smf_deleted, smf_trivial, smf_trivial>, "");

/// True iff any SMF availability value is deleted.
template <int... Vs>
constexpr bool smf_any_deleted = disjunction_v<std::integral_constant<bool, (smf_deleted == Vs)>...>;
static_assert(smf_any_deleted<smf_deleted, smf_trivial, smf_trivial>, "");
static_assert(smf_any_deleted<smf_trivial, smf_deleted, smf_trivial>, "");
static_assert(smf_any_deleted<smf_trivial, smf_trivial, smf_deleted>, "");
static_assert(!smf_any_deleted<smf_trivial, smf_trivial, smf_trivial>, "");

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
            if (types<Ts...>::avail_dtor != smf_trivial)  // A decent compiler will know what to do.
            {
                this->visit([](auto& val) {
                    using T = std::decay_t<decltype(val)>;
                    val.~T();
                });
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
template <typename Seq, int = Seq::avail_dtor>
struct base_destruction;
template <typename... Ts>
struct base_destruction<types<Ts...>, smf_trivial> : storage<Ts...>  // NOLINT(*-pro-type-member-init)
{};
template <typename... Ts>
struct base_destruction<types<Ts...>, smf_nontrivial> : storage<Ts...>  // NOLINT(*-pro-type-member-init)
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
template <typename... Ts>
struct base_destruction<types<Ts...>, smf_deleted>;  // Definition omitted, all variant types shall be destructible.

/// COPY CONSTRUCTION POLICY
template <typename Seq, int = Seq::avail_copy_ctor>
struct base_copy_construction;
template <typename... Ts>
struct base_copy_construction<types<Ts...>, smf_trivial> : base_destruction<types<Ts...>>
{};
template <typename... Ts>
struct base_copy_construction<types<Ts...>, smf_nontrivial> : base_destruction<types<Ts...>>
{
    base_copy_construction() = default;
    base_copy_construction(const base_copy_construction& other)
    {
        if (!other.is_valueless())
        {
            other.visit([this, &other](auto& val) {
                static_assert(sizeof(val) <= sizeof(this->m_data), "");
                new (this->m_data) decltype(val)(val);
            });
            this->m_index = other.m_index;
        }
    }
    base_copy_construction(base_copy_construction&&)                 = default;
    base_copy_construction& operator=(const base_copy_construction&) = default;
    base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                               = default;
};
template <typename... Ts>
struct base_copy_construction<types<Ts...>, smf_deleted> : base_destruction<types<Ts...>>
{
    base_copy_construction()                                         = default;
    base_copy_construction(const base_copy_construction& other)      = delete;
    base_copy_construction(base_copy_construction&&)                 = default;
    base_copy_construction& operator=(const base_copy_construction&) = default;
    base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                               = default;
};

/// MOVE CONSTRUCTION POLICY
template <typename Seq, int = Seq::avail_move_ctor>
struct base_move_construction;
template <typename... Ts>
struct base_move_construction<types<Ts...>, smf_trivial> : base_copy_construction<types<Ts...>>
{};
template <typename... Ts>
struct base_move_construction<types<Ts...>, smf_nontrivial> : base_copy_construction<types<Ts...>>
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
template <typename... Ts>
struct base_move_construction<types<Ts...>, smf_deleted> : base_copy_construction<types<Ts...>>
{
    base_move_construction()                                         = default;
    base_move_construction(const base_move_construction&)            = default;
    base_move_construction(base_move_construction&& other)           = delete;
    base_move_construction& operator=(const base_move_construction&) = default;
    base_move_construction& operator=(base_move_construction&&)      = default;
    ~base_move_construction() noexcept                               = default;
};

/// COPY ASSIGNMENT POLICY
template <typename Seq,
          int = smf_all_trivial<Seq::avail_copy_ctor, Seq::avail_copy_assign, Seq::avail_dtor>
                    ? smf_trivial
                    : (smf_any_deleted<Seq::avail_copy_ctor, Seq::avail_copy_assign, Seq::avail_dtor> ? smf_deleted
                                                                                                      : smf_nontrivial)>
struct base_copy_assignment;
template <typename... Ts>
struct base_copy_assignment<types<Ts...>, smf_trivial> : base_move_construction<types<Ts...>>
{};
template <typename... Ts>
struct base_copy_assignment<types<Ts...>, smf_nontrivial> : base_move_construction<types<Ts...>>
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
template <typename... Ts>
struct base_copy_assignment<types<Ts...>, smf_deleted> : base_move_construction<types<Ts...>>
{
    base_copy_assignment()                                             = default;
    base_copy_assignment(const base_copy_assignment&)                  = default;
    base_copy_assignment(base_copy_assignment&&)                       = default;
    base_copy_assignment& operator=(const base_copy_assignment& other) = delete;
    base_copy_assignment& operator=(base_copy_assignment&&)            = default;
    ~base_copy_assignment() noexcept                                   = default;
};

/// MOVE ASSIGNMENT POLICY
template <typename Seq,
          int = smf_all_trivial<Seq::avail_move_ctor, Seq::avail_move_assign, Seq::avail_dtor>
                    ? smf_trivial
                    : (smf_any_deleted<Seq::avail_move_ctor, Seq::avail_move_assign, Seq::avail_dtor> ? smf_deleted
                                                                                                      : smf_nontrivial)>
struct base_move_assignment;
template <typename... Ts>
struct base_move_assignment<types<Ts...>, smf_trivial> : base_copy_assignment<types<Ts...>>
{};
template <typename... Ts>
struct base_move_assignment<types<Ts...>, smf_nontrivial> : base_copy_assignment<types<Ts...>>
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
template <typename... Ts>
struct base_move_assignment<types<Ts...>, smf_deleted> : base_copy_assignment<types<Ts...>>
{
    using tys                                                     = types<Ts...>;
    base_move_assignment()                                        = default;
    base_move_assignment(const base_move_assignment&)             = default;
    base_move_assignment(base_move_assignment&&)                  = default;
    base_move_assignment& operator=(const base_move_assignment&)  = default;
    base_move_assignment& operator=(base_move_assignment&& other) = delete;
    ~base_move_assignment() noexcept                              = default;
};

}  // namespace var
}  // namespace detail

/// An implementation of \ref std::variant.
template <typename... Ts>
class variant : private detail::var::base_move_assignment<detail::var::types<Ts...>>
{
    using base = detail::var::base_move_assignment<detail::var::types<Ts...>>;
};

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_VARIANT_HPP_INCLUDED
