#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
cmake_minimum_required(VERSION 3.22.0)

project(cetlvast_compile_tests CXX)

include(CompileTest)

file(GLOB COMPILE_TESTS
     LIST_DIRECTORIES false
     CONFIGURE_DEPENDS
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/test_*.cpp
)

set(ALL_TESTS_BUILD "")
set(ALL_TESTS_RUN "")

foreach(COMPILE_TEST ${COMPILE_TESTS})
    define_compile_failure_test(TEST_SOURCE ${COMPILE_TEST}
                                EXTRA_TEST_LIBS cetl
                                OUT_TEST_BUILD_TARGET COMPILE_TEST_BUILD_TARGET
                                OUT_TEST_PRECHECK_TARGET COMPILE_TEST_PRECHECK_TARGET)
    list(APPEND ALL_TESTS_BUILD "${COMPILE_TEST_BUILD_TARGET}")
    list(APPEND ALL_TESTS_RUN "${COMPILE_TEST_PRECHECK_TARGET}")
endforeach()

add_custom_target(
     build_compile_tests
     DEPENDS ${ALL_TESTS_BUILD}
)

add_custom_target(
     run_compile_precheck_tests
     DEPENDS ${ALL_TESTS_RUN}
)
