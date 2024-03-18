/// @file
/// Unit tests for cetl/pf17/type_traits.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/type_traits.hpp>

struct noncopyable
{
    noncopyable()                              = default;
    noncopyable(const noncopyable&)            = delete;
    noncopyable(noncopyable&&)                 = delete;
    noncopyable& operator=(const noncopyable&) = delete;
    noncopyable& operator=(noncopyable&&)      = delete;
    ~noncopyable()                             = default;
};
struct nothrow_swappable
{
    nothrow_swappable()                                        = default;
    nothrow_swappable(const nothrow_swappable&)                = default;
    nothrow_swappable(nothrow_swappable&&) noexcept            = default;
    nothrow_swappable& operator=(const nothrow_swappable&)     = default;
    nothrow_swappable& operator=(nothrow_swappable&&) noexcept = default;
    ~nothrow_swappable()                                       = default;
};
struct throw_swappable
{
    throw_swappable()                       = default;
    throw_swappable(const throw_swappable&) = default;
    throw_swappable(throw_swappable&&) {}  // NOLINT(*-noexcept-move-constructor)
    throw_swappable& operator=(const throw_swappable&) = default;
    throw_swappable& operator=(throw_swappable&&)
    {
        return *this;
    }
    ~throw_swappable() = default;
};

namespace test_is_swappable
{
using namespace cetl::pf17;

static_assert(is_swappable_v<int>, "");
static_assert(!is_swappable_v<noncopyable>, "");
static_assert(is_swappable_v<nothrow_swappable>, "");
static_assert(is_swappable_v<throw_swappable>, "");

static_assert(is_nothrow_swappable_v<int>, "");
static_assert(!is_nothrow_swappable_v<noncopyable>, "");
static_assert(is_nothrow_swappable_v<nothrow_swappable>, "");
static_assert(!is_nothrow_swappable_v<throw_swappable>, "");
}  // namespace test_is_swappable

namespace test_logical
{
using namespace cetl::pf17;

static_assert(conjunction_v<> == true, "");
static_assert(conjunction_v<std::true_type> == true, "");
static_assert(conjunction_v<std::false_type> == false, "");
static_assert(conjunction_v<std::true_type, std::true_type> == true, "");
static_assert(conjunction_v<std::true_type, std::false_type> == false, "");
static_assert(conjunction_v<std::false_type, std::true_type> == false, "");
static_assert(conjunction_v<std::false_type, std::false_type> == false, "");

static_assert(disjunction_v<> == false, "");
static_assert(disjunction_v<std::true_type> == true, "");
static_assert(disjunction_v<std::false_type> == false, "");
static_assert(disjunction_v<std::true_type, std::true_type> == true, "");
static_assert(disjunction_v<std::true_type, std::false_type> == true, "");
static_assert(disjunction_v<std::false_type, std::true_type> == true, "");
static_assert(disjunction_v<std::false_type, std::false_type> == false, "");

static_assert(negation_v<std::true_type> == false, "");
static_assert(negation_v<std::false_type> == true, "");
}  // namespace test_logical
