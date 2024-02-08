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

struct empty_t
{};

/// A simple pair of types for testing where foo is implicitly convertible to bar but not vice versa.
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
    explicit operator foo<Base>() const noexcept
    {
        return foo<Base>{value};
    }
    std::int64_t value;
};

// Check implicit conversions.
static_assert(std::is_convertible<foo<empty_t>, bar<empty_t>>::value, "");
static_assert(!std::is_convertible<bar<empty_t>, foo<empty_t>>::value, "");
static_assert(std::is_convertible<optional<foo<empty_t>>, optional<bar<empty_t>>>::value, "");
static_assert(!std::is_convertible<optional<bar<empty_t>>, optional<foo<empty_t>>>::value, "");
// Check explicit conversions.
static_assert(std::is_constructible<bar<empty_t>, foo<empty_t>>::value, "");
static_assert(std::is_constructible<foo<empty_t>, bar<empty_t>>::value, "");
static_assert(std::is_constructible<optional<bar<empty_t>>, optional<foo<empty_t>>>::value, "");
static_assert(std::is_constructible<optional<foo<empty_t>>, optional<bar<empty_t>>>::value, "");
// Check triviality of foo.
static_assert(std::is_trivially_copy_constructible<optional<foo<empty_t>>>::value, "");
static_assert(std::is_trivially_move_constructible<optional<foo<empty_t>>>::value, "");
static_assert(std::is_trivially_copy_assignable<optional<foo<empty_t>>>::value, "");
static_assert(std::is_trivially_move_assignable<optional<foo<empty_t>>>::value, "");
static_assert(std::is_trivially_destructible<optional<foo<empty_t>>>::value, "");
// Check triviality of bar.
static_assert(std::is_trivially_copy_constructible<optional<bar<empty_t>>>::value, "");
static_assert(std::is_trivially_move_constructible<optional<bar<empty_t>>>::value, "");
static_assert(std::is_trivially_copy_assignable<optional<bar<empty_t>>>::value, "");
static_assert(std::is_trivially_move_assignable<optional<bar<empty_t>>>::value, "");
static_assert(std::is_trivially_destructible<optional<bar<empty_t>>>::value, "");

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
struct test_ctor_3<T, policy_deleted>
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

template <typename T, std::uint8_t CopyCtorPolicy = T::copy_ctor_policy_value>
struct test_ctor_8
{
    struct value_type final : public T
    {
        explicit value_type(const std::uint8_t val)
            : value{val}
        {
        }
        std::uint8_t value;
    };
    static void test()
    {
        std::uint32_t val_destructed = 0;
        std::uint32_t opt_destructed = 0;
        value_type    val(123U);
        val.configure_destruction_counter(&val_destructed);
        optional<value_type> opt(val);
        opt->configure_destruction_counter(&opt_destructed);
        EXPECT_EQ(0U, val.get_copy_ctor_count());
        EXPECT_EQ(0U, val.get_move_ctor_count());
        EXPECT_EQ(0U, val.get_copy_assignment_count());
        EXPECT_EQ(0U, val.get_move_assignment_count());
        EXPECT_EQ(0U, val_destructed);
        value_type& inner = opt.value();
        EXPECT_EQ((CopyCtorPolicy == policy_nontrivial) ? 1U : 0, inner.get_copy_ctor_count());
        EXPECT_EQ(0U, inner.get_move_ctor_count());
        EXPECT_EQ(0U, inner.get_copy_assignment_count());
        EXPECT_EQ(0U, inner.get_move_assignment_count());
        EXPECT_EQ(0U, opt_destructed);
        opt.reset();
        EXPECT_EQ((T::dtor_policy_value == policy_nontrivial) ? 1U : 0, opt_destructed);
    }
};
template <typename PolicyType>
struct test_ctor_8<PolicyType, policy_deleted>
{
    static void test()
    {
        static_assert(!std::is_copy_constructible<PolicyType>::value, "");
        static_assert(!std::is_copy_constructible<optional<PolicyType>>::value, "");
    }
};

TYPED_TEST(test_optional_combinations, ctor_8)
{
    test_ctor_8<TypeParam>::test();
}

/// ------------------------------------------------------------------------------------------------
