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

inline std::nullptr_t throw_bad_alloc()
{
#if defined(__cpp_exceptions)
    throw std::bad_alloc();
#else
    return nullptr;
#endif
}

[[noreturn]] inline void throw_bad_unbounded_variant_access()
{
#if defined(__cpp_exceptions)
    throw bad_unbounded_variant_access();
#else
    CETL_DEBUG_ASSERT(false, "bad_unbounded_variant_access");
    std::terminate();
#endif
}

// MARK: - Storage policy.
//
template <std::size_t Footprint, bool IsPmr, std::size_t Alignment>
struct base_storage;
//
// In-Place only case.
template <std::size_t Footprint, std::size_t Alignment>
struct base_storage<Footprint, false /*IsPmr*/, Alignment>
{
    /// @brief Allocates enough raw storage for a target value.
    ///
    /// @param size_bytes Size of the target type in bytes.
    /// @return Pointer to the storage (never `nullptr` for this specialization).
    ///
    CETL_NODISCARD void* alloc_new_raw_storage(const std::size_t size_bytes) noexcept
    {
        CETL_DEBUG_ASSERT((0UL < size_bytes) && (size_bytes <= Footprint), "");
        CETL_DEBUG_ASSERT(0UL == value_size_, "This object is expected to be a brand new one.");

        // We need to store (presumably) allocated size b/c this is important
        // for detection of the "valueless by exception" state in case something goes wrong later
        // (like in-place value construction failure) - in such failure state
        // there will `>0` allocated size BUT `nullptr` handlers (f.e. `value_destroyer_`).
        //
        value_size_ = size_bytes;

        return inplace_buffer_;
    }

    CETL_NODISCARD bool copy_raw_storage_from(const base_storage& src) noexcept
    {
        CETL_DEBUG_ASSERT(0UL == value_size_, "This object is expected to be a brand new (or reset) one.");

        value_size_ = src.value_size_;

        return value_size_ > 0UL;
    }

    CETL_NODISCARD bool move_raw_storage_from(base_storage& src) noexcept
    {
        CETL_DEBUG_ASSERT(0UL == value_size_, "This object is expected to be a brand new (or reset) one.");

        value_size_ = src.value_size_;

        return value_size_ > 0UL;
    }

    void reset() noexcept
    {
        value_size_ = 0UL;
    }

    CETL_NODISCARD std::size_t get_value_size() const noexcept
    {
        return value_size_;
    }

    CETL_NODISCARD constexpr void* get_raw_storage() noexcept
    {
        CETL_DEBUG_ASSERT(0UL != value_size_, "");

        return inplace_buffer_;
    }

    CETL_NODISCARD constexpr const void* get_raw_storage() const noexcept
    {
        CETL_DEBUG_ASSERT(0UL != value_size_, "");

        return inplace_buffer_;
    }

private:
    // We need to align `inplace_buffer_` to the given value (maximum alignment by default).
    // Also, we need to ensure that the buffer is at least 1 byte long.
    // NB! It's intentional and by design that the `inplace_buffer_` is the very first member of `unbounded_variant`
    // memory layout. In such way pointer to a `unbounded_variant` and its stored value are the same -
    // could be useful during debugging/troubleshooting.
    alignas(Alignment) cetl::byte inplace_buffer_[std::max(Footprint, 1UL)];

    // Stores the size of the allocated space in the buffer for a value.
    // The size could be less than `Footprint`, but never zero except when variant is empty (valid and valueless).
    std::size_t value_size_{0UL};
};
//
// PMR only case.
template <std::size_t Alignment>
struct base_storage<0UL /*Footprint*/, true /*IsPmr*/, Alignment>
{
    base_storage()
        : value_size_{0UL}
        , allocated_buffer_{nullptr}
        , mem_res_{cetl::pmr::get_default_resource()}
    {
    }

