/// @file
/// Unit tests for cetl/pf17/new.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/new.hpp>
#include <cetl/pf17/cetlpf.hpp>

#include <gtest/gtest.h>

#include <new>
#include <type_traits>

namespace
{

struct constant
{
    explicit constant(const int v)
        : value{v}
    {
    }
    const int value;
};

static_assert(std::is_same<decltype(cetl::pf17::launder(std::declval<int*>())), int*>::value, "");
static_assert(std::is_same<decltype(cetl::pf17::launder(std::declval<const int*>())), const int*>::value, "");
static_assert(std::is_same<decltype(cetl::pf17::launder(std::declval<volatile int*>())), volatile int*>::value, "");
static_assert(noexcept(cetl::pf17::launder(std::declval<int*>())), "");

TEST(test_pf17_new, launder_returns_the_same_address)
{
    int value = 7;
    EXPECT_EQ(&value, cetl::pf17::launder(&value));
    EXPECT_EQ(&value, cetl::launder(&value));
}

TEST(test_pf17_new, launder_reaches_object_replaced_in_storage)
{
    alignas(constant) unsigned char storage[sizeof(constant)];
    const void* const               raw = storage;

    const constant* const first = new (storage) constant{1};
    EXPECT_EQ(1, cetl::pf17::launder(static_cast<const constant*>(raw))->value);

    // A constant has a const member, so a pointer to the first object cannot be used to reach the second one;
    // since C++17 that takes std::launder.
    first->~constant();
    new (storage) constant{2};
    const constant* const second = cetl::pf17::launder(first);
    EXPECT_EQ(2, second->value);
    EXPECT_EQ(2, cetl::launder(static_cast<const constant*>(raw))->value);

    second->~constant();
}

}  // namespace
