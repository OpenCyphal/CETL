/// @file
/// Defines C++17 entities from the \c utility header.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_PF17_UTILITY_HPP_INCLUDED
#define CETL_PF17_UTILITY_HPP_INCLUDED

#include <cstddef>
#include <utility>
#include <type_traits>

namespace cetl
{
namespace pf17
{
/// Polyfill for std::in_place_t.
struct in_place_t
{
    explicit in_place_t() = default;
};

// --------------------------------------------------------------------------------------------

/// Polyfill for std::in_place.
constexpr in_place_t in_place{};

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::in_place_type_t.
template <typename T>
struct in_place_type_t
{
    explicit in_place_type_t() = default;
};

/// Implementation of \ref std::in_place_type.
template <typename T>
constexpr in_place_type_t<T> in_place_type{};

// --------------------------------------------------------------------------------------------

/// Implementation of \ref std::in_place_index_t.
template <std::size_t I>
struct in_place_index_t
{
    explicit in_place_index_t() = default;
};

/// Implementation of \ref std::in_place_index.
template <std::size_t I>
constexpr in_place_index_t<I> in_place_index{};

// --------------------------------------------------------------------------------------------

/// This is a helper for use with \ref visit that uses standard overload resolution to pick the best overload
/// among a set of lambdas given by the user. Unfortunately, in C++14 we have to use it with a factory function;
/// please see \ref make_overloaded for details. In C++17 this can be used without the factory in a much simpler way.
///
/// This function is not found in the C++ standard library, but is a common extension in the wild.
template <typename...>
struct overloaded;
template <typename T>
struct overloaded<T> : public T
{
    using T::operator();
    // SFINAE is needed to ensure this constructor does not hide the copy/move constructors.
    template <typename A, std::enable_if_t<!std::is_same<overloaded<T>, std::decay_t<A>>::value, int> = 0>
    constexpr explicit overloaded(A&& arg)
        : T(std::forward<A>(arg))
    {
    }
};
template <typename T, typename... Ts>
struct overloaded<T, Ts...> : public T, public overloaded<Ts...>
{
    using T::operator();
    using overloaded<Ts...>::operator();
    // If B were empty, the ctor would need sfinae to avoid hiding the copy/move ctors; ensure this is not so.
    template <typename A, typename... B, std::enable_if_t<(sizeof...(B) > 0), int> = 0>
    constexpr explicit overloaded(A&& a, B&&... b)
        : T(std::forward<A>(a))
        , overloaded<B...>(std::forward<B>(b)...)
    {
    }
};

/// Returns an instance of \ref overloaded that can be used with \ref visit. The usage is as follows:
/// @code
/// visit(make_overloaded(
///     [](const auto&)        { return "fallback"; },
///     [](double)             { return "double";   },
///     [](const std::string&) { return "string";   }
/// ), variant);
/// @endcode
template <typename... Ts>
constexpr overloaded<Ts...> make_overloaded(Ts&&... ts)
{
    return overloaded<Ts...>(std::forward<Ts>(ts)...);
}

// --------------------------------------------------------------------------------------------

// Non-standard extensions for internal use.
// We keep the definitions here to make it clear they are designed to detect the above-defined types, not std:: ones.
namespace detail
{
template <typename>
struct is_in_place_type_impl : std::false_type
{};
template <typename T>
struct is_in_place_type_impl<in_place_type_t<T>> : std::true_type
{};
/// A SFINAE helper for detecting \ref in_place_type_t.
template <typename T>
struct is_in_place_type : is_in_place_type_impl<std::decay_t<T>>
{};
static_assert(is_in_place_type<decltype(in_place_type<int>)>::value, "self-test failure");
static_assert(!is_in_place_type<decltype(in_place_index<0>)>::value, "self-test failure");

// --------------------------------------------------------------------------------------------

template <typename>
struct is_in_place_index_impl : std::false_type
{};
template <std::size_t I>
struct is_in_place_index_impl<in_place_index_t<I>> : std::true_type
{};
/// A SFINAE helper for detecting \ref in_place_index_t.
template <typename T>
struct is_in_place_index : is_in_place_index_impl<std::decay_t<T>>
{};
static_assert(is_in_place_index<decltype(in_place_index<0>)>::value, "self-test failure");
static_assert(!is_in_place_index<decltype(in_place_type<int>)>::value, "self-test failure");
}  // namespace detail

}  // namespace pf17
}  // namespace cetl

#endif  // CETL_PF17_UTILITY_HPP_INCLUDED
