/// @file
/// Unit tests for cetl/pf23/expected.hpp: observers, monadic operations, comparison, unexpected and the polyfill
/// header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "test_pf23_expected.hpp"
#include "cetl/pf23/cetlpf.hpp"

#include <memory>
#include <string>

// NOLINTBEGIN(*-use-after-move)

namespace
{
using namespace cetlvast::pf23_expected;  // NOLINT(*-build-using-namespace)

template <typename Impl>
class test_pf23_expected_operations : public test_pf23_expected_base<Impl>
{};
TYPED_TEST_SUITE(test_pf23_expected_operations, implementations, );

int as_int(const category c)
{
    return static_cast<int>(c);
}

/// Calls op with a copy of source as each of lvalue, const lvalue, rvalue and const rvalue.
template <typename Ex, typename Op>
void for_each_category(const Ex& source, Op op)
{
    Ex       lvalue(source);
    const Ex const_lvalue(source);
    Ex       rvalue(source);
    const Ex const_rvalue(source);
    op(lvalue);
    op(const_lvalue);
    op(std::move(rvalue));
    op(std::move(const_rvalue));
}

struct widget
{
    int get() const
    {
        return value;
    }
    int add(const int x) const
    {
        return value + x;
    }
    int value;
};

// +----------------------------------------------------------------------+
// | OBSERVERS
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected_operations, value_access)
{
    using ex = expected_t<TypeParam, widget, int>;
    ex       mutable_ex(widget{1});
    const ex const_ex(widget{2});
    EXPECT_EQ(1, mutable_ex->value);
    EXPECT_EQ(2, const_ex->value);
    EXPECT_EQ(1, (*mutable_ex).value);
    EXPECT_EQ(2, (*const_ex).value);
    EXPECT_EQ(1, mutable_ex.value().value);
    EXPECT_EQ(2, const_ex.value().value);
    mutable_ex->value = 3;
    EXPECT_EQ(3, mutable_ex.value().value);
    EXPECT_EQ(2, std::move(const_ex).value().value);
    EXPECT_EQ(3, std::move(mutable_ex).value().value);
    static_assert(std::is_same<decltype(*mutable_ex), widget&>::value, "");
    static_assert(std::is_same<decltype(*const_ex), const widget&>::value, "");
    static_assert(std::is_same<decltype(*std::move(mutable_ex)), widget&&>::value, "");
    static_assert(std::is_same<decltype(*std::move(const_ex)), const widget&&>::value, "");
    static_assert(std::is_same<decltype(mutable_ex.value()), widget&>::value, "");
    static_assert(std::is_same<decltype(const_ex.value()), const widget&>::value, "");
    static_assert(std::is_same<decltype(std::move(mutable_ex).value()), widget&&>::value, "");
    static_assert(std::is_same<decltype(std::move(const_ex).value()), const widget&&>::value, "");
    static_assert(std::is_same<decltype(mutable_ex.operator->()), widget*>::value, "");
    static_assert(std::is_same<decltype(const_ex.operator->()), const widget*>::value, "");
    static_assert(noexcept(*mutable_ex), "");
    static_assert(noexcept(mutable_ex.has_value()), "");
}

TYPED_TEST(test_pf23_expected_operations, error_access)
{
    using ex = expected_t<TypeParam, int, widget>;
    ex       mutable_ex(TypeParam::unexpect(), widget{1});
    const ex const_ex(TypeParam::unexpect(), widget{2});
    EXPECT_EQ(1, mutable_ex.error().value);
    EXPECT_EQ(2, const_ex.error().value);
    static_assert(std::is_same<decltype(mutable_ex.error()), widget&>::value, "");
    static_assert(std::is_same<decltype(const_ex.error()), const widget&>::value, "");
    static_assert(std::is_same<decltype(std::move(mutable_ex).error()), widget&&>::value, "");
    static_assert(std::is_same<decltype(std::move(const_ex).error()), const widget&&>::value, "");
    static_assert(noexcept(mutable_ex.error()), "");
    using vex = expected_t<TypeParam, void, widget>;
    vex       mutable_vex(TypeParam::unexpect(), widget{3});
    const vex const_vex(TypeParam::unexpect(), widget{4});
    EXPECT_EQ(3, mutable_vex.error().value);
    EXPECT_EQ(4, const_vex.error().value);
    static_assert(std::is_same<decltype(std::move(mutable_vex).error()), widget&&>::value, "");
    static_assert(std::is_same<decltype(std::move(const_vex).error()), const widget&&>::value, "");
}

TYPED_TEST(test_pf23_expected_operations, value_or)
{
    using ex = expected_t<TypeParam, std::string, int>;
    const ex value("value");
    const ex error(TypeParam::unexpect(), 1);
    EXPECT_EQ("value", value.value_or("default"));
    EXPECT_EQ("default", error.value_or("default"));
    ex movable("moved");
    EXPECT_EQ("moved", std::move(movable).value_or("default"));
    EXPECT_TRUE(movable->empty());
    ex movable_error(TypeParam::unexpect(), 2);
    EXPECT_EQ("default", std::move(movable_error).value_or("default"));
}

