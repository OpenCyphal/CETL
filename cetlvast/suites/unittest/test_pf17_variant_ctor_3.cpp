/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/variant.hpp>  // The tested header always goes first.
#include "test_pf17_variant.hpp"

namespace cetlvast
{
namespace unittest
{
namespace pf17
{
namespace variant
{
// Caveat: types without a move constructor but with a copy constructor that accepts const T& arguments,
// satisfy std::is_move_constructible.
template <typename SMF,
          std::uint8_t CopyCtorPolicy = SMF::copy_ctor_policy_value,
          std::uint8_t MoveCtorPolicy = SMF::move_ctor_policy_value>
struct test_ctor_3
{
    static void test_basic()
    {
        using cetl::pf17::variant;
        using cetl::pf17::in_place_type;
        using cetl::pf17::get;
        using cetl::pf17::monostate;
        using cetlvast::smf_policies::policy_nontrivial;
        using cetlvast::smf_policies::policy_deleted;
        struct T : SMF
        {
            explicit T(const std::int64_t val)
                : value(val)
            {
            }
            T(T&& other) noexcept
                : SMF(std::forward<T>(other))
                , value(other.value)
            {
                other.value = 0;
            }
            std::int64_t value = 0;
        };
        std::uint32_t destructed = 0;
        {
            variant<T, std::int64_t, monostate> v1(in_place_type<T>, 123456);
            EXPECT_EQ(123456, get<T>(v1).value);
            get<T>(v1).configure_destruction_counter(&destructed);
            {
                variant<T, std::int64_t, monostate> v2(std::move(v1));
                EXPECT_EQ(((T::move_ctor_policy_value == policy_deleted) &&
                           (T::copy_ctor_policy_value == policy_nontrivial))
                              ? 1
                              : 0,
                          get<T>(v2).get_copy_ctor_count());
                EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, get<T>(v2).get_move_ctor_count());
                EXPECT_EQ(0, get<T>(v1).value);  // moved out
                EXPECT_EQ(123456, get<T>(v2).value);
                EXPECT_EQ(0, destructed);
                v2.template emplace<std::int64_t>(789);
                EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
                EXPECT_EQ(789, get<std::int64_t>(v2));
            }
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
        // The valueless state cannot occur in a ctor test.
    }

    static void test_valueless()
    {
#if __cpp_exceptions
        using cetl::pf17::variant;
        using cetl::pf17::variant_npos;
        using cetl::pf17::get;
        using cetlvast::smf_policies::policy_nontrivial;
        struct T : SMF
        {};
        struct U : SMF
        {
            U()
            {
                throw std::exception();
            }
        };
        std::uint32_t destructed = 0;
        {
            variant<T, U> v1;
            get<T>(v1).configure_destruction_counter(&destructed);
            EXPECT_ANY_THROW(v1.template emplace<U>());
            EXPECT_EQ((U::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
            EXPECT_TRUE(v1.valueless_by_exception());
            {
                const variant<T, U> v2(std::move(v1));
                EXPECT_TRUE(v1.valueless_by_exception());  // NOLINT(*-use-after-move)
                EXPECT_TRUE(v2.valueless_by_exception());
                EXPECT_EQ(variant_npos, v1.index());
                EXPECT_EQ(variant_npos, v2.index());
            }
        }
        EXPECT_EQ((U::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);  // Same.
#endif
    }

    static void test()
    {
        test_basic();
        test_valueless();
    }
};
template <typename SMF>
struct test_ctor_3<SMF, cetlvast::smf_policies::policy_deleted, cetlvast::smf_policies::policy_deleted>
{
    // Caveat: types without a move constructor but with a copy constructor that accepts const T& arguments,
    // satisfy std::is_move_constructible.
    static_assert(!std::is_move_constructible<SMF>::value, "");
    static_assert(!std::is_move_constructible<cetl::pf17::variant<SMF>>::value, "");
    static void test() {}
};

TYPED_TEST(test_smf_policy_combinations, ctor_3)
{
    test_ctor_3<TypeParam>::test();
}

static constexpr auto test_ctor_3_constexpr()
{
    using cetl::pf17::variant;
    using cetl::pf17::monostate;
    using cetl::pf17::in_place_index;
    using cetl::pf17::get;
    struct U
    {
        constexpr explicit U(const std::int64_t value)
            : value(value)
        {
        }
        constexpr U(const U&) noexcept            = delete;
        constexpr U(U&&) noexcept                 = default;
        constexpr U& operator=(const U&) noexcept = delete;
        constexpr U& operator=(U&&) noexcept      = delete;
        ~U() noexcept                             = default;
        std::int64_t value                        = 0;
    };
    variant<monostate, U> v1(in_place_index<1>, 123456);
    variant<monostate, U> v2(std::move(v1));
    return get<1>(v2).value;
}
static_assert(test_ctor_3_constexpr() == 123456, "");
}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
