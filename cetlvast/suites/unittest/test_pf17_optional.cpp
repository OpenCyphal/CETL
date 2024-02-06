/// @file
/// Unit tests for cetl::pf17::pmr::polymorphic_allocator defined in memory_resource.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/optional.hpp>
#include <cetlvast/typelist.hpp>
#include <gtest/gtest.h>
#include <cstdint>

/// This has to be an old-style enum because C++14 requires it to be convertible to int.
enum special_function_policy
{
    policy_deleted    = 0,
    policy_trivial    = 1,
    policy_nontrivial = 2,
};

/// COPY CONSTRUCTION POLICY -------------------------------------------------------------------------------------------
template <std::uint8_t>
struct copy_ctor_policy;
template <>
struct copy_ctor_policy<policy_nontrivial>
{
    static constexpr auto copy_ctor_policy_value = policy_nontrivial;
    copy_ctor_policy()                           = default;
    copy_ctor_policy(const copy_ctor_policy& other)
        : copy_constructed{other.copy_constructed + 1U}
    {
    }
    copy_ctor_policy(copy_ctor_policy&&)                 = default;
    copy_ctor_policy& operator=(const copy_ctor_policy&) = default;
    copy_ctor_policy& operator=(copy_ctor_policy&&)      = default;
    ~copy_ctor_policy()                                  = default;
    CETL_NODISCARD auto get_copy_ctor_count() const
    {
        return copy_constructed;
    }
    std::uint32_t copy_constructed = 0;
};
template <>
struct copy_ctor_policy<policy_trivial>
{
    static constexpr auto copy_ctor_policy_value = policy_trivial;
    CETL_NODISCARD auto   get_copy_ctor_count() const
    {
        (void) this;
        return 0U;
    }
};
template <>
struct copy_ctor_policy<policy_deleted>
{
    static constexpr auto copy_ctor_policy_value         = policy_deleted;
    copy_ctor_policy()                                   = default;
    copy_ctor_policy(const copy_ctor_policy&)            = delete;
    copy_ctor_policy(copy_ctor_policy&&)                 = default;
    copy_ctor_policy& operator=(const copy_ctor_policy&) = default;
    copy_ctor_policy& operator=(copy_ctor_policy&&)      = default;
    ~copy_ctor_policy()                                  = default;
    CETL_NODISCARD auto get_copy_ctor_count() const
    {
        (void) this;
        return 0U;
    }
};

/// MOVE CONSTRUCTION POLICY -------------------------------------------------------------------------------------------
template <std::uint8_t>
struct move_ctor_policy;
template <>
struct move_ctor_policy<policy_nontrivial>
{
    static constexpr auto move_ctor_policy_value = policy_nontrivial;
    move_ctor_policy()                           = default;
    move_ctor_policy(const move_ctor_policy&)    = default;
    move_ctor_policy(move_ctor_policy&& other) noexcept
        : move_constructed{other.move_constructed + 1U}
    {
    }
    move_ctor_policy& operator=(const move_ctor_policy&) = default;
    move_ctor_policy& operator=(move_ctor_policy&&)      = default;
    ~move_ctor_policy()                                  = default;
    std::uint32_t       move_constructed                 = 0;
    CETL_NODISCARD auto get_move_ctor_count() const
    {
        return move_constructed;
    }
};
template <>
struct move_ctor_policy<policy_trivial>
{
    static constexpr auto move_ctor_policy_value = policy_trivial;
    CETL_NODISCARD auto   get_move_ctor_count() const
    {
        (void) this;
        return 0U;
    }
};
template <>
struct move_ctor_policy<policy_deleted>
{
    static constexpr auto move_ctor_policy_value         = policy_deleted;
    move_ctor_policy()                                   = default;
    move_ctor_policy(const move_ctor_policy&)            = default;
    move_ctor_policy(move_ctor_policy&&)                 = delete;
    move_ctor_policy& operator=(const move_ctor_policy&) = default;
    move_ctor_policy& operator=(move_ctor_policy&&)      = default;
    ~move_ctor_policy()                                  = default;
    CETL_NODISCARD auto get_move_ctor_count() const
    {
        (void) this;
        return 0U;
    }
};

