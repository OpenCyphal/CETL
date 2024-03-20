/// @file
/// Defines the C++17 `std::any` type and several related entities.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_ANY_HPP_INCLUDED
#define CETL_ANY_HPP_INCLUDED

#include "pf17/cetlpf.hpp"
#include "pf17/utility.hpp"
#include "pf17/attribute.hpp"

#include <cassert>
#include <algorithm>
#include <initializer_list>

namespace cetl
{

// Forward declarations
template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment>
class any;

namespace detail
{

// Move policy.
template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment>
struct base_move;

// Copy policy.
template <std::size_t Footprint, bool Copyable, std::size_t Alignment>
struct base_copy;

template <std::size_t Footprint, std::size_t Alignment>
struct base_storage
{
    // We need to align the buffer to the given value (maximum alignment by default).
    // Also, we need to ensure that the buffer is at least 1 byte long.
    alignas(Alignment) char buffer_[std::max(Footprint, 1UL)];
};

// Movable case.
template <std::size_t Footprint, bool Copyable, std::size_t Alignment>
struct base_move<Footprint, Copyable, true, Alignment> : base_copy<Footprint, Copyable, Alignment>
{
public:
    constexpr base_move()                      = default;
    base_move(base_move&&) noexcept            = default;
    base_move& operator=(base_move&&) noexcept = default;
};

// Non-movable case.
template <std::size_t Footprint, bool Copyable, std::size_t Alignment>
struct base_move<Footprint, Copyable, false, Alignment> : base_copy<Footprint, Copyable, Alignment>
{
public:
    constexpr base_move()                      = default;
    base_move(base_move&&) noexcept            = delete;
    base_move& operator=(base_move&&) noexcept = delete;
};

// Copyable case.
template <std::size_t Footprint, std::size_t Alignment>
struct base_copy<Footprint, true, Alignment> : base_storage<Footprint, Alignment>
{
public:
    constexpr base_copy()                  = default;
    base_copy(const base_copy&)            = default;
    base_copy& operator=(const base_copy&) = default;
};

// Non-copyable case.
template <std::size_t Footprint, std::size_t Alignment>
struct base_copy<Footprint, false, Alignment> : base_storage<Footprint, Alignment>
{
public:
    constexpr base_copy()                  = default;
    base_copy(const base_copy&)            = delete;
    base_copy& operator=(const base_copy&) = delete;
};

enum class action
{
    Get,
    Copy,
    Move,
    Destroy
};

[[noreturn]] inline void throw_bad_any_cast()
{
#if defined(__cpp_exceptions)
    throw bad_any_cast();
#else
    std::terminate();
#endif
}

}  // namespace detail

template <std::size_t Footprint,
          bool        Copyable  = true,
          bool        Movable   = Copyable,
          std::size_t Alignment = alignof(std::max_align_t)>
class any : private detail::base_move<Footprint, Copyable, Movable, Alignment>
{
public:
    constexpr any() noexcept = default;

    any(const any& other)
    {
        if (other.has_value())
        {
            other.handle(detail::action::Copy, this);
        }
    }

    any(any&& other) noexcept
    {
        if (other.has_value())
        {
            other.handle(detail::action::Move, this);
        }
    }

    template <
        typename ValueType,
        typename Tp = std::decay_t<ValueType>,
        typename = std::enable_if_t<!std::is_same<Tp, any>::value && !pf17::detail::is_in_place_type<ValueType>::value>>
    any(ValueType&& value)
    {
        soo_handler<Tp>::create(*this, std::forward<ValueType>(value));
    }

    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit any(in_place_type_t<ValueType>, Args&&... args)
    {
        soo_handler<Tp>::create(*this, std::forward<Args>(args)...);
    }

    template <typename ValueType, typename Up, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit any(in_place_type_t<ValueType>, std::initializer_list<Up> list, Args&&... args)
    {
        soo_handler<Tp>::create(*this, list, std::forward<Args>(args)...);
    }

    ~any()
    {
        reset();
    }

    any& operator=(const any& rhs)
    {
        any(rhs).swap(*this);
        return *this;
    }

    any& operator=(any&& rhs) noexcept
    {
        any(std::move(rhs)).swap(*this);
        return *this;
    }

    template <typename ValueType,
              typename Tp = std::decay_t<ValueType>,
              typename    = std::enable_if_t<!std::is_same<Tp, any>::value>>
    any& operator=(ValueType&& value)
    {
        any(std::forward<ValueType>(value)).swap(*this);
        return *this;
    }

    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    Tp& emplace(Args&&... args)
    {
        reset();
        return soo_handler<Tp>::create(*this, std::forward<Args>(args)...);
    }

    template <typename ValueType, typename Up, typename... Args, typename Tp = std::decay_t<ValueType>>
    Tp& emplace(std::initializer_list<Up> list, Args&&... args)
    {
        reset();
        return soo_handler<Tp>::create(*this, list, std::forward<Args>(args)...);
    }

    void reset() noexcept
    {
        handle(detail::action::Destroy);
    }

    void swap(any& rhs) noexcept
    {
        if (this == &rhs)
        {
            return;
        }

        if (has_value())
        {
            if (rhs.has_value())
            {
                any tmp;
                rhs.handle(detail::action::Move, &tmp);
                handle(detail::action::Move, &rhs);
                tmp.handle(detail::action::Move, this);
            }
            else
            {
                handle(detail::action::Move, &rhs);
            }
        }
        else if (rhs.has_value())
        {
            rhs.handle(detail::action::Move, this);
        }
    }

