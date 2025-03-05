#
# Copyright (C) 2014 Pavel Kirienko <pavel@opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(cetlvast C CXX)

if(NOT DEFINED CMAKE_MESSAGE_LOG_LEVEL)
    set(CMAKE_MESSAGE_LOG_LEVEL STATUS)
endif()

if (NOT DEFINED CETL_PROJECT_VERSION)
    message(WARNING "CETL_PROJECT_VERSION was not set. Setting CETL_VERSION to default of 0.0.0.")
    set(CETL_VERSION "0.0.0")
else()
    set(CETL_VERSION ${CETL_PROJECT_VERSION})
endif()

add_compile_definitions("CETL_VERSION=\"${CETL_VERSION}\"")

if (NOT DEFINED CETL_ROOT)
    cmake_path(GET CMAKE_SOURCE_DIR PARENT_PATH CETL_ROOT)
    message(DEBUG "Setting CETL_ROOT = ${CETL_ROOT}")
else()
    message(DEBUG "Using ${CETL_ROOT} for CETL_ROOT")
endif()

if(DEFINED CETLVAST_CMAKE_MODULE_PATH)
    message(DEBUG "CETLVAST_CMAKE_MODULE_PATH was set to ${CETLVAST_CMAKE_MODULE_PATH}. Will use this to override CMAKE_MODULE_PATH.")
    set(CMAKE_MODULE_PATH ${CETLVAST_CMAKE_MODULE_PATH})
else()
    #
    # Tell cmake where to find our custom scripts.
    #
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
    message(DEBUG "CMAKE_MODULE_PATH set to ${CMAKE_MODULE_PATH}")
endif()

