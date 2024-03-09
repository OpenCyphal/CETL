/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
// CSpell: words chronomorphize

#include <cetl/pf17/variant.hpp>  // The tested header always goes first.
#include "test_pf17_variant.hpp"
#include <cetlvast/helpers.hpp>

namespace cetlvast
{
namespace unittest
{
namespace pf17
{
namespace variant
{
namespace test_detail_types
{
using cetlvast::smf_policies::copy_ctor_policy;
using cetlvast::smf_policies::move_ctor_policy;
using cetlvast::smf_policies::copy_assignment_policy;
using cetlvast::smf_policies::move_assignment_policy;
using cetlvast::smf_policies::dtor_policy;
using cetlvast::smf_policies::policy_deleted;
using cetlvast::smf_policies::policy_trivial;
using cetlvast::smf_policies::policy_nontrivial;
using cetl::pf17::detail::var::types;
using cetl::pf17::detail::var::smf_deleted;
using cetl::pf17::detail::var::smf_trivial;
using cetl::pf17::detail::var::smf_nontrivial;

static_assert(types<copy_ctor_policy<policy_deleted>>::avail_copy_ctor == smf_deleted, "");
static_assert(types<copy_ctor_policy<policy_trivial>>::avail_copy_ctor == smf_trivial, "");
static_assert(types<copy_ctor_policy<policy_nontrivial>>::avail_copy_ctor == smf_nontrivial, "");

static_assert(types<move_ctor_policy<policy_deleted>>::avail_move_ctor == smf_deleted, "");
static_assert(types<move_ctor_policy<policy_trivial>>::avail_move_ctor == smf_trivial, "");
static_assert(types<move_ctor_policy<policy_nontrivial>>::avail_move_ctor == smf_nontrivial, "");

static_assert(types<copy_assignment_policy<policy_deleted>>::avail_copy_assign == smf_deleted, "");
static_assert(types<copy_assignment_policy<policy_trivial>>::avail_copy_assign == smf_trivial, "");
static_assert(types<copy_assignment_policy<policy_nontrivial>>::avail_copy_assign == smf_nontrivial, "");

static_assert(types<move_assignment_policy<policy_deleted>>::avail_move_assign == smf_deleted, "");
static_assert(types<move_assignment_policy<policy_trivial>>::avail_move_assign == smf_trivial, "");
static_assert(types<move_assignment_policy<policy_nontrivial>>::avail_move_assign == smf_nontrivial, "");

static_assert(types<dtor_policy<policy_deleted>>::avail_dtor == smf_deleted, "");
static_assert(types<dtor_policy<policy_trivial>>::avail_dtor == smf_trivial, "");
static_assert(types<dtor_policy<policy_nontrivial>>::avail_dtor == smf_nontrivial, "");
}  // namespace test_detail_types

// --------------------------------------------------------------------------------------------

namespace test_variant_alternative
{
using cetl::pf17::variant;
using cetl::pf17::variant_alternative_t;
using cetl::pf17::monostate;

static_assert(std::is_same<int, variant_alternative_t<0, variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<char, variant_alternative_t<1, variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<monostate, variant_alternative_t<2, variant<int, char, monostate>>>::value, "");

static_assert(std::is_same<const int, variant_alternative_t<0, const variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<const char, variant_alternative_t<1, const variant<int, char, monostate>>>::value, "");
static_assert(std::is_same<const monostate, variant_alternative_t<2, const variant<int, char, monostate>>>::value, "");

static_assert(std::is_same<int*, variant_alternative_t<0, variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<char*, variant_alternative_t<1, variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<monostate*, variant_alternative_t<2, variant<int*, char*, monostate*>>>::value, "");

static_assert(std::is_same<int* const, variant_alternative_t<0, const variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<char* const, variant_alternative_t<1, const variant<int*, char*, monostate*>>>::value, "");
static_assert(std::is_same<monostate* const, variant_alternative_t<2, const variant<int*, char*, monostate*>>>::value,
              "");
}  // namespace test_variant_alternative

// --------------------------------------------------------------------------------------------

namespace test_variant_size
{
using cetl::pf17::variant;
using cetl::pf17::variant_size_v;
static_assert(variant_size_v<variant<int>> == 1, "");
static_assert(variant_size_v<const variant<double>> == 1, "");
static_assert(variant_size_v<variant<int, char, double>> == 3, "");
static_assert(variant_size_v<const variant<int, char, double>> == 3, "");

// Well, this is also size...
using cetl::pf17::monostate;
static_assert(sizeof(variant<char>) == (2 * sizeof(std::size_t)), "");  // mind the index field alignment
static_assert(sizeof(variant<std::size_t, char>) == (2 * sizeof(std::size_t)), "");
static_assert(sizeof(variant<std::size_t, monostate>) == (2 * sizeof(std::size_t)), "");
static_assert(sizeof(variant<std::size_t, monostate, std::int64_t>) == (sizeof(std::int64_t) + sizeof(std::size_t)),
              "");
}  // namespace test_variant_size

// --------------------------------------------------------------------------------------------

namespace test_smf_availability_basics
{
using cetl::pf17::variant;
using cetl::pf17::monostate;

struct restricted
{
    restricted()                             = default;
    restricted(const restricted&)            = delete;
    restricted(restricted&&)                 = delete;
    restricted& operator=(const restricted&) = delete;
    restricted& operator=(restricted&&)      = delete;
    ~restricted()
    {
        std::terminate(); /* a nontrivial dtor */
    }
};

static_assert(std::is_trivially_copy_constructible<variant<bool>>::value, "");
static_assert(std::is_trivially_move_constructible<variant<bool>>::value, "");
static_assert(std::is_trivially_copy_assignable<variant<bool>>::value, "");
static_assert(std::is_trivially_move_assignable<variant<bool>>::value, "");
static_assert(std::is_trivially_destructible<variant<bool>>::value, "");
static_assert(std::is_trivially_copyable<variant<bool>>::value, "");

static_assert(std::is_trivially_copy_constructible<variant<monostate>>::value, "");
static_assert(std::is_trivially_move_constructible<variant<monostate>>::value, "");
static_assert(std::is_trivially_copy_assignable<variant<monostate>>::value, "");
static_assert(std::is_trivially_move_assignable<variant<monostate>>::value, "");
static_assert(std::is_trivially_destructible<variant<monostate>>::value, "");
static_assert(std::is_trivially_copyable<variant<monostate>>::value, "");

static_assert(!std::is_trivially_copy_constructible<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_move_constructible<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_copy_assignable<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_move_assignable<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_destructible<variant<monostate, restricted>>::value, "");
static_assert(!std::is_trivially_copyable<variant<monostate, restricted>>::value, "");
}  // namespace test_smf_availability_basics

// --------------------------------------------------------------------------------------------

namespace test_noexcept_propagation
{
using cetl::pf17::variant;
using cetl::pf17::monostate;

// These are noexcept because they are all trivial.
// The variant specification does not require noexcept copy ctor/assignment if they are nontrivial.
// Noexcept move ctor/assignment, however, are possible depending on Ts.
static_assert(std::is_nothrow_copy_constructible<variant<monostate>>::value, "");
static_assert(std::is_nothrow_move_constructible<variant<monostate>>::value, "");
static_assert(std::is_nothrow_copy_assignable<variant<monostate>>::value, "");
static_assert(std::is_nothrow_move_assignable<variant<monostate>>::value, "");

// Copy is throwing but move is still noexcept.
struct throw_copy_ctor
{
    throw_copy_ctor() = default;
    throw_copy_ctor(const throw_copy_ctor&);
    throw_copy_ctor(throw_copy_ctor&&) noexcept;
    throw_copy_ctor& operator=(const throw_copy_ctor&) noexcept;
    throw_copy_ctor& operator=(throw_copy_ctor&&) noexcept;
};
static_assert(!std::is_nothrow_copy_constructible<variant<monostate, throw_copy_ctor>>::value, "");
static_assert(std::is_nothrow_move_constructible<variant<monostate, throw_copy_ctor>>::value, "");
static_assert(!std::is_nothrow_copy_assignable<variant<monostate, throw_copy_ctor>>::value, "");
static_assert(std::is_nothrow_move_assignable<variant<monostate, throw_copy_ctor>>::value, "");

// Move is throwing; copy is noexcept but this is not propagated to the variant (see the spec).
struct throw_move_ctor
{
    throw_move_ctor() = default;
    throw_move_ctor(const throw_move_ctor&) noexcept;
    throw_move_ctor(throw_move_ctor&&);  // NOLINT(*-noexcept-move-constructor)
    throw_move_ctor& operator=(const throw_move_ctor&) noexcept;
    throw_move_ctor& operator=(throw_move_ctor&&) noexcept;
};
static_assert(!std::is_nothrow_copy_constructible<variant<monostate, throw_move_ctor>>::value, "");  // nontrivial
static_assert(!std::is_nothrow_move_constructible<variant<monostate, throw_move_ctor>>::value, "");
static_assert(!std::is_nothrow_copy_assignable<variant<monostate, throw_move_ctor>>::value, "");
static_assert(!std::is_nothrow_move_assignable<variant<monostate, throw_move_ctor>>::value, "");

struct throw_copy_assignment
{
    throw_copy_assignment() = default;
    throw_copy_assignment(const throw_copy_assignment&) noexcept;
    throw_copy_assignment(throw_copy_assignment&&) noexcept;
    throw_copy_assignment& operator=(const throw_copy_assignment&);
    throw_copy_assignment& operator=(throw_copy_assignment&&) noexcept;
};
static_assert(!std::is_nothrow_copy_constructible<variant<monostate, throw_copy_assignment>>::value, "");  // nontrivial
static_assert(std::is_nothrow_move_constructible<variant<monostate, throw_copy_assignment>>::value, "");
static_assert(!std::is_nothrow_copy_assignable<variant<monostate, throw_copy_assignment>>::value, "");
static_assert(std::is_nothrow_move_assignable<variant<monostate, throw_copy_assignment>>::value, "");

struct throw_move_assignment
{
    throw_move_assignment() = default;
    throw_move_assignment(const throw_move_assignment&) noexcept;
    throw_move_assignment(throw_move_assignment&&) noexcept;
    throw_move_assignment& operator=(const throw_move_assignment&) noexcept;
    throw_move_assignment& operator=(throw_move_assignment&&);  // NOLINT(*-noexcept-move-constructor)
};
static_assert(!std::is_nothrow_copy_constructible<variant<monostate, throw_move_assignment>>::value, "");  // nontrivial
static_assert(std::is_nothrow_move_constructible<variant<monostate, throw_move_assignment>>::value, "");
static_assert(!std::is_nothrow_copy_assignable<variant<monostate, throw_move_assignment>>::value, "");
static_assert(!std::is_nothrow_move_assignable<variant<monostate, throw_move_assignment>>::value, "");

}  // namespace test_noexcept_propagation

// --------------------------------------------------------------------------------------------

namespace test_match_ctor
{
using cetl::pf17::detail::var::best_converting_ctor_index_v;
constexpr auto bad = std::numeric_limits<std::size_t>::max();

struct A
{};
struct B
{
    B(std::int8_t);  // NOLINT(*-explicit-constructor)
};
struct C
{
    C(double);  // NOLINT(*-explicit-constructor)
};

static_assert(best_converting_ctor_index_v<std::int8_t, A> == bad, "");
static_assert(best_converting_ctor_index_v<std::int8_t, A, B> == 1, "");

static_assert(best_converting_ctor_index_v<std::int8_t, A, B, C> == 1, "");
static_assert(best_converting_ctor_index_v<std::int8_t, C, B> == 1, "");
static_assert(best_converting_ctor_index_v<std::int8_t, B, C> == 0, "");
static_assert(best_converting_ctor_index_v<std::int8_t, A, B, C, B> == bad, "");  // not unique

// Ensure narrowing conversions are not considered; see https://en.cppreference.com/w/cpp/utility/variant/operator%3D
static_assert(best_converting_ctor_index_v<std::int32_t, A, B, C> == bad, "");
static_assert(best_converting_ctor_index_v<std::int32_t, C, B> == bad, "");
static_assert(best_converting_ctor_index_v<float, std::int32_t, float, double, bool> == 1, "");
static_assert(best_converting_ctor_index_v<double, std::int32_t, float, double, bool> == 2, "");
static_assert(best_converting_ctor_index_v<float, std::int32_t, long double, double, bool> == 2, "");
static_assert(best_converting_ctor_index_v<char, float, double, bool> == bad, "");  // not unique

static_assert(best_converting_ctor_index_v<int, int, bool> == 0, "");
static_assert(best_converting_ctor_index_v<bool, int, bool> == 1, "");
}  // namespace test_match_ctor

// --------------------------------------------------------------------------------------------

namespace test_match_assignment
{
using cetl::pf17::detail::var::best_converting_assignment_index_v;
constexpr auto bad = std::numeric_limits<std::size_t>::max();

struct A
{};
struct B
{
    B(std::int8_t);  // NOLINT(*-explicit-constructor)
    B& operator=(std::int8_t);
};
struct C
{
    C(double);  // NOLINT(*-explicit-constructor)
    C& operator=(double);
};

static_assert(best_converting_assignment_index_v<std::int8_t, A> == bad, "");
static_assert(best_converting_assignment_index_v<std::int8_t, A, B> == 1, "");

static_assert(best_converting_assignment_index_v<std::int8_t, A, B, C> == 1, "");
static_assert(best_converting_assignment_index_v<std::int8_t, C, B> == 1, "");
static_assert(best_converting_assignment_index_v<std::int8_t, B, C> == 0, "");
static_assert(best_converting_assignment_index_v<std::int8_t, A, B, C, B> == bad, "");  // not unique

// Ensure narrowing conversions are not considered; see https://en.cppreference.com/w/cpp/utility/variant/operator%3D
static_assert(best_converting_assignment_index_v<std::int32_t, A, B, C> == bad, "");
static_assert(best_converting_assignment_index_v<std::int8_t, A, B, C> == 1, "");
static_assert(best_converting_assignment_index_v<std::int32_t, C, B> == bad, "");
static_assert(best_converting_assignment_index_v<double, C, B> == 0, "");
static_assert(best_converting_assignment_index_v<float, std::int32_t, float, double, bool> == 1, "");
static_assert(best_converting_assignment_index_v<double, std::int32_t, float, double, bool> == 2, "");
static_assert(best_converting_assignment_index_v<float, std::int32_t, long double, double, bool> == 2, "");
static_assert(best_converting_assignment_index_v<char, float, double, bool> == bad, "");  // not unique

static_assert(best_converting_assignment_index_v<int, int, bool> == 0, "");
static_assert(best_converting_assignment_index_v<bool, int, bool> == 1, "");
}  // namespace test_match_assignment

// --------------------------------------------------------------------------------------------

TEST(test_variant, chronomorphize)
{
    using namespace cetl::pf17::detail::var;
    struct checker final
    {
        // clang-format off
        auto operator()(const std::integral_constant<std::size_t, 0> ix) { return check(decltype(ix)::value); }
        auto operator()(const std::integral_constant<std::size_t, 1> ix) { return check(decltype(ix)::value); }
        auto operator()(const std::integral_constant<std::size_t, 2> ix) { return check(decltype(ix)::value); }
        // clang-format on
        std::size_t check(const std::size_t value)
        {
            if ((!armed) || (value != expected_value))
            {
                std::terminate();
            }
            armed = false;
            return value;
        }
        std::size_t expected_value = 0;
        bool        armed          = false;
    };
    {
        checker chk{0, true};
        EXPECT_EQ(0, chronomorphize<3>(chk, 0));
        EXPECT_FALSE(chk.armed);
    }
    {
        checker chk{1, true};
        EXPECT_EQ(1, chronomorphize<3>(chk, 1));
        EXPECT_FALSE(chk.armed);
    }
    {
        checker chk{2, true};
        EXPECT_EQ(2, chronomorphize<3>(chk, 2));
        EXPECT_FALSE(chk.armed);
    }
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, monostate)
{
    using cetl::pf17::monostate;
    EXPECT_TRUE(monostate{} == monostate{});
    EXPECT_FALSE(monostate{} != monostate{});
    EXPECT_FALSE(monostate{} < monostate{});
    EXPECT_FALSE(monostate{} > monostate{});
    EXPECT_TRUE(monostate{} <= monostate{});
    EXPECT_TRUE(monostate{} >= monostate{});
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, arena)
{
    using cetl::pf17::detail::var::types;
    using cetl::pf17::detail::var::arena;
    using cetl::pf17::detail::var::alt;
    using cetl::pf17::detail::var::construct;
    struct anchored
    {
        explicit anchored(const std::int64_t val)
            : value(val)
        {
        }
        anchored(const anchored&)            = delete;
        anchored(anchored&&)                 = delete;
        anchored& operator=(const anchored&) = delete;
        anchored& operator=(anchored&&)      = delete;
        ~anchored()                          = default;
        std::int64_t value                   = 0;
    };
    using my_arena = arena<0, types<int, const char*, anchored>>;
    my_arena arn;
    static_assert(std::is_same<decltype(alt<0>(arn)), int&>::value, "");
    static_assert(std::is_same<decltype(alt<1>(arn)), const char*&>::value, "");
    static_assert(std::is_same<decltype(alt<2>(arn)), anchored&>::value, "");
    static_assert(std::is_same<decltype(alt<0>(static_cast<const my_arena&>(arn))), const int&>::value, "");
    static_assert(std::is_same<decltype(alt<1>(static_cast<const my_arena&>(arn))), const char* const&>::value, "");
    static_assert(std::is_same<decltype(alt<2>(static_cast<const my_arena&>(arn))), const anchored&>::value, "");
    static_assert(std::is_same<decltype(alt<0>(std::move(arn))), int&&>::value, "");  // NOLINT(*-move-const-arg)
    static_assert(std::is_same<decltype(alt<2>(std::move(arn))), anchored&&>::value, "");
    static_assert(std::is_same<decltype(alt<0>(
                                   std::move(static_cast<const my_arena&>(arn)))),  // NOLINT(*-move-const-arg)
                               const int&&>::value,
                  "");
    static_assert(std::is_same<decltype(alt<2>(std::move(static_cast<const my_arena&>(arn)))), const anchored&&>::value,
                  "");

    EXPECT_EQ(123, construct<0>(arn, 123));
    EXPECT_EQ(123, alt<0>(arn));
    EXPECT_STREQ("abc", construct<1>(arn, "abc"));
    EXPECT_STREQ("abc", alt<1>(arn));
    EXPECT_EQ(9876543210, construct<2>(arn, 9876543210).value);
    EXPECT_EQ(9876543210, alt<2>(arn).value);
}

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, smf_asserts)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;