TYPED_TEST(test_pf23_expected_operations, error_or)
{
    using ex = expected_t<TypeParam, int, std::string>;
    const ex value(1);
    const ex error(TypeParam::unexpect(), "error");
    EXPECT_EQ("default", value.error_or("default"));
    EXPECT_EQ("error", error.error_or("default"));
    ex movable(TypeParam::unexpect(), "moved");
    EXPECT_EQ("moved", std::move(movable).error_or("default"));
    EXPECT_TRUE(movable.error().empty());
    using vex = expected_t<TypeParam, void, std::string>;
    const vex vvalue;
    const vex verror(TypeParam::unexpect(), "error");
    EXPECT_EQ("default", vvalue.error_or("default"));
    EXPECT_EQ("error", verror.error_or("default"));
    vex vmovable(TypeParam::unexpect(), "moved");
    EXPECT_EQ("moved", std::move(vmovable).error_or("default"));
}

/// Constructible from conversion_source both explicitly (the better match) and, through the source's conversion
/// operator, implicitly; how records which one was used.
struct conversion_source;
struct conversion_target
{
    explicit conversion_target(const conversion_source&) noexcept
        : how{1}
    {
    }
    conversion_target(const int h) noexcept  // NOLINT(*-explicit-constructor)
        : how{h}
    {
    }
    int how;
};
struct conversion_source
{
    operator conversion_target() const noexcept  // NOLINT(*-explicit-constructor)
    {
        return conversion_target{2};
    }
};

TYPED_TEST(test_pf23_expected_operations, error_or_converts_implicitly)
{
    // error_or returns its argument, so the implicit conversion is used even where an explicit constructor would match
    // better.
    using ex  = expected_t<TypeParam, int, conversion_target>;
    using vex = expected_t<TypeParam, void, conversion_target>;
    const ex  value(1);
    const vex vvalue;
    EXPECT_EQ(2, value.error_or(conversion_source{}).how);
    EXPECT_EQ(2, ex(1).error_or(conversion_source{}).how);
    EXPECT_EQ(2, vvalue.error_or(conversion_source{}).how);
    EXPECT_EQ(2, vex().error_or(conversion_source{}).how);
}

TYPED_TEST(test_pf23_expected_operations, void_value_access)
{
    using vex = expected_t<TypeParam, void, int>;
    const vex value;
    EXPECT_TRUE(value.has_value());
    EXPECT_TRUE(static_cast<bool>(value));
    static_assert(std::is_same<decltype(*value), void>::value, "");
    static_assert(std::is_same<decltype(value.value()), void>::value, "");
    static_assert(std::is_same<decltype(std::move(value).value()), void>::value, "");
    *value;
    value.value();
    const vex in_place(TypeParam::in_place());
    EXPECT_TRUE(in_place.has_value());
    const vex error(TypeParam::unexpect(), 1);
    EXPECT_FALSE(error.has_value());
    EXPECT_FALSE(static_cast<bool>(error));
}

#if defined(__cpp_exceptions)

TYPED_TEST(test_pf23_expected_operations, value_throws_bad_expected_access)
{
    using ex     = expected_t<TypeParam, int, std::string>;
    using access = typename TypeParam::template bad_expected_access<std::string>;
    ex       error(TypeParam::unexpect(), "error");
    const ex const_error(TypeParam::unexpect(), "const error");
    try
    {
        consume(error.value());
        FAIL() << "value() did not throw";
    } catch (const access& e)
    {
        EXPECT_EQ("error", e.error());
    }
    try
    {
        consume(const_error.value());
        FAIL() << "value() did not throw";
    } catch (const access& e)
    {
        EXPECT_EQ("const error", e.error());
    }
    // An rvalue moves its error into the exception.
    try
    {
        consume(std::move(error).value());
        FAIL() << "value() did not throw";
    } catch (access& e)
    {
        EXPECT_EQ("error", e.error());
        EXPECT_EQ("error", std::move(e).error());
        EXPECT_TRUE(error.error().empty());
    }
    EXPECT_THROW(consume(std::move(const_error).value()), access);
    // The exception can be caught through its bases.
    using base_access = typename TypeParam::template bad_expected_access<void>;
    EXPECT_THROW(consume(const_error.value()), base_access);
    EXPECT_THROW(consume(const_error.value()), std::exception);
    try
    {
        consume(const_error.value());
    } catch (const std::exception& e)
    {
        EXPECT_NE(nullptr, e.what());
    }
    static_assert(!std::is_default_constructible<base_access>::value, "");
}

TYPED_TEST(test_pf23_expected_operations, void_value_throws_bad_expected_access)
{
    using vex    = expected_t<TypeParam, void, int>;
    using access = typename TypeParam::template bad_expected_access<int>;
    vex error(TypeParam::unexpect(), 1);
    try
    {
        error.value();
        FAIL() << "value() did not throw";
    } catch (const access& e)
    {
        EXPECT_EQ(1, e.error());
    }
    EXPECT_THROW(std::move(error).value(), access);
}

