/// @file
/// CETL common header.
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///
/// @note
/// Keep this very spare. CETL's desire is to adapt to future C++ standards and too many CETL-specific definitions makes
/// it difficult for users to switch off of CETL in the future.
///
/// @def CETL_H_ERASE
/// If `CETL_H_ERASE` is defined then all CETL types will exclude all cetl headers which removes all common
/// dependencies, other than C++ standard headers, from CETL. The types will not build due to
/// missing macros and/or type aliases but the user can re-define these based on subsequent compiler errors. This allows
/// elision of cetl.hpp and dependencies on CETL polyfill types without modifying CETL source code.
/// Note that CETL polyfill headers cannot be used if CETL_H_ERASE is defined.
///
/// @mainpage Example Code
/// This area contains a series of examples used to illustrate CETL type usage. Each full example is actually
/// a set of googletest test cases to allow our build automation to verify their correctness but you can treat
/// any given example as a stand-alone program.
/// - @subpage example_01_polyfill_20
/// - @subpage example_02_span
/// - @subpage example_03_memory_resource
/// - @subpage example_04_buffer_memory_resource
/// - @subpage example_05_array_memory_resource
/// - @subpage example_06_memory_resource_deleter
/// - @subpage example_07_polymorphic_alloc_deleter
/// - @subpage example_08_variable_length_array_vs_vector
/// - @subpage example_09_variant
/// - @subpage example_10_unbounded_variant
///
/// @page example_01_polyfill_20 Example 1: CETL C++20 Polyfill Header
/// Full example for @ref cetl/pf20/cetlpf.hpp
/// @include example_01_polyfill_20.cpp
///
/// @page example_02_span Example 2: CETL span type.
/// Full example for cetl::pf20::span
/// @include example_02_span.cpp
///
/// @page example_03_memory_resource Example 3: Implementing a CETL Memory Resource
/// Full example for cetl::pf17::pmr::memory_resource
/// @include example_03_memory_resource.cpp
///
/// @page example_04_buffer_memory_resource Example 4: Using the UnsynchronizedBufferMemoryResource class
/// Full example for cetl::pmr::UnsynchronizedBufferMemoryResourceDelegate
/// @include example_04_buffer_memory_resource.cpp
/// Also see @ref example_05_array_memory_resource
///
/// @page example_05_array_memory_resource Example 5: Using the UnsynchronizedBArrayMemoryResource class
/// Also see @ref example_04_buffer_memory_resource
/// @include example_05_array_memory_resource.cpp
///
/// @page example_06_memory_resource_deleter Example 6: Using the MemoryResourceDeleter class
/// Full example for cetl::pmr::MemoryResourceDeleter
/// @include example_06_memory_resource_deleter.cpp
///
/// @page example_07_polymorphic_alloc_deleter Example 7: Using the PolymorphicAllocatorDeleter class
/// Full example for cetl::pmr::Factory
/// @include example_07_polymorphic_alloc_deleter.cpp
///
/// @page example_08_variable_length_array_vs_vector Example 8: Comparing std::vector to CETL's VariableLengthArray
/// Full example for cetl::VariableLengthArray
/// @include example_08_variable_length_array_vs_vector.cpp
///
/// @page example_09_variant Example 9: Using CETL's variant
/// Full example for cetl::variant
/// @include example_09_variant.cpp
///
/// @page example_10_unbounded_variant Example 10: Using CETL's unbounded_variant
/// Full example for cetl::unbounded_variant
/// @include example_10_unbounded_variant.cpp
///

#ifndef CETL_H_INCLUDED
#define CETL_H_INCLUDED

#ifdef CETL_H_ERASE
#    error "CETL_H_ERASE was defined. This header should never be included when the build is trying to erase it!"
#elif defined(CETL_DOXYGEN)
// Define then undefine to expose CETL_H_ERASE to doxygen.
#    define CETL_H_ERASE
#    undef CETL_H_ERASE
#endif

/// @defgroup CETL_VERSION The semantic version number of the CETL library.
/// These macros are an AUTOSAR-14 Rule A16-0-1 violation but we feel it necessary to provide them.
/// @{

/// @def CETL_VERSION_PATCH
/// CETL Patch version.
/// Patch versions shall always be backwards compatible with the same major
/// and minor version. A patch version number change will only occur if library source code is changed.
/// Documentation or test suite changes will not require a change to `cetl/cetl.hpp` and will not bump
/// the patch version.
#define CETL_VERSION_PATCH 0

/// @def CETL_VERSION_MINOR
/// CETL minor version.
/// Minor versions shall only add to CETL or modify it in a backwards compatible way.
#define CETL_VERSION_MINOR 0

/// @def CETL_VERSION_MAJOR
/// CETL Major version.
/// New major versions shall be rare. No overarching guarantees are made about compatibility
/// between major versions.
#define CETL_VERSION_MAJOR 0

