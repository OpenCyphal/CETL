/// @file
/// Defines the C++23 std::expected type and several related entities.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF23_EXPECTED_HPP_INCLUDED
#define CETL_PF23_EXPECTED_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cetl/pf17/utility.hpp>
#include <cetl/pf17/type_traits.hpp>

#include <exception>  // We need this even if exceptions are disabled for std::terminate.
#include <functional>
#include <initializer_list>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

// From C++20 a constant expression can change the active member of a union (std::construct_at and constexpr
// destructors), so the operations that do so are constexpr there, as they are in std::expected. Before C++20 they
// cannot be. The macro is undefined at the end of this header.
#if (__cplusplus >= CETL_CPP_STANDARD_20) && defined(__cpp_constexpr_dynamic_alloc) && \
    defined(__cpp_lib_constexpr_dynamic_alloc)
#    define CETL_PF23_CONSTEXPR20 constexpr
#else
#    define CETL_PF23_CONSTEXPR20
#endif

namespace cetl
{
namespace pf23
{
template <typename E>
class unexpected;

template <typename T, typename E>
class expected;

/// An implementation of C++23 \c std::unexpect_t.
struct unexpect_t
{
    explicit unexpect_t() = default;
};

/// An implementation of C++23 \c std::unexpect.
constexpr unexpect_t unexpect{};

#if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)
template <typename E>
class bad_expected_access;

/// A polyfill for \c std::bad_expected_access<void>, the base class of every \ref bad_expected_access.
/// This is only available if exceptions are enabled (__cpp_exceptions is defined).
template <>
class bad_expected_access<void> : public std::exception
{
public:
    CETL_NODISCARD const char* what() const noexcept override
    {
        return "bad_expected_access";
    }

protected:
    bad_expected_access() noexcept                                      = default;
    bad_expected_access(const bad_expected_access&) noexcept            = default;
    bad_expected_access(bad_expected_access&&) noexcept                 = default;
    bad_expected_access& operator=(const bad_expected_access&) noexcept = default;
    bad_expected_access& operator=(bad_expected_access&&) noexcept      = default;
    ~bad_expected_access() noexcept override                            = default;
};

/// A polyfill for \c std::bad_expected_access, thrown by expected::value() when the expected holds an error.
/// This is only available if exceptions are enabled (__cpp_exceptions is defined).
template <typename E>
class bad_expected_access : public bad_expected_access<void>
{
public:
    explicit bad_expected_access(E e)
        : m_error(std::move(e))
    {
    }

