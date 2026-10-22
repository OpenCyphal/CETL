/// @file
/// Unit tests for cetl/pf23/expected.hpp: type properties, construction, assignment, emplace() and swap().
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include "test_pf23_expected.hpp"

#include <memory>
#include <string>
#include <vector>

// NOLINTBEGIN(*-use-after-move)

namespace
{
using namespace cetlvast::pf23_expected;  // NOLINT(*-build-using-namespace)

template <typename Impl>
class test_pf23_expected : public test_pf23_expected_base<Impl>
{};
TYPED_TEST_SUITE(test_pf23_expected, implementations, );

// +----------------------------------------------------------------------+
// | TYPE PROPERTIES
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected, member_types)
{
    using ex = expected_t<TypeParam, int, long>;
    static_assert(std::is_same<typename ex::value_type, int>::value, "");
    static_assert(std::is_same<typename ex::error_type, long>::value, "");
    static_assert(std::is_same<typename ex::unexpected_type, unexpected_t<TypeParam, long>>::value, "");
    static_assert(std::is_same<typename ex::template rebind<char>, expected_t<TypeParam, char, long>>::value, "");
    using vex = expected_t<TypeParam, void, long>;
    static_assert(std::is_same<typename vex::value_type, void>::value, "");
    static_assert(std::is_same<typename vex::template rebind<char>, expected_t<TypeParam, char, long>>::value, "");
}

TYPED_TEST(test_pf23_expected, special_members_trivial)
{
    using ex = expected_t<TypeParam, int, long>;
    static_assert(std::is_trivially_copy_constructible<ex>::value, "");
    static_assert(std::is_trivially_move_constructible<ex>::value, "");
    static_assert(std::is_trivially_destructible<ex>::value, "");
    static_assert(std::is_nothrow_move_constructible<ex>::value, "");
    static_assert(std::is_copy_assignable<ex>::value, "");
    static_assert(std::is_nothrow_move_assignable<ex>::value, "");
    using vex = expected_t<TypeParam, void, long>;
    static_assert(std::is_trivially_copy_constructible<vex>::value, "");
    static_assert(std::is_trivially_move_constructible<vex>::value, "");
    static_assert(std::is_trivially_destructible<vex>::value, "");
    static_assert(std::is_nothrow_default_constructible<vex>::value, "");
}

TYPED_TEST(test_pf23_expected, special_members_non_trivial)
{
    using ex = expected_t<TypeParam, std::string, int>;
    static_assert(!std::is_trivially_copy_constructible<ex>::value, "");
    static_assert(!std::is_trivially_move_constructible<ex>::value, "");
    static_assert(!std::is_trivially_destructible<ex>::value, "");
    static_assert(std::is_copy_constructible<ex>::value, "");
    static_assert(std::is_nothrow_move_constructible<ex>::value, "");
    static_assert(std::is_copy_assignable<ex>::value, "");
    static_assert(std::is_move_assignable<ex>::value, "");
    using rex = expected_t<TypeParam, int, std::string>;
    static_assert(!std::is_trivially_destructible<rex>::value, "");
    static_assert(std::is_copy_assignable<rex>::value, "");
    using vex = expected_t<TypeParam, void, std::string>;
    static_assert(!std::is_trivially_copy_constructible<vex>::value, "");
    static_assert(!std::is_trivially_destructible<vex>::value, "");
    static_assert(std::is_copy_assignable<vex>::value, "");
}

TYPED_TEST(test_pf23_expected, special_members_move_only)
{
    using ex = expected_t<TypeParam, move_only, int>;
    static_assert(!std::is_copy_constructible<ex>::value, "");
    static_assert(std::is_move_constructible<ex>::value, "");
    static_assert(!std::is_copy_assignable<ex>::value, "");
    static_assert(std::is_move_assignable<ex>::value, "");
    using rex = expected_t<TypeParam, int, move_only>;
    static_assert(!std::is_copy_constructible<rex>::value, "");
    static_assert(std::is_move_constructible<rex>::value, "");
    static_assert(!std::is_copy_assignable<rex>::value, "");
    static_assert(std::is_move_assignable<rex>::value, "");
    using vex = expected_t<TypeParam, void, move_only>;
    static_assert(!std::is_copy_constructible<vex>::value, "");
    static_assert(std::is_move_constructible<vex>::value, "");
    static_assert(!std::is_copy_assignable<vex>::value, "");
    static_assert(std::is_move_assignable<vex>::value, "");
}

