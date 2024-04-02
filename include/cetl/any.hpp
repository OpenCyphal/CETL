/// @file
/// Defines the C++17 `std::any` type and several related entities.
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_ANY_HPP_INCLUDED
#define CETL_ANY_HPP_INCLUDED

#include "rtti.hpp"
#include "pf17/cetlpf.hpp"
#include "pf17/utility.hpp"
#include "pf17/attribute.hpp"

#include <algorithm>
#include <initializer_list>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

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
        static_assert(sizeof(Tp) <= Footprint, "Enlarge the footprint");

        CETL_DEBUG_ASSERT(nullptr == value_destroyer_, "Expected to be empty before making handlers.");
        CETL_DEBUG_ASSERT(nullptr == value_converter_, "");
        CETL_DEBUG_ASSERT(nullptr == value_const_converter_, "");

        value_destroyer_ = [](void* const storage) {
            const auto ptr = static_cast<Tp*>(storage);
            ptr->~Tp();
        };

        make_converters<Tp>();
    }

    template <typename Tp, std::enable_if_t<is_rtti_convertible<Tp>, int> = 0>
    void make_converters() noexcept
    {
        value_const_converter_ = [](const void* const storage, const type_id& id) {
            const auto ptr = static_cast<const Tp*>(storage);
            return ptr->_cast_(id);
        };
        value_converter_ = [](void* const storage, const type_id& id) {
            auto ptr = static_cast<Tp*>(storage);
            return ptr->_cast_(id);
        };
    }

    template <typename Tp, std::enable_if_t<!is_rtti_convertible<Tp>, int> = 0>
    void make_converters() noexcept
    {
        value_const_converter_ = [](const void* const storage, const type_id& id) {
            return (id == type_id_value<Tp>) ? storage : nullptr;
        };
        value_converter_ = [](void* const storage, const type_id& id) {
            return (id == type_id_value<Tp>) ? storage : nullptr;
        };
    }

    template <typename ValueType>
    CETL_NODISCARD void* get_ptr() noexcept
    {
        static_assert(sizeof(ValueType) <= Footprint,
                      "Cannot contain the requested type since the footprint is too small");

        if (!has_value())
        {
            return nullptr;
        }
        CETL_DEBUG_ASSERT(nullptr != value_const_converter_, "Non-empty storage is expected to have value converter.");

        return value_converter_(get_raw_storage(), type_id_value<ValueType>);
    }

    template <typename ValueType>
    CETL_NODISCARD const void* get_ptr() const noexcept
    {
        static_assert(sizeof(ValueType) <= Footprint,
                      "Cannot contain the requested type since the footprint is too small");

        if (!has_value())
        {
            return nullptr;
        }
        CETL_DEBUG_ASSERT(nullptr != value_const_converter_, "Non-empty storage is expected to have value converter.");

        return value_const_converter_(get_raw_storage(), type_id_value<ValueType>);
    }

    void copy_handlers_from(const base_storage& src) noexcept
    {
        value_destroyer_       = src.value_destroyer_;
        value_converter_       = src.value_converter_;
        value_const_converter_ = src.value_const_converter_;
    }

    void reset() noexcept
    {
        if (value_destroyer_)
        {
            value_destroyer_(get_raw_storage());
            value_destroyer_ = nullptr;
        }

        value_converter_       = nullptr;
        value_const_converter_ = nullptr;
    }

private:
    // We need to align the buffer to the given value (maximum alignment by default).
    // Also, we need to ensure that the buffer is at least 1 byte long.
    // NB! It's intentional and by design that the `buffer_` is the very first member of `any` memory layout.
    // In such way pointer to a `any` and its stored value are the same - could be useful during
    // debugging/troubleshooting.
    alignas(Alignment) char buffer_[std::max(Footprint, 1UL)];

    // Holds type-erased value destroyer. `nullptr` if storage has no value stored.
    void (*value_destroyer_)(void* self) = nullptr;

    // Holds type-erased value converters (const and non-const). `nullptr` if storage has no value stored.
    //
    void* (*value_converter_)(void* self, const type_id& id)                   = nullptr;
    const void* (*value_const_converter_)(const void* self, const type_id& id) = nullptr;

};  // base_storage

template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment>
struct base_handlers;
//
template <std::size_t Footprint, std::size_t Alignment>
struct base_handlers<Footprint, false, false, Alignment> : base_storage<Footprint, Alignment>
{};
//
template <std::size_t Footprint, std::size_t Alignment>
struct base_handlers<Footprint, true, false, Alignment> : base_storage<Footprint, Alignment>
{
    void copy_handlers_from(const base_handlers& src) noexcept
    {
        base::copy_handlers_from(src);
        value_copier_ = src.value_copier_;
    }