TEST(test_pf23_expected_cetl, bad_expected_access_what)
{
    const cetl::pf23::bad_expected_access<int> e{3};
    EXPECT_STREQ("bad_expected_access", e.what());
    EXPECT_EQ(3, e.error());
    cetl::pf23::bad_expected_access<int> copy{e};
    copy = e;
    EXPECT_EQ(3, copy.error());
}

#endif  // __cpp_exceptions

// +----------------------------------------------------------------------+
// | MONADIC OPERATIONS
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected_operations, and_then)
{
    using ex        = expected_t<TypeParam, int, std::string>;
    const auto half = [](const int x) -> ex {
        if (x % 2 != 0)
        {
            return ex(TypeParam::unexpect(), "odd");
        }
        return x / 2;
    };
    EXPECT_EQ(2, *ex(8).and_then(half).and_then(half));
    EXPECT_EQ("odd", ex(6).and_then(half).and_then(half).error());
    int        calls = 0;
    const auto count = [&calls](const int x) -> ex {
        ++calls;
        return x;
    };
    EXPECT_EQ("error", ex(TypeParam::unexpect(), "error").and_then(count).error());
    EXPECT_EQ(0, calls);
    // The result may have a different value type.
    const auto to_string = [](const int x) {
        return expected_t<TypeParam, std::string, std::string>(std::to_string(x));
    };
    EXPECT_EQ("3", *ex(3).and_then(to_string));
}

TYPED_TEST(test_pf23_expected_operations, and_then_value_categories)
{
    using ex       = expected_t<TypeParam, int, int>;
    using recorder = category_recorder<ex>;
    ex       value(0);
    const ex const_value(0);
    EXPECT_EQ(as_int(category::lvalue), *value.and_then(recorder{}));
    EXPECT_EQ(as_int(category::const_lvalue), *const_value.and_then(recorder{}));
    EXPECT_EQ(as_int(category::rvalue), *std::move(value).and_then(recorder{}));
    EXPECT_EQ(as_int(category::const_rvalue), *std::move(const_value).and_then(recorder{}));
    // The error is forwarded with the same category.
    using mex       = expected_t<TypeParam, int, std::unique_ptr<int>>;
    const auto keep = [](int x) { return mex(x); };
    mex        error(TypeParam::unexpect(), new int{5});
    const mex  result = std::move(error).and_then(keep);
    EXPECT_EQ(5, *result.error());
    EXPECT_EQ(nullptr, error.error().get());
}

TYPED_TEST(test_pf23_expected_operations, or_else)
{
    using ex           = expected_t<TypeParam, int, std::string>;
    const auto recover = [](const std::string& e) { return ex(static_cast<int>(e.size())); };
    EXPECT_EQ(5, *ex(TypeParam::unexpect(), "error").or_else(recover));
    EXPECT_EQ(1, *ex(1).or_else(recover));
    // The result may have a different error type.
    const auto rethrow = [](const std::string& e) {
        return expected_t<TypeParam, int, int>(TypeParam::unexpect(), static_cast<int>(e.size()));
    };
    EXPECT_EQ(5, ex(TypeParam::unexpect(), "error").or_else(rethrow).error());
    EXPECT_EQ(7, *ex(7).or_else(rethrow));
    // Value categories of the error, and of the value carried over.
    using iex      = expected_t<TypeParam, int, int>;
    using recorder = category_recorder<iex>;
    iex       error(TypeParam::unexpect(), 0);
    const iex const_error(TypeParam::unexpect(), 0);
    EXPECT_EQ(as_int(category::lvalue), *error.or_else(recorder{}));
    EXPECT_EQ(as_int(category::const_lvalue), *const_error.or_else(recorder{}));
    EXPECT_EQ(as_int(category::rvalue), *std::move(error).or_else(recorder{}));
    EXPECT_EQ(as_int(category::const_rvalue), *std::move(const_error).or_else(recorder{}));
    using mex = expected_t<TypeParam, std::unique_ptr<int>, int>;
    mex       value(TypeParam::in_place(), new int{6});
    const mex result = std::move(value).or_else([](int e) { return mex(TypeParam::unexpect(), e); });
    EXPECT_EQ(6, **result);
    EXPECT_EQ(nullptr, value->get());
}