TYPED_TEST(test_pf23_expected, special_members_copy_only)
{
    // The move constructor does not take part in overload resolution, so rvalues are copied.
    using ex = expected_t<TypeParam, copy_only, int>;
    static_assert(std::is_copy_constructible<ex>::value, "");
    static_assert(std::is_move_constructible<ex>::value, "");
    static_assert(std::is_copy_assignable<ex>::value, "");
    static_assert(std::is_move_assignable<ex>::value, "");
}

TYPED_TEST(test_pf23_expected, special_members_non_movable)
{
    using ex = expected_t<TypeParam, non_movable, int>;
    static_assert(!std::is_copy_constructible<ex>::value, "");
    static_assert(!std::is_move_constructible<ex>::value, "");
    static_assert(!std::is_copy_assignable<ex>::value, "");
    static_assert(!std::is_move_assignable<ex>::value, "");
    static_assert(std::is_default_constructible<ex>::value, "");
}

TYPED_TEST(test_pf23_expected, assignment_needs_one_nothrow_move)
{
    // Switching between the value and the error can give the strong exception guarantee only if one of them can be
    // moved without throwing.
    using neither = expected_t<TypeParam, maybe_throwing, maybe_throwing>;
    static_assert(std::is_copy_constructible<neither>::value, "");
    static_assert(!std::is_copy_assignable<neither>::value, "");
    static_assert(!std::is_move_assignable<neither>::value, "");
    using value_nothrow = expected_t<TypeParam, int, maybe_throwing>;
    static_assert(std::is_copy_assignable<value_nothrow>::value, "");
    static_assert(std::is_move_assignable<value_nothrow>::value, "");
    static_assert(!std::is_nothrow_move_assignable<value_nothrow>::value, "");
    using error_nothrow = expected_t<TypeParam, maybe_throwing, int>;
    static_assert(std::is_copy_assignable<error_nothrow>::value, "");
    static_assert(std::is_move_assignable<error_nothrow>::value, "");
    // Assigning a value or an error has the same requirement.
    static_assert(!std::is_assignable<neither&, maybe_throwing>::value, "");
    static_assert(!std::is_assignable<neither&, unexpected_t<TypeParam, maybe_throwing>>::value, "");
    static_assert(std::is_assignable<value_nothrow&, int>::value, "");
    static_assert(std::is_assignable<value_nothrow&, unexpected_t<TypeParam, maybe_throwing>>::value, "");
}

TYPED_TEST(test_pf23_expected, default_constructible)
{
    static_assert(std::is_default_constructible<expected_t<TypeParam, int, int>>::value, "");
    static_assert(!std::is_default_constructible<expected_t<TypeParam, no_default, int>>::value, "");
    static_assert(std::is_default_constructible<expected_t<TypeParam, int, no_default>>::value, "");
    static_assert(std::is_default_constructible<expected_t<TypeParam, void, no_default>>::value, "");
}

TYPED_TEST(test_pf23_expected, implicit_and_explicit_conversions)
{
    using implicit_ex = expected_t<TypeParam, implicit_from_int, implicit_from_int>;
    using explicit_ex = expected_t<TypeParam, explicit_from_int, explicit_from_int>;
    using int_ex      = expected_t<TypeParam, int, int>;
    using int_unex    = unexpected_t<TypeParam, int>;
    // From a value.
    static_assert(std::is_convertible<int, implicit_ex>::value, "");
    static_assert(!std::is_convertible<int, explicit_ex>::value, "");
    static_assert(std::is_constructible<explicit_ex, int>::value, "");
    // From an unexpected.
    static_assert(std::is_convertible<int_unex, implicit_ex>::value, "");
    static_assert(std::is_convertible<const int_unex&, implicit_ex>::value, "");
    static_assert(!std::is_convertible<int_unex, explicit_ex>::value, "");
    static_assert(!std::is_convertible<const int_unex&, explicit_ex>::value, "");
    static_assert(std::is_constructible<explicit_ex, int_unex>::value, "");
    static_assert(std::is_constructible<explicit_ex, const int_unex&>::value, "");
    // From another expected.
    static_assert(std::is_convertible<int_ex, implicit_ex>::value, "");
    static_assert(std::is_convertible<const int_ex&, implicit_ex>::value, "");
    static_assert(!std::is_convertible<int_ex, explicit_ex>::value, "");
    static_assert(!std::is_convertible<const int_ex&, explicit_ex>::value, "");
    static_assert(std::is_constructible<explicit_ex, int_ex>::value, "");
    static_assert(std::is_constructible<explicit_ex, const int_ex&>::value, "");
    // Explicit if either conversion is.
    using mixed_ex = expected_t<TypeParam, implicit_from_int, explicit_from_int>;
    static_assert(!std::is_convertible<int_ex, mixed_ex>::value, "");
    static_assert(std::is_constructible<mixed_ex, int_ex>::value, "");
    // Not from unrelated types.
    static_assert(!std::is_constructible<int_ex, std::string>::value, "");
    static_assert(!std::is_constructible<int_ex, unexpected_t<TypeParam, std::string>>::value, "");
    static_assert(!std::is_constructible<int_ex, expected_t<TypeParam, std::string, int>>::value, "");
    // A void expected converts only from another void expected.
    using void_ex = expected_t<TypeParam, void, implicit_from_int>;
    static_assert(std::is_convertible<expected_t<TypeParam, void, int>, void_ex>::value, "");
    static_assert(!std::is_constructible<void_ex, int_ex>::value, "");
    static_assert(!std::is_convertible<expected_t<TypeParam, void, int>,
                                       expected_t<TypeParam, void, explicit_from_int>>::value,
                  "");
    static_assert(std::is_constructible<expected_t<TypeParam, void, explicit_from_int>,
                                        expected_t<TypeParam, void, int>>::value,
                  "");
}

