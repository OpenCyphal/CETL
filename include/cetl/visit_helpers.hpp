/// @file
/// Contains helpers for building visitors for the `cetl::visit` invocation.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_VISIT_HELPERS_HPP_INCLUDED
#define CETL_VISIT_HELPERS_HPP_INCLUDED

#include <type_traits>
#include <utility>

namespace cetl
{
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

}  // namespace cetl

#endif  // CETL_VISIT_HELPERS_HPP_INCLUDED
