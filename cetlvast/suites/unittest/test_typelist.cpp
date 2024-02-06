/// @file
/// Unit tests for cetl::pf17::pmr::polymorphic_allocator defined in memory_resource.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetlvast/typelist.hpp>
#include <type_traits>
#include <tuple>

struct A;
struct B;
struct C;
struct D;
struct E;
struct F;

using std::tuple;

template <typename...>
struct my_typelist
{};

/// cat
namespace
{
using cetlvast::typelist::cat;
static_assert(std::is_same<tuple<>, cat<tuple<>>>::value, "");
static_assert(std::is_same<tuple<A>, cat<tuple<A>, tuple<>>>::value, "");
static_assert(std::is_same<tuple<A, B, C, A, D, B>,  //
                           cat<tuple<A, B, C, A>, tuple<D, B>>>::value,
              "");
static_assert(std::is_same<tuple<A, B, C, A, D, B>,  //
                           cat<tuple<A, B>,          //
                               tuple<C, A>,
                               tuple<D, B>>>::value,
              "");

static_assert(std::is_same<tuple<A, B, C, A, D, B, E, F>,  //
                           cat<tuple<A, B>,                //
                               tuple<C, A>,
                               tuple<D, B>,
                               tuple<E, F>>>::value,
              "");
static_assert(std::is_same<tuple<tuple<A, B>, tuple<C, A>, D, B, E, F>,  //
                           cat<tuple<tuple<A, B>,                        //
                                     tuple<C, A>>,
                               tuple<D, B>,
                               my_typelist<E, F>,
                               tuple<>>>::value,
              "");
}  // namespace

/// cartesian_product
namespace
{
using cetlvast::typelist::_impl_cartesian_product::cons;

// flatten
using cetlvast::typelist::_impl_cartesian_product::flatten;
static_assert(std::is_same<tuple<>,  //
                           flatten<tuple>::template impl<>::type>::value,
              "");
static_assert(std::is_same<tuple<A, B>,  //
                           flatten<tuple>::template impl<cons<A, B>>::type>::value,
              "");
static_assert(std::is_same<tuple<A, B, C, D>,  //
                           flatten<tuple>::template impl<cons<cons<A, B>, cons<C, D>>>::type>::value,
              "");
static_assert(std::is_same<tuple<A, B, C>,  //
                           flatten<tuple>::template impl<cons<cons<A, B>, C>>::type>::value,
              "");
static_assert(std::is_same<tuple<A, B, C>,  //
                           flatten<tuple>::template impl<cons<A, cons<B, C>>>::type>::value,
              "");
static_assert(std::is_same<tuple<A, B, C, D>,  //
                           flatten<tuple>::template impl<cons<cons<cons<A, B>, C>, D>>::type>::value,
              "");
static_assert(std::is_same<tuple<A, B, C, D>,  //
                           flatten<tuple>::template impl<cons<A, cons<B, cons<C, D>>>>::type>::value,
              "");

// flatten_each
using cetlvast::typelist::_impl_cartesian_product::flatten_each;
static_assert(std::is_same<tuple<tuple<A, B, C>, tuple<C, D>>,  //
                           flatten_each<tuple<cons<A, cons<B, C>>, cons<C, D>>>::type>::value,
              "");

// cartesian_product
using cetlvast::typelist::cartesian_product;
static_assert(std::is_same<tuple<>,  //
                           cartesian_product<tuple<>>>::value,
              "");
static_assert(std::is_same<tuple<>,  //
                           cartesian_product<tuple<>, tuple<>>>::value,
              "");
static_assert(std::is_same<tuple<>,  //
                           cartesian_product<tuple<>, tuple<B>>>::value,
              "");
static_assert(std::is_same<tuple<>,  //
                           cartesian_product<tuple<A>, tuple<>>>::value,
              "");
static_assert(std::is_same<tuple<tuple<A, B>>,  //
                           cartesian_product<tuple<A>, tuple<B>>>::value,
              "");
static_assert(std::is_same<tuple<tuple<A, B>,  //
                                 tuple<A, C>,
                                 tuple<A, D>>,
                           cartesian_product<tuple<A>,  //
                                             tuple<B, C, D>>>::value,
              "");
static_assert(std::is_same<tuple<tuple<A, C>,  //
                                 tuple<A, D>,
                                 tuple<A, E>,
                                 tuple<B, C>,
                                 tuple<B, D>,
                                 tuple<B, E>>,
                           cartesian_product<tuple<A, B>,  //
                                             tuple<C, D, E>>>::value,
              "");
static_assert(std::is_same<tuple<tuple<A, C, E>,  //
                                 tuple<A, C, F>,
                                 tuple<A, D, E>,
                                 tuple<A, D, F>,
                                 tuple<B, C, E>,
                                 tuple<B, C, F>,
                                 tuple<B, D, E>,
                                 tuple<B, D, F>>,
                           cartesian_product<tuple<A, B>,  //
                                             my_typelist<C, D>,
                                             tuple<E, F>>>::value,
              "");
}  // namespace

/// into
namespace
{
using cetlvast::typelist::into;
static_assert(std::is_same<tuple<>, into<tuple>::from<my_typelist<>>>::value, "");
static_assert(std::is_same<tuple<>, into<tuple>::from<tuple<>>>::value, "");
static_assert(std::is_same<tuple<A, B, tuple<C>>, into<tuple>::from<my_typelist<A, B, tuple<C>>>>::value, "");
static_assert(std::is_same<my_typelist<A, B, tuple<C>>, into<my_typelist>::from<tuple<A, B, tuple<C>>>>::value, "");
}  // namespace
