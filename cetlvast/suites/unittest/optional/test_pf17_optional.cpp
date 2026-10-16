/// @file
/// Unit tests for cetl/pf17/optional.hpp
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

// +******************************************************************************************************************+
// * WARNING:
// *    This test can take a _very_ long time to compile. No, the build isn't stuck. Ninja is purposefully very
// * quiet and the time is all spent doing some really-big-O operations when processing all the template code.
// * This is a complex set of test templates used to test a complex set of templates (optional).
// *
// * For example: on a virt one user saw a 4 minute compilation time for this one file:
// *    [driver] Build completed: 00:04:14.217
// *
// * Be patient. Sorry.
// +******************************************************************************************************************+

#include "test_pf17_optional.hpp"
#include "test_pf17_optional_combinations.hpp"

// NOLINTBEGIN(*-use-after-move)

using cetl::pf17::optional;
using cetl::pf17::nullopt;

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
// ctor8
static_assert(cetl::pf17::detail::opt::enable_ctor8<bar<noncopyable_t>, foo<noncopyable_t>, false>, "");
static_assert(cetl::pf17::detail::opt::enable_ctor8<foo<noncopyable_t>, bar<noncopyable_t>, true>, "");

// Explicitly deleted moves differ from defaulted moves that become deleted: the former must
// disable optional's defaulted moves so overload resolution can fall back to its copy operations.
struct copy_only final
{
    copy_only() noexcept = default;
    copy_only(const copy_only& other) noexcept
        : copies(other.copies + 1)
    {
    }
    copy_only(copy_only&&) = delete;
    copy_only& operator=(const copy_only& other) noexcept
    {
        copies = other.copies + 1;
        return *this;
    }
    copy_only& operator=(copy_only&&) = delete;
    ~copy_only()                      = default;
    int copies                        = 0;
};

static_assert(!std::is_move_constructible<copy_only>::value, "");
static_assert(!std::is_move_assignable<copy_only>::value, "");
static_assert(std::is_nothrow_move_constructible<optional<copy_only>>::value, "");
static_assert(std::is_nothrow_move_assignable<optional<copy_only>>::value, "");

// The policy layers delete the special members that T lacks instead of declaring non-trivial ones, so optional<T>
// stays trivially copyable when each of T's special members is trivial or deleted; std::optional reports false for
// these types. The policy layers are optional's only bases, so nesting optionals adds no empty base subobjects.
// https://github.com/OpenCyphal/CETL/issues/125
struct trivial_copy_only final
{
    trivial_copy_only()                                    = default;
    trivial_copy_only(const trivial_copy_only&)            = default;
    trivial_copy_only(trivial_copy_only&&)                 = delete;
    trivial_copy_only& operator=(const trivial_copy_only&) = default;
    trivial_copy_only& operator=(trivial_copy_only&&)      = delete;
    ~trivial_copy_only()                                   = default;
};

static_assert(std::is_trivially_copyable<trivial_copy_only>::value, "");
static_assert(std::is_trivially_copyable<optional<trivial_copy_only>>::value, "");
static_assert(std::is_trivially_copyable<optional<optional<trivial_copy_only>>>::value, "");
static_assert(std::is_trivially_copyable<optional<const int>>::value, "");
static_assert(sizeof(optional<optional<char>>) == 3, "");

TEST(test_optional, explicitly_deleted_moves_fall_back_to_copy)
{
    optional<copy_only> source{cetl::pf17::in_place};
    optional<copy_only> destination{std::move(source)};
    ASSERT_TRUE(source);
    ASSERT_TRUE(destination);
    EXPECT_EQ(0, source->copies);
    EXPECT_EQ(1, destination->copies);
    destination = std::move(source);
    EXPECT_EQ(1, destination->copies);
    destination.reset();
    destination = std::move(source);
    ASSERT_TRUE(destination);
    EXPECT_EQ(1, destination->copies);
    source.reset();
    destination = std::move(source);
    EXPECT_FALSE(destination);
}