    explicit base_storage(pmr::memory_resource* const mem_res)
        : value_size_{0UL}
        , allocated_buffer_{nullptr}
        , mem_res_{mem_res}
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res_, "");
    }

    CETL_NODISCARD pmr::memory_resource* get_memory_resource() const noexcept
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res_, "");
        return mem_res_;
    }

    pmr::memory_resource* set_memory_resource(pmr::memory_resource* mem_res) noexcept
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res, "");
        CETL_DEBUG_ASSERT(nullptr != mem_res_, "");

        std::swap(mem_res_, mem_res);
        return mem_res;
    }

    /// @brief Allocates enough raw storage for a target value.
    ///
    /// The storage is always attempted to be allocated from PMR. If it fails then this object
    /// will be in the valueless by exception state (see `valueless_by_exception()` method).
    ///
    /// @param size_bytes Size of the type in bytes.
    /// @return Pointer to the successfully allocated buffer.
    ///         Could be `nullptr` if PMR allocation fails and c++ exceptions are disabled.
    /// @throws `std::bad_alloc` if PMR allocation fails and c++ exceptions are enabled.
    ///
    CETL_NODISCARD void* alloc_new_raw_storage(const std::size_t size_bytes)
    {
        CETL_DEBUG_ASSERT(0UL < size_bytes, "");
        CETL_DEBUG_ASSERT((0UL == value_size_) && (nullptr == allocated_buffer_),
                          "This object is expected to be a brand new one.");

        // We need to store (presumably) allocated size, regardless of whether actual allocation below succeeded.
        // This is important for detection of the "valueless by exception" state in case something goes wrong later
        // (like the PRM allocation below, or a value construction failure even later) - in such failure state
        // there will `>0` allocated size BUT `nullptr` handlers (f.e. `value_destroyer_`).
        //
        value_size_ = size_bytes;

        allocated_buffer_ = static_cast<cetl::byte*>(get_memory_resource()->allocate(size_bytes, Alignment));
        if (nullptr == allocated_buffer_)
        {
            return throw_bad_alloc();
        }

        return allocated_buffer_;
    }

    CETL_NODISCARD bool copy_raw_storage_from(const base_storage& src)
    {
        CETL_DEBUG_ASSERT((0UL == value_size_) && (nullptr == allocated_buffer_),
                          "This object is expected to be a brand new (or reset) one.");

        mem_res_ = src.get_memory_resource();

        // We don't allocate buffer for the valueless variant.
        if (src.value_size_ == 0UL)
        {
            // Later we don't need to call copy-ctor for the valueless variant.
            return false;
        }

        return alloc_new_raw_storage(src.value_size_) != nullptr;
    }

    /// @brief Moves source memory resource and its possible allocated buffer to the destination.
    ///
    /// This is actually a value "move" operation, so there will be no move-ctor call later.
    ///
    /// @return `false` indicating that the value has been moved already (as part of allocated buffer borrowing).
    ///
    CETL_NODISCARD bool move_raw_storage_from(base_storage& src) noexcept
    {
        CETL_DEBUG_ASSERT((0UL == value_size_) && (nullptr == allocated_buffer_),
                          "This object is expected to be a brand new (or reset) one.");

        mem_res_          = src.get_memory_resource();
        value_size_       = src.value_size_;
        allocated_buffer_ = src.allocated_buffer_;

        // We've borrowed the buffer from the source.
        if (allocated_buffer_ != nullptr)
        {
            src.value_size_       = 0UL;
            src.allocated_buffer_ = nullptr;
        }

        // B/c there is no in-place storage (Footprint == 0),
        // we always move previously allocated buffer along with its value.
        return false;
    }

    void reset() noexcept
    {
        CETL_DEBUG_ASSERT((nullptr == allocated_buffer_) || (0UL != value_size_), "");

        if (nullptr != allocated_buffer_)
        {
            get_memory_resource()->deallocate(allocated_buffer_, value_size_, Alignment);
            allocated_buffer_ = nullptr;
        }

        value_size_ = 0UL;
    }

    CETL_NODISCARD std::size_t get_value_size() const noexcept
    {
        return value_size_;
    }

    CETL_NODISCARD void* get_raw_storage() noexcept
    {
        CETL_DEBUG_ASSERT(0UL != value_size_, "");
        CETL_DEBUG_ASSERT(nullptr != allocated_buffer_, "");

        return allocated_buffer_;
    }

    CETL_NODISCARD const void* get_raw_storage() const noexcept
    {
        CETL_DEBUG_ASSERT(0UL != value_size_, "");
        CETL_DEBUG_ASSERT(nullptr != allocated_buffer_, "");

        return allocated_buffer_;
    }

