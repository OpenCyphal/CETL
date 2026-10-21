/// @file
/// An explicit mechanism for implementing dynamic type introspection for high-integrity systems.
/// This should be used instead of standard C++ RunTime Type Information (RTTI) (i.e. -fno-rtti)
/// where a codebase must trace from lines of source back to verifications (correctness)
/// and requirements (suitability).
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT

#ifndef CETL_RTTI_HPP_INCLUDED
#define CETL_RTTI_HPP_INCLUDED

#ifndef CETL_H_ERASE
#    include "cetl/cetl.hpp"
#endif

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>
#include <type_traits>

namespace cetl
{
/// This many bytes are used to represent a type ID. This is enough to hold a standard UUID (GUID).
constexpr std::size_t type_id_size = 16;

/// A 16-byte UUID (GUID) that uniquely identifies a type.
/// The user is responsible for ensuring that each type that has opted into this RTTI capability has a unique type ID
/// exposed via a public method <tt>static constexpr cetl::type_id _get_type_id_() noexcept</tt>.
using type_id = std::array<std::uint8_t, type_id_size>;

/// This is used for representing the type ID of a type as a type, similar to \ref std::integral_constant.
/// The bytes of the UUID are given as a list of template parameters; there shall be at most 16 of them;
/// if any are missing, they are assumed to be 0.
/// For conversion to \ref type_id use \ref cetl::type_id_type_value.
/// Please don't use empty or all zeros bytes (reserved for `type_id_value<void>` specialization).
template <std::uint8_t... Bytes>
using type_id_type = std::integer_sequence<std::uint8_t, Bytes...>;

namespace detail
{
// has_type_id_impl
template <typename T>
auto has_type_id_impl(int) -> decltype(std::decay_t<T>::_get_type_id_(), std::true_type{});
template <typename>
std::false_type has_type_id_impl(...);

// has_cast_impl
template <typename T>
auto has_cast_impl(int) -> decltype(std::declval<std::decay_t<T>&>()._cast_(std::declval<type_id>()), std::true_type{});
template <typename>
std::false_type has_cast_impl(...);

// is_any_of
template <typename T, typename... Ts>
struct is_any_of : std::false_type
{};
template <typename T, typename Head, typename... Tail>
struct is_any_of<T, Head, Tail...>
    : std::conditional_t<std::is_same<T, Head>::value, std::true_type, is_any_of<T, Tail...>>
{};

// has_builtin_type_id: the built-in types that CETL provides type_id_getter specializations for.
template <typename T>
constexpr bool has_builtin_type_id = is_any_of<T,
                                               bool,
                                               char,
                                               signed char,
                                               unsigned char,
                                               wchar_t,
                                               char16_t,
                                               char32_t,
#if defined(__cpp_char8_t)
                                               char8_t,
#endif
                                               short,
                                               unsigned short,
                                               int,
                                               unsigned int,
                                               long,
                                               unsigned long,
                                               long long,
                                               unsigned long long,
                                               float,
                                               double,
                                               long double,
                                               std::nullptr_t>::value;

// type_id_type_value_impl
template <std::uint8_t... Bytes>
constexpr type_id type_id_type_value_impl(const type_id_type<Bytes...>) noexcept
{
    return {{Bytes...}};
}
}  // namespace detail

/// True iff \c T has a public static method \c _get_type_id_() or is a built-in type for which CETL provides
/// a \ref cetl::type_id_getter specialization. CV-qualifiers and references are ignored.
/// User-provided \ref cetl::type_id_getter specializations are not detected.
template <typename T>
constexpr bool has_type_id =
    decltype(detail::has_type_id_impl<std::decay_t<T>>(0))::value || detail::has_builtin_type_id<std::decay_t<T>>;

/// True iff \c T has a public method \c _cast_().
template <typename T>
constexpr bool is_rtti_convertible = decltype(detail::has_cast_impl<std::decay_t<T>>(0))::value;

/// A helper that converts \ref cetl::type_id_type to \ref cetl::type_id.
template <typename TypeIDType>
constexpr type_id type_id_type_value() noexcept
{
    return detail::type_id_type_value_impl(TypeIDType{});
}

/// The type ID getter for the given type.
/// This helper is provided for regularity; it returns the same value as \c T::_get_type_id_().
/// The type shall satisfy \ref cetl::has_type_id.
/// Specialize this getter to add RTTI support to types where it is not possible to define a static method
/// (e.g., pointers, third-party classes, etc). The built-in types are already specialized by CETL below.
/// Specialize it only for cv-unqualified non-reference types; \ref cetl::type_id_value strips the qualifiers.
template <typename T>
constexpr type_id type_id_getter() noexcept
{
    return T::_get_type_id_();
}

/// The type ID getter specialization reserved for `void` type - returns all zeros.
///
template <>
constexpr type_id type_id_getter<void>() noexcept
{
    return {};
}

/// Type ID getter specializations for the built-in (fundamental) types, which cannot define \c _get_type_id_().
/// These make the built-in types usable with \ref cetl::unbounded_variant and the RTTI helpers out of the box.
/// The values are fixed UUIDs that are part of the CETL API; they will not change between releases.
/// @{
// NOLINTBEGIN(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
// 6B02B2B9-610B-414E-9304-E7FC5BC0D061
template <>
constexpr type_id type_id_getter<bool>() noexcept
{
    return {0x6B, 0x02, 0xB2, 0xB9, 0x61, 0x0B, 0x41, 0x4E, 0x93, 0x04, 0xE7, 0xFC, 0x5B, 0xC0, 0xD0, 0x61};
}
// 05855903-D323-41C3-8C58-691E035507D8
template <>
constexpr type_id type_id_getter<char>() noexcept
{
    return {0x05, 0x85, 0x59, 0x03, 0xD3, 0x23, 0x41, 0xC3, 0x8C, 0x58, 0x69, 0x1E, 0x03, 0x55, 0x07, 0xD8};
}
// CA295A7D-851F-42B0-9E4B-B9D318C01CCC
template <>
constexpr type_id type_id_getter<signed char>() noexcept
{
    return {0xCA, 0x29, 0x5A, 0x7D, 0x85, 0x1F, 0x42, 0xB0, 0x9E, 0x4B, 0xB9, 0xD3, 0x18, 0xC0, 0x1C, 0xCC};
}
// 6BC0579E-B665-480A-AFB0-45DB755A143E
template <>
constexpr type_id type_id_getter<unsigned char>() noexcept
{
    return {0x6B, 0xC0, 0x57, 0x9E, 0xB6, 0x65, 0x48, 0x0A, 0xAF, 0xB0, 0x45, 0xDB, 0x75, 0x5A, 0x14, 0x3E};
}
// ADB357FE-DDB1-4A14-B41C-035D2D1FBB6D
template <>
constexpr type_id type_id_getter<wchar_t>() noexcept
{
    return {0xAD, 0xB3, 0x57, 0xFE, 0xDD, 0xB1, 0x4A, 0x14, 0xB4, 0x1C, 0x03, 0x5D, 0x2D, 0x1F, 0xBB, 0x6D};
}
// 9E7A3934-4D6D-40F4-8155-D8524D8687E1
template <>
constexpr type_id type_id_getter<char16_t>() noexcept
{
    return {0x9E, 0x7A, 0x39, 0x34, 0x4D, 0x6D, 0x40, 0xF4, 0x81, 0x55, 0xD8, 0x52, 0x4D, 0x86, 0x87, 0xE1};
}
// 2AF4DA75-4A3F-4769-8DE3-4A39B38F7812
template <>
constexpr type_id type_id_getter<char32_t>() noexcept
{
    return {0x2A, 0xF4, 0xDA, 0x75, 0x4A, 0x3F, 0x47, 0x69, 0x8D, 0xE3, 0x4A, 0x39, 0xB3, 0x8F, 0x78, 0x12};
}
#if defined(__cpp_char8_t)
// 95F505AD-CFB9-477E-B538-BCDB71667E64
template <>
constexpr type_id type_id_getter<char8_t>() noexcept
{
    return {0x95, 0xF5, 0x05, 0xAD, 0xCF, 0xB9, 0x47, 0x7E, 0xB5, 0x38, 0xBC, 0xDB, 0x71, 0x66, 0x7E, 0x64};
}
#endif
// 0E1E2001-3D58-43CF-ABF5-CDC937C3A016
template <>
constexpr type_id type_id_getter<short>() noexcept
{
    return {0x0E, 0x1E, 0x20, 0x01, 0x3D, 0x58, 0x43, 0xCF, 0xAB, 0xF5, 0xCD, 0xC9, 0x37, 0xC3, 0xA0, 0x16};
}
// 3C22EF31-63C0-4710-9AAE-966E89134C19
template <>
constexpr type_id type_id_getter<unsigned short>() noexcept
{
    return {0x3C, 0x22, 0xEF, 0x31, 0x63, 0xC0, 0x47, 0x10, 0x9A, 0xAE, 0x96, 0x6E, 0x89, 0x13, 0x4C, 0x19};
}
// AA3F7C4D-0E44-43CB-AB4C-2AE19E646F91
template <>
constexpr type_id type_id_getter<int>() noexcept
{
    return {0xAA, 0x3F, 0x7C, 0x4D, 0x0E, 0x44, 0x43, 0xCB, 0xAB, 0x4C, 0x2A, 0xE1, 0x9E, 0x64, 0x6F, 0x91};
}
// 89A2F7BC-5BEA-47BF-96C4-CFFA3A2DBBB2
template <>
constexpr type_id type_id_getter<unsigned int>() noexcept
{
    return {0x89, 0xA2, 0xF7, 0xBC, 0x5B, 0xEA, 0x47, 0xBF, 0x96, 0xC4, 0xCF, 0xFA, 0x3A, 0x2D, 0xBB, 0xB2};
}
// F2C4457F-55EB-45DE-888C-F54506129768
template <>
constexpr type_id type_id_getter<long>() noexcept
{
    return {0xF2, 0xC4, 0x45, 0x7F, 0x55, 0xEB, 0x45, 0xDE, 0x88, 0x8C, 0xF5, 0x45, 0x06, 0x12, 0x97, 0x68};
}
// 6A4E9AEC-AB7C-45E0-AF75-1574C93C529A
template <>
constexpr type_id type_id_getter<unsigned long>() noexcept
{
    return {0x6A, 0x4E, 0x9A, 0xEC, 0xAB, 0x7C, 0x45, 0xE0, 0xAF, 0x75, 0x15, 0x74, 0xC9, 0x3C, 0x52, 0x9A};
}
// 28A39F1C-EDF3-4973-AB97-ADA9FBFAFE5D
template <>
constexpr type_id type_id_getter<long long>() noexcept
{
    return {0x28, 0xA3, 0x9F, 0x1C, 0xED, 0xF3, 0x49, 0x73, 0xAB, 0x97, 0xAD, 0xA9, 0xFB, 0xFA, 0xFE, 0x5D};
}
// FBFECFA7-B41C-4A59-9D4C-FF977CC4877E
template <>
constexpr type_id type_id_getter<unsigned long long>() noexcept
{
    return {0xFB, 0xFE, 0xCF, 0xA7, 0xB4, 0x1C, 0x4A, 0x59, 0x9D, 0x4C, 0xFF, 0x97, 0x7C, 0xC4, 0x87, 0x7E};
}
// 42844900-45ED-41A0-AA63-D6A42B60B343
template <>
constexpr type_id type_id_getter<float>() noexcept
{
    return {0x42, 0x84, 0x49, 0x00, 0x45, 0xED, 0x41, 0xA0, 0xAA, 0x63, 0xD6, 0xA4, 0x2B, 0x60, 0xB3, 0x43};
}
// 6B5BE490-194C-4E2E-B8DE-3BB15CC52777
template <>
constexpr type_id type_id_getter<double>() noexcept
{
    return {0x6B, 0x5B, 0xE4, 0x90, 0x19, 0x4C, 0x4E, 0x2E, 0xB8, 0xDE, 0x3B, 0xB1, 0x5C, 0xC5, 0x27, 0x77};
}
// 035E728E-8539-48BA-8D3F-FBCD4EEF0F16
template <>
constexpr type_id type_id_getter<long double>() noexcept
{
    return {0x03, 0x5E, 0x72, 0x8E, 0x85, 0x39, 0x48, 0xBA, 0x8D, 0x3F, 0xFB, 0xCD, 0x4E, 0xEF, 0x0F, 0x16};
}
// 7A9A6AB3-2259-48D4-B6B5-2648A0A7673A
template <>
constexpr type_id type_id_getter<std::nullptr_t>() noexcept
{
    return {0x7A, 0x9A, 0x6A, 0xB3, 0x22, 0x59, 0x48, 0xD4, 0xB6, 0xB5, 0x26, 0x48, 0xA0, 0xA7, 0x67, 0x3A};
}
// NOLINTEND(cppcoreguidelines-avoid-magic-numbers, readability-magic-numbers)
/// @}

/// The type ID value of the given type.
/// This helper is provided for regularity; it has the same value as \c T::_get_type_id_().
/// The type shall satisfy \ref cetl::has_type_id or have a \ref cetl::type_id_getter specialization.
/// CV-qualifiers and references are ignored, so \c type_id_value<const int&> equals \c type_id_value<int>.
template <typename T>
constexpr type_id type_id_value = type_id_getter<std::remove_cv_t<std::remove_reference_t<T>>>();

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
/// <tt>static constexpr cetl::type_id _get_type_id_() noexcept</tt> that
/// returns the type ID (i.e., the UUID) of the type it is defined on.
/// Types that provide said method satisfy \ref cetl::has_type_id.
///
/// If the type is polymorphic, it should either manually implement \ref cetl::rtti through \e virtual inheritance,
/// or use the \ref cetl::rtti_helper helper which will provide the necessary implementation of the RTTI-related
/// methods along with the aforementioned \c _get_type_id_() method. More about the latter option in its documentation.
///
/// \attention Use only virtual inheritance when deriving from this type.
///
/// RTTI-related members are named with surrounding underscores to avoid name clashes with user-defined members,
/// including the members of DSDL types (where identifiers surrounded with underscores are reserved).
/// The user code should not invoke such underscored methods directly but instead use the free functions defined here.
///
/// A basic usage example:
/// \code
/// class ICat final : public virtual cetl::rtti  // <-- use virtual inheritance when implementing cetl::rtti
/// {
/// public:
///     static constexpr cetl::type_id _get_type_id_() noexcept { return cetl::type_id_type_value<0x12>; }
///     void* _cast_(const cetl::type_id& id) & noexcept override
///     {
///         return (id == _get_type_id_()) ? static_cast<void*>(this) : nullptr;
///     }
///     const void* _cast_(const cetl::type_id& id) const& noexcept override
///     {
///         return (id == _get_type_id_()) ? static_cast<const void*>(this) : nullptr;
///     }
/// };
///
/// class Tabby final : public ICat
/// {
/// public:
///     static constexpr cetl::type_id _get_type_id_() noexcept { return cetl::type_id_type_value<0x34>; }
///     void* _cast_(const cetl::type_id& id) & noexcept override
///     {
///         if (id == _get_type_id_()) { return static_cast<void*>(this); }
///         return ICat::_cast_(id);
///     }
///     const void* _cast_(const cetl::type_id& id) const& noexcept override
///     {
///         if (id == _get_type_id_()) { return static_cast<const void*>(this); }
///         return ICat::_cast_(id);
///     }
/// };
///
/// void foo(ICat& some_cat)
/// {
///     // you can query the cat directly:
///     const is_tabby = cetl::is_instance_of<Tabby>(some_cat);
///     // or you can try converting it and see if it succeeds:
///     Tabby* maybe_tabby = cetl::rtti_cast<Tabby*>(&some_cat);
/// }
/// \endcode
///
/// A basic usage example with a class implementing multiple interfaces:
/// \code
/// // Assume that Tabby and Boxer are polymorphic types that implement the cetl::rtti interface.
/// class CatDog final : public Tabby, public Boxer
/// {
///  public:
///     void* _cast_(const type_id& id) & noexcept override
///     {
///         if (void* const p = Tabby::_cast_(id)) { return p; }
///         return Boxer::_cast_(id);
///     }
///     const void* _cast_(const type_id& id) const& noexcept override
///     {
///         if (const void* const p = Tabby::_cast_(id)) { return p; }
///         return Boxer::_cast_(id);
///     }
/// };
/// \endcode
///
/// A similar example with composition:
/// \code
/// class CatDog final : public virtual cetl::rtti
/// {
/// public:
///     void* _cast_(const type_id& id) & noexcept override
///     {
///         if (void* const p = m_cat._cast_(id)) { return p; }
///         return m_dog._cast_(id);
///     }
///     const void* _cast_(const type_id& id) const& noexcept override
///     {
///         if (const void* const p = m_cat._cast_(id)) { return p; }
///         return m_dog._cast_(id);
///     }
/// private:
///     // Assume that Tabby and Boxer are polymorphic types that implement the cetl::rtti interface.
///     Tabby m_cat{};
///     Boxer m_dog{};
/// };
/// \endcode
class rtti
{
public:
    /// Implementations can choose to use \ref cetl::rtti_helper instead of implementing this manually.
    /// If manual implementation is preferred, then the method body should be implemented as follows
    /// (add \c const in the const overload):
    ///
    /// @code
    /// return (id == _get_type_id_()) ? static_cast<void*>(this) : base::_cast_(id);
    /// @endcode
    ///
    /// Where \c base is the base class of the current class that implements this method, if any;
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
    CETL_NODISCARD virtual void*       _cast_(const type_id& id) & noexcept      = 0;
    CETL_NODISCARD virtual const void* _cast_(const type_id& id) const& noexcept = 0;
    /// @}

protected:
    ~rtti() noexcept = default;
};

/// Non-polymorphic types that want to support RTTI should simply provide a \c _get_type_id_()
/// method that returns \ref cetl::type_id; there is no need for them to use this helper.
///
/// Polymorphic types, on the other hand, are trickier because their runtime type may not be the same as their
/// static type; for this reason, they should either implement \ref cetl::rtti manually, or, alternatively,
/// publicly inherit from this helper class, which will provide the necessary implementation of the
/// RTTI-related methods along with the aforementioned \c _get_type_id_ method.
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
/// \tparam Bases An optional list of base class that implement the \ref cetl::rtti interface.
///         If this helper is used, base classes that implement the \ref cetl::rtti interface can only be inherited
///         via this helper, not directly. If this is for whatever reason not possible, the user should implement
///         the \ref cetl::rtti interface manually.
template <typename TypeIDType, typename... Bases>
struct rtti_helper : public virtual cetl::rtti, public Bases...
{
    /// The recommended implementation that simply returns the value of the \c TypeIDType template parameter.
    static constexpr type_id _get_type_id_() noexcept
    {
        return type_id_type_value<TypeIDType>();
    }
    /// The recommended implementation that performs an exhaustive search for a matching conversion
    /// throughout the entire type hierarchy tree in the presence of multiple inheritance.
    /// @{
    CETL_NODISCARD void* _cast_(const type_id& id) & noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<void*>(this) : search<Bases...>(id);
    }
    CETL_NODISCARD const void* _cast_(const cetl::type_id& id) const& noexcept override
    {
        return (id == _get_type_id_()) ? static_cast<const void*>(this) : search<Bases...>(id);
    }
    /// @}