TYPED_TEST(test_pf23_expected, construction_from_tags_is_explicit)
{
    using ex = expected_t<TypeParam, int, int>;
    static_assert(!std::is_convertible<typename TypeParam::in_place_t, ex>::value, "");
    static_assert(!std::is_convertible<typename TypeParam::unexpect_t, ex>::value, "");
    static_assert(std::is_constructible<ex, typename TypeParam::in_place_t>::value, "");
    static_assert(std::is_constructible<ex, typename TypeParam::unexpect_t>::value, "");
    static_assert(std::is_constructible<ex, typename TypeParam::in_place_t, int>::value, "");
    static_assert(!std::is_constructible<ex, typename TypeParam::in_place_t, int, int>::value, "");
}

// +----------------------------------------------------------------------+
// | CONSTRUCTION
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected, default_ctor_value_initializes)
{
    const expected_t<TypeParam, int, int> ex;
    ASSERT_TRUE(ex.has_value());
    EXPECT_TRUE(static_cast<bool>(ex));
    EXPECT_EQ(0, *ex);
    const expected_t<TypeParam, value_counted, error_counted> counted_ex;
    EXPECT_EQ(1, value_counted::stats().value_ctor);
    EXPECT_EQ(0, error_counted::stats().value_ctor);
}

TYPED_TEST(test_pf23_expected, value_ctor)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    const value_counted lvalue{3};
    const ex            copied(lvalue);
    EXPECT_EQ(3, copied->value);
    EXPECT_EQ(1, value_counted::stats().copy_ctor);
    const ex moved(value_counted{4});
    EXPECT_EQ(4, moved->value);
    EXPECT_EQ(1, value_counted::stats().move_ctor);
    const expected_t<TypeParam, std::string, int> converted = "hello";
    EXPECT_EQ("hello", *converted);
    // Empty braces default-construct the value.
    const expected_t<TypeParam, std::vector<int>, int> braced = {};
    EXPECT_TRUE(braced.has_value());
}

TYPED_TEST(test_pf23_expected, unexpected_ctor)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    const unexpected_t<TypeParam, error_counted> lvalue{error_counted{5}};
    const ex                                     copied(lvalue);
    ASSERT_FALSE(copied.has_value());
    EXPECT_EQ(5, copied.error().value);
    EXPECT_EQ(1, error_counted::stats().copy_ctor);
    const ex moved(unexpected_t<TypeParam, error_counted>{error_counted{6}});
    ASSERT_FALSE(moved.has_value());
    EXPECT_EQ(6, moved.error().value);
    EXPECT_EQ(0, value_counted::stats().value_ctor);
    // The error converts.
    const expected_t<TypeParam, int, long> converted = unexpected_t<TypeParam, int>(7);
    EXPECT_EQ(7L, converted.error());
}

TYPED_TEST(test_pf23_expected, in_place_ctors)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    const ex none(TypeParam::in_place());
    EXPECT_EQ(0, none->value);
    const ex two(TypeParam::in_place(), 2, 3);
    EXPECT_EQ(5, two->value);
    const ex il(TypeParam::in_place(), {1, 2, 3}, 4);
    EXPECT_EQ(7, il->value);
    EXPECT_EQ(3, value_counted::stats().value_ctor);
    EXPECT_EQ(0, value_counted::stats().copy_ctor + value_counted::stats().move_ctor);
    const expected_t<TypeParam, std::vector<int>, int> vec(TypeParam::in_place(), {1, 2, 3});
    EXPECT_EQ(3U, vec->size());
}

