#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(cetlvast_docs_examples C CXX)

find_package(googletest REQUIRED)
find_package(o1heap REQUIRED)
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
find_package(gcovr REQUIRED)
endif()

# +---------------------------------------------------------------------------+
# | BUILD NATIVE EXAMPLE CODE
# +---------------------------------------------------------------------------+

file(GLOB NATIVE_EXAMPLES
    LIST_DIRECTORIES false
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
        example_*.cpp
)

set(ALL_EXAMPLES "")
set(ALL_EXAMPLE_RUNS "")

foreach(NATIVE_EXAMPLE ${NATIVE_EXAMPLES})
    define_native_gtest_unittest_targets(
        TEST_SOURCE ${NATIVE_EXAMPLE}
        EXTRA_TEST_LIBS cetl o1heap
        LINK_TO_MAIN
        OUT_TEST_LIB_VARIABLE LOCAL_TEST_LIB
        OUT_TEST_EXE_VARIABLE LOCAL_TEST_TARGET
        OUT_TEST_REPORT_VARIABLE LOCAL_TEST_REPORT
    )
    if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
        define_gcovr_tracefile_target(
            TARGET ${LOCAL_TEST_TARGET}
            ROOT_DIRECTORY ${CETL_ROOT}
            TARGET_EXECUTION_DEPENDS ${LOCAL_TEST_REPORT}
            OBJECT_LIBRARY ${LOCAL_TEST_LIB}
            EXCLUDE_TEST_FRAMEWORKS
            EXCLUDE_TARGET
            ENABLE_INSTRUMENTATION
            OUT_TRACEFILE_VARIABLE LOCAL_TEST_TRACEFILE
        )
    endif()
    list(APPEND ALL_EXAMPLES "${LOCAL_TEST_TARGET}")
    list(APPEND ALL_EXAMPLE_RUNS "${LOCAL_TEST_REPORT}")
endforeach()

add_custom_target(
     build_examples
     DEPENDS
          ${ALL_EXAMPLES}
)

define_junit_to_sonarqube_conversion_rule(
    SOURCE_FILES ${ALL_EXAMPLE_RUNS}
    OUTPUT "examples-sonarqube.xml"
)

add_custom_target(
     run_examples
     DEPENDS
        "examples-sonarqube.xml"
)

set_directory_properties(PROPERTIES
    IN_BUILD_TESTS "${ALL_EXAMPLE_RUNS}"
)

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    enable_coverage_report(COVERAGE_REPORT_FORMATS sonarqube html
                           ROOT_DIRECTORY ${CETL_ROOT}
    )
endif()
