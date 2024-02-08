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
    CETL_NODISCARD auto get_move_ctor_count() const
    {
        return move_constructed;
    }
    std::uint32_t move_constructed = 0;
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
/// Before the object is destroyed, the destruction counter should be configured by calling
/// configure_destruction_counter. The reason we don't keep the destruction counter as a member variable is that
/// we can't safely access it after the object is destroyed.
/// The trivial destruction policy does not maintain the destruction counter and the method does nothing.
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
        if (nullptr != destructed)
        {
            ++*destructed;
        }
    }
    void configure_destruction_counter(std::uint32_t* const counter)
    {
        destructed = counter;
    }
    std::uint32_t* destructed = nullptr;
};
template <>
struct dtor_policy<policy_trivial>
{
    static constexpr auto dtor_policy_value = policy_trivial;
    void                  configure_destruction_counter(std::uint32_t* const) {}
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
template <typename>
struct combine_bases;
template <template <typename...> class Q, typename... Ts>
struct combine_bases<Q<Ts...>> : public Ts...
{};

namespace self_check
{
template <std::uint8_t P, std::uint8_t DtorPolicy = P>
using same_policy = combine_bases<std::tuple<copy_ctor_policy<P>,
                                             move_ctor_policy<P>,
                                             copy_assignment_policy<P>,
                                             move_assignment_policy<P>,
                                             dtor_policy<DtorPolicy>>>;
//
static_assert(std::is_trivially_copy_constructible<same_policy<policy_trivial>>::value, "");
static_assert(std::is_trivially_move_constructible<same_policy<policy_trivial>>::value, "");
static_assert(std::is_trivially_copy_assignable<same_policy<policy_trivial>>::value, "");
static_assert(std::is_trivially_move_assignable<same_policy<policy_trivial>>::value, "");
static_assert(std::is_trivially_destructible<same_policy<policy_trivial>>::value, "");
//
static_assert(!std::is_trivially_copy_constructible<same_policy<policy_nontrivial>>::value, "");
static_assert(!std::is_trivially_move_constructible<same_policy<policy_nontrivial>>::value, "");
static_assert(!std::is_trivially_copy_assignable<same_policy<policy_nontrivial>>::value, "");
static_assert(!std::is_trivially_move_assignable<same_policy<policy_nontrivial>>::value, "");
static_assert(!std::is_trivially_destructible<same_policy<policy_nontrivial>>::value, "");
//
static_assert(!std::is_copy_constructible<same_policy<policy_deleted, policy_trivial>>::value, "");
static_assert(!std::is_move_constructible<same_policy<policy_deleted, policy_trivial>>::value, "");
static_assert(!std::is_copy_assignable<same_policy<policy_deleted, policy_trivial>>::value, "");
static_assert(!std::is_move_assignable<same_policy<policy_deleted, policy_trivial>>::value, "");
}  // namespace self_check

template <typename>
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

using cetl::pf17::optional;
using cetl::pf17::nullopt;
using cetl::pf17::in_place;

static_assert(std::is_same<optional<bool>::value_type, bool>::value, "");
static_assert(std::is_same<optional<long>::value_type, long>::value, "");

static_assert(std::is_trivially_copy_constructible<optional<bool>>::value, "");
static_assert(std::is_trivially_move_constructible<optional<bool>>::value, "");
static_assert(std::is_trivially_copy_assignable<optional<bool>>::value, "");
static_assert(std::is_trivially_move_assignable<optional<bool>>::value, "");
static_assert(std::is_trivially_destructible<optional<bool>>::value, "");

template <typename>
class test_optional_combinations : public ::testing::Test
{};

TYPED_TEST_SUITE(test_optional_combinations, testing_types, );

struct copyable_t
{};
struct noncopyable_t
{
    noncopyable_t()                                = default;
    noncopyable_t(const noncopyable_t&)            = delete;
    noncopyable_t(noncopyable_t&&)                 = delete;
    noncopyable_t& operator=(const noncopyable_t&) = delete;
    noncopyable_t& operator=(noncopyable_t&&)      = delete;
    ~noncopyable_t()                               = default;
};

/// A simple pair of types for testing where foo is implicitly convertible to bar but not vice versa.
template <typename>
struct bar;
template <typename Base>
struct foo final : Base
{
    foo()
        : value{0}
    {
    }
    explicit foo(const std::int64_t val) noexcept
        : value{val}
    {
    }
    explicit foo(const bar<Base>& val) noexcept;
    explicit foo(bar<Base>&& val) noexcept;
    foo(const std::initializer_list<std::int64_t> il)
        : value{static_cast<std::int64_t>(il.size())}
    {
    }
    std::int64_t value;
};
template <typename Base>
struct bar final : Base
{
    bar(const std::int64_t val) noexcept  // NOLINT(*-explicit-constructor)
        : value{val}
    {
    }
    bar(const foo<Base>& other) noexcept  // NOLINT(*-explicit-constructor)
        : value{other.value}
    {
    }
    bar(foo<Base>&& other) noexcept  // NOLINT(*-explicit-constructor)
        : value{other.value}
    {
        other.value = 0;  // Moving zeroes the source.
    }
    std::int64_t value;
};
template <typename Base>
foo<Base>::foo(const bar<Base>& val) noexcept
    : value{val.value}
{
}
template <typename Base>
foo<Base>::foo(bar<Base>&& val) noexcept
    : value{val.value}
{
    val.value = 0;  // Moving zeroes the source.
}

// COPYABLE CASE
// Check implicit conversions.
static_assert(std::is_convertible<foo<copyable_t>, bar<copyable_t>>::value, "");
static_assert(!std::is_convertible<bar<copyable_t>, foo<copyable_t>>::value, "");
static_assert(std::is_convertible<optional<foo<copyable_t>>, optional<bar<copyable_t>>>::value, "");
static_assert(!std::is_convertible<optional<bar<copyable_t>>, optional<foo<copyable_t>>>::value, "");
// Check explicit conversions.
static_assert(std::is_constructible<bar<copyable_t>, foo<copyable_t>>::value, "");
static_assert(std::is_constructible<foo<copyable_t>, bar<copyable_t>>::value, "");
static_assert(std::is_constructible<optional<bar<copyable_t>>, optional<foo<copyable_t>>>::value, "");
static_assert(std::is_constructible<optional<foo<copyable_t>>, optional<bar<copyable_t>>>::value, "");
// Check triviality of foo.
static_assert(std::is_trivially_copy_constructible<optional<foo<copyable_t>>>::value, "");
static_assert(std::is_trivially_move_constructible<optional<foo<copyable_t>>>::value, "");
static_assert(std::is_trivially_copy_assignable<optional<foo<copyable_t>>>::value, "");
static_assert(std::is_trivially_move_assignable<optional<foo<copyable_t>>>::value, "");
static_assert(std::is_trivially_destructible<optional<foo<copyable_t>>>::value, "");
// Check triviality of bar.
static_assert(std::is_trivially_copy_constructible<optional<bar<copyable_t>>>::value, "");
static_assert(std::is_trivially_move_constructible<optional<bar<copyable_t>>>::value, "");
static_assert(std::is_trivially_copy_assignable<optional<bar<copyable_t>>>::value, "");
static_assert(std::is_trivially_move_assignable<optional<bar<copyable_t>>>::value, "");
static_assert(std::is_trivially_destructible<optional<bar<copyable_t>>>::value, "");
// NONCOPYABLE CASE
// Check implicit conversions.
// CAVEAT: in C++14, std::is_convertible<F, T> is not true if T is not copyable, even if F is convertible to T,
// so we use std::is_convertible<F, T&&> instead.
static_assert(std::is_convertible<foo<noncopyable_t>, bar<noncopyable_t>&&>::value, "");
static_assert(!std::is_convertible<bar<noncopyable_t>, foo<noncopyable_t>&&>::value, "");
static_assert(std::is_convertible<optional<foo<noncopyable_t>>, optional<bar<noncopyable_t>>&&>::value, "");
static_assert(!std::is_convertible<optional<bar<noncopyable_t>>, optional<foo<noncopyable_t>>&&>::value, "");
// Check explicit conversions.
static_assert(std::is_constructible<bar<noncopyable_t>, foo<noncopyable_t>>::value, "");
static_assert(std::is_constructible<foo<noncopyable_t>, bar<noncopyable_t>>::value, "");
static_assert(std::is_constructible<optional<bar<noncopyable_t>>, optional<foo<noncopyable_t>>>::value, "");
static_assert(std::is_constructible<optional<foo<noncopyable_t>>, optional<bar<noncopyable_t>>>::value, "");
// Check triviality of foo.
static_assert(!std::is_copy_constructible<optional<foo<noncopyable_t>>>::value, "");
static_assert(!std::is_move_constructible<optional<foo<noncopyable_t>>>::value, "");
static_assert(!std::is_copy_assignable<optional<foo<noncopyable_t>>>::value, "");
static_assert(!std::is_move_assignable<optional<foo<noncopyable_t>>>::value, "");
static_assert(std::is_trivially_destructible<optional<foo<noncopyable_t>>>::value, "");
// Check triviality of bar.
static_assert(!std::is_copy_constructible<optional<bar<noncopyable_t>>>::value, "");
static_assert(!std::is_move_constructible<optional<bar<noncopyable_t>>>::value, "");
static_assert(!std::is_copy_assignable<optional<bar<noncopyable_t>>>::value, "");
static_assert(!std::is_move_assignable<optional<bar<noncopyable_t>>>::value, "");
static_assert(std::is_trivially_destructible<optional<bar<noncopyable_t>>>::value, "");

/// ------------------------------------------------------------------------------------------------

/// This test checks common behaviors that are independent of the copy/move policies.
TYPED_TEST(test_optional_combinations, common)
{
    // Ensure trivial copy/move policies are correctly inherited from the value type.
    static_assert(std::is_trivially_copy_constructible<TypeParam>::value ==
                      std::is_trivially_copy_constructible<optional<TypeParam>>::value,
                  "");
    static_assert(std::is_trivially_move_constructible<TypeParam>::value ==
                      std::is_trivially_move_constructible<optional<TypeParam>>::value,
                  "");
    static_assert((std::is_trivially_copy_assignable<TypeParam>::value &&
                   std::is_trivially_copy_constructible<TypeParam>::value &&
                   std::is_trivially_destructible<TypeParam>::value) ==
                      std::is_trivially_copy_assignable<optional<TypeParam>>::value,
                  "");
    static_assert((std::is_trivially_move_assignable<TypeParam>::value &&
                   std::is_trivially_move_constructible<TypeParam>::value &&
                   std::is_trivially_destructible<TypeParam>::value) ==
                      std::is_trivially_move_assignable<optional<TypeParam>>::value,
                  "");
    static_assert(std::is_trivially_destructible<TypeParam>::value ==
                      std::is_trivially_destructible<optional<TypeParam>>::value,
                  "");

    // Ensure implicit convertibility is inherited from the value type.
    // Note that these checks behave differently in C++14 vs. newer standards because in C++14,
    // std::is_convertible<F, T> is not true if T is not copyable, even if F is convertible to T.
    static_assert(std::is_convertible<foo<TypeParam>, bar<TypeParam>>::value ==
                      std::is_convertible<optional<foo<TypeParam>>, optional<bar<TypeParam>>>::value,
                  "");
    static_assert(std::is_convertible<bar<TypeParam>, foo<TypeParam>>::value ==
                      std::is_convertible<optional<bar<TypeParam>>, optional<foo<TypeParam>>>::value,
                  "");

    // Ensure explicit convertibility is inherited from the value type.
    static_assert(std::is_constructible<bar<TypeParam>, foo<TypeParam>>::value ==
                      std::is_constructible<optional<bar<TypeParam>>, optional<foo<TypeParam>>>::value,
                  "");
    static_assert(std::is_constructible<foo<TypeParam>, bar<TypeParam>>::value ==
                      std::is_constructible<optional<foo<TypeParam>>, optional<bar<TypeParam>>>::value,
                  "");

    // Runtime tests.
    std::uint32_t            destruction_count = 0;
    optional<foo<TypeParam>> opt;
    EXPECT_FALSE(opt.has_value());
    EXPECT_FALSE(opt);
    opt.emplace(12345).configure_destruction_counter(&destruction_count);
    EXPECT_TRUE(opt.has_value());
    EXPECT_TRUE(opt);
    EXPECT_EQ(0, destruction_count);
    EXPECT_EQ(12345, opt->value);
    EXPECT_EQ(12345, (*opt).value);
    EXPECT_EQ(12345, (*std::move(opt)).value);
    EXPECT_EQ(12345, opt.value().value);
    EXPECT_EQ(12345, std::move(opt).value().value);
    {
        const auto& copt = opt;
        EXPECT_EQ(12345, copt->value);
        EXPECT_EQ(12345, (*copt).value);
        EXPECT_EQ(12345, (*std::move(copt)).value);
        EXPECT_EQ(12345, copt.value().value);
        EXPECT_EQ(12345, std::move(copt).value().value);
    }
    EXPECT_EQ(0, opt->get_copy_ctor_count());
    EXPECT_EQ(0, opt->get_move_ctor_count());
    EXPECT_EQ(0, opt->get_copy_assignment_count());
    EXPECT_EQ(0, opt->get_move_assignment_count());
    EXPECT_EQ(0, destruction_count);
    opt = cetl::pf17::nullopt;
    EXPECT_FALSE(opt);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, destruction_count);
    opt.emplace(std::initializer_list<std::int64_t>{1, 2, 3, 4, 5}).configure_destruction_counter(&destruction_count);
    EXPECT_TRUE(opt);
    EXPECT_EQ(5, opt->value);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, destruction_count);
    EXPECT_EQ(0, opt->get_copy_ctor_count());
    EXPECT_EQ(0, opt->get_move_ctor_count());
    EXPECT_EQ(0, opt->get_copy_assignment_count());
    EXPECT_EQ(0, opt->get_move_assignment_count());
    opt.reset();
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 2 : 0, destruction_count);
}

