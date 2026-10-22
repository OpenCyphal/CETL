/// @file
/// Common types for the unit tests of cetl/pf23/expected.hpp.
///
/// The tests are typed over an "implementation" so that, where the standard library provides std::expected with the
/// monadic operations, every behavioural test also runs against it. That checks the expectations in the tests, not
/// only the polyfill.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETLVAST_TEST_PF23_EXPECTED_HPP_INCLUDED
#define CETLVAST_TEST_PF23_EXPECTED_HPP_INCLUDED

#include "cetl/pf23/expected.hpp"
#include "cetlvast/helpers_gtest.hpp"

#include <exception>
#include <initializer_list>
#include <type_traits>
#include <utility>

#if (__cplusplus >= CETL_CPP_STANDARD_20)
#    include <version>
#endif
#if defined(__cpp_lib_expected) && (__cpp_lib_expected >= 202211L)
#    include <expected>
#    define CETLVAST_HAS_STD_EXPECTED 1
#else
#    define CETLVAST_HAS_STD_EXPECTED 0
#endif

namespace cetlvast
{
namespace pf23_expected
{

/// The polyfill.
struct cetl_impl
{
    template <typename T, typename E>
    using expected = cetl::pf23::expected<T, E>;
    template <typename E>
    using unexpected = cetl::pf23::unexpected<E>;
    using unexpect_t = cetl::pf23::unexpect_t;
#if __cplusplus >= CETL_CPP_STANDARD_17
    using in_place_t = std::in_place_t;
#else
    using in_place_t = cetl::pf17::in_place_t;
#endif
#if defined(__cpp_exceptions)
    template <typename E>
    using bad_expected_access = cetl::pf23::bad_expected_access<E>;
#endif
    static constexpr unexpect_t unexpect() noexcept
    {
        return unexpect_t{};
    }
    static constexpr in_place_t in_place() noexcept
    {
        return in_place_t{};
    }
};

#if CETLVAST_HAS_STD_EXPECTED
/// The standard library, as the oracle.
struct std_impl
{
    template <typename T, typename E>
    using expected = std::expected<T, E>;
    template <typename E>
    using unexpected = std::unexpected<E>;
    using unexpect_t = std::unexpect_t;
    using in_place_t = std::in_place_t;
#    if defined(__cpp_exceptions)
    template <typename E>
    using bad_expected_access = std::bad_expected_access<E>;
#    endif
    static constexpr unexpect_t unexpect() noexcept
    {
        return unexpect_t{};
    }
    static constexpr in_place_t in_place() noexcept
    {
        return in_place_t{};
    }
};
using implementations = ::testing::Types<cetl_impl, std_impl>;
#else
using implementations = ::testing::Types<cetl_impl>;
#endif

/// Uses a result, which a cast to void does not do for GCC's warn_unused_result (CETL_NODISCARD in C++14).
template <typename T>
void consume(const T&) noexcept
{
}

template <typename Impl, typename T, typename E>
using expected_t = typename Impl::template expected<T, E>;

template <typename Impl, typename E>
using unexpected_t = typename Impl::template unexpected<E>;

/// Counts the special member function calls of each instantiation, so that a test can check exactly what an operation
/// did to the value and the error.
struct counters
{
    int value_ctor;
    int copy_ctor;
    int move_ctor;
    int copy_assign;
    int move_assign;
    int dtor;

    int alive() const
    {
        return value_ctor + copy_ctor + move_ctor - dtor;
    }
};

template <int Tag>
struct counted
{
    static counters& stats()
    {
        static counters instance{};
        return instance;
    }
    static void reset()
    {
        stats() = counters{};
    }

    explicit counted(const int v = 0) noexcept
        : value{v}
    {
        ++stats().value_ctor;
    }
    counted(const int a, const int b) noexcept
        : value{a + b}
    {
        ++stats().value_ctor;
    }
    counted(std::initializer_list<int> il, const int b) noexcept
        : value{static_cast<int>(il.size()) + b}
    {
        ++stats().value_ctor;
    }
    counted(const counted& other) noexcept
        : value{other.value}
    {
        ++stats().copy_ctor;
    }
    counted(counted&& other) noexcept
        : value{other.value}
    {
        other.value = -1;
        ++stats().move_ctor;
    }
    counted& operator=(const counted& other) noexcept
    {
        value = other.value;
        ++stats().copy_assign;
        return *this;
    }
    counted& operator=(counted&& other) noexcept
    {
        value       = other.value;
        other.value = -1;
        ++stats().move_assign;
        return *this;
    }
    ~counted() noexcept
    {
        ++stats().dtor;
    }

    friend bool operator==(const counted& lhs, const counted& rhs) noexcept
    {
        return lhs.value == rhs.value;
    }
    friend bool operator!=(const counted& lhs, const counted& rhs) noexcept
    {
        return lhs.value != rhs.value;
    }