set(CETLVAST_EXTERNAL_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/build_external")

message(STATUS
"[ globals ]-----------------------------------------------\n\
    CETL_ROOT:                       ${CETL_ROOT}\n\
    CETL_VERSION:                    ${CETL_VERSION}\n\
    CMAKE_GENERATOR:                 ${CMAKE_GENERATOR}\n\
    CMAKE_MODULE_PATH:               ${CMAKE_MODULE_PATH}\n\
    CMAKE_SOURCE_DIR:                ${CMAKE_SOURCE_DIR}\n\
    CMAKE_CURRENT_BINARY_DIR:        ${CMAKE_CURRENT_BINARY_DIR}\n\
    CMAKE_MESSAGE_LOG_LEVEL:         ${CMAKE_MESSAGE_LOG_LEVEL}\n\
    CMAKE_INSTALL_PREFIX:            ${CMAKE_INSTALL_PREFIX}\n\
    CETLVAST_EXTERNAL_ROOT:          ${CETLVAST_EXTERNAL_ROOT}\n\
    FETCHCONTENT_FULLY_DISCONNECTED: ${FETCHCONTENT_FULLY_DISCONNECTED}\n\
    CMAKE_SYSTEM_NAME:               ${CMAKE_SYSTEM_NAME}\n\
    CMAKE_HOST_SYSTEM_PROCESSOR:     ${CMAKE_HOST_SYSTEM_PROCESSOR}\n\
    CMAKE_CROSSCOMPILING:            ${CMAKE_CROSSCOMPILING}\n\
    CMAKE_SYSTEM_PROCESSOR:          ${CMAKE_SYSTEM_PROCESSOR}\n\
-----------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | OPTIONS/VARIANTS
# +---------------------------------------------------------------------------+

set(CETLVAST_CPP_STANDARD "20" CACHE STRING "C++ standard to use when compiling.")
set_property(CACHE CETLVAST_CPP_STANDARD PROPERTY STRINGS 14 17 20)

set(CMAKE_CXX_STANDARD ${CETLVAST_CPP_STANDARD})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions("CETL_ENABLE_DEBUG_ASSERT=$<IF:$<OR:$<CONFIG:Release>,$<CONFIG:ReleaseEP>>,0,1>")

# See https://libcxx.llvm.org/Hardening.html
add_compile_definitions(
    $<$<CONFIG:Release>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_FAST>
    $<$<CONFIG:ReleaseEP>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_NONE>
    $<$<CONFIG:DebugEP>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_EXTENSIVE>
    $<$<CONFIG:Debug>:_LIBCPP_HARDENING_MODE=_LIBCPP_HARDENING_MODE_DEBUG>
)

message(STATUS "[ Build Options ]--------------------------------------\n\
    CETLVAST_CPP_STANDARD:           ${CETLVAST_CPP_STANDARD}\n\
    CETLVAST_ENABLE_BENCHMARKS       ${CETLVAST_ENABLE_BENCHMARKS}\n\
-----------------------------------------------------------\n\
")

# Always need this.
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# +---------------------------------------------------------------------------+
# | KITS
# +---------------------------------------------------------------------------+

if(NOT DEFINED CETLVAST_FLAG_SET)
    message(VERBOSE "CETLVAST_FLAG_SET was not defined. Setting to \"default\" as a default")
    set(CETLVAST_FLAG_SET ${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/default.cmake)
else()
    set(CETLVAST_FLAG_SET ${CETLVAST_FLAG_SET})
endif()

if(CMAKE_CXX_FLAGS MATCHES "-stdlib=libc\\+\\+")
    set(LOCAL_CXX_STANDARD_LIBRARY "-stdlib=libc++")
elseif(COMPILE_FLAGS MATCHES "-stdlib=libstdc\\+\\+")
    set(LOCAL_CXX_STANDARD_LIBRARY "-stdlib=libstdc++")
else()
    set(LOCAL_CXX_STANDARD_LIBRARY "(default)")
endif()

message(STATUS "[ Build Kit ]-------------------------------------------\n\
    CMAKE_TOOLCHAIN_FILE:           ${CMAKE_TOOLCHAIN_FILE}\n\
    CETLVAST_FLAG_SET:              ${CETLVAST_FLAG_SET}\n\
    CMAKE_CXX_COMPILER_[ID-VER]:    ${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}\n\
    CMAKE_C_COMPILER_[ID-VER]:      ${CMAKE_CXX_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}\n\
    CMAKE_AR:                       ${CMAKE_AR}\n\
    CMAKE_LINKER:                   ${CMAKE_LINKER}\n\
    CMAKE_RANLIB:                   ${CMAKE_RANLIB}\n\
    CMAKE_[lang]_PLATFORM_ID:       ${CMAKE_CXX_PLATFORM_ID}\n\
    standard library:               ${LOCAL_CXX_STANDARD_LIBRARY}\n\
-----------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | FLAG SETS
# +---------------------------------------------------------------------------+
#
# Load all our compile flag sets into the appropriate places.
#
include(${CETLVAST_FLAG_SET})

# +---------------------------------------------------------------------------+
# | CETL
# +---------------------------------------------------------------------------+
cmake_path(APPEND CETL_ROOT "include" OUTPUT_VARIABLE CETL_INCLUDE)

include(ProjectLibrary)

add_project_library(
    NAME cetl
    HEADER_PATH
        ${CETL_INCLUDE}/
    HEADER_INCLUDE_PATTERNS
        *.hpp
)

find_package(clangformat)
if(${CETLVAST_ENABLE_BENCHMARKS})
find_package(benchmark)
endif()

if(clangformat_FOUND)
    # define a dry-run version that we always run.
    enable_clang_format_check_for_directory(
        DIRECTORY ${CETL_INCLUDE}
        GLOB_PATTERN "**/*.hpp"
        ADD_TO_ALL
        OUT_TARGET_NAME LOCAL_FORMAT_CHECK_TARGET_NAME)

    # provide an in-place format version as a helper that must be manually run.
    enable_clang_format_check_for_directory(
        DIRECTORY ${CETL_INCLUDE}
        GLOB_PATTERN "**/*.hpp"
        FORMAT_IN_PLACE)
endif()
# +---------------------------------------------------------------------------+
# | CETLVAST
# +---------------------------------------------------------------------------+
add_library(cetlvast STATIC ${CMAKE_SOURCE_DIR}/src/cetlvast.cpp)
target_include_directories(cetlvast INTERFACE ${CMAKE_SOURCE_DIR}/include)
target_link_libraries(cetlvast PUBLIC cetl gmock gtest)
target_compile_definitions(cetlvast
    PUBLIC
        $<$<CONFIG:Coverage>:CETLVAST_COVERAGE>
)

# +---------------------------------------------------------------------------+
# | TEST SUITES
# +---------------------------------------------------------------------------+

add_subdirectory(${CMAKE_SOURCE_DIR}/suites/unittest)
add_subdirectory(${CMAKE_SOURCE_DIR}/suites/docs)
add_subdirectory(${CMAKE_SOURCE_DIR}/suites/compile)
if(benchmark_FOUND)
    add_subdirectory(${CMAKE_SOURCE_DIR}/suites/benchmark)
endif()
# +---------------------------------------------------------------------------+
# | BUILD TARGET ALIASES
# +---------------------------------------------------------------------------+
if(DEFINED LOCAL_FORMAT_CHECK_TARGET_NAME)
    add_custom_target(
        lint
        DEPENDS
            ${LOCAL_FORMAT_CHECK_TARGET_NAME}
    )
else()
    # no-op since there was no format target
    add_custom_target(
        lint
    )
endif()

add_custom_target(
    docs
    DEPENDS
        lint
        generate_CETL_docs
)

add_custom_target(
    build
    DEPENDS
        build_unittests
        build_examples
        build_compile_tests
)

add_custom_target(
    unittest
    DEPENDS
        run_unittests
        $<$<CONFIG:Coverage>:gcovr_sonarqube_report_for_unittest>
        $<$<CONFIG:Coverage>:gcovr_html_report_for_unittest>
        run_examples
        $<$<CONFIG:Coverage>:gcovr_sonarqube_report_for_examples>
        $<$<CONFIG:Coverage>:gcovr_html_report_for_examples>
        run_compile_precheck_tests
)

# "release" is a legacy target supported for older build systems that use this
# as a default. It actually doesn't build the docs which is part of a CETL
# release.
add_custom_target(
    release
    DEPENDS
        build
        unittest
)
