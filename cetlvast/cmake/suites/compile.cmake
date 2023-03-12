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
# :param ARG_TEST_NAME str:     The name for the test executable. This will only be created if
#                               the test fails. The ctest will be named "ct_${ARG_TEST_NAME}".
# :param ARG_TEST_SOURCE path: The test source file.
#
function(define_compile_failure_test ARG_TEST_NAME ARG_TEST_SOURCE)

    # First build with "precheck" enabled to ensure the test compiles without the negative case included...
    add_executable(${ARG_TEST_NAME}_precheck ${ARG_TEST_SOURCE})

    target_compile_definitions(${ARG_TEST_NAME}_precheck PRIVATE CETLVAST_COMPILETEST_PRECHECK=1)

    # Now define the doomed version for ctest to run...
    add_executable(${ARG_TEST_NAME} ${ARG_TEST_SOURCE})

    set_target_properties(
        ${ARG_TEST_NAME}
        PROPERTIES
            EXCLUDE_FROM_ALL ON
    )

    add_test(
        NAME ct_${ARG_TEST_NAME}
        COMMAND
            ${CMAKE_COMMAND}
            --build ${CMAKE_CURRENT_BINARY_DIR}
            --target ${ARG_TEST_NAME}
            --config $<CONFIGURATION>
    )

    set_tests_properties(
        ct_${ARG_TEST_NAME}
        PROPERTIES
            WILL_FAIL true
    )

endfunction()

file(GLOB COMPILE_TESTS
     LIST_DIRECTORIES false
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/suites/compile/test_*.cpp
)

foreach(COMPILE_TEST ${COMPILE_TESTS})
    get_filename_component(COMPILE_TEST_NAME ${COMPILE_TEST} NAME_WE)
    define_compile_failure_test(${COMPILE_TEST_NAME} ${COMPILE_TEST})
endforeach()