    /// The error that the expected held.
    /// @{
    CETL_NODISCARD E& error() & noexcept
    {
        return m_error;
    }
    CETL_NODISCARD const E& error() const& noexcept
    {
        return m_error;
    }
    CETL_NODISCARD E&& error() && noexcept
    {
        return std::move(m_error);
    }
    CETL_NODISCARD const E&& error() const&& noexcept
    {
        return std::move(m_error);
    }
    /// @}

private:
    E m_error;
};
#endif

namespace detail
{
namespace exp
{
// In C++17 and later the polyfill takes std::in_place_t, which is what cetl::in_place from cetl/pf17/cetlpf.hpp is
// there, so code written against cetl/pf23/cetlpf.hpp compiles unchanged whether it gets this type or std::expected.
#if __cplusplus >= CETL_CPP_STANDARD_17
using in_place_t = std::in_place_t;
#else
using in_place_t = cetl::pf17::in_place_t;
#endif

template <typename T>
using remove_cvref_t = std::remove_cv_t<std::remove_reference_t<T>>;

/// Makes a type depend on a template parameter so that a constraint on a class template parameter is checked during
/// overload resolution (SFINAE) instead of when the class is instantiated.
template <typename Dependency, typename T>
struct dependent
{
    using type = T;
};
template <typename Dependency, typename T>
using dependent_t = typename dependent<Dependency, T>::type;

template <typename T>
struct is_unexpected : std::false_type
{};
template <typename E>
struct is_unexpected<unexpected<E>> : std::true_type
{};

template <typename T>
struct is_expected : std::false_type
{};
template <typename T, typename E>
struct is_expected<expected<T, E>> : std::true_type
{};

/// True if E is a valid template argument for unexpected: a non-array, non-cv-qualified object type that is not a
/// specialization of unexpected.
template <typename E>
constexpr bool is_valid_error = std::is_object<E>::value && (!std::is_array<E>::value) && (!std::is_const<E>::value) &&
                                (!std::is_volatile<E>::value) && (!is_unexpected<E>::value);

/// True if T is a valid value type for expected: void or a non-array object type that is not in_place_t, unexpect_t or
/// a specialization of unexpected (all ignoring cv-qualifiers).
template <typename T>
constexpr bool is_valid_value = std::is_void<T>::value || (std::is_object<T>::value && (!std::is_array<T>::value) &&
                                                           (!std::is_same<std::remove_cv_t<T>, in_place_t>::value) &&
                                                           (!std::is_same<std::remove_cv_t<T>, unexpect_t>::value) &&
                                                           (!is_unexpected<std::remove_cv_t<T>>::value));

/// std::is_convertible as specified for C++17.
/// CAVEAT: C++14 has no guaranteed copy elision, so std::is_convertible<From, To> is false whenever To cannot be moved,
/// even if From converts to it implicitly. Binding To&& sees the implicit conversion without the move. It cannot bind
/// an lvalue of To itself, but that case is covered by std::is_convertible<From, To>.
#if __cplusplus >= CETL_CPP_STANDARD_17
template <typename From, typename To>
using is_convertible = std::is_convertible<From, To>;
#else
template <typename From, typename To>
using is_convertible = cetl::pf17::disjunction<std::is_convertible<From, To>, std::is_convertible<From, To&&>>;
#endif

/// The converts-from-any-cvref exposition-only concept of the standard.
template <typename T, typename W>
using converts_from_any_cvref = cetl::pf17::disjunction<std::is_constructible<T, W&>,
                                                        is_convertible<W&, T>,
                                                        std::is_constructible<T, W>,
                                                        is_convertible<W, T>,
                                                        std::is_constructible<T, const W&>,
                                                        is_convertible<const W&, T>,
                                                        std::is_constructible<T, const W>,
                                                        is_convertible<const W, T>>;

/// True if unexpected<E> is constructible from any cv-ref qualification of W.
template <typename E, typename W>
using unexpected_from_any_cvref = cetl::pf17::disjunction<std::is_constructible<unexpected<E>, W&>,
                                                          std::is_constructible<unexpected<E>, W>,
                                                          std::is_constructible<unexpected<E>, const W&>,
                                                          std::is_constructible<unexpected<E>, const W>>;

template <typename T>
struct is_reference_wrapper : std::false_type
{};
template <typename U>
struct is_reference_wrapper<std::reference_wrapper<U>> : std::true_type
{};

/// INVOKE as specified for C++17 std::invoke. It is always called qualified, because in C++17 and later an unqualified
/// call would also find std::invoke through ADL.
/// @{
template <typename F, typename... Args>
constexpr auto invoke(F&& f, Args&&... args) -> decltype(std::forward<F>(f)(std::forward<Args>(args)...))
{
    return std::forward<F>(f)(std::forward<Args>(args)...);
}
template <typename M,
          typename C,
          typename Obj,
          typename... Args,
          std::enable_if_t<std::is_function<M>::value && std::is_base_of<C, std::decay_t<Obj>>::value, int> = 0>
constexpr auto invoke(M C::* f, Obj&& obj, Args&&... args)
    -> decltype((std::forward<Obj>(obj).*f)(std::forward<Args>(args)...))
{
    return (std::forward<Obj>(obj).*f)(std::forward<Args>(args)...);
}
template <typename M,
          typename C,
          typename Obj,
          typename... Args,
          std::enable_if_t<std::is_function<M>::value && is_reference_wrapper<std::decay_t<Obj>>::value, int> = 0>
constexpr auto invoke(M C::* f, Obj&& obj, Args&&... args) -> decltype((obj.get().*f)(std::forward<Args>(args)...))
{
    return (obj.get().*f)(std::forward<Args>(args)...);
}
template <typename M,
          typename C,
          typename Obj,
          typename... Args,
          std::enable_if_t<std::is_function<M>::value && (!std::is_base_of<C, std::decay_t<Obj>>::value) &&
                               (!is_reference_wrapper<std::decay_t<Obj>>::value),
                           int> = 0>
constexpr auto invoke(M C::* f, Obj&& obj, Args&&... args)
    -> decltype(((*std::forward<Obj>(obj)).*f)(std::forward<Args>(args)...))
{
    return ((*std::forward<Obj>(obj)).*f)(std::forward<Args>(args)...);
}
template <typename M,
          typename C,
          typename Obj,
          std::enable_if_t<(!std::is_function<M>::value) && std::is_base_of<C, std::decay_t<Obj>>::value, int> = 0>
constexpr auto invoke(M C::* f, Obj&& obj) -> decltype(std::forward<Obj>(obj).*f)
{
    return std::forward<Obj>(obj).*f;
}
template <typename M,
          typename C,
          typename Obj,
          std::enable_if_t<(!std::is_function<M>::value) && is_reference_wrapper<std::decay_t<Obj>>::value, int> = 0>
constexpr auto invoke(M C::* f, Obj&& obj) -> decltype(obj.get().*f)
{
    return obj.get().*f;
}
template <typename M,
          typename C,
          typename Obj,
          std::enable_if_t<(!std::is_function<M>::value) && (!std::is_base_of<C, std::decay_t<Obj>>::value) &&
                               (!is_reference_wrapper<std::decay_t<Obj>>::value),
                           int> = 0>
constexpr auto invoke(M C::* f, Obj&& obj) -> decltype((*std::forward<Obj>(obj)).*f)
{
    return (*std::forward<Obj>(obj)).*f;
}
/// @}

template <typename F, typename... Args>
using invoke_result_t = decltype(detail::exp::invoke(std::declval<F>(), std::declval<Args>()...));

/// Stands in for the value of expected<void, E> so that it can share the storage of expected<T, E>.
struct void_value final
{};

struct copy_tag final
{};
struct invoke_value_tag final
{};
struct invoke_error_tag final
{};

/// Constructs a union member in place. The member may be const-qualified.
template <typename T, typename... Args>
CETL_PF23_CONSTEXPR20 void construct_member(T& member, Args&&... args)
{
#if (__cplusplus >= CETL_CPP_STANDARD_20) && defined(__cpp_lib_constexpr_dynamic_alloc)
    std::construct_at(const_cast<std::remove_cv_t<T>*>(std::addressof(member)), std::forward<Args>(args)...);
#else
    ::new (const_cast<void*>(static_cast<const volatile void*>(std::addressof(member)))) T(std::forward<Args>(args)...);
#endif
}

template <typename T>
CETL_PF23_CONSTEXPR20 void destroy_member(T& member) noexcept
{
    member.~T();
}

/// The reinit-expected exposition-only function of the standard: replaces oldval, the active member of the union, with
/// newval constructed from args. If the construction throws, oldval is restored. When the old member is the stand-in
/// for void there is nothing to restore, so newval is constructed directly as the standard specifies for
/// expected<void, E>.
/// @{
template <typename New, typename Old, typename... Args>
using reinit_strategy =
    std::integral_constant<int,
                           (std::is_nothrow_constructible<New, Args...>::value || std::is_same<Old, void_value>::value)
                               ? 0
                               : (std::is_nothrow_move_constructible<New>::value ? 1 : 2)>;

template <typename New, typename Old, typename... Args>
CETL_PF23_CONSTEXPR20 void reinit(std::integral_constant<int, 0>, New& newval, Old& oldval, Args&&... args)
{
    detail::exp::destroy_member(oldval);
    detail::exp::construct_member(newval, std::forward<Args>(args)...);
}
template <typename New, typename Old, typename... Args>
CETL_PF23_CONSTEXPR20 void reinit(std::integral_constant<int, 1>, New& newval, Old& oldval, Args&&... args)
{
    New tmp(std::forward<Args>(args)...);
    detail::exp::destroy_member(oldval);
    detail::exp::construct_member(newval, std::move(tmp));
}
template <typename New, typename Old, typename... Args>
CETL_PF23_CONSTEXPR20 void reinit(std::integral_constant<int, 2>, New& newval, Old& oldval, Args&&... args)
{
    Old tmp(std::move(oldval));
    detail::exp::destroy_member(oldval);
#if defined(__cpp_exceptions)
    try
    {
        detail::exp::construct_member(newval, std::forward<Args>(args)...);
    } catch (...)
    {
        detail::exp::construct_member(oldval, std::move(tmp));
        throw;
    }
#else
    detail::exp::construct_member(newval, std::forward<Args>(args)...);
#endif
}

template <typename New, typename Old, typename... Args>
CETL_PF23_CONSTEXPR20 void reinit_expected(New& newval, Old& oldval, Args&&... args)
{
    detail::exp::reinit(reinit_strategy<New, Old, Args...>{}, newval, oldval, std::forward<Args>(args)...);
}
/// @}

template <typename E, typename Err>
[[noreturn]] void throw_bad_expected_access(Err&& error)
{
#if defined(__cpp_exceptions)
    throw bad_expected_access<E>(std::forward<Err>(error));
#else
    (void) error;
    std::terminate();
#endif
}

/// DESTRUCTION POLICY
/// T is the value type, or void_value for expected<void, E>.
template <typename T,
          typename E,
          bool = std::is_trivially_destructible<T>::value && std::is_trivially_destructible<E>::value>
struct base_destruction;
/// Trivially destructible case.
template <typename T, typename E>
struct base_destruction<T, E, true>
{
    template <typename... Args>
    constexpr explicit base_destruction(const in_place_t, Args&&... args)
        : m_value(std::forward<Args>(args)...)
        , m_has_value(true)
    {
    }
    template <typename... Args>
    constexpr explicit base_destruction(const unexpect_t, Args&&... args)
        : m_error(std::forward<Args>(args)...)
        , m_has_value(false)
    {
    }
    // The invoke constructors initialize the member from the result of the call directly, so that in C++17 and later a
    // transform() result that cannot be moved is still constructed in place as the standard requires.
    template <typename F, typename... Args>
    constexpr explicit base_destruction(const invoke_value_tag, F&& f, Args&&... args)
        : m_value(detail::exp::invoke(std::forward<F>(f), std::forward<Args>(args)...))
        , m_has_value(true)
    {
    }
    template <typename F, typename... Args>
    constexpr explicit base_destruction(const invoke_error_tag, F&& f, Args&&... args)
        : m_error(detail::exp::invoke(std::forward<F>(f), std::forward<Args>(args)...))
        , m_has_value(false)
    {
    }
    template <typename Other>
    CETL_PF23_CONSTEXPR20 explicit base_destruction(const copy_tag, Other&& other)
        : m_has_value(other.m_has_value)
    {
        if (m_has_value)
        {
            detail::exp::construct_member(m_value, std::forward<Other>(other).m_value);
        }
        else
        {
            detail::exp::construct_member(m_error, std::forward<Other>(other).m_error);
        }
    }
    union
    {
        T m_value;
        E m_error;
    };
    bool m_has_value;
};
/// Non-trivially destructible case.
template <typename T, typename E>
struct base_destruction<T, E, false>
{
    template <typename... Args>
    constexpr explicit base_destruction(const in_place_t, Args&&... args)
        : m_value(std::forward<Args>(args)...)
        , m_has_value(true)
    {
    }
    template <typename... Args>
    constexpr explicit base_destruction(const unexpect_t, Args&&... args)
        : m_error(std::forward<Args>(args)...)
        , m_has_value(false)
    {
    }
    template <typename F, typename... Args>
    constexpr explicit base_destruction(const invoke_value_tag, F&& f, Args&&... args)
        : m_value(detail::exp::invoke(std::forward<F>(f), std::forward<Args>(args)...))
        , m_has_value(true)
    {
    }
    template <typename F, typename... Args>
    constexpr explicit base_destruction(const invoke_error_tag, F&& f, Args&&... args)
        : m_error(detail::exp::invoke(std::forward<F>(f), std::forward<Args>(args)...))
        , m_has_value(false)
    {
    }
    template <typename Other>
    CETL_PF23_CONSTEXPR20 explicit base_destruction(const copy_tag, Other&& other)
        : m_has_value(other.m_has_value)
    {
        if (m_has_value)
        {
            detail::exp::construct_member(m_value, std::forward<Other>(other).m_value);
        }
        else
        {
            detail::exp::construct_member(m_error, std::forward<Other>(other).m_error);
        }
    }
    constexpr base_destruction(const base_destruction&)            = default;
    constexpr base_destruction(base_destruction&&)                 = default;
    constexpr base_destruction& operator=(const base_destruction&) = default;
    constexpr base_destruction& operator=(base_destruction&&)      = default;
    CETL_PF23_CONSTEXPR20 ~base_destruction() noexcept
    {
        if (m_has_value)
        {
            detail::exp::destroy_member(m_value);
        }
        else
        {
            detail::exp::destroy_member(m_error);
        }
    }
    union
    {
        T m_value;
        E m_error;
    };
    bool m_has_value;
};

/// COPY CONSTRUCTION POLICY
template <typename T,
          typename E,
          bool = std::is_trivially_copy_constructible<T>::value && std::is_trivially_copy_constructible<E>::value,
          bool = std::is_copy_constructible<T>::value && std::is_copy_constructible<E>::value>
struct base_copy_construction;
/// Trivially copy constructible case.
template <typename T, typename E>
struct base_copy_construction<T, E, true, true> : base_destruction<T, E>
{
    using base_destruction<T, E>::base_destruction;
};
/// Non-trivially copy constructible case.
template <typename T, typename E>
struct base_copy_construction<T, E, false, true> : base_destruction<T, E>
{
    using base_destruction<T, E>::base_destruction;
    CETL_PF23_CONSTEXPR20 base_copy_construction(const base_copy_construction& other)
        : base_destruction<T, E>(copy_tag{}, other)
    {
    }
    constexpr base_copy_construction(base_copy_construction&&)                 = default;
    constexpr base_copy_construction& operator=(const base_copy_construction&) = default;
    constexpr base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                                         = default;
};
/// Disabled copy construction case.
template <typename T, typename E>
struct base_copy_construction<T, E, false, false> : base_destruction<T, E>
{
    using base_destruction<T, E>::base_destruction;
    constexpr base_copy_construction(const base_copy_construction&)            = delete;
    constexpr base_copy_construction(base_copy_construction&&)                 = default;
    constexpr base_copy_construction& operator=(const base_copy_construction&) = default;
    constexpr base_copy_construction& operator=(base_copy_construction&&)      = default;
    ~base_copy_construction() noexcept                                         = default;
};

/// MOVE CONSTRUCTION POLICY
template <typename T,
          typename E,
          bool = std::is_trivially_move_constructible<T>::value && std::is_trivially_move_constructible<E>::value,
          bool = std::is_move_constructible<T>::value && std::is_move_constructible<E>::value>
struct base_move_construction;
/// Trivially move constructible case.
template <typename T, typename E>
struct base_move_construction<T, E, true, true> : base_copy_construction<T, E>
{
    using base_copy_construction<T, E>::base_copy_construction;
};
/// Non-trivially move constructible case.
template <typename T, typename E>
struct base_move_construction<T, E, false, true> : base_copy_construction<T, E>
{
    using base_copy_construction<T, E>::base_copy_construction;
    constexpr base_move_construction(const base_move_construction&) = default;
    CETL_PF23_CONSTEXPR20 base_move_construction(base_move_construction&& other) noexcept(
        std::is_nothrow_move_constructible<T>::value && std::is_nothrow_move_constructible<E>::value)
        : base_copy_construction<T, E>(copy_tag{}, std::move(other))
    {
    }
    constexpr base_move_construction& operator=(const base_move_construction&) = default;
    constexpr base_move_construction& operator=(base_move_construction&&)      = default;
    ~base_move_construction() noexcept                                         = default;
};
/// Non-move-constructible case. As in optional, this deletion only mirrors the other disabled cases; the one that takes
/// effect is in the final layer, base_move_assignment<T, E, false, false, false>.
template <typename T, typename E>
struct base_move_construction<T, E, false, false> : base_copy_construction<T, E>
{
    using base_copy_construction<T, E>::base_copy_construction;
    constexpr base_move_construction(const base_move_construction&)            = default;
    constexpr base_move_construction(base_move_construction&&)                 = delete;
    constexpr base_move_construction& operator=(const base_move_construction&) = default;
    constexpr base_move_construction& operator=(base_move_construction&&)      = default;
    ~base_move_construction() noexcept                                         = default;
};

/// True if either alternative can be moved without throwing, which the standard requires of every assignment that may
/// switch between the value and the error so that the strong exception guarantee can be given.
template <typename T, typename E>
constexpr bool either_nothrow_move_constructible =
    std::is_nothrow_move_constructible<T>::value || std::is_nothrow_move_constructible<E>::value;

/// COPY ASSIGNMENT POLICY
template <typename T, typename E>
constexpr bool copy_assignment_enabled =
    std::is_copy_assignable<T>::value && std::is_copy_constructible<T>::value && std::is_copy_assignable<E>::value &&
    std::is_copy_constructible<E>::value && either_nothrow_move_constructible<T, E>;
template <typename T,
          typename E,
          bool = copy_assignment_enabled<T, E> &&                                                                  //
                 std::is_trivially_copy_assignable<T>::value && std::is_trivially_copy_constructible<T>::value &&  //
                 std::is_trivially_destructible<T>::value &&                                                       //
                 std::is_trivially_copy_assignable<E>::value && std::is_trivially_copy_constructible<E>::value &&  //
                 std::is_trivially_destructible<E>::value,
          bool = copy_assignment_enabled<T, E>>
struct base_copy_assignment;
/// Trivially copy assignable case.
template <typename T, typename E>
struct base_copy_assignment<T, E, true, true> : base_move_construction<T, E>
{
    using base_move_construction<T, E>::base_move_construction;
};
/// Non-trivially copy assignable case.
template <typename T, typename E>
struct base_copy_assignment<T, E, false, true> : base_move_construction<T, E>
{
    using base_move_construction<T, E>::base_move_construction;
    constexpr base_copy_assignment(const base_copy_assignment&) = default;
    constexpr base_copy_assignment(base_copy_assignment&&)      = default;
    CETL_PF23_CONSTEXPR20 base_copy_assignment& operator=(const base_copy_assignment& other)
    {
        if (this->m_has_value && other.m_has_value)
        {
            this->m_value = other.m_value;
        }
        else if (this->m_has_value)
        {
            detail::exp::reinit_expected(this->m_error, this->m_value, other.m_error);
            this->m_has_value = false;
        }
        else if (other.m_has_value)
        {
            detail::exp::reinit_expected(this->m_value, this->m_error, other.m_value);
            this->m_has_value = true;
        }
        else
        {
            this->m_error = other.m_error;
        }
        return *this;
    }
    constexpr base_copy_assignment& operator=(base_copy_assignment&&) = default;
    ~base_copy_assignment() noexcept                                  = default;
};
/// Disabled copy assignment case.
template <typename T, typename E>
struct base_copy_assignment<T, E, false, false> : base_move_construction<T, E>
{
    using base_move_construction<T, E>::base_move_construction;
    constexpr base_copy_assignment(const base_copy_assignment&)            = default;
    constexpr base_copy_assignment(base_copy_assignment&&)                 = default;
    constexpr base_copy_assignment& operator=(const base_copy_assignment&) = delete;
    constexpr base_copy_assignment& operator=(base_copy_assignment&&)      = default;
    ~base_copy_assignment() noexcept                                       = default;
};

/// MOVE ASSIGNMENT POLICY
template <typename T, typename E>
constexpr bool move_assignment_enabled =
    std::is_move_assignable<T>::value && std::is_move_constructible<T>::value && std::is_move_assignable<E>::value &&
    std::is_move_constructible<E>::value && either_nothrow_move_constructible<T, E>;
template <typename T,
          typename E,
          bool = move_assignment_enabled<T, E> &&                                                                  //
                 std::is_trivially_move_assignable<T>::value && std::is_trivially_move_constructible<T>::value &&  //
                 std::is_trivially_destructible<T>::value &&                                                       //
                 std::is_trivially_move_assignable<E>::value && std::is_trivially_move_constructible<E>::value &&  //
                 std::is_trivially_destructible<E>::value,
          bool = move_assignment_enabled<T, E>,
          bool = std::is_move_constructible<T>::value && std::is_move_constructible<E>::value>
struct base_move_assignment;
/// Trivially move assignable case.
template <typename T, typename E>
struct base_move_assignment<T, E, true, true, true> : base_copy_assignment<T, E>
{
    using base_copy_assignment<T, E>::base_copy_assignment;
};
/// Non-trivially move assignable case.
template <typename T, typename E>
struct base_move_assignment<T, E, false, true, true> : base_copy_assignment<T, E>
{
    using base_copy_assignment<T, E>::base_copy_assignment;
    constexpr base_move_assignment(const base_move_assignment&)            = default;
    constexpr base_move_assignment(base_move_assignment&&)                 = default;
    constexpr base_move_assignment& operator=(const base_move_assignment&) = default;
    base_move_assignment&           operator=(base_move_assignment&& other) noexcept(
        std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value &&
        std::is_nothrow_move_assignable<E>::value && std::is_nothrow_move_constructible<E>::value)
    {
        if (this->m_has_value && other.m_has_value)
        {
            this->m_value = std::move(other.m_value);
        }
        else if (this->m_has_value)
        {
            detail::exp::reinit_expected(this->m_error, this->m_value, std::move(other.m_error));
            this->m_has_value = false;
        }
        else if (other.m_has_value)
        {
            detail::exp::reinit_expected(this->m_value, this->m_error, std::move(other.m_value));
            this->m_has_value = true;
        }
        else
        {
            this->m_error = std::move(other.m_error);
        }
        return *this;
    }
    ~base_move_assignment() noexcept = default;
};
/// Disabled move assignment case.
template <typename T, typename E>
struct base_move_assignment<T, E, false, false, true> : base_copy_assignment<T, E>
{
    using base_copy_assignment<T, E>::base_copy_assignment;
    constexpr base_move_assignment(const base_move_assignment&)            = default;
    constexpr base_move_assignment(base_move_assignment&&)                 = default;
    constexpr base_move_assignment& operator=(const base_move_assignment&) = default;
    constexpr base_move_assignment& operator=(base_move_assignment&&)      = delete;
    ~base_move_assignment() noexcept                                       = default;
};
/// Keep the move constructor explicitly deleted in the final policy layer when T or E cannot be moved, so that the
/// defaulted move constructor of expected is defined as deleted and rvalues bind to the copy constructor instead. See
/// the matching comment in cetl/pf17/optional.hpp.
template <typename T, typename E>
struct base_move_assignment<T, E, false, false, false> : base_copy_assignment<T, E>
{
    using base_copy_assignment<T, E>::base_copy_assignment;
    constexpr base_move_assignment(const base_move_assignment&)            = default;
    constexpr base_move_assignment(base_move_assignment&&)                 = delete;
    constexpr base_move_assignment& operator=(const base_move_assignment&) = default;
    constexpr base_move_assignment& operator=(base_move_assignment&&)      = delete;
    ~base_move_assignment() noexcept                                       = default;
};

/// CONSTRAINTS
/// https://en.cppreference.com/w/cpp/utility/expected/expected
template <typename T, typename E, typename U>
using enable_value_ctor =
    cetl::pf17::conjunction<cetl::pf17::negation<std::is_same<remove_cvref_t<U>, in_place_t>>,
                            cetl::pf17::negation<std::is_same<remove_cvref_t<U>, expected<T, E>>>,
                            cetl::pf17::negation<std::is_same<remove_cvref_t<U>, unexpect_t>>,
                            cetl::pf17::negation<is_unexpected<remove_cvref_t<U>>>,
                            std::is_constructible<T, U>,
                            cetl::pf17::disjunction<cetl::pf17::negation<std::is_same<std::remove_cv_t<T>, bool>>,
                                                    cetl::pf17::negation<is_expected<remove_cvref_t<U>>>>>;

template <typename T, typename E, typename U, typename G, typename UF, typename GF>
using enable_converting_ctor =
    cetl::pf17::conjunction<std::is_constructible<T, UF>,
                            std::is_constructible<E, GF>,
                            cetl::pf17::disjunction<std::is_same<std::remove_cv_t<T>, bool>,
                                                    cetl::pf17::negation<converts_from_any_cvref<T, expected<U, G>>>>,
                            cetl::pf17::negation<unexpected_from_any_cvref<E, expected<U, G>>>>;

template <typename E, typename U, typename G, typename GF>
using enable_void_converting_ctor =
    cetl::pf17::conjunction<std::is_void<U>,
                            std::is_constructible<E, GF>,
                            cetl::pf17::negation<unexpected_from_any_cvref<E, expected<U, G>>>>;

/// https://en.cppreference.com/w/cpp/utility/expected/operator%3D
template <typename T, typename E, typename U>
using enable_value_assignment =
    cetl::pf17::conjunction<cetl::pf17::negation<std::is_same<expected<T, E>, remove_cvref_t<U>>>,
                            cetl::pf17::negation<is_unexpected<remove_cvref_t<U>>>,
                            std::is_constructible<T, U>,
                            std::is_assignable<T&, U>,
                            cetl::pf17::disjunction<std::is_nothrow_constructible<T, U>,
                                                    std::is_nothrow_move_constructible<T>,
                                                    std::is_nothrow_move_constructible<E>>>;

/// T is void_value for expected<void, E>, which makes the last condition true as the standard specifies for it.
template <typename T, typename E, typename GF>
using enable_unexpected_assignment =
    cetl::pf17::conjunction<std::is_constructible<E, GF>,
                            std::is_assignable<E&, GF>,
                            cetl::pf17::disjunction<std::is_nothrow_constructible<E, GF>,
                                                    std::is_nothrow_move_constructible<T>,
                                                    std::is_nothrow_move_constructible<E>>>;

/// https://en.cppreference.com/w/cpp/utility/expected/swap
template <typename T, typename E>
using enable_swap = cetl::pf17::conjunction<
    cetl::pf17::is_swappable<T>,
    cetl::pf17::is_swappable<E>,
    std::is_move_constructible<T>,
    std::is_move_constructible<E>,
    cetl::pf17::disjunction<std::is_nothrow_move_constructible<T>, std::is_nothrow_move_constructible<E>>>;

template <typename E>
using enable_void_swap = cetl::pf17::conjunction<cetl::pf17::is_swappable<E>, std::is_move_constructible<E>>;

/// The equality operators take part in overload resolution only if every comparison they make is well-formed and
/// yields something implicitly convertible to bool (P3379R0). They return the result of the comparison, so that
/// conversion is the one used, never an explicit one (LWG 4366).
template <typename... Results>
using enable_comparison = std::enable_if_t<cetl::pf17::conjunction<std::is_convertible<Results, bool>...>::value, bool>;

}  // namespace exp
}  // namespace detail

/// A strict implementation of C++23 \c std::unexpected: the error held by an \ref expected.
template <typename E>
class unexpected
{
    static_assert(detail::exp::is_valid_error<E>,
                  "E must be a non-array object type that is neither cv-qualified nor a specialization of unexpected.");

public:
    constexpr unexpected(const unexpected&) = default;
    constexpr unexpected(unexpected&&)      = default;

