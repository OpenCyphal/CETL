/// @file
/// Unit tests for cetl::pf17::pmr::polymorphic_allocator defined in memory_resource.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#include <cetl/pf17/optional.hpp>
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
template <>
struct dtor_policy<policy_deleted>
{
    static constexpr auto dtor_policy_value        = policy_deleted;
    dtor_policy()                                  = default;
    dtor_policy(const dtor_policy&)                = default;
    dtor_policy(dtor_policy&&) noexcept            = default;
    dtor_policy& operator=(const dtor_policy&)     = default;
    dtor_policy& operator=(dtor_policy&&) noexcept = default;
    ~dtor_policy()                                 = delete;
    CETL_NODISCARD auto get_destruction_count() const
    {
        (void) this;
        return 0U;
    }
};

/// CONFIGURABLE POLICY TYPE -------------------------------------------------------------------------------------------
template <std::uint8_t copy_ctor,  //
          std::uint8_t move_ctor,
          std::uint8_t copy_assignment,
          std::uint8_t move_assignment,
          std::uint8_t destruction>
struct configurable_special_function_type : copy_ctor_policy<copy_ctor>,
                                            move_ctor_policy<move_ctor>,
                                            copy_assignment_policy<copy_assignment>,
                                            move_assignment_policy<move_assignment>,
                                            dtor_policy<destruction>
{
    configurable_special_function_type() = default;
};

constexpr std::uint32_t power(const std::uint32_t base, const std::uint32_t exponent)  // NOLINT(*-no-recursion)
{
    return (exponent == 0) ? 1 : ((exponent == 1) ? base : (base * power(base, exponent - 1U)));
}

template <std::uint16_t packed_policy>
struct packed_configurable_special_function_type
    : configurable_special_function_type<(packed_policy / power(3, 4)) % 3,  //
                                         (packed_policy / power(3, 3)) % 3,  //
                                         (packed_policy / power(3, 2)) % 3,  //
                                         (packed_policy / power(3, 1)) % 3,  //
                                         (packed_policy / power(3, 0)) % 3>
{
    static_assert(packed_policy < power(3, 5), "packed_policy is out of range.");
};