TYPED_TEST(test_pf23_expected, unexpect_ctors)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    const ex none(TypeParam::unexpect());
    ASSERT_FALSE(none.has_value());
    EXPECT_EQ(0, none.error().value);
    const ex two(TypeParam::unexpect(), 2, 3);
    EXPECT_EQ(5, two.error().value);
    const ex il(TypeParam::unexpect(), {1, 2}, 4);
    EXPECT_EQ(6, il.error().value);
    EXPECT_EQ(3, error_counted::stats().value_ctor);
    EXPECT_EQ(0, value_counted::stats().value_ctor);
    using vex = expected_t<TypeParam, void, error_counted>;
    const vex vil(TypeParam::unexpect(), {1, 2, 3}, 1);
    EXPECT_EQ(4, vil.error().value);
}

TYPED_TEST(test_pf23_expected, copy_and_move_ctors)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    ex       value_src(TypeParam::in_place(), 1);
    ex       error_src(TypeParam::unexpect(), 2);
    const ex value_copy(value_src);
    const ex error_copy(error_src);
    EXPECT_EQ(1, value_copy->value);
    EXPECT_EQ(2, error_copy.error().value);
    EXPECT_EQ(1, value_counted::stats().copy_ctor);
    EXPECT_EQ(1, error_counted::stats().copy_ctor);
    const ex value_move(std::move(value_src));
    const ex error_move(std::move(error_src));
    EXPECT_EQ(1, value_move->value);
    EXPECT_EQ(2, error_move.error().value);
    EXPECT_EQ(1, value_counted::stats().move_ctor);
    EXPECT_EQ(1, error_counted::stats().move_ctor);
    // A moved-from expected keeps its state.
    EXPECT_TRUE(value_src.has_value());
    EXPECT_EQ(-1, value_src->value);
    EXPECT_FALSE(error_src.has_value());
    EXPECT_EQ(-1, error_src.error().value);
}

TYPED_TEST(test_pf23_expected, converting_ctors)
{
    using src = expected_t<TypeParam, int, int>;
    using dst = expected_t<TypeParam, long, long long>;
    const src value_src(3);
    const src error_src(TypeParam::unexpect(), 4);
    const dst value_dst = value_src;
    const dst error_dst = error_src;
    EXPECT_EQ(3L, *value_dst);
    EXPECT_EQ(4LL, error_dst.error());
    const expected_t<TypeParam, explicit_from_int, explicit_from_int> explicit_value(value_src);
    const expected_t<TypeParam, explicit_from_int, explicit_from_int> explicit_error(error_src);
    EXPECT_EQ(3, explicit_value->value);
    EXPECT_EQ(4, explicit_error.error().value);
    // Rvalues are moved.
    using move_src = expected_t<TypeParam, std::unique_ptr<int>, std::unique_ptr<int>>;
    using move_dst = expected_t<TypeParam, std::shared_ptr<int>, std::shared_ptr<const int>>;
    move_src       value_ptr(TypeParam::in_place(), new int{5});
    move_src       error_ptr(TypeParam::unexpect(), new int{6});
    const move_dst value_shared = std::move(value_ptr);
    const move_dst error_shared = std::move(error_ptr);
    EXPECT_EQ(5, **value_shared);
    EXPECT_EQ(6, *error_shared.error());
    EXPECT_EQ(nullptr, value_ptr->get());
    EXPECT_EQ(nullptr, error_ptr.error().get());
}

TYPED_TEST(test_pf23_expected, converting_ctors_void)
{
    using src = expected_t<TypeParam, void, int>;
    using dst = expected_t<TypeParam, void, long>;
    const src value_src;
    const src error_src(TypeParam::unexpect(), 4);
    const dst value_dst = value_src;
    const dst error_dst = error_src;
    EXPECT_TRUE(value_dst.has_value());
    EXPECT_EQ(4L, error_dst.error());
    expected_t<TypeParam, void, std::unique_ptr<int>>       ptr(TypeParam::unexpect(), new int{7});
    const expected_t<TypeParam, void, std::shared_ptr<int>> shared(std::move(ptr));
    EXPECT_EQ(7, *shared.error());
}

#if CETLVAST_HAS_STD_EXPECTED
/// True if the standard library implements LWG 3836 (libstdc++ 13 does not; libstdc++ 15 and libc++ 23 do).
bool std_implements_lwg3836()
{
    const std::expected<int, int>  zero(0);
    const std::expected<bool, int> converted(zero);
    return !*converted;
}
#endif

