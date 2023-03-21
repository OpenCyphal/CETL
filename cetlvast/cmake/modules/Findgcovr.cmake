#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

find_program(GCOVR gcovr)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(gcovr
    REQUIRED_VARS GCOVR
)

# +---------------------------------------------------------------------------+
# | Contributed helpers for enabling and processing coverage data
# +---------------------------------------------------------------------------+
define_property(DIRECTORY
    PROPERTY GCOV_TRACE_FILES
    BRIEF_DOCS "private collection used by gcovr module."
    FULL_DOCS "private collection used by gcovr module."
)

#
# function: define_gcovr_tracefile_target
#
# param: TARGET target      - The target that will run to produce TARGET_EXECUTION_DEPENDS.
#                             While TARGET_EXECUTION_DEPENDS is used as the dependency of
#                             the tracefile target created by this function the target itself
#                             is queried for properties that help filter the coverage data.
#
# param: TARGET_EXECUTION_DEPENDS target    - A target that will be a dependency of the tracefile
#                                             target defined by this function.
# param: EXCLUDES list[target]              - A list of targets to exclude from the coverage data.
# option: EXCLUDE_TARGET                    - If set the target itself will be excluded from the
#                                             coverage data.
# option: EXCLUDE_TEST_FRAMEWORKS           - If set the test frameworks will be excluded from the
#                                             coverage data. This uses the custome TEST_FRAMEWORK_LINK_LIBRARIES
#                                             property to determine the test frameworks.
# param: OUT_TRACEFILE_VARIABLE string      - The name of a variable to set to the tracefile
#                                             that the custom command defined by this function
#                                             will generate.
#
function(define_gcovr_tracefile_target)
    #+-[input]----------------------------------------------------------------+
    set(options EXCLUDE_TARGET EXCLUDE_TEST_FRAMEWORKS)
    set(singleValueArgs TARGET TARGET_EXECUTION_DEPENDS OUT_TRACEFILE_VARIABLE)
    set(multiValueArgs EXCLUDES)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    #+-[body]-----------------------------------------------------------------+
    get_target_property(LOCAL_TARGET_RUNTIME_DIR ${ARG_TARGET} RUNTIME_OUTPUT_DIRECTORY)

    if (LOCAL_TARGET_RUNTIME_DIR STREQUAL "LOCAL_TARGET_RUNTIME_DIR-NOTFOUND")
        set(LOCAL_TARGET_RUNTIME_DIR ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    set(LOCAL_EXCLUDE_ARGUMENTS "")

    if (ARG_EXCLUDE_TARGET)
        list(APPEND ARG_EXCLUDES ${ARG_TARGET})
    endif()

    if (ARG_EXCLUDE_TEST_FRAMEWORKS)
        get_target_property(LOCAL_TEST_FRAMEWORK_LINK_LIBRARIES ${ARG_TARGET} TEST_FRAMEWORK_LINK_LIBRARIES)
        if(NOT LOCAL_TEST_FRAMEWORK_LINK_LIBRARIES STREQUAL "LOCAL_TEST_FRAMEWORK_LINK_LIBRARIES-NOTFOUND")
            list(APPEND ARG_EXCLUDES ${LOCAL_TEST_FRAMEWORK_LINK_LIBRARIES})
        endif()
    endif()

    list(REMOVE_DUPLICATES ARG_EXCLUDES)
    set(LOCAL_EXCLUDE_PATHS "")

    foreach(LOCAL_EXCLUDE ${ARG_EXCLUDES})

        if(${LOCAL_EXCLUDE} MATCHES ".*-NOTFOUND$")
            message(WARNING "gcovr ${ARG_TARGET}: Invalid exclude target ${LOCAL_EXCLUDE} passed to define_gcovr_tracefile_target.")
            continue()
        endif()

        message(TRACE "gcovr ${ARG_TARGET}: using ${LOCAL_EXCLUDE} to calculate exclude path.")

        get_target_property(LOCAL_TARGET_SOURCE_DIR ${LOCAL_EXCLUDE} SOURCE_DIR)

        list(APPEND LOCAL_EXCLUDE_PATHS ${LOCAL_TARGET_SOURCE_DIR})

    endforeach()

    list(REMOVE_DUPLICATES LOCAL_EXCLUDE_PATHS)

    foreach(LOCAL_EXCLUDE_PATH ${LOCAL_EXCLUDE_PATHS})

        message(DEBUG "gcovr ${ARG_TARGET}: will exclude ${LOCAL_EXCLUDE_PATH} from coverage data.")

        list(APPEND LOCAL_EXCLUDE_ARGUMENTS "--exclude")
        list(APPEND LOCAL_EXCLUDE_ARGUMENTS "\"${LOCAL_EXCLUDE_PATH}\"")
        list(APPEND LOCAL_EXCLUDE_ARGUMENTS "--gcov-exclude")
        list(APPEND LOCAL_EXCLUDE_ARGUMENTS "\"${LOCAL_EXCLUDE_PATH}\"")
    endforeach()

    cmake_path(APPEND LOCAL_TARGET_RUNTIME_DIR "${ARG_TARGET}.json" OUTPUT_VARIABLE LOCAL_TRACEFILE_PATH)

    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        set(LOCAL_C_COVERAGE_PROCESSOR "llvm-cov gcov")
    else()
        set(LOCAL_C_COVERAGE_PROCESSOR "gcov")
    endif()

    add_custom_command(
        COMMAND # Generate tracefile from tests.
            ${GCOVR}
                --gcov-executable ${LOCAL_C_COVERAGE_PROCESSOR}
                --r ${CMAKE_SOURCE_DIR}
                --json
                --output ${LOCAL_TRACEFILE_PATH}
                ${LOCAL_EXCLUDE_ARGUMENTS}
                ${LOCAL_TARGET_RUNTIME_DIR}
        WORKING_DIRECTORY ${LOCAL_TARGET_RUNTIME_DIR}
        OUTPUT ${LOCAL_TRACEFILE_PATH}
        DEPENDS ${ARG_TARGET_EXECUTION_DEPENDS}
    )

    message(DEBUG "${GCOVR} will run under ${LOCAL_TARGET_RUNTIME_DIR} if ${ARG_TARGET_EXECUTION_DEPENDS}.")

    add_custom_target(
        create_${ARG_TARGET}_tracefile
        DEPENDS ${LOCAL_TRACEFILE_PATH}
    )

    #+-[output]---------------------------------------------------------------+
    set_property(DIRECTORY APPEND PROPERTY GCOV_TRACE_FILES ${LOCAL_TRACEFILE_PATH})

    if (NOT ARG_OUT_TRACEFILE_VARIABLE STREQUAL "")
        set(${ARG_OUT_TRACEFILE_VARIABLE} "${LOCAL_TRACEFILE_PATH}" PARENT_SCOPE)
    endif()

endfunction(define_gcovr_tracefile_target)

#
# function: enable_html_report - Creates a custom target that will generate a html report using gcovr
# for the current directory. Be sure to call this only after all calls to define_gcovr_tracefile_target
# have been made.
#
# param: OUT_REPORT_INDEX string - The name of a variable to set to the index file of the html report.
#
function (enable_html_report)
    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs OUT_REPORT_INDEX)
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    get_property(LOCAL_TRACEFILES DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} PROPERTY GCOV_TRACE_FILES)
    #+-[body]-----------------------------------------------------------------+

    cmake_path(GET CMAKE_CURRENT_SOURCE_DIR STEM LOCAL_DIRECTORY_NAME)

    set(LOCAL_ALL_TRACEFILES "")
    set(LOCAL_ALL_GCOV_ARGS "")

    if (NOT LOCAL_TRACEFILES MATCHES ".*-NOTFOUND$")
        foreach(LOCAL_TRACEFILE ${LOCAL_TRACEFILES})
            list(APPEND LOCAL_ALL_TRACEFILES ${LOCAL_TRACEFILE})
            list(APPEND LOCAL_ALL_GCOV_ARGS "--add-tracefile")
            list(APPEND LOCAL_ALL_GCOV_ARGS "${LOCAL_TRACEFILE}")
        endforeach()
    endif()

    set(LOCAL_REPORT_INDEX "gcovr_html/coverage.html")

    add_custom_command(
        OUTPUT ${LOCAL_REPORT_INDEX}
        COMMAND
            ${GCOVR}
                ${LOCAL_ALL_GCOV_ARGS}
                --r ${CMAKE_SOURCE_DIR}
                --html-details ${LOCAL_REPORT_INDEX}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        DEPENDS ${LOCAL_ALL_TRACEFILES}
    )

    add_custom_target(
        gcovr_html_report_for_${LOCAL_DIRECTORY_NAME}
        DEPENDS ${LOCAL_REPORT_INDEX}
    )

    #+-[output]---------------------------------------------------------------+

    if (NOT ARG_OUT_REPORT_INDEX STREQUAL "")
        set(${ARG_OUT_REPORT_INDEX} "${LOCAL_REPORT_INDEX}" PARENT_SCOPE)
    endif()

endfunction(enable_html_report)
