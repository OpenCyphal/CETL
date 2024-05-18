/// @file
/// Includes cetl::unbounded_variant type and non-member functions.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_UNBOUNDED_VARIANT_HPP_INCLUDED
#define CETL_UNBOUNDED_VARIANT_HPP_INCLUDED

#include "rtti.hpp"
#include "pf17/cetlpf.hpp"
#include "pf17/utility.hpp"

#include <algorithm>
#include <initializer_list>

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

namespace cetl
{

#if defined(__cpp_exceptions) || defined(CETL_DOXYGEN)

/// \brief Defines a type of object to be thrown by the `get` on failure.
///
/// This is only available if exceptions are enabled (`__cpp_exceptions` is defined).
///
class bad_unbounded_variant_access : public std::bad_cast
{
public:
    bad_unbounded_variant_access() noexcept                                               = default;
    bad_unbounded_variant_access(const bad_unbounded_variant_access&) noexcept            = default;
    bad_unbounded_variant_access(bad_unbounded_variant_access&&) noexcept                 = default;
    bad_unbounded_variant_access& operator=(const bad_unbounded_variant_access&) noexcept = default;
    bad_unbounded_variant_access& operator=(bad_unbounded_variant_access&&) noexcept      = default;
    ~bad_unbounded_variant_access() noexcept override                                     = default;

    CETL_NODISCARD const char* what() const noexcept override
    {
        return "bad unbounded variant access";
    }
};

#endif  // defined(__cpp_exceptions) || defined(CETL_DOXYGEN)

// Forward declarations
template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment, bool IsPmr>
class unbounded_variant;

namespace detail
{

// MARK: - Storage policy.
//
template <std::size_t Footprint, bool IsPmr, std::size_t Alignment>
struct base_storage;

template <std::size_t Footprint, std::size_t Alignment>
struct base_storage<Footprint, false /*IsPmr*/, Alignment>
{
    template <typename Tp>
    void make_handlers() noexcept
    {
        static_assert(sizeof(Tp) <= std::max(Footprint, 1UL), "Enlarge the footprint");
    }

    void copy_handlers_from(const base_storage&) noexcept {}

    void reset() noexcept
    {
        // Nothing to reset - we have in-place buffer.
    }

    CETL_NODISCARD void* get_raw_storage() noexcept
    {
        return inplace_buffer_;
    }

    CETL_NODISCARD const void* get_raw_storage() const noexcept
    {
        return inplace_buffer_;
    }

private:
    // We need to align `inplace_buffer_` to the given value (maximum alignment by default).
    // Also, we need to ensure that the buffer is at least 1 byte long.
    // NB! It's intentional and by design that the `inplace_buffer_` is the very first member of `unbounded_variant`
    // memory layout. In such way pointer to a `unbounded_variant` and its stored value are the same -
    // could be useful during debugging/troubleshooting.
    alignas(Alignment) cetl::byte inplace_buffer_[std::max(Footprint, 1UL)];
};

template <std::size_t Alignment>
struct base_storage<0UL /*Footprint*/, true /*IsPmr*/, Alignment>
{
    base_storage() = delete;
    explicit base_storage(pmr::memory_resource* const mem_res)
        : mem_res_{mem_res}
    {
    }

    template <typename Tp>
    void make_handlers() noexcept
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res_, "");
        CETL_DEBUG_ASSERT((0UL == allocated_size_) && (nullptr == allocated_buffer_), "");

        allocated_buffer_ = static_cast<cetl::byte*>(mem_res_->allocate(sizeof(Tp), Alignment));
        allocated_size_   = (nullptr != allocated_buffer_) ? sizeof(Tp) : 0;
    }

    void copy_handlers_from(const base_storage& src) noexcept
    {
        mem_res_ = src.mem_res_;
    }

    void reset() noexcept
    {
        CETL_DEBUG_ASSERT((0UL == allocated_size_) == (nullptr == allocated_buffer_), "");

        if (nullptr != allocated_buffer_)
        {
            CETL_DEBUG_ASSERT(nullptr != mem_res_, "");

            mem_res_->deallocate(allocated_buffer_, allocated_size_, Alignment);
            allocated_buffer_ = nullptr;
            allocated_size_   = 0;
        }
    }