    virtual ~rtti_helper() noexcept = default;

private:
    // Exhaustively search for a matching conversion throughout the entire type hierarchy tree.
    // Template parameter pack expansion is not available in C++14 so we do it the hard way.
    template <typename... E, typename = std::enable_if_t<sizeof...(E) == 0>>
    CETL_NODISCARD void* search(const type_id&) const noexcept
    {
        return nullptr;
    }
    template <typename Head, typename... Tail>
    void* search(const type_id& id) noexcept
    {
        if (void* const p = Head::_cast_(id))
        {
            return p;
        }
        return search<Tail...>(id);
    }
    template <typename Head, typename... Tail>
    CETL_NODISCARD const void* search(const type_id& id) const noexcept
    {
        if (const void* const p = Head::_cast_(id))
        {
            return p;
        }
        return search<Tail...>(id);
    }
};

/// Performs a safe dynamic type up-/down-conversion in constant time by invoking \ref cetl::rtti::_cast_.
/// \c T shall be a pointer and \c std::remove_pointer_t<T> shall satisfy \ref cetl::has_type_id.
/// Returns \c nullptr if a safe dynamic type conversion to \c T is not possible.
/// @{
// clang-format 19 and 22 lay this declaration out differently, and CI runs both; keep it as written.
// clang-format off
template <typename T, typename _from>
CETL_NODISCARD std::enable_if_t<is_rtti_convertible<_from> &&     //
                                    std::is_pointer<T>::value &&  //
                                    has_type_id<std::remove_pointer_t<T>>,
                                T>
               rtti_cast(_from* const obj) noexcept