    /// Constructs the error from \p e.
    template <
        typename Err                                                                                           = E,
        std::enable_if_t<!std::is_same<detail::exp::remove_cvref_t<Err>, unexpected>::value, int>              = 0,
        std::enable_if_t<!std::is_same<detail::exp::remove_cvref_t<Err>, detail::exp::in_place_t>::value, int> = 0,
        std::enable_if_t<std::is_constructible<E, Err>::value, int>                                            = 0>
    constexpr explicit unexpected(Err&& e) noexcept(std::is_nothrow_constructible<E, Err>::value)
        : m_error(std::forward<Err>(e))
    {
    }

    /// Constructs the error in place from \p args.
    template <typename... Args, std::enable_if_t<std::is_constructible<E, Args...>::value, int> = 0>
    constexpr explicit unexpected(const detail::exp::in_place_t,
                                  Args&&... args) noexcept(std::is_nothrow_constructible<E, Args...>::value)
        : m_error(std::forward<Args>(args)...)
    {
    }
    template <typename U,
              typename... Args,
              std::enable_if_t<std::is_constructible<E, std::initializer_list<U>&, Args...>::value, int> = 0>
    constexpr explicit unexpected(const detail::exp::in_place_t, std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible<E, std::initializer_list<U>&, Args...>::value)
        : m_error(il, std::forward<Args>(args)...)
    {
    }