/// ------------------------------------------------------------------------------------------------

#if defined(__cpp_exceptions)
TYPED_TEST(test_optional_combinations, exceptions)
{
    optional<TypeParam> opt;
    const auto sink = [](auto&&) {};  // Workaround for GCC bug https://gcc.gnu.org/bugzilla/show_bug.cgi?id=66425
    EXPECT_THROW(sink(opt.value()), cetl::pf17::bad_optional_access);
    EXPECT_THROW(sink(std::move(opt).value()), cetl::pf17::bad_optional_access);
    opt.emplace();
    EXPECT_NO_THROW(sink(opt.value()));
    EXPECT_NO_THROW(sink(std::move(opt).value()));
}
#endif

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations, ctor_1)
{
    optional<TypeParam> opt1;
    EXPECT_FALSE(opt1.has_value());
    optional<TypeParam> opt2(nullopt);
    EXPECT_FALSE(opt2.has_value());
}

/// ------------------------------------------------------------------------------------------------

template <typename T, std::uint8_t CopyCtorPolicy = T::copy_ctor_policy_value>
struct test_ctor_2
{
    static void test()
    {
        std::uint32_t destructed = 0;
        optional<T>   opt;
        opt.emplace().configure_destruction_counter(&destructed);
        {
            optional<T> opt2 = opt;
            EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_copy_ctor_count());
            EXPECT_EQ(0U, opt2->get_move_ctor_count());
            EXPECT_EQ(0U, opt2->get_copy_assignment_count());
            EXPECT_EQ(0U, opt2->get_move_assignment_count());
            EXPECT_EQ(0U, destructed);
            EXPECT_EQ(0U, opt->get_copy_ctor_count());
            EXPECT_EQ(0U, opt->get_move_ctor_count());
            EXPECT_EQ(0U, opt->get_copy_assignment_count());
            EXPECT_EQ(0U, opt->get_move_assignment_count());
            opt.reset();
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
    }
};
template <typename T>
struct test_ctor_2<T, policy_deleted>
{
    static void test()
    {
        static_assert(!std::is_copy_constructible<T>::value, "");
        static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    }
};

