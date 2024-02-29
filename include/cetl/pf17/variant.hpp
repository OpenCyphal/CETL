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
/// Implementation of \c std::variant_npos.
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

// --------------------------------------------------------------------------------------------------------------------

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
        static const std::array<R (*)(F&&, Args && ...), sizeof...(Is)> lut = {
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

// --------------------------------------------------------------------------------------------------------------------

template <typename, typename...>
struct count_occurrences_impl;
template <typename T, typename A>
struct count_occurrences_impl<T, A> : std::integral_constant<std::size_t, std::is_same<T, A>::value ? 1 : 0>
{};
template <typename T, typename A, typename B, typename... C>
struct count_occurrences_impl<T, A, B, C...>
    : std::integral_constant<std::size_t,
                             count_occurrences_impl<T, A>::value + count_occurrences_impl<T, B, C...>::value>
{};
/// Counts the occurrences of T in Ts.
template <typename T, typename... Ts>
constexpr std::size_t count_occurrences = count_occurrences_impl<T, Ts...>::value;
static_assert(0 == count_occurrences<int, char>, "");
static_assert(1 == count_occurrences<int, int, char>, "");
static_assert(2 == count_occurrences<int, char, int, double, int>, "");

// --------------------------------------------------------------------------------------------------------------------

template <typename T, typename... Ts>
struct first_index_of_impl;
template <typename T, typename... Ts>
struct first_index_of_impl<T, T, Ts...> : std::integral_constant<std::size_t, 0>
{};
template <typename T, typename U, typename... Ts>
struct first_index_of_impl<T, U, Ts...> : std::integral_constant<std::size_t, 1 + first_index_of_impl<T, Ts...>::value>
{};

/// Find the index of the first occurrence of T in Ts. Fails compilation if T is not found in Ts.
template <typename T, typename... Ts>
constexpr std::size_t first_index_of = first_index_of_impl<T, Ts...>::value;

/// Find the index of T in Ts. Fails compilation if T is not found in Ts or if T is found more than once.
template <typename T, typename... Ts>
constexpr std::enable_if_t<count_occurrences<T, Ts...> == 1, std::size_t> unique_index_of = first_index_of<T, Ts...>;

static_assert(0 == unique_index_of<int, int>, "");
static_assert(0 == unique_index_of<int, int, double, char>, "");
static_assert(1 == unique_index_of<double, int, double, char>, "");
static_assert(2 == unique_index_of<char, int, double, char>, "");
static_assert(2 == first_index_of<char, int, double, char, char>, "");

// --------------------------------------------------------------------------------------------------------------------

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
    static constexpr bool nothrow_swappable          = all_satisfy<is_nothrow_swappable>;

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

// --------------------------------------------------------------------------------------------------------------------

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
/// The storage class does not construct, destroy, copy, or move anything automatically;
/// by itself it is trivially copyable and standard-layout.
/// All SMF behaviors have to be implemented by the descendants according to the properties of the variant types.
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
                this->chronomorphize([this](const auto index) {
                    using T = nth_type<index.value, Ts...>;
                    this->template as<index.value>().~T();
                });
            }
            this->m_index = variant_npos;
        }
    }
    /// Invoke \c fun(integral_constant<size_t, index>); throws bad_variant_access if valueless.
    template <typename F>
    decltype(auto) chronomorphize(F&& fun) const
    {
        bad_access_unless(m_index != variant_npos);
        return detail::var::chronomorphize<sizeof...(Ts)>(
            // https://twitter.com/PavelKirienko/status/1761525562370040002
            [check_index = m_index, &fun](const auto index) -> decltype(auto) {
                assert(index.value == check_index);
                (void) check_index;  // Silence the warning about unused variable.
                return std::forward<F>(fun)(index);
            },
            m_index);
    }
    CETL_NODISCARD bool is_valueless() const noexcept
    {
        return m_index == variant_npos;
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD T& as() & noexcept
    {
        static_assert(N < sizeof...(Ts), "Variant alternative index is out of range");
        assert(N == m_index);  // Internal contract check; the caller is responsible for the correctness of N.
        return *reinterpret_cast<T*>(m_data);
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD const T& as() const& noexcept
    {
        static_assert(N < sizeof...(Ts), "Variant alternative index is out of range");
        assert(N == m_index);  // Internal contract check; the caller is responsible for the correctness of N.
        return *reinterpret_cast<const T*>(m_data);
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD T&& as() && noexcept
    {
        static_assert(N < sizeof...(Ts), "Variant alternative index is out of range");
        assert(N == m_index);  // Internal contract check; the caller is responsible for the correctness of N.
        return std::move(*reinterpret_cast<T*>(m_data));
    }
    template <std::size_t N, typename T = nth_type<N, Ts...>>
    CETL_NODISCARD const T&& as() const&& noexcept
    {
        static_assert(N < sizeof...(Ts), "Variant alternative index is out of range");
        assert(N == m_index);  // Internal contract check; the caller is responsible for the correctness of N.
        return std::move(*reinterpret_cast<const T*>(m_data));
    }
    // The address of the stored alternative equals the address of the first byte of the storage.
    // This means taking the address of the variant is equivalent to taking the address of the active alternative.
    alignas(std::max({alignof(Ts)...})) unsigned char m_data[std::max({sizeof(Ts)...})];
    std::size_t m_index = variant_npos;
};
static_assert(std::is_trivially_destructible<storage<int, double, char>>::value, "");
static_assert(std::is_trivially_copyable<storage<int, double, char>>::value, "");
static_assert(std::is_standard_layout<storage<int, double, char>>::value, "");

// --------------------------------------------------------------------------------------------------------------------

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

// --------------------------------------------------------------------------------------------------------------------

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
            other.chronomorphize([this, &other](const auto index) {
                using T = nth_type<index.value, Ts...>;
                new (this->m_data) T(other.template as<index.value>());
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
    base_copy_construction(const base_copy_construction&)            = delete;
    base_copy_construction(base_copy_construction&&)                 = default;
    base_copy_construction& operator=(const base_copy_construction&) = default;
    base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                               = default;
};

// --------------------------------------------------------------------------------------------------------------------

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
            other.chronomorphize([this, &other](const auto index) {
                using T = nth_type<index.value, Ts...>;
                new (this->m_data) T(std::move(other.template as<index.value>()));
            });
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
    base_move_construction(base_move_construction&&)                 = delete;
    base_move_construction& operator=(const base_move_construction&) = default;
    base_move_construction& operator=(base_move_construction&&)      = default;
    ~base_move_construction() noexcept                               = default;
};

// --------------------------------------------------------------------------------------------------------------------

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
            other.chronomorphize([this, &other](const auto index) {
                assert((index.value == other.m_index) && (index.value == this->m_index));
                this->template as<index.value>() = other.template as<index.value>();
            });
        }
        else if (!other.is_valueless())  // Invoke copy constructor.
        {
            // Here, this may or may not be valueless; either way we don't care about its state as it
            // needs to be replaced. If an exception is thrown, *this becomes valueless inside construct().
            other.chronomorphize([this, &other](const auto index) {
                assert(index.value == other.m_index);
                this->construct<index.value>(other.template as<index.value>());
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
    base_copy_assignment()                                       = default;
    base_copy_assignment(const base_copy_assignment&)            = default;
    base_copy_assignment(base_copy_assignment&&)                 = default;
    base_copy_assignment& operator=(const base_copy_assignment&) = delete;
    base_copy_assignment& operator=(base_copy_assignment&&)      = default;
    ~base_copy_assignment() noexcept                             = default;
};

// --------------------------------------------------------------------------------------------------------------------

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
    base_move_assignment& operator=(base_move_assignment&& other) noexcept(
        tys::nothrow_move_constructible&& tys::nothrow_move_assignable)
    {
        if ((!this->is_valueless()) && (this->m_index == other.m_index))  // Invoke move assignment.
        {
            assert(!other.is_valueless());
            // If an exception is thrown, *this does not become valueless:
            // the value depends on the exception safety guarantee of the alternative's move assignment.
            other.chronomorphize([this, &other](const auto index) {
                assert((index.value == other.m_index) && (index.value == this->m_index));
                this->template as<index.value>() = std::move(other.template as<index.value>());
            });
        }
        else if (!other.is_valueless())  // Invoke move constructor.
        {
            // Here, this may or may not be valueless; either way we don't care about its state as it
            // needs to be replaced. If an exception is thrown, *this becomes valueless inside construct().
            other.chronomorphize([this, &other](const auto index) {
                assert(index.value == other.m_index);
                this->construct<index.value>(std::move(other.template as<index.value>()));
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
    using tys                                                    = types<Ts...>;
    base_move_assignment()                                       = default;
    base_move_assignment(const base_move_assignment&)            = default;
    base_move_assignment(base_move_assignment&&)                 = default;
    base_move_assignment& operator=(const base_move_assignment&) = default;
    base_move_assignment& operator=(base_move_assignment&&)      = delete;
    ~base_move_assignment() noexcept                             = default;
};

// --------------------------------------------------------------------------------------------------------------------

template <typename F>
decltype(auto) visit(F&& fun)  // For some reason the standard requires this silly overload.
{
    return std::forward<F>(fun)();
}
template <typename F, typename V>
decltype(auto) visit(F&& fun, V&& var)
{
    // https://twitter.com/PavelKirienko/status/1761525562370040002
    return std::forward<V>(var).chronomorphize([&fun, &var](const auto index) {
        return std::forward<F>(fun)(std::forward<V>(var).template as<index.value>());
    });
}
template <typename F, typename V0, typename... Vs, std::enable_if_t<(sizeof...(Vs) > 0), int> = 0>
decltype(auto) visit(F&& fun, V0&& var0, Vs&&... vars)
{
    // Instead of generating a multidimensional vtable as it is commonly done, we use recursive visiting;
    // this allows us to achieve a similar result with much less code at the expense of one extra call indirection
    // per variant. The recursive structure below builds a Cartesian product of the variant combinations
    // one level at a time starting with the leftmost variant. The complexity is one table jump per variant level.
    return visit(
        [&](auto&& hh) {  // https://twitter.com/PavelKirienko/status/1761525562370040002
            return visit(
                [&](auto&&... tt) {
                    return std::forward<F>(fun)(std::forward<decltype(hh)>(hh), std::forward<decltype(tt)>(tt)...);
                },
                std::forward<Vs>(vars)...);
        },
        std::forward<V0>(var0));
}

}  // namespace var
}  // namespace detail

// --------------------------------------------------------------------------------------------------------------------

/// Implementation of \ref std::variant_alternative.
/// This implementation also accepts other typelist-parameterized classes, such as \ref std::variant.
template <size_t, typename>
struct variant_alternative;
template <size_t N, template <typename...> class V, typename... Ts>
struct variant_alternative<N, V<Ts...>>
{
    static_assert(N < sizeof...(Ts), "Variant type index out of range");
    using type = detail::var::nth_type<N, Ts...>;
};
template <size_t N, template <typename...> class V, typename... Ts>
struct variant_alternative<N, const V<Ts...>>
{
    using type = const typename variant_alternative<N, V<Ts...>>::type;
};

/// Implementation of \ref std::variant_alternative_t.
/// This implementation also accepts other typelist-parameterized classes, such as \ref std::variant.
template <size_t N, typename V>
using variant_alternative_t = typename variant_alternative<N, V>::type;

// --------------------------------------------------------------------------------------------------------------------

/// Implementation of \ref std::variant_size.
/// This implementation also accepts other typelist-parameterized classes, such as \ref std::variant.
template <typename>
struct variant_size;
template <typename V>
struct variant_size<const V> : variant_size<V>
{};
template <template <typename...> class V, typename... Ts>
struct variant_size<V<Ts...>> : std::integral_constant<std::size_t, sizeof...(Ts)>
{};

/// Implementation of \ref std::variant_size_v.
/// This implementation also accepts other typelist-parameterized classes, such as \ref std::variant.
template <typename V>
constexpr size_t variant_size_v = variant_size<V>::value;

// --------------------------------------------------------------------------------------------------------------------

/// An implementation of \ref std::variant.
/// In this implementation, the address of the variant object is equivalent to the address of the active alternative;
/// this can sometimes be useful for low-level debugging.
template <typename... Ts>
class variant : private detail::var::base_move_assignment<detail::var::types<Ts...>>
{
    using tys  = detail::var::types<Ts...>;
    using base = detail::var::base_move_assignment<tys>;

    template <std::size_t N>
    using nth_type = detail::var::nth_type<N, Ts...>;
    template <typename T>
    static constexpr std::size_t index_of = detail::var::unique_index_of<T, Ts...>;
    template <typename T>
    static constexpr bool is_unique = (detail::var::count_occurrences<T, Ts...> == 1);

    // get_if<>() friends
    template <std::size_t Ix, typename... Us>
    friend constexpr std::add_pointer_t<variant_alternative_t<Ix, variant<Us...>>> get_if(
        variant<Us...>* const var) noexcept;
    template <std::size_t Ix, typename... Us>
    friend constexpr std::add_pointer_t<const variant_alternative_t<Ix, variant<Us...>>> get_if(
        const variant<Us...>* const var) noexcept;

    // get<>() friends
    template <std::size_t Ix, typename... Us>
    friend constexpr variant_alternative_t<Ix, variant<Us...>>& get(variant<Us...>& var);
    template <std::size_t Ix, typename... Us>
    friend constexpr variant_alternative_t<Ix, variant<Us...>>&& get(variant<Us...>&& var);
    template <std::size_t Ix, typename... Us>
    friend constexpr const variant_alternative_t<Ix, variant<Us...>>& get(const variant<Us...>& var);
    template <std::size_t Ix, typename... Us>
    friend constexpr const variant_alternative_t<Ix, variant<Us...>>&& get(const variant<Us...>&& var);

    // visit() friends
    template <typename F, typename V>
    friend decltype(auto) detail::var::visit(F&& fun, V&& var);

public:
    /// Constructor 1
    template <std::enable_if_t<std::is_default_constructible<nth_type<0>>::value, int> = 0>
    variant() noexcept(std::is_nothrow_default_constructible<nth_type<0>>::value)
        : variant(in_place_index<0>)
    {
    }

    /// Constructor 2
    variant(const variant& other) = default;

    /// Constructor 3
    variant(variant&& other) noexcept(std::is_nothrow_move_constructible<nth_type<0>>::value) = default;

    /// Constructor 4
    // TODO FIXME IMPLEMENT https://en.cppreference.com/w/cpp/utility/variant/variant

    /// Constructor 5
    template <typename T,
              typename... Args,
              std::enable_if_t<is_unique<T> && std::is_constructible<T, Args...>::value, int> = 0>
    explicit variant(const in_place_type_t<T>, Args&&... args)
    {
        this->template construct<index_of<T>>(std::forward<Args>(args)...);
    }

    /// Constructor 6
    template <
        typename T,
        typename U,
        typename... Args,
        std::enable_if_t<is_unique<T> && std::is_constructible<T, std::initializer_list<U>&, Args...>::value, int> = 0>
    explicit variant(const in_place_type_t<T>, const std::initializer_list<U> il, Args&&... args)
    {
        this->template construct<index_of<T>>(il, std::forward<Args>(args)...);
    }

    /// Constructor 7
    template <std::size_t Ix,
              typename... Args,
              std::enable_if_t<(Ix < sizeof...(Ts)) && std::is_constructible<nth_type<Ix>, Args...>::value, int> = 0>
    explicit variant(const in_place_index_t<Ix>, Args&&... args)
    {
        this->template construct<Ix>(std::forward<Args>(args)...);
    }

    /// Constructor 8
    template <std::size_t Ix,
              typename U,
              typename... Args,
              std::enable_if_t<(Ix < sizeof...(Ts)) &&
                                   std::is_constructible<nth_type<Ix>, std::initializer_list<U>&, Args...>::value,
                               int> = 0>
    explicit variant(const in_place_index_t<Ix>, const std::initializer_list<U> il, Args&&... args)
    {
        this->template construct<Ix>(il, std::forward<Args>(args)...);
    }

    /// Assignment 1
    /// If the current alternative is different and the copy constructor throws, the variant becomes valueless.
    variant& operator=(const variant& rhs) = default;

    /// Assignment 2
    /// If the current alternative is different and the move constructor throws, the variant becomes valueless.
    variant& operator=(variant&& rhs) noexcept(tys::nothrow_move_constructible&& tys::nothrow_move_assignable) =
        default;

    /// Assignment 3
    // TODO FIXME IMPLEMENT https://en.cppreference.com/w/cpp/utility/variant/operator%3D

    /// These methods only participate in overload resolution if the template parameters are valid.
    /// The type-based overloads only participate if the type is unique in the variant.
    /// If the constructor throws, the variant becomes valueless.
    /// @{
    template <typename T,
              typename... Args,
              std::size_t Ix                                                  = index_of<T>,
              std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
    T& emplace(Args&&... ar)
    {
        return this->template construct<Ix>(std::forward<Args>(ar)...);
    }
    template <typename T,
              typename U,
              typename... Args,
              std::size_t Ix                                                  = index_of<T>,
              std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
    T& emplace(const std::initializer_list<U> il, Args&&... ar)
    {
        return this->template construct<Ix>(il, std::forward<Args>(ar)...);
    }
    template <std::size_t Ix,
              typename... Args,
              std::enable_if_t<(Ix < sizeof...(Ts)), int>                     = 0,
              typename T                                                      = variant_alternative_t<Ix, variant>,
              std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
    T& emplace(Args&&... ar)
    {
        return this->template construct<Ix>(std::forward<Args>(ar)...);
    }
    template <std::size_t Ix,
              typename U,
              typename... Args,
              std::enable_if_t<(Ix < sizeof...(Ts)), int>                     = 0,
              typename T                                                      = variant_alternative_t<Ix, variant>,
              std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
    T& emplace(const std::initializer_list<U> il, Args&&... ar)
    {
        return this->template construct<Ix>(il, std::forward<Args>(ar)...);
    }
    /// @}

    /// Invokes swap() on the Ts if the indices match, otherwise swaps the variants.
    /// In the first case, if an exception is thrown, the state of the values depends on the exception safety of
    /// the swap function called.
    /// In the second case, if the move constructor or the move assignment operator throw,
    /// either or both variants may become valueless.
    void swap(variant& other) noexcept(tys::nothrow_move_constructible&& tys::nothrow_swappable)
    {
        if ((!this->is_valueless()) || (!other.is_valueless()))
        {
            if (this->m_index == other.m_index)
            {
                // If an exception is thrown, the state of the values depends on the exception safety
                // of the swap function called.
                this->chronomorphize([this, &other](const auto index) {
                    using std::swap;  // Engage ADL
                    swap(this->template as<index.value>(), other.template as<index.value>());
                });
            }
            else
            {
                // Exeption safety is important for move operations. Consider the failure modes involved on throwing:
                variant tmp(std::move(*this));  // this may get stuck in the moved-out state
                *this = std::move(other);       // this may become valueless; other may get stuck in the moved-out state
                other = std::move(tmp);         // other may become valueless
            }
        }
    }

    /// The index of the currently held alternative, or \ref variant_npos if the variant is valueless.
    CETL_NODISCARD constexpr std::size_t index() const noexcept
    {
        return this->m_index;
    }

    /// True if the variant is valueless.
    CETL_NODISCARD constexpr bool valueless_by_exception() const noexcept
    {
        return this->is_valueless();
    }
};

// --------------------------------------------------------------------------------------------------------------------

/// Implementation of \ref std::holds_alternative.
template <typename T, typename... Ts>
CETL_NODISCARD constexpr bool holds_alternative(const variant<Ts...>& var) noexcept
{
    return detail::var::unique_index_of<T, Ts...> == var.index();
}

// --------------------------------------------------------------------------------------------------------------------

/// Implementation of \c std::get_if(std::variant).
/// @{
template <std::size_t Ix, typename... Ts>
CETL_NODISCARD constexpr std::add_pointer_t<variant_alternative_t<Ix, variant<Ts...>>> get_if(
    variant<Ts...>* const var) noexcept
{
    return ((var != nullptr) && (var->index() == Ix)) ? &var->template as<Ix>() : nullptr;
}
template <std::size_t Ix, typename... Ts>
CETL_NODISCARD constexpr std::add_pointer_t<const variant_alternative_t<Ix, variant<Ts...>>> get_if(
    const variant<Ts...>* const var) noexcept
{
    return ((var != nullptr) && (var->index() == Ix)) ? &var->template as<Ix>() : nullptr;
}
template <typename T, typename... Ts>
CETL_NODISCARD constexpr std::add_pointer_t<T> get_if(variant<Ts...>* const var) noexcept
{
    return get_if<detail::var::unique_index_of<T, Ts...>>(var);
}
template <typename T, typename... Ts>
CETL_NODISCARD constexpr std::add_pointer_t<const T> get_if(const variant<Ts...>* const var) noexcept
{
    return get_if<detail::var::unique_index_of<T, Ts...>>(var);
}
/// @}

// --------------------------------------------------------------------------------------------------------------------

/// Implementation of \c std::get(std::variant).
/// @{
template <std::size_t Ix, typename... Ts>
CETL_NODISCARD constexpr variant_alternative_t<Ix, variant<Ts...>>& get(variant<Ts...>& var)
{
    detail::var::bad_access_unless(var.index() == Ix);
    return var.template as<Ix>();
}
template <std::size_t Ix, typename... Ts>
CETL_NODISCARD constexpr variant_alternative_t<Ix, variant<Ts...>>&& get(variant<Ts...>&& var)
{
    detail::var::bad_access_unless(var.index() == Ix);
    return std::move(var).template as<Ix>();
}
template <std::size_t Ix, typename... Ts>
CETL_NODISCARD constexpr const variant_alternative_t<Ix, variant<Ts...>>& get(const variant<Ts...>& var)
{
    detail::var::bad_access_unless(var.index() == Ix);
    return var.template as<Ix>();
}
template <std::size_t Ix, typename... Ts>
CETL_NODISCARD constexpr const variant_alternative_t<Ix, variant<Ts...>>&& get(const variant<Ts...>&& var)
{
    detail::var::bad_access_unless(var.index() == Ix);
    return std::move(var).template as<Ix>();
}
template <typename T, typename... Ts>
CETL_NODISCARD constexpr T& get(variant<Ts...>& var)
{
    return get<detail::var::unique_index_of<T, Ts...>>(var);
}
template <typename T, typename... Ts>
CETL_NODISCARD constexpr T&& get(variant<Ts...>&& var)
{
    return get<detail::var::unique_index_of<T, Ts...>>(std::move(var));
}
template <typename T, typename... Ts>
CETL_NODISCARD constexpr const T& get(const variant<Ts...>& var)
{
    return get<detail::var::unique_index_of<T, Ts...>>(var);
}
template <typename T, typename... Ts>
CETL_NODISCARD constexpr const T&& get(const variant<Ts...>&& var)
{
    return get<detail::var::unique_index_of<T, Ts...>>(std::move(var));
}
/// @}

// --------------------------------------------------------------------------------------------------------------------

/// Implementation of \ref std::visit.
template <typename F, typename... Vs>
decltype(auto) visit(F&& fun, Vs&&... vars)
{
    return detail::var::visit(std::forward<F>(fun), std::forward<Vs>(vars)...);
}

// --------------------------------------------------------------------------------------------------------------------

template <typename... Ts>
constexpr bool operator==(const variant<Ts...>& lhs, const variant<Ts...>& rhs)
{
    return (lhs.index() == rhs.index()) && (lhs.index() != variant_npos) &&
           detail::var::chronomorphize<sizeof...(Ts)>(lhs.index(), [&lhs, &rhs](const auto index) {
               return static_cast<bool>(get<index.value>(lhs) == get<index.value>(rhs));
           });
}
template <typename... Ts>
constexpr bool operator!=(const variant<Ts...>& lhs, const variant<Ts...>& rhs)
{
    return !(lhs == rhs);
}
template <typename... Ts>
constexpr bool operator<(const variant<Ts...>& lhs, const variant<Ts...>& rhs)
{
    if (rhs.valueless_by_exception())
    {
        return false;
    }
    if (lhs.valueless_by_exception())
    {
        return true;
    }
    if (lhs.index() < rhs.index())
    {
        return true;
    }
    if (lhs.index() > rhs.index())
    {
        return false;
    }
    return detail::var::chronomorphize<sizeof...(Ts)>(lhs.index(), [&lhs, &rhs](const auto index) {
        return get<index.value>(lhs) < get<index.value>(rhs);
    });
}
template <typename... Ts>
constexpr bool operator>(const variant<Ts...>& lhs, const variant<Ts...>& rhs)
{
    return rhs < lhs;
}
template <typename... Ts>
constexpr bool operator<=(const variant<Ts...>& lhs, const variant<Ts...>& rhs)
{
    return !(rhs < lhs);
}
template <typename... Ts>
constexpr bool operator>=(const variant<Ts...>& lhs, const variant<Ts...>& rhs)
{
    return !(lhs < rhs);
}

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_VARIANT_HPP_INCLUDED