    ~unexpected() = default;

    unexpected& operator=(const unexpected&) = default;
    unexpected& operator=(unexpected&&)      = default;

    /// The error.
    /// @{
    CETL_NODISCARD constexpr const E& error() const& noexcept
    {
        return m_error;
    }
    CETL_NODISCARD constexpr E& error() & noexcept
    {
        return m_error;
    }
    CETL_NODISCARD constexpr const E&& error() const&& noexcept
    {
        return std::move(m_error);
    }
    CETL_NODISCARD constexpr E&& error() && noexcept
    {
        return std::move(m_error);
    }
    /// @}

    /// Swaps the errors.
    CETL_PF23_CONSTEXPR20 void swap(unexpected& other) noexcept(cetl::pf17::is_nothrow_swappable<E>::value)
    {
        static_assert(cetl::pf17::is_swappable<E>::value, "E must be swappable.");
        using std::swap;
        swap(m_error, other.m_error);
    }

    template <typename Dependency                                                                             = void,
              std::enable_if_t<cetl::pf17::is_swappable<detail::exp::dependent_t<Dependency, E>>::value, int> = 0>
    friend CETL_PF23_CONSTEXPR20 void swap(unexpected& x, unexpected& y) noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }

    /// Compares the errors.
    /// @{
    template <typename E2>
    friend constexpr bool operator==(const unexpected& x, const unexpected<E2>& y)
    {
        return x.error() == y.error();
    }
    template <typename E2>
    friend constexpr bool operator!=(const unexpected& x, const unexpected<E2>& y)
    {
        return !(x == y);
    }
    /// @}

private:
    E m_error;
};

#if defined(__cpp_deduction_guides)
template <typename E>
unexpected(E) -> unexpected<E>;
#endif

/// A strict implementation of C++23 \c std::expected: either a value of type T or an error of type E.
///
/// Differences from \c std::expected that are visible to a program:
/// - Copy and move assignment are trivial when T and E are trivially copyable, so
///   \c std::is_trivially_copy_assignable<expected<T, E>> can be true where the standard leaves it unspecified.
/// - Before C++20 the members that change which alternative is held (assignment, emplace(), swap(), and constructors
///   that convert from another expected) are not \c constexpr, because only C++20 lets a constant expression change
///   the active member of a union.
/// - In C++14 transform() and transform_error() require the new value or error type to be move constructible, because
///   C++14 has no guaranteed copy elision.
/// - In C++14 the in-place constructors take \c cetl::pf17::in_place_t; in C++17 and later they take
///   \c std::in_place_t. \c cetl::in_place from cetl/pf17/cetlpf.hpp is the right one in every case.
/// - cv-qualified void is not supported as the value type; plain void is.
/// - If exceptions are disabled, value() calls \c std::terminate instead of throwing bad_expected_access.
///
/// @tparam T   The value type: a non-array object type other than \c in_place_t, \ref unexpect_t or a specialization of
///             \ref unexpected, or void.
/// @tparam E   The error type: any type that is a valid template argument for \ref unexpected.
template <typename T, typename E>
class expected : private detail::exp::base_move_assignment<T, E>
{
    template <typename, typename>
    friend class expected;

    using base = detail::exp::base_move_assignment<T, E>;

    static_assert(std::is_object<T>::value && (!std::is_array<T>::value),
                  "T must be a non-array object type or (not cv-qualified) void.");
    static_assert(!std::is_same<std::remove_cv_t<T>, detail::exp::in_place_t>::value, "T must not be in_place_t.");
    static_assert(!std::is_same<std::remove_cv_t<T>, unexpect_t>::value, "T must not be unexpect_t.");
    static_assert(!detail::exp::is_unexpected<std::remove_cv_t<T>>::value,
                  "T must not be a specialization of unexpected.");
    static_assert(detail::exp::is_valid_error<E>, "E must be a valid template argument for unexpected.");

public:
    using value_type      = T;
    using error_type      = E;
    using unexpected_type = unexpected<E>;

    template <typename U>
    using rebind = expected<U, error_type>;

    /// Constructs an expected that holds a value-initialized T.
    template <typename Dependency = void,
              std::enable_if_t<std::is_default_constructible<detail::exp::dependent_t<Dependency, T>>::value, int> = 0>
    constexpr expected() noexcept(std::is_nothrow_default_constructible<T>::value)
        : base(detail::exp::in_place_t{})
    {
    }

    /// Copy constructor. Deleted unless T and E are copy constructible; trivial if they are trivially so.
    constexpr expected(const expected&) = default;

    /// Move constructor. Trivial if T and E are trivially move constructible.
    constexpr expected(expected&&) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                            std::is_nothrow_move_constructible<E>::value) = default;

    /// Converts from another expected.
    /// @{
    template <
        typename U,
        typename G,
        std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_converting_ctor<T, E, U, G, const U&, const G&>,
                                                 detail::exp::is_convertible<const U&, T>,
                                                 detail::exp::is_convertible<const G&, E>>::value,
                         int> = 0>
    CETL_PF23_CONSTEXPR20 expected(const expected<U, G>& other)  // NOLINT(*-explicit-constructor)
        : base(detail::exp::copy_tag{}, static_cast<const typename expected<U, G>::base&>(other))
    {
    }
    template <typename U,
              typename G,
              std::enable_if_t<
                  cetl::pf17::conjunction<
                      detail::exp::enable_converting_ctor<T, E, U, G, const U&, const G&>,
                      cetl::pf17::negation<cetl::pf17::conjunction<detail::exp::is_convertible<const U&, T>,
                                                                   detail::exp::is_convertible<const G&, E>>>>::value,
                  int> = 0>
    CETL_PF23_CONSTEXPR20 explicit expected(const expected<U, G>& other)
        : base(detail::exp::copy_tag{}, static_cast<const typename expected<U, G>::base&>(other))
    {
    }
    template <typename U,
              typename G,
              std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_converting_ctor<T, E, U, G, U, G>,
                                                       detail::exp::is_convertible<U, T>,
                                                       detail::exp::is_convertible<G, E>>::value,
                               int> = 0>
    CETL_PF23_CONSTEXPR20 expected(expected<U, G>&& other)  // NOLINT(*-explicit-constructor)
        : base(detail::exp::copy_tag{}, static_cast<typename expected<U, G>::base&&>(other))
    {
    }
    template <
        typename U,
        typename G,
        std::enable_if_t<cetl::pf17::conjunction<
                             detail::exp::enable_converting_ctor<T, E, U, G, U, G>,
                             cetl::pf17::negation<cetl::pf17::conjunction<detail::exp::is_convertible<U, T>,
                                                                          detail::exp::is_convertible<G, E>>>>::value,
                         int> = 0>
    CETL_PF23_CONSTEXPR20 explicit expected(expected<U, G>&& other)
        : base(detail::exp::copy_tag{}, static_cast<typename expected<U, G>::base&&>(other))
    {
    }
    /// @}

