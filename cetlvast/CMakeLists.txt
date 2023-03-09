#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 3.16.0)

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(cetlvast CXX)

if (NOT DEFINED CETLVAST_PROJECT_ROOT)
    get_filename_component(CETLVAST_PROJECT_ROOT
                           "${CMAKE_SOURCE_DIR}/../"
                           REALPATH BASE_DIR
                           "${CMAKE_BINARY_DIR}")
    message(STATUS "Setting CETLVAST_PROJECT_ROOT = ${CETLVAST_PROJECT_ROOT}")
else()
    message(STATUS "Using ${CETLVAST_PROJECT_ROOT} for CETLVAST_PROJECT_ROOT")
endif()

if (NOT DEFINED CETLVAST_EXT_FOLDER)
    get_filename_component(CETLVAST_BUILD_DIR_STUB ${CMAKE_CURRENT_BINARY_DIR} NAME)
    set(EXTERNAL_PROJECT_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/${CETLVAST_BUILD_DIR_STUB}_ext")
else()
    set(EXTERNAL_PROJECT_DIRECTORY "${CETLVAST_EXT_FOLDER}")
endif()

message(STATUS "Ensuring EXTERNAL_PROJECT_DIRECTORY=${EXTERNAL_PROJECT_DIRECTORY}")

#
# Make sure we have our external project directory created. We use this
# to share external project artifacts with different build flavors.
#
file(MAKE_DIRECTORY ${EXTERNAL_PROJECT_DIRECTORY})

set(CETL_INCLUDE "${CETLVAST_PROJECT_ROOT}/include")

include_directories(
    ${CETL_INCLUDE}
    "${CETLVAST_PROJECT_ROOT}/cetlvast/include"
)

if(DEFINED CETLVAST_CMAKE_MODULE_PATH)
    message(STATUS "CETLVAST_CMAKE_MODULE_PATH was set to ${CETLVAST_CMAKE_MODULE_PATH}. Will use this to override CMAKE_MODULE_PATH.")
    set(CMAKE_MODULE_PATH ${CETLVAST_CMAKE_MODULE_PATH})
else()
    #
    # Tell cmake where to find our custom scripts.
    #
    set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake/modules")
    message(STATUS "CMAKE_MODULE_PATH set to ${CMAKE_MODULE_PATH}")
endif()

# +---------------------------------------------------------------------------+
# | OPTIONS/VARIANTS
# +---------------------------------------------------------------------------+

option(CETL_ENABLE_DEBUG_ASSERT "Enable or disable runtime asserts in CETL code." OFF)
option(CETLVAST_ENABLE_COVERAGE "Enable or disable coverage data instrumentation in test builds." OFF)
option(CETLVAST_STYLE_CHECK "Enable or disable a check for violations of the project's style standards." ON)

set(CETLVAST_CPP_STANDARD "20" CACHE STRING "C++ standard to use when compiling.")
set_property(CACHE CETLVAST_CPP_STANDARD PROPERTY STRINGS 14 17 20 23)

message(STATUS "[ Build Options ]--------------------------------------\n\
    CMAKE_BUILD_TYPE:         ${CMAKE_BUILD_TYPE}\n\
    CETL_ENABLE_DEBUG_ASSERT: ${CETL_ENABLE_DEBUG_ASSERT}\n\
    CETLVAST_ENABLE_COVERAGE: ${CETLVAST_ENABLE_COVERAGE}\n\
    CETLVAST_STYLE_CHECK:     ${CETLVAST_STYLE_CHECK}\n\
    CETLVAST_CPP_STANDARD:    ${CETLVAST_CPP_STANDARD}\n\
-----------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | KITS
# +---------------------------------------------------------------------------+

if(NOT DEFINED CETLVAST_FLAG_SET)
    message(VERBOSE "CETLVAST_FLAG_SET was not defined. Setting to \"native-full\" as a default")
    set(CETLVAST_FLAG_SET ${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/native-full.cmake)
else()
    set(CETLVAST_FLAG_SET ${CETLVAST_FLAG_SET})
endif()

if(NOT DEFINED CETLVAST_TEST_SUITE)
    message(VERBOSE "CETLVAST_TEST_SUITE was not defined. Setting to \"unittest\" as a default")
    set(CETLVAST_TEST_SUITE ${CMAKE_SOURCE_DIR}/cmake/suites/unittest.cmake)
else()
    set(CETLVAST_TEST_SUITE ${CETLVAST_TEST_SUITE})
endif()

message(STATUS "[ Build Kit ]-------------------------------------------\n\
    CMAKE_TOOLCHAIN_FILE:     ${CMAKE_TOOLCHAIN_FILE}\n\
    CETLVAST_FLAG_SET:        ${CETLVAST_FLAG_SET}\n\
    CETLVAST_TEST_SUITE:      ${CETLVAST_TEST_SUITE}\n\
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
# | BUILD SELECTED TEST SUITE
# +---------------------------------------------------------------------------+
include(${CETLVAST_TEST_SUITE})
