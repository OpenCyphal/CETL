/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_OPTIONAL_HPP_INCLUDED
#define CETL_PF17_OPTIONAL_HPP_INCLUDED

#include <cetl/_helpers.hpp>
#include <cetl/pf17/utility.hpp>
#include <cetl/pf17/attribute.hpp>

#include <algorithm>
#include <cassert>
#include <type_traits>
#include <exception>  // We need this even if exceptions are disabled for std::terminate.

namespace cetl
{
namespace pf17
{
/// A polyfill for std::optional.
template <typename T>
class optional;

/// A polyfill for std::nullopt_t.
struct nullopt_t final
{
    enum class _detail
    {
        _detail  ///< The secret tag is needed to meet the behavioral requirements.
    };

    constexpr explicit nullopt_t(const _detail) noexcept {}
};

/// A polyfill for std::nullopt.
constexpr nullopt_t nullopt{nullopt_t::_detail::_detail};

#if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)
/// A polyfill for std::bad_optional_access.
/// This is only available if exceptions are enabled.
class bad_optional_access : public std::exception
{
public:
    bad_optional_access() noexcept                                      = default;
    bad_optional_access(const bad_optional_access&) noexcept            = default;
    bad_optional_access(bad_optional_access&&) noexcept                 = default;
    bad_optional_access& operator=(const bad_optional_access&) noexcept = default;
    bad_optional_access& operator=(bad_optional_access&&) noexcept      = default;
    ~bad_optional_access() noexcept override                            = default;

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "bad_optional_access";
    }
};
#endif

namespace detail
{
namespace opt
{
struct copy_tag final
{};

/// DESTRUCTION POLICY
template <typename T, bool = std::is_trivially_destructible<T>::value>
struct base_destruction;
/// Trivially destructible case.
template <typename T>
struct base_destruction<T, true>
{
    constexpr base_destruction() noexcept
        : m_nihil{}
        , m_engaged{false}
    {
    }
    template <typename... Args>
    constexpr explicit base_destruction(const in_place_t, Args&&... args)
        : m_value(std::forward<Args>(args)...)
        , m_engaged(true)
    {
    }
    template <typename U>
    constexpr explicit base_destruction(const copy_tag, U&& other)
        : m_engaged{other.m_engaged}
    {
        if (m_engaged)
        {
            new (std::addressof(m_value)) T(std::forward<U>(other).m_value);
        }
    }
    void reset() noexcept
    {
        this->m_engaged = false;
    }
    union
    {
        in_place_t m_nihil;
        T          m_value;
    };
    bool m_engaged;
};
/// Non-trivially destructible case.
template <typename T>
struct base_destruction<T, false>
{
    constexpr base_destruction() noexcept
        : m_nihil{}
        , m_engaged{false}
    {
    }
    template <typename... Args>
    constexpr explicit base_destruction(const in_place_t, Args&&... args)
        : m_value(std::forward<Args>(args)...)
        , m_engaged(true)
    {
    }
    template <typename U>
    constexpr explicit base_destruction(const copy_tag, U&& other)
        : m_engaged{other.m_engaged}
    {
        if (m_engaged)
        {
            new (std::addressof(m_value)) T(std::forward<U>(other).m_value);
        }
    }
    constexpr base_destruction(const base_destruction&) noexcept            = default;
    constexpr base_destruction(base_destruction&&) noexcept                 = default;
    constexpr base_destruction& operator=(const base_destruction&) noexcept = default;
    constexpr base_destruction& operator=(base_destruction&&) noexcept      = default;
    ~base_destruction() noexcept
    {
        reset();
    }
    void reset() noexcept
    {
        if (this->m_engaged)
        {
            this->m_value.~T();
            this->m_engaged = false;
        }
    }
    union
    {
        in_place_t m_nihil;
        T          m_value;
    };
    bool m_engaged;
};

/// COPY CONSTRUCTION POLICY
template <typename T, bool = std::is_trivially_copy_constructible<T>::value>
struct base_copy_construction;
/// Trivially copy constructible case.
template <typename T>
struct base_copy_construction<T, true> : base_destruction<T>
{
    using base_destruction<T>::base_destruction;
};
/// Non-trivially copy constructible case.
template <typename T>
struct base_copy_construction<T, false> : base_destruction<T>
{
    using base_destruction<T>::base_destruction;
    constexpr base_copy_construction() noexcept = default;
    constexpr base_copy_construction(const base_copy_construction& other) noexcept(
        std::is_nothrow_copy_constructible<T>::value)
        : base_destruction<T>(copy_tag{}, other)
    {
    }
    constexpr base_copy_construction(base_copy_construction&&) noexcept                 = default;
    constexpr base_copy_construction& operator=(const base_copy_construction&) noexcept = default;
    constexpr base_copy_construction& operator=(base_copy_construction&&) noexcept      = default;
    ~base_copy_construction() noexcept                                                  = default;
};

/// MOVE CONSTRUCTION POLICY
template <typename T, bool = std::is_trivially_move_constructible<T>::value>
struct base_move_construction;
/// Trivially move constructible case.
template <typename T>
struct base_move_construction<T, true> : base_copy_construction<T>
{
    using base_copy_construction<T>::base_copy_construction;
};
/// Non-trivially move constructible case.
template <typename T>
struct base_move_construction<T, false> : base_copy_construction<T>
{
    using base_copy_construction<T>::base_copy_construction;
    constexpr base_move_construction() noexcept                              = default;
    constexpr base_move_construction(const base_move_construction&) noexcept = default;
    constexpr base_move_construction(base_move_construction&& other) noexcept(
        std::is_nothrow_move_constructible<T>::value)
        : base_copy_construction<T>(copy_tag{}, std::move(other))
    {
    }
    constexpr base_move_construction& operator=(const base_move_construction&) noexcept = default;
    constexpr base_move_construction& operator=(base_move_construction&&) noexcept      = default;
    ~base_move_construction() noexcept                                                  = default;
};

/// COPY ASSIGNMENT POLICY
template <typename T,
          bool = std::is_trivially_copy_assignable<T>::value &&     //
                 std::is_trivially_copy_constructible<T>::value &&  //
                 std::is_trivially_destructible<T>::value>
struct base_copy_assignment;
/// Trivially copy assignable case.
template <typename T>
struct base_copy_assignment<T, true> : base_move_construction<T>
{
    using base_move_construction<T>::base_move_construction;
};
/// Non-trivially copy assignable case.
template <typename T>
struct base_copy_assignment<T, false> : base_move_construction<T>
{
    using base_move_construction<T>::base_move_construction;
    constexpr base_copy_assignment() noexcept                            = default;
    constexpr base_copy_assignment(const base_copy_assignment&) noexcept = default;
    constexpr base_copy_assignment(base_copy_assignment&&) noexcept      = default;
    constexpr base_copy_assignment& operator=(const base_copy_assignment& other) noexcept(
        std::is_nothrow_copy_assignable<T>::value && std::is_nothrow_copy_constructible<T>::value)
    {
        if (this->m_engaged && other.m_engaged)
        {
            this->m_value = other.m_value;
        }
        else if (other.m_engaged)
        {
            new (std::addressof(this->m_value)) T(other.m_value);
            this->m_engaged = true;
        }
        else
        {
            this->reset();
        }
        return *this;
    }
    constexpr base_copy_assignment& operator=(base_copy_assignment&&) noexcept = default;
    ~base_copy_assignment() noexcept                                           = default;
};

/// MOVE ASSIGNMENT POLICY
template <typename T,
          bool = std::is_trivially_move_assignable<T>::value &&     //
                 std::is_trivially_move_constructible<T>::value &&  //
                 std::is_trivially_destructible<T>::value>
struct base_move_assignment;
/// Trivially move assignable case.
template <typename T>
struct base_move_assignment<T, true> : base_copy_assignment<T>
{
    using base_copy_assignment<T>::base_copy_assignment;
};
/// Non-trivially move assignable case.
template <typename T>
struct base_move_assignment<T, false> : base_copy_assignment<T>
{
    using base_copy_assignment<T>::base_copy_assignment;
    constexpr base_move_assignment() noexcept                                       = default;
    constexpr base_move_assignment(const base_move_assignment&) noexcept            = default;
    constexpr base_move_assignment(base_move_assignment&&) noexcept                 = default;
    constexpr base_move_assignment& operator=(const base_move_assignment&) noexcept = default;
    constexpr base_move_assignment& operator=(base_move_assignment&& other) noexcept(
        std::is_nothrow_move_assignable<T>::value && std::is_nothrow_move_constructible<T>::value)
    {
        if (this->m_engaged && other.m_engaged)
        {
            this->m_value = std::move(other.m_value);
        }
        else if (other.m_engaged)
        {
            new (std::addressof(this->m_value)) T(std::move(other.m_value));
            this->m_engaged = true;
        }
        else
        {
            this->reset();
        }
        return *this;
    }
    ~base_move_assignment() noexcept = default;
};

/// True if T is constructible or convertible from const F& or F&&.
/// CAVEAT: in C++14, std::is_convertible<F, T> is not true if T is not copyable, even if F is convertible to T,
/// so we use std::is_convertible<F, T&&> instead.
/// The specification of std::optional<> prescribes the use of T over T&& because it is written for C++17.
template <typename T, typename F>
constexpr bool convertible = std::is_constructible<T, F&>::value ||         //
                             std::is_constructible<T, const F&>::value ||   //
                             std::is_constructible<T, F&&>::value ||        //
                             std::is_constructible<T, const F&&>::value ||  //
                             std::is_convertible<F&, T&&>::value ||         //
                             std::is_convertible<const F&, T&&>::value ||   //
                             std::is_convertible<F&&, T&&>::value ||        //
                             std::is_convertible<const F&&, T&&>::value;
/// True if T is assignable from const F& or F&&.
template <typename T, typename F>
constexpr bool assignable = std::is_assignable<T, F&>::value ||        //
                            std::is_assignable<T, const F&>::value ||  //
                            std::is_assignable<T, F&&>::value ||       //
                            std::is_assignable<T, const F&&>::value;
/// True if T is a specialization of optional.
template <typename T>
struct is_optional : std::false_type
{};
template <typename T>
struct is_optional<optional<T>> : std::true_type
{};

/// https://en.cppreference.com/w/cpp/utility/optional/optional
/// CAVEAT: in C++14, std::is_convertible<F, T> is not true if T is not copyable, even if F is convertible to T,
/// so we use std::is_convertible<F, T&&> instead.
/// The specification of std::optional<> prescribes the use of T over T&& because it is written for C++17.
template <typename T, typename U, bool Explicit>
constexpr bool enable_ctor4 = (!std::is_same<bool, std::decay_t<T>>::value) &&            //
                              std::is_constructible<T, const U&>::value &&                //
                              (std::is_convertible<const U&, T&&>::value != Explicit) &&  //
                              !convertible<T, optional<U>>;
template <typename T, typename U, bool Explicit>
constexpr bool enable_ctor5 = (!std::is_same<bool, std::decay_t<T>>::value) &&       //
                              std::is_constructible<T, U&&>::value &&                //
                              (std::is_convertible<U&&, T&&>::value != Explicit) &&  //
                              !convertible<T, optional<U>>;
template <typename T, typename U, bool Explicit>
constexpr bool enable_ctor8 =
    std::is_constructible<T, U&&>::value &&                                                    //
    (!std::is_same<std::decay_t<U>, in_place_t>::value) &&                                     //
    (!std::is_same<std::decay_t<U>, optional<T>>::value) &&                                    //
    (!(std::is_same<std::decay_t<T>, bool>::value && is_optional<std::decay_t<U>>::value)) &&  //
    (std::is_convertible<U&&, T&&>::value != Explicit);

}  // namespace opt
}  // namespace detail

template <typename T>
class optional : private detail::opt::base_move_assignment<T>,
                 private cetl::detail::enable_copy_move_construction<std::is_copy_constructible<T>::value,
                                                                     std::is_move_constructible<T>::value>,
                 private cetl::detail::enable_copy_move_assignment<
                     std::is_copy_constructible<T>::value && std::is_copy_assignable<T>::value,
                     std::is_move_constructible<T>::value && std::is_move_assignable<T>::value>
{
    template <typename>
    friend class optional;
    template <typename, bool>
    friend struct detail::opt::base_destruction;

    using base = detail::opt::base_move_assignment<T>;

    static_assert(!std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, in_place_t>::value, "");
    static_assert(!std::is_same<std::remove_cv_t<std::remove_reference_t<T>>, nullopt_t>::value, "");
    static_assert(!std::is_reference<T>::value, "");
    static_assert(!std::is_array<T>::value, "");

public:
    using value_type = T;

    /// Destroy the value if engaged, otherwise do nothing.
    using base::reset;

    /// Constructor 1
    constexpr optional() noexcept = default;
    constexpr optional(const nullopt_t) noexcept {}  // NOLINT(*-explicit-constructor)

    /// Constructor 2
    constexpr optional(const optional&) noexcept = default;

    /// Constructor 3
    constexpr optional(optional&&) noexcept = default;

    /// Constructor 4
    template <typename U, std::enable_if_t<detail::opt::enable_ctor4<T, U, false>, int> = 0>
    optional(const optional<U>& other)  // NOLINT(*-explicit-constructor)
        : base(detail::opt::copy_tag{}, other)
    {
    }
    template <typename U, std::enable_if_t<detail::opt::enable_ctor4<T, U, true>, int> = 0>
    explicit optional(const optional<U>& other)
        : base(detail::opt::copy_tag{}, other)
    {
    }

    /// Constructor 5
    template <typename U, std::enable_if_t<detail::opt::enable_ctor5<T, U, false>, int> = 0>
    optional(optional<U>&& other)  // NOLINT(*-explicit-constructor)
        : base(detail::opt::copy_tag{}, std::move(other))
    {
    }
    template <typename U, std::enable_if_t<detail::opt::enable_ctor5<T, U, true>, int> = 0>
    explicit optional(optional<U>&& other)
        : base(detail::opt::copy_tag{}, std::move(other))
    {
    }

    /// Constructor 6
    template <typename... Args>
    explicit constexpr optional(const in_place_t, Args&&... args)  //
        noexcept(std::is_nothrow_constructible<T, Args...>::value)
        : base(in_place, std::forward<Args>(args)...)
    {
    }

    /// Constructor 7
    template <typename U, typename... Args>
    explicit constexpr optional(const in_place_t, std::initializer_list<U> il, Args&&... args)  //
        noexcept(std::is_nothrow_constructible<T, std::initializer_list<U>, Args...>::value)
        : base(in_place, il, std::forward<Args>(args)...)
    {
    }

    /// Constructor 8
    template <typename U = T, std::enable_if_t<detail::opt::enable_ctor8<T, U, false>, int> = 0>
    constexpr optional(U&& value)  // NOLINT(*-explicit-constructor)
        noexcept(std::is_nothrow_constructible<T, U>::value)
        : base(in_place, std::forward<U>(value))
    {
    }
    template <typename U = T, std::enable_if_t<detail::opt::enable_ctor8<T, U, true>, int> = 0>
    explicit constexpr optional(U&& value) noexcept(std::is_nothrow_constructible<T, U>::value)
        : base(in_place, std::forward<U>(value))
    {
    }

    ~optional() noexcept = default;

    /// Assignment 1
    optional& operator=(const nullopt_t) noexcept
    {
        reset();
        return *this;
    }

    /// Assignment 2
    optional& operator=(const optional& other) = default;

    /// Assignment 3
    constexpr optional& operator=(optional&& other) noexcept(std::is_nothrow_move_assignable<T>::value &&
                                                             std::is_nothrow_move_constructible<T>::value) = default;

    /// Assignment 4
    template <typename U                                                                                       = T,
              std::enable_if_t<!std::is_same<std::decay_t<U>, optional>::value, int>                           = 0,
              std::enable_if_t<std::is_constructible<T, U>::value, int>                                        = 0,
              std::enable_if_t<std::is_assignable<T&, U>::value, int>                                          = 0,
              std::enable_if_t<(!std::is_scalar<T>::value) || (!std::is_same<std::decay_t<U>, T>::value), int> = 0>
    optional& operator=(U&& value)
    {
        if (this->m_engaged)
        {
            this->m_value = std::forward<U>(value);
        }
        else
        {
            new (std::addressof(this->m_value)) T(std::forward<U>(value));
            this->m_engaged = true;
        }
        return *this;
    }

    /// Assignment 5
    template <typename U,
              std::enable_if_t<!detail::opt::convertible<T, optional<U>>, int> = 0,
              std::enable_if_t<!detail::opt::assignable<T&, optional<U>>, int> = 0,
              std::enable_if_t<std::is_constructible<T, const U&>::value, int> = 0,
              std::enable_if_t<std::is_assignable<T&, const U&>::value, int>   = 0>
    optional& operator=(const optional<U>& other)
    {
        if (this->m_engaged && other.m_engaged)
        {
            this->m_value = other.m_value;
        }
        else if (other.m_engaged)
        {
            new (std::addressof(this->m_value)) T(other.m_value);
            this->m_engaged = true;
        }
        else
        {
            this->reset();
        }
        return *this;
    }

    /// Assignment 6
    template <typename U,
              std::enable_if_t<!detail::opt::convertible<T, optional<U>>, int> = 0,
              std::enable_if_t<!detail::opt::assignable<T&, optional<U>>, int> = 0,
              std::enable_if_t<std::is_constructible<T, U>::value, int>        = 0,
              std::enable_if_t<std::is_assignable<T&, U>::value, int>          = 0>
    optional& operator=(optional<U>&& other)
    {
        if (this->m_engaged && other.m_engaged)
        {
            this->m_value = std::move(other.m_value);
        }
        else if (other.m_engaged)
        {
            new (std::addressof(this->m_value)) T(std::move(other.m_value));
            this->m_engaged = true;
        }
        else
        {
            this->reset();
        }
        return *this;
    }

    /// True if the optional is engaged.
    CETL_NODISCARD constexpr bool has_value() const noexcept
    {
        return this->m_engaged;
    }

    /// Construct the value in-place. If the optional is already engaged, the
    /// value is destroyed and replaced with the new value.
    template <typename... Args>
    T& emplace(Args&&... args) noexcept(std::is_nothrow_constructible<T, Args...>::value)
    {
        reset();
        new (std::addressof(this->m_value)) T(std::forward<Args>(args)...);
        this->m_engaged = true;
        return this->m_value;
    }
    template <typename U, typename... Args>
    T& emplace(std::initializer_list<U> il, Args&&... args)  //
        noexcept(std::is_nothrow_constructible<T, std::initializer_list<U>, Args...>::value)
    {
        reset();
        new (std::addressof(this->m_value)) T(il, std::forward<Args>(args)...);
        this->m_engaged = true;
        return this->m_value;
    }

    /// Swaps two optionals. If either is not engaged, acts like move assignment.
    void swap(optional& other) noexcept(std::is_nothrow_move_constructible<T>::value &&  //
                                        std::is_nothrow_swappable<T>::value)
    {
        using std::swap;
        if (has_value() && other.has_value())
        {
            swap(**this, *other);
        }
        else if (has_value())
        {
            other.emplace(std::move(**this));
            reset();
        }
        else if (other.has_value())
        {
            emplace(std::move(*other));
            other.reset();
        }
        else
        {
            // Neither is engaged, do nothing.
        }
    }

    /// Checked access to the value.
    /// If the optional is not engaged, the behavior depends on whether exception handling is enabled:
    /// - If exceptions are enabled, a bad_optional_access exception is thrown.
    /// - If exceptions are disabled, the behavior is undefined.
    CETL_NODISCARD constexpr T& value() &
    {
        ensure_engaged();
        return this->m_value;
    }
    CETL_NODISCARD constexpr const T& value() const&
    {
        ensure_engaged();
        return this->m_value;
    }
    CETL_NODISCARD constexpr T&& value() &&
    {
        ensure_engaged();
        return std::move(this->m_value);
    }
    CETL_NODISCARD constexpr const T&& value() const&&
    {
        ensure_engaged();
        return std::move(this->m_value);
    }

    /// Checked access to the value with a fallback value.
    /// If the optional is engaged, a copy of its value is returned; otherwise, the default value is
    /// converted to T and the result is returned.
    template <class U>
    CETL_NODISCARD constexpr T value_or(U&& default_value) const&
    {
        return has_value() ? **this : static_cast<T>(std::forward<U>(default_value));
    }
    template <class U>
    CETL_NODISCARD constexpr T value_or(U&& default_value) &&
    {
        return has_value() ? std::move(**this) : static_cast<T>(std::forward<U>(default_value));
    }

    /// Alias of has_value().
    constexpr explicit operator bool() const noexcept
    {
        return has_value();
    }

    /// The arrow operator provides unchecked access to the value.
    /// Behavior undefined if the optional is not engaged.
    constexpr T* operator->() noexcept
    {
        return &this->m_value;
    }
    constexpr const T* operator->() const noexcept
    {
        return &this->m_value;
    }

    /// The dereference operator provides unchecked access to the value.
    /// Behavior undefined if the optional is not engaged.
    constexpr T& operator*() & noexcept
    {
        return this->m_value;
    }
    constexpr const T& operator*() const& noexcept
    {
        return this->m_value;
    }
    constexpr T&& operator*() && noexcept
    {
        return std::move(this->m_value);
    }
    constexpr const T&& operator*() const&& noexcept
    {
        return std::move(this->m_value);
    }

private:
    void ensure_engaged() const
    {
        if (!this->m_engaged)
        {
#if defined(__cpp_exceptions)
            throw bad_optional_access{};
#else
            std::terminate();
#endif
        }
    }
};

/// Compares two optional objects, lhs and rhs.
/// The contained values are compared (using the corresponding operator of T) only if both lhs and rhs contain values.
/// Otherwise,
/// - lhs is considered equal to rhs if, and only if, both lhs and rhs do not contain a value.
/// - lhs is considered less than rhs if, and only if, rhs contains a value and lhs does not.
/// @{
template <typename T, typename U>
constexpr bool operator==(const optional<T>& lhs, const optional<U>& rhs)
{
    return (lhs.has_value() == rhs.has_value()) && ((!lhs.has_value()) || ((*lhs) == (*rhs)));
}
template <typename T, typename U>
constexpr bool operator!=(const optional<T>& lhs, const optional<U>& rhs)
{
    return !(lhs == rhs);
}
template <typename T, typename U>
constexpr bool operator<(const optional<T>& lhs, const optional<U>& rhs)
{
    return rhs.has_value() && ((!lhs.has_value()) || (*lhs) < (*rhs));
}
template <typename T, typename U>
constexpr bool operator<=(const optional<T>& lhs, const optional<U>& rhs)
{
    return !(rhs < lhs);
}
template <typename T, typename U>
constexpr bool operator>(const optional<T>& lhs, const optional<U>& rhs)
{
    return rhs < lhs;
}
template <typename T, typename U>
constexpr bool operator>=(const optional<T>& lhs, const optional<U>& rhs)
{
    return !(lhs < rhs);
}
/// @}

/// Compares an optional with a nullopt.
/// Equivalent to the above case when comparing to an optional that does not contain a value.
/// @{
template <typename T>
constexpr bool operator==(const optional<T>& opt, const nullopt_t) noexcept
{
    return !opt.has_value();
}
template <typename T>
constexpr bool operator==(const nullopt_t, const optional<T>& opt) noexcept
{
    return !opt.has_value();
}
template <typename T>
constexpr bool operator!=(const optional<T>& opt, const nullopt_t) noexcept
{
    return opt.has_value();
}
template <typename T>
constexpr bool operator!=(const nullopt_t, const optional<T>& opt) noexcept
{
    return opt.has_value();
}
template <typename T>
constexpr bool operator<(const optional<T>&, const nullopt_t) noexcept
{
    return false;
}
template <typename T>
constexpr bool operator<(const nullopt_t, const optional<T>& opt) noexcept
{
    return opt.has_value();
}
template <typename T>
constexpr bool operator<=(const optional<T>& opt, const nullopt_t) noexcept
{
    return !opt.has_value();
}
template <typename T>
constexpr bool operator<=(const nullopt_t, const optional<T>&) noexcept
{
    return true;
}
template <typename T>
constexpr bool operator>(const optional<T>& opt, const nullopt_t) noexcept
{
    return opt.has_value();
}
template <typename T>
constexpr bool operator>(const nullopt_t, const optional<T>&) noexcept
{
    return false;
}
template <typename T>
constexpr bool operator>=(const optional<T>&, const nullopt_t) noexcept
{
    return true;
}
template <typename T>
constexpr bool operator>=(const nullopt_t, const optional<T>& opt) noexcept
{
    return !opt.has_value();
}
/// @}

/// Compares an optional with a value.
/// The values are compared (using the corresponding operator of T) only if the optional contains a value.
/// Otherwise, the optional is considered less than value.
/// @{
template <typename T, typename U>
constexpr bool operator==(const optional<T>& opt, const U& value)
{
    return opt.has_value() && ((*opt) == value);
}
template <typename T, typename U>
constexpr bool operator==(const T& value, const optional<U>& opt)
{
    return opt.has_value() && (value == (*opt));
}
template <typename T, typename U>
constexpr bool operator!=(const optional<T>& opt, const U& value)
{
    return (!opt.has_value()) || ((*opt) != value);
}
template <typename T, typename U>
constexpr bool operator!=(const T& value, const optional<U>& opt)
{
    return (!opt.has_value()) || ((*opt) != value);
}
template <typename T, typename U>
constexpr bool operator<(const optional<T>& opt, const U& value)
{
    return (!opt.has_value()) || ((*opt) < value);
}
template <typename T, typename U>
constexpr bool operator<(const T& value, const optional<U>& opt)
{
    return opt.has_value() && (value < (*opt));
}
template <typename T, typename U>
constexpr bool operator<=(const optional<T>& opt, const U& value)
{
    return (!opt.has_value()) || ((*opt) <= value);
}
template <typename T, typename U>
constexpr bool operator<=(const T& value, const optional<U>& opt)
{
    return opt.has_value() && (value <= (*opt));
}
template <typename T, typename U>
constexpr bool operator>(const optional<T>& opt, const U& value)
{
    return opt.has_value() && ((*opt) > value);
}
template <typename T, typename U>
constexpr bool operator>(const T& value, const optional<U>& opt)
{
    return (!opt.has_value()) || (value > (*opt));
}
template <typename T, typename U>
constexpr bool operator>=(const optional<T>& opt, const U& value)
{
    return opt.has_value() && ((*opt) >= value);
}
template <typename T, typename U>
constexpr bool operator>=(const T& value, const optional<U>& opt)
{
    return (!opt.has_value()) || (value >= (*opt));
}
/// @}

/// Polyfill for std::make_optional.
/// @{
template <typename T>
constexpr optional<std::decay_t<T>> make_optional(T&& value)
{
    return optional<std::decay_t<T>>{std::forward<T>(value)};
}
template <typename T, typename... Args>
constexpr optional<T> make_optional(Args&&... args)
{
    return optional<T>{in_place, std::forward<Args>(args)...};
}
template <typename T, typename U, typename... Args>
constexpr optional<T> make_optional(std::initializer_list<U> il, Args&&... args)
{
    return optional<T>{in_place, il, std::forward<Args>(args)...};
}
/// @}

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_OPTIONAL_HPP_INCLUDED
