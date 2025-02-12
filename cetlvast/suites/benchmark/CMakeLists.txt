#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

project(cetlvast_benchmark CXX)

find_package(benchmark REQUIRED)

set(TEST_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/pmr/benchmark_pmr_function.cpp
)

set(ALL_TESTS_BUILD "")

foreach(TEST_SOURCE ${TEST_SOURCES})

    cmake_path(GET TEST_SOURCE STEM TEST_NAME)

    add_executable(${TEST_NAME} ${TEST_SOURCE})
    target_link_libraries(${TEST_NAME} cetl benchmark::benchmark)

    list(APPEND ALL_TESTS_BUILD ${TEST_NAME})

endforeach()

add_custom_target(
        build_benchmarks
        DEPENDS
        ${ALL_TESTS_BUILD}
)
