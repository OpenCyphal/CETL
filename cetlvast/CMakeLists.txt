#
# Copyright (C) 2014 Pavel Kirienko <pavel@opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

###############################################################################
# DON'T USE CMAKE!
#
#     ...directly that is. We provide a verify.py script that wraps up a bunch
# of conventions and environment variables that you'd otherwise need to know.
# Use it to invoke cmake or at least use it to tell you how to invoke cmake.
# We also provide full vscode cmake integration that has the same knowledge
# of cetlvast that the verify.py script has.
###############################################################################

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(cetlvast C CXX)

if(NOT DEFINED CMAKE_MESSAGE_LOG_LEVEL)
    set(CMAKE_MESSAGE_LOG_LEVEL STATUS)
endif()

if (NOT DEFINED CYPHAL_PROJECT_VERSION)
    message(WARNING "CYPHAL_PROJECT_VERSION was not set. Setting CYPHAL_VERSION to default of 0.0.0.")
    set(CYPHAL_VERSION "0.0.0")
else()
    set(CYPHAL_VERSION ${CYPHAL_PROJECT_VERSION})
endif()

add_compile_definitions("CYPHAL_VERSION=\"${CYPHAL_VERSION}\"")

if (NOT DEFINED CETL_ROOT)
    set(LOCAL_PARENT_OF_SOURCE_PATH "${CMAKE_SOURCE_DIR}/../")
    cmake_path(ABSOLUTE_PATH LOCAL_PARENT_OF_SOURCE_PATH
               OUTPUT_VARIABLE CETL_ROOT
               BASE_DIRECTORY "${CMAKE_BINARY_DIR}")
    cmake_path(NORMAL_PATH CETL_ROOT OUTPUT_VARIABLE CETL_ROOT)
    message(DEBUG "Setting CETL_ROOT = ${CETL_ROOT}")
else()
    message(DEBUG "Using ${CETL_ROOT} for CETL_ROOT")
endif()

include_directories(
    "${CMAKE_SOURCE_DIR}/include"
)

if(NOT DEFINED FETCHCONTENT_FULLY_DISCONNECTED)
    message(WARNING "FETCHCONTENT_FULLY_DISCONNECTED was not defined. Setting to \"OFF\" as a default but the build "
                    "will fail if you haven't previously downloaded the external dependencies.")
    set(FETCHCONTENT_FULLY_DISCONNECTED OFF)
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
    CMAKE_MODULE_PATH:               ${CMAKE_MODULE_PATH}\n\
    CMAKE_SOURCE_DIR:                ${CMAKE_SOURCE_DIR}\n\
    CMAKE_CURRENT_BINARY_DIR:        ${CMAKE_CURRENT_BINARY_DIR}\n\
    CMAKE_MESSAGE_LOG_LEVEL:         ${CMAKE_MESSAGE_LOG_LEVEL}\n\
    CETLVAST_EXTERNAL_ROOT:          ${CETLVAST_EXTERNAL_ROOT}\n\
    FETCHCONTENT_FULLY_DISCONNECTED: ${FETCHCONTENT_FULLY_DISCONNECTED}\n\
-----------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | OPTIONS/VARIANTS
# +---------------------------------------------------------------------------+

option(CETL_ENABLE_DEBUG_ASSERT "Enable or disable runtime asserts in CETL code." OFF)
option(CETLVAST_STYLE_CHECK "Enable or disable a check for violations of the project's style standards." ON)

set(CETLVAST_CPP_STANDARD "20" CACHE STRING "C++ standard to use when compiling.")
set_property(CACHE CETLVAST_CPP_STANDARD PROPERTY STRINGS 14 17 20)

set(CMAKE_CXX_STANDARD ${CETLVAST_CPP_STANDARD})

set(CMAKE_BUILD_TYPE "Debug" CACHE STRING "Build type.")
set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Release Debug)