TYPED_TEST(test_pf23_expected_operations, transform)
{
    using ex          = expected_t<TypeParam, int, std::string>;
    const auto twice  = [](const int x) { return 2 * x; };
    const auto string = [](const int x) { return std::to_string(x); };
    const auto r1     = ex(3).transform(twice).transform(string);
    static_assert(std::is_same<decltype(r1), const expected_t<TypeParam, std::string, std::string>>::value, "");
    EXPECT_EQ("6", *r1);
    EXPECT_EQ("error", ex(TypeParam::unexpect(), "error").transform(twice).error());
    // A callable that returns void gives an expected<void, E>.
    int        calls = 0;
    const auto touch = [&calls](const int) { ++calls; };
    const auto r2    = ex(1).transform(touch);
    static_assert(std::is_same<decltype(r2), const expected_t<TypeParam, void, std::string>>::value, "");
    EXPECT_TRUE(r2.has_value());
    EXPECT_EQ(1, calls);
    EXPECT_FALSE(ex(TypeParam::unexpect(), "error").transform(touch).has_value());
    EXPECT_EQ(1, calls);
    // cv-qualifiers are removed from the result.
    const auto constant = [](const int x) -> const std::string { return std::to_string(x); };
    static_assert(std::is_same<decltype(ex(1).transform(constant)),
                               expected_t<TypeParam, std::string, std::string>>::value,
                  "");
    // Value categories.
    using iex = expected_t<TypeParam, int, int>;
    iex       value(0);
    const iex const_value(0);
    EXPECT_EQ(as_int(category::lvalue), *value.transform(category_recorder<int>{}));
    EXPECT_EQ(as_int(category::const_lvalue), *const_value.transform(category_recorder<int>{}));
    EXPECT_EQ(as_int(category::rvalue), *std::move(value).transform(category_recorder<int>{}));
    EXPECT_EQ(as_int(category::const_rvalue), *std::move(const_value).transform(category_recorder<int>{}));
}

TYPED_TEST(test_pf23_expected_operations, transform_error)
{
    using ex          = expected_t<TypeParam, int, int>;
    const auto string = [](const int e) { return std::to_string(e); };
    const auto r1     = ex(TypeParam::unexpect(), 4).transform_error(string);
    static_assert(std::is_same<decltype(r1), const expected_t<TypeParam, int, std::string>>::value, "");
    EXPECT_EQ("4", r1.error());
    EXPECT_EQ(5, *ex(5).transform_error(string));
    ex       error(TypeParam::unexpect(), 0);
    const ex const_error(TypeParam::unexpect(), 0);
    EXPECT_EQ(as_int(category::lvalue), error.transform_error(category_recorder<int>{}).error());
    EXPECT_EQ(as_int(category::const_lvalue), const_error.transform_error(category_recorder<int>{}).error());
    EXPECT_EQ(as_int(category::rvalue), std::move(error).transform_error(category_recorder<int>{}).error());
    EXPECT_EQ(as_int(category::const_rvalue), std::move(const_error).transform_error(category_recorder<int>{}).error());
    // The value is moved from an rvalue.
    using mex = expected_t<TypeParam, std::unique_ptr<int>, int>;
    mex        value(TypeParam::in_place(), new int{7});
    const auto r2 = std::move(value).transform_error(string);
    EXPECT_EQ(7, **r2);
    EXPECT_EQ(nullptr, value->get());
}

TYPED_TEST(test_pf23_expected_operations, monadic_operations_on_void)
{
    using vex  = expected_t<TypeParam, void, int>;
    using iex  = expected_t<TypeParam, int, int>;
    int  calls = 0;
    auto bump  = [&calls]() { return iex(++calls); };
    vex  value;
    vex  error(TypeParam::unexpect(), 9);
    EXPECT_EQ(1, *value.and_then(bump));
    EXPECT_EQ(9, error.and_then(bump).error());
    EXPECT_EQ(1, calls);
    EXPECT_EQ(2, *value.transform([] { return 2; }));
    EXPECT_EQ(9, error.transform([] { return 2; }).error());
    const auto nothing = [] {};
    static_assert(std::is_same<decltype(value.transform(nothing)), vex>::value, "");
    EXPECT_TRUE(value.transform(nothing).has_value());
    EXPECT_TRUE(value.or_else([](int) { return vex(TypeParam::unexpect(), 0); }).has_value());
    EXPECT_TRUE(error.or_else([](int) { return vex(); }).has_value());
    EXPECT_EQ(10, error.or_else([](int e) { return vex(TypeParam::unexpect(), e + 1); }).error());
    const auto string = [](int e) { return std::to_string(e); };
    EXPECT_TRUE(value.transform_error(string).has_value());
    EXPECT_EQ("9", error.transform_error(string).error());
    static_assert(std::is_same<decltype(error.transform_error(string)),
                               expected_t<TypeParam, void, std::string>>::value,
                  "");
    // Value categories of the error.
    const vex const_error(TypeParam::unexpect(), 0);
    vex       mutable_error(TypeParam::unexpect(), 0);
    EXPECT_EQ(as_int(category::lvalue), mutable_error.transform_error(category_recorder<int>{}).error());
    EXPECT_EQ(as_int(category::const_lvalue), const_error.transform_error(category_recorder<int>{}).error());
    EXPECT_EQ(as_int(category::rvalue), std::move(mutable_error).transform_error(category_recorder<int>{}).error());
    EXPECT_EQ(as_int(category::const_rvalue), std::move(const_error).transform_error(category_recorder<int>{}).error());
    const auto void_recorder = [](auto&& e) {
        return vex(TypeParam::unexpect(), std::is_const<std::remove_reference_t<decltype(e)>>::value ? 1 : 0);
    };
    EXPECT_EQ(1, const_error.or_else(void_recorder).error());
    EXPECT_EQ(0, std::move(const_error).and_then([] { return vex(); }).error());
    EXPECT_EQ(0, std::move(mutable_error).transform([] { return 1; }).error());
}

