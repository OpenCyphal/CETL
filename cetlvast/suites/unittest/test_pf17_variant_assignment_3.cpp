/// @file
/// Unit tests for cetl/pf17/variant.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
// CSpell: words borked

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
TYPED_TEST(test_smf_policy_combinations, assignment_3)
{
    using cetl::pf17::variant;
    using cetl::pf17::get;
    using cetl::pf17::monostate;
    using cetlvast::smf_policies::policy_deleted;
    struct tag_t
    {
        tag_t()                        = default;
        tag_t(const tag_t&)            = delete;
        tag_t(tag_t&&)                 = delete;
        tag_t& operator=(const tag_t&) = delete;
        tag_t& operator=(tag_t&&)      = delete;
        ~tag_t()                       = default;
    } const tag;
    struct T : TypeParam
    {
        T(const tag_t&) {}  // NOLINT(*-explicit-constructor)
        T& operator=(const tag_t&)
        {
            return *this;
        }
        // This is to ensure our copy ctor/assignment are not noexcept.
        struct copyable_may_throw
        {
            copyable_may_throw() = default;
            copyable_may_throw(const copyable_may_throw&) noexcept(false) {}  // NOLINT(*-use-equals-default)
            copyable_may_throw(copyable_may_throw&&) noexcept = default;
            copyable_may_throw& operator=(const copyable_may_throw&) noexcept(false)  // NOLINT(*-use-equals-default)
            {
                return *this;
            }
            copyable_may_throw& operator=(copyable_may_throw&&) noexcept = default;
        } dummy;
    };
    using V                  = variant<std::int64_t, float, T, monostate>;
    std::uint32_t dtor_count = 0;

    // Default-initialize.
    V var;
    EXPECT_EQ(0, get<0>(var));

    // Change value of the same alternative (no ctor invoked).
    var = 123;
    EXPECT_EQ(123, get<0>(var));

    // Switch to float.
    var = 123.456F;
    EXPECT_FLOAT_EQ(123.456F, get<float>(var));

    // Change value of the same alternative (no ctor invoked).
    var = 789.012F;
    EXPECT_FLOAT_EQ(789.012F, get<float>(var));

    // Switch to T.
    var = tag;
    EXPECT_EQ(2, var.index());
    get<T>(var).configure_destruction_counter(&dtor_count);
    EXPECT_EQ(0, dtor_count);

    // Assign the same alternative (no ctor invoked).
    var = tag;
    EXPECT_EQ(2, var.index());
    EXPECT_EQ(0, dtor_count);

    // Switch to monostate.
    var = monostate{};
    EXPECT_EQ(3, var.index());
    EXPECT_EQ((T::dtor_policy_value == smf_policies::policy_nontrivial) ? 1 : 0,  // T is dead baby, T is dead.
              dtor_count);

    // Float cannot be selected due to ambiguity.
    static_assert(!std::is_constructible<V, double>::value, "");
    static_assert(!std::is_assignable<V&, double>::value, "");

    // Example from https://en.cppreference.com/w/cpp/utility/variant/operator%3D
    variant<float, long, double> v4;
    v4 = 0;  // Selects long, because the floating-point types are not candidates.
    EXPECT_EQ(0, get<long>(v4));

#if __cpp_exceptions
    // This type may or may not support direct assignment (without the temporary) depending on whether it's movable.
    struct panicky : TypeParam
    {
        panicky() = default;
        panicky(const tag_t&)  // NOLINT(*-explicit-constructor)
        {
            throw std::exception();
        }
        panicky& operator=(const tag_t&)
        {
            throw std::exception();
            return *this;
        }
        // This is to ensure our copy ctor/assignment are not noexcept.
        struct copyable_may_throw
        {
            copyable_may_throw() = default;
            copyable_may_throw(const copyable_may_throw&) noexcept(false) {}  // NOLINT(*-use-equals-default)
            copyable_may_throw(copyable_may_throw&&) noexcept = default;
            copyable_may_throw& operator=(const copyable_may_throw&) noexcept(false)  // NOLINT(*-use-equals-default)
            {
                return *this;
            }
            copyable_may_throw& operator=(copyable_may_throw&&) noexcept = default;
        } dummy;
    };
    variant<monostate, panicky> v5;
    EXPECT_ANY_THROW(v5 = tag);
    EXPECT_EQ((TypeParam::copy_ctor_policy_value == policy_deleted) &&
                  (TypeParam::move_ctor_policy_value == policy_deleted),
              v5.valueless_by_exception());
    EXPECT_EQ((std::is_nothrow_constructible<panicky, tag_t>::value ||
               (!std::is_nothrow_move_constructible<panicky>::value)),
              v5.valueless_by_exception());
    v5.template emplace<panicky>();
    EXPECT_EQ(1, v5.index());
    EXPECT_ANY_THROW(v5 = tag);
    EXPECT_FALSE(v5.valueless_by_exception());
    EXPECT_EQ(1, v5.index());
#endif
}
}  // namespace variant
}  // namespace pf17
}  // namespace unittest
}  // namespace cetlvast