    int value;
};

using value_counted = counted<0>;
using error_counted = counted<1>;

/// Fixture that resets the counters and checks that every test leaves no value or error alive.
template <typename Impl>
class test_pf23_expected_base : public ::testing::Test
{
protected:
    void SetUp() override
    {
        value_counted::reset();
        error_counted::reset();
    }
    void TearDown() override
    {
        EXPECT_EQ(0, value_counted::stats().alive());
        EXPECT_EQ(0, error_counted::stats().alive());
    }
};

struct move_only
{
    explicit move_only(const int v = 0) noexcept
        : value{v}
    {
    }
    move_only(const move_only&)            = delete;
    move_only(move_only&&) noexcept        = default;
    move_only& operator=(const move_only&) = delete;
    move_only& operator=(move_only&&)      = default;
    ~move_only()                           = default;
    int value;
};

/// Copyable, but its move operations are deleted, so rvalues do not bind to its copy operations.
struct copy_only
{
    copy_only() = default;
    copy_only(const copy_only&) noexcept;
    copy_only(copy_only&&) = delete;
    copy_only& operator=(const copy_only&) noexcept;
    copy_only& operator=(copy_only&&) = delete;
    ~copy_only()                      = default;
};

struct non_movable
{
    non_movable()                              = default;
    non_movable(const non_movable&)            = delete;
    non_movable(non_movable&&)                 = delete;
    non_movable& operator=(const non_movable&) = delete;
    non_movable& operator=(non_movable&&)      = delete;
    ~non_movable()                             = default;
};

struct no_default
{
    explicit no_default(int) noexcept {}
};

/// Copy and move may throw (as far as the type system knows); they never actually throw.
struct maybe_throwing
{
    maybe_throwing() = default;
    maybe_throwing(const maybe_throwing&) noexcept(false);
    maybe_throwing(maybe_throwing&&) noexcept(false);
    maybe_throwing& operator=(const maybe_throwing&) noexcept(false);
    maybe_throwing& operator=(maybe_throwing&&) noexcept(false);
    ~maybe_throwing() = default;
};

/// An int whose copy and move are not noexcept, although they never throw.
struct potentially_throwing_int
{
    explicit potentially_throwing_int(const int v) noexcept
        : value{v}
    {
    }
    potentially_throwing_int(const potentially_throwing_int& other) noexcept(false)
        : value{other.value}
    {
    }
    potentially_throwing_int(potentially_throwing_int&& other) noexcept(false)
        : value{other.value}
    {
    }
    potentially_throwing_int& operator=(const potentially_throwing_int& other) noexcept(false)
    {
        value = other.value;
        return *this;
    }
    potentially_throwing_int& operator=(potentially_throwing_int&& other) noexcept(false)
    {
        value = other.value;
        return *this;
    }
    ~potentially_throwing_int() = default;

    int value;
};

/// Constructible from int only explicitly.
struct explicit_from_int
{
    explicit explicit_from_int(const int v) noexcept
        : value{v}
    {
    }
    int value;
};

/// Implicitly constructible from int.
struct implicit_from_int
{
    implicit_from_int(const int v) noexcept  // NOLINT(*-explicit-constructor)
        : value{v}
    {
    }
    int value;
};

/// The value category with which a callable was invoked.
enum class category : int
{
    lvalue       = 1,
    const_lvalue = 2,
    rvalue       = 3,
    const_rvalue = 4,
};

/// Returns R built from the value category of its argument.
template <typename R>
struct category_recorder
{
    R operator()(int&) const
    {
        return R(static_cast<int>(category::lvalue));
    }
    R operator()(const int&) const
    {
        return R(static_cast<int>(category::const_lvalue));
    }
    R operator()(int&&) const
    {
        return R(static_cast<int>(category::rvalue));
    }
    R operator()(const int&&) const
    {
        return R(static_cast<int>(category::const_rvalue));
    }
};

#if defined(__cpp_exceptions)
struct test_exception final : std::exception
{
    const char* what() const noexcept override
    {
        return "test_exception";
    }
};

/// Copying throws while armed. Moving never throws.
struct throws_on_copy
{
    static bool& armed()
    {
        static bool instance = false;
        return instance;
    }

    explicit throws_on_copy(const int v) noexcept
        : value{v}
    {
    }
    throws_on_copy(const throws_on_copy& other)
        : value{other.value}
    {
        if (armed())
        {
            throw test_exception{};
        }
    }
    throws_on_copy(throws_on_copy&&) noexcept = default;
    throws_on_copy& operator=(const throws_on_copy& other)
    {
        if (armed())
        {
            throw test_exception{};
        }
        value = other.value;
        return *this;
    }
    throws_on_copy& operator=(throws_on_copy&&) noexcept = default;
    ~throws_on_copy()                                    = default;

    int value;
};

/// Copying and moving throw while armed.
struct throws_on_copy_and_move
{
    static bool& armed()
    {
        static bool instance = false;
        return instance;
    }

    explicit throws_on_copy_and_move(const int v) noexcept
        : value{v}
    {
    }
    throws_on_copy_and_move(const throws_on_copy_and_move& other)
        : value{other.value}
    {
        if (armed())
        {
            throw test_exception{};
        }
    }
    throws_on_copy_and_move(throws_on_copy_and_move&& other)
        : value{other.value}
    {
        if (armed())
        {
            throw test_exception{};
        }
    }
    throws_on_copy_and_move& operator=(const throws_on_copy_and_move& other)
    {
        if (armed())
        {
            throw test_exception{};
        }
        value = other.value;
        return *this;
    }
    throws_on_copy_and_move& operator=(throws_on_copy_and_move&& other)
    {
        if (armed())
        {
            throw test_exception{};
        }
        value = other.value;
        return *this;
    }
    ~throws_on_copy_and_move() = default;

    int value;
};

/// Arms a throwing type for the lifetime of the guard.
template <typename T>
class arm_guard final
{
public:
    arm_guard() noexcept
    {
        T::armed() = true;
    }
    arm_guard(const arm_guard&)            = delete;
    arm_guard(arm_guard&&)                 = delete;
    arm_guard& operator=(const arm_guard&) = delete;
    arm_guard& operator=(arm_guard&&)      = delete;
    ~arm_guard() noexcept
    {
        T::armed() = false;
    }
};
#endif

}  // namespace pf23_expected
}  // namespace cetlvast

#endif  // CETLVAST_TEST_PF23_EXPECTED_HPP_INCLUDED