TYPED_TEST(test_pf23_expected_operations, monadic_operations_every_category_and_state)
{
    using ex = expected_t<TypeParam, int, int>;
    for (const ex& source : {ex(3), ex(TypeParam::unexpect(), 4)})
    {
        const bool has = source.has_value();
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).and_then([](const int v) { return ex(v + 1); });
            EXPECT_EQ(has, r.has_value());
            EXPECT_EQ(4, r.has_value() ? *r : r.error());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r =
                std::forward<decltype(e)>(e).or_else([](const int v) { return ex(TypeParam::unexpect(), v + 2); });
            EXPECT_EQ(has, r.has_value());
            EXPECT_EQ(has ? 3 : 6, r.has_value() ? *r : r.error());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).transform([](const int v) { return v * 2; });
            EXPECT_EQ(has, r.has_value());
            EXPECT_EQ(has ? 6 : 4, r.has_value() ? *r : r.error());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).transform([](const int) {});
            EXPECT_EQ(has, r.has_value());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).transform_error([](const int v) { return v * 3; });
            EXPECT_EQ(has, r.has_value());
            EXPECT_EQ(has ? 3 : 12, r.has_value() ? *r : r.error());
        });
    }
    using vex = expected_t<TypeParam, void, int>;
    for (const vex& source : {vex(), vex(TypeParam::unexpect(), 4)})
    {
        const bool has = source.has_value();
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).and_then([] { return vex(); });
            EXPECT_EQ(has, r.has_value());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r =
                std::forward<decltype(e)>(e).or_else([](const int v) { return vex(TypeParam::unexpect(), v + 2); });
            EXPECT_EQ(has, r.has_value());
            EXPECT_EQ(has ? 0 : 6, r.has_value() ? 0 : r.error());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).transform([] { return 2; });
            EXPECT_EQ(has, r.has_value());
            EXPECT_EQ(has ? 2 : 4, r.has_value() ? *r : r.error());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).transform([] {});
            EXPECT_EQ(has, r.has_value());
        });
        for_each_category(source, [has](auto&& e) {
            const auto r = std::forward<decltype(e)>(e).transform_error([](const int v) { return v * 3; });
            EXPECT_EQ(has, r.has_value());
            EXPECT_EQ(has ? 0 : 12, r.has_value() ? 0 : r.error());
        });
    }
}

TYPED_TEST(test_pf23_expected_operations, monadic_operations_invoke_members)
{
    using ex = expected_t<TypeParam, widget, int>;
    const ex value(widget{4});
    EXPECT_EQ(4, *value.transform(&widget::get));
    using pex = expected_t<TypeParam, const widget*, int>;
    const widget w{5};
    const pex    pointer(&w);
    EXPECT_EQ(5, *pointer.transform(&widget::get));
    using rex = expected_t<TypeParam, std::reference_wrapper<const widget>, int>;
    const rex ref(std::cref(w));
    EXPECT_EQ(5, *ref.transform(&widget::get));
    // The value category of a data member follows the object.
    using iex         = expected_t<TypeParam, int, int>;
    const auto rvalue = ex(widget{6}).and_then([](widget&& x) { return iex(std::move(x).value); });
    EXPECT_EQ(6, *rvalue);
}

TYPED_TEST(test_pf23_expected_operations, monadic_operations_forward_the_callable)
{
    struct qualified_callable
    {
        int operator()(int) & noexcept
        {
            return 1;
        }
        int operator()(int) && noexcept
        {
            return 2;
        }
    };
    using ex = expected_t<TypeParam, int, int>;
    const ex           value(0);
    qualified_callable callable{};
    EXPECT_EQ(1, *value.transform(callable));
    EXPECT_EQ(2, *value.transform(qualified_callable{}));
}

TYPED_TEST(test_pf23_expected_operations, transform_constructs_in_place)
{
    // transform() must not copy or move the new value: it is initialized from the result of the call.
    using ex = expected_t<TypeParam, int, int>;
    value_counted::reset();
    {
        const auto made = ex(3).transform([](const int x) { return value_counted{x}; });
        EXPECT_EQ(3, made->value);
#if __cplusplus >= CETL_CPP_STANDARD_17
        EXPECT_EQ(0, value_counted::stats().copy_ctor + value_counted::stats().move_ctor);
#endif
    }
}

// +----------------------------------------------------------------------+
// | COMPARISON
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected_operations, compare_expected)
{
    using ex  = expected_t<TypeParam, int, std::string>;
    using lex = expected_t<TypeParam, long, std::string>;
    const ex one(1);
    const ex error(TypeParam::unexpect(), "e");
    EXPECT_TRUE(one == ex(1));
    EXPECT_FALSE(one != ex(1));
    EXPECT_FALSE(one == ex(2));
    EXPECT_TRUE(one != ex(2));
    EXPECT_FALSE(one == error);
    EXPECT_FALSE(error == one);
    EXPECT_TRUE(error == ex(TypeParam::unexpect(), "e"));
    EXPECT_FALSE(error == ex(TypeParam::unexpect(), "f"));
    EXPECT_TRUE(one == lex(1L));
    EXPECT_TRUE(lex(1L) == one);
    EXPECT_TRUE(error != lex(1L));
}