// The policy layers must not give optional a stronger exception specification than T's copy and move operations,
// or an exception thrown by T would be reported as impossible and then call std::terminate.
// https://github.com/OpenCyphal/CETL/issues/220
struct copy_move_failure final
{
    static void raise()
    {
#if defined(__cpp_exceptions)
        throw copy_move_failure{};
#endif
    }
};

struct throwing_copy final
{
    throwing_copy() noexcept = default;
    throwing_copy(const throwing_copy&) noexcept(false)
    {
        copy_move_failure::raise();
    }
    throwing_copy(throwing_copy&&) noexcept = default;
    throwing_copy& operator=(const throwing_copy&) noexcept(false)
    {
        copy_move_failure::raise();
        return *this;
    }
    throwing_copy& operator=(throwing_copy&&) noexcept = default;
    ~throwing_copy()                                   = default;
};

/// The explicitly deleted moves make the moves of optional fall back to the throwing copy.
struct throwing_copy_only final
{
    throwing_copy_only() noexcept = default;
    throwing_copy_only(const throwing_copy_only&) noexcept(false)
    {
        copy_move_failure::raise();
    }
    throwing_copy_only(throwing_copy_only&&) = delete;
    throwing_copy_only& operator=(const throwing_copy_only&) noexcept(false)
    {
        copy_move_failure::raise();
        return *this;
    }
    throwing_copy_only& operator=(throwing_copy_only&&) = delete;
    ~throwing_copy_only()                               = default;
};

struct throwing_move_only final
{
    throwing_move_only() noexcept                            = default;
    throwing_move_only(const throwing_move_only&)            = delete;
    throwing_move_only& operator=(const throwing_move_only&) = delete;
    throwing_move_only(throwing_move_only&&) noexcept(false)
    {
        copy_move_failure::raise();
    }
    throwing_move_only& operator=(throwing_move_only&&) noexcept(false)
    {
        copy_move_failure::raise();
        return *this;
    }
    ~throwing_move_only() = default;
};

static_assert(!std::is_nothrow_copy_constructible<optional<throwing_copy>>::value, "");
static_assert(!std::is_nothrow_copy_assignable<optional<throwing_copy>>::value, "");
static_assert(std::is_nothrow_move_constructible<optional<throwing_copy>>::value, "");
static_assert(std::is_nothrow_move_assignable<optional<throwing_copy>>::value, "");

static_assert(!std::is_nothrow_copy_constructible<optional<throwing_copy_only>>::value, "");
static_assert(!std::is_nothrow_copy_assignable<optional<throwing_copy_only>>::value, "");
static_assert(!std::is_nothrow_move_constructible<optional<throwing_copy_only>>::value, "");
static_assert(!std::is_nothrow_move_assignable<optional<throwing_copy_only>>::value, "");

static_assert(!std::is_nothrow_move_constructible<optional<throwing_move_only>>::value, "");
static_assert(!std::is_nothrow_move_assignable<optional<throwing_move_only>>::value, "");

#if defined(__cpp_exceptions)
TEST(test_optional, throwing_copy_propagates)
{
    const optional<throwing_copy> source{cetl::pf17::in_place};
    EXPECT_THROW(optional<throwing_copy>{source}, copy_move_failure);
    optional<throwing_copy> destination;
    EXPECT_THROW(destination = source, copy_move_failure);
    EXPECT_FALSE(destination);
    destination.emplace();
    EXPECT_THROW(destination = source, copy_move_failure);
    EXPECT_TRUE(destination);
}

TEST(test_optional, throwing_copy_only_propagates)
{
    optional<throwing_copy_only> source{cetl::pf17::in_place};
    EXPECT_THROW(optional<throwing_copy_only>{source}, copy_move_failure);
    EXPECT_THROW(optional<throwing_copy_only>{std::move(source)}, copy_move_failure);
    optional<throwing_copy_only> destination;
    EXPECT_THROW(destination = source, copy_move_failure);
    EXPECT_FALSE(destination);
    EXPECT_THROW(destination = std::move(source), copy_move_failure);
    EXPECT_FALSE(destination);
    destination.emplace();
    EXPECT_THROW(destination = source, copy_move_failure);
    EXPECT_TRUE(destination);
    EXPECT_THROW(destination = std::move(source), copy_move_failure);
    EXPECT_TRUE(destination);
}