TYPED_TEST(test_optional_combinations, ctor_2)
{
    test_ctor_2<TypeParam>::test();
}

/// ------------------------------------------------------------------------------------------------

// Caveat: types without a move constructor but with a copy constructor that accepts const T& arguments,
// satisfy std::is_move_constructible.
template <typename T, std::uint8_t MoveCtorPolicy = T::move_ctor_policy_value>
struct test_ctor_3
{
    static void test()
    {
        std::uint32_t destructed = 0;
        optional<T>   opt;
        opt.emplace().configure_destruction_counter(&destructed);
        {
            optional<T> opt2 = std::move(opt);
            EXPECT_EQ(0, opt2->get_copy_ctor_count());
            EXPECT_EQ((T::move_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_move_ctor_count());
            EXPECT_EQ(0U, opt2->get_copy_assignment_count());
            EXPECT_EQ(0U, opt2->get_move_assignment_count());
            EXPECT_EQ(0U, destructed);
            EXPECT_EQ(0U, opt->get_copy_ctor_count());
            EXPECT_EQ(0U, opt->get_move_ctor_count());
            EXPECT_EQ(0U, opt->get_copy_assignment_count());
            EXPECT_EQ(0U, opt->get_move_assignment_count());
            opt.reset();
            EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        }
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
    }
};
template <typename T>
struct test_ctor_3<T, policy_deleted>  // FIXME: allow testing if either copy or move ctors available.
{
    static void test()
    {
        // Caveat: types without a move constructor but with a copy constructor that accepts const T& arguments,
        // satisfy std::is_move_constructible.
        static_assert(std::is_move_constructible<T>::value == (T::copy_ctor_policy_value != policy_deleted), "");
        static_assert(std::is_move_constructible<optional<T>>::value == (T::copy_ctor_policy_value != policy_deleted),
                      "");
    }
};

TYPED_TEST(test_optional_combinations, ctor_3)
{
    test_ctor_3<TypeParam>::test();
}

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations, ctor_4)
{
    using F = foo<TypeParam>;
    using B = bar<TypeParam>;
    static_assert(std::is_constructible<F, B>::value, "");
    static_assert(std::is_constructible<B, F>::value, "");
    static_assert(std::is_constructible<optional<F>, optional<B>>::value, "");
    static_assert(std::is_constructible<optional<B>, optional<F>>::value, "");
    std::uint32_t f_dtor = 0;
    std::uint32_t b_dtor = 0;
    optional<F>   f1;
    f1.emplace(12345).configure_destruction_counter(&f_dtor);
    optional<B> b1(f1);  // Use implicit constructor because foo is implicitly convertible to bar
    b1.value().configure_destruction_counter(&b_dtor);
    {
        optional<F> f2(b1);  // Use explicit constructor because bar is not implicitly convertible to foo
        f2.value().configure_destruction_counter(&f_dtor);
        EXPECT_EQ(12345, f1.value().value);
        EXPECT_EQ(12345, b1.value().value);
        EXPECT_EQ(12345, f2.value().value);
        // Ensure no copy/move of the base took place.
        EXPECT_EQ(0, f1->get_copy_ctor_count());
        EXPECT_EQ(0, f1->get_move_ctor_count());
        EXPECT_EQ(0, f1->get_copy_assignment_count());
        EXPECT_EQ(0, f1->get_move_assignment_count());
        EXPECT_EQ(0, b1->get_copy_ctor_count());
        EXPECT_EQ(0, b1->get_move_ctor_count());
        EXPECT_EQ(0, b1->get_copy_assignment_count());
        EXPECT_EQ(0, b1->get_move_assignment_count());
        EXPECT_EQ(0, f2->get_copy_ctor_count());
        EXPECT_EQ(0, f2->get_move_ctor_count());
        EXPECT_EQ(0, f2->get_copy_assignment_count());
        EXPECT_EQ(0, f2->get_move_assignment_count());
        EXPECT_EQ(0, f_dtor);
        EXPECT_EQ(0, b_dtor);
    }
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ(0, b_dtor);
    b1.reset();
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
    f1.reset();
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 2 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
}

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations, ctor_5)
{
    using F = foo<TypeParam>;
    using B = bar<TypeParam>;
    static_assert(std::is_constructible<F, B>::value, "");
    static_assert(std::is_constructible<B, F>::value, "");
    static_assert(std::is_constructible<optional<F>, optional<B>>::value, "");
    static_assert(std::is_constructible<optional<B>, optional<F>>::value, "");
    std::uint32_t f_dtor = 0;
    std::uint32_t b_dtor = 0;
    optional<F>   f1;
    f1.emplace(12345).configure_destruction_counter(&f_dtor);
    optional<B> b1(std::move(f1));   // Use implicit constructor because foo is implicitly convertible to bar
    EXPECT_EQ(0, f1.value().value);  // Moving zeroes the source.
    EXPECT_EQ(12345, b1.value().value);
    b1.value().configure_destruction_counter(&b_dtor);
    {
        optional<F> f2(std::move(b1));  // Use explicit constructor because bar is not implicitly convertible to foo
        f2.value().configure_destruction_counter(&f_dtor);
        EXPECT_EQ(0, f1.value().value);
        EXPECT_EQ(0, b1.value().value);  // Moving zeroes the source.
        EXPECT_EQ(12345, f2.value().value);
        // Ensure no copy/move of the base took place.
        EXPECT_EQ(0, f1->get_copy_ctor_count());
        EXPECT_EQ(0, f1->get_move_ctor_count());
        EXPECT_EQ(0, f1->get_copy_assignment_count());
        EXPECT_EQ(0, f1->get_move_assignment_count());
        EXPECT_EQ(0, b1->get_copy_ctor_count());
        EXPECT_EQ(0, b1->get_move_ctor_count());
        EXPECT_EQ(0, b1->get_copy_assignment_count());
        EXPECT_EQ(0, b1->get_move_assignment_count());
        EXPECT_EQ(0, f2->get_copy_ctor_count());
        EXPECT_EQ(0, f2->get_move_ctor_count());
        EXPECT_EQ(0, f2->get_copy_assignment_count());
        EXPECT_EQ(0, f2->get_move_assignment_count());
        EXPECT_EQ(0, f_dtor);
        EXPECT_EQ(0, b_dtor);
    }
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ(0, b_dtor);
    b1.reset();
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
    f1.reset();
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 2 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
}

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations, ctor_6)
{
    std::uint32_t            f_dtor = 0;
    optional<foo<TypeParam>> f1(in_place, 12345);
    f1.value().configure_destruction_counter(&f_dtor);
    EXPECT_TRUE(f1.has_value());
    EXPECT_EQ(12345, f1.value().value);
    // Ensure no copy/move of the base took place.
    EXPECT_EQ(0, f1->get_copy_ctor_count());
    EXPECT_EQ(0, f1->get_move_ctor_count());
    EXPECT_EQ(0, f1->get_copy_assignment_count());
    EXPECT_EQ(0, f1->get_move_assignment_count());
    EXPECT_EQ(0, f_dtor);
    f1 = nullopt;
    EXPECT_FALSE(f1);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
}

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations, ctor_7)
{
    std::uint32_t            f_dtor = 0;
    optional<foo<TypeParam>> f1(in_place, std::initializer_list<std::int64_t>{1, 2, 3, 4, 5});
    f1.value().configure_destruction_counter(&f_dtor);
    EXPECT_TRUE(f1.has_value());
    EXPECT_EQ(5, f1.value().value);
    // Ensure no copy/move of the base took place.
    EXPECT_EQ(0, f1->get_copy_ctor_count());
    EXPECT_EQ(0, f1->get_move_ctor_count());
    EXPECT_EQ(0, f1->get_copy_assignment_count());
    EXPECT_EQ(0, f1->get_move_assignment_count());
    EXPECT_EQ(0, f_dtor);
    f1 = nullopt;
    EXPECT_FALSE(f1);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
}

