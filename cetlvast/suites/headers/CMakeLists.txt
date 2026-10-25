#
# Copyright (C) OpenCyphal Development Team <opencyphal.org>
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

project(cetlvast_header_tests CXX)

# Discover new public headers automatically. Compile each header in isolation and on either side of each
# polyfill facade: namespace aliases can expose diagnostics that isolated headers do not trigger (#263).
file(GLOB_RECURSE PUBLIC_HEADERS CONFIGURE_DEPENDS RELATIVE "${CETL_INCLUDE}" "${CETL_INCLUDE}/cetl/*.hpp")
set(HEADER_TEST_SOURCES)
foreach(HEADER IN LISTS PUBLIC_HEADERS)
    string(MAKE_C_IDENTIFIER "${HEADER}" HEADER_ID)
    foreach(LAYOUT standalone pf17_first pf17_last pf20_first pf20_last)
        set(INCLUDES "#include <${HEADER}>\n")
        if(LAYOUT MATCHES "^pf(17|20)_(first|last)$")
            set(FACADE "#include <cetl/pf${CMAKE_MATCH_1}/cetlpf.hpp>\n")
            if(CMAKE_MATCH_2 STREQUAL "first")
                string(PREPEND INCLUDES "${FACADE}")
            else()
                string(APPEND INCLUDES "${FACADE}")
            endif()
        endif()
        set(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/${HEADER_ID}_${LAYOUT}.cpp")
        file(CONFIGURE OUTPUT "${SOURCE}" CONTENT "${INCLUDES}" @ONLY)
        list(APPEND HEADER_TEST_SOURCES "${SOURCE}")
    endforeach()
endforeach()

foreach(ORDER forward reverse)
    set(ORDERED_HEADERS ${PUBLIC_HEADERS})
    if(ORDER STREQUAL "reverse")
        list(REVERSE ORDERED_HEADERS)
    endif()
    set(INCLUDES "")
    foreach(HEADER IN LISTS ORDERED_HEADERS)
        string(APPEND INCLUDES "#include <${HEADER}>\n")
    endforeach()
    set(SOURCE "${CMAKE_CURRENT_BINARY_DIR}/all_headers_${ORDER}.cpp")
    file(CONFIGURE OUTPUT "${SOURCE}" CONTENT "${INCLUDES}" @ONLY)
    list(APPEND HEADER_TEST_SOURCES "${SOURCE}")
endforeach()

add_library(header_tests OBJECT ${HEADER_TEST_SOURCES})
target_link_libraries(header_tests PRIVATE cetl o1heap)
# Keep each include order in its own translation unit, including in unity builds.
set_target_properties(header_tests PROPERTIES UNITY_BUILD OFF DISABLE_PRECOMPILE_HEADERS ON)

add_custom_target(build_header_tests DEPENDS header_tests)
# CI presets build and run unit tests directly as well as through the aggregate build target.
add_dependencies(build_unittests build_header_tests)
add_dependencies(run_unittests build_header_tests)