    /// Constructs the value from \p value.
    /// @{
    template <typename U            = std::remove_cv_t<T>,
              std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_value_ctor<T, E, U>,
                                                       detail::exp::is_convertible<U, T>>::value,
                               int> = 0>
    constexpr expected(U&& value)  // NOLINT(*-explicit-constructor)
        noexcept(std::is_nothrow_constructible<T, U>::value)
        : base(detail::exp::in_place_t{}, std::forward<U>(value))
    {
    }
    template <typename U            = std::remove_cv_t<T>,
              std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_value_ctor<T, E, U>,
                                                       cetl::pf17::negation<detail::exp::is_convertible<U, T>>>::value,
                               int> = 0>
    constexpr explicit expected(U&& value) noexcept(std::is_nothrow_constructible<T, U>::value)
        : base(detail::exp::in_place_t{}, std::forward<U>(value))
    {
    }
    /// @}

    /// Constructs the error from an \ref unexpected.
    /// @{
    template <typename G,
              std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, const G&>,
                                                       detail::exp::is_convertible<const G&, E>>::value,
                               int> = 0>
    constexpr expected(const unexpected<G>& e)  // NOLINT(*-explicit-constructor)
        noexcept(std::is_nothrow_constructible<E, const G&>::value)
        : base(unexpect, e.error())
    {
    }
    template <
        typename G,
        std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, const G&>,
                                                 cetl::pf17::negation<detail::exp::is_convertible<const G&, E>>>::value,
                         int> = 0>
    constexpr explicit expected(const unexpected<G>& e) noexcept(std::is_nothrow_constructible<E, const G&>::value)
        : base(unexpect, e.error())
    {
    }
    template <
        typename G,
        std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, G>, detail::exp::is_convertible<G, E>>::value,
                         int> = 0>
    constexpr expected(unexpected<G>&& e)  // NOLINT(*-explicit-constructor)
        noexcept(std::is_nothrow_constructible<E, G>::value)
        : base(unexpect, std::move(e).error())
    {
    }
    template <typename G,
              std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, G>,
                                                       cetl::pf17::negation<detail::exp::is_convertible<G, E>>>::value,
                               int> = 0>
    constexpr explicit expected(unexpected<G>&& e) noexcept(std::is_nothrow_constructible<E, G>::value)
        : base(unexpect, std::move(e).error())
    {
    }
    /// @}

    /// Constructs the value in place.
    /// @{
    template <typename... Args, std::enable_if_t<std::is_constructible<T, Args...>::value, int> = 0>
    constexpr explicit expected(const detail::exp::in_place_t,
                                Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value)
        : base(detail::exp::in_place_t{}, std::forward<Args>(args)...)
    {
    }
    template <typename U,
              typename... Args,
              std::enable_if_t<std::is_constructible<T, std::initializer_list<U>&, Args...>::value, int> = 0>
    constexpr explicit expected(const detail::exp::in_place_t, std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible<T, std::initializer_list<U>&, Args...>::value)
        : base(detail::exp::in_place_t{}, il, std::forward<Args>(args)...)
    {
    }
    /// @}

    /// Constructs the error in place.
    /// @{
    template <typename... Args, std::enable_if_t<std::is_constructible<E, Args...>::value, int> = 0>
    constexpr explicit expected(const unexpect_t,
                                Args&&... args) noexcept(std::is_nothrow_constructible<E, Args...>::value)
        : base(unexpect, std::forward<Args>(args)...)
    {
    }
    template <typename U,
              typename... Args,
              std::enable_if_t<std::is_constructible<E, std::initializer_list<U>&, Args...>::value, int> = 0>
    constexpr explicit expected(const unexpect_t, std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible<E, std::initializer_list<U>&, Args...>::value)
        : base(unexpect, il, std::forward<Args>(args)...)
    {
    }
    /// @}

    ~expected() = default;

    /// Copy assignment. Deleted unless T and E are copy constructible and copy assignable and at least one of them is
    /// nothrow move constructible. If the assignment switches between value and error and constructing the new
    /// alternative throws, the old one is restored (strong exception guarantee).
    expected& operator=(const expected&) = default;

    /// Move assignment. Same constraints and guarantee as the copy assignment, for moving.
    expected& operator=(expected&&) noexcept(std::is_nothrow_move_assignable<T>::value &&
                                             std::is_nothrow_move_constructible<T>::value &&
                                             std::is_nothrow_move_assignable<E>::value &&
                                             std::is_nothrow_move_constructible<E>::value) = default;

    /// Assigns a value.
    template <typename U                                                                  = std::remove_cv_t<T>,
              std::enable_if_t<detail::exp::enable_value_assignment<T, E, U>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 expected& operator=(U&& value)
    {
        if (has_value())
        {
            this->m_value = std::forward<U>(value);
        }
        else
        {
            detail::exp::reinit_expected(this->m_value, this->m_error, std::forward<U>(value));
            this->m_has_value = true;
        }
        return *this;
    }

    /// Assigns an error.
    /// @{
    template <typename G, std::enable_if_t<detail::exp::enable_unexpected_assignment<T, E, const G&>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 expected& operator=(const unexpected<G>& e)
    {
        assign_error(e.error());
        return *this;
    }
    template <typename G, std::enable_if_t<detail::exp::enable_unexpected_assignment<T, E, G>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 expected& operator=(unexpected<G>&& e)
    {
        assign_error(std::move(e).error());
        return *this;
    }
    /// @}

    /// Destroys the value or the error and constructs a value in place.
    /// @{
    template <typename... Args, std::enable_if_t<std::is_nothrow_constructible<T, Args...>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 T& emplace(Args&&... args) noexcept
    {
        destroy_active();
        detail::exp::construct_member(this->m_value, std::forward<Args>(args)...);
        this->m_has_value = true;
        return this->m_value;
    }
    template <typename U,
              typename... Args,
              std::enable_if_t<std::is_nothrow_constructible<T, std::initializer_list<U>&, Args...>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 T& emplace(std::initializer_list<U> il, Args&&... args) noexcept
    {
        destroy_active();
        detail::exp::construct_member(this->m_value, il, std::forward<Args>(args)...);
        this->m_has_value = true;
        return this->m_value;
    }
    /// @}

    /// Swaps the contents. If one holds a value and the other an error, the alternative that is nothrow move
    /// constructible is moved first so that the swap gives the strong exception guarantee.
    template <typename Dependency                                                                                = void,
              std::enable_if_t<detail::exp::enable_swap<detail::exp::dependent_t<Dependency, T>, E>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 void swap(expected& other) noexcept(std::is_nothrow_move_constructible<T>::value &&
                                                              cetl::pf17::is_nothrow_swappable<T>::value &&
                                                              std::is_nothrow_move_constructible<E>::value &&
                                                              cetl::pf17::is_nothrow_swappable<E>::value)
    {
        if (has_value() && other.has_value())
        {
            using std::swap;
            swap(this->m_value, other.m_value);
        }
        else if (has_value())
        {
            swap_value_with_error(other, std::is_nothrow_move_constructible<E>{});
            this->m_has_value = false;
            other.m_has_value = true;
        }
        else if (other.has_value())
        {
            other.swap(*this);
        }
        else
        {
            using std::swap;
            swap(this->m_error, other.m_error);
        }
    }

