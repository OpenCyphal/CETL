/// @file
/// A simple runtime type information (RTTI) support designed for high-integrity
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
#include <utility>
#include <type_traits>

namespace cetl
{
/// This many bytes are used to represent a type ID. This is enough to hold a standard UUID (GUID).
constexpr std::size_t type_id_size = 16;

/// A 16-byte UUID (GUID) that uniquely identifies a type.
/// The user is responsible for ensuring that each type that has opted into this RTTI capability has a unique type ID
/// exposed via a public method <tt>static constexpr cetl::type_id _get_static_type_id_() noexcept</tt>.
using type_id = std::array<std::uint8_t, type_id_size>;

/// This is used for representing the type ID of a type as a type, similar to \ref std::integral_constant.
/// The bytes of the UUID are given as a list of template parameters; there shall be at most 16 of them;
/// if any are missing, they are assumed to be 0.
/// For conversion to \ref type_id use \ref cetl::type_id_type_value.
template <std::uint8_t... Bytes>
using type_id_type = std::integer_sequence<std::uint8_t, Bytes...>;

namespace detail
{
// has_static_type_id_impl
template <typename T>
auto has_static_type_id_impl(int) -> decltype(std::decay_t<T>::_get_static_type_id_(), std::true_type{});
template <typename>
std::false_type has_static_type_id_impl(...);

// has_polymorphic_type_id_impl
template <typename T>
auto has_polymorphic_type_id_impl(int)
    -> decltype(std::declval<std::decay_t<T>&>()._get_polymorphic_type_id_(), std::true_type{});
template <typename>
std::false_type has_polymorphic_type_id_impl(...);

// has_cast_impl
template <typename T>
auto has_cast_impl(int) -> decltype(std::declval<std::decay_t<T>&>()._cast_(std::declval<type_id>()), std::true_type{});
template <typename>
std::false_type has_cast_impl(...);

// type_id_type_value_impl
template <std::uint8_t... Bytes>
constexpr type_id type_id_type_value_impl(const type_id_type<Bytes...>) noexcept
{
    return {{Bytes...}};
}
}  // namespace detail

/// True iff \c T has a public static method \c _get_static_type_id_().
template <typename T>
constexpr bool has_static_type_id = decltype(detail::has_static_type_id_impl<std::decay_t<T>>(0))::value;

/// True iff \c T has a public method \c _get_polymorphic_type_id_().
/// One could argue that instead of relying on this trait, we could just use `const rtti&` or
/// `std::is_base_of<rtti, T>`.
/// This won't work in the presence of multiple inheritance because it makes implicit conversion
/// to the base class ambiguous.
template <typename T>
constexpr bool has_polymorphic_type_id = decltype(detail::has_polymorphic_type_id_impl<std::decay_t<T>>(0))::value;

/// True iff \c T has a public method \c _cast_().
template <typename T>
constexpr bool is_rtti_convertible = decltype(detail::has_cast_impl<std::decay_t<T>>(0))::value;

/// A helper that converts \ref cetl::type_id_type to \ref cetl::type_id.
template <typename TypeIDType>
constexpr type_id type_id_type_value = detail::type_id_type_value_impl(TypeIDType{});

/// An alternative implementation of simple runtime type information (RTTI) capability designed for high-integrity
/// real-time systems, where the use of the standard C++ RTTI is discouraged.
///
/// Unlike the standard C++ RTTI, this implementation requires the user to manually specify the type ID per type
/// in the form of a 16-byte UUID (GUID). The capabilities include querying runtime type information in constant time
/// and performing safe dynamic type down-conversion (and up-conversion, similar to \c dynamic_cast) in constant time,
/// including the case of multiple inheritance.
///
/// The limitations are that a type has to opt into this RTTI capability explicitly (it doesn't work for all types)
/// and that CV-qualifiers and references are not considered in the type comparison;
/// that is, <tt>T</tt> and <tt>const T&</tt> are considered the same type.
///
/// In order to support this RTTI capability, a type must at least define a public method
/// <tt>static constexpr cetl::type_id _get_static_type_id_() noexcept</tt> that
/// returns the type ID (i.e., the UUID) of the type it is defined on.
/// Types that provide said method satisfy \ref cetl::has_static_type_id.
/// If the type is polymorphic, it should publicly inherit from \ref cetl::rtti_helper, which will provide the
/// necessary implementation of the RTTI-related methods along with the aforementioned
/// \c _get_static_type_id_() method.
///
/// RTTI-related members are named with surrounding underscores to avoid name clashes with user-defined members,
/// including the members of DSDL types (where identifiers surrounded with underscores are reserved).
/// The user code should not invoke such underscored methods directly but instead use the free functions defined here.
class rtti
{
public:
    /// Implementations are adivsed to use \ref cetl::rtti_helper instead of implementing this manually.
    /// If manual implementation is preferred, then the method body should simply forward the result of this
    /// type's static \c _get_static_type_id_ method:
    ///
    /// @code
    /// return _get_static_type_id_();
    /// @endcode
    ///
    /// This method should not be invoked directly; instead, use \ref cetl::get_type_id.
    CETL_NODISCARD virtual type_id _get_polymorphic_type_id_() const noexcept = 0;

