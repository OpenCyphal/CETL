/// @file
/// An alternative implementation of simple runtime type information (RTTI) capability designed for high-integrity
/// real-time systems, where the use of the standard C++ RTTI is discouraged.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_RTTI_HPP_INCLUDED
#define CETL_RTTI_HPP_INCLUDED

#include <cetl/pf17/attribute.hpp>

#include <array>
#include <cstdint>
#include <type_traits>

namespace cetl
{
/// This many bytes are used to represent a type ID. This is enough to hold a standard UUID (GUID).
constexpr std::size_t type_id_size = 16;

/// A 16-byte UUID (GUID) that uniquely identifies a type.
/// The user is responsible for ensuring that each type that has opted into this RTTI capability has a unique type ID
/// exposed via a public method <tt>static constexpr cetl::type_id _get_static_type_id_() noexcept</tt>.
using type_id = std::array<std::uint8_t, type_id_size>;

namespace detail
{
template <typename T>
auto has_type_id_impl(int) -> decltype(std::declval<std::decay_t<T>&>()._get_static_type_id_(), std::true_type{});
template <typename>
std::false_type has_type_id_impl(...);
}  // namespace detail

/// True iff \c T has a public static method \c _get_static_type_id_().
template <typename T>
constexpr bool has_type_id = decltype(detail::has_type_id_impl<std::decay_t<T>>(0))::value;

/// An alternative implementation of simple runtime type information (RTTI) capability designed for high-integrity
/// real-time systems, where the use of the standard C++ RTTI is discouraged.
///
/// Unlike the standard C++ RTTI, this implementation allows/requires the user to manually specify the type ID per type
/// in the form of a 16-byte UUID (GUID), query runtime type information in constant time,
/// and perform safe dynamic type down-conversion in constant time (similar to \c dynamic_cast).
/// The limitations are that a type has to opt into this RTTI capability explicitly and that CV-qualifiers
/// are not considered in the type comparison.
///
/// In order to support this RTTI capability, a type must opt in by at least defining a public method
/// <tt>static constexpr cetl::type_id _get_static_type_id_() noexcept</tt>;
/// such types satisfy \ref cetl::has_type_id.
/// If the type is polymorphic, it should also implement this interface.
///
/// The static \c _get_static_type_id_() method returns the type ID of the type it is defined on.
/// It could be a static member variable as well, but in C++14 dealing with static members
/// of non-literal types is not as straightforward as in C++17, so we use a static method instead.
/// The virtual \ref rtti::_get_polymorphic_type_id_() method forwards the call to the static
/// \c _get_static_type_id_() method of the actual type of the object;
/// see the method documentation for details.
///
/// RTTI-related members are named with surrounding underscores to avoid name clashes with user-defined members,
/// including the members of DSDL types (where identifiers surrounded with underscores are reserved).
class rtti
{
    friend type_id get_type_id(const rtti& obj) noexcept;

    template <typename T, typename P, std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int>>
    friend T rtti_cast(rtti*) noexcept;
    template <typename T, typename P, std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int>>
    friend const P* rtti_cast(const rtti*) noexcept;

    friend bool is_instance_of(const rtti& obj, const type_id& id) noexcept;

protected:
    /// The method body should simply forward the result of this type's static \c _get_static_type_id_ method:
    ///
    /// @code
    /// return _get_static_type_id_();
    /// @endcode
    ///
    /// This method should not be invoked directly; instead, use \ref cetl::get_type_id.
    CETL_NODISCARD virtual type_id _get_polymorphic_type_id_() const noexcept = 0;

    /// The method body should be implemented as follows (add \c const in the const overload):
    ///
    /// @code
    /// return (id == _get_static_type_id_()) ? static_cast<void*>(this) : base::_cast_(id);
    /// @endcode
    ///
    /// Where \c base is the base class of the current class that implements this method.
    /// The objective here is to check if the current type in the hierarchy is the one being sought;
    /// if not, delegate the call to the base class; the topmost class is this \c rtti type that
    /// simply returns a null pointer, indicating that the search has returned no matches and a
    /// safe dynamic type conversion is not possible.
    ///
    /// This method should not be invoked directly; instead, use \ref cetl::rtti_cast.
    /// @{
    CETL_NODISCARD virtual void* _cast_(const type_id& id) & noexcept
    {
        (void) id;
        return nullptr;
    }
    CETL_NODISCARD virtual const void* _cast_(const type_id& id) const& noexcept
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

/// Returns the type ID of the given type.
/// This function is provided for regularity; it simply forwards the call to \c T::_get_static_type_id_().
/// The type shall satisfy \ref cetl::has_type_id.
template <typename T>
CETL_NODISCARD constexpr type_id get_type_id() noexcept
{
    return std::decay_t<T>::_get_static_type_id_();
}
/// Returns the type ID of the given object.
/// This overload is for objects that implement the \ref cetl::rtti interface; it simply forwards the result of the
/// \ref cetl::rtti::_get_polymorphic_type_id_() method.
CETL_NODISCARD inline type_id get_type_id(const rtti& obj) noexcept
{
    return obj._get_polymorphic_type_id_();
}
/// Returns the type ID of the given object.
/// This overload is selected for objects that do not implement the \ref cetl::rtti interface but satisfy
/// \ref cetl::has_type_id.
template <typename T, std::enable_if_t<has_type_id<T>, int> = 0>
CETL_NODISCARD constexpr type_id get_type_id(const T&) noexcept
{
    return get_type_id<T>();
}

/// Performs a safe dynamic type conversion in constant time by invoking \ref cetl::rtti::_cast_.
/// T shall satisfy \ref cetl::has_type_id.
/// Returns nullptr if a safe dynamic type conversion to T is not possible.
/// @{
template <typename T,
          typename P                                                         = std::remove_pointer_t<T>,
          std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int> = 0>
CETL_NODISCARD T rtti_cast(rtti* const obj) noexcept
{
    return (obj == nullptr) ? nullptr : static_cast<T>(obj->_cast_(get_type_id<P>()));
}
template <typename T,
          typename P                                                         = std::remove_pointer_t<T>,
          std::enable_if_t<std::is_pointer<T>::value && has_type_id<P>, int> = 0>
CETL_NODISCARD const P* rtti_cast(const rtti* const obj) noexcept
{
    return (obj == nullptr) ? nullptr : static_cast<const P*>(obj->_cast_(get_type_id<P>()));
}
/// @}

/// Indicates whether the given polymorphic object is an instance of the type with the given type ID.
/// For example, given a polymorphic type hierarchy <tt>A<-B<-C</tt>,
/// \c is_instance_of(C{}, get_type_id<X>()) is true for \c X in \c {A,B,C}.
CETL_NODISCARD inline bool is_instance_of(const rtti& obj, const type_id& id) noexcept
{
    return nullptr != obj._cast_(id);
}
/// Indicates whether the given polymorphic object is an instance of the given type.
/// T shall satisfy \ref cetl::has_type_id.
/// Refer to the non-template overload for more details.
template <typename T>
CETL_NODISCARD bool is_instance_of(const rtti& obj) noexcept
{
    return is_instance_of(obj, get_type_id<T>());
}

}  // namespace cetl

#endif  // CETL_RTTI_HPP_INCLUDED