    template <typename Dependency                                                                                = void,
              std::enable_if_t<detail::exp::enable_swap<detail::exp::dependent_t<Dependency, T>, E>::value, int> = 0>
    friend CETL_PF23_CONSTEXPR20 void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }

    /// Unchecked access to the value. The behavior is undefined if the expected holds an error.
    /// @{
    constexpr const T* operator->() const noexcept
    {
        CETL_DEBUG_ASSERT(has_value(), "CDE_expected_001: operator-> requires a value.");
        return std::addressof(this->m_value);
    }
    constexpr T* operator->() noexcept
    {
        CETL_DEBUG_ASSERT(has_value(), "CDE_expected_002: operator-> requires a value.");
        return std::addressof(this->m_value);
    }
    constexpr const T& operator*() const& noexcept
    {
        CETL_DEBUG_ASSERT(has_value(), "CDE_expected_003: operator* requires a value.");
        return this->m_value;
    }
    constexpr T& operator*() & noexcept
    {
        CETL_DEBUG_ASSERT(has_value(), "CDE_expected_004: operator* requires a value.");
        return this->m_value;
    }
    constexpr const T&& operator*() const&& noexcept
    {
        CETL_DEBUG_ASSERT(has_value(), "CDE_expected_005: operator* requires a value.");
        return std::move(this->m_value);
    }
    constexpr T&& operator*() && noexcept
    {
        CETL_DEBUG_ASSERT(has_value(), "CDE_expected_006: operator* requires a value.");
        return std::move(this->m_value);
    }
    /// @}

    /// True if the expected holds a value.
    /// @{
    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }
    CETL_NODISCARD constexpr bool has_value() const noexcept
    {
        return this->m_has_value;
    }
    /// @}

    /// Checked access to the value.
    /// If the expected holds an error, the behavior depends on whether exception handling is enabled:
    /// - If exceptions are enabled, a bad_expected_access<E> holding a copy (or for rvalues a move) of the error is
    ///   thrown.
    /// - If exceptions are disabled, std::terminate is called.
    /// @{
    CETL_NODISCARD constexpr const T& value() const&
    {
        static_assert(std::is_copy_constructible<E>::value, "value() requires a copy constructible error type.");
        if (!has_value())
        {
            detail::exp::throw_bad_expected_access<E>(this->m_error);
        }
        return this->m_value;
    }
    CETL_NODISCARD constexpr T& value() &
    {
        static_assert(std::is_copy_constructible<E>::value, "value() requires a copy constructible error type.");
        if (!has_value())
        {
            detail::exp::throw_bad_expected_access<E>(static_cast<const E&>(this->m_error));
        }
        return this->m_value;
    }
    CETL_NODISCARD constexpr const T&& value() const&&
    {
        static_assert(std::is_copy_constructible<E>::value && std::is_constructible<E, const E&&>::value,
                      "value() requires a copy constructible error type.");
        if (!has_value())
        {
            detail::exp::throw_bad_expected_access<E>(std::move(this->m_error));
        }
        return std::move(this->m_value);
    }
    CETL_NODISCARD constexpr T&& value() &&
    {
        static_assert(std::is_copy_constructible<E>::value && std::is_constructible<E, E&&>::value,
                      "value() requires a copy and move constructible error type.");
        if (!has_value())
        {
            detail::exp::throw_bad_expected_access<E>(std::move(this->m_error));
        }
        return std::move(this->m_value);
    }
    /// @}

    /// Unchecked access to the error. The behavior is undefined if the expected holds a value.
    /// @{
    CETL_NODISCARD constexpr const E& error() const& noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_007: error() requires an error.");
        return this->m_error;
    }
    CETL_NODISCARD constexpr E& error() & noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_008: error() requires an error.");
        return this->m_error;
    }
    CETL_NODISCARD constexpr const E&& error() const&& noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_009: error() requires an error.");
        return std::move(this->m_error);
    }
    CETL_NODISCARD constexpr E&& error() && noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_010: error() requires an error.");
        return std::move(this->m_error);
    }
    /// @}

    /// The value if there is one, otherwise \p default_value converted to T.
    /// @{
    template <typename U = std::remove_cv_t<T>>
    CETL_NODISCARD constexpr T value_or(U&& default_value) const&
    {
        static_assert(std::is_copy_constructible<T>::value, "value_or() requires a copy constructible value type.");
        static_assert(detail::exp::is_convertible<U, T>::value, "value_or() requires an argument convertible to T.");
        return has_value() ? this->m_value : static_cast<T>(std::forward<U>(default_value));
    }
    template <typename U = std::remove_cv_t<T>>
    CETL_NODISCARD constexpr T value_or(U&& default_value) &&
    {
        static_assert(std::is_move_constructible<T>::value, "value_or() requires a move constructible value type.");
        static_assert(detail::exp::is_convertible<U, T>::value, "value_or() requires an argument convertible to T.");
        return has_value() ? std::move(this->m_value) : static_cast<T>(std::forward<U>(default_value));
    }
    /// @}

    /// The error if there is one, otherwise \p default_error converted to E.
    /// @{
    template <typename G = E>
    CETL_NODISCARD constexpr E error_or(G&& default_error) const&
    {
        static_assert(std::is_copy_constructible<E>::value, "error_or() requires a copy constructible error type.");
        static_assert(detail::exp::is_convertible<G, E>::value, "error_or() requires an argument convertible to E.");
        if (has_value())
        {
            return std::forward<G>(default_error);
        }
        return this->m_error;
    }
    template <typename G = E>
    CETL_NODISCARD constexpr E error_or(G&& default_error) &&
    {
        static_assert(std::is_move_constructible<E>::value, "error_or() requires a move constructible error type.");
        static_assert(detail::exp::is_convertible<G, E>::value, "error_or() requires an argument convertible to E.");
        if (has_value())
        {
            return std::forward<G>(default_error);
        }
        return std::move(this->m_error);
    }
    /// @}

    /// If there is a value, returns the result of invoking \p f with it, which must be an expected with the same error
    /// type; otherwise returns the error.
    /// @{
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&>>::value, int> = 0>
    constexpr auto and_then(F&& f) &
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, T&>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f), this->m_value);
        }
        return U(unexpect, this->m_error);
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&>>::value, int> = 0>
    constexpr auto and_then(F&& f) const&
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, const T&>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f), this->m_value);
        }
        return U(unexpect, this->m_error);
    }
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&&>>::value, int> = 0>
    constexpr auto and_then(F&& f) &&
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, T&&>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f), std::move(this->m_value));
        }
        return U(unexpect, std::move(this->m_error));
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&&>>::value, int> = 0>
    constexpr auto and_then(F&& f) const&&
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, const T&&>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f), std::move(this->m_value));
        }
        return U(unexpect, std::move(this->m_error));
    }
    /// @}

    /// If there is an error, returns the result of invoking \p f with it, which must be an expected with the same value
    /// type; otherwise returns the value.
    /// @{
    template <typename F, std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, T&>>::value, int> = 0>
    constexpr auto or_else(F&& f) &
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, E&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, T>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G(detail::exp::in_place_t{}, this->m_value);
        }
        return detail::exp::invoke(std::forward<F>(f), this->m_error);
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, const T&>>::value, int> = 0>
    constexpr auto or_else(F&& f) const&
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, const E&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, T>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G(detail::exp::in_place_t{}, this->m_value);
        }
        return detail::exp::invoke(std::forward<F>(f), this->m_error);
    }
    template <typename F, std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, T&&>>::value, int> = 0>
    constexpr auto or_else(F&& f) &&
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, E&&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, T>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G(detail::exp::in_place_t{}, std::move(this->m_value));
        }
        return detail::exp::invoke(std::forward<F>(f), std::move(this->m_error));
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, const T&&>>::value, int> = 0>
    constexpr auto or_else(F&& f) const&&
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, const E&&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, T>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G(detail::exp::in_place_t{}, std::move(this->m_value));
        }
        return detail::exp::invoke(std::forward<F>(f), std::move(this->m_error));
    }
    /// @}

    /// If there is a value, returns an expected holding the result of invoking \p f with it (or a value of void if
    /// \p f returns void); otherwise returns the error.
    /// @{
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&>>::value, int> = 0>
    constexpr auto transform(F&& f) &
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F, T&>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f), this->m_value);
        }
        return expected<U, E>(unexpect, this->m_error);
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&>>::value, int> = 0>
    constexpr auto transform(F&& f) const&
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F, const T&>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f), this->m_value);
        }
        return expected<U, E>(unexpect, this->m_error);
    }
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&&>>::value, int> = 0>
    constexpr auto transform(F&& f) &&
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F, T&&>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f), std::move(this->m_value));
        }
        return expected<U, E>(unexpect, std::move(this->m_error));
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&&>>::value, int> = 0>
    constexpr auto transform(F&& f) const&&
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F, const T&&>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f), std::move(this->m_value));
        }
        return expected<U, E>(unexpect, std::move(this->m_error));
    }
    /// @}

    /// If there is an error, returns an expected holding the result of invoking \p f with it; otherwise returns the
    /// value.
    /// @{
    template <typename F, std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, T&>>::value, int> = 0>
    constexpr auto transform_error(F&& f) &
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, E&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<T, G>(detail::exp::in_place_t{}, this->m_value);
        }
        return expected<T, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), this->m_error);
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, const T&>>::value, int> = 0>
    constexpr auto transform_error(F&& f) const&
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, const E&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<T, G>(detail::exp::in_place_t{}, this->m_value);
        }
        return expected<T, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), this->m_error);
    }
    template <typename F, std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, T&&>>::value, int> = 0>
    constexpr auto transform_error(F&& f) &&
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, E&&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<T, G>(detail::exp::in_place_t{}, std::move(this->m_value));
        }
        return expected<T, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), std::move(this->m_error));
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<T, detail::exp::dependent_t<F, const T&&>>::value, int> = 0>
    constexpr auto transform_error(F&& f) const&&
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, const E&&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<T, G>(detail::exp::in_place_t{}, std::move(this->m_value));
        }
        return expected<T, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), std::move(this->m_error));
    }
    /// @}

    /// Compares with another expected: equal if both hold equal values or both hold equal errors.
    /// @{
    template <typename T2, typename E2, std::enable_if_t<!std::is_void<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const T&>() == std::declval<const T2&>()),
                                                    decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator==(const expected& x, const expected<T2, E2>& y)
    {
        if (x.has_value() != y.has_value())
        {
            return false;
        }
        if (x.has_value())
        {
            return *x == *y;
        }
        return x.error() == y.error();
    }
    template <typename T2, typename E2, std::enable_if_t<!std::is_void<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const T&>() == std::declval<const T2&>()),
                                                    decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator!=(const expected& x, const expected<T2, E2>& y)
    {
        return !(x == y);
    }
    /// @}

    /// Compares with a value: equal if the expected holds a value equal to \p v.
    /// @{
    template <typename T2, std::enable_if_t<!detail::exp::is_expected<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const T&>() == std::declval<const T2&>())>
    operator==(const expected& x, const T2& v)
    {
        if (x.has_value())
        {
            return *x == v;
        }
        return false;
    }
    template <typename T2, std::enable_if_t<!detail::exp::is_expected<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const T&>() == std::declval<const T2&>())>
    operator==(const T2& v, const expected& x)
    {
        if (x.has_value())
        {
            return *x == v;
        }
        return false;
    }
    template <typename T2, std::enable_if_t<!detail::exp::is_expected<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const T&>() == std::declval<const T2&>())>
    operator!=(const expected& x, const T2& v)
    {
        return !(x == v);
    }
    template <typename T2, std::enable_if_t<!detail::exp::is_expected<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const T&>() == std::declval<const T2&>())>
    operator!=(const T2& v, const expected& x)
    {
        return !(x == v);
    }
    /// @}

    /// Compares with an \ref unexpected. They are equal if the expected holds an error equal to the one in \p e.
    /// @{
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator==(const expected& x, const unexpected<E2>& e)
    {
        if (!x.has_value())
        {
            return x.error() == e.error();
        }
        return false;
    }
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator==(const unexpected<E2>& e, const expected& x)
    {
        if (!x.has_value())
        {
            return x.error() == e.error();
        }
        return false;
    }
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator!=(const expected& x, const unexpected<E2>& e)
    {
        return !(x == e);
    }
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator!=(const unexpected<E2>& e, const expected& x)
    {
        return !(x == e);
    }
    /// @}