    void reset() noexcept
    {
        value_copier_ = nullptr;
        base::reset();
    }

    // Holds type-erased value copyer. `nullptr` when copy operation is not supported.
    void (*value_copier_)(const void* src, void* dst) = nullptr;

private:
    using base = base_storage<Footprint, Alignment>;
};
//
template <std::size_t Footprint, std::size_t Alignment>
struct base_handlers<Footprint, false, true, Alignment> : base_storage<Footprint, Alignment>
{
    void copy_handlers_from(const base_handlers& src) noexcept
    {
        base::copy_handlers_from(src);
        value_mover_ = src.value_mover_;
    }

    void reset() noexcept
    {
        value_mover_ = nullptr;
        base::reset();
    }

    // Holds type-erased value mover. `nullptr` when move operation is not supported.
    void (*value_mover_)(void* src, void* dst) = nullptr;

private:
    using base = base_storage<Footprint, Alignment>;
};
//
template <std::size_t Footprint, std::size_t Alignment>
struct base_handlers<Footprint, true, true, Alignment> : base_storage<Footprint, Alignment>
{
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

    // Holds type-erased value copyer. `nullptr` when copy operation is not supported.
    void (*value_copier_)(const void* src, void* dst) = nullptr;

    // Holds type-erased value mover. `nullptr` when move operation is not supported.
    void (*value_mover_)(void* src, void* dst) = nullptr;

private:
    using base = base_storage<Footprint, Alignment>;

};  // base_handlers

// Copy policy.
//
template <std::size_t Footprint, bool Copyable, bool Moveable, std::size_t Alignment>
struct base_copy;
//
// Non-copyable case.
template <std::size_t Footprint, bool Moveable, std::size_t Alignment>
struct base_copy<Footprint, false, Moveable, Alignment> : base_handlers<Footprint, false, Moveable, Alignment>
{
    constexpr base_copy()                  = default;
    base_copy(const base_copy&)            = delete;
    base_copy& operator=(const base_copy&) = delete;
};
//
// Copyable case.
template <std::size_t Footprint, bool Moveable, std::size_t Alignment>
struct base_copy<Footprint, true, Moveable, Alignment> : base_handlers<Footprint, true, Moveable, Alignment>
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
        CETL_DEBUG_ASSERT(nullptr == base::value_copier_, "Expected to be empty before making handlers.");

        base::template make_handlers<Tp>();

        base::value_copier_ = [](const void* const src, void* const dst) {
            CETL_DEBUG_ASSERT(nullptr != src, "");
            CETL_DEBUG_ASSERT(nullptr != dst, "");

            new (dst) Tp(*static_cast<const Tp*>(src));
        };
    }

