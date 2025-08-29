#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

#
# XML converter for junit test reports.
#


set(LOCAL_BIN_MODULE_PATHS ${CMAKE_MODULE_PATH})
list(TRANSFORM LOCAL_BIN_MODULE_PATHS APPEND "/../bin")

find_file(LOCAL_TEST_REPORT_UTILITY test_report_util.py
            PATHS ${LOCAL_BIN_MODULE_PATHS}
)

if (NOT LOCAL_TEST_REPORT_UTILITY STREQUAL "LOCAL_TEST_REPORT_UTILITY-NOTFOUND")
    set(LOCAL_TEST_REPORT_UTILITY_FOUND TRUE)
endif()

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(TestReport
    REQUIRED_VARS LOCAL_TEST_REPORT_UTILITY_FOUND
)

#
# :function: define_junit_to_sonarqube_conversion_rule
# The target will be the OUTPUT file which is generated from the provided set
# of SOURCE_FILES.
#
# :param SOURCE_FILES list[path]    - A list of files to convert and combine.
# :param OUTPUT path                - The output file to write to.
#
function(define_junit_to_sonarqube_conversion_rule)
    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs OUTPUT BASEDIR)
    set(multiValueArgs SOURCE_FILES)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    #+-[body]-----------------------------------------------------------------+
    set(LOCAL_ARG_LIST)
    if (ARG_BASEDIR)
        list(APPEND LOCAL_ARG_LIST "--base-dir" ${ARG_BASEDIR})
    endif()
    foreach(LOCAL_INPUT_FILE ${ARG_SOURCE_FILES})
        list(APPEND LOCAL_ARG_LIST "-i")
        list(APPEND LOCAL_ARG_LIST ${LOCAL_INPUT_FILE})
    endforeach()
    add_custom_command(OUTPUT ${ARG_OUTPUT}
                       COMMAND ${LOCAL_TEST_REPORT_UTILITY}
                               ${LOCAL_ARG_LIST}
                               ${ARG_OUTPUT}
                       DEPENDS ${ARG_SOURCE_FILES}
                       VERBATIM
    )

endfunction(define_junit_to_sonarqube_conversion_rule)