/// COPY ASSIGNMENT POLICY -------------------------------------------------------------------------------------------
template <std::uint8_t>
struct copy_assignment_policy;
template <>
struct copy_assignment_policy<policy_nontrivial>
{
    static constexpr auto copy_assignment_policy_value    = policy_nontrivial;
    copy_assignment_policy()                              = default;
    copy_assignment_policy(const copy_assignment_policy&) = default;
    copy_assignment_policy(copy_assignment_policy&&)      = default;
    copy_assignment_policy& operator=(const copy_assignment_policy& other)
    {
        copy_assigned = other.copy_assigned + 1U;
        return *this;
    }
    copy_assignment_policy& operator=(copy_assignment_policy&&) = default;
    ~copy_assignment_policy()                                   = default;
    CETL_NODISCARD auto get_copy_assignment_count() const
    {
        return copy_assigned;
    }
    std::uint32_t copy_assigned = 0;
};
template <>
struct copy_assignment_policy<policy_trivial>
{
    static constexpr auto copy_assignment_policy_value = policy_trivial;
    CETL_NODISCARD auto   get_copy_assignment_count() const
    {
        (void) this;
        return 0U;
    }
};
template <>
struct copy_assignment_policy<policy_deleted>
{
    static constexpr auto copy_assignment_policy_value               = policy_deleted;
    copy_assignment_policy()                                         = default;
    copy_assignment_policy(const copy_assignment_policy&)            = default;
    copy_assignment_policy(copy_assignment_policy&&)                 = default;
    copy_assignment_policy& operator=(const copy_assignment_policy&) = delete;
    copy_assignment_policy& operator=(copy_assignment_policy&&)      = default;
    ~copy_assignment_policy()                                        = default;
    CETL_NODISCARD auto get_copy_assignment_count() const
    {
        (void) this;
        return 0U;
    }
};

/// MOVE ASSIGNMENT POLICY -------------------------------------------------------------------------------------------
template <std::uint8_t>
struct move_assignment_policy;
template <>
struct move_assignment_policy<policy_nontrivial>
{
    static constexpr auto move_assignment_policy_value               = policy_nontrivial;
    move_assignment_policy()                                         = default;
    move_assignment_policy(const move_assignment_policy&)            = default;
    move_assignment_policy(move_assignment_policy&&)                 = default;
    move_assignment_policy& operator=(const move_assignment_policy&) = default;
    move_assignment_policy& operator=(move_assignment_policy&& other) noexcept
    {
        move_assigned = other.move_assigned + 1U;
        return *this;
    }
    ~move_assignment_policy() = default;
    CETL_NODISCARD auto get_move_assignment_count() const
    {
        return move_assigned;
    }
    std::uint32_t move_assigned = 0;
};
template <>
struct move_assignment_policy<policy_trivial>
{
    static constexpr auto move_assignment_policy_value = policy_trivial;
    CETL_NODISCARD auto   get_move_assignment_count() const
    {
        (void) this;
        return 0U;
    }
};
template <>
struct move_assignment_policy<policy_deleted>
{
    static constexpr auto move_assignment_policy_value               = policy_deleted;
    move_assignment_policy()                                         = default;
    move_assignment_policy(const move_assignment_policy&)            = default;
    move_assignment_policy(move_assignment_policy&&)                 = default;
    move_assignment_policy& operator=(const move_assignment_policy&) = default;
    move_assignment_policy& operator=(move_assignment_policy&&)      = delete;
    ~move_assignment_policy()                                        = default;
    CETL_NODISCARD auto get_move_assignment_count() const
    {
        (void) this;
        return 0U;
    }
};

/// DESTRUCTION POLICY -------------------------------------------------------------------------------------------
template <std::uint8_t>
struct dtor_policy;
template <>
struct dtor_policy<policy_nontrivial>
{
    static constexpr auto dtor_policy_value        = policy_nontrivial;
    dtor_policy()                                  = default;
    dtor_policy(const dtor_policy&)                = default;
    dtor_policy(dtor_policy&&) noexcept            = default;
    dtor_policy& operator=(const dtor_policy&)     = default;
    dtor_policy& operator=(dtor_policy&&) noexcept = default;
    ~dtor_policy()
    {
        destructed++;
    }
    CETL_NODISCARD auto get_destruction_count() const
    {
        return destructed;
    }
    std::uint32_t destructed = 0;
};
template <>
struct dtor_policy<policy_trivial>
{
    static constexpr auto dtor_policy_value = policy_trivial;
    CETL_NODISCARD auto   get_destruction_count() const
    {
        (void) this;
        return 0U;
    }
};
// There is no specialization for policy_deleted because std::optional requires the destructor to be accessible.