TYPED_TEST(test_pf23_expected, bool_value_does_not_convert_from_expected)
{
    // An expected<bool, E> made from another expected copies its value rather than testing whether it has one
    // (LWG 3836).
#if CETLVAST_HAS_STD_EXPECTED
    if (std::is_same<TypeParam, std_impl>::value && !std_implements_lwg3836())
    {
        GTEST_SKIP() << "This standard library predates LWG 3836.";
    }
#endif
    using bool_ex = expected_t<TypeParam, bool, int>;
    const expected_t<TypeParam, int, int> zero(0);
    const bool_ex                         converted(zero);
    ASSERT_TRUE(converted.has_value());
    EXPECT_FALSE(*converted);
    const expected_t<TypeParam, int, int> error(TypeParam::unexpect(), 1);
    const bool_ex                         converted_error(error);
    ASSERT_FALSE(converted_error.has_value());
    EXPECT_EQ(1, converted_error.error());
}

TYPED_TEST(test_pf23_expected, nested_expected)
{
    using inner = expected_t<TypeParam, int, int>;
    using outer = expected_t<TypeParam, inner, int>;
    const outer value(inner(TypeParam::unexpect(), 1));
    ASSERT_TRUE(value.has_value());
    EXPECT_FALSE(value->has_value());
    EXPECT_EQ(1, value->error());
    const outer error(TypeParam::unexpect(), 2);
    EXPECT_FALSE(error.has_value());
}

TYPED_TEST(test_pf23_expected, const_value_type)
{
    using ex = expected_t<TypeParam, const int, int>;
    const ex value(3);
    EXPECT_EQ(3, *value);
    const ex copy(value);
    EXPECT_EQ(3, *copy);
    static_assert(!std::is_copy_assignable<ex>::value, "");
}

// +----------------------------------------------------------------------+
// | ASSIGNMENT
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected, copy_assignment)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    const ex value_src(TypeParam::in_place(), 1);
    const ex error_src(TypeParam::unexpect(), 2);
    {
        ex dst(TypeParam::in_place(), 3);
        dst = value_src;  // value <- value assigns the value
        EXPECT_EQ(1, dst->value);
        EXPECT_EQ(1, value_counted::stats().copy_assign);
    }
    {
        ex dst(TypeParam::unexpect(), 3);
        dst = error_src;  // error <- error assigns the error
        EXPECT_EQ(2, dst.error().value);
        EXPECT_EQ(1, error_counted::stats().copy_assign);
    }
    {
        ex dst(TypeParam::in_place(), 3);
        dst = error_src;  // value <- error destroys the value and constructs the error
        ASSERT_FALSE(dst.has_value());
        EXPECT_EQ(2, dst.error().value);
    }
    {
        ex dst(TypeParam::unexpect(), 3);
        dst = value_src;  // error <- value destroys the error and constructs the value
        ASSERT_TRUE(dst.has_value());
        EXPECT_EQ(1, dst->value);
    }
    {
        ex        dst(TypeParam::in_place(), 3);
        const ex& self = dst;
        dst            = self;
        EXPECT_EQ(3, dst->value);
    }
}

TYPED_TEST(test_pf23_expected, move_assignment)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    {
        ex dst(TypeParam::in_place(), 3);
        ex src(TypeParam::in_place(), 1);
        dst = std::move(src);
        EXPECT_EQ(1, dst->value);
        EXPECT_EQ(-1, src->value);
        EXPECT_EQ(1, value_counted::stats().move_assign);
    }
    {
        ex dst(TypeParam::unexpect(), 3);
        ex src(TypeParam::unexpect(), 2);
        dst = std::move(src);
        EXPECT_EQ(2, dst.error().value);
        EXPECT_EQ(-1, src.error().value);
        EXPECT_EQ(1, error_counted::stats().move_assign);
    }
    {
        ex dst(TypeParam::in_place(), 3);
        ex src(TypeParam::unexpect(), 2);
        dst = std::move(src);
        ASSERT_FALSE(dst.has_value());
        EXPECT_EQ(2, dst.error().value);
        EXPECT_FALSE(src.has_value());
    }
    {
        ex dst(TypeParam::unexpect(), 3);
        ex src(TypeParam::in_place(), 1);
        dst = std::move(src);
        ASSERT_TRUE(dst.has_value());
        EXPECT_EQ(1, dst->value);
        EXPECT_TRUE(src.has_value());
    }
    {
        expected_t<TypeParam, move_only, int> dst(TypeParam::unexpect(), 1);
        dst = expected_t<TypeParam, move_only, int>(TypeParam::in_place(), 4);
        EXPECT_EQ(4, dst->value);
    }
}