/// @}

/// @def CETL_DEBUG_ASSERT
/// When `CETL_ENABLE_DEBUG_ASSERT` is defined and not 0 then this is redirected to
/// assert as included from `<cassert>`. Because assert does not support a failure message
/// we embed the `m` argument in a comma operator statement such that the compiler places the
/// both the text and the failure clause `c` in the text section of the binary which is normally
/// shown from standard error when an assert occurs.
///
/// When `CETL_ENABLE_DEBUG_ASSERT` is <em>not</em> defined or is 0 then these statements have no
/// effect and should not effect the resulting binary.
///
/// @warning
/// Define `CETL_ENABLE_DEBUG_ASSERT` as 1 to enable assertions within CETL code. Enabling this
/// in production code is <em>strongly</em> discouraged.
///
#if defined NDEBUG && defined CETL_ENABLE_DEBUG_ASSERT
#    undef CETL_ENABLE_DEBUG_ASSERT
#endif

#if defined CETL_ENABLE_DEBUG_ASSERT
#    include <cassert>
#    define CETL_DEBUG_ASSERT(c, m) assert(((void) (m), (c)))
#else
#    define CETL_DEBUG_ASSERT(c, m) ((void) (m))
#endif  // CETL_ENABLE_DEBUG_ASSERT

// Make the standard exceptions available only if exceptions are enabled.
#if defined(__cpp_exceptions)
#    include <stdexcept>
#endif

/// @defgroup CETL_CPP_STANDARD Guaranteed CETL c++ standard numbers
/// These macros are an AUTOSAR-14 Rule A16-0-1 violation but can be used to conditionally include headers which
/// is compliant with A16-0-1. The values were obtained by observation of compiler output using
/// [godbolt](https://godbolt.org/z/Thsn8qf1a) and as predicted by
/// [cppreference.com](https://en.cppreference.com/w/cpp/preprocessor/replace#Predefined_macros).
///
/// @note
/// Some CETL types don't use these values directly to reduce the number of explicit dependencies on cetl.hpp but by
/// including `cetl/cetl.hpp` these types inherit the static assertions that the only valid values of `__cplusplus`
/// found are one of the the list found in this group or a value greater than the target support `CETL_CPP_STANDARD_20`
/// value.
///
/// @{

/// @def CETL_CPP_STANDARD_14
/// Provides the proper value to test against `__cplusplus` for c++14.
/// ```
/// #if __cplusplus >= CETL_CPP_STANDARD_14
/// #include <something_from_cpp_14>
/// #endif
/// ```
#define CETL_CPP_STANDARD_14 201402L

/// @def CETL_CPP_STANDARD_17
/// Provides the proper value to test against `__cplusplus` for c++14.
/// ```
/// #if __cplusplus >= CETL_CPP_STANDARD_17
/// #include <something_from_cpp_14>
/// #include <something_from_cpp_17>
/// #endif
/// ```
#define CETL_CPP_STANDARD_17 201703L

/// @def CETL_CPP_STANDARD_20
/// Provides the proper value to test against `__cplusplus` for c++14.
/// ```
/// #if __cplusplus >= CETL_CPP_STANDARD_17
/// #include <something_from_cpp_14>
/// #include <something_from_cpp_17>
/// #include <something_from_cpp_20>
/// #endif
/// ```
#define CETL_CPP_STANDARD_20 202002L

/// @}

// Ensure base support.
static_assert(__cplusplus >= CETL_CPP_STANDARD_14,
              "Unsupported language: ISO C14, C++14, or a newer version of either is required to use this type.");

// Detect weird versions
static_assert((__cplusplus == CETL_CPP_STANDARD_14 || __cplusplus == CETL_CPP_STANDARD_17 ||
               __cplusplus >= CETL_CPP_STANDARD_20),
              "Unknown __cplusplus value found?");

/// CETL_NODISCARD
#ifndef CETL_NODISCARD
#    if (__cplusplus >= CETL_CPP_STANDARD_17) || defined(CETL_DOXYGEN)
/// A compatibility macros that expands to \c [[nodiscard]] if C++17 or later is used, otherwise it expands to
/// a compiler-specific alternative if one is known, otherwise it expands to nothing.
#        define CETL_NODISCARD [[nodiscard]]
#    else
#        if defined(__GNUC__) || defined(__clang__)
#            define CETL_NODISCARD __attribute__((warn_unused_result))
#        else
#            define CETL_NODISCARD
#        endif
#    endif
#endif

/// @namespace cetl This namespace contains types specific to CETL and nested namespaces that contain types adhering
///                 to target C++ specifications.
/// @namespace cetl::pmr CETL extensions to the standard Polymorphic Memory Resource (PMR) namespace, `std::pmr`.
#endif  // CETL_H_INCLUDED