private:
    // Stores the size of the allocated buffer for a value.
    std::size_t           value_size_;
    cetl::byte*           allocated_buffer_;
    pmr::memory_resource* mem_res_;
};
//
// Both PMR & In-Place case.
template <std::size_t Footprint, std::size_t Alignment>
struct base_storage<Footprint, true /*IsPmr*/, Alignment>
{
    base_storage()
        : value_size_{0UL}
        , allocated_buffer_{nullptr}
        , mem_res_{cetl::pmr::get_default_resource()}
    {
    }

    explicit base_storage(pmr::memory_resource* const mem_res)
        : value_size_{0UL}
        , allocated_buffer_{nullptr}
        , mem_res_{mem_res}
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res_, "");
    }

    CETL_NODISCARD pmr::memory_resource* get_memory_resource() const noexcept
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res_, "");
        return mem_res_;
    }

    pmr::memory_resource* set_memory_resource(pmr::memory_resource* mem_res) noexcept
    {
        CETL_DEBUG_ASSERT(nullptr != mem_res, "");
        CETL_DEBUG_ASSERT(nullptr != mem_res_, "");

        std::swap(mem_res_, mem_res);
        return mem_res;
    }

    /// @brief Allocates enough raw storage for a target type value.
    ///
    /// The storage is either in-place buffer (if it fits), or attempted to be allocated from PMR. If it fails
    /// then this object will be in the valueless by exception state (see `valueless_by_exception()` method).
    ///
    /// @param size_bytes Size of the type in bytes.
    /// @return Pointer to the buffer (either in-place or allocated one).
    ///         Could be `nullptr` if PMR allocation fails and c++ exceptions are disabled.
    /// @throws `std::bad_alloc` if PMR allocation fails and c++ exceptions are enabled.
    ///
    CETL_NODISCARD void* alloc_new_raw_storage(const std::size_t size_bytes)
    {
        CETL_DEBUG_ASSERT(0UL < size_bytes, "");
        CETL_DEBUG_ASSERT((0UL == value_size_) && (nullptr == allocated_buffer_),
                          "This object is expected to be a brand new one.");

        // Regardless of the path (in-place or allocated buffer), we need to store (presumably) allocated size.
        // This is important for detection of the "valueless by exception" state in case something goes wrong later
        // (like possible PRM allocation below, or a value construction failure even later) -
        // in such failure state there will `>0` allocated size BUT `nullptr` handlers (f.e. `value_destroyer_`).
        //
        value_size_ = size_bytes;

        if (size_bytes <= Footprint)
        {
            return inplace_buffer_;
        }

        allocated_buffer_ = static_cast<cetl::byte*>(get_memory_resource()->allocate(size_bytes, Alignment));
        if (nullptr == allocated_buffer_)
        {
            return throw_bad_alloc();
        }

        return allocated_buffer_;
    }

    CETL_NODISCARD bool copy_raw_storage_from(const base_storage& src)
    {
        CETL_DEBUG_ASSERT((0UL == value_size_) && (nullptr == allocated_buffer_),
                          "This object is expected to be a brand new (or reset) one.");

        mem_res_ = src.get_memory_resource();

        // We don't even try to allocate buffer for the valueless variant.
        if (src.value_size_ == 0UL)
        {
            // Later we don't need to call copy-ctor for the valueless variant.
            return false;
        }

        return alloc_new_raw_storage(src.value_size_) != nullptr;
    }

    /// @brief Moves source memory resource and its possible allocated buffer to the destination.
    ///
    /// If source value was stored in the allocated buffer (in contrast to in-place buffer),
    /// the value will be moved as well, so there will be no move-ctor call later.
    ///
    /// @return `false` indicating that the value has been moved already (as part of allocated buffer borrowing).
    ///         Otherwise, move-ctor call has to be performed as well (by the caller).
    ///
    CETL_NODISCARD bool move_raw_storage_from(base_storage& src) noexcept
    {
        mem_res_          = src.mem_res_;
        value_size_       = src.value_size_;
        allocated_buffer_ = src.allocated_buffer_;

        // We've borrowed the buffer from the source.
        if (nullptr != allocated_buffer_)
        {
            src.value_size_       = 0UL;
            src.allocated_buffer_ = nullptr;
        }

        // Only borrowing of previously allocated buffer moves the value as well.
        // In contrast, in-place buffer value still remains in the source.
        return (value_size_ > 0UL) && (nullptr == allocated_buffer_);
    }

    void reset() noexcept
    {
        if (nullptr != allocated_buffer_)
        {
            get_memory_resource()->deallocate(allocated_buffer_, value_size_, Alignment);
            allocated_buffer_ = nullptr;
        }

        value_size_ = 0UL;
    }

    CETL_NODISCARD std::size_t get_value_size() const noexcept
    {
        return value_size_;
    }

    CETL_NODISCARD void* get_raw_storage() noexcept
    {
        CETL_DEBUG_ASSERT(0UL != value_size_, "");
        CETL_DEBUG_ASSERT((nullptr == allocated_buffer_) || (Footprint < value_size_), "");

        return (nullptr != allocated_buffer_) ? allocated_buffer_ : inplace_buffer_;
    }

    CETL_NODISCARD const void* get_raw_storage() const noexcept
    {
        CETL_DEBUG_ASSERT(0UL != value_size_, "");
        CETL_DEBUG_ASSERT((nullptr == allocated_buffer_) || (Footprint < value_size_), "");

        return (nullptr != allocated_buffer_) ? allocated_buffer_ : inplace_buffer_;
    }

