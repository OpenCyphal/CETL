/// @file
/// Defines the C++17 `std::any` type and several related entities.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_ANY_HPP_INCLUDED
#define CETL_ANY_HPP_INCLUDED

#include "pf17/any.hpp"

#include <algorithm>
#include <type_traits>

namespace cetl
{

// Forward declarations
template <std::size_t Footprint, bool Copyable, bool Movable>
class any;

namespace detail
{

// Move policy.
template <std::size_t Footprint, bool Copyable, bool Movable>
struct base_move;

// Copy policy.
template <std::size_t Footprint, bool Copyable>
struct base_copy;

template <std::size_t Footprint, std::size_t Align = sizeof(std::max_align_t)>
struct base_storage
{
    // We need to align the buffer to the given value (maximum alignment by default).
    // Also, we need to ensure that the buffer is at least 1 byte long.
    alignas(Align) char buffer_[std::max(Footprint, 1UL)];
};

// Movable case.
template <std::size_t Footprint, bool Copyable>
struct base_move<Footprint, Copyable, true> : base_copy<Footprint, Copyable>
{
public:
    constexpr base_move()                      = default;
    base_move(base_move&&) noexcept            = default;
    base_move& operator=(base_move&&) noexcept = default;
};

// Non-movable case.
template <std::size_t Footprint, bool Copyable>
struct base_move<Footprint, Copyable, false> : base_copy<Footprint, Copyable>
{
public:
    constexpr base_move()                      = default;
    base_move(base_move&&) noexcept            = delete;
    base_move& operator=(base_move&&) noexcept = delete;
};

// Copyable case.
template <std::size_t Footprint>
struct base_copy<Footprint, true> : base_storage<Footprint>
{
public:
    constexpr base_copy()                  = default;
    base_copy(const base_copy&)            = default;
    base_copy& operator=(const base_copy&) = default;
};

// Non-copyable case.
template <std::size_t Footprint>
struct base_copy<Footprint, false> : base_storage<Footprint>
{
public:
    constexpr base_copy()                  = default;
    base_copy(const base_copy&)            = delete;
    base_copy& operator=(const base_copy&) = delete;
};

enum class action
{
    Get,
    Destroy
};

}  // namespace detail

template <std::size_t Footprint, bool Copyable = true, bool Movable = Copyable>
class any : private detail::base_move<Footprint, Copyable, Movable>
{
public:
    constexpr any() noexcept = default;

    template <typename ValueType, typename Tp = std::decay_t<ValueType>>
    any(ValueType&& value)
    {
        soo_handler<Tp>::create(*this, std::forward<ValueType>(value));
    }

    ~any()
    {
        reset();
    }

    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    Tp& emplace(Args&&... args)
    {
        reset();
        return soo_handler<Tp>::create(*this, std::forward<Args>(args)...);
    }

    void reset() noexcept
    {
        handle(detail::action::Destroy);
    }

    CETL_NODISCARD bool has_value() const noexcept
    {
        return handler_ != nullptr;
    }

private:
    // Type-erased handler.
    using any_handler = void* (*) (detail::action, const any* /*self*/);

    // Small Object Optimization (SOO) handler.
    template <typename Tp>
    struct soo_handler
    {
        static_assert(sizeof(Tp) <= Footprint, "Enlarge the footprint");

        static void* handle(detail::action action, const any* self)
        {
            switch (action)
            {
            case detail::action::Get:
                return get(const_cast<any&>(*self));
            case detail::action::Destroy:
                destroy(const_cast<any&>(*self));
                return nullptr;
            }
        }

        template <typename... Args>
        static Tp& create(any& dest, Args&&... args)
        {
            Tp* ret = static_cast<Tp*>(static_cast<void*>(&dest.buffer_));
            new (ret) Tp(std::forward<Args>(args)...);
            dest.handler_ = handle;
            return *ret;
        }

    private:
        static void* get(any& self)
        {
            // TODO: Add RTTI check here.
            return static_cast<void*>(&self.buffer_);
        }

        static void destroy(any& self)
        {
            Tp* ptr = static_cast<Tp*>(static_cast<void*>(&self.buffer_));
            ptr->~Tp();
            self.handler_ = nullptr;
        }

    };  // struct soo_handler

    template <typename ValueType, typename Any>
    friend std::add_pointer_t<ValueType> any_cast(Any* operand) noexcept;

    void* handle(detail::action action) const
    {
        return handler_ ? handler_(action, this) : nullptr;
    }

    any_handler handler_ = nullptr;

};  // class any

/// \brief Performs type-safe access to the `const` contained object.
///
/// \tparam ValueType Type of the requested value; may not be a reference.
/// \tparam Any Type of the `any` object.
/// \param operand Target constant any object.
/// \return If operand is not a null pointer,
///     and the typeid of the requested `ValueType` matches that of the contents of operand,
///     a pointer to the value contained by operand, otherwise a null pointer.
///
template <typename ValueType, typename Any>
std::add_pointer_t<std::add_const_t<ValueType>> any_cast(const Any* operand) noexcept
{
    return any_cast<ValueType>(const_cast<Any*>(operand));
}

/// \brief Performs type-safe access to the contained object.
///
/// \tparam ValueType Type of the requested value; may not be a reference.
/// \tparam Any Type of the `any` object.
/// \param operand Target `any` object.
/// \return If operand is not a null pointer,
///     and the typeid of the requested `ValueType` matches that of the contents of operand,
///     a pointer to the value contained by operand, otherwise a null pointer.
///
template <typename ValueType, typename Any>
std::add_pointer_t<ValueType> any_cast(Any* operand) noexcept
{
    static_assert(!std::is_reference<ValueType>::value, "`ValueType` may not be a reference.");

    if (!operand)
    {
        return nullptr;
    }

    void* ptr = operand->handle(detail::action::Get);

    using ReturnType = std::add_pointer_t<ValueType>;
    return static_cast<ReturnType>(ptr);
}

}  // namespace cetl

#endif  // CETL_ANY_HPP_INCLUDED
