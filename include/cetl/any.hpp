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

template <std::size_t Footprint, std::size_t Alignment>
struct base_storage  // NOLINT(*-pro-type-member-init)
{
    base_storage() = default;

    CETL_NODISCARD void* get_raw_storage() noexcept
    {
        return static_cast<void*>(buffer_);
    }

    CETL_NODISCARD const void* get_raw_storage() const noexcept
    {
        return static_cast<const void*>(buffer_);
    }

    CETL_NODISCARD bool has_value() const noexcept
    {
        return nullptr != value_destroyer_;
    }

    template <typename Tp>
    void make_handlers() noexcept
    {
        assert(nullptr == value_destroyer_);

        value_destroyer_ = [](void* const storage) {
            const auto ptr = static_cast<Tp*>(storage);
            ptr->~Tp();
        };
    }

    template <typename ValueType>
    CETL_NODISCARD void* get_ptr() noexcept
    {
        if (!has_value())
        {
            return nullptr;
        }

        // TODO: Add RTTI check here.
        return get_raw_storage();
    }

    template <typename ValueType>
    CETL_NODISCARD const void* get_ptr() const noexcept
    {
        if (!has_value())
        {
            return nullptr;
        }

        // TODO: Add RTTI check here.
        return get_raw_storage();
    }

    void copy_handlers_from(const base_storage& src) noexcept
    {
        value_destroyer_ = src.value_destroyer_;
    }

    void reset() noexcept
    {
        if (value_destroyer_)
        {
            value_destroyer_(get_raw_storage());
            value_destroyer_ = nullptr;
        }
    }

private:
    // We need to align the buffer to the given value (maximum alignment by default).
    // Also, we need to ensure that the buffer is at least 1 byte long.
    alignas(Alignment) char buffer_[std::max(Footprint, 1UL)];

    // Holds type-erased value destroyer. `nullptr` if storage has no value stored.
    void (*value_destroyer_)(void* self) = nullptr;

};  // base_storage

template <std::size_t Footprint, std::size_t Alignment>
struct base_handlers : base_storage<Footprint, Alignment>
{
    // Holds type-erased value copyer. `nullptr` when copy operation is not supported.
    void (*value_copier_)(const void* src, void* dst) = nullptr;

    // Holds type-erased value mover. `nullptr` when move operation is not supported.
    void (*value_mover_)(void* src, void* dst) = nullptr;

    void copy_handlers_from(const base_handlers& src) noexcept
    {
        base::copy_handlers_from(src);

        value_copier_ = src.value_copier_;
        value_mover_  = src.value_mover_;
    }

    void reset() noexcept
    {
        value_copier_ = nullptr;
        value_mover_  = nullptr;

        base::reset();
    }

private:
    using base = base_storage<Footprint, Alignment>;

};  // base_handlers

// Copy policy.
//
template <std::size_t Footprint, bool Copyable, std::size_t Alignment>
struct base_copy;
//
// Non-copyable case.
template <std::size_t Footprint, std::size_t Alignment>
struct base_copy<Footprint, false, Alignment> : base_handlers<Footprint, Alignment>
{
    constexpr base_copy()                  = default;
    base_copy(const base_copy&)            = delete;
    base_copy& operator=(const base_copy&) = delete;
};
//
// Copyable case.
template <std::size_t Footprint, std::size_t Alignment>
struct base_copy<Footprint, true, Alignment> : base_handlers<Footprint, Alignment>
{
    constexpr base_copy() = default;
    base_copy(const base_copy& other)
    {
        copy_from(other);
    }

    base_copy& operator=(const base_copy& other)
    {
        base::reset();
        copy_from(other);
        return *this;
    }

    template <typename Tp>
    void make_handlers() noexcept
    {
        assert(nullptr == base::value_copier_);

        base::template make_handlers<Tp>();

        base::value_copier_ = [](const void* const src, void* const dst) {
            assert(nullptr != src);
            assert(nullptr != dst);

            new (dst) Tp(*static_cast<const Tp*>(src));
        };
    }

private:
    using base = base_handlers<Footprint, Alignment>;

    void copy_from(const base_copy& src)
    {
        assert(!base::has_value());

        if (src.has_value())
        {
            base::copy_handlers_from(src);
            assert(nullptr != base::value_copier_);

            base::value_copier_(src.get_raw_storage(), base::get_raw_storage());
        }
    }

};  // base_copy

// Move policy.
//
template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment>
struct base_move;
//
// Non-movable case.
template <std::size_t Footprint, bool Copyable, std::size_t Alignment>
struct base_move<Footprint, Copyable, false, Alignment> : base_copy<Footprint, Copyable, Alignment>
{
    constexpr base_move()           = default;
    base_move(const base_move&)     = default;
    base_move(base_move&&) noexcept = delete;

    base_move& operator=(const base_move&)     = default;
    base_move& operator=(base_move&&) noexcept = delete;
};
//
// Movable case.
template <std::size_t Footprint, bool Copyable, std::size_t Alignment>
struct base_move<Footprint, Copyable, true, Alignment> : base_copy<Footprint, Copyable, Alignment>
{
    constexpr base_move()                  = default;
    base_move(const base_move&)            = default;
    base_move& operator=(const base_move&) = default;

    base_move(base_move&& other) noexcept
    {
        move_from(other);
    }

    base_move& operator=(base_move&& other) noexcept
    {
        move_from(other);
        return *this;
    }