    CETL_NODISCARD void* get_raw_storage() noexcept
    {
        CETL_DEBUG_ASSERT(0UL != allocated_size_, "");
        CETL_DEBUG_ASSERT(nullptr == allocated_buffer_, "");

        return allocated_buffer_;
    }

    CETL_NODISCARD const void* get_raw_storage() const noexcept
    {
        CETL_DEBUG_ASSERT(0UL != allocated_size_, "");
        CETL_DEBUG_ASSERT(nullptr == allocated_buffer_, "");

        return allocated_buffer_;
    }

private:
    std::size_t           allocated_size_{0};
    cetl::byte*           allocated_buffer_{nullptr};
    pmr::memory_resource* mem_res_{nullptr};
};

template <std::size_t Footprint, std::size_t Alignment>
struct base_storage<Footprint, true /*IsPmr*/, Alignment>
{
    template <typename Tp>
    void make_handlers() noexcept
    {
        CETL_DEBUG_ASSERT((0UL == allocated_size_) && (nullptr == allocated_buffer_), "");

        // TODO: conditionally allocate memory depending on whether it fits in the footprint.
        // CETL_DEBUG_ASSERT(nullptr != mem_res_, "");
        // allocated_buffer_ = static_cast<cetl::byte*>(mem_res_->allocate(sizeof(Tp), Alignment));
        // allocated_size_   = (nullptr != allocated_buffer_) ? sizeof(Tp) : 0;
    }

    void copy_handlers_from(const base_storage& src) noexcept
    {
        mem_res_ = src.mem_res_;
    }

    void reset() noexcept
    {
        CETL_DEBUG_ASSERT((nullptr == allocated_buffer_) == (0UL == allocated_size_), "");

        if (nullptr != allocated_buffer_)
        {
            CETL_DEBUG_ASSERT(nullptr != mem_res_, "");

            mem_res_->deallocate(allocated_buffer_, allocated_size_, Alignment);
            allocated_buffer_ = nullptr;
            allocated_size_   = 0;
        }
    }

    CETL_NODISCARD void* get_raw_storage() noexcept
    {
        CETL_DEBUG_ASSERT((nullptr == allocated_buffer_) == (0UL == allocated_size_), "");

        return allocated_size_ != 0UL ? allocated_buffer_ : inplace_buffer_;
    }

    CETL_NODISCARD const void* get_raw_storage() const noexcept
    {
        CETL_DEBUG_ASSERT((nullptr == allocated_buffer_) == (0UL == allocated_size_), "");

        return allocated_size_ != 0UL ? allocated_buffer_ : inplace_buffer_;
    }

private:
    // We need to align `inplace_buffer_` to the given value (maximum alignment by default).
    // NB! It's intentional and by design that the `inplace_buffer_` is the very first member of `unbounded_variant`
    // memory layout. In such way pointer to a `unbounded_variant` and its stored value are the same
    // in case of small object optimization - could be useful during debugging/troubleshooting.
    alignas(Alignment) cetl::byte inplace_buffer_[Footprint];

    std::size_t           allocated_size_{0};
    cetl::byte*           allocated_buffer_{nullptr};
    pmr::memory_resource* mem_res_{nullptr};
};

// MARK: - Access policy.
//
template <std::size_t Footprint, std::size_t Alignment, bool IsPmr>
struct base_access : base_storage<Footprint, IsPmr, Alignment>
{
    constexpr base_access() = default;

