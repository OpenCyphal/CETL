#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
cmake_minimum_required(VERSION 3.22.0)

project(cetlvast_compile_tests CXX)

include(CompileTest)

set(COMPILE_TESTS
     ${CMAKE_CURRENT_SOURCE_DIR}/test_cetl_h_erase.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_span_empty.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_unbounded_variant_footprint_get_const.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_unbounded_variant_footprint_get_non_const.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_unbounded_variant_footprint_set.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/test_unbounded_variant_zero_footprint_non_pmr.cpp
)

set(ALL_TESTS_BUILD "")
set(ALL_TESTS_RUN "")

foreach(COMPILE_TEST ${COMPILE_TESTS})
    define_compile_failure_test(TEST_SOURCE ${COMPILE_TEST}
                                EXTRA_TEST_LIBS cetl cetlvast
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