    template <typename Tp>
    void make_handlers() noexcept
    {
        assert(nullptr == base::value_mover_);

        base::template make_handlers<Tp>();

        base::value_mover_ = [](void* const src, void* const dst) {
            assert(nullptr != src);
            assert(nullptr != dst);

            new (dst) Tp(std::move(*static_cast<Tp*>(src)));
        };
    }

private:
    using base = base_copy<Footprint, Copyable, Alignment>;

    void move_from(base_move& src) noexcept
    {
        assert(!base::has_value());

        if (src.has_value())
        {
            base::copy_handlers_from(src);
            assert(nullptr != base::value_mover_);

            base::value_mover_(src.get_raw_storage(), base::get_raw_storage());

            src.reset();
        }
    }

};  // base_move

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
class any : detail::base_move<Footprint, Copyable, Movable, Alignment>
{
    using base = detail::base_move<Footprint, Copyable, Movable, Alignment>;

public:
    constexpr any()           = default;
    any(const any& other)     = default;
    any(any&& other) noexcept = default;

    template <
        typename ValueType,
        typename Tp = std::decay_t<ValueType>,
        typename = std::enable_if_t<!std::is_same<Tp, any>::value && !pf17::detail::is_in_place_type<ValueType>::value>>
    any(ValueType&& value)  // NOLINT(*-explicit-constructor)
    {
        create<Tp>(std::forward<ValueType>(value));
    }

    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit any(in_place_type_t<ValueType>, Args&&... args)
    {
        create<Tp>(std::forward<Args>(args)...);
    }

    template <typename ValueType, typename Up, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit any(in_place_type_t<ValueType>, std::initializer_list<Up> list, Args&&... args)
    {
        create<Tp>(list, std::forward<Args>(args)...);
    }

    ~any()
    {
        reset();
    }

    any& operator=(const any& rhs)
    {
        if (this != &rhs)
        {
            any(rhs).swap(*this);
        }
        return *this;
    }

    any& operator=(any&& rhs) noexcept
    {
        if (this != &rhs)
        {
            any(std::move(rhs)).swap(*this);
        }
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

        return create<Tp>(std::forward<Args>(args)...);
    }

    template <typename ValueType, typename Up, typename... Args, typename Tp = std::decay_t<ValueType>>
    Tp& emplace(std::initializer_list<Up> list, Args&&... args)
    {
        reset();

        return create<Tp>(list, std::forward<Args>(args)...);
    }

    /// \brief If not empty, destroys the contained object.
    ///
    void reset() noexcept
    {
        base::reset();
    }

    template <bool CopyableAlias = Copyable,
              bool MovableAlias  = Movable,
              typename           = std::enable_if_t<CopyableAlias && !MovableAlias>>
    void swap(any& rhs)
    {
        if (this == &rhs)
        {
            return;
        }

        if (has_value())
        {
            if (rhs.has_value())
            {
                any tmp{rhs};
                static_cast<base&>(rhs)   = *this;
                static_cast<base&>(*this) = tmp;
            }
            else
            {
                static_cast<base&>(rhs) = *this;
                reset();
            }
        }
        else if (rhs.has_value())
        {
            static_cast<base&>(*this) = rhs;
            rhs.reset();
        }
    }

    template <bool MovableAlias = Movable, typename = std::enable_if_t<MovableAlias>>
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
                any tmp{std::move(rhs)};
                static_cast<base&>(rhs)   = std::move(*this);
                static_cast<base&>(*this) = std::move(tmp);
            }
            else
            {
                static_cast<base&>(rhs) = std::move(*this);
            }
        }
        else if (rhs.has_value())
        {
            static_cast<base&>(*this) = std::move(rhs);
        }
    }

    CETL_NODISCARD bool has_value() const noexcept
    {
        return base::has_value();
    }

private:
    template <typename ValueType, typename Any>
    friend std::add_pointer_t<ValueType> any_cast(Any* operand) noexcept;

    template <typename ValueType, typename Any>
    friend std::add_pointer_t<std::add_const_t<ValueType>> any_cast(const Any* operand) noexcept;

    template <typename Tp, typename... Args>
    Tp& create(Args&&... args)
    {
        base::template make_handlers<Tp>();
        return *new (base::get_raw_storage()) Tp(std::forward<Args>(args)...);
    }

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
CETL_NODISCARD ValueType any_cast(const Any& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, const RawValueType&>::value,
                  "ValueType is required to be a const lvalue reference "
                  "or a CopyConstructible type");

    const auto ptr = any_cast<std::add_const_t<RawValueType>>(&operand);
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
CETL_NODISCARD ValueType any_cast(Any& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, RawValueType&>::value,
                  "ValueType is required to be an lvalue reference "
                  "or a CopyConstructible type");

    const auto ptr = any_cast<RawValueType>(&operand);
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
CETL_NODISCARD ValueType any_cast(Any&& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, RawValueType>::value,
                  "ValueType is required to be an rvalue reference "
                  "or a CopyConstructible type");

    const auto ptr = any_cast<RawValueType>(&operand);
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
    static_assert(!std::is_reference<ValueType>::value, "`ValueType` may not be a reference.");

    if (!operand)
    {
        return nullptr;
    }

    const auto ptr = operand->template get_ptr<ValueType>();

    using ReturnType = std::add_pointer_t<std::add_const_t<ValueType>>;
    return static_cast<ReturnType>(ptr);
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

    const auto ptr = operand->template get_ptr<ValueType>();

    using ReturnType = std::add_pointer_t<ValueType>;
    return static_cast<ReturnType>(ptr);
}

}  // namespace cetl

#endif  // CETL_ANY_HPP_INCLUDED
