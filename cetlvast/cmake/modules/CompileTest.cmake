#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

enable_testing()

#
# Creates a ctest that will succeed if the test fails to compile. Yep, you read that
# correctly: SUCCESS == FAILURE for this set of tests. We use these to validate
# compile-time asserts which guard against illegal template formation.
#
# This cmake-based solution was Inspired by a similar script written by
# Louis Dionne for libawful: https://github.com/ldionne/libawful
#
# :param TEST_SOURCE path: A single source file that defines the test main().
# :param OUT_TEST_BUILD_TARGET path: The name of the target that builds the test.
# :param OUT_TEST_PRECHECK_TARGET path: The name of the target that runs a precheck
#   version of the test.
# :param OUT_CTEST_NAME path: The name of the ctest that runs the test.
#
function(define_compile_failure_test)

    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs TEST_SOURCE OUT_TEST_BUILD_TARGET OUT_TEST_PRECHECK_TARGET OUT_CTEST_NAME)
    set(multiValueArgs EXTRA_TEST_LIBS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    cmake_path(GET ARG_TEST_SOURCE STEM LOCAL_TEST_NAME)

    #+-[body]-----------------------------------------------------------------+
    # First build with "precheck" enabled to ensure the test compiles without the negative case included...
    add_executable(${LOCAL_TEST_NAME}_precheck ${ARG_TEST_SOURCE})

    target_compile_definitions(${LOCAL_TEST_NAME}_precheck PRIVATE CETLVAST_COMPILETEST_PRECHECK=1)
    target_link_libraries(${LOCAL_TEST_NAME}_precheck PUBLIC ${ARG_EXTRA_TEST_LIBS})

    add_custom_target(
          "run_${LOCAL_TEST_NAME}_precheck"
          COMMAND
               ${CMAKE_CURRENT_BINARY_DIR}/${LOCAL_TEST_NAME}_precheck
          DEPENDS
               "${LOCAL_TEST_NAME}_precheck"
    )

    # Now define the doomed version for ctest to run...
    add_executable(${LOCAL_TEST_NAME} ${ARG_TEST_SOURCE})
    target_link_libraries(${LOCAL_TEST_NAME} PUBLIC ${ARG_EXTRA_TEST_LIBS})

    set_target_properties(
        ${LOCAL_TEST_NAME}
        PROPERTIES
            EXCLUDE_FROM_ALL ON
    )

    add_test(
        NAME run_${LOCAL_TEST_NAME}
        COMMAND
            ${CMAKE_COMMAND}
            --build ${CMAKE_BINARY_DIR}
            --target ${LOCAL_TEST_NAME}
            --config $<CONFIGURATION>
    )

    set_tests_properties(
        run_${LOCAL_TEST_NAME}
        PROPERTIES
            WILL_FAIL true
    )

    #+-[output]---------------------------------------------------------------+

    if (NOT ARG_OUT_TEST_BUILD_TARGET STREQUAL "")
        set(${ARG_OUT_TEST_BUILD_TARGET} "${LOCAL_TEST_NAME}_precheck" PARENT_SCOPE)
    endif()

    if (NOT ARG_OUT_TEST_PRECHECK_TARGET STREQUAL "")
        set(${ARG_OUT_TEST_PRECHECK_TARGET} "run_${LOCAL_TEST_NAME}_precheck" PARENT_SCOPE)
    endif()

    if (NOT ARG_OUT_CTEST_NAME STREQUAL "")
        set(${ARG_OUT_CTEST_NAME} "run_${LOCAL_TEST_NAME}" PARENT_SCOPE)
    endif()

endfunction()
