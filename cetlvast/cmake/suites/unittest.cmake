#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# unit tests compiled for whatever environment the build is running on. They assume they are running
# on a fairly robust POSIX environment and use googletest/googlemock to organize the tests. Native tests should work
# on linux, osx, or Windows hosts and should work on any popular architecture including 32-bit and 64-bit ARM and x86.
# Finally, native tests assume the available toolchain can compile and run executables as part of the build process.
#

# All test binaries and reports will be created under this directory.
set(CETLVAST_NATIVE_TEST_BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/cetlvast/suites/unittest)

#
# googletest (and googlemock) external project.
#
find_package(gtest REQUIRED)

# +---------------------------------------------------------------------------+
# | BUILD NATIVE UNIT TESTS
# +---------------------------------------------------------------------------+

#
# function: define_native_unit_test - creates an executable target and links it
# to the "all" target to build a gtest binary for the given test source.
#
# param: ARG_TEST_NAME string       - The name to give the test binary.
# param: ARG_TEST_SOURCE List[path] - A list of source files to compile into
#                                     the test binary.
# param: ARG_OUTDIR path            - A path to output test binaries and coverage data under.
#
function(define_native_unit_test ARG_TEST_NAME ARG_TEST_SOURCE ARG_OUTDIR)

     add_executable(${ARG_TEST_NAME} ${ARG_TEST_SOURCE})
     target_link_libraries(${ARG_TEST_NAME} gmock_main)
     set_target_properties(${ARG_TEST_NAME}
                           PROPERTIES
                           RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTDIR}"
     )

endfunction()


#
# function: define_native_test_run - creates a rule that will build and run individual
# unit tests.
#
# param: ARG_TEST_NAME string - The name of the test to run. A target will be created
#                               with the name run_${ARG_TEST_NAME}
# param: ARG_OUTDIR path      - The path where the test binaries live.
#
function(define_native_test_run ARG_TEST_NAME ARG_OUTDIR)
     add_custom_target(
          run_${ARG_TEST_NAME}
          COMMAND
               ${ARG_OUTDIR}/${ARG_TEST_NAME}
          DEPENDS
               ${ARG_TEST_NAME}
     )

endfunction()

# +---------------------------------------------------------------------------+
#   We generate individual test binaires so we can record which test generated
#   what coverage. We also allow test authors to generate coverage reports for
#   just one test allowing for faster iteration.
file(GLOB NATIVE_TESTS
     LIST_DIRECTORIES false
     RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
     ${CMAKE_CURRENT_SOURCE_DIR}/suites/unittest/test_*.cpp
)

add_custom_target(
     lcov_zero
     ${LCOV}
          ${CETLVAST_GCOV_TOOL_ARG}
          --zerocounters
          --directory ${CMAKE_CURRENT_BINARY_DIR}
     COMMENT "Resetting coverage counters."
)

set(ALL_TESTS_BUILD "")
set(ALL_TESTS "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
    get_filename_component(NATIVE_TEST_NAME ${NATIVE_TEST} NAME_WE)
    message(STATUS "Defining googletest binary ${NATIVE_TEST_NAME} for source file ${NATIVE_TEST}")
    define_native_unit_test(${NATIVE_TEST_NAME} ${NATIVE_TEST} ${CETLVAST_NATIVE_TEST_BINARY_DIR})
    define_native_test_run(${NATIVE_TEST_NAME} ${CETLVAST_NATIVE_TEST_BINARY_DIR})
    list(APPEND ALL_TESTS_BUILD "${NATIVE_TEST_NAME}")
    list(APPEND ALL_TESTS "run_${NATIVE_TEST_NAME}")
endforeach()

add_custom_target(
     build_all
     DEPENDS ${ALL_TESTS_BUILD}
)

if (CETLVAST_ENABLE_COVERAGE)

message(STATUS "Coverage is enabled: adding coverage targets.")

# +---------------------------------------------------------------------------+
#   If coverage is enabled we have more work to do...
# +---------------------------------------------------------------------------+

find_package(lcov REQUIRED)
find_package(genhtml REQUIRED)

set(ALL_TESTS_WITH_LCOV "")
set(ALL_TEST_COVERAGE "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
    get_filename_component(NATIVE_TEST_NAME ${NATIVE_TEST} NAME_WE)
    define_native_test_run_with_lcov(${NATIVE_TEST_NAME} ${CETLVAST_NATIVE_TEST_BINARY_DIR})
    define_natve_test_coverage(${NATIVE_TEST_NAME} ${CETLVAST_NATIVE_TEST_BINARY_DIR})
    list(APPEND ALL_TESTS_WITH_LCOV "run_${NATIVE_TEST_NAME}_with_lcov")
    list(APPEND ALL_TEST_COVERAGE "--add-tracefile")
    list(APPEND ALL_TEST_COVERAGE "${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.${NATIVE_TEST_NAME}.filtered.info")
endforeach()

add_custom_command(
     OUTPUT ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.all.info
     COMMAND
          ${LCOV}
               ${CETLVAST_GCOV_TOOL_ARG}
               --rc lcov_branch_coverage=1
               ${ALL_TEST_COVERAGE}
               --output-file ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.all.info
     DEPENDS ${ALL_TESTS_WITH_LCOV}
)

add_custom_command(
     OUTPUT ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.info
     COMMAND
          ${LCOV}
               ${CETLVAST_GCOV_TOOL_ARG}
               --rc lcov_branch_coverage=1
               --extract ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.all.info
                         ${CETL_ROOT}/include/\\*
               --output-file ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.info
     DEPENDS ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.all.info
)

add_custom_target(
     cov_info
     DEPENDS ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.info
)

add_custom_target(
     cov_all
     ${GENHTML} --title "${PROJECT_NAME} native test coverage"
          --output-directory ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage/all
          --demangle-cpp
          --sort
          --num-spaces 4
          --function-coverage
          --branch-coverage
          --legend
          --highlight
          --show-details
          ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.info
     DEPENDS ${CETLVAST_NATIVE_TEST_BINARY_DIR}/coverage.info
     COMMENT "Build and run all tests and generate an overall html coverage report."
)

endif()

# +---------------------------------------------------------------------------+

add_custom_target(
     test_all
     DEPENDS
          ${ALL_TESTS}
)

add_custom_target(
     suite_all
     COMMENT
        "All CETL suites define this target as a default action scripts can rely on."
     DEPENDS
        test_all
)

# Write a README to create the tests folder.
file(WRITE ${CETLVAST_NATIVE_TEST_BINARY_DIR}/README.txt
     "All test binaries and output will appear under here.")
