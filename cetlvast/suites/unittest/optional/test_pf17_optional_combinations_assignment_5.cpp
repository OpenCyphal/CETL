/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT


#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_assignment_5 : public ::testing::Test
{};


TYPED_TEST_SUITE(test_optional_combinations_assignment_5, testing_types, );


using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations_assignment_5, assignment)
{
    struct From final : TypeParam
    {
        explicit From(const std::int64_t val) noexcept
            : value{val}
        {
        }
        std::int64_t value;
    };
    struct To final : TypeParam
    {
        explicit To(const std::int64_t val) noexcept
            : value{val}
        {
        }
        explicit To(const From& val) noexcept
            : value{val.value}
        {
        }
        To& operator=(const From& val) noexcept
        {
            value = val.value;
            return *this;
        }
        std::int64_t value;
    };
    static_assert(std::is_assignable<To, From>::value, "");
    static_assert(std::is_assignable<optional<To>, optional<From>>::value, "");
    std::uint32_t  a_dtor = 0;
    std::uint32_t  b_dtor = 0;
    optional<To>   a;
    optional<From> b;
    EXPECT_FALSE(a);
    EXPECT_FALSE(b);
    // Assign empty to empty.
    a = b;
    EXPECT_FALSE(a);
    EXPECT_FALSE(b);
    // Non-empty to empty.
    b.emplace(12345).configure_destruction_counter(&b_dtor);
    EXPECT_TRUE(b);
    EXPECT_FALSE(a);
    a = b;
    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    a.value().configure_destruction_counter(&a_dtor);
    EXPECT_EQ(12345, a.value().value);
    EXPECT_EQ(12345, b.value().value);
    EXPECT_EQ(0, a->get_copy_ctor_count());
    EXPECT_EQ(0, a->get_move_ctor_count());
    EXPECT_EQ(0, a->get_copy_assignment_count());
    EXPECT_EQ(0, a->get_move_assignment_count());
    EXPECT_EQ(0, a_dtor);
    EXPECT_EQ(0, b->get_copy_ctor_count());
    EXPECT_EQ(0, b->get_move_ctor_count());
    EXPECT_EQ(0, b->get_copy_assignment_count());
    EXPECT_EQ(0, b->get_move_assignment_count());
    EXPECT_EQ(0, b_dtor);
    // Non-empty to non-empty.
    b.value().value = 23456;
    EXPECT_EQ(12345, a.value().value);
    EXPECT_EQ(23456, b.value().value);
    a = b;
    EXPECT_TRUE(a);
    EXPECT_TRUE(b);
    EXPECT_EQ(23456, a.value().value);
    EXPECT_EQ(23456, b.value().value);
    EXPECT_EQ(0, a->get_copy_ctor_count());
    EXPECT_EQ(0, a->get_move_ctor_count());
    EXPECT_EQ(0, a->get_copy_assignment_count());
    EXPECT_EQ(0, a->get_move_assignment_count());
    EXPECT_EQ(0, a_dtor);
    EXPECT_EQ(0, b->get_copy_ctor_count());
    EXPECT_EQ(0, b->get_move_ctor_count());
    EXPECT_EQ(0, b->get_copy_assignment_count());
    EXPECT_EQ(0, b->get_move_assignment_count());
    EXPECT_EQ(0, b_dtor);
    // Empty to non-empty.
    b = cetl::pf17::nullopt;
    EXPECT_TRUE(a);
    EXPECT_FALSE(b);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
    a = b;
    EXPECT_FALSE(a);
    EXPECT_FALSE(b);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, a_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
}
