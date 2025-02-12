#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

if(NOT TARGET benchmark)

include(${CMAKE_CURRENT_LIST_DIR}/ExternalDependenciesGitHub.cmake.in)
include(FetchContent)
include(FindPackageHandleStandardArgs)

FetchContent_Declare(
    benchmark
    SOURCE_DIR      "${CETLVAST_EXTERNAL_ROOT}/benchmark"
    GIT_REPOSITORY  "https://github.com/google/benchmark.git"
    GIT_TAG         ${GIT_TAG_benchmark}
    GIT_SHALLOW     ON
    GIT_SUBMODULES_RECURSE OFF
)

set(BENCHMARK_ENABLE_TESTING OFF CACHE BOOL "")
# google benchmark uninitialized variable access list 😔
set(BENCHMARK_CXX_LINKER_FLAGS "" CACHE STRING "")
set(BENCHMARK_CXX_LIBRARIES "" CACHE STRING "")
set(BENCHMARK_CXX_FLAGS "" CACHE STRING "")
set(CMAKE_REQUIRED_FLAGS "" CACHE STRING "")
set(CMAKE_BUILD_TYPE "" CACHE STRING "")
set(CMAKE_CXX_FLAGS_COVERAGE "" CACHE STRING "")
set(BENCHMARK_PRIVATE_LINK_LIBRARIES "" CACHE STRING "")

FetchContent_MakeAvailable(
    benchmark
)

find_package_handle_standard_args(benchmark
    REQUIRED_VARS benchmark_SOURCE_DIR
)

target_compile_options(benchmark PRIVATE
    -Wno-switch-enum
    -Wno-missing-declarations
    -Wno-sign-conversion
)

endif()