using special_function_policy_combinations = ::testing::Types<  //
    packed_configurable_special_function_type<0>,
    packed_configurable_special_function_type<1>,
    packed_configurable_special_function_type<2>,
    packed_configurable_special_function_type<3>,
    packed_configurable_special_function_type<4>,
    packed_configurable_special_function_type<5>,
    packed_configurable_special_function_type<6>,
    packed_configurable_special_function_type<7>,
    packed_configurable_special_function_type<8>,
    packed_configurable_special_function_type<9>,
    packed_configurable_special_function_type<10>,
    packed_configurable_special_function_type<11>,
    packed_configurable_special_function_type<12>,
    packed_configurable_special_function_type<13>,
    packed_configurable_special_function_type<14>,
    packed_configurable_special_function_type<15>,
    packed_configurable_special_function_type<16>,
    packed_configurable_special_function_type<17>,
    packed_configurable_special_function_type<18>,
    packed_configurable_special_function_type<19>,
    packed_configurable_special_function_type<20>,
    packed_configurable_special_function_type<21>,
    packed_configurable_special_function_type<22>,
    packed_configurable_special_function_type<23>,
    packed_configurable_special_function_type<24>,
    packed_configurable_special_function_type<25>,
    packed_configurable_special_function_type<26>,
    packed_configurable_special_function_type<27>,
    packed_configurable_special_function_type<28>,
    packed_configurable_special_function_type<29>,
    packed_configurable_special_function_type<30>,
    packed_configurable_special_function_type<31>,
    packed_configurable_special_function_type<32>,
    packed_configurable_special_function_type<33>,
    packed_configurable_special_function_type<34>,
    packed_configurable_special_function_type<35>,
    packed_configurable_special_function_type<36>,
    packed_configurable_special_function_type<37>,
    packed_configurable_special_function_type<38>,
    packed_configurable_special_function_type<39>,
    packed_configurable_special_function_type<40>,
    packed_configurable_special_function_type<41>,
    packed_configurable_special_function_type<42>,
    packed_configurable_special_function_type<43>,
    packed_configurable_special_function_type<44>,
    packed_configurable_special_function_type<45>,
    packed_configurable_special_function_type<46>,
    packed_configurable_special_function_type<47>,
    packed_configurable_special_function_type<48>,
    packed_configurable_special_function_type<49>,
    packed_configurable_special_function_type<50>,
    packed_configurable_special_function_type<51>,
    packed_configurable_special_function_type<52>,
    packed_configurable_special_function_type<53>,
    packed_configurable_special_function_type<54>,
    packed_configurable_special_function_type<55>,
    packed_configurable_special_function_type<56>,
    packed_configurable_special_function_type<57>,
    packed_configurable_special_function_type<58>,
    packed_configurable_special_function_type<59>,
    packed_configurable_special_function_type<60>,
    packed_configurable_special_function_type<61>,
    packed_configurable_special_function_type<62>,
    packed_configurable_special_function_type<63>,
    packed_configurable_special_function_type<64>,
    packed_configurable_special_function_type<65>,
    packed_configurable_special_function_type<66>,
    packed_configurable_special_function_type<67>,
    packed_configurable_special_function_type<68>,
    packed_configurable_special_function_type<69>,
    packed_configurable_special_function_type<70>,
    packed_configurable_special_function_type<71>,
    packed_configurable_special_function_type<72>,
    packed_configurable_special_function_type<73>,
    packed_configurable_special_function_type<74>,
    packed_configurable_special_function_type<75>,
    packed_configurable_special_function_type<76>,
    packed_configurable_special_function_type<77>,
    packed_configurable_special_function_type<78>,
    packed_configurable_special_function_type<79>,
    packed_configurable_special_function_type<80>,
    packed_configurable_special_function_type<81>,
    packed_configurable_special_function_type<82>,
    packed_configurable_special_function_type<83>,
    packed_configurable_special_function_type<84>,
    packed_configurable_special_function_type<85>,
    packed_configurable_special_function_type<86>,
    packed_configurable_special_function_type<87>,
    packed_configurable_special_function_type<88>,
    packed_configurable_special_function_type<89>,
    packed_configurable_special_function_type<90>,
    packed_configurable_special_function_type<91>,
    packed_configurable_special_function_type<92>,
    packed_configurable_special_function_type<93>,
    packed_configurable_special_function_type<94>,
    packed_configurable_special_function_type<95>,
    packed_configurable_special_function_type<96>,
    packed_configurable_special_function_type<97>,
    packed_configurable_special_function_type<98>,
    packed_configurable_special_function_type<99>,
    packed_configurable_special_function_type<100>,
    packed_configurable_special_function_type<101>,
    packed_configurable_special_function_type<102>,
    packed_configurable_special_function_type<103>,
    packed_configurable_special_function_type<104>,
    packed_configurable_special_function_type<105>,
    packed_configurable_special_function_type<106>,
    packed_configurable_special_function_type<107>,
    packed_configurable_special_function_type<108>,
    packed_configurable_special_function_type<109>,
    packed_configurable_special_function_type<110>,
    packed_configurable_special_function_type<111>,
    packed_configurable_special_function_type<112>,
    packed_configurable_special_function_type<113>,
    packed_configurable_special_function_type<114>,
    packed_configurable_special_function_type<115>,
    packed_configurable_special_function_type<116>,
    packed_configurable_special_function_type<117>,
    packed_configurable_special_function_type<118>,
    packed_configurable_special_function_type<119>,
    packed_configurable_special_function_type<120>,
    packed_configurable_special_function_type<121>,
    packed_configurable_special_function_type<122>,
    packed_configurable_special_function_type<123>,
    packed_configurable_special_function_type<124>,
    packed_configurable_special_function_type<125>,
    packed_configurable_special_function_type<126>,
    packed_configurable_special_function_type<127>,
    packed_configurable_special_function_type<128>,
    packed_configurable_special_function_type<129>,
    packed_configurable_special_function_type<130>,
    packed_configurable_special_function_type<131>,
    packed_configurable_special_function_type<132>,
    packed_configurable_special_function_type<133>,
    packed_configurable_special_function_type<134>,
    packed_configurable_special_function_type<135>,
    packed_configurable_special_function_type<136>,
    packed_configurable_special_function_type<137>,
    packed_configurable_special_function_type<138>,
    packed_configurable_special_function_type<139>,
    packed_configurable_special_function_type<140>,
    packed_configurable_special_function_type<141>,
    packed_configurable_special_function_type<142>,
    packed_configurable_special_function_type<143>,
    packed_configurable_special_function_type<144>,
    packed_configurable_special_function_type<145>,
    packed_configurable_special_function_type<146>,
    packed_configurable_special_function_type<147>,
    packed_configurable_special_function_type<148>,
    packed_configurable_special_function_type<149>,
    packed_configurable_special_function_type<150>,
    packed_configurable_special_function_type<151>,
    packed_configurable_special_function_type<152>,
    packed_configurable_special_function_type<153>,
    packed_configurable_special_function_type<154>,
    packed_configurable_special_function_type<155>,
    packed_configurable_special_function_type<156>,
    packed_configurable_special_function_type<157>,
    packed_configurable_special_function_type<158>,
    packed_configurable_special_function_type<159>,
    packed_configurable_special_function_type<160>,
    packed_configurable_special_function_type<161>,
    packed_configurable_special_function_type<162>,
    packed_configurable_special_function_type<163>,
    packed_configurable_special_function_type<164>,
    packed_configurable_special_function_type<165>,
    packed_configurable_special_function_type<166>,
    packed_configurable_special_function_type<167>,
    packed_configurable_special_function_type<168>,
    packed_configurable_special_function_type<169>,
    packed_configurable_special_function_type<170>,
    packed_configurable_special_function_type<171>,
    packed_configurable_special_function_type<172>,
    packed_configurable_special_function_type<173>,
    packed_configurable_special_function_type<174>,
    packed_configurable_special_function_type<175>,
    packed_configurable_special_function_type<176>,
    packed_configurable_special_function_type<177>,
    packed_configurable_special_function_type<178>,
    packed_configurable_special_function_type<179>,
    packed_configurable_special_function_type<180>,
    packed_configurable_special_function_type<181>,
    packed_configurable_special_function_type<182>,
    packed_configurable_special_function_type<183>,
    packed_configurable_special_function_type<184>,
    packed_configurable_special_function_type<185>,
    packed_configurable_special_function_type<186>,
    packed_configurable_special_function_type<187>,
    packed_configurable_special_function_type<188>,
    packed_configurable_special_function_type<189>,
    packed_configurable_special_function_type<190>,
    packed_configurable_special_function_type<191>,
    packed_configurable_special_function_type<192>,
    packed_configurable_special_function_type<193>,
    packed_configurable_special_function_type<194>,
    packed_configurable_special_function_type<195>,
    packed_configurable_special_function_type<196>,
    packed_configurable_special_function_type<197>,
    packed_configurable_special_function_type<198>,
    packed_configurable_special_function_type<199>,
    packed_configurable_special_function_type<200>,
    packed_configurable_special_function_type<201>,
    packed_configurable_special_function_type<202>,
    packed_configurable_special_function_type<203>,
    packed_configurable_special_function_type<204>,
    packed_configurable_special_function_type<205>,
    packed_configurable_special_function_type<206>,
    packed_configurable_special_function_type<207>,
    packed_configurable_special_function_type<208>,
    packed_configurable_special_function_type<209>,
    packed_configurable_special_function_type<210>,
    packed_configurable_special_function_type<211>,
    packed_configurable_special_function_type<212>,
    packed_configurable_special_function_type<213>,
    packed_configurable_special_function_type<214>,
    packed_configurable_special_function_type<215>,
    packed_configurable_special_function_type<216>,
    packed_configurable_special_function_type<217>,
    packed_configurable_special_function_type<218>,
    packed_configurable_special_function_type<219>,
    packed_configurable_special_function_type<220>,
    packed_configurable_special_function_type<221>,
    packed_configurable_special_function_type<222>,
    packed_configurable_special_function_type<223>,
    packed_configurable_special_function_type<224>,
    packed_configurable_special_function_type<225>,
    packed_configurable_special_function_type<226>,
    packed_configurable_special_function_type<227>,
    packed_configurable_special_function_type<228>,
    packed_configurable_special_function_type<229>,
    packed_configurable_special_function_type<230>,
    packed_configurable_special_function_type<231>,
    packed_configurable_special_function_type<232>,
    packed_configurable_special_function_type<233>,
    packed_configurable_special_function_type<234>,
    packed_configurable_special_function_type<235>,
    packed_configurable_special_function_type<236>,
    packed_configurable_special_function_type<237>,
    packed_configurable_special_function_type<238>,
    packed_configurable_special_function_type<239>,
    packed_configurable_special_function_type<240>,
    packed_configurable_special_function_type<241>,
    packed_configurable_special_function_type<242>>;

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

