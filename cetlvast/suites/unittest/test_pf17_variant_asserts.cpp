/// @file
/// Assertion policy checks for cetl/pf17/variant.hpp.
/// @copyright
/// Copyright (C) OpenCyphal Development Team <opencyphal.org>
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/variant.hpp>  // The tested header always goes first.
#include <cetlvast/helpers_gtest.hpp>

#include <cassert>

namespace
{
using storage = cetl::pf17::detail::var::base_storage<cetl::pf17::detail::var::types<int, char>>;

TEST(VariantAssertionPolicy, ValidAlternativeAccess)
{
    storage value{cetl::pf17::in_place_index<0>, 42};
    EXPECT_EQ(42, cetl::pf17::detail::var::alt<0>(value));
}

#if defined(CETL_ENABLE_DEBUG_ASSERT) && CETL_ENABLE_DEBUG_ASSERT
void access_wrong_alternative()
{
    flush_coverage_on_death();
    storage value{cetl::pf17::in_place_index<0>, 42};
    auto&   wrong = cetl::pf17::detail::var::alt<1>(value);
    (void) wrong;
}

TEST(DeathTestVariantAssertionPolicy, MismatchedAlternative)
{
    EXPECT_DEATH(access_wrong_alternative(), "variant internal contract");
}
#endif

#if !defined(NDEBUG)
TEST(DeathTestVariantAssertionPolicy, ApplicationAssertionRemainsEnabled)
{
    EXPECT_DEATH(
        {
            flush_coverage_on_death();
            assert(false && "application assertion remains active");
        },
        "application assertion remains active");
}
#endif
}  // namespace
