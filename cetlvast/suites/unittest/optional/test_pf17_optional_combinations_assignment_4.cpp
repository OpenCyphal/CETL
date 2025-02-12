/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT


#include "test_pf17_optional_combinations.hpp"

template <typename>
class test_optional_combinations_assignment_4 : public ::testing::Test
{};


TYPED_TEST_SUITE(test_optional_combinations_assignment_4, testing_types, );


using cetl::pf17::optional;

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations_assignment_4, assignment)
{
    struct value_type final : TypeParam
    {
        explicit value_type(const std::int64_t val) noexcept
            : value{val}
        {
        }
        value_type& operator=(const std::int64_t val) noexcept
        {
            value = val;
            return *this;
        }
        std::int64_t value;
    };
    std::uint32_t        dtor = 0;
    optional<value_type> v1;
    // Assign empty. Copy or move ctor is invoked.
    v1 = 12345;
    v1.value().configure_destruction_counter(&dtor);
    EXPECT_TRUE(v1.has_value());
    EXPECT_EQ(12345, v1.value().value);
    EXPECT_EQ(0, v1->get_copy_ctor_count());
    EXPECT_EQ(0, v1->get_move_ctor_count());
    EXPECT_EQ(0, v1->get_copy_assignment_count());
    EXPECT_EQ(0, v1->get_move_assignment_count());
    EXPECT_EQ(0, dtor);
    // Assign non-empty.
    v1 = 23456;
    EXPECT_TRUE(v1);
    EXPECT_EQ(23456, v1.value().value);
    EXPECT_EQ(0, v1->get_copy_ctor_count());
    EXPECT_EQ(0, v1->get_move_ctor_count());
    EXPECT_EQ(0, v1->get_copy_assignment_count());
    EXPECT_EQ(0, v1->get_move_assignment_count());
    EXPECT_EQ(0, dtor);
    // Drop.
    v1 = cetl::pf17::nullopt;
    EXPECT_FALSE(v1);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, dtor);
}

/// ------------------------------------------------------------------------------------------------
