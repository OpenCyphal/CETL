/// @file
/// Type traits extensions not found in C++ standards. This is mostly needed for internal use in the library,
/// but can also be useful for users.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_TYPE_TRAITS_EXTENSIONS_HPP_INCLUDED
#define CETL_TYPE_TRAITS_EXTENSIONS_HPP_INCLUDED

#include <algorithm>
#include <array>
#include <utility>
#include <limits>
#include <tuple>
#include <type_traits>
#include <cetl/pf17/type_traits.hpp>

namespace cetl
{
namespace type_traits_ext
{
using pf17::void_t;

/// `value` is true for any parameter.
template <typename...>
struct universal_predicate : std::true_type
{};

// --------------------------------------------------------------------------------------------

namespace detail
{
// The predicate is an unary function but we declare it as variadic for composability with generic tools
// like partial application. A variadic argument accepts an unary template, but not the other way around.
template <template <typename...> class, std::size_t, typename...>
struct find_impl;
template <template <typename...> class Predicate, std::size_t Ix>
struct find_impl<Predicate, Ix>
{
    static constexpr std::size_t first_match_index = std::numeric_limits<std::size_t>::max();
    static constexpr std::size_t match_count       = 0;
};
template <template <typename...> class Predicate, std::size_t Ix, typename Head, typename... Tail>
struct find_impl<Predicate, Ix, Head, Tail...> : find_impl<Predicate, Ix + 1, Tail...>
{
    using base                                     = find_impl<Predicate, Ix + 1, Tail...>;
    static constexpr std::size_t first_match_index = (Predicate<Head>::value ? Ix : base::first_match_index);
    static constexpr std::size_t match_count       = (Predicate<Head>::value ? 1 : 0) + base::match_count;
};
}  // namespace detail

/// The \c value member contains the index of the first type T in the typelist for which Predicate<T>::value is true,
/// or `std::numeric_limits<std::size_t>::max()` if no such type exists.
template <template <typename...> class Predicate, typename... Ts>
struct find : std::integral_constant<std::size_t, detail::find_impl<Predicate, 0, Ts...>::first_match_index>
{};

/// Alias for \c find.
/// /code
/// static_assert(find_v<std::is_integral, int, char, double, std::int64_t, std::int16_t, std::int8_t> == 0, "");
/// static_assert(find_v<std::is_integral, double, float, std::int64_t, std::int16_t, std::int8_t> == 2, "");
/// static_assert(find_v<std::is_integral, double, float> == std::numeric_limits<std::size_t>::max(), "");
/// /endcode
template <template <typename...> class Predicate, typename... Ts>
static constexpr std::size_t find_v = find<Predicate, Ts...>::value;

/// The \c value member contains the number of types in the typelist for which Predicate<T>::value is true.
template <template <typename...> class Predicate, typename... Ts>
struct count : std::integral_constant<std::size_t, detail::find_impl<Predicate, 0, Ts...>::match_count>
{};

/// Alias for \c count.
/// \code
/// static_assert(count_v<std::is_integral, int, char, double, std::int64_t, std::int16_t, std::int8_t> == 5, "");
/// static_assert(count_v<std::is_integral, double, float, std::int64_t, std::int16_t, std::int8_t> == 3, "");
/// static_assert(count_v<std::is_integral, double, float> == 0, "");
/// \endcode
template <template <typename...> class Predicate, typename... Ts>
static constexpr std::size_t count_v = count<Predicate, Ts...>::value;

// --------------------------------------------------------------------------------------------

/// Partially instantiate a template by holding the Left type arguments fixed.
/// The resulting template is accessible via the member template alias named \c type.
/// For correct usage don't forget to prefix the \c type member with \c template; e.g.:
/// @code
/// template <typename T, typename... Ts>
/// constexpr std::size_t first_index_of_v =
///     type_traits_ext::find_v<type_traits_ext::partial<std::is_same, T>::template type, Ts...>;
/// @endcode
/// Does not work with non-type template parameters.
template <template <typename...> class F, typename... Left>
struct partial
{
    template <typename... Right>
    using type = F<Left..., Right...>;
};

// --------------------------------------------------------------------------------------------

/// The `value` is true if the following expression is well-formed:
/// @code
/// To x[] = { std::forward<From>(from) };
/// @endcode
template <typename From, typename To, typename = void>
struct is_convertible_without_narrowing : std::false_type
{};
template <typename From, typename To>
struct is_convertible_without_narrowing<
    From,
    To,
    // The number of braces is of an essential importance here: {{ }} attempts to initialize the first element
    // of the array with the value, while {{{ }}} performs direct-list-initialization of To.
    // Incorrect usage of the braces will cause incorrect detection of the applicable conversion.
    // Notice that there is a subtle difference between C++14 and the newer standards with the guaranteed copy
    // elision: a double-brace conversion is invalid in C++14 for noncopyable types while in C++17+ it is valid.
    //
    // An alternative way to test the conversion is to define a function that accepts an array rvalue:
    //  static void test_conversion(To (&&)[1]);
    // And check if it is invocable with the argument of type From.
    void_t<decltype(std::array<To, 1>{{{std::declval<From>()}}})>> : std::true_type
{};
static_assert(is_convertible_without_narrowing<int, long long>::value, "");
static_assert(!is_convertible_without_narrowing<long long, int>::value, "");

// --------------------------------------------------------------------------------------------

namespace detail
{
namespace best_conversion_index
{
template <typename...>
class types;

// In C++14 we have to go the hard way while in C++17+ we could have used a single type with multiple inheritance from
// the conversion case class with the using-declaration pack expansion. Sadly the latter is not available in C++14.
// The traditional replacement for using-declaration pack expansion is chained inheritance, but here it is not
// applicable because it affects the overload resolution order --- the overload brought into scope with the
// using-declaration gets a lower priority, which affects the result. To work around this, we do not define overload
// alternatives directly in-scope, but instead pull all of the alternatives from different scopes, two at a time. This
// forces the compiler to give each overload an equal priority during overload resolution.
//
// This utility was originally created for the variant class where narrowing conversions are not acceptable.
// The required behavior is defined as follows:
//     An overload F(T_i) is only considered if the declaration T_i x[] = { std::forward<T>(t) };
//     is valid for some invented variable x;
//
// Originally, this utility used to define the match function argument as (T (&&)[1]) to achieve the desired behavior,
// because it would force direct-list-initialization at invocation, which weeds out narrowing conversions.
// However, this didn't work as expected because the compiler will not eliminate overloads that require narrowing
// conversions until after the overload is resolved, meaning that if there are two overloads where one doesn't require
// narrowing and the other does but is otherwise equally good, the compiler will report that the call is
// not well-formed due to ambiguity. I'm not sure I understand why it works this way, though.
//
// The solution was to remove the narrowing check from this utility and instead providing a separate trait
// \ref is_convertible_without_narrowing that can be used in the predicate. A positive side effect of this change is
// that the utility is now more general and can be used for other purposes as well.
template <template <typename...> class Q, std::size_t N, typename T>
struct candidate
{
    template <typename DependentT = T, std::enable_if_t<Q<DependentT>::value, int> = 0>
    static std::integral_constant<std::size_t, N> match(T&&);
};
template <template <typename...> class Q, std::size_t N, typename... Ts>
struct resolver : resolver<Q, N - 1, Ts...>, candidate<Q, N, std::tuple_element_t<N, std::tuple<Ts...>>>
{
    using candidate<Q, N, std::tuple_element_t<N, std::tuple<Ts...>>>::match;
    using resolver<Q, N - 1, Ts...>::match;
};
template <template <typename...> class Q, typename... Ts>
struct resolver<Q, 0, Ts...> : candidate<Q, 0, std::tuple_element_t<0, std::tuple<Ts...>>>
{
    using candidate<Q, 0, std::tuple_element_t<0, std::tuple<Ts...>>>::match;
};

// This wrapper is used to fallback the index to size_t(-1) if no conversion is possible.
// I considered doing that the easier way via an ellipsis overload of `match(...)` but that overload
// can sometimes take precedence over some valid conversion sequences; i.e.,
// `match(...)` is a better match than `match(T&&)` for some U where there is a poorly ranked but valid conversion
// sequence from U to T.
template <template <typename...> class, typename, typename, typename = void>
struct impl : std::integral_constant<std::size_t, std::numeric_limits<std::size_t>::max()>
{};
template <template <typename...> class Q, typename F, typename... Ts>
struct impl<Q, F, types<Ts...>, void_t<decltype(resolver<Q, sizeof...(Ts) - 1U, Ts...>::match(std::declval<F>()))>>
    : decltype(resolver<Q, sizeof...(Ts) - 1U, Ts...>::match(std::declval<F>()))
{};
}  // namespace best_conversion_index
}  // namespace detail

/// Index of the type in the \c Tos list selected by overload resolution for the expression F(std::forward<T>(t))
/// if there was an overload of imaginary function F(T_i) for every T_i from Tos... in scope at the same time.
///
/// The Predicate is an unary template that, upon successful instantiation with a single argument being a type
/// from the Tos typelist, contains a static member `value` whose value is truth if the conversion is a valid
/// candidate for acceptance and false if the conversion should not be considered even if it is otherwise valid.
/// The predicate is defined as a variadic template for enhanced composability, as a variadic template template
/// argument can accept both variadic and non-variadic parameters, even if it is only instantiated with one
/// parameter.
///
/// Hint: to weed out narrowing conversions, use \ref is_convertible_without_narrowing in the predicate.
///
/// If no suitable conversion is available, the value is std::numeric_limits<std::size_t>::max().
///
/// \code
/// best_conversion_index_v<universal_predicate, long, float> == 0
/// best_conversion_index_v<universal_predicate, float, long, float, double, bool> == 1
/// best_conversion_index_v<universal_predicate, int, long, float, bool> == ambiguity
///
/// best_conversion_index_v<std::is_signed, long, char, long, unsigned long> == 1
/// best_conversion_index_v<std::is_unsigned, long, char, long, unsigned long> == 2
/// best_conversion_index_v<std::is_volatile, char, int, const int, volatile int> == 2
///
/// best_conversion_index_v<partial<is_convertible_without_narrowing, int>::template type, int, float, bool, long> == 2
///
/// struct foo { foo(bool); };
/// best_conversion_index_v<universal_predicate, char, foo, int> == 1
/// best_conversion_index_v<universal_predicate, char, foo> == 0
///
/// best_conversion_index_v<partial<is_convertible_without_narrowing, char>::template type, char, foo, int> == 1
/// best_conversion_index_v<partial<is_convertible_without_narrowing, char>::template type, char, foo> == unavailable
/// best_conversion_index_v<partial<is_convertible_without_narrowing, bool>::template type, bool, foo> == 0
/// \endcode
template <template <typename...> class Predicate, typename From, typename... Tos>
constexpr std::size_t best_conversion_index_v =
    detail::best_conversion_index::impl<Predicate, From, detail::best_conversion_index::types<Tos...>>::value;

}  // namespace type_traits_ext
}  // namespace cetl

#endif  // CETL_TYPE_TRAITS_EXTENSIONS_HPP_INCLUDED
