#
# Copyright (C) 2014 Pavel Kirienko <pavel.kirienko@gmail.com>
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#

cmake_minimum_required(VERSION 3.16.0)

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

if(NOT DEFINED CETLVAST_FLAG_SET)
    message(STATUS "CETLVAST_FLAG_SET was not defined. Setting to \"native-full\" as a default")
    set(CETLVAST_FLAG_SET ${CMAKE_SOURCE_DIR}/cmake/compiler_flag_sets/native-full.cmake)
else()
    set(CETLVAST_FLAG_SET ${CETLVAST_FLAG_SET})
endif()

if(NOT DEFINED CETLVAST_TESTBUILD)
    message(STATUS "CETLVAST_TESTBUILD was not defined. Setting to \"native tests\" as a default")
    set(CETLVAST_TESTBUILD ${CMAKE_SOURCE_DIR}/cmake/suites/native.cmake)
else()
    set(CETLVAST_TESTBUILD ${CETLVAST_TESTBUILD})
endif()

if(NOT DEFINED CETL_ENABLE_DEBUG_ASSERT)
    message(STATUS "CETL_ENABLE_DEBUG_ASSERT was not defined. Setting to off as a default")
    set(CETL_ENABLE_DEBUG_ASSERT OFF)
endif()

if(NOT DEFINED CETLVAST_ENABLE_COVERAGE)
    message(STATUS "CETLVAST_ENABLE_COVERAGE was not defined. Coverage will not be enabled.")
    set(CETLVAST_ENABLE_COVERAGE OFF)
endif()

if(NOT DEFINED CETLVAST_STYLE_CHECK)
    message(STATUS "CETLVAST_STYLE_CHECK was not defined. Style will always be checked. Set to OFF to disable.")
    set(CETLVAST_STYLE_CHECK ON)
endif()

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

if(DEFINED CETLVAST_CPP_STANDARD)
    message(STATUS "CETLVAST_CPP_STANDARD was set to C++${CETLVAST_CPP_STANDARD} by the CETLVAST_CPP_STANDARD variable.")
else()
    #
    # Tell cmake where to find our custom scripts.
    #
    set(CETLVAST_CPP_STANDARD "20")
    message(STATUS "CETLVAST_CPP_STANDARD set to default: C++${CETLVAST_CPP_STANDARD}")
endif()


# +---------------------------------------------------------------------------+
# | FLAG SETS
# +---------------------------------------------------------------------------+
#
# Load all our compile flag sets into the appropriate places.
#
include(${CETLVAST_FLAG_SET})

# +---------------------------------------------------------------------------+
# | EXTERNAL DEPENDENCIES
# +---------------------------------------------------------------------------+

#
# googletest (and googlemock) external project.
#
find_package(gtest REQUIRED)

#
# Finds programs needed to build the CETL documentation
#
find_package(docs REQUIRED)

#
# We require clang-format to check the style as part of CI builds.
#
find_package(clangformat REQUIRED)

# +---------------------------------------------------------------------------+
# | STYLE
# +---------------------------------------------------------------------------+
create_check_style_target(format-check ${CETLVAST_STYLE_CHECK} "${CETL_INCLUDE}/**/*.h")

# +---------------------------------------------------------------------------+
# | BUILD UNIT TESTS
# +---------------------------------------------------------------------------+
include(${CETLVAST_TESTBUILD})

# +---------------------------------------------------------------------------+
# | DOCUMENTATION GENERATION
# +---------------------------------------------------------------------------+
if(NOT DEFINED CETLVAST_SKIP_DOCS OR NOT CETLVAST_SKIP_DOCS)
    create_docs_target(docs OFF)
else()
    message(STATUS "CETLVAST_SKIP_DOCS is ON. Skipping documentation generation for this build.")
endif()
