/// @file
/// CETL VerificAtion SuiTe – Test suite helpers.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#ifndef CETLVAST_SMF_POLICIES_HPP_INCLUDED
#define CETLVAST_SMF_POLICIES_HPP_INCLUDED

#include <cetl/cetl.hpp>
#include <cstdint>

namespace cetlvast
{
/// All special method functions are noexcept. If you want them to be non-noexcept, make a wrapper type.
namespace smf_policies
{
/// This has to be an old-style enum because C++14 requires it to be convertible to int.
enum special_function_policy
{
    policy_deleted    = 0,
    policy_trivial    = 1,
    policy_nontrivial = 2,
};

/// COPY CONSTRUCTION POLICY
template <std::uint8_t>
struct copy_ctor_policy;
template <>
struct copy_ctor_policy<policy_nontrivial>
{
    static constexpr auto copy_ctor_policy_value = policy_nontrivial;
    constexpr copy_ctor_policy() noexcept        = default;
    constexpr copy_ctor_policy(const copy_ctor_policy& other) noexcept
        : copy_constructed{other.copy_constructed + 1U}
    {
    }
    constexpr copy_ctor_policy(copy_ctor_policy&&) noexcept                 = default;
    constexpr copy_ctor_policy& operator=(const copy_ctor_policy&) noexcept = default;
    constexpr copy_ctor_policy& operator=(copy_ctor_policy&&) noexcept      = default;
    ~copy_ctor_policy() noexcept                                            = default;
    CETL_NODISCARD constexpr auto get_copy_ctor_count() const noexcept
    {
        return copy_constructed;
    }
    std::uint32_t copy_constructed = 0;
};
template <>
struct copy_ctor_policy<policy_trivial>
{
    static constexpr auto         copy_ctor_policy_value = policy_trivial;
    CETL_NODISCARD constexpr auto get_copy_ctor_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};
template <>
struct copy_ctor_policy<policy_deleted>
{
    static constexpr auto copy_ctor_policy_value                            = policy_deleted;
    constexpr copy_ctor_policy() noexcept                                   = default;
    constexpr copy_ctor_policy(const copy_ctor_policy&)                     = delete;
    constexpr copy_ctor_policy(copy_ctor_policy&&) noexcept                 = default;
    constexpr copy_ctor_policy& operator=(const copy_ctor_policy&) noexcept = default;
    constexpr copy_ctor_policy& operator=(copy_ctor_policy&&) noexcept      = default;
    ~copy_ctor_policy() noexcept                                            = default;
    CETL_NODISCARD constexpr auto get_copy_ctor_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};

/// MOVE CONSTRUCTION POLICY
template <std::uint8_t>
struct move_ctor_policy;
template <>
struct move_ctor_policy<policy_nontrivial>
{
    static constexpr auto move_ctor_policy_value                 = policy_nontrivial;
    constexpr move_ctor_policy() noexcept                        = default;
    constexpr move_ctor_policy(const move_ctor_policy&) noexcept = default;
    constexpr move_ctor_policy(move_ctor_policy&& other) noexcept
        : move_constructed{other.move_constructed + 1U}
    {
    }
    constexpr move_ctor_policy& operator=(const move_ctor_policy&) noexcept = default;
    constexpr move_ctor_policy& operator=(move_ctor_policy&&) noexcept      = default;
    ~move_ctor_policy() noexcept                                            = default;
    CETL_NODISCARD constexpr auto get_move_ctor_count() const noexcept
    {
        return move_constructed;
    }
    std::uint32_t move_constructed = 0;
};
template <>
struct move_ctor_policy<policy_trivial>
{
    static constexpr auto         move_ctor_policy_value = policy_trivial;
    CETL_NODISCARD constexpr auto get_move_ctor_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};
template <>
struct move_ctor_policy<policy_deleted>
{
    static constexpr auto move_ctor_policy_value                            = policy_deleted;
    constexpr move_ctor_policy() noexcept                                   = default;
    constexpr move_ctor_policy(const move_ctor_policy&) noexcept            = default;
    constexpr move_ctor_policy(move_ctor_policy&&)                          = delete;
    constexpr move_ctor_policy& operator=(const move_ctor_policy&) noexcept = default;
    constexpr move_ctor_policy& operator=(move_ctor_policy&&) noexcept      = default;
    ~move_ctor_policy() noexcept                                            = default;
    CETL_NODISCARD constexpr auto get_move_ctor_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};

/// COPY ASSIGNMENT POLICY
template <std::uint8_t>
struct copy_assignment_policy;
template <>
struct copy_assignment_policy<policy_nontrivial>
{
    static constexpr auto copy_assignment_policy_value                       = policy_nontrivial;
    constexpr copy_assignment_policy() noexcept                              = default;
    constexpr copy_assignment_policy(const copy_assignment_policy&) noexcept = default;
    constexpr copy_assignment_policy(copy_assignment_policy&&) noexcept      = default;
    constexpr copy_assignment_policy& operator=(const copy_assignment_policy& other) noexcept
    {
        copy_assigned = other.copy_assigned + 1U;
        return *this;
    }
    constexpr copy_assignment_policy& operator=(copy_assignment_policy&&) noexcept = default;
    ~copy_assignment_policy() noexcept                                             = default;
    CETL_NODISCARD constexpr auto get_copy_assignment_count() const
    {
        return copy_assigned;
    }
    std::uint32_t copy_assigned = 0;
};
template <>
struct copy_assignment_policy<policy_trivial>
{
    static constexpr auto         copy_assignment_policy_value = policy_trivial;
    CETL_NODISCARD constexpr auto get_copy_assignment_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};
template <>
struct copy_assignment_policy<policy_deleted>
{
    static constexpr auto copy_assignment_policy_value                             = policy_deleted;
    constexpr copy_assignment_policy() noexcept                                    = default;
    constexpr copy_assignment_policy(const copy_assignment_policy&) noexcept       = default;
    constexpr copy_assignment_policy(copy_assignment_policy&&) noexcept            = default;
    constexpr copy_assignment_policy& operator=(const copy_assignment_policy&)     = delete;
    constexpr copy_assignment_policy& operator=(copy_assignment_policy&&) noexcept = default;
    ~copy_assignment_policy() noexcept                                             = default;
    CETL_NODISCARD constexpr auto get_copy_assignment_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};

/// MOVE ASSIGNMENT POLICY
template <std::uint8_t>
struct move_assignment_policy;
template <>
struct move_assignment_policy<policy_nontrivial>
{
    static constexpr auto move_assignment_policy_value                                  = policy_nontrivial;
    constexpr move_assignment_policy() noexcept                                         = default;
    constexpr move_assignment_policy(const move_assignment_policy&) noexcept            = default;
    constexpr move_assignment_policy(move_assignment_policy&&) noexcept                 = default;
    constexpr move_assignment_policy& operator=(const move_assignment_policy&) noexcept = default;
    constexpr move_assignment_policy& operator=(move_assignment_policy&& other) noexcept
    {
        move_assigned = other.move_assigned + 1U;
        return *this;
    }
    ~move_assignment_policy() = default;
    CETL_NODISCARD constexpr auto get_move_assignment_count() const noexcept
    {
        return move_assigned;
    }
    std::uint32_t move_assigned = 0;
};
template <>
struct move_assignment_policy<policy_trivial>
{
    static constexpr auto         move_assignment_policy_value = policy_trivial;
    CETL_NODISCARD constexpr auto get_move_assignment_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};
template <>
struct move_assignment_policy<policy_deleted>
{
    static constexpr auto move_assignment_policy_value                                  = policy_deleted;
    constexpr move_assignment_policy() noexcept                                         = default;
    constexpr move_assignment_policy(const move_assignment_policy&) noexcept            = default;
    constexpr move_assignment_policy(move_assignment_policy&&) noexcept                 = default;
    constexpr move_assignment_policy& operator=(const move_assignment_policy&) noexcept = default;
    constexpr move_assignment_policy& operator=(move_assignment_policy&&)               = delete;
    ~move_assignment_policy() noexcept                                                  = default;
    CETL_NODISCARD constexpr auto get_move_assignment_count() const noexcept
    {
        (void) this;
        return 0U;
    }
};

/// DESTRUCTION POLICY
/// Before the object is destroyed, the destruction counter should be configured by calling
/// configure_destruction_counter. The reason we don't keep the destruction counter as a member variable is that
/// we can't safely access it after the object is destroyed.
/// The trivial destruction policy does not maintain the destruction counter and the method does nothing.
template <std::uint8_t>
struct dtor_policy;
template <>
struct dtor_policy<policy_nontrivial>
{
    static constexpr auto dtor_policy_value             = policy_nontrivial;
    constexpr dtor_policy() noexcept                    = default;
    constexpr dtor_policy(const dtor_policy&) noexcept  = default;
    constexpr dtor_policy(dtor_policy&&) noexcept       = default;
    dtor_policy& operator=(const dtor_policy&) noexcept = default;
    dtor_policy& operator=(dtor_policy&&) noexcept      = default;
    ~dtor_policy() noexcept
    {
        if (nullptr != destructed)
        {
            ++*destructed;
        }
    }
    constexpr void configure_destruction_counter(std::uint32_t* const counter) const noexcept
    {
        destructed = counter;
    }
    mutable std::uint32_t* destructed = nullptr;
};
template <>
struct dtor_policy<policy_trivial>
{
    static constexpr auto dtor_policy_value = policy_trivial;
    constexpr void        configure_destruction_counter(std::uint32_t* const) const noexcept {}
};
template <>
struct dtor_policy<policy_deleted>
{
    static constexpr auto dtor_policy_value             = policy_deleted;
    constexpr dtor_policy() noexcept                    = default;
    constexpr dtor_policy(const dtor_policy&) noexcept  = default;
    constexpr dtor_policy(dtor_policy&&) noexcept       = default;
    dtor_policy& operator=(const dtor_policy&) noexcept = default;
    dtor_policy& operator=(dtor_policy&&) noexcept      = default;
    ~dtor_policy()                                      = delete;
    constexpr void configure_destruction_counter(std::uint32_t* const) const noexcept {}
};

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

}  // namespace smf_policies
}  // namespace cetlvast

#endif  // CETLVAST_SMF_POLICIES_HPP_INCLUDED