set(CETLVAST_COVERAGE_REPORT_FORMAT "html" CACHE STRING "Coverage reporting standard to use when building coverage reports.")
set_property(CACHE CETLVAST_COVERAGE_REPORT_FORMAT PROPERTY STRINGS "html" "sonarqube" "json")

option(CETLVAST_DISABLE_CPP_EXCEPTIONS "Attempt to disable C++ exceptions for the current toolchain." OFF)

if (CETL_ENABLE_DEBUG_ASSERT)
add_compile_definitions("CETL_ENABLE_DEBUG_ASSERT=1")
endif()

if (LIBCXX_ENABLE_ASSERTIONS)
add_compile_definitions("LIBCXX_ENABLE_ASSERTIONS=1")
endif()

message(STATUS "[ Build Options ]--------------------------------------\n\
    CMAKE_BUILD_TYPE:                ${CMAKE_BUILD_TYPE}\n\
    CETL_ENABLE_DEBUG_ASSERT:        ${CETL_ENABLE_DEBUG_ASSERT}\n\
    CETLVAST_COVERAGE_REPORT_FORMAT: ${CETLVAST_COVERAGE_REPORT_FORMAT}\n\
    CETLVAST_STYLE_CHECK:            ${CETLVAST_STYLE_CHECK}\n\
    CETLVAST_CPP_STANDARD:           ${CETLVAST_CPP_STANDARD}\n\
    CETLVAST_DISABLE_CPP_EXCEPTIONS: ${CETLVAST_DISABLE_CPP_EXCEPTIONS}\n\
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

message(STATUS "[ Build Kit ]-------------------------------------------\n\
    CMAKE_TOOLCHAIN_FILE:           ${CMAKE_TOOLCHAIN_FILE}\n\
    CETLVAST_FLAG_SET:              ${CETLVAST_FLAG_SET}\n\
    CMAKE_CXX_COMPILER_[ID-VER]:    ${CMAKE_CXX_COMPILER_ID}-${CMAKE_CXX_COMPILER_VERSION}\n\
    CMAKE_C_COMPILER_[ID-VER]:      ${CMAKE_CXX_COMPILER_ID}-${CMAKE_C_COMPILER_VERSION}\n\
    CMAKE_AR:                       ${CMAKE_AR}\n\
    CMAKE_LINKER:                   ${CMAKE_LINKER}\n\
    CMAKE_RANLIB:                   ${CMAKE_RANLIB}\n\
    CMAKE_[lang]_PLATFORM_ID:       ${CMAKE_CXX_PLATFORM_ID}\n\
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
find_package(clangformat REQUIRED)
cmake_path(APPEND CETL_ROOT "include" OUTPUT_VARIABLE CETL_INCLUDE)

add_library(cetl INTERFACE)
target_include_directories(cetl INTERFACE ${CETL_INCLUDE})

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

# +---------------------------------------------------------------------------+
# | TEST SUITES
# +---------------------------------------------------------------------------+

add_subdirectory(${CMAKE_SOURCE_DIR}/suites/unittest)
add_subdirectory(${CMAKE_SOURCE_DIR}/suites/docs)
add_subdirectory(${CMAKE_SOURCE_DIR}/suites/compile)

# +---------------------------------------------------------------------------+
# | BUILD TARGET ALIASES
# +---------------------------------------------------------------------------+
add_custom_target(
    lint
    DEPENDS
        ${LOCAL_FORMAT_CHECK_TARGET_NAME}
)

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

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    add_custom_target(
        unittest
        DEPENDS
            run_unittests
            gcovr_sonarqube_report_for_unittest
            gcovr_html_report_for_unittest
            run_examples
            gcovr_sonarqube_report_for_examples
            gcovr_html_report_for_examples
            run_compile_precheck_tests
    )
else()
    add_custom_target(
        unittest
        DEPENDS
            run_unittests
            run_examples
            run_compile_precheck_tests
    )
endif()

add_custom_target(
    release
    DEPENDS
        build
        unittest
        docs
)
