#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

if(NOT TARGET o1heap)

include(${CMAKE_CURRENT_LIST_DIR}/ExternalDependenciesGitHub.cmake.in)
include(FetchContent)
include(FindPackageHandleStandardArgs)

FetchContent_Declare(
    o1heap
    SOURCE_DIR      "${CETLVAST_EXTERNAL_ROOT}/o1heap"
    GIT_REPOSITORY  "https://github.com/pavel-kirienko/o1heap.git"
    GIT_TAG         ${GIT_TAG_o1heap}
    GIT_SHALLOW     ON
    GIT_SUBMODULES_RECURSE OFF
)

FetchContent_MakeAvailable(
    o1heap
)

find_package_handle_standard_args(o1heap
    REQUIRED_VARS o1heap_SOURCE_DIR
)

add_library(o1heap STATIC ${o1heap_SOURCE_DIR}/o1heap/o1heap.c)
target_include_directories(o1heap PUBLIC ${o1heap_SOURCE_DIR}/o1heap)

endif()