private:
    template <typename F, typename... Args>
    constexpr expected(const detail::exp::invoke_value_tag, F&& f, Args&&... args)
        : base(detail::exp::invoke_value_tag{}, std::forward<F>(f), std::forward<Args>(args)...)
    {
    }
    template <typename F, typename... Args>
    constexpr expected(const detail::exp::invoke_error_tag, F&& f, Args&&... args)
        : base(detail::exp::invoke_error_tag{}, std::forward<F>(f), std::forward<Args>(args)...)
    {
    }

    template <typename G>
    CETL_PF23_CONSTEXPR20 void assign_error(G&& error)
    {
        if (has_value())
        {
            detail::exp::reinit_expected(this->m_error, this->m_value, std::forward<G>(error));
            this->m_has_value = false;
        }
        else
        {
            this->m_error = std::forward<G>(error);
        }
    }

    CETL_PF23_CONSTEXPR20 void destroy_active() noexcept
    {
        if (has_value())
        {
            detail::exp::destroy_member(this->m_value);
        }
        else
        {
            detail::exp::destroy_member(this->m_error);
        }
    }

    /// Moves this value into other and other's error into this. E can be moved without throwing, so it is set aside
    /// first and put back if moving the value throws.
    CETL_PF23_CONSTEXPR20 void swap_value_with_error(expected& other, std::true_type)
    {
        E tmp(std::move(other.m_error));
        detail::exp::destroy_member(other.m_error);
#if defined(__cpp_exceptions)
        try
        {
            detail::exp::construct_member(other.m_value, std::move(this->m_value));
        } catch (...)
        {
            detail::exp::construct_member(other.m_error, std::move(tmp));
            throw;
        }
#else
        detail::exp::construct_member(other.m_value, std::move(this->m_value));
#endif
        detail::exp::destroy_member(this->m_value);
        detail::exp::construct_member(this->m_error, std::move(tmp));
    }
    /// As above, but only T can be moved without throwing, so the value is set aside first.
    CETL_PF23_CONSTEXPR20 void swap_value_with_error(expected& other, std::false_type)
    {
        T tmp(std::move(this->m_value));
        detail::exp::destroy_member(this->m_value);
#if defined(__cpp_exceptions)
        try
        {
            detail::exp::construct_member(this->m_error, std::move(other.m_error));
        } catch (...)
        {
            detail::exp::construct_member(this->m_value, std::move(tmp));
            throw;
        }
#else
        detail::exp::construct_member(this->m_error, std::move(other.m_error));
#endif
        detail::exp::destroy_member(other.m_error);
        detail::exp::construct_member(other.m_value, std::move(tmp));
    }
};

/// A strict implementation of C++23 \c std::expected<void, E>: either nothing (success) or an error of type E.
/// See the primary template for the differences from \c std::expected.
template <typename E>
class expected<void, E> : private detail::exp::base_move_assignment<detail::exp::void_value, E>
{
    template <typename, typename>
    friend class expected;

    using base = detail::exp::base_move_assignment<detail::exp::void_value, E>;

    static_assert(detail::exp::is_valid_error<E>, "E must be a valid template argument for unexpected.");

public:
    using value_type      = void;
    using error_type      = E;
    using unexpected_type = unexpected<E>;

    template <typename U>
    using rebind = expected<U, error_type>;

    /// Constructs an expected that holds a value.
    constexpr expected() noexcept
        : base(detail::exp::in_place_t{})
    {
    }

    /// Copy constructor. Deleted unless E is copy constructible; trivial if it is trivially so.
    constexpr expected(const expected&) = default;

    /// Move constructor. Trivial if E is trivially move constructible.
    constexpr expected(expected&&) noexcept(std::is_nothrow_move_constructible<E>::value) = default;

    /// Converts from another expected with a void value type.
    /// @{
    template <typename U,
              typename G,
              std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_void_converting_ctor<E, U, G, const G&>,
                                                       detail::exp::is_convertible<const G&, E>>::value,
                               int> = 0>
    CETL_PF23_CONSTEXPR20 expected(const expected<U, G>& other)  // NOLINT(*-explicit-constructor)
        : base(detail::exp::copy_tag{}, static_cast<const typename expected<U, G>::base&>(other))
    {
    }
    template <
        typename U,
        typename G,
        std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_void_converting_ctor<E, U, G, const G&>,
                                                 cetl::pf17::negation<detail::exp::is_convertible<const G&, E>>>::value,
                         int> = 0>
    CETL_PF23_CONSTEXPR20 explicit expected(const expected<U, G>& other)
        : base(detail::exp::copy_tag{}, static_cast<const typename expected<U, G>::base&>(other))
    {
    }
    template <typename U,
              typename G,
              std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_void_converting_ctor<E, U, G, G>,
                                                       detail::exp::is_convertible<G, E>>::value,
                               int> = 0>
    CETL_PF23_CONSTEXPR20 expected(expected<U, G>&& other)  // NOLINT(*-explicit-constructor)
        : base(detail::exp::copy_tag{}, static_cast<typename expected<U, G>::base&&>(other))
    {
    }
    template <typename U,
              typename G,
              std::enable_if_t<cetl::pf17::conjunction<detail::exp::enable_void_converting_ctor<E, U, G, G>,
                                                       cetl::pf17::negation<detail::exp::is_convertible<G, E>>>::value,
                               int> = 0>
    CETL_PF23_CONSTEXPR20 explicit expected(expected<U, G>&& other)
        : base(detail::exp::copy_tag{}, static_cast<typename expected<U, G>::base&&>(other))
    {
    }
    /// @}

    /// Constructs the error from an \ref unexpected.
    /// @{
    template <typename G,
              std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, const G&>,
                                                       detail::exp::is_convertible<const G&, E>>::value,
                               int> = 0>
    constexpr expected(const unexpected<G>& e)  // NOLINT(*-explicit-constructor)
        noexcept(std::is_nothrow_constructible<E, const G&>::value)
        : base(unexpect, e.error())
    {
    }
    template <
        typename G,
        std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, const G&>,
                                                 cetl::pf17::negation<detail::exp::is_convertible<const G&, E>>>::value,
                         int> = 0>
    constexpr explicit expected(const unexpected<G>& e) noexcept(std::is_nothrow_constructible<E, const G&>::value)
        : base(unexpect, e.error())
    {
    }
    template <
        typename G,
        std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, G>, detail::exp::is_convertible<G, E>>::value,
                         int> = 0>
    constexpr expected(unexpected<G>&& e)  // NOLINT(*-explicit-constructor)
        noexcept(std::is_nothrow_constructible<E, G>::value)
        : base(unexpect, std::move(e).error())
    {
    }
    template <typename G,
              std::enable_if_t<cetl::pf17::conjunction<std::is_constructible<E, G>,
                                                       cetl::pf17::negation<detail::exp::is_convertible<G, E>>>::value,
                               int> = 0>
    constexpr explicit expected(unexpected<G>&& e) noexcept(std::is_nothrow_constructible<E, G>::value)
        : base(unexpect, std::move(e).error())
    {
    }
    /// @}

    /// Constructs an expected that holds a value.
    constexpr explicit expected(const detail::exp::in_place_t) noexcept
        : base(detail::exp::in_place_t{})
    {
    }

    /// Constructs the error in place.
    /// @{
    template <typename... Args, std::enable_if_t<std::is_constructible<E, Args...>::value, int> = 0>
    constexpr explicit expected(const unexpect_t,
                                Args&&... args) noexcept(std::is_nothrow_constructible<E, Args...>::value)
        : base(unexpect, std::forward<Args>(args)...)
    {
    }
    template <typename U,
              typename... Args,
              std::enable_if_t<std::is_constructible<E, std::initializer_list<U>&, Args...>::value, int> = 0>
    constexpr explicit expected(const unexpect_t, std::initializer_list<U> il, Args&&... args) noexcept(
        std::is_nothrow_constructible<E, std::initializer_list<U>&, Args...>::value)
        : base(unexpect, il, std::forward<Args>(args)...)
    {
    }
    /// @}

    ~expected() = default;

    /// Copy assignment. Deleted unless E is copy constructible and copy assignable.
    expected& operator=(const expected&) = default;

    /// Move assignment. Deleted unless E is move constructible and move assignable.
    expected& operator=(expected&&) noexcept(std::is_nothrow_move_assignable<E>::value &&
                                             std::is_nothrow_move_constructible<E>::value) = default;

    /// Assigns an error.
    /// @{
    template <typename G,
              std::enable_if_t<detail::exp::enable_unexpected_assignment<detail::exp::void_value, E, const G&>::value,
                               int> = 0>
    CETL_PF23_CONSTEXPR20 expected& operator=(const unexpected<G>& e)
    {
        assign_error(e.error());
        return *this;
    }
    template <
        typename G,
        std::enable_if_t<detail::exp::enable_unexpected_assignment<detail::exp::void_value, E, G>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 expected& operator=(unexpected<G>&& e)
    {
        assign_error(std::move(e).error());
        return *this;
    }
    /// @}

    /// Destroys the error, if any, so that the expected holds a value.
    CETL_PF23_CONSTEXPR20 void emplace() noexcept
    {
        if (!has_value())
        {
            detail::exp::destroy_member(this->m_error);
            detail::exp::construct_member(this->m_value);
            this->m_has_value = true;
        }
    }

    /// Swaps the contents.
    template <typename Dependency = void,
              std::enable_if_t<detail::exp::enable_void_swap<detail::exp::dependent_t<Dependency, E>>::value, int> = 0>
    CETL_PF23_CONSTEXPR20 void swap(expected& other) noexcept(std::is_nothrow_move_constructible<E>::value &&
                                                              cetl::pf17::is_nothrow_swappable<E>::value)
    {
        if (has_value() && other.has_value())
        {
            // Nothing to swap.
        }
        else if (has_value())
        {
            detail::exp::construct_member(this->m_error, std::move(other.m_error));
            detail::exp::destroy_member(other.m_error);
            detail::exp::construct_member(other.m_value);
            this->m_has_value = false;
            other.m_has_value = true;
        }
        else if (other.has_value())
        {
            other.swap(*this);
        }
        else
        {
            using std::swap;
            swap(this->m_error, other.m_error);
        }
    }

    template <typename Dependency = void,
              std::enable_if_t<detail::exp::enable_void_swap<detail::exp::dependent_t<Dependency, E>>::value, int> = 0>
    friend CETL_PF23_CONSTEXPR20 void swap(expected& x, expected& y) noexcept(noexcept(x.swap(y)))
    {
        x.swap(y);
    }