/// POLICY COMBINATIONS -------------------------------------------------------------------------------------------
using policy_combinations = cetlvast::typelist::cartesian_product<  //
    std::tuple<copy_ctor_policy<policy_deleted>,                    //
               copy_ctor_policy<policy_trivial>,
               copy_ctor_policy<policy_nontrivial>>,
    std::tuple<move_ctor_policy<policy_deleted>,  //
               move_ctor_policy<policy_trivial>,
               move_ctor_policy<policy_nontrivial>>,
    std::tuple<copy_assignment_policy<policy_deleted>,  //
               copy_assignment_policy<policy_trivial>,
               copy_assignment_policy<policy_nontrivial>>,
    std::tuple<move_assignment_policy<policy_deleted>,  //
               move_assignment_policy<policy_trivial>,
               move_assignment_policy<policy_nontrivial>>,
    std::tuple<dtor_policy<policy_trivial>,  //
               dtor_policy<policy_nontrivial>>>;

/// Creates a new type that inherits from all the given types in the specified order.
/// The list of types shall be given in a typelist container, like std::tuple.
template <typename...>
struct combine_bases;
template <template <typename...> class Q, typename... Ts>
struct combine_bases<Q<Ts...>> : public Ts...
{};

template <typename...>
struct generate_bases;
template <typename... Ts>
struct generate_bases<std::tuple<Ts...>>
{
    using type = std::tuple<combine_bases<Ts>...>;
};

/// This is a long list of all the possible combinations of special function policies.
/// Derive from each type to test all possible policies.
using testing_types = cetlvast::typelist::into<::testing::Types>::from<generate_bases<policy_combinations>::type>;

/// TESTS -----------------------------------------------------------------------------------------------------------

/// This is a simple helper for testing that allows us to apply arbitrary special function policies to a value type.
template <typename V, typename... B>
struct value_type final : public B...
{
    explicit value_type(V&& value)
        : value{std::forward<V>(value)}
    {
    }
    V value;
};

template <typename>
class TestOptionalSpecialFunctionPolicy : public ::testing::Test
{};

TYPED_TEST_SUITE(TestOptionalSpecialFunctionPolicy, testing_types, );

template <typename T, std::uint8_t CopyCtorPolicy = T::copy_ctor_policy_value>
struct test_ctor8
{
    static void test()
    {
        using ty = value_type<std::uint8_t, T>;
        ty                       val(123U);
        cetl::pf17::optional<ty> opt(val);
        EXPECT_EQ(0U, val.get_copy_ctor_count());
        EXPECT_EQ(0U, val.get_move_ctor_count());
        EXPECT_EQ(0U, val.get_copy_assignment_count());
        EXPECT_EQ(0U, val.get_move_assignment_count());
        EXPECT_EQ(0U, val.get_destruction_count());
        ty& inner = opt.value();
        EXPECT_EQ((CopyCtorPolicy == policy_nontrivial) ? 1U : 0, inner.get_copy_ctor_count());
        EXPECT_EQ(0U, inner.get_move_ctor_count());
        EXPECT_EQ(0U, inner.get_copy_assignment_count());
        EXPECT_EQ(0U, inner.get_move_assignment_count());
        EXPECT_EQ(0U, inner.get_destruction_count());
        opt.reset();
        EXPECT_EQ((CopyCtorPolicy == policy_nontrivial) ? 1U : 0, inner.get_copy_ctor_count());
        EXPECT_EQ(0U, inner.get_move_ctor_count());
        EXPECT_EQ(0U, inner.get_copy_assignment_count());
        EXPECT_EQ(0U, inner.get_move_assignment_count());
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1U : 0, inner.get_destruction_count());
    }
};
template <typename PolicyType>
struct test_ctor8<PolicyType, policy_deleted>
{
    static void test()
    {
        static_assert(!std::is_copy_constructible<PolicyType>::value);
        static_assert(!std::is_copy_constructible<cetl::pf17::optional<PolicyType>>::value);
    }
};

TYPED_TEST(TestOptionalSpecialFunctionPolicy, TestCtor8)
{
    test_ctor8<TypeParam>::test();
}
