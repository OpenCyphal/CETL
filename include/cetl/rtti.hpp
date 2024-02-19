/// @file
///
/// An alternative implementation of simple runtime type information (RTTI) capability designed for high-integrity
/// real-time systems, where the use of the standard C++ RTTI is discouraged.
///
/// Unlike the standard C++ RTTI, this implementation allows the user to manually specify the type ID per type
/// in the form of a 16-byte UUID (GUID), query runtime type information in constant time,
/// and perform safe dynamic type conversion in constant time (like \c dynamic_cast).
///
/// In order to support this RTTI capability, a type must opt in by at least defining a public
/// static constexpr member named \c _type_id_ of type \ref cetl::type_id; if the type is polymorphic,
/// it should also implement the \ref cetl::rtti interface (more on this in the documentation of said interface).
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_RTTI_HPP_INCLUDED
#define CETL_RTTI_HPP_INCLUDED

#include <cetl/pf17/optional.hpp>  // This could be optionally replaced with cetlpf.hpp.
#include <cetl/pf17/attribute.hpp>

#include <array>
#include <cstdint>
#include <type_traits>

namespace cetl
{
constexpr std::size_t type_id_size = 16;

/// A 16-byte UUID (GUID) that uniquely identifies a type.
/// The user is responsible for ensuring that each type that has opted into this RTTI capability has a unique type ID
/// exposed via a static constexpr member named \c _type_id_ of this type.
using type_id = std::array<std::uint8_t, type_id_size>;

/// A null option is used to represent a type that has not opted into this RTTI capability (undefined type ID).
using maybe_type_id = pf17::optional<type_id>;

namespace detail
{
template <typename T>
auto has_type_id_impl(int) -> decltype(std::declval<std::decay_t<T>&>()._type_id_, std::true_type{});
template <typename>
std::false_type has_type_id_impl(...);
}  // namespace detail

/// True iff \c T has a public field named \c _type_id_ of type \ref cetl::type_id;
/// i.e., \c T supports the RTTI capability.
template <typename T>
constexpr bool has_type_id = decltype(detail::has_type_id_impl<std::decay_t<T>>(0))::value;

/// A polymorphic interface that allows the user to query runtime type information and perform safe dynamic type
/// conversion in constant time.
///
/// In order to support this RTTI implementation, a type must opt in by at least defining a public
/// static constexpr member named \c _type_id_ of type \ref cetl::type_id; if the type is polymorphic,
/// it should also implement this interface.
///
/// The members of this type are named with surrounding underscores to avoid name clashes with user-defined members,
/// including the members of DSDL types (where identifiers surrounded with underscores are reserved).
class rtti
{
    friend type_id get_type_id(const rtti& obj) noexcept;

    template <typename T, typename P, std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int>>
    friend T rtti_cast(class rtti*) noexcept;
    template <typename T, typename P, std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int>>
    friend const P* rtti_cast(const class rtti*) noexcept;

protected:
    /// The method body should be implemented as follows,
    /// where \c _type_id_ is a static constexpr member of type \ref cetl::type_id:
    ///
    /// @code
    /// return _type_id_;
    /// @endcode
    ///
    /// This method should not be invoked directly; instead, use the \ref cetl::get_type_id function.
    CETL_NODISCARD virtual type_id _get_type_id_() const noexcept = 0;

    /// The method body should be implemented as follows,
    /// where \c _type_id_ is a static constexpr member of type \ref cetl::type_id:
    ///
    /// @code
    /// return (id == _type_id_) ? this : base::_cast_(id);
    /// @endcode
    ///
    /// Where \c base is the base class of the current class that implements this method.
    /// The objective here is to check if the current type in the hierarchy is the one being sought;
    /// if not, delegate the call to the base class; the topmost class is this \c rtti type that
    /// simply returns a null pointer, indicating that the search has returned no matches and a
    /// safe dynamic type conversion is not possible.
    ///
    /// This method should not be invoked directly; instead, use the \ref cetl::rtti_cast function.
    /// @{
    CETL_NODISCARD virtual void* _cast_(const type_id& id) noexcept
    {
        (void) id;
        return nullptr;
    }
    CETL_NODISCARD virtual const void* _cast_(const type_id& id) const noexcept
    {
        (void) id;
        return nullptr;
    }
    /// @}

    rtti()                       = default;
    rtti(const rtti&)            = default;
    rtti(rtti&&)                 = default;
    virtual ~rtti() noexcept     = default;
    rtti& operator=(const rtti&) = default;
    rtti& operator=(rtti&&)      = default;
};

/// Returns the type ID of the given object.
/// This overload is for objects that implement the \ref cetl::rtti interface; it simply forwards the result of the
/// \ref cetl::rtti::_get_type_id_ method.
///
/// Generic code is recommended to convert the result of this function into \ref maybe_type_id.
CETL_NODISCARD inline type_id get_type_id(const rtti& obj) noexcept
{
    return obj._get_type_id_();
}
/// Returns the type ID of the given object.
/// This overload is selected for objects that do not implement the \ref cetl::rtti interface but provide a static
/// constexpr member named \c _type_id_ of type \ref cetl::type_id.
///
/// Generic code is recommended to convert the result of this function into \ref maybe_type_id.
template <typename T, std::enable_if_t<has_type_id<T>, int> = 0>
CETL_NODISCARD constexpr type_id get_type_id(const T& obj) noexcept
{
    return obj._type_id_;
}
/// Returns the type ID of the given object. This is a fallback overload for objects that are not RTTI-capable;
/// it always returns an empty option.
template <typename T, std::enable_if_t<!has_type_id<T>, int> = 0>
CETL_NODISCARD constexpr maybe_type_id get_type_id(const T&) noexcept
{
    return {};
}

/// Performs a safe dynamic type conversion in constant time by invoking \ref cetl::rtti::_cast_.
/// T shall be a pointer of a type that contains a public static constexpr member named \c _type_id_ of type
/// \ref cetl::type_id.
/// Returns nullptr if a safe dynamic type conversion to T is not possible.
/// @{
template <typename T,
          typename P                                                         = std::remove_pointer_t<T>,
          std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int> = 0>
CETL_NODISCARD T rtti_cast(rtti* const obj) noexcept
{
    return (obj == nullptr) ? nullptr : static_cast<T>(obj->_cast_(P::_type_id_));
}
template <typename T,
          typename P                                                         = std::remove_pointer_t<T>,
          std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int> = 0>
CETL_NODISCARD const P* rtti_cast(const rtti* const obj) noexcept
{
    return (obj == nullptr) ? nullptr : static_cast<const P*>(obj->_cast_(P::_type_id_));
}
/// @}

}  // namespace cetl

#endif  // CETL_RTTI_HPP_INCLUDED