private:
    using base = base_handlers<Footprint, true, Moveable, Alignment>;

    void copy_from(const base_copy& src)
    {
        CETL_DEBUG_ASSERT(!base::has_value(), "Expected to be empty before copying from source.");

        if (src.has_value())
        {
            base::copy_handlers_from(src);

            CETL_DEBUG_ASSERT(nullptr != base::value_copier_, "");

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
struct base_move<Footprint, Copyable, false, Alignment> : base_copy<Footprint, Copyable, false, Alignment>
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
struct base_move<Footprint, Copyable, true, Alignment> : base_copy<Footprint, Copyable, true, Alignment>
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
        CETL_DEBUG_ASSERT(nullptr == base::value_mover_, "Expected to be empty before making handlers.");

        base::template make_handlers<Tp>();

        base::value_mover_ = [](void* const src, void* const dst) {
            CETL_DEBUG_ASSERT(nullptr != src, "");
            CETL_DEBUG_ASSERT(nullptr != dst, "");

            new (dst) Tp(std::move(*static_cast<Tp*>(src)));
        };
    }

private:
    using base = base_copy<Footprint, Copyable, true, Alignment>;

    void move_from(base_move& src) noexcept
    {
        CETL_DEBUG_ASSERT(!base::has_value(), "Expected to be empty before moving from source.");

        if (src.has_value())
        {
            base::copy_handlers_from(src);

            CETL_DEBUG_ASSERT(nullptr != base::value_mover_, "");

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

/// \brief The class `any` describes a type-safe container for single values of any copy and/or move constructible type.
///
/// \tparam Footprint Maximum size of a contained object (in bytes).
/// \tparam Copyable Determines whether a contained object is copy constructible.
/// \tparam Movable Determines whether a contained object is move constructible.
/// \tparam Alignment Alignment of storage for a contained object.
///
template <std::size_t Footprint,
          bool        Copyable  = true,
          bool        Movable   = Copyable,
          std::size_t Alignment = alignof(std::max_align_t)>
class any : detail::base_move<Footprint, Copyable, Movable, Alignment>
{
    using base = detail::base_move<Footprint, Copyable, Movable, Alignment>;

public:
    /// \brief Constructs an empty `any` object.
    constexpr any() = default;
    /// \brief Constructs an `any` object with a copy of the content of `other`.
    any(const any& other) = default;
    /// \brief Constructs an `any` object with the content of `other` using move semantics.
    any(any&& other) noexcept = default;

    /// \brief Constructs an `any` object with `value` using move semantics.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    ///
    template <
        typename ValueType,
        typename Tp = std::decay_t<ValueType>,
        typename = std::enable_if_t<!std::is_same<Tp, any>::value && !pf17::detail::is_in_place_type<ValueType>::value>>
    any(ValueType&& value)  // NOLINT(*-explicit-constructor)
    {
        create<Tp>(std::forward<ValueType>(value));
    }

    /// \brief Constructs an `any` object with in place constructed value.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit any(in_place_type_t<ValueType>, Args&&... args)
    {
        create<Tp>(std::forward<Args>(args)...);
    }

    /// \brief Constructs an `any` object with in place constructed value.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Up Type of the elements of the initializer list.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param list Initializer list to be forwarded to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType, typename Up, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit any(in_place_type_t<ValueType>, std::initializer_list<Up> list, Args&&... args)
    {
        create<Tp>(list, std::forward<Args>(args)...);
    }

    /// \brief Destroys the contained object if there is one.
    ~any()
    {
        reset();
    }

    /// \brief Assigns the content of `rhs` to `*this`.
    any& operator=(const any& rhs)
    {
        if (this != &rhs)
        {
            any(rhs).swap(*this);
        }
        return *this;
    }

    /// \brief Assigns the content of `rhs` to `*this` using move semantics.
    any& operator=(any&& rhs) noexcept
    {
        if (this != &rhs)
        {
            any(std::move(rhs)).swap(*this);
        }
        return *this;
    }

    /// \brief Assigns `value` to `*this` using move semantics.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    ///
    template <typename ValueType,
              typename Tp = std::decay_t<ValueType>,
              typename    = std::enable_if_t<!std::is_same<Tp, any>::value>>
    any& operator=(ValueType&& value)
    {
        any(std::forward<ValueType>(value)).swap(*this);
        return *this;
    }

    /// \brief Emplaces a new value to `*this`.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    Tp& emplace(Args&&... args)
    {
        reset();

        return create<Tp>(std::forward<Args>(args)...);
    }

    /// \brief Emplaces a new value to `*this`.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Up Type of the elements of the initializer list.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param list Initializer list to be forwarded to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
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

    /// \brief Swaps the content of `*this` with the content of `rhs` using copy semantics.
    ///
    /// In use for copyable-only `any` objects.
    ///
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

    /// \brief Swaps the content of `*this` with the content of `rhs` using move semantics.
    ///
    /// In use for moveable `any` objects.
    ///
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

/// \brief Typealias for `any` with the given `ValueType` with the default
/// footprint, copyability, movability, and alignment of the `ValueType`.
///
/// In use by `cetl::make_any` overloads to make them close to `std::make_any`.
///
template <typename ValueType>
using any_like = any<sizeof(ValueType),
                     std::is_copy_constructible<ValueType>::value,
                     std::is_move_constructible<ValueType>::value,
                     alignof(ValueType)>;

/// \brief Constructs an any object containing an object of type T, passing the provided arguments to T's constructor.
///
/// Equivalent to `cetl::any(cetl::in_place_type<ValueType>, std::forward<Args>(args)...)`.
///
template <typename ValueType, typename Any = any_like<ValueType>, typename... Args>
CETL_NODISCARD Any make_any(Args&&... args)
{
    return Any(in_place_type<ValueType>, std::forward<Args>(args)...);
}

/// \brief Constructs an any object containing an object of type T, passing the provided arguments to T's constructor.
///
/// Equivalent to `cetl::any(cetl::in_place_type<ValueType>, list, std::forward<Args>(args)...)`.
///
template <typename ValueType, typename Any = any_like<ValueType>, typename Up, typename... Args>
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

    using RawValueType = std::remove_cv_t<ValueType>;
    const auto ptr     = operand->template get_ptr<RawValueType>();

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

    using RawValueType = std::remove_cv_t<ValueType>;
    const auto ptr     = operand->template get_ptr<RawValueType>();

    using ReturnType = std::add_pointer_t<ValueType>;
    return static_cast<ReturnType>(ptr);
}

}  // namespace cetl

#endif  // CETL_ANY_HPP_INCLUDED