private:
    // We need to align `inplace_buffer_` to the given value (maximum alignment by default).
    // NB! It's intentional and by design that the `inplace_buffer_` is the very first member of `unbounded_variant`
    // memory layout. In such way pointer to a `unbounded_variant` and its stored value are the same
    // in case of small object optimization - could be useful during debugging/troubleshooting.
    alignas(Alignment) cetl::byte inplace_buffer_[Footprint];

    // Stores the size of the allocated space in a buffer for a value.
    // The size could be either `<=Footprint` (in-place) or `>Footprint (PMR allocated),
    // but never zero except when variant is empty (valid and valueless).
    std::size_t           value_size_;
    cetl::byte*           allocated_buffer_;
    pmr::memory_resource* mem_res_;

};  // base_storage

// MARK: - Access policy.
//
template <std::size_t Footprint, std::size_t Alignment, bool IsPmr>
struct base_access : base_storage<Footprint, IsPmr, Alignment>
{
    base_access() = default;

    explicit base_access(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

    CETL_NODISCARD bool has_value() const noexcept
    {
        return (base::get_value_size() > 0UL) && (nullptr != value_destroyer_);
    }

    /// True if the variant is valueless b/c of an exception.
    ///
    /// Use `reset` method (or try assign a new value) to recover from this state.
    ///
    CETL_NODISCARD constexpr bool valueless_by_exception() const noexcept
    {
        // Missing destroyer is the sign that there was an attempt store a value (and hence value size is >0),
        // but something went wrong along the way (like PMR allocation or value construction failure).
        return (base::get_value_size() > 0UL) && (nullptr == value_destroyer_);
    }

    template <typename Tp>
    void make_handlers() noexcept
    {
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
        static_assert(sizeof(ValueType) <= Footprint || IsPmr,
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
        static_assert(sizeof(ValueType) <= Footprint || IsPmr,
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
        value_destroyer_       = src.value_destroyer_;
        value_converter_       = src.value_converter_;
        value_const_converter_ = src.value_const_converter_;
    }

    void move_handlers_from(base_access& src) noexcept
    {
        value_destroyer_       = src.value_destroyer_;
        value_converter_       = src.value_converter_;
        value_const_converter_ = src.value_const_converter_;

        src.reset();
    }

    void reset() noexcept
    {
        if (has_value())
        {
            value_destroyer_(base::get_raw_storage());
        }

        value_destroyer_       = nullptr;
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
    base_handlers() = default;

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
    base_handlers() = default;

    explicit base_handlers(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

    void copy_handlers_from(const base_handlers& src) noexcept
    {
        value_copier_ = src.value_copier_;

        base::copy_handlers_from(src);
    }

    void move_handlers_from(base_handlers& src) noexcept
    {
        value_copier_     = src.value_copier_;
        src.value_copier_ = nullptr;

        base::move_handlers_from(src);
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
    base_handlers() = default;

    explicit base_handlers(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

    void copy_handlers_from(const base_handlers& src) noexcept
    {
        value_mover_ = src.value_mover_;

        base::copy_handlers_from(src);
    }

    void move_handlers_from(base_handlers& src) noexcept
    {
        value_mover_     = src.value_mover_;
        src.value_mover_ = nullptr;

        base::move_handlers_from(src);
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
    base_handlers() = default;

    explicit base_handlers(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
    }

    void copy_handlers_from(const base_handlers& src) noexcept
    {
        value_mover_  = src.value_mover_;
        value_copier_ = src.value_copier_;

        base::copy_handlers_from(src);
    }

    void move_handlers_from(base_handlers& src) noexcept
    {
        value_mover_      = src.value_mover_;
        value_copier_     = src.value_copier_;
        src.value_mover_  = nullptr;
        src.value_copier_ = nullptr;

        base::move_handlers_from(src);
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
    base_copy()                            = default;
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
    base_copy() = default;

    base_copy(const base_copy& other)
        : base{}
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

        if (base::copy_raw_storage_from(src))
        {
            CETL_DEBUG_ASSERT(nullptr != src.value_copier_, "");

            src.value_copier_(src.get_raw_storage(), base::get_raw_storage());

            base::copy_handlers_from(src);
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
    base_move()                     = default;
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
    base_move()                            = default;
    base_move(const base_move&)            = default;
    base_move& operator=(const base_move&) = default;

    base_move(base_move&& other) noexcept
        : base{}
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

        if (base::move_raw_storage_from(src))
        {
            CETL_DEBUG_ASSERT(nullptr != src.value_mover_, "");

            src.value_mover_(src.get_raw_storage(), base::get_raw_storage());
        }

        base::move_handlers_from(src);
    }

};  // base_move

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
    using IsPmrT     = std::integral_constant<bool, IsPmr>;
    using IsMovableT = std::integral_constant<bool, Movable>;
    using base       = detail::base_move<Footprint, Copyable, Movable, Alignment, IsPmr>;

public:
    using base::reset;
    using base::has_value;
    using base::valueless_by_exception;

    /// \brief Constructs an empty `unbounded_variant` object.
    ///
    unbounded_variant() = default;

    explicit unbounded_variant(pmr::memory_resource* const mem_res)
        : base{mem_res}
    {
        static_assert(IsPmr, "Cannot construct non-PMR unbounded_variant with memory resource.");
    }

    /// \brief Constructs an `unbounded_variant` object with a copy of the content of `other`.
    ///
    unbounded_variant(const unbounded_variant& other) = default;

    /// \brief Constructs an `unbounded_variant` object with the content of `other` using move semantics.
    ///
    unbounded_variant(unbounded_variant&& other) noexcept = default;

    /// \brief Constructs an `unbounded_variant` object with `value` using move semantics.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    ///
    template <typename ValueType,
              typename Tp = std::decay_t<ValueType>,
              typename    = std::enable_if_t<!std::is_same<Tp, unbounded_variant>::value &&
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
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType, typename... Args, typename Tp = std::enable_if_t<IsPmr, std::decay_t<ValueType>>>
    explicit unbounded_variant(pmr::memory_resource* const mem_res, in_place_type_t<ValueType>, Args&&... args)
        : base{mem_res}
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

    /// \brief Constructs an `unbounded_variant` object with in place constructed value.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Up Type of the elements of the initializer list.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param list Initializer list to be forwarded to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType,
              typename Up,
              typename... Args,
              typename Tp = std::enable_if_t<IsPmr, std::decay_t<ValueType>>>
    explicit unbounded_variant(pmr::memory_resource* const mem_res,
                               in_place_type_t<ValueType>,
                               std::initializer_list<Up> list,
                               Args&&... args)
        : base{mem_res}
    {
        create<Tp>(list, std::forward<Args>(args)...);
    }

    /// \brief Destroys the contained object if there is one.
    ///
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
        makeVariant(IsPmrT{}, std::forward<ValueType>(value)).swap(*this);
        return *this;
    }

    /// \brief Emplaces a new value to `*this`.
    ///
    /// \tparam ValueType Type of the value to be stored. Its size must be less than or equal to `Footprint`.
    /// \tparam Args Types of arguments to be passed to the constructor of `ValueType`.
    /// \param args Arguments to be forwarded to the constructor of `ValueType`.
    ///
    template <typename ValueType, typename... Args, typename Tp = std::decay_t<ValueType>>
    Tp* emplace(Args&&... args)
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
    Tp* emplace(std::initializer_list<Up> list, Args&&... args)
    {
        reset();

        return create<Tp>(list, std::forward<Args>(args)...);
    }

    /// \brief Swaps the content of `*this` with the content of `rhs`
    /// using either move (if available) or copy semantics.
    ///
    void swap(unbounded_variant& rhs) noexcept(Movable)
    {
        if (this == &rhs)
        {
            return;
        }

        swapVariants(IsPmrT{}, IsMovableT{}, rhs);
    }

    CETL_NODISCARD pmr::memory_resource* get_memory_resource() const noexcept
    {
        static_assert(IsPmr, "Cannot get memory resource from non-PMR unbounded_variant.");

        return base::get_memory_resource();
    }

    pmr::memory_resource* set_memory_resource(pmr::memory_resource* const mem_res) noexcept
    {
        static_assert(IsPmr, "Cannot set memory resource to non-PMR unbounded_variant.");

        return base::set_memory_resource(mem_res);
    }

private:
    template <typename ValueType, typename UnboundedVariant>
    friend std::add_pointer_t<ValueType> get_if(UnboundedVariant* operand) noexcept;

    template <typename ValueType, typename UnboundedVariant>
    friend std::add_pointer_t<std::add_const_t<ValueType>> get_if(const UnboundedVariant* operand) noexcept;

    template <typename Tp, typename... Args>
    Tp* create(Args&&... args)
    {
        void* const raw_storage = base::alloc_new_raw_storage(sizeof(Tp));
        if (nullptr == raw_storage)
        {
            return nullptr;
        }

        auto* ptr = new (raw_storage) Tp(std::forward<Args>(args)...);

        base::template make_handlers<Tp>();

        return ptr;
    }

    template <typename ValueType>
    unbounded_variant makeVariant(std::false_type /*IsPmr*/, ValueType&& value)
    {
        return unbounded_variant(std::forward<ValueType>(value));
    }

    template <typename ValueType>
    unbounded_variant makeVariant(std::true_type /*IsPmr*/, ValueType&& value)
    {
        return unbounded_variant(get_memory_resource(), std::forward<ValueType>(value));
    }

    void swapVariants(std::false_type /*IsPmr*/, std::false_type /*Movable*/, unbounded_variant& rhs)
    {
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

    void swapVariants(std::true_type /*IsPmr*/, std::false_type /*Movable*/, unbounded_variant& rhs)
    {
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
                auto* const tmp_mr      = rhs.get_memory_resource();
                static_cast<base&>(rhs) = *this;
                reset();
                base::set_memory_resource(tmp_mr);
            }
        }
        else if (rhs.has_value())
        {
            auto* const tmp_mr        = get_memory_resource();
            static_cast<base&>(*this) = rhs;
            rhs.reset();
            rhs.set_memory_resource(tmp_mr);
        }
        else
        {
            auto* const tmp_mr = base::set_memory_resource(rhs.get_memory_resource());
            rhs.set_memory_resource(tmp_mr);
        }
    }

    void swapVariants(std::false_type /*IsPmr*/, std::true_type /*Movable*/, unbounded_variant& rhs) noexcept
    {
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

    void swapVariants(std::true_type /*IsPmr*/, std::true_type /*Movable*/, unbounded_variant& rhs) noexcept
    {
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
                auto* const tmp_mr      = rhs.get_memory_resource();
                static_cast<base&>(rhs) = std::move(*this);
                base::set_memory_resource(tmp_mr);
            }
        }
        else if (rhs.has_value())
        {
            auto* const tmp_mr        = get_memory_resource();
            static_cast<base&>(*this) = std::move(rhs);
            rhs.set_memory_resource(tmp_mr);
        }
        else
        {
            auto* const tmp_mr = base::set_memory_resource(rhs.get_memory_resource());
            rhs.set_memory_resource(tmp_mr);
        }
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