/// ------------------------------------------------------------------------------------------------

static_assert(cetl::pf17::detail::opt::enable_ctor8<bar<noncopyable_t>, foo<noncopyable_t>, false>, "");
static_assert(cetl::pf17::detail::opt::enable_ctor8<foo<noncopyable_t>, bar<noncopyable_t>, true>, "");

TYPED_TEST(test_optional_combinations, ctor_8)
{
    std::uint32_t            f_dtor = 0;
    std::uint32_t            b_dtor = 0;
    optional<foo<TypeParam>> f1(12345);  // Use explicit constructor.
    optional<bar<TypeParam>> b1(23456);  // Use implicit constructor.
    f1.value().configure_destruction_counter(&f_dtor);
    b1.value().configure_destruction_counter(&b_dtor);
    EXPECT_TRUE(f1.has_value());
    EXPECT_EQ(12345, f1.value().value);
    EXPECT_TRUE(b1.has_value());
    EXPECT_EQ(23456, b1.value().value);
    // Ensure no copy/move of the base took place.
    EXPECT_EQ(0, f1->get_copy_ctor_count());
    EXPECT_EQ(0, f1->get_move_ctor_count());
    EXPECT_EQ(0, f1->get_copy_assignment_count());
    EXPECT_EQ(0, f1->get_move_assignment_count());
    EXPECT_EQ(0, f_dtor);
    EXPECT_EQ(0, b1->get_copy_ctor_count());
    EXPECT_EQ(0, b1->get_move_ctor_count());
    EXPECT_EQ(0, b1->get_copy_assignment_count());
    EXPECT_EQ(0, b1->get_move_assignment_count());
    EXPECT_EQ(0, b_dtor);
    f1 = nullopt;
    b1 = nullopt;
    EXPECT_FALSE(f1);
    EXPECT_FALSE(b1);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, f_dtor);
    EXPECT_EQ((TypeParam::dtor_policy_value == policy_nontrivial) ? 1 : 0, b_dtor);
}