// clang-format on
{
    return (obj == nullptr)  //
               ? nullptr
               : static_cast<T>(obj->_cast_(type_id_value<std::remove_pointer_t<T>>));
}
// clang-format off
template <typename T, typename _from>
CETL_NODISCARD std::enable_if_t<is_rtti_convertible<_from> &&     //
                                    std::is_pointer<T>::value &&  //
                                    has_type_id<std::remove_pointer_t<T>>,
                                const std::remove_pointer_t<T>*>
               rtti_cast(const _from* const obj) noexcept
// clang-format on
{
    return (obj == nullptr)  //
               ? nullptr
               : static_cast<const std::remove_pointer_t<T>*>(obj->_cast_(type_id_value<std::remove_pointer_t<T>>));
}
/// @}

namespace detail
{
// True iff the pointer conversion From -> To keeps the object's static type, up to added cv-qualifiers.
template <typename To, typename From>
using is_static_identity_cast = std::integral_constant<
    bool,
    std::is_same<std::remove_cv_t<std::remove_pointer_t<To>>, std::remove_cv_t<std::remove_pointer_t<From>>>::value &&
        std::is_convertible<From, To>::value>;

template <typename To, typename From>
CETL_NODISCARD To static_identity_cast(const From obj, std::true_type) noexcept
{
    return obj;
}
template <typename To, typename From>
CETL_NODISCARD To static_identity_cast(const From, std::false_type) noexcept
{
    return nullptr;
}
}  // namespace detail