TYPED_TEST(test_pf23_expected, value_assignment)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    ex ex1(TypeParam::in_place(), 3);
    ex1 = value_counted{1};
    EXPECT_EQ(1, ex1->value);
    EXPECT_EQ(1, value_counted::stats().move_assign);
    ex ex2(TypeParam::unexpect(), 3);
    ex2 = value_counted{2};
    ASSERT_TRUE(ex2.has_value());
    EXPECT_EQ(2, ex2->value);
    expected_t<TypeParam, std::string, int> str(TypeParam::unexpect(), 1);
    str = "converted";
    EXPECT_EQ("converted", *str);
}

TYPED_TEST(test_pf23_expected, unexpected_assignment)
{
    using ex   = expected_t<TypeParam, value_counted, error_counted>;
    using unex = unexpected_t<TypeParam, error_counted>;
    const unex e1(error_counted{1});
    ex         ex1(TypeParam::in_place(), 3);
    ex1 = e1;
    ASSERT_FALSE(ex1.has_value());
    EXPECT_EQ(1, ex1.error().value);
    ex1 = unex(error_counted{2});
    EXPECT_EQ(2, ex1.error().value);
    EXPECT_EQ(1, error_counted::stats().move_assign);
    expected_t<TypeParam, int, long> converted(1);
    converted = unexpected_t<TypeParam, int>(5);
    EXPECT_EQ(5L, converted.error());
}

TYPED_TEST(test_pf23_expected, void_assignment)
{
    using vex  = expected_t<TypeParam, void, error_counted>;
    using unex = unexpected_t<TypeParam, error_counted>;
    vex       value;
    const vex error(TypeParam::unexpect(), 1);
    value = error;
    ASSERT_FALSE(value.has_value());
    EXPECT_EQ(1, value.error().value);
    value = vex{};
    EXPECT_TRUE(value.has_value());
    value = unex(error_counted{2});
    EXPECT_EQ(2, value.error().value);
    // The void stand-in is not moved through a temporary: the error is constructed directly.
    EXPECT_EQ(0, error_counted::stats().copy_assign + error_counted::stats().move_assign);
    value = unex(error_counted{3});
    EXPECT_EQ(3, value.error().value);
    EXPECT_EQ(1, error_counted::stats().move_assign);
    vex other(TypeParam::unexpect(), 4);
    value = std::move(other);
    EXPECT_EQ(4, value.error().value);
}

// +----------------------------------------------------------------------+
// | EMPLACE
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected, emplace)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    ex    from_value(TypeParam::in_place(), 1);
    auto& ref = from_value.emplace(2, 3);
    EXPECT_EQ(5, ref.value);
    EXPECT_EQ(&ref, &*from_value);
    ex from_error(TypeParam::unexpect(), 1);
    from_error.emplace({1, 2}, 3);
    ASSERT_TRUE(from_error.has_value());
    EXPECT_EQ(5, from_error->value);
    EXPECT_EQ(1, error_counted::stats().dtor);
    static_assert(noexcept(from_value.emplace(1)), "");

    using vex = expected_t<TypeParam, void, error_counted>;
    vex vvalue;
    vvalue.emplace();
    EXPECT_TRUE(vvalue.has_value());
    vex verror(TypeParam::unexpect(), 1);
    verror.emplace();
    EXPECT_TRUE(verror.has_value());
}

// +----------------------------------------------------------------------+
// | SWAP
// +----------------------------------------------------------------------+

TYPED_TEST(test_pf23_expected, swap)
{
    using ex = expected_t<TypeParam, value_counted, error_counted>;
    ex a(TypeParam::in_place(), 1);
    ex b(TypeParam::in_place(), 2);
    a.swap(b);
    EXPECT_EQ(2, a->value);
    EXPECT_EQ(1, b->value);
    ex c(TypeParam::unexpect(), 3);
    ex d(TypeParam::unexpect(), 4);
    swap(c, d);
    EXPECT_EQ(4, c.error().value);
    EXPECT_EQ(3, d.error().value);
    a.swap(c);
    ASSERT_FALSE(a.has_value());
    ASSERT_TRUE(c.has_value());
    EXPECT_EQ(4, a.error().value);
    EXPECT_EQ(2, c->value);
    swap(a, c);
    ASSERT_TRUE(a.has_value());
    ASSERT_FALSE(c.has_value());
    EXPECT_EQ(2, a->value);
    EXPECT_EQ(4, c.error().value);
    static_assert(noexcept(a.swap(b)), "");
    static_assert(cetl::pf17::is_swappable<ex>::value, "");
}