/// TESTS -----------------------------------------------------------------------------------------------------------

template <typename>
class TestOptionalSpecialFunctionPolicy : public ::testing::Test
{};

TYPED_TEST_SUITE(TestOptionalSpecialFunctionPolicy, special_function_policy_combinations, );

template <typename policy_type,
          std::uint8_t policy_copy_ctor = policy_type::copy_ctor_policy_value,
          std::uint8_t policy_dtor      = policy_type::dtor_policy_value>
struct test_ctor8
{
    static void test()
    {
        using ty = value_type<std::uint8_t, policy_type>;
        ty                       val(123U);
        cetl::pf17::optional<ty> opt(val);
        EXPECT_EQ(0U, val.get_copy_ctor_count());
        EXPECT_EQ(0U, val.get_move_ctor_count());
        EXPECT_EQ(0U, val.get_copy_assignment_count());
        EXPECT_EQ(0U, val.get_move_assignment_count());
        EXPECT_EQ(0U, val.get_destruction_count());
        ty& inner = opt.value();
        EXPECT_EQ((policy_copy_ctor == policy_nontrivial) ? 1U : 0, inner.get_copy_ctor_count());
        EXPECT_EQ(0U, inner.get_move_ctor_count());
        EXPECT_EQ(0U, inner.get_copy_assignment_count());
        EXPECT_EQ(0U, inner.get_move_assignment_count());
        EXPECT_EQ(0U, inner.get_destruction_count());
        opt.reset();
        EXPECT_EQ((policy_copy_ctor == policy_nontrivial) ? 1U : 0, inner.get_copy_ctor_count());
        EXPECT_EQ(0U, inner.get_move_ctor_count());
        EXPECT_EQ(0U, inner.get_copy_assignment_count());
        EXPECT_EQ(0U, inner.get_move_assignment_count());
        EXPECT_EQ((policy_dtor == policy_nontrivial) ? 1U : 0, inner.get_destruction_count());
    }
};
template <typename PolicyType, std::uint8_t policy_dtor>
struct test_ctor8<PolicyType, policy_deleted, policy_dtor>
{
    static void test()
    {
        static_assert(!std::is_copy_constructible<PolicyType>::value);
        static_assert(!std::is_copy_constructible<cetl::pf17::optional<PolicyType>>::value);
    }
};
template <typename PolicyType>
struct test_ctor8<PolicyType, policy_nontrivial, policy_deleted>
{
    static void test() {}
};
template <typename PolicyType>
struct test_ctor8<PolicyType, policy_trivial, policy_deleted>
{
    static void test() {}
};
template <typename PolicyType>
struct test_ctor8<PolicyType, policy_deleted, policy_deleted>
{
    static void test() {}
};

TYPED_TEST(TestOptionalSpecialFunctionPolicy, TestCtor8)
{
    test_ctor8<TypeParam>::test();
}
