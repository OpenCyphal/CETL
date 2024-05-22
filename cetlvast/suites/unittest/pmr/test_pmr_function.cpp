/// @file
/// Unit tests for cetl/pmr/function.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/cetlpf.hpp>
#include <cetl/pmr/function.hpp>
#include <cetlvast/tracking_memory_resource.hpp>

#include <gmock/gmock.h>
#include <gtest/gtest.h>
#include <string>

namespace
{

using cetl::pmr::function;

using testing::Not;
using testing::IsEmpty;

class TestPmrFunction : public testing::Test
{
protected:
    void SetUp() override {}

    void TearDown() override
    {
        EXPECT_THAT(mr_.allocations, IsEmpty());
        EXPECT_THAT(mr_.total_allocated_bytes, mr_.total_deallocated_bytes);

        cetl::pmr::set_default_resource(nullptr);
    }

    static std::string print_num(int i)
    {
        return testing::PrintToString(i);
    }

    cetl::pmr::memory_resource* get_mr() noexcept
    {
        return &mr_;
    }

    TrackingMemoryResource mr_;
};

TEST_F(TestPmrFunction, cpp_reference)
{
    struct Foo
    {
        Foo(int num)
            : num_{num}
        {
        }
        std::string print_add(int i) const
        {
            return print_num(num_ + i);
        }
        int num_;
    };
    struct PrintNum
    {
        std::string operator()(int i) const
        {
            return print_num(i);
        }
    };

    // store a free function
    function<std::string(int), 16> f_display = print_num;
    EXPECT_THAT(f_display(-9), "-9");

    // store a lambda
    function<std::string(), 16> f_display_42 = []() { return print_num(42); };
    EXPECT_THAT(f_display_42(), "42");

    // store the result of a call to std::bind
    function<std::string(), 24> f_display_31337 = std::bind(print_num, 31337);
    EXPECT_THAT(f_display_31337(), "31337");

    // store a call to a member function
    // function<void(const Foo&, int), 24> f_add_display = &Foo::print_add;
    const Foo foo(314159);
    // f_add_display(foo, 1);
    // f_add_display(314159, 1);

    // store a call to a data member accessor
    // function<int(Foo const&), 0> f_num = &Foo::num_;
    // EXPECT_THAT(f_num(foo), 314159);

    // store a call to a member function and object
    using std::placeholders::_1;
    function<std::string(int), 64> f_add_display2 = std::bind(&Foo::print_add, foo, _1);
    f_add_display2(2);

    // store a call to a member function and object ptr
    function<std::string(int), 32> f_add_display3 = std::bind(&Foo::print_add, &foo, _1);
    f_add_display3(3);

    // store a call to a function object
    function<std::string(int), 16> f_display_obj = PrintNum();
    EXPECT_THAT(f_display_obj(18), "18");

    auto factorial = [](int n) {
        // store a lambda object to emulate "recursive lambda"; aware of extra overhead
        function<int(int), 16> fac = [&](int _n) { return (_n < 2) ? 1 : _n * fac(_n - 1); };
        return fac(n);
    };
    EXPECT_THAT(factorial(1), 1);
    EXPECT_THAT(factorial(2), 1 * 2);
    EXPECT_THAT(factorial(3), 1 * 2 * 3);
    EXPECT_THAT(factorial(4), 1 * 2 * 3 * 4);
    EXPECT_THAT(factorial(5), 1 * 2 * 3 * 4 * 5);
    EXPECT_THAT(factorial(6), 1 * 2 * 3 * 4 * 5 * 6);
    EXPECT_THAT(factorial(7), 1 * 2 * 3 * 4 * 5 * 6 * 7);
}

TEST_F(TestPmrFunction, ctor_1_default)
{
    const function<void(), 0> f1{};
    EXPECT_THAT(!f1, Not(false));
    EXPECT_THAT(static_cast<bool>(f1), false);
}

TEST_F(TestPmrFunction, ctor_2_nullptr)
{
    const function<void(), 0> f1{nullptr};
    EXPECT_THAT(!f1, Not(false));
    EXPECT_THAT(static_cast<bool>(f1), false);
}

TEST_F(TestPmrFunction, ctor_3_copy)
{
    using str_function = function<std::string(), 24>;

    const str_function fn{std::bind(print_num, 123)};

    str_function fn_copy{fn};
    EXPECT_THAT(!!fn_copy, true);
    EXPECT_THAT(static_cast<bool>(fn_copy), true);
    EXPECT_THAT(fn_copy(), "123");

    fn_copy = {};
    const str_function fn_empty{fn_copy};
    EXPECT_THAT(!!fn_empty, false);
}

TEST_F(TestPmrFunction, ctor_4_move)
{
    using str_function = function<std::string(), 24>;

    str_function fn{[]() { return print_num(123); }};

    str_function fn_moved{std::move(fn)};
    EXPECT_THAT(!!fn_moved, true);
    EXPECT_THAT(static_cast<bool>(fn_moved), true);
    EXPECT_THAT(fn_moved(), "123");

    fn_moved = {};
    const str_function fn_empty{std::move(fn_moved)};
    EXPECT_THAT(!!fn_empty, false);
}

TEST_F(TestPmrFunction, ctor_5_functor_lambda)
{
    using str_function = function<std::string(), 24>;

    str_function fn{[]() { return print_num(123); }};
    EXPECT_THAT(!!fn, true);
    EXPECT_THAT(fn(), "123");
}

TEST_F(TestPmrFunction, assign_1_copy)
{
    using str_function = function<std::string(), 24>;

    const str_function fn1{[]() { return print_num(123); }};
    EXPECT_THAT(!!fn1, true);

    const str_function fn2 = fn1;
    EXPECT_THAT(!!fn1, true);
    EXPECT_THAT(!!fn2, true);
    EXPECT_THAT(fn2(), "123");
}

TEST_F(TestPmrFunction, assign_2_move)
{
    using str_function = function<std::string(), 24>;

    str_function fn1{[]() { return print_num(123); }};
    EXPECT_THAT(!!fn1, true);

    const str_function fn2 = std::move(fn1);
    EXPECT_THAT(!!fn1, false);
    EXPECT_THAT(!!fn2, true);
    EXPECT_THAT(fn2(), "123");
}

TEST_F(TestPmrFunction, assign_3_nullptr)
{
    using str_function = function<std::string(), 24>;
    str_function fn{[]() { return print_num(123); }};
    EXPECT_THAT(!!fn, true);

    fn = nullptr;
    EXPECT_THAT(!!fn, false);
}

TEST_F(TestPmrFunction, assign_4_rv_functor)
{
    function<std::string(const std::string&), 24> f1;
    f1 = [](const std::string& rhs) { return "A" + rhs; };
    EXPECT_THAT(f1("x"), "Ax");

    function<std::string(const std::string&), 96> f2;
    f2 = [f1](const std::string& rhs) { return f1(rhs) + "B"; };
    EXPECT_THAT(f2("x"), "AxB");

    // Note: we move assign different type of function (Footprint and IsPmr).
    function<std::string(const std::string&), 0, true /*IsPmr*/> f0{[](const std::string&) { return "123"; }};
    f2 = std::move(f0);
    EXPECT_THAT(!!f0, false);
    EXPECT_THAT(f2("x"), "123");
}

TEST_F(TestPmrFunction, assign_4_lv_functor)
{
    function<std::string(const std::string&), 24> f1;
    auto                                          l1 = [](const std::string& rhs) { return "A" + rhs; };
    f1                                               = l1;
    EXPECT_THAT(f1("x"), "Ax");

    function<std::string(const std::string&), 80> f2;
    auto                                          l2 = [f1](const std::string& rhs) { return f1(rhs) + "B"; };
    f2                                               = l2;
    EXPECT_THAT(f2("x"), "AxB");

    // Note: we copy assign different type of function (Footprint and IsPmr).
    const function<std::string(const std::string&), 0, true /*IsPmr*/> f0{[](const std::string&) { return "123"; }};
    f2 = f0;
    EXPECT_THAT(!!f0, true);
    EXPECT_THAT(f0.get_memory_resource(), cetl::pmr::get_default_resource());
    EXPECT_THAT(f2("x"), "123");
}

TEST_F(TestPmrFunction, pmr_ctor_1_default)
{
    const function<void(), 0, true /*IsPmr*/> f1;
    EXPECT_THAT(!f1, Not(false));
    EXPECT_THAT(static_cast<bool>(f1), false);
    EXPECT_THAT(f1.get_memory_resource(), cetl::pmr::get_default_resource());
}

TEST_F(TestPmrFunction, pmr_ctor_2_nullptr)
{
    const function<void(), 0, true /*IsPmr*/> f1{nullptr};
    EXPECT_THAT(!f1, Not(false));
    EXPECT_THAT(static_cast<bool>(f1), false);
    EXPECT_THAT(f1.get_memory_resource(), cetl::pmr::get_default_resource());
}

TEST_F(TestPmrFunction, pmr_ctor_3_copy)
{
    using str_function = function<std::string(), 24, true /*IsPmr*/>;
    const str_function fn{get_mr(), std::bind(print_num, 123)};

    str_function fn_copy{fn};
    EXPECT_THAT(!!fn_copy, true);
    EXPECT_THAT(static_cast<bool>(fn_copy), true);
    EXPECT_THAT(fn_copy.get_memory_resource(), get_mr());
    EXPECT_THAT(fn_copy(), "123");

    fn_copy = {};
    const str_function fn_empty{fn_copy};
    EXPECT_THAT(!!fn_empty, false);
}

TEST_F(TestPmrFunction, pmr_ctor_4_move)
{
    using str_function = function<std::string(), 24, true /*IsPmr*/>;
    str_function fn{get_mr(), []() { return print_num(123); }};

    str_function fn_moved{std::move(fn)};
    EXPECT_THAT(!!fn_moved, true);
    EXPECT_THAT(static_cast<bool>(fn_moved), true);
    EXPECT_THAT(fn_moved(), "123");
    EXPECT_THAT(fn_moved.get_memory_resource(), get_mr());

    fn_moved = {};
    const str_function fn_empty{std::move(fn_moved)};
    EXPECT_THAT(!!fn_empty, false);
}

TEST_F(TestPmrFunction, pmr_ctor_5_lambda_default_mr_)
{
    using str_function = function<std::string(), 24, true /*IsPmr*/>;
    str_function fn{[]() { return print_num(123); }};
    EXPECT_THAT(!!fn, true);
    EXPECT_THAT(fn(), "123");
    EXPECT_THAT(fn.get_memory_resource(), cetl::pmr::get_default_resource());
}

TEST_F(TestPmrFunction, pmr_ctor_5_lambda_custom_mr_)
{
    using str_function = function<std::string(), 0, true /*IsPmr*/>;
    str_function fn{get_mr(), []() { return print_num(123); }};
    EXPECT_THAT(!!fn, true);
    EXPECT_THAT(fn(), "123");
    EXPECT_THAT(fn.get_memory_resource(), get_mr());
}

TEST_F(TestPmrFunction, pmr_ctor_memory_resource)
{
    // TODO: Remove `static_cast` when Callable constraint will be available.
    const function<void(), 0, true /*IsPmr*/> f1{get_mr()};
    EXPECT_THAT(!f1, Not(false));
    EXPECT_THAT(static_cast<bool>(f1), false);
}

TEST_F(TestPmrFunction, pmr_assign_1_copy_fit)
{
    using str_function = function<std::string(), 32, true /*IsPmr*/>;

    const str_function fn1{get_mr(), []() { return print_num(123); }};
    EXPECT_THAT(!!fn1, true);

    str_function fn2 = fn1;
    EXPECT_THAT(!!fn1, true);
    EXPECT_THAT(fn1.get_memory_resource(), get_mr());
    EXPECT_THAT(!!fn2, true);
    EXPECT_THAT(fn2(), "123");
    EXPECT_THAT(fn2.get_memory_resource(), get_mr());

    fn2 = {};
    EXPECT_THAT(!!fn1, true);
    EXPECT_THAT(fn1.get_memory_resource(), get_mr());
    EXPECT_THAT(!!fn2, false);
}

TEST_F(TestPmrFunction, pmr_assign_1_copy_nofit)
{
    using str_function = function<std::string(), 1, true /*IsPmr*/>;

    const str_function fn1{get_mr(), []() { return print_num(123); }};
    EXPECT_THAT(!!fn1, true);

    str_function fn2 = fn1;
    EXPECT_THAT(!!fn1, true);
    EXPECT_THAT(fn1.get_memory_resource(), get_mr());
    EXPECT_THAT(!!fn2, true);
    EXPECT_THAT(fn2(), "123");
    EXPECT_THAT(fn2.get_memory_resource(), get_mr());

    fn2 = {};
    EXPECT_THAT(!!fn1, true);
    EXPECT_THAT(fn1.get_memory_resource(), get_mr());
    EXPECT_THAT(!!fn2, false);
    EXPECT_THAT(fn2.get_memory_resource(), get_mr());
}

TEST_F(TestPmrFunction, pmr_assign_2_move_fit)
{
    using str_function = function<std::string(), 32, true /*IsPmr*/>;

    str_function fn1{get_mr(), []() { return print_num(123); }};
    EXPECT_THAT(!!fn1, true);

    str_function fn2 = std::move(fn1);
    EXPECT_THAT(!!fn1, false);
    EXPECT_THAT(!!fn2, true);
    EXPECT_THAT(fn2(), "123");

    fn2 = {};
    EXPECT_THAT(!!fn2, false);
    EXPECT_THAT(fn2.get_memory_resource(), get_mr());
}

TEST_F(TestPmrFunction, pmr_assign_2_move_nofit)
{
    using str_function = function<std::string(), 1, true /*IsPmr*/>;

    str_function fn1{get_mr(), []() { return print_num(123); }};
    EXPECT_THAT(!!fn1, true);

    str_function fn2 = std::move(fn1);
    EXPECT_THAT(!!fn1, false);
    EXPECT_THAT(!!fn2, true);
    EXPECT_THAT(fn2(), "123");

    fn2 = {};
    EXPECT_THAT(!!fn2, false);
    EXPECT_THAT(fn2.get_memory_resource(), get_mr());
}

TEST_F(TestPmrFunction, pmr_assign_3_nullptr)
{
    using str_function = function<std::string(), 24, true /*IsPmr*/>;
    str_function fn{get_mr(), []() { return print_num(123); }};
    EXPECT_THAT(!!fn, true);

    fn = nullptr;
    EXPECT_THAT(!!fn, false);
    EXPECT_THAT(fn.get_memory_resource(), get_mr());
}

TEST_F(TestPmrFunction, pmr_assign_4_rv_functor)
{
    using str_function = function<std::string(const std::string&), 24, true /*IsPmr*/>;

    // TODO: Remove `static_cast` when Callable constraint will be available.
    str_function f1{get_mr()};
    f1 = [](const std::string& rhs) { return "A" + rhs; };
    EXPECT_THAT(f1("x"), "Ax");
    EXPECT_THAT(f1.get_memory_resource(), get_mr());

    // TODO: Remove `static_cast` when Callable constraint will be available.
    str_function f2{get_mr()};
    f2 = [f1](const std::string& rhs) { return f1(rhs) + "B"; };
    EXPECT_THAT(f2("x"), "AxB");
    EXPECT_THAT(f2.get_memory_resource(), get_mr());

    // Note: we assign different type of function (different Footprint and IsPmr).
    function<std::string(const std::string&), 16, false /*IsPmr*/> f0{[](const std::string&) { return "123"; }};
    f2 = std::move(f0);
    EXPECT_THAT(!!f0, false);
    EXPECT_THAT(f2("x"), "123");
    EXPECT_THAT(f2.get_memory_resource(), get_mr());
}

TEST_F(TestPmrFunction, pmr_assign_4_lv_functor)
{
    using str_function = function<std::string(const std::string&), 24, true /*IsPmr*/>;

    // TODO: Remove `static_cast` when Callable constraint will be available.
    str_function f1{get_mr()};
    auto         l1 = [](const std::string& rhs) { return "A" + rhs; };
    f1              = l1;
    EXPECT_THAT(f1("x"), "Ax");
    EXPECT_THAT(f1.get_memory_resource(), get_mr());

    // TODO: Remove `static_cast` when Callable constraint will be available.
    str_function f2{get_mr()};
    auto         l2 = [f1](const std::string& rhs) { return f1(rhs) + "B"; };
    f2              = l2;
    EXPECT_THAT(f2("x"), "AxB");
    EXPECT_THAT(f2.get_memory_resource(), get_mr());

    // Note: we copy assign different type of function (Footprint and IsPmr).
    const function<std::string(const std::string&), 16, false /*IsPmr*/> f0{[](const std::string&) { return "123"; }};
    f2 = f0;
    EXPECT_THAT(!!f0, true);
    EXPECT_THAT(f2("x"), "123");
    EXPECT_THAT(f2.get_memory_resource(), get_mr());
}

}  // namespace