TYPED_TEST(test_pf23_expected_operations, compare_value)
{
    using ex = expected_t<TypeParam, int, int>;
    const ex one(1);
    const ex error(TypeParam::unexpect(), 1);
    EXPECT_TRUE(one == 1);
    EXPECT_TRUE(1 == one);
    EXPECT_TRUE(one == 1L);
    EXPECT_FALSE(one != 1);
    EXPECT_TRUE(one != 2);
    EXPECT_TRUE(2 != one);
    EXPECT_FALSE(error == 1);
    EXPECT_FALSE(1 == error);
    EXPECT_TRUE(error != 1);
}

TYPED_TEST(test_pf23_expected_operations, compare_unexpected)
{
    using ex   = expected_t<TypeParam, int, int>;
    using unex = unexpected_t<TypeParam, int>;
    const ex one(1);
    const ex error(TypeParam::unexpect(), 1);
    EXPECT_TRUE(error == unex(1));
    EXPECT_TRUE(unex(1) == error);
    EXPECT_TRUE((error == unexpected_t<TypeParam, long>(1L)));
    EXPECT_FALSE(error == unex(2));
    EXPECT_TRUE(error != unex(2));
    EXPECT_TRUE(unex(2) != error);
    EXPECT_FALSE(one == unex(1));
    EXPECT_TRUE(one != unex(1));
}

TYPED_TEST(test_pf23_expected_operations, compare_void)
{
    using vex  = expected_t<TypeParam, void, int>;
    using unex = unexpected_t<TypeParam, int>;
    const vex value;
    const vex error(TypeParam::unexpect(), 1);
    EXPECT_TRUE(value == vex());
    EXPECT_TRUE((value == expected_t<TypeParam, void, long>()));
    EXPECT_FALSE(value == error);
    EXPECT_FALSE(error == value);
    EXPECT_TRUE(value != error);
    EXPECT_TRUE(error == vex(TypeParam::unexpect(), 1));
    EXPECT_FALSE(error == vex(TypeParam::unexpect(), 2));
    EXPECT_TRUE(error == unex(1));
    EXPECT_TRUE(unex(1) == error);
    EXPECT_TRUE(error != unex(2));
    EXPECT_TRUE(unex(2) != error);
    EXPECT_FALSE(value == unex(1));
}

/// Detects whether `a == b` is well-formed.
template <typename A, typename B, typename = void>
struct is_equality_comparable : std::false_type
{};
template <typename A, typename B>
struct is_equality_comparable<A, B, decltype((void) (std::declval<const A&>() == std::declval<const B&>()))>
    : std::true_type
{};

struct not_comparable
{};

TEST(test_pf23_expected_cetl, comparison_constraints)
{
    // The operators take part in overload resolution only where the comparisons they make are valid (P3379R0, a
    // C++26 change). C++23 made these Mandates, and libc++ 23 still does, so this is not checked against std.
    using ex = cetl::pf23::expected<not_comparable, int>;
    static_assert(!is_equality_comparable<ex, not_comparable>::value, "");
    static_assert(!is_equality_comparable<ex, ex>::value, "");
    static_assert(is_equality_comparable<ex, cetl::pf23::unexpected<int>>::value, "");
    static_assert(!is_equality_comparable<ex, cetl::pf23::unexpected<not_comparable>>::value, "");
    using iex = cetl::pf23::expected<int, int>;
    using vex = cetl::pf23::expected<void, int>;
    static_assert(!is_equality_comparable<iex, vex>::value, "");
    static_assert(!is_equality_comparable<vex, iex>::value, "");
    static_assert(is_equality_comparable<iex, int>::value, "");
    static_assert(is_equality_comparable<int, iex>::value, "");
    static_assert(!is_equality_comparable<vex, int>::value, "");
    SUCCEED();
}

TEST(test_pf23_expected_cetl, invoke_member_pointers)
{
    // transform() cannot return a reference, so the data member forms of INVOKE are checked directly.
    using cetl::pf23::detail::exp::invoke;
    widget       w{1};
    const widget cw{2};
    static_assert(std::is_same<decltype(invoke(&widget::value, w)), int&>::value, "");
    static_assert(std::is_same<decltype(invoke(&widget::value, cw)), const int&>::value, "");
    static_assert(std::is_same<decltype(invoke(&widget::value, std::move(w))), int&&>::value, "");
    static_assert(std::is_same<decltype(invoke(&widget::value, &w)), int&>::value, "");
    static_assert(std::is_same<decltype(invoke(&widget::value, std::ref(w))), int&>::value, "");
    static_assert(std::is_same<decltype(invoke(&widget::value, std::cref(w))), const int&>::value, "");
    EXPECT_EQ(1, invoke(&widget::value, w));
    EXPECT_EQ(2, invoke(&widget::value, &cw));
    EXPECT_EQ(2, invoke(&widget::value, std::cref(cw)));
    EXPECT_EQ(4, invoke(&widget::add, &cw, 2));
    EXPECT_EQ(3, invoke(&widget::add, std::ref(w), 2));
    EXPECT_EQ(5, invoke([](const int x) { return x; }, 5));
}