TYPED_TEST(test_pf23_expected, swap_value_with_error_not_nothrow)
{
    // E is not nothrow move constructible, so the value is set aside instead.
    using ex = expected_t<TypeParam, int, potentially_throwing_int>;
    static_assert(!std::is_nothrow_move_constructible<potentially_throwing_int>::value, "");
    ex a(1);
    ex b(TypeParam::unexpect(), 2);
    swap(a, b);
    ASSERT_FALSE(a.has_value());
    ASSERT_TRUE(b.has_value());
    EXPECT_EQ(2, a.error().value);
    EXPECT_EQ(1, *b);
    swap(a, b);
    EXPECT_EQ(1, *a);
    EXPECT_EQ(2, b.error().value);
}

TYPED_TEST(test_pf23_expected, swap_void)
{
    using vex = expected_t<TypeParam, void, error_counted>;
    vex a;
    vex b(TypeParam::unexpect(), 2);
    a.swap(b);
    ASSERT_FALSE(a.has_value());
    EXPECT_TRUE(b.has_value());
    EXPECT_EQ(2, a.error().value);
    swap(a, b);
    EXPECT_TRUE(a.has_value());
    ASSERT_FALSE(b.has_value());
    EXPECT_EQ(2, b.error().value);
    vex c(TypeParam::unexpect(), 3);
    b.swap(c);
    EXPECT_EQ(3, b.error().value);
    EXPECT_EQ(2, c.error().value);
    vex d;
    a.swap(d);
    EXPECT_TRUE(a.has_value());
    EXPECT_TRUE(d.has_value());
}

TYPED_TEST(test_pf23_expected, swap_constraints)
{
    static_assert(cetl::pf17::is_swappable<expected_t<TypeParam, move_only, int>>::value, "");
    static_assert(!cetl::pf17::is_swappable<expected_t<TypeParam, non_movable, int>>::value, "");
    static_assert(!cetl::pf17::is_swappable<expected_t<TypeParam, void, non_movable>>::value, "");
    static_assert(!cetl::pf17::is_nothrow_swappable<expected_t<TypeParam, maybe_throwing, int>>::value, "");
}

// +----------------------------------------------------------------------+
// | EXCEPTION SAFETY
// +----------------------------------------------------------------------+
#if defined(__cpp_exceptions)

TYPED_TEST(test_pf23_expected, copy_assignment_strong_guarantee_via_temporary)
{
    // The new value cannot be copied without throwing but can be moved, so it is built in a temporary first.
    using ex = expected_t<TypeParam, throws_on_copy, int>;
    const ex src(TypeParam::in_place(), 1);
    ex       dst(TypeParam::unexpect(), 2);
    {
        const arm_guard<throws_on_copy> armed;
        EXPECT_THROW(dst = src, test_exception);
    }
    ASSERT_FALSE(dst.has_value());
    EXPECT_EQ(2, dst.error());
    dst = src;
    EXPECT_EQ(1, dst->value);
}

TYPED_TEST(test_pf23_expected, copy_assignment_strong_guarantee_via_backup)
{
    // The new value cannot be copied or moved without throwing, so the old error is set aside and restored.
    using ex = expected_t<TypeParam, throws_on_copy_and_move, std::string>;
    const ex src(TypeParam::in_place(), 1);
    ex       dst(TypeParam::unexpect(), "error");
    {
        const arm_guard<throws_on_copy_and_move> armed;
        EXPECT_THROW(dst = src, test_exception);
    }
    ASSERT_FALSE(dst.has_value());
    EXPECT_EQ("error", dst.error());
    // The same with the roles swapped: the old value is set aside and restored.
    using rex = expected_t<TypeParam, std::string, throws_on_copy_and_move>;
    const rex error_src(TypeParam::unexpect(), 1);
    rex       value_dst(TypeParam::in_place(), "value");
    {
        const arm_guard<throws_on_copy_and_move> armed;
        EXPECT_THROW(value_dst = error_src, test_exception);
    }
    ASSERT_TRUE(value_dst.has_value());
    EXPECT_EQ("value", *value_dst);
}