    using T = TypeParam;

    // Enrich the variant with SMF-trivial types to ensure we always pick the most restrictive policy.
    using V = variant<int, T, monostate, T>;
    static_assert(sizeof(V) == (cetlvast::align_size_up(sizeof(T), sizeof(std::size_t)) + sizeof(std::size_t)), "");

    // Ensure trivial copy/move policies are correctly inherited from the value type.
    // copy ctor
    static_assert(std::is_copy_constructible<T>::value == std::is_copy_constructible<V>::value, "");
    static_assert(std::is_trivially_copy_constructible<T>::value == std::is_trivially_copy_constructible<V>::value, "");
    // move ctor
    static_assert(std::is_move_constructible<T>::value == std::is_move_constructible<V>::value, "");
    static_assert(std::is_trivially_move_constructible<T>::value == std::is_trivially_move_constructible<V>::value, "");
    // copy assign
    // We don't check is_trivially_copyable because this check operates on memory representation rather than
    // the availability of the corresponding SMFs. As such, the is_trivially_copyable can be true even if the
    // copy ctor is deleted.
    static_assert((std::is_copy_assignable<T>::value &&     //
                   std::is_copy_constructible<T>::value &&  //
                   std::is_destructible<T>::value) == std::is_copy_assignable<V>::value,
                  "");
    static_assert((std::is_trivially_copy_assignable<T>::value &&     //
                   std::is_trivially_copy_constructible<T>::value &&  //
                   std::is_trivially_destructible<T>::value) == std::is_trivially_copy_assignable<V>::value,
                  "");
    // move assign
    static_assert((std::is_move_assignable<T>::value &&     //
                   std::is_move_constructible<T>::value &&  //
                   std::is_destructible<T>::value) == std::is_move_assignable<V>::value,
                  "");
    static_assert((std::is_trivially_move_assignable<T>::value &&     //
                   std::is_trivially_move_constructible<T>::value &&  //
                   std::is_trivially_destructible<T>::value) == std::is_trivially_move_assignable<V>::value,
                  "");
    // dtor
    static_assert(std::is_trivially_destructible<T>::value == std::is_trivially_destructible<V>::value, "");
}

// --------------------------------------------------------------------------------------------

TYPED_TEST(test_smf_policy_combinations, emplace)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::get;
    using cetl::pf17::holds_alternative;
    struct T : TypeParam
    {
        explicit T(const std::int64_t val)
            : value(val)
        {
        }
        T(const std::initializer_list<std::int64_t> il, const std::int64_t val)
            : value(static_cast<std::int64_t>(il.size()) + val)
        {
        }
        std::int64_t value = 0;
    };
    variant<monostate, T, monostate, std::int64_t, std::int64_t> var;