TEST(test_optional, throwing_move_only_propagates)
{
    optional<throwing_move_only> source{cetl::pf17::in_place};
    EXPECT_THROW(optional<throwing_move_only>{std::move(source)}, copy_move_failure);
    optional<throwing_move_only> destination;
    EXPECT_THROW(destination = std::move(source), copy_move_failure);
    EXPECT_FALSE(destination);
    destination.emplace();
    EXPECT_THROW(destination = std::move(source), copy_move_failure);
    EXPECT_TRUE(destination);
}
#endif

/// This wrapper is used to test the comparison operators. It is made non-copyable and non-movable to ensure that the
/// comparison operators are not using the copy/move constructors or assignment operators.
template <typename T>
struct comparable final
{
    explicit constexpr comparable(const T& value)
        : value(value)
    {
    }
    comparable(const comparable&)            = delete;
    comparable(comparable&&)                 = delete;
    comparable& operator=(const comparable&) = delete;
    comparable& operator=(comparable&&)      = delete;
    ~comparable()                            = default;
    T value;
};

template <typename T, typename U>
constexpr bool operator==(const comparable<T>& lhs, const comparable<U>& rhs)
{
    return lhs.value == rhs.value;
}
template <typename T, typename U>
constexpr bool operator!=(const comparable<T>& lhs, const comparable<U>& rhs)
{
    return lhs.value != rhs.value;
}
template <typename T, typename U>
constexpr bool operator<(const comparable<T>& lhs, const comparable<U>& rhs)
{
    return lhs.value < rhs.value;
}
template <typename T, typename U>
constexpr bool operator<=(const comparable<T>& lhs, const comparable<U>& rhs)
{
    return lhs.value <= rhs.value;
}
template <typename T, typename U>
constexpr bool operator>(const comparable<T>& lhs, const comparable<U>& rhs)
{
    return lhs.value > rhs.value;
}
template <typename T, typename U>
constexpr bool operator>=(const comparable<T>& lhs, const comparable<U>& rhs)
{
    return lhs.value >= rhs.value;
}

TEST(test_optional, comparison_optional_to_optional)
{
    using A = optional<comparable<std::int64_t>>;
    using B = optional<comparable<std::int32_t>>;
    // ==
    EXPECT_TRUE(A{} == B{});
    EXPECT_TRUE(A{10} == B{10});
    EXPECT_FALSE(A{} == B{10});
    EXPECT_FALSE(A{10} == B{});
    // !=
    EXPECT_FALSE(A{} != B{});
    EXPECT_FALSE(A{10} != B{10});
    EXPECT_TRUE(A{} != B{10});
    EXPECT_TRUE(A{10} != B{});
    // <
    EXPECT_FALSE(A{} < B{});
    EXPECT_FALSE(A{10} < B{10});
    EXPECT_TRUE(A{} < B{10});
    EXPECT_FALSE(A{10} < B{});
    // <=
    EXPECT_TRUE(A{} <= B{});
    EXPECT_TRUE(A{10} <= B{10});
    EXPECT_TRUE(A{} <= B{10});
    EXPECT_FALSE(A{10} <= B{});
    // >
    EXPECT_FALSE(A{} > B{});
    EXPECT_FALSE(A{10} > B{10});
    EXPECT_FALSE(A{} > B{10});
    EXPECT_TRUE(A{10} > B{});
    // >=
    EXPECT_TRUE(A{} >= B{});
    EXPECT_TRUE(A{10} >= B{10});
    EXPECT_FALSE(A{} >= B{10});
    EXPECT_TRUE(A{10} >= B{});
}

