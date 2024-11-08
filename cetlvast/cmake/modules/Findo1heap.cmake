#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
if(NOT TARGET o1heap)
include(FetchContent)
include(FindPackageHandleStandardArgs)

set(o1heap_GIT_REPOSITORY "https://github.com/pavel-kirienko/o1heap.git")
set(o1heap_GIT_TAG "master")

FetchContent_Declare(
    o1heap
    GIT_REPOSITORY  ${o1heap_GIT_REPOSITORY}
    GIT_TAG         ${o1heap_GIT_TAG}
)

# The automatic management of the <lowercase>_POPULATED name appears to be broken in
# cmake 3.21 and earlier.  This workaround may not be needed after 3.24.
get_property(o1heap_POPULATED GLOBAL PROPERTY o1heap_POPULATED)

if(NOT o1heap_POPULATED)

    if (NOT FETCHCONTENT_SOURCE_DIR_o1heap)
        set(FETCHCONTENT_SOURCE_DIR_o1heap ${CETLVAST_EXTERNAL_ROOT}/o1heap)
    endif()

    if (NOT ${FETCHCONTENT_FULLY_DISCONNECTED})
        FetchContent_Populate(
            o1heap
            SOURCE_DIR      ${FETCHCONTENT_SOURCE_DIR_o1heap}
            GIT_REPOSITORY  ${o1heap_GIT_REPOSITORY}
            GIT_TAG         ${o1heap_GIT_TAG}
        )
    else()
        set(o1heap_SOURCE_DIR ${FETCHCONTENT_SOURCE_DIR_o1heap})
    endif()

    # The automatic management of the <lowercase>_POPULATED name appears to be broken in
    # cmake 3.21 and earlier.  This workaround may not be needed after 3.24.
    set_property(GLOBAL PROPERTY o1heap_POPULATED true)

    find_package_handle_standard_args(o1heap
        REQUIRED_VARS o1heap_SOURCE_DIR
    )

    add_library(o1heap STATIC ${o1heap_SOURCE_DIR}/o1heap/o1heap.c)
    target_include_directories(o1heap PUBLIC ${o1heap_SOURCE_DIR}/o1heap)

endif()
endif()