    EXPECT_EQ(123456, var.template emplace<T>(123456).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(123456, get<T>(var).value);
    monostate* mono = &var.template emplace<0>();
    (void) mono;

    EXPECT_EQ(992, var.template emplace<T>(std::initializer_list<std::int64_t>{1, 2, 3, 4, 5}, 987).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(992, get<T>(var).value);
    mono = &var.template emplace<0>();
    (void) mono;

    EXPECT_EQ(123456, var.template emplace<1>(123456).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(123456, get<1>(var).value);
    mono = &var.template emplace<0>();
    (void) mono;

    EXPECT_EQ(992, var.template emplace<1>(std::initializer_list<std::int64_t>{1, 2, 3, 4, 5}, 987).value);
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_EQ(992, get<1>(var).value);
    mono = &var.template emplace<0>();
    (void) mono;
}

// --------------------------------------------------------------------------------------------

template <typename T, bool = cetl::pf17::is_swappable_v<T>>
struct test_swap;
template <typename T>
struct test_swap<T, true>
{
    static void test_noexcept()
    {
        using cetl::pf17::variant;
        using cetl::pf17::get;
        using cetl::pf17::in_place_type;
        struct U : T
        {
            explicit U(const std::int64_t val)
                : value(val)
            {
            }
            std::int64_t value = 0;
        };
        variant<U, std::int64_t> v1(in_place_type<U>, 123456);
        variant<U, std::int64_t> v2(in_place_type<U>, 987654);
        // For the case where both variants have the same active alternative we provide no exception safety
        // guarantee because these concerns are delegated to the ADL-selected swap implementation,
        // which we don't care about.
        EXPECT_EQ(123456, get<U>(v1).value);
        EXPECT_EQ(987654, get<U>(v2).value);
        v1.swap(v2);
        EXPECT_EQ(987654, get<U>(v1).value);
        EXPECT_EQ(123456, get<U>(v2).value);

        v1.template emplace<1>(147852);
        EXPECT_EQ(147852, get<1>(v1));
        EXPECT_EQ(123456, get<U>(v2).value);
        v2.swap(v1);
        EXPECT_EQ(123456, get<U>(v1).value);
        EXPECT_EQ(147852, get<1>(v2));
    }

    static void test_throwing()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::in_place_type;
        using cetl::pf17::in_place_index;
        using cetl::pf17::get;
        struct U : T
        {
            U()                  = default;
            U(const U&) noexcept = default;
            U(U&& other)                        // NOLINT(*-noexcept-move-constructor)
                : T(std::move(other))           // Note that this may get resolved to the copy ctor.
                , move_throw(other.move_throw)  // NOLINT(*-use-after-move)
            {
                if (move_throw)
                {
                    throw std::exception();
                }
            }
            U& operator=(const U&) noexcept = default;
            U& operator=(U&& other)  // NOLINT(*-noexcept-move-constructor)
            {
                if (move_throw || other.move_throw)
                {
                    throw std::exception();
                }
                T::operator=(std::move(other));
                move_throw = other.move_throw;  // NOLINT(*-use-after-move)
                return *this;
            }
            bool move_throw = false;
        };
        struct W : T
        {
            W()
            {
                throw std::exception();
            }
        };
        variant<U, std::int64_t, W> v1(in_place_type<U>);
        variant<U, std::int64_t, W> v2(in_place_index<1>);

        // Swap two distinct types.
        EXPECT_EQ(0, v1.index());
        EXPECT_EQ(1, v2.index());
        v1.swap(v2);
        EXPECT_EQ(1, v1.index());
        EXPECT_EQ(0, v2.index());

        // Induce valueless state in one of the operands.
        get<U>(v2).move_throw = true;
        EXPECT_ANY_THROW(v1.swap(v2));
        EXPECT_TRUE(v1.valueless_by_exception());
        EXPECT_FALSE(v2.valueless_by_exception());

        // Swap a normal with a valueless type.
        get<U>(v2).move_throw = false;
        v1.swap(v2);
        EXPECT_FALSE(v1.valueless_by_exception());
        EXPECT_TRUE(v2.valueless_by_exception());

        // Swap two valueless.
        EXPECT_ANY_THROW(v1.template emplace<W>());
        EXPECT_TRUE(v1.valueless_by_exception());
        EXPECT_TRUE(v2.valueless_by_exception());
        v1.swap(v2);
        EXPECT_TRUE(v1.valueless_by_exception());
        EXPECT_TRUE(v2.valueless_by_exception());
#endif
    }

    static void test()
    {
        test_noexcept();
        test_throwing();
    }
};
template <typename T>
struct test_swap<T, false>
{
    static_assert(!cetl::pf17::is_swappable_v<T>, "");
    static_assert(!cetl::pf17::is_swappable_v<cetl::pf17::variant<T>>, "");
    static void test() {}
};

TYPED_TEST(test_smf_policy_combinations, swap)
{
    test_swap<TypeParam>::test();
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, basic_operations)
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::visit;
    using cetl::pf17::holds_alternative;
    using cetl::pf17::get;
    using cetl::pf17::get_if;
    using cetl::pf17::make_overloaded;
    using cetl::pf17::in_place_index;

    variant<int, char, monostate> var;
    EXPECT_EQ(0, var.index());
    EXPECT_FALSE(var.valueless_by_exception());
    EXPECT_TRUE(holds_alternative<int>(var));
    EXPECT_FALSE(holds_alternative<char>(var));

    EXPECT_FALSE(get_if<char>(&var));
    EXPECT_FALSE(get_if<char>(static_cast<variant<int, char, monostate>*>(nullptr)));
    EXPECT_FALSE(get_if<char>(static_cast<const variant<int, char, monostate>*>(nullptr)));
    EXPECT_TRUE(get_if<int>(&var));
    *get_if<int>(&var) = 42;
    EXPECT_EQ(42, get<int>(var));
    EXPECT_EQ(42, *get_if<int>(&var));

    const auto& const_var = var;
    EXPECT_EQ(42, *get_if<int>(&const_var));
    EXPECT_EQ(42, get<int>(const_var));

    EXPECT_EQ(43,
              cetl::pf17::visit(make_overloaded([](const int arg) { return arg + 1; },
                                                [](const char arg) { return static_cast<int>(arg) + 2; },
                                                [](const monostate) {
                                                    std::terminate();
                                                    return '\0';
                                                }),
                                var));
    EXPECT_EQ(42 + 'a',
              cetl::pf17::visit(make_overloaded([](int, double) { return 0; },
                                                [](int a, char b) { return a + b; },
                                                [](char, double) { return 0; },
                                                [](char, char) { return 0; },
                                                [](monostate, double) { return 0; },
                                                [](monostate, char) { return 0; }),
                                var,
                                variant<double, char>{in_place_index<1>, 'a'}));
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, get)
{
    // NOLINTBEGIN(*-use-after-move)
    using cetl::pf17::variant;
    using cetl::pf17::in_place_index;
    using cetl::pf17::get;
    using cetl::pf17::get_if;
    using cetl::pf17::holds_alternative;
#if __cpp_exceptions
    using cetl::pf17::bad_variant_access;
#endif
    struct anchored
    {
        anchored()                           = default;
        anchored(const anchored&)            = delete;
        anchored(anchored&&)                 = delete;
        anchored& operator=(const anchored&) = delete;
        anchored& operator=(anchored&&)      = delete;
        ~anchored()                          = default;
    };
    struct T : anchored
    {
        explicit T(const std::int64_t val)
            : value(val)
        {
        }
        std::int64_t value = 0;
    };
    struct U : anchored
    {
        explicit U(const std::int16_t val)
            : value(val)
        {
        }
        std::int16_t value = 0;
    };

    using V = variant<T, U>;
    V var(in_place_index<0>, 123456);

    // holds_alternative
    EXPECT_TRUE(holds_alternative<T>(var));
    EXPECT_FALSE(holds_alternative<U>(var));

    // get<I>
    EXPECT_EQ(123456, get<0>(var).value);
    EXPECT_EQ(123456, get<0>(static_cast<const V&>(var)).value);
    EXPECT_EQ(123456, get<0>(std::move(var)).value);
    EXPECT_EQ(123456, get<0>(std::move(static_cast<const V&>(var))).value);
#if __cpp_exceptions
    EXPECT_THROW(([](U&) {})(get<1>(var)), bad_variant_access);
    EXPECT_THROW(([](const U&) {})(get<1>(static_cast<const V&>(var))), bad_variant_access);
    EXPECT_THROW(([](U&&) {})(get<1>(std::move(var))), bad_variant_access);
    EXPECT_THROW(([](const U&&) {})(get<1>(std::move(static_cast<const V&>(var)))), bad_variant_access);
#endif

    // get<T>
    EXPECT_EQ(123456, get<T>(var).value);
    EXPECT_EQ(123456, get<T>(static_cast<const V&>(var)).value);
    EXPECT_EQ(123456, get<T>(std::move(var)).value);
    EXPECT_EQ(123456, get<T>(std::move(static_cast<const V&>(var))).value);
#if __cpp_exceptions
    EXPECT_THROW(([](U&) {})(get<U>(var)), bad_variant_access);
    EXPECT_THROW(([](const U&) {})(get<U>(static_cast<const V&>(var))), bad_variant_access);
    EXPECT_THROW(([](U&&) {})(get<U>(std::move(var))), bad_variant_access);
    EXPECT_THROW(([](const U&&) {})(get<U>(std::move(static_cast<const V&>(var)))), bad_variant_access);
#endif

    // get_if<I>
    EXPECT_EQ(&get<0>(var), get_if<0>(&var));
    EXPECT_EQ(&get<0>(var), get_if<0>(static_cast<const V*>(&var)));
    EXPECT_EQ(nullptr, get_if<1>(&var));
    EXPECT_EQ(nullptr, get_if<1>(static_cast<const V*>(&var)));
    EXPECT_EQ(nullptr, get_if<0>(static_cast<V*>(nullptr)));
    EXPECT_EQ(nullptr, get_if<0>(static_cast<const V*>(nullptr)));

    // get_if<T>
    EXPECT_EQ(&get<T>(var), get_if<T>(&var));
    EXPECT_EQ(&get<T>(var), get_if<T>(static_cast<const V*>(&var)));
    EXPECT_EQ(nullptr, get_if<U>(&var));
    EXPECT_EQ(nullptr, get_if<U>(static_cast<const V*>(&var)));
    EXPECT_EQ(nullptr, get_if<T>(static_cast<V*>(nullptr)));
    EXPECT_EQ(nullptr, get_if<T>(static_cast<const V*>(nullptr)));
    // NOLINTEND(*-use-after-move)
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, visit)
{
    using cetl::pf17::variant;
    using cetl::pf17::in_place_index;
    using cetl::pf17::get;
    using cetl::pf17::visit;
    using cetl::pf17::monostate;
    using cetl::pf17::make_overloaded;
#if __cpp_exceptions
    using cetl::pf17::bad_variant_access;
#endif
    struct anchored
    {
        explicit anchored(const std::int64_t val)
            : value(val)
        {
        }
        anchored(const anchored&)               = delete;
        anchored(anchored&&)                    = delete;
        anchored&    operator=(const anchored&) = delete;
        anchored&    operator=(anchored&&)      = delete;
        std::int64_t value                      = 0;
    };

    // Visit const variants.
    EXPECT_EQ(123456LL + (987654LL * 147852LL),
              visit(make_overloaded(
                        [](const anchored& a, const std::int64_t& b, const anchored& c) {
                            return a.value + (b * c.value);  //
                        },
                        [](const auto&, const auto&, const auto&) {
                            std::terminate();
                            return 0;
                        }),
                    variant<anchored, std::int64_t, anchored>(in_place_index<0>, 123456),
                    variant<anchored, std::int64_t>(in_place_index<1>, 987654),
                    variant<std::int64_t, anchored>(in_place_index<1>, 147852)));

    // Visit mutable variants.
    variant<anchored, std::int64_t, anchored> a(in_place_index<2>, 654321);
    variant<std::int64_t, anchored>           b(in_place_index<0>, 1234);
    std::int64_t                              div = 0;
    visit(make_overloaded(
              [&div](anchored& a, std::int64_t& b) {
                  div = a.value / b;
                  std::swap(a.value, b);
              },
              [](const auto&, const auto&) { std::terminate(); }),
          a,
          b);
    EXPECT_EQ(530, div);
    EXPECT_EQ(1234, get<2>(a).value);
    EXPECT_EQ(654321, get<0>(b));

    // Special case: empty visitor.
    EXPECT_EQ(42, visit([]() { return 42; }));

    // Exception handling.
#if __cpp_exceptions
    struct panicky : anchored
    {
        panicky()
            : anchored(0)
        {
            throw std::exception();
        }
    };
    variant<monostate, anchored, panicky> var;
    EXPECT_ANY_THROW(var.emplace<panicky>());
    EXPECT_TRUE(var.valueless_by_exception());
    EXPECT_THROW(visit([](auto&&) {}, var), bad_variant_access);
#endif

    // Check result types.
    const auto ovd1 = make_overloaded([](const std::int64_t) -> std::int64_t { return {}; },
                                      [](const anchored&) -> const std::int32_t& {
                                          static std::int32_t x;
                                          return x;
                                      });
    static_assert(std::is_same<std::int64_t, decltype(visit(ovd1, variant<std::int64_t, anchored>()))>::value, "");

    const auto ovd2 = make_overloaded([](const std::int64_t) -> std::int64_t* { return nullptr; },
                                      [](const anchored&) -> const void* { return nullptr; });
    static_assert(std::is_same<const void*, decltype(visit(ovd2, variant<std::int64_t, anchored>()))>::value, "");
}

// --------------------------------------------------------------------------------------------

TEST(test_variant, comparison)
{
    using cetl::pf17::variant;
    using cetl::pf17::in_place_index;

    using V       = variant<std::int8_t, std::int16_t>;
    const auto v0 = [](const std::int8_t alt) { return V(in_place_index<0>, alt); };
    const auto v1 = [](const std::int16_t alt) { return V(in_place_index<1>, alt); };

    EXPECT_TRUE(v0(1) == v0(1));
    EXPECT_FALSE(v0(1) == v1(1));
    EXPECT_FALSE(v1(1) == v0(1));
    EXPECT_FALSE(v0(1) == v0(2));

    EXPECT_FALSE(v0(1) != v0(1));
    EXPECT_TRUE(v0(1) != v1(1));
    EXPECT_TRUE(v1(1) != v0(1));
    EXPECT_TRUE(v0(1) != v0(2));

    EXPECT_TRUE(v0(1) < v0(2));
    EXPECT_FALSE(v0(2) < v0(1));
    EXPECT_TRUE(v0(2) < v1(1));
    EXPECT_FALSE(v1(1) < v0(2));
    EXPECT_TRUE(v0(1) < v1(2));
    EXPECT_FALSE(v1(2) < v0(1));

    EXPECT_TRUE(v0(1) <= v0(2));
    EXPECT_FALSE(v0(2) <= v0(1));
    EXPECT_TRUE(v0(2) <= v1(1));
    EXPECT_FALSE(v1(1) <= v0(2));
    EXPECT_TRUE(v0(1) <= v1(2));
    EXPECT_FALSE(v1(2) <= v0(1));

    EXPECT_FALSE(v0(1) > v0(2));
    EXPECT_TRUE(v0(2) > v0(1));
    EXPECT_FALSE(v0(2) > v1(1));
    EXPECT_TRUE(v1(1) > v0(2));
    EXPECT_FALSE(v0(1) > v1(2));
    EXPECT_TRUE(v1(2) > v0(1));

    EXPECT_FALSE(v0(1) >= v0(2));
    EXPECT_TRUE(v0(2) >= v0(1));
    EXPECT_FALSE(v0(2) >= v1(1));
    EXPECT_TRUE(v1(1) >= v0(2));
    EXPECT_FALSE(v0(1) >= v1(2));
    EXPECT_TRUE(v1(2) >= v0(1));

#if __cpp_exceptions
    struct panicky
    {
        panicky()
        {
            throw std::exception();
        }
        bool operator<(const panicky&) const
        {
            return false;
        }
        bool operator==(const panicky&) const
        {
            return true;
        }
    };
    const variant<std::int64_t, panicky> ok;
    variant<std::int64_t, panicky>       ex;
    EXPECT_ANY_THROW(ex.emplace<panicky>());
    EXPECT_TRUE(ex.valueless_by_exception());

    EXPECT_TRUE(ex == ex);
    EXPECT_FALSE(ex == ok);
    EXPECT_FALSE(ok == ex);
    EXPECT_FALSE(ex != ex);
    EXPECT_TRUE(ex != ok);
    EXPECT_TRUE(ok != ex);
    EXPECT_FALSE(ex < ex);
    EXPECT_TRUE(ex < ok);
    EXPECT_FALSE(ok < ex);
    EXPECT_TRUE(ex <= ex);
    EXPECT_TRUE(ex <= ok);
    EXPECT_FALSE(ok <= ex);
    EXPECT_FALSE(ex > ex);
    EXPECT_FALSE(ex > ok);
    EXPECT_TRUE(ok > ex);
    EXPECT_TRUE(ex >= ex);
    EXPECT_FALSE(ex >= ok);
    EXPECT_TRUE(ok >= ex);
#endif
}

}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