    explicit base_access(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

    CETL_NODISCARD bool has_value() const noexcept
    {
        return nullptr != value_destroyer_;
    }

    template <typename Tp>
    void make_handlers() noexcept
    {
        base::template make_handlers<Tp>();

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
        static_assert(sizeof(ValueType) <= Footprint && !IsPmr,
                      "Cannot contain the requested type since the footprint is too small");

        if (!has_value())
        {
            return nullptr;
        }
        CETL_DEBUG_ASSERT(nullptr != value_const_converter_, "Non-empty storage is expected to have value converter.");

        return value_converter_(base::get_raw_storage(), type_id_value<ValueType>);
    }

    template <typename ValueType>
    CETL_NODISCARD const void* get_ptr() const noexcept
    {
        static_assert(sizeof(ValueType) <= Footprint && !IsPmr,
                      "Cannot contain the requested type since the footprint is too small");

        if (!has_value())
        {
            return nullptr;
        }
        CETL_DEBUG_ASSERT(nullptr != value_const_converter_, "Non-empty storage is expected to have value converter.");

        return value_const_converter_(base::get_raw_storage(), type_id_value<ValueType>);
    }

    void copy_handlers_from(const base_access& src) noexcept
    {
        base::copy_handlers_from(src);

        value_destroyer_       = src.value_destroyer_;
        value_converter_       = src.value_converter_;
        value_const_converter_ = src.value_const_converter_;
    }

    void reset() noexcept
    {
        if (value_destroyer_)
        {
            value_destroyer_(base::get_raw_storage());
            value_destroyer_ = nullptr;
        }

        value_converter_       = nullptr;
        value_const_converter_ = nullptr;

        base::reset();
    }

private:
    using base = base_storage<Footprint, IsPmr, Alignment>;

    // Holds type-erased value destroyer. `nullptr` if storage has no value stored.
    void (*value_destroyer_)(void* self) = nullptr;

    // Holds type-erased value converters (const and non-const). `nullptr` if storage has no value stored.
    //
    void* (*value_converter_)(void* self, const type_id& id)                   = nullptr;
    const void* (*value_const_converter_)(const void* self, const type_id& id) = nullptr;

};  // base_access

// MARK: - Handlers policy.
//
template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment, bool IsPmr>
struct base_handlers;
//
template <std::size_t Footprint, std::size_t Alignment, bool IsPmr>
struct base_handlers<Footprint, false /*Copyable*/, false /*Moveable*/, Alignment, IsPmr>
    : base_access<Footprint, Alignment, IsPmr>
{
    constexpr base_handlers() = default;

    explicit base_handlers(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

private:
    using base = base_access<Footprint, Alignment, IsPmr>;
};
//
template <std::size_t Footprint, std::size_t Alignment, bool IsPmr>
struct base_handlers<Footprint, true /*Copyable*/, false /*Moveable*/, Alignment, IsPmr>
    : base_access<Footprint, Alignment, IsPmr>
{
    constexpr base_handlers() = default;

    explicit base_handlers(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

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
    using base = base_access<Footprint, Alignment, IsPmr>;
};
//
template <std::size_t Footprint, std::size_t Alignment, bool IsPmr>
struct base_handlers<Footprint, false /*Copyable*/, true /*Moveable*/, Alignment, IsPmr>
    : base_access<Footprint, Alignment, IsPmr>
{
    constexpr base_handlers() = default;

    explicit base_handlers(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

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
    using base = base_access<Footprint, Alignment, IsPmr>;
};
//
template <std::size_t Footprint, std::size_t Alignment, bool IsPmr>
struct base_handlers<Footprint, true /*Copyable*/, true /*Moveable*/, Alignment, IsPmr>
    : base_access<Footprint, Alignment, IsPmr>
{
    constexpr base_handlers() = default;

    explicit base_handlers(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

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
    using base = base_access<Footprint, Alignment, IsPmr>;

};  // base_handlers

// MARK: - Copy policy.
//
template <std::size_t Footprint, bool Copyable, bool Moveable, std::size_t Alignment, bool IsPmr>
struct base_copy;
//
// Non-copyable case.
template <std::size_t Footprint, bool Moveable, std::size_t Alignment, bool IsPmr>
struct base_copy<Footprint, false /*Copyable*/, Moveable, Alignment, IsPmr>
    : base_handlers<Footprint, false /*Copyable*/, Moveable, Alignment, IsPmr>
{
    constexpr base_copy()                  = default;
    base_copy(const base_copy&)            = delete;
    base_copy& operator=(const base_copy&) = delete;

    explicit base_copy(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

private:
    using base = base_handlers<Footprint, false, Moveable, Alignment, IsPmr>;
};
//
// Copyable case.
template <std::size_t Footprint, bool Moveable, std::size_t Alignment, bool IsPmr>
struct base_copy<Footprint, true /*Copyable*/, Moveable, Alignment, IsPmr>
    : base_handlers<Footprint, true /*Copyable*/, Moveable, Alignment, IsPmr>
{
    constexpr base_copy() = default;

    base_copy(const base_copy& other)
    {
        copy_from(other);
    }

    explicit base_copy(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
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
    using base = base_handlers<Footprint, true, Moveable, Alignment, IsPmr>;

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

// MARK: - Move policy.
//
template <std::size_t Footprint, bool Copyable, bool Movable, std::size_t Alignment, bool IsPmr>
struct base_move;
//
// Non-movable case.
template <std::size_t Footprint, bool Copyable, std::size_t Alignment, bool IsPmr>
struct base_move<Footprint, Copyable, false /*Movable*/, Alignment, IsPmr>
    : base_copy<Footprint, Copyable, false /*Movable*/, Alignment, IsPmr>
{
    constexpr base_move()           = default;
    base_move(const base_move&)     = default;
    base_move(base_move&&) noexcept = delete;

    base_move& operator=(const base_move&)     = default;
    base_move& operator=(base_move&&) noexcept = delete;

    explicit base_move(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

private:
    using base = base_copy<Footprint, Copyable, false, Alignment, IsPmr>;
};
//
// Movable case.
template <std::size_t Footprint, bool Copyable, std::size_t Alignment, bool IsPmr>
struct base_move<Footprint, Copyable, true /*Movable*/, Alignment, IsPmr>
    : base_copy<Footprint, Copyable, true /*Movable*/, Alignment, IsPmr>
{
    constexpr base_move()                  = default;
    base_move(const base_move&)            = default;
    base_move& operator=(const base_move&) = default;

    base_move(base_move&& other) noexcept
    {
        move_from(other);
    }

    explicit base_move(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
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
    using base = base_copy<Footprint, Copyable, true, Alignment, IsPmr>;

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

[[noreturn]] inline void throw_bad_unbounded_variant_access()
{
#if defined(__cpp_exceptions)
    throw bad_unbounded_variant_access();
#else
    std::terminate();
#endif
}

}  // namespace detail

/// \brief The class `unbounded_variant` describes a type-safe container
///        for single values of unbounded_variant copy and/or move constructible type.
///
/// \tparam Footprint Maximum size of a contained object (in bytes).
/// \tparam Copyable Determines whether a contained object is copy constructible.
/// \tparam Movable Determines whether a contained object is move constructible.
/// \tparam Alignment Alignment of storage for a contained object.
///
template <std::size_t Footprint,
          bool        Copyable  = true,
          bool        Movable   = Copyable,
          std::size_t Alignment = alignof(std::max_align_t),
          bool        IsPmr     = false>
class unbounded_variant : detail::base_move<Footprint, Copyable, Movable, Alignment, IsPmr>
{
    using base = detail::base_move<Footprint, Copyable, Movable, Alignment, IsPmr>;

public:
    /// \brief Constructs an empty `unbounded_variant` object.
    constexpr unbounded_variant() = default;
    /// \brief Constructs an `unbounded_variant` object with a copy of the content of `other`.
    unbounded_variant(const unbounded_variant& other) = default;
    /// \brief Constructs an `unbounded_variant` object with the content of `other` using move semantics.
    unbounded_variant(unbounded_variant&& other) noexcept = default;

    /// \brief Constructs an `unbounded_variant` object with `value` using move semantics.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    ///
    template <typename ValueType,
              typename Tp = std::decay_t<ValueType>,
              typename    = std::enable_if_t<!std::is_same<Tp, unbounded_variant>::value && !IsPmr &&
                                             !pf17::detail::is_in_place_type<ValueType>::value>>
    unbounded_variant(ValueType&& value)  // NOLINT(*-explicit-constructor)
    {
        create<Tp>(std::forward<ValueType>(value));
    }

    template <typename ValueType,
              typename Tp = std::decay_t<ValueType>,
              typename    = std::enable_if_t<!std::is_same<Tp, unbounded_variant>::value && IsPmr &&
                                             !pf17::detail::is_in_place_type<ValueType>::value>>
    unbounded_variant(pmr::memory_resource* const mem_res, ValueType&& value)
        : base{mem_res}
    {
        create<Tp>(std::forward<ValueType>(value));
    }

    explicit unbounded_variant(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

    /// \brief Constructs an `unbounded_variant` object with in place constructed value.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit unbounded_variant(in_place_type_t<ValueType>, Args&&... args)
    {
        create<Tp>(std::forward<Args>(args)...);
    }

    /// \brief Constructs an `unbounded_variant` object with in place constructed value.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Up Type of the elements of the initializer list.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param list Initializer list to be forwarded to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType, typename Up, typename... Args, typename Tp = std::decay_t<ValueType>>
    explicit unbounded_variant(in_place_type_t<ValueType>, std::initializer_list<Up> list, Args&&... args)
    {
        create<Tp>(list, std::forward<Args>(args)...);
    }

    /// \brief Destroys the contained object if there is one.
    ~unbounded_variant()
    {
        reset();
    }

    /// \brief Assigns the content of `rhs` to `*this`.
    unbounded_variant& operator=(const unbounded_variant& rhs)
    {
        if (this != &rhs)
        {
            unbounded_variant(rhs).swap(*this);
        }
        return *this;
    }

    /// \brief Assigns the content of `rhs` to `*this` using move semantics.
    unbounded_variant& operator=(unbounded_variant&& rhs) noexcept
    {
        if (this != &rhs)
        {
            unbounded_variant(std::move(rhs)).swap(*this);
        }
        return *this;
    }

    /// \brief Assigns `value` to `*this` using move semantics.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    ///
    template <typename ValueType,
              typename Tp = std::decay_t<ValueType>,
              typename    = std::enable_if_t<!std::is_same<Tp, unbounded_variant>::value>>
    unbounded_variant& operator=(ValueType&& value)
    {
        unbounded_variant(std::forward<ValueType>(value)).swap(*this);
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
    /// In use for copyable-only `unbounded_variant` objects.
    ///
    template <bool CopyableAlias = Copyable,
              bool MovableAlias  = Movable,
              typename           = std::enable_if_t<CopyableAlias && !MovableAlias>>
    void swap(unbounded_variant& rhs)
    {
        if (this == &rhs)
        {
            return;
        }

        if (has_value())
        {
            if (rhs.has_value())
            {
                unbounded_variant tmp{rhs};
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
    /// In use for moveable `unbounded_variant` objects.
    ///
    template <bool MovableAlias = Movable, typename = std::enable_if_t<MovableAlias>>
    void swap(unbounded_variant& rhs) noexcept
    {
        if (this == &rhs)
        {
            return;
        }

        if (has_value())
        {
            if (rhs.has_value())
            {
                unbounded_variant tmp{std::move(rhs)};
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
    template <typename ValueType, typename UnboundedVariant>
    friend std::add_pointer_t<ValueType> get_if(UnboundedVariant* operand) noexcept;

    template <typename ValueType, typename UnboundedVariant>
    friend std::add_pointer_t<std::add_const_t<ValueType>> get_if(const UnboundedVariant* operand) noexcept;

    template <typename Tp, typename... Args>
    Tp& create(Args&&... args)
    {
        base::template make_handlers<Tp>();
        return *new (base::get_raw_storage()) Tp(std::forward<Args>(args)...);
    }

};  // class unbounded_variant

/// \brief Typealias for `unbounded_variant` with the given `ValueType` with the default
/// footprint, copyability, movability, and alignment of the `ValueType`.
///
/// In use by `cetl::make_unbounded_variant` overloads.
///
template <typename ValueType>
using unbounded_variant_like = unbounded_variant<sizeof(ValueType),
                                                 std::is_copy_constructible<ValueType>::value,
                                                 std::is_move_constructible<ValueType>::value,
                                                 alignof(ValueType)>;

/// \brief Constructs an unbounded_variant object containing an object of type T,
///        passing the provided arguments to T's constructor.
///
/// Equivalent to `cetl::unbounded_variant(cetl::in_place_type<ValueType>, std::forward<Args>(args)...)`.
///
template <typename ValueType, typename UnboundedVariant = unbounded_variant_like<ValueType>, typename... Args>
CETL_NODISCARD UnboundedVariant make_unbounded_variant(Args&&... args)
{
    return UnboundedVariant(in_place_type<ValueType>, std::forward<Args>(args)...);
}

/// \brief Constructs an unbounded_variant object containing an object of type T,
///        passing the provided arguments to T's constructor.
///
/// Equivalent to `cetl::unbounded_variant(cetl::in_place_type<ValueType>, list, std::forward<Args>(args)...)`.
///
template <typename ValueType,
          typename UnboundedVariant = unbounded_variant_like<ValueType>,
          typename Up,
          typename... Args>
CETL_NODISCARD UnboundedVariant make_unbounded_variant(std::initializer_list<Up> list, Args&&... args)
{
    return UnboundedVariant(in_place_type<ValueType>, list, std::forward<Args>(args)...);
}

/// \brief Performs type-safe access to the contained object.
///
/// \param operand Target unbounded_variant object.
/// \return Returns `std::static_cast<ValueType>(*cetl::get_if<const U>(&operand))`,
///     where let `U` be `std::remove_cv_t<std::remove_reference_t<ValueType>>`.
///
template <typename ValueType, typename UnboundedVariant>
CETL_NODISCARD ValueType get(const UnboundedVariant& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, const RawValueType&>::value,
                  "ValueType is required to be a const lvalue reference "
                  "or a CopyConstructible type");

    const auto ptr = get_if<std::add_const_t<RawValueType>>(&operand);
    if (ptr == nullptr)
    {
        detail::throw_bad_unbounded_variant_access();
    }
    return static_cast<ValueType>(*ptr);
}

/// \brief Performs type-safe access to the contained object.
///
/// \param operand Target unbounded_variant object.
/// \return Returns `std::static_cast<ValueType>(*cetl::get_if<U>(&operand))`,
///     where let `U` be `std::remove_cv_t<std::remove_reference_t<ValueType>>`.
///
template <typename ValueType, typename UnboundedVariant>
CETL_NODISCARD ValueType get(UnboundedVariant& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, RawValueType&>::value,
                  "ValueType is required to be an lvalue reference "
                  "or a CopyConstructible type");

    const auto ptr = get_if<RawValueType>(&operand);
    if (ptr == nullptr)
    {
        detail::throw_bad_unbounded_variant_access();
    }
    return static_cast<ValueType>(*ptr);
}

/// \brief Performs type-safe access to the contained object.
///
/// \param operand Target unbounded_variant object.
/// \return Returns `std::static_cast<ValueType>(std::move(*cetl::get_if<U>(&operand)))`,
///     where let `U` be `std::remove_cv_t<std::remove_reference_t<ValueType>>`.
///
template <typename ValueType, typename UnboundedVariant>
CETL_NODISCARD ValueType get(UnboundedVariant&& operand)
{
    using RawValueType = std::remove_cv_t<std::remove_reference_t<ValueType>>;
    static_assert(std::is_constructible<ValueType, RawValueType>::value,
                  "ValueType is required to be an rvalue reference "
                  "or a CopyConstructible type");

    const auto ptr = get_if<RawValueType>(&operand);
    if (ptr == nullptr)
    {
        detail::throw_bad_unbounded_variant_access();
    }
    return static_cast<ValueType>(std::move(*ptr));
}

/// \brief Performs type-safe access to the `const` contained object.
///
/// \tparam ValueType Type of the requested value; may not be a reference.
/// \tparam UnboundedVariant Type of the `unbounded_variant` object.
/// \param operand Target constant unbounded_variant object.
/// \return If operand is not a null pointer,
///     and the typeid of the requested `ValueType` matches that of the contents of operand,
///     a pointer to the value contained by operand, otherwise a null pointer.
///
template <typename ValueType, typename UnboundedVariant>
CETL_NODISCARD std::add_pointer_t<std::add_const_t<ValueType>> get_if(const UnboundedVariant* const operand) noexcept
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
/// \tparam UnboundedVariant Type of the `unbounded_variant` object.
/// \param operand Target `unbounded_variant` object.
/// \return If operand is not a null pointer,
///     and the typeid of the requested `ValueType` matches that of the contents of operand,
///     a pointer to the value contained by operand, otherwise a null pointer.
///
template <typename ValueType, typename UnboundedVariant>
CETL_NODISCARD std::add_pointer_t<ValueType> get_if(UnboundedVariant* const operand) noexcept
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

#endif  // CETL_UNBOUNDED_VARIANT_HPP_INCLUDED
