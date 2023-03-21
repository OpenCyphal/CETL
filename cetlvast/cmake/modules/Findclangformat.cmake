#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

#
# Ensure clang-format is available and check style rules.
#

find_program(CLANG_FORMAT clang-format)

set(LOCAL_BIN_MODULE_PATHS ${CMAKE_MODULE_PATH})
list(TRANSFORM LOCAL_BIN_MODULE_PATHS APPEND "/../bin")

find_file(CLANG_FORMAT_PYTHON_SHIM clang-format-check.py
            PATHS ${LOCAL_BIN_MODULE_PATHS}
)

if (NOT CLANG_FORMAT_PYTHON_SHIM STREQUAL "CLANG_FORMAT_PYTHON_SHIM-NOTFOUND")
    set(CLANG_FORMAT_PYTHON_SHIM_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(clangformat
    REQUIRED_VARS CLANG_FORMAT CLANG_FORMAT_PYTHON_SHIM_FOUND
)

# +---------------------------------------------------------------------------+
# | clang-format helpers
# +---------------------------------------------------------------------------+
#
# :function: enable_clang_format_check_for_directory
# Create a target that checks for compliance with code style rules.
#
# :param DIRECTORY path             - If provided the directory otherwise this is
#                                     ${CMAKE_CURRENT_SOURCE_DIR}
# :param GLOB_PATTERN glob          - A pattern to match files against.
# :option ADD_TO_ALL                - If set the target is added to the default build target.
#
function(enable_clang_format_check_for_directory)
    #+-[input]----------------------------------------------------------------+
    set(options ADD_TO_ALL)
    set(singleValueArgs GLOB_PATTERN DIRECTORY)
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if(NOT ARG_DIRECTORY)
        set(ARG_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
    #+-[body]-----------------------------------------------------------------+
    cmake_path(GET CMAKE_CURRENT_SOURCE_DIR STEM LOCAL_DIRECTORY_NAME)
    set(LOCAL_TARGET_NAME "${LOCAL_DIRECTORY_NAME}_clang_format_check")

    if  (ARG_ADD_TO_ALL)
        set(LOCAL_ALL "ALL")
    else()
        set(LOCAL_ALL  "")
    endif()

    add_custom_target(${LOCAL_TARGET_NAME} ${LOCAL_ALL}
                      COMMAND ${CLANG_FORMAT_PYTHON_SHIM}
                              --clang-format-path ${CLANG_FORMAT}
                              ${ARG_GLOB_PATTERN}
                      VERBATIM
                      WORKING_DIRECTORY ${ARG_DIRECTORY}
    )

endfunction(enable_clang_format_check_for_directory)

#
# :function: enable_clang_format_in_place_for_directory
# Create a target that reformats source, in-place, based on formatting rules.
#
# :param DIRECTORY path       - If provided the directory otherwise this is
#                               ${CMAKE_CURRENT_SOURCE_DIR}
# :param GLOB_PATTERN glob    - A pattern to match files against.
#
function(enable_clang_format_in_place_for_directory)

    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs GLOB_PATTERN DIRECTORY)
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if(NOT ARG_DIRECTORY)
        set(ARG_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()
    #+-[body]-----------------------------------------------------------------+
    cmake_path(GET CMAKE_CURRENT_SOURCE_DIR STEM LOCAL_DIRECTORY_NAME)
    set(LOCAL_TARGET_NAME "danger-danger-${LOCAL_DIRECTORY_NAME}-clang-format-in-place")

    add_custom_target(${LOCAL_TARGET_NAME}
                      COMMAND ${CLANG_FORMAT_PYTHON_SHIM}
                              --clang-format-path ${CLANG_FORMAT}
                              -i
                              ${ARG_GLOB_PATTERN}
                      VERBATIM
                      WORKING_DIRECTORY ${ARG_DIRECTORY}
    )
endfunction(enable_clang_format_in_place_for_directory)
