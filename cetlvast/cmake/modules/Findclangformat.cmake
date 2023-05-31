#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

#
# Ensure clang-format is available and check style rules.
#

find_program(CLANG_FORMAT clang-format)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(clangformat
    REQUIRED_VARS CLANG_FORMAT
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
# :option FORMAT_IN_PLACE           - If set the target is defined as an in-place formatter.
# :return OUT_TARGET_NAME           - The name of a variable, in the parent scope, to set to the
#                                     name of the target created.
#
function(enable_clang_format_check_for_directory)
    #+-[input]----------------------------------------------------------------+
    set(options ADD_TO_ALL FORMAT_IN_PLACE)
    set(singleValueArgs GLOB_PATTERN DIRECTORY OUT_TARGET_NAME)
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if(NOT ARG_DIRECTORY)
        set(ARG_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR})
    endif()

    #+-[body]-----------------------------------------------------------------+
    cmake_path(GET CMAKE_CURRENT_SOURCE_DIR STEM LOCAL_DIRECTORY_NAME)

    cmake_path(APPEND ARG_DIRECTORY "${ARG_GLOB_PATTERN}" OUTPUT_VARIABLE LOCAL_GLOB_PATTERN_WITH_PATH)

    if  (ARG_ADD_TO_ALL)
    set(LOCAL_ALL "ALL")
    else()
    set(LOCAL_ALL  "")
    endif()

    file(GLOB_RECURSE LOCAL_SOURCE_FILES
         CONFIGURE_DEPENDS
         LIST_DIRECTORIES false
         RELATIVE ${ARG_DIRECTORY}
         ${LOCAL_GLOB_PATTERN_WITH_PATH}
    )

    if (ARG_FORMAT_IN_PLACE)
        set(LOCAL_FORMAT_IN_PLACE "-i")
        set(LOCAL_TARGET_NAME "danger-danger-${LOCAL_DIRECTORY_NAME}-clang-format-in-place")
    else()
        set(LOCAL_FORMAT_IN_PLACE "--dry-run")
        set(LOCAL_TARGET_NAME "${LOCAL_DIRECTORY_NAME}-clang-format-check")
    endif()

    add_custom_target(${LOCAL_TARGET_NAME} ${LOCAL_ALL}
                      COMMAND ${CLANG_FORMAT}
                              --fallback-style=none
                              -style=file
                              --verbose
                              --Werror
                              ${LOCAL_FORMAT_IN_PLACE}
                              ${LOCAL_SOURCE_FILES}
                      VERBATIM
                      WORKING_DIRECTORY ${ARG_DIRECTORY}
    )

    #+-[output]---------------------------------------------------------------+
    if (ARG_OUT_TARGET_NAME)
        set(${ARG_OUT_TARGET_NAME} ${LOCAL_TARGET_NAME} PARENT_SCOPE)
    endif()

endfunction(enable_clang_format_check_for_directory)