/// Converts to bool implicitly as true and explicitly (from a non-const object) as false, so the result of an operator
/// shows which conversion it applied.
struct conversion_witness
{
    operator bool() const noexcept  // NOLINT(*-explicit-constructor)
    {
        return true;
    }
    explicit operator bool() noexcept
    {
        return false;
    }
};

struct witnessed
{
    int value;
};

conversion_witness operator==(const witnessed&, const witnessed&) noexcept
{
    return {};
}

/// As conversion_witness, but the explicit conversion is deleted, so an operator that applies it does not compile.
struct deleted_explicit_bool
{
    operator bool() const noexcept  // NOLINT(*-explicit-constructor)
    {
        return true;
    }
    explicit operator bool() = delete;
};

struct strictly_compared
{
    int value;
};

deleted_explicit_bool operator==(const strictly_compared&, const strictly_compared&) noexcept
{
    return {};
}

#if CETLVAST_HAS_STD_EXPECTED
/// True if the standard library implements LWG 4366 (libc++ 23 does; libstdc++ 13 and 15 do not).
bool std_implements_lwg4366()
{
    return std::expected<witnessed, int>(witnessed{1}) == witnessed{2};
}
#endif

TYPED_TEST(test_pf23_expected_operations, expected_equality_converts_implicitly)
{
    // The operators return the comparison, so its implicit conversion to bool is used (LWG 4366).
#if CETLVAST_HAS_STD_EXPECTED
    if (std::is_same<TypeParam, std_impl>::value && !std_implements_lwg4366())
    {
        GTEST_SKIP() << "This standard library predates LWG 4366.";
    }
#endif
    using wex  = expected_t<TypeParam, witnessed, witnessed>;
    using unex = unexpected_t<TypeParam, witnessed>;
    const wex value(witnessed{1});
    const wex error(TypeParam::unexpect(), witnessed{2});
    EXPECT_TRUE(value == wex(witnessed{3}));
    EXPECT_TRUE(error == wex(TypeParam::unexpect(), witnessed{4}));
    EXPECT_TRUE(value == witnessed{3});
    EXPECT_TRUE(witnessed{3} == value);
    EXPECT_FALSE(value != witnessed{3});
    EXPECT_TRUE(error == unex(witnessed{4}));
    EXPECT_TRUE(unex(witnessed{4}) == error);
    using vex = expected_t<TypeParam, void, witnessed>;
    const vex verror(TypeParam::unexpect(), witnessed{2});
    EXPECT_TRUE(verror == vex(TypeParam::unexpect(), witnessed{4}));
    EXPECT_TRUE(verror == unex(witnessed{4}));
}

TEST(test_pf23_expected_cetl, equality_never_converts_explicitly)
{
    // With the explicit conversion deleted, these compile only because no operator applies it (LWG 4366).
    using sex  = cetl::pf23::expected<strictly_compared, strictly_compared>;
    using unex = cetl::pf23::unexpected<strictly_compared>;
    using vex  = cetl::pf23::expected<void, strictly_compared>;
    const sex value(strictly_compared{1});
    const sex error(cetl::pf23::unexpect, strictly_compared{2});
    const vex verror(cetl::pf23::unexpect, strictly_compared{3});
    EXPECT_TRUE(value == sex(strictly_compared{4}));
    EXPECT_TRUE(error == sex(cetl::pf23::unexpect, strictly_compared{4}));
    EXPECT_TRUE(value == strictly_compared{4});
    EXPECT_TRUE(strictly_compared{4} == value);
    EXPECT_TRUE(error == unex(strictly_compared{4}));
    EXPECT_TRUE(unex(strictly_compared{4}) == error);
    EXPECT_TRUE(verror == vex(cetl::pf23::unexpect, strictly_compared{4}));
    EXPECT_TRUE(verror == unex(strictly_compared{4}));
    EXPECT_TRUE(unex(strictly_compared{1}) == unex(strictly_compared{4}));
}

