/// @file
/// CETL VerificAtion SuiTe – Test suite helpers.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETLVAST_TYPLIST_HPP_INCLUDED
#define CETLVAST_TYPLIST_HPP_INCLUDED

namespace cetlvast
{
namespace typelist
{
namespace _impl_cat
{
template <typename...>
struct cat;
template <template <typename...> class Q, typename... A>
struct cat<Q<A...>>
{
    using type = Q<A...>;
};
template <template <typename...> class QA, template <typename...> class QB, typename... A, typename... B>
struct cat<QA<A...>, QB<B...>>
{
    using type = QA<A..., B...>;
};
template <typename A, typename B, typename... C>
struct cat<A, B, C...>
{
    using type = typename cat<typename cat<A, B>::type, C...>::type;
};
}  // namespace _impl_cat

// ------------------------------------------------------------------------------------------------

/// TYPELIST CONCATENATOR: (list(A...), list(B...), list...) -> list(A..., B..., ...)
/// Accepts an arbitrary number of typelists and concatenates them into a single typelist.
/// The container type is determined by the first typelist.
template <typename... Ts>
using cat = typename _impl_cat::cat<Ts...>::type;

// ------------------------------------------------------------------------------------------------

namespace _impl_cartesian_product
{
/// A pair type used for constructing temporary typelists while computing the cartesian product.
template <typename, typename>
struct cons final
{};

/// Flattens an hierarchy of cons types into a single typelist contained inside Q (e.g., std::tuple).
template <template <typename...> class Q>
struct flatten
{
    template <typename...>
    struct impl;
    template <>
    struct impl<>
    {
        using type = Q<>;
    };
    template <typename T, typename... X>
    struct impl<T, X...>
    {
        using type = Q<T>;
    };
    template <typename L, typename R, typename... Ts>
    struct impl<cons<L, R>, Ts...>
    {
        using type = cat<typename impl<L>::type,  //
                         typename impl<R>::type,
                         typename impl<Ts...>::type>;
    };
};

/// Applies the flatten operation to each element of a typelist.
/// The result container has the same type as the input container.
template <typename...>
struct flatten_each;
template <template <typename...> class Q, typename... Ts>
struct flatten_each<Q<Ts...>>
{
    using type = Q<typename flatten<Q>::template impl<Ts>::type...>;
};

/// The actual cartesian product computation is implemented here, but the result is contained in a cons hierarchy.
/// The final result is obtained by applying the flatten_each operation to the intermediate result.
template <typename...>
struct product;
template <template <typename...> class QA>
struct product<QA<>>
{
    using type = QA<>;
};
template <template <typename...> class QA, template <typename...> class QB, typename... B>
struct product<QA<>, QB<B...>>
{
    using type = QA<>;
};
template <template <typename...> class QA,
          template <typename...>
          class QB,
          typename Head,
          typename... Tail,
          typename... B>
struct product<QA<Head, Tail...>, QB<B...>>
{
    using type = cat<QA<cons<Head, B>...>,  //
                     typename product<QA<Tail...>, QB<B...>>::type>;
};
template <typename A, typename B, typename... C>
struct product<A, B, C...>
{
    using type = typename product<typename product<A, B>::type, C...>::type;
};

/// The final result is computed here.
template <typename...>
struct cartesian_product;
template <template <typename...> class Q, typename... A, typename... B>
struct cartesian_product<Q<A...>, B...>
{
    using type = flatten_each<typename product<Q<A...>, B...>::type>::type;
};
}  // namespace _impl_cartesian_product

// ------------------------------------------------------------------------------------------------

/// CARTESIAN PRODUCT OF TYPELISTS
/// Accepts an arbitrary number of typelists and returns a typelist containing the cartesian product of the input
/// typelists. The container type is determined by the first typelist.
/// An empty set of arguments is not allowed as it makes container type deduction undecidable.
template <typename... Ts>
using cartesian_product = typename _impl_cartesian_product::cartesian_product<Ts...>::type;

// ------------------------------------------------------------------------------------------------

/// Creates a new typelist with the same types but a different container type.
template <template <typename...> class NewContainer>
class into
{
    template <typename...>
    struct impl;
    template <template <typename...> class OldContainer, typename... Ts>
    struct impl<OldContainer<Ts...>>
    {
        using type = NewContainer<Ts...>;
    };

public:
    template <typename... Ts>
    using from = typename impl<Ts...>::type;
};

}  // namespace typelist
}  // namespace cetlvast

#endif  // CETLVAST_TYPLIST_HPP_INCLUDED
