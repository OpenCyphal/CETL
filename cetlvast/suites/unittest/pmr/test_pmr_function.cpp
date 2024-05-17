/// @file
/// Unit tests for cetl/pmr/function.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pmr/function.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <iostream>

namespace
{

using cetl::pmr::function;

using testing::Not;

class TestPmrFunction : public testing::Test
{
protected:
    static void print_num(int i)
    {
        std::cout << i << '\n';
    }
};

TEST_F(TestPmrFunction, cpp_reference)
{
    struct Foo
    {
        Foo(int num)
            : num_{num}
        {
        }
        void print_add(int i) const
        {
            std::cout << num_ + i << '\n';
        }
        int num_;
    };
    struct PrintNum
    {
        void operator()(int i) const
        {
            std::cout << i << '\n';
        }
    };

    // store a free function
    function<void(int), 16> f_display = print_num;
    f_display(-9);

    // store a lambda
    function<void(), 16> f_display_42 = []() { print_num(42); };
    f_display_42();

    // store the result of a call to std::bind
    function<void(), 24> f_display_31337 = std::bind(print_num, 31337);
    f_display_31337();

    // store a call to a member function
    //function<void(const Foo&, int), 24> f_add_display = &Foo::print_add;
    const Foo                           foo(314159);
    //f_add_display(foo, 1);
    //f_add_display(314159, 1);

    // store a call to a data member accessor
    //function<int(Foo const&), 0> f_num = &Foo::num_;
    //EXPECT_THAT(f_num(foo), 314159);

    // store a call to a member function and object
    using std::placeholders::_1;
    function<void(int), 64> f_add_display2 = std::bind(&Foo::print_add, foo, _1);
    f_add_display2(2);
/*
    // store a call to a member function and object ptr
    function<void(int), 0> f_add_display3 = std::bind(&Foo::print_add, &foo, _1);
    f_add_display3(3);

    // store a call to a function object
    function<void(int), 16> f_display_obj = PrintNum();
    f_display_obj(18);

    auto factorial = [](int n)
    {
        // store a lambda object to emulate "recursive lambda"; aware of extra overhead
        function<int(int), 16> fac = [&](int _n) { return (_n < 2) ? 1 : _n * fac(_n - 1); };
        return fac(n);
    };
    for (int i{5}; i != 8; ++i)
    {
        std::cout << i << "! = " << factorial(i) << ";  ";
    }
    std::cout << '\n';
*/
}

TEST_F(TestPmrFunction, ctor_1_default)
{
    function<void(), 0> f1;
    EXPECT_THAT(!f1, Not(false));
    EXPECT_THAT(static_cast<bool>(f1), false);
}

TEST_F(TestPmrFunction, ctor_2_nullptr)
{
    function<void(), 0> f1{nullptr};
    EXPECT_THAT(!f1, Not(false));
    EXPECT_THAT(static_cast<bool>(f1), false);
}

}  // namespace