    /// True if the expected holds a value.
    /// @{
    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }
    CETL_NODISCARD constexpr bool has_value() const noexcept
    {
        return this->m_has_value;
    }
    /// @}

    /// Does nothing. The behavior is undefined if the expected holds an error.
    constexpr void operator*() const noexcept
    {
        CETL_DEBUG_ASSERT(has_value(), "CDE_expected_011: operator* requires a value.");
    }

    /// Checked access to the value: does nothing if there is one.
    /// If the expected holds an error, the behavior depends on whether exception handling is enabled:
    /// - If exceptions are enabled, a bad_expected_access<E> holding a copy (or for rvalues a move) of the error is
    ///   thrown.
    /// - If exceptions are disabled, std::terminate is called.
    /// @{
    constexpr void value() const&
    {
        static_assert(std::is_copy_constructible<E>::value, "value() requires a copy constructible error type.");
        if (!has_value())
        {
            detail::exp::throw_bad_expected_access<E>(this->m_error);
        }
    }
    constexpr void value() &&
    {
        static_assert(std::is_copy_constructible<E>::value && std::is_move_constructible<E>::value,
                      "value() requires a copy and move constructible error type.");
        if (!has_value())
        {
            detail::exp::throw_bad_expected_access<E>(std::move(this->m_error));
        }
    }
    /// @}

    /// Unchecked access to the error. The behavior is undefined if the expected holds a value.
    /// @{
    CETL_NODISCARD constexpr const E& error() const& noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_012: error() requires an error.");
        return this->m_error;
    }
    CETL_NODISCARD constexpr E& error() & noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_013: error() requires an error.");
        return this->m_error;
    }
    CETL_NODISCARD constexpr const E&& error() const&& noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_014: error() requires an error.");
        return std::move(this->m_error);
    }
    CETL_NODISCARD constexpr E&& error() && noexcept
    {
        CETL_DEBUG_ASSERT(!has_value(), "CDE_expected_015: error() requires an error.");
        return std::move(this->m_error);
    }
    /// @}

    /// The error if there is one, otherwise \p default_error converted to E.
    /// @{
    template <typename G = E>
    CETL_NODISCARD constexpr E error_or(G&& default_error) const&
    {
        static_assert(std::is_copy_constructible<E>::value, "error_or() requires a copy constructible error type.");
        static_assert(detail::exp::is_convertible<G, E>::value, "error_or() requires an argument convertible to E.");
        if (has_value())
        {
            return std::forward<G>(default_error);
        }
        return this->m_error;
    }
    template <typename G = E>
    CETL_NODISCARD constexpr E error_or(G&& default_error) &&
    {
        static_assert(std::is_move_constructible<E>::value, "error_or() requires a move constructible error type.");
        static_assert(detail::exp::is_convertible<G, E>::value, "error_or() requires an argument convertible to E.");
        if (has_value())
        {
            return std::forward<G>(default_error);
        }
        return std::move(this->m_error);
    }
    /// @}

    /// If there is a value, returns the result of invoking \p f, which must be an expected with the same error type;
    /// otherwise returns the error.
    /// @{
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&>>::value, int> = 0>
    constexpr auto and_then(F&& f) &
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f));
        }
        return U(unexpect, this->m_error);
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&>>::value, int> = 0>
    constexpr auto and_then(F&& f) const&
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f));
        }
        return U(unexpect, this->m_error);
    }
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&&>>::value, int> = 0>
    constexpr auto and_then(F&& f) &&
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f));
        }
        return U(unexpect, std::move(this->m_error));
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&&>>::value, int> = 0>
    constexpr auto and_then(F&& f) const&&
    {
        using U = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F>>;
        static_assert(detail::exp::is_expected<U>::value, "and_then() requires f to return an expected.");
        static_assert(std::is_same<typename U::error_type, E>::value,
                      "and_then() requires f to return an expected with the same error type.");
        if (has_value())
        {
            return detail::exp::invoke(std::forward<F>(f));
        }
        return U(unexpect, std::move(this->m_error));
    }
    /// @}

    /// If there is an error, returns the result of invoking \p f with it, which must be an expected with a void value
    /// type; otherwise returns a value.
    /// @{
    template <typename F>
    constexpr auto or_else(F&& f) &
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, E&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, void>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G();
        }
        return detail::exp::invoke(std::forward<F>(f), this->m_error);
    }
    template <typename F>
    constexpr auto or_else(F&& f) const&
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, const E&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, void>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G();
        }
        return detail::exp::invoke(std::forward<F>(f), this->m_error);
    }
    template <typename F>
    constexpr auto or_else(F&& f) &&
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, E&&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, void>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G();
        }
        return detail::exp::invoke(std::forward<F>(f), std::move(this->m_error));
    }
    template <typename F>
    constexpr auto or_else(F&& f) const&&
    {
        using G = detail::exp::remove_cvref_t<detail::exp::invoke_result_t<F, const E&&>>;
        static_assert(detail::exp::is_expected<G>::value, "or_else() requires f to return an expected.");
        static_assert(std::is_same<typename G::value_type, void>::value,
                      "or_else() requires f to return an expected with the same value type.");
        if (has_value())
        {
            return G();
        }
        return detail::exp::invoke(std::forward<F>(f), std::move(this->m_error));
    }
    /// @}

    /// If there is a value, returns an expected holding the result of invoking \p f (or a value of void if \p f
    /// returns void); otherwise returns the error.
    /// @{
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&>>::value, int> = 0>
    constexpr auto transform(F&& f) &
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f));
        }
        return expected<U, E>(unexpect, this->m_error);
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&>>::value, int> = 0>
    constexpr auto transform(F&& f) const&
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f));
        }
        return expected<U, E>(unexpect, this->m_error);
    }
    template <typename F, std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, E&&>>::value, int> = 0>
    constexpr auto transform(F&& f) &&
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f));
        }
        return expected<U, E>(unexpect, std::move(this->m_error));
    }
    template <typename F,
              std::enable_if_t<std::is_constructible<E, detail::exp::dependent_t<F, const E&&>>::value, int> = 0>
    constexpr auto transform(F&& f) const&&
    {
        using U = std::remove_cv_t<detail::exp::invoke_result_t<F>>;

        static_assert(detail::exp::is_valid_value<U>, "transform() requires f to return a valid value type.");
        if (has_value())
        {
            return expected<U, E>(detail::exp::invoke_value_tag{}, std::forward<F>(f));
        }
        return expected<U, E>(unexpect, std::move(this->m_error));
    }
    /// @}

    /// If there is an error, returns an expected holding the result of invoking \p f with it; otherwise returns a
    /// value.
    /// @{
    template <typename F>
    constexpr auto transform_error(F&& f) &
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, E&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<void, G>();
        }
        return expected<void, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), this->m_error);
    }
    template <typename F>
    constexpr auto transform_error(F&& f) const&
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, const E&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<void, G>();
        }
        return expected<void, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), this->m_error);
    }
    template <typename F>
    constexpr auto transform_error(F&& f) &&
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, E&&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<void, G>();
        }
        return expected<void, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), std::move(this->m_error));
    }
    template <typename F>
    constexpr auto transform_error(F&& f) const&&
    {
        using G = std::remove_cv_t<detail::exp::invoke_result_t<F, const E&&>>;

        static_assert(detail::exp::is_valid_error<G>, "transform_error() requires f to return a valid error type.");
        if (has_value())
        {
            return expected<void, G>();
        }
        return expected<void, G>(detail::exp::invoke_error_tag{}, std::forward<F>(f), std::move(this->m_error));
    }
    /// @}

    /// Compares with another expected with a void value type: equal if both hold a value or both hold equal errors.
    /// @{
    template <typename T2, typename E2, std::enable_if_t<std::is_void<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator==(const expected& x, const expected<T2, E2>& y)
    {
        if (x.has_value() != y.has_value())
        {
            return false;
        }
        if (x.has_value())
        {
            return true;
        }
        return x.error() == y.error();
    }
    template <typename T2, typename E2, std::enable_if_t<std::is_void<T2>::value, int> = 0>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator!=(const expected& x, const expected<T2, E2>& y)
    {
        return !(x == y);
    }
    /// @}

    /// Compares with an \ref unexpected. They are equal if the expected holds an error equal to the one in \p e.
    /// @{
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator==(const expected& x, const unexpected<E2>& e)
    {
        if (!x.has_value())
        {
            return x.error() == e.error();
        }
        return false;
    }
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator==(const unexpected<E2>& e, const expected& x)
    {
        if (!x.has_value())
        {
            return x.error() == e.error();
        }
        return false;
    }
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator!=(const expected& x, const unexpected<E2>& e)
    {
        return !(x == e);
    }
    template <typename E2>
    friend constexpr detail::exp::enable_comparison<decltype(std::declval<const E&>() == std::declval<const E2&>())>
    operator!=(const unexpected<E2>& e, const expected& x)
    {
        return !(x == e);
    }
    /// @}

private:
    template <typename F, typename... Args>
    constexpr expected(const detail::exp::invoke_value_tag, F&& f, Args&&... args)
        : base(detail::exp::in_place_t{})
    {
        detail::exp::invoke(std::forward<F>(f), std::forward<Args>(args)...);
    }
    template <typename F, typename... Args>
    constexpr expected(const detail::exp::invoke_error_tag, F&& f, Args&&... args)
        : base(detail::exp::invoke_error_tag{}, std::forward<F>(f), std::forward<Args>(args)...)
    {
    }

    template <typename G>
    CETL_PF23_CONSTEXPR20 void assign_error(G&& error)
    {
        if (has_value())
        {
            detail::exp::reinit_expected(this->m_error, this->m_value, std::forward<G>(error));
            this->m_has_value = false;
        }
        else
        {
            this->m_error = std::forward<G>(error);
        }
    }
};

}  // namespace pf23
}  // namespace cetl

#undef CETL_PF23_CONSTEXPR20

#endif  // CETL_PF23_EXPECTED_HPP_INCLUDED