TYPED_TEST(test_pf23_expected, value_and_unexpected_assignment_strong_guarantee)
{
    using ex = expected_t<TypeParam, throws_on_copy, throws_on_copy>;
    const throws_on_copy                          value{1};
    const unexpected_t<TypeParam, throws_on_copy> error{throws_on_copy{2}};
    ex                                            dst(TypeParam::unexpect(), 3);
    {
        const arm_guard<throws_on_copy> armed;
        EXPECT_THROW(dst = value, test_exception);
    }
    ASSERT_FALSE(dst.has_value());
    EXPECT_EQ(3, dst.error().value);
    dst = value;
    {
        const arm_guard<throws_on_copy> armed;
        EXPECT_THROW(dst = error, test_exception);
    }
    ASSERT_TRUE(dst.has_value());
    EXPECT_EQ(1, dst->value);
}

TYPED_TEST(test_pf23_expected, swap_strong_guarantee)
{
    // E is nothrow move constructible: it is set aside, and restored when moving the value throws.
    using ex = expected_t<TypeParam, throws_on_copy_and_move, std::string>;
    ex a(TypeParam::in_place(), 1);
    ex b(TypeParam::unexpect(), "error");
    {
        const arm_guard<throws_on_copy_and_move> armed;
        EXPECT_THROW(a.swap(b), test_exception);
    }
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ(1, a->value);
    ASSERT_FALSE(b.has_value());
    EXPECT_EQ("error", b.error());
}

TYPED_TEST(test_pf23_expected, swap_strong_guarantee_value_set_aside)
{
    using ex = expected_t<TypeParam, std::string, throws_on_copy_and_move>;
    ex a(TypeParam::in_place(), "value");
    ex b(TypeParam::unexpect(), 1);
    {
        const arm_guard<throws_on_copy_and_move> armed;
        EXPECT_THROW(swap(a, b), test_exception);
    }
    ASSERT_TRUE(a.has_value());
    EXPECT_EQ("value", *a);
    ASSERT_FALSE(b.has_value());
    EXPECT_EQ(1, b.error().value);
    swap(a, b);
    EXPECT_EQ(1, a.error().value);
    EXPECT_EQ("value", *b);
}

TYPED_TEST(test_pf23_expected, constructor_exception_propagates)
{
    using ex = expected_t<TypeParam, throws_on_copy, throws_on_copy>;
    const ex                        value(TypeParam::in_place(), 1);
    const ex                        error(TypeParam::unexpect(), 2);
    const arm_guard<throws_on_copy> armed;
    EXPECT_THROW((ex{value}), test_exception);
    EXPECT_THROW((ex{error}), test_exception);
    const throws_on_copy v{3};
    EXPECT_THROW((ex{v}), test_exception);
}

#endif  // __cpp_exceptions

// +----------------------------------------------------------------------+
// | POLYFILL-SPECIFIC
// +----------------------------------------------------------------------+

TEST(test_pf23_expected_cetl, trivial_assignment)
{
    // The standard leaves the triviality of assignment unspecified; the polyfill makes it trivial when it can be.
    using ex = cetl::pf23::expected<int, long>;
    static_assert(std::is_trivially_copy_assignable<ex>::value, "");
    static_assert(std::is_trivially_move_assignable<ex>::value, "");
    static_assert(std::is_trivially_copyable<ex>::value, "");
    using vex = cetl::pf23::expected<void, long>;
    static_assert(std::is_trivially_copyable<vex>::value, "");
    static_assert(!std::is_trivially_copy_assignable<cetl::pf23::expected<std::string, long>>::value, "");
}

TEST(test_pf23_expected_cetl, constexpr_construction_and_observers)
{
    using ex = cetl::pf23::expected<int, long>;
    constexpr ex value{3};
    static_assert(value.has_value(), "");
    static_assert(static_cast<bool>(value), "");
    static_assert(*value == 3, "");
    static_assert(value.value() == 3, "");
    static_assert(value.value_or(4) == 3, "");
    static_assert(value.error_or(5L) == 5L, "");
    constexpr ex error{cetl::pf23::unexpect, 6L};
    static_assert(!error.has_value(), "");
    static_assert(error.error() == 6L, "");
    static_assert(error.value_or(4) == 4, "");
    static_assert(error.error_or(5L) == 6L, "");
    constexpr ex copy{value};
    static_assert(*copy == 3, "");
    constexpr cetl::pf23::expected<void, long> void_value{};
    static_assert(void_value.has_value(), "");
    constexpr cetl::pf23::unexpected<int> unex{7};
    static_assert(unex.error() == 7, "");
    constexpr cetl::pf23::expected<int, long> from_unex{unex};
    static_assert(from_unex.error() == 7L, "");
    SUCCEED();
}

}  // namespace

// NOLINTEND(*-use-after-move)