// +----------------------------------------------------------------------+
// | UNEXPECTED
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected_operations, unexpected)
{
    using unex = unexpected_t<TypeParam, error_counted>;
    unex       from_value(error_counted{1});
    const unex in_place(TypeParam::in_place(), 2, 3);
    const unex from_list(TypeParam::in_place(), {1, 2}, 3);
    EXPECT_EQ(1, from_value.error().value);
    EXPECT_EQ(5, in_place.error().value);
    EXPECT_EQ(5, from_list.error().value);
    static_assert(std::is_same<decltype(from_value.error()), error_counted&>::value, "");
    static_assert(std::is_same<decltype(in_place.error()), const error_counted&>::value, "");
    static_assert(std::is_same<decltype(std::move(from_value).error()), error_counted&&>::value, "");
    static_assert(std::is_same<decltype(std::move(in_place).error()), const error_counted&&>::value, "");
    unex copy(from_value);
    EXPECT_TRUE(copy == from_value);
    EXPECT_FALSE(copy != from_value);
    EXPECT_TRUE(copy != in_place);
    copy = in_place;
    EXPECT_EQ(5, copy.error().value);
    copy.swap(from_value);
    EXPECT_EQ(1, copy.error().value);
    EXPECT_EQ(5, from_value.error().value);
    swap(copy, from_value);
    EXPECT_EQ(5, copy.error().value);
    EXPECT_TRUE((unexpected_t<TypeParam, int>(1) == unexpected_t<TypeParam, long>(1L)));
    // The comparison is returned, so its implicit conversion to bool is used.
    EXPECT_TRUE((unexpected_t<TypeParam, witnessed>(witnessed{1}) == unexpected_t<TypeParam, witnessed>(witnessed{2})));
    // Construction from a value is explicit.
    static_assert(!std::is_convertible<int, unexpected_t<TypeParam, int>>::value, "");
    static_assert(std::is_constructible<unexpected_t<TypeParam, int>, int>::value, "");
    static_assert(!std::is_default_constructible<unexpected_t<TypeParam, int>>::value, "");
}

TYPED_TEST(test_pf23_expected_operations, unexpected_swap_constraints)
{
    // What libc++ and libstdc++ agree on ([expected.un.swap]): the free swap takes part in overload resolution only if
    // E is swappable, so the swappable traits see through to E, and swapping works when E is swappable. They disagree
    // on whether the member swap is constrained (libstdc++) or only mandated (the standard's wording, libc++ and this
    // polyfill), so that is not checked here; test_pf23_unexpected_swap_not_swappable.cpp checks that calling it with
    // a non-swappable E does not compile.
    static_assert(cetl::pf17::is_swappable<unexpected_t<TypeParam, int>>::value, "");
    static_assert(cetl::pf17::is_nothrow_swappable<unexpected_t<TypeParam, int>>::value, "");
    static_assert(cetl::pf17::is_swappable<unexpected_t<TypeParam, move_only>>::value, "");
    static_assert(!cetl::pf17::is_swappable<unexpected_t<TypeParam, non_movable>>::value, "");
    static_assert(cetl::pf17::is_swappable<unexpected_t<TypeParam, potentially_throwing_int>>::value, "");
    static_assert(!cetl::pf17::is_nothrow_swappable<unexpected_t<TypeParam, potentially_throwing_int>>::value, "");
#if __cplusplus >= CETL_CPP_STANDARD_17
    static_assert(std::is_swappable_v<unexpected_t<TypeParam, move_only>>, "");
    static_assert(!std::is_swappable_v<unexpected_t<TypeParam, non_movable>>, "");
    static_assert(!std::is_nothrow_swappable_v<unexpected_t<TypeParam, potentially_throwing_int>>, "");
#endif
    unexpected_t<TypeParam, move_only> a(move_only{1});
    unexpected_t<TypeParam, move_only> b(move_only{2});
    swap(a, b);
    EXPECT_EQ(2, a.error().value);
    EXPECT_EQ(1, b.error().value);
    a.swap(b);
    EXPECT_EQ(1, a.error().value);
    EXPECT_EQ(2, b.error().value);
    unexpected_t<TypeParam, potentially_throwing_int> c(potentially_throwing_int{3});
    unexpected_t<TypeParam, potentially_throwing_int> d(potentially_throwing_int{4});
    swap(c, d);
    EXPECT_EQ(4, c.error().value);
    EXPECT_EQ(3, d.error().value);
}

#if defined(__cpp_deduction_guides)
TEST(test_pf23_expected_cetl, unexpected_deduction_guide)
{
    const cetl::pf23::unexpected deduced(3);
    static_assert(std::is_same<decltype(deduced), const cetl::pf23::unexpected<int>>::value, "");
    EXPECT_EQ(3, deduced.error());
}
#endif

// +----------------------------------------------------------------------+
// | POLYFILL HEADER
// +----------------------------------------------------------------------+

TEST(test_pf23_expected_cetl, cetlpf)
{
#if CETLVAST_HAS_STD_EXPECTED
    static_assert(std::is_same<cetl::expected<int, long>, std::expected<int, long>>::value, "");
    static_assert(std::is_same<cetl::unexpected<int>, std::unexpected<int>>::value, "");
    static_assert(std::is_same<cetl::unexpect_t, std::unexpect_t>::value, "");
#else
    static_assert(std::is_same<cetl::expected<int, long>, cetl::pf23::expected<int, long>>::value, "");
    static_assert(std::is_same<cetl::unexpected<int>, cetl::pf23::unexpected<int>>::value, "");
    static_assert(std::is_same<cetl::unexpect_t, cetl::pf23::unexpect_t>::value, "");
#endif
    // The tag types from the polyfill headers work with whichever expected was selected.
    const cetl::expected<int, long> value(cetl::in_place, 1);
    const cetl::expected<int, long> error(cetl::unexpect, 2L);
    EXPECT_EQ(1, *value);
    EXPECT_EQ(2L, error.error());
}

}  // namespace

// NOLINTEND(*-use-after-move)