    /// Implementations are adivsed to use \ref cetl::rtti_helper instead of implementing this manually.
    /// If manual implementation is preferred, then the method body should be implemented as follows
    /// (add \c const in the const overload):
    ///
    /// @code
    /// return (id == _get_static_type_id_()) ? static_cast<void*>(this) : base::_cast_(id);
    /// @endcode
    ///
    /// Where \c base is the base class of the current class that implements this method;
    /// if there is more than one base available, all of those that implement this \ref cetl::rtti interface
    /// should be checked in the same way one by one and the first match (non-nullptr result) should be returned.
    ///
    /// The objective here is to check if the current type in the hierarchy is the one being sought;
    /// if not, delegate the call to the base class(es); the topmost class is this \c rtti type that
    /// simply returns a null pointer, indicating that the search has returned no matches and a
    /// safe dynamic type conversion is not possible (at least not along the current branch of the type tree).
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

protected:
    rtti()                       = default;
    rtti(const rtti&)            = default;
    rtti(rtti&&)                 = default;
    virtual ~rtti() noexcept     = default;
    rtti& operator=(const rtti&) = default;
    rtti& operator=(rtti&&)      = default;
};

/// Non-polymorphic types that want to support RTTI should simply provide a \c _get_static_type_id_()
/// method that returns \ref cetl::type_id; there is no need for them to use this helper.
///
/// Polymorphic types, on the other hand, are trickier because their runtime type may not be the same as their
/// static type; for this reason, they should publicly inherit from this helper class,
/// which will provide the necessary implementation of the RTTI-related methods
/// along with the aforementioned \c _get_static_type_id_ method.
///
/// This helper shall be the first base class in the inheritance list. This is because it assumes the equivalency
/// between a pointer to itself and a pointer to the derived type, which is only guaranteed if it is the first base.
/// This restriction could be lifted with the help of CRTP, but it could make usage a bit more complex.
///
/// If a polymorphic type inherits from other classes that also implement the \ref cetl::rtti interface,
/// such inheritance should be done not directly but through this helper; that is, instead of inheriting
/// from \c A and \c B, the type should inherit from \c rtti_helper<A,B>.
/// This is done to inform the helper about the type hierarchy, allowing it to perform an exhaustive search
/// for a matching conversion throughout the entire type hierarchy tree in the presence of multiple inheritance.
///
/// Conversion to an ambiguous base class is not allowed in C++ (except for the case of virtual inheritance
///
/// \tparam TypeIDType The type ID encoded via \ref cetl::type_id_type.
/// \tparam FirstBase  An optional base class that implements the \ref cetl::rtti interface.
/// \tparam OtherBases Other base classes that implement the \ref cetl::rtti interface, if any.
template <typename TypeIDType, typename FirstBase = cetl::rtti, typename... OtherBases>
struct rtti_helper : public FirstBase, public OtherBases...
{
    static constexpr type_id _get_static_type_id_() noexcept
    {
        return type_id_type_value<TypeIDType>;
    }
    type_id _get_polymorphic_type_id_() const noexcept override
    {
        return _get_static_type_id_();
    }
    void* _cast_(const type_id& id) & noexcept override
    {
        return (id == _get_static_type_id_()) ? static_cast<void*>(this) : search<FirstBase, OtherBases...>(id);
    }
    const void* _cast_(const cetl::type_id& id) const& noexcept override
    {
        return (id == _get_static_type_id_()) ? static_cast<const void*>(this) : search<FirstBase, OtherBases...>(id);
    }

private:
    // Exhaustively search for a matching conversion throughout the entire type hierarchy tree.
    // Template parameter pack expansion is not available in C++14 so we do it the hard way.
    template <typename T>
    void* search(const type_id& id) noexcept
    {
        return T::_cast_(id);
    }
    template <typename A, typename B, typename... C>
    void* search(const type_id& id) noexcept
    {
        if (void* const p = search<A>(id))
        {
            return p;
        }
        return search<B, C...>(id);
    }
    // Same but const.
    template <typename T>
    const void* search(const type_id& id) const noexcept
    {
        return T::_cast_(id);
    }
    template <typename A, typename B, typename... C>
    const void* search(const type_id& id) const noexcept
    {
        if (const void* const p = search<A>(id))
        {
            return p;
        }
        return search<B, C...>(id);
    }
};

/// Returns the type ID of the given type.
/// This function is provided for regularity; it simply forwards the call to \c T::_get_static_type_id_().
/// The type shall satisfy \ref cetl::has_static_type_id.
template <typename T>
CETL_NODISCARD constexpr type_id get_type_id() noexcept
{
    return std::decay_t<T>::_get_static_type_id_();
}
/// Returns the type ID of the given object.
/// This overload is for objects that implement the \ref cetl::rtti interface; it simply forwards the result of the
/// \ref cetl::rtti::_get_polymorphic_type_id_() method.
template <typename T, std::enable_if_t<has_polymorphic_type_id<T>, int> = 0>
CETL_NODISCARD type_id get_type_id(const T& obj) noexcept
{
    return obj._get_polymorphic_type_id_();
}
/// Returns the type ID of the given object.
/// This overload is selected for objects that do not implement the \ref cetl::rtti interface but satisfy
/// \ref cetl::has_static_type_id.
template <typename T, std::enable_if_t<has_static_type_id<T> && !has_polymorphic_type_id<T>, int> = 0>
CETL_NODISCARD constexpr type_id get_type_id(const T&) noexcept
{
    return get_type_id<T>();
}

/// Performs a safe dynamic type up-/down-conversion in constant time by invoking \ref cetl::rtti::_cast_.
/// \c T shall be a pointer and \c std::remove_pointer_t<T> shall satisfy \ref cetl::has_static_type_id.
/// Returns \c nullptr if a safe dynamic type conversion to \c T is not possible.
/// @{
template <typename T, typename _from>
CETL_NODISCARD std::enable_if_t<is_rtti_convertible<_from> &&     //
                                    std::is_pointer<T>::value &&  //
                                    has_static_type_id<std::remove_pointer_t<T>>,
                                T>
               rtti_cast(_from* const obj) noexcept
{
    return (obj == nullptr)  //
               ? nullptr
               : static_cast<T>(obj->_cast_(get_type_id<std::remove_pointer_t<T>>()));
}
template <typename T, typename _from>
CETL_NODISCARD std::enable_if_t<is_rtti_convertible<_from> &&     //
                                    std::is_pointer<T>::value &&  //
                                    has_static_type_id<std::remove_pointer_t<T>>,
                                const std::remove_pointer_t<T>*>
               rtti_cast(const _from* const obj) noexcept
{
    return (obj == nullptr)  //
               ? nullptr
               : static_cast<const std::remove_pointer_t<T>*>(obj->_cast_(get_type_id<std::remove_pointer_t<T>>()));
}
/// @}

/// Detects whether the given polymorphic object is an instance of the type with the given type ID.
/// For example, given a polymorphic type hierarchy <tt>A<-B<-C</tt>,
/// \c is_instance_of(C{}, get_type_id<X>())==true for \c X in \c {A,B,C};
/// while \c is_instance_of(A{}, get_type_id<X>())==true only for \c X=A.
///
/// Note that the type of the object argument is not `rtti&` but the actual type of the object,
/// because in the presence of multiple inheritance implicit conversion to the base class is ambiguous.
template <typename _u>
CETL_NODISCARD std::enable_if_t<is_rtti_convertible<_u>, bool> is_instance_of(const _u& obj, const type_id& id) noexcept
{
    return nullptr != obj._cast_(id);
}
/// Detects whether the given polymorphic object is an instance of the given type.
/// T shall satisfy \ref cetl::has_static_type_id.
/// Refer to the non-template overload for details.
template <typename Q, typename _u>
CETL_NODISCARD std::enable_if_t<is_rtti_convertible<_u>, bool> is_instance_of(const _u& obj) noexcept
{
    return is_instance_of(obj, get_type_id<Q>());
}

}  // namespace cetl

#endif  // CETL_RTTI_HPP_INCLUDED