/// ------------------------------------------------------------------------------------------------

TYPED_TEST(test_optional_combinations, assignment_1)
{
    optional<TypeParam> opt1(in_place);
    EXPECT_TRUE(opt1.has_value());
    opt1 = nullopt;
    EXPECT_FALSE(opt1.has_value());
}

/// ------------------------------------------------------------------------------------------------

/// For the copy assignment to work, T shall be both copy-constructible and copy-assignable.
template <typename T,
          std::uint8_t CopyCtorPolicy       = T::copy_ctor_policy_value,
          std::uint8_t CopyAssignmentPolicy = T::copy_assignment_policy_value>
struct test_assignment_2
{
    static void test()
    {
        std::uint32_t destructed = 0;
        optional<T>   opt1;
        optional<T>   opt2;
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        // Empty to empty.
        opt1 = opt2;
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        // Non-empty to empty. A copy ctor is invoked.
        opt1.emplace().configure_destruction_counter(&destructed);
        EXPECT_TRUE(opt1);
        EXPECT_FALSE(opt2);
        opt2 = opt1;
        EXPECT_TRUE(opt1);
        EXPECT_TRUE(opt2);
        EXPECT_EQ(0U, opt1->get_copy_ctor_count());
        EXPECT_EQ(0U, opt1->get_move_ctor_count());
        EXPECT_EQ(0U, opt1->get_copy_assignment_count());
        EXPECT_EQ(0U, opt1->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_copy_ctor_count());
        EXPECT_EQ(0U, opt2->get_move_ctor_count());
        EXPECT_EQ(0U, opt2->get_copy_assignment_count());
        EXPECT_EQ(0U, opt2->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        // Non-empty to non-empty. A copy assignment is invoked.
        opt1 = opt2;
        EXPECT_TRUE(opt1);
        EXPECT_TRUE(opt2);
        // The copy ctor count is copied over from opt2!
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_copy_ctor_count());
        EXPECT_EQ(0U, opt1->get_move_ctor_count());
        EXPECT_EQ((T::copy_assignment_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_copy_assignment_count());
        EXPECT_EQ(0U, opt1->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_copy_ctor_count());
        EXPECT_EQ(0U, opt2->get_move_ctor_count());
        EXPECT_EQ(0U, opt2->get_copy_assignment_count());
        EXPECT_EQ(0U, opt2->get_move_assignment_count());
        EXPECT_EQ(0U, destructed);
        // Empty to non-empty. Destructor is invoked.
        opt1 = nullopt;
        EXPECT_FALSE(opt1);
        EXPECT_TRUE(opt2);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1 : 0, destructed);
        opt2 = opt1;
        EXPECT_FALSE(opt1);
        EXPECT_FALSE(opt2);
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 2 : 0, destructed);
        // Other counters unchanged.
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_copy_ctor_count());
        EXPECT_EQ(0U, opt1->get_move_ctor_count());
        EXPECT_EQ((T::copy_assignment_policy_value == policy_nontrivial) ? 1 : 0, opt1->get_copy_assignment_count());
        EXPECT_EQ(0U, opt1->get_move_assignment_count());
        EXPECT_EQ((T::copy_ctor_policy_value == policy_nontrivial) ? 1 : 0, opt2->get_copy_ctor_count());
        EXPECT_EQ(0U, opt2->get_move_ctor_count());
        EXPECT_EQ(0U, opt2->get_copy_assignment_count());
        EXPECT_EQ(0U, opt2->get_move_assignment_count());
    }
};
template <typename T, std::uint8_t CopyCtorPolicy>
struct test_assignment_2<T, CopyCtorPolicy, policy_deleted>
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static void test() {}
};
template <typename T, std::uint8_t CopyAssignmentPolicy>
struct test_assignment_2<T, policy_deleted, CopyAssignmentPolicy>
{
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static void test() {}
};
template <typename T>
struct test_assignment_2<T, policy_deleted, policy_deleted>  // This is to avoid the specialization ambiguity.
{
    static_assert(!std::is_copy_assignable<T>::value, "");
    static_assert(!std::is_copy_constructible<T>::value, "");
    static_assert(!std::is_copy_constructible<optional<T>>::value, "");
    static_assert(!std::is_copy_assignable<optional<T>>::value, "");
    static void test() {}
};

TYPED_TEST(test_optional_combinations, assignment_2)
{
    test_assignment_2<TypeParam>::test();
}