    CETL_NODISCARD bool has_value() const noexcept
    {
        return handler_ != nullptr;
    }

private:
    // Type-erased handler.
    using any_handler = void* (*) (detail::action, const any*, any*);

    // Small Object Optimization (SOO) handler.
    template <typename Tp>
    struct soo_handler
    {
        static_assert(sizeof(Tp) <= Footprint, "Enlarge the footprint");

        static void* handle(const detail::action action, const any* const self, any* const other)
        {
            switch (action)
            {
            case detail::action::Get:

                return get(const_cast<any&>(*self));

            case detail::action::Copy:

                copy(*other, *self);
                return nullptr;

            case detail::action::Move:

                move(*other, const_cast<any&>(*self));
                return nullptr;

            case detail::action::Destroy:

                destroy(const_cast<any&>(*self));
                return nullptr;

            default:

                std::terminate();
            }
        }

        template <typename... Args>
        static Tp& create(any& dest, Args&&... args)
        {
            assert(nullptr == dest.handler_);

            Tp* ret = static_cast<Tp*>(static_cast<void*>(&dest.buffer_));
            new (ret) Tp(std::forward<Args>(args)...);
            dest.handler_ = handle;
            return *ret;
        }

    private:
        CETL_NODISCARD static void* get(any& self)
        {
            // TODO: Add RTTI check here.
            return static_cast<void*>(&self.buffer_);
        }

        static void copy(any& self, const any& source)
        {
            const Tp* src = static_cast<const Tp*>(static_cast<const void*>(&source.buffer_));
            create(self, *src);
        }

        static void move(any& self, any& source)
        {
            Tp* src = static_cast<Tp*>(static_cast<void*>(&source.buffer_));
            create(self, std::move(*src));
            destroy(source);
        }

        static void destroy(any& self)
        {
            assert(nullptr != self.handler_);

            Tp* ptr = static_cast<Tp*>(static_cast<void*>(&self.buffer_));
            ptr->~Tp();
            self.handler_ = nullptr;
        }

    };  // struct soo_handler

    template <typename ValueType, typename Any>
    friend std::add_pointer_t<ValueType> any_cast(Any* operand) noexcept;

    void* handle(const detail::action action, any* const other = nullptr) const
    {
        return handler_ ? handler_(action, this, other) : nullptr;
    }

    any_handler handler_ = nullptr;

};  // class any

/// \brief Constructs an any object containing an object of type T, passing the provided arguments to T's constructor.
///
/// Equivalent to `cetl::any(cetl::in_place_type<ValueType>, std::forward<Args>(args)...)`.
///
template <typename ValueType, typename Any, typename... Args>
CETL_NODISCARD Any make_any(Args&&... args)
{
    return Any(in_place_type<ValueType>, std::forward<Args>(args)...);
}

/// \brief Constructs an any object containing an object of type T, passing the provided arguments to T's constructor.
///
/// Equivalent to `cetl::any(cetl::in_place_type<ValueType>, list, std::forward<Args>(args)...)`.
///
template <typename ValueType, typename Any, typename Up, typename... Args>
CETL_NODISCARD Any make_any(std::initializer_list<Up> list, Args&&... args)
{
    return Any(in_place_type<ValueType>, list, std::forward<Args>(args)...);
}

/// \brief Performs type-safe access to the contained object.
///
/// \param operand Target any object.
/// \return Returns `std::static_cast<ValueType>(*cetl::any_cast<const U>(&operand))`,
///     where let `U` be `std::remove_cv_t<std::remove_reference_t<ValueType>>`.
///
template <typename ValueType, typename Any>
ValueType any_cast(const Any& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, const RawValueType&>::value,
                  "ValueType is required to be a const lvalue reference "
                  "or a CopyConstructible type");

    auto ptr = any_cast<std::add_const_t<RawValueType>>(&operand);
    if (ptr == nullptr)
    {
        detail::throw_bad_any_cast();
    }
    return static_cast<ValueType>(*ptr);
}

/// \brief Performs type-safe access to the contained object.
///
/// \param operand Target any object.
/// \return Returns `std::static_cast<ValueType>(*cetl::any_cast<U>(&operand))`,
///     where let `U` be `std::remove_cv_t<std::remove_reference_t<ValueType>>`.
///
template <typename ValueType, typename Any>
ValueType any_cast(Any& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, RawValueType&>::value,
                  "ValueType is required to be an lvalue reference "
                  "or a CopyConstructible type");

    auto ptr = any_cast<RawValueType>(&operand);
    if (ptr == nullptr)
    {
        detail::throw_bad_any_cast();
    }
    return static_cast<ValueType>(*ptr);
}

/// \brief Performs type-safe access to the contained object.
///
/// \param operand Target any object.
/// \return Returns `std::static_cast<ValueType>(std::move(*cetl::any_cast<U>(&operand)))`,
///     where let `U` be `std::remove_cv_t<std::remove_reference_t<ValueType>>`.
///
template <typename ValueType, typename Any>
ValueType any_cast(Any&& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, RawValueType>::value,
                  "ValueType is required to be an rvalue reference "
                  "or a CopyConstructible type");

    auto ptr = any_cast<RawValueType>(&operand);
    if (ptr == nullptr)
    {
        detail::throw_bad_any_cast();
    }
    return static_cast<ValueType>(std::move(*ptr));
}

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
CETL_NODISCARD std::add_pointer_t<std::add_const_t<ValueType>> any_cast(const Any* const operand) noexcept
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
CETL_NODISCARD std::add_pointer_t<ValueType> any_cast(Any* const operand) noexcept
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