TEST(test_optional, comparison_optional_to_nullopt)
{
    using A = optional<comparable<std::int64_t>>;
    // ==
    EXPECT_TRUE(A{} == nullopt);
    EXPECT_TRUE(nullopt == A{});
    EXPECT_FALSE(A{10} == nullopt);
    EXPECT_FALSE(nullopt == A{10});
    // !=
    EXPECT_FALSE(A{} != nullopt);
    EXPECT_FALSE(nullopt != A{});
    EXPECT_TRUE(A{10} != nullopt);
    EXPECT_TRUE(nullopt != A{10});
    // <
    EXPECT_FALSE(A{} < nullopt);
    EXPECT_FALSE(nullopt < A{});
    EXPECT_FALSE(A{10} < nullopt);
    EXPECT_TRUE(nullopt < A{10});
    // <=
    EXPECT_TRUE(A{} <= nullopt);
    EXPECT_TRUE(nullopt <= A{});
    EXPECT_FALSE(A{10} <= nullopt);
    EXPECT_TRUE(nullopt <= A{10});
    // >
    EXPECT_FALSE(A{} > nullopt);
    EXPECT_FALSE(nullopt > A{});
    EXPECT_TRUE(A{10} > nullopt);
    EXPECT_FALSE(nullopt > A{10});
    // >=
    EXPECT_TRUE(A{} >= nullopt);
    EXPECT_TRUE(nullopt >= A{});
    EXPECT_TRUE(A{10} >= nullopt);
    EXPECT_FALSE(nullopt >= A{10});
}

TEST(test_optional, comparison_optional_to_value)
{
    using A = optional<std::int64_t>;
    // ==
    EXPECT_FALSE(A{} == 10);
    EXPECT_FALSE(10 == A{});
    EXPECT_TRUE(A{10} == 10);
    EXPECT_TRUE(10 == A{10});
    EXPECT_FALSE(A{10} == 0);
    EXPECT_FALSE(0 == A{10});
    // !=
    EXPECT_TRUE(A{} != 10);
    EXPECT_TRUE(10 != A{});
    EXPECT_FALSE(A{10} != 10);
    EXPECT_FALSE(10 != A{10});
    EXPECT_TRUE(A{10} != 0);
    EXPECT_TRUE(0 != A{10});
    // <
    EXPECT_TRUE(A{} < 10);
    EXPECT_FALSE(10 < A{});
    EXPECT_FALSE(A{10} < 10);
    EXPECT_FALSE(10 < A{10});
    EXPECT_FALSE(A{10} < 0);
    EXPECT_TRUE(0 < A{10});
    // <=
    EXPECT_TRUE(A{} <= 10);
    EXPECT_FALSE(10 <= A{});
    EXPECT_TRUE(A{10} <= 10);
    EXPECT_TRUE(10 <= A{10});
    EXPECT_FALSE(A{10} <= 0);
    EXPECT_TRUE(0 <= A{10});
    // >
    EXPECT_FALSE(A{} > 10);
    EXPECT_TRUE(10 > A{});
    EXPECT_FALSE(A{10} > 10);
    EXPECT_FALSE(10 > A{10});
    EXPECT_TRUE(A{10} > 0);
    EXPECT_FALSE(0 > A{10});
    // >=
    EXPECT_FALSE(A{} >= 10);
    EXPECT_TRUE(10 >= A{});
    EXPECT_TRUE(A{10} >= 10);
    EXPECT_TRUE(10 >= A{10});
    EXPECT_TRUE(A{10} >= 0);
    EXPECT_FALSE(0 >= A{10});
}

template <typename>
class test_optional_combinations_common : public ::testing::Test
{};

TYPED_TEST_SUITE(test_optional_combinations_common, testing_types, );

using cetl::pf17::optional;

TYPED_TEST(test_optional_combinations_common, common)
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

#if defined(__cpp_exceptions)
TYPED_TEST(test_optional_combinations_common, exceptions)
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

// NOLINTEND(*-use-after-move)