/// Performs a safe type conversion for types that have a type ID but do not implement \ref cetl::rtti::_cast_,
/// such as non-polymorphic types and built-in types. Such types carry no runtime type information beyond their
/// static type, so the conversion succeeds iff \c std::remove_pointer_t<T> is the static type of the object,
/// ignoring cv-qualifiers (which can be added but not removed).
/// Matching type IDs are not sufficient because a derived type may inherit the \c _get_type_id_() of its base;
/// conversion to a base class is not supported.
/// Returns \c nullptr if the object pointer is null or the static types differ.
/// @{
// clang-format off
template <typename T, typename _from>
CETL_NODISCARD std::enable_if_t<!is_rtti_convertible<_from> &&    //
                                    has_type_id<_from> &&         //
                                    std::is_pointer<T>::value &&  //
                                    has_type_id<std::remove_pointer_t<T>>,
                                T>
               rtti_cast(_from* const obj) noexcept
// clang-format on
{
    return detail::static_identity_cast<T>(obj, detail::is_static_identity_cast<T, _from*>{});
}
// clang-format off
template <typename T, typename _from>
CETL_NODISCARD std::enable_if_t<!is_rtti_convertible<_from> &&    //
                                    has_type_id<_from> &&         //
                                    std::is_pointer<T>::value &&  //
                                    has_type_id<std::remove_pointer_t<T>>,
                                const std::remove_pointer_t<T>*>
               rtti_cast(const _from* const obj) noexcept
// clang-format on
{
    using to = const std::remove_pointer_t<T>*;
    return detail::static_identity_cast<to>(obj, detail::is_static_identity_cast<to, const _from*>{});
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
/// Detects whether the given object of a type that has a type ID but does not implement \ref cetl::rtti::_cast_
/// (such as a non-polymorphic or built-in type) is an instance of the type with the given type ID.
/// Such an object is an instance of its own static type only.
template <typename _u>
CETL_NODISCARD std::enable_if_t<!is_rtti_convertible<_u> && has_type_id<_u>, bool> is_instance_of(
    const _u&,
    const type_id& id) noexcept
{
    return id == type_id_value<_u>;
}
/// Detects whether the given object is an instance of the given type.
/// Q shall satisfy \ref cetl::has_type_id.
/// Refer to the non-template overloads for details.
template <typename Q, typename _u>
CETL_NODISCARD std::enable_if_t<is_rtti_convertible<_u> || has_type_id<_u>, bool> is_instance_of(const _u& obj) noexcept
{
    return is_instance_of(obj, type_id_value<Q>);
}

}  // namespace cetl

#endif  // CETL_RTTI_HPP_INCLUDED
