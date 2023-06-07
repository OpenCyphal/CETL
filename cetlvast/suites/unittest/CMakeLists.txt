#
# Copyright Amazon.com, Inc. or its affiliates. All Rights Reserved.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

project(cetlvast_unittest CXX)

find_package(googletest REQUIRED)
find_package(TestReport REQUIRED)
find_package(o1heap REQUIRED)
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
find_package(gcovr REQUIRED)
endif()


# +---------------------------------------------------------------------------+
#   We generate individual test binaires so we can record which test generated
#   what coverage. We also allow test authors to generate coverage reports for
#   just one test allowing for faster iteration.
file(GLOB NATIVE_TESTS
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    RELATIVE ${CMAKE_CURRENT_SOURCE_DIR}
    test_*.cpp
)

set(ALL_TESTS_BUILD "")
set(ALL_TESTS_RUN "")

foreach(NATIVE_TEST ${NATIVE_TESTS})
    define_native_gtest_unittest_targets(
        TEST_SOURCE ${NATIVE_TEST}
        EXTRA_TEST_LIBS
            cetlvast
            cetl
            o1heap
        OUT_TEST_LIB_VARIABLE LOCAL_TEST_LIB
        OUT_TEST_EXE_VARIABLE LOCAL_TEST_TARGET
        OUT_TEST_REPORT_VARIABLE LOCAL_TEST_REPORT
    )

    # To use cetlvast as the test main we have to annotate the test library
    # to indicate that cetlvast is a test framework.
    mark_as_test_framework(
        TEST_TARGET ${LOCAL_TEST_TARGET}
        FRAMEWORK_TARGETS cetlvast
    )

    list(APPEND ALL_TESTS_BUILD ${LOCAL_TEST_TARGET})
    list(APPEND ALL_TESTS_RUN ${LOCAL_TEST_REPORT})

    if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
        define_gcovr_tracefile_target(
            TARGET ${LOCAL_TEST_TARGET}
            ROOT_DIRECTORY ${CETL_ROOT}
            TARGET_EXECUTION_DEPENDS ${LOCAL_TEST_REPORT}
            OBJECT_LIBRARY ${LOCAL_TEST_LIB}
            EXCLUDE_TEST_FRAMEWORKS
            EXCLUDE_TARGET
            ENABLE_INSTRUMENTATION
            OUT_TRACEFILE_VARIABLE LOCAL_TEST_TRACEFILE
        )
    endif()
endforeach()

add_custom_target(
     build_unittests
     DEPENDS
          ${ALL_TESTS_BUILD}
)

define_junit_to_sonarqube_conversion_rule(
    SOURCE_FILES ${ALL_TESTS_RUN}
    OUTPUT "unittest-sonarqube.xml"
)

add_custom_target(
     run_unittests
     DEPENDS
        "unittest-sonarqube.xml"
)

set_directory_properties(PROPERTIES
    IN_BUILD_TESTS "${ALL_TESTS_RUN}"
)

if (CMAKE_BUILD_TYPE STREQUAL "Coverage")
    enable_coverage_report(COVERAGE_REPORT_FORMATS html sonarqube
                           ROOT_DIRECTORY ${CETL_ROOT}
                           OUT_REPORT_INDICIES LOCAL_COVERAGE_REPORT_INDICIES
    )
endif()


# +---------------------------------------------------------------------------+
# | SONARQUBE
# +---------------------------------------------------------------------------+
if (CMAKE_BUILD_TYPE STREQUAL "Coverage")

# we need to build at least one source file to get sonarqube to look at the cetl
# headers.
add_executable(cetlvast_sonar
    "sonar.cpp"
)

target_link_libraries(cetlvast_sonar
    PRIVATE cetl
)

find_package(SonarScanner)

if (${SonarScanner_FOUND})

    # This is allows local debugging of the github action that runs sonarqube.
    # It's not supposed to be part of the normal development process. To use sonarqube
    # as part of your normal development process, you should use SonarSource.sonarlint-vscode
    # in vscode.
    #
    # WARNING: If you are a project admin and you have a SONAR_TOKEN, this will upload
    # the measure to the CETL project on sonarcloud.io. Be sure you are on a branch
    # if you do this.

    message(STATUS "SonarScanner found. Adding a local Sonar Scanner target.")

    if(NOT DEFINED ENV{SONAR_TOKEN})
        message(WARNING "SONAR_TOKEN token not found. Set this in the environment upload scan results to "
                        "${SONARSCANNER_SONARCLOUD_URL}. While the scan will still run, the scanner will return an "
                        "error which will fail the build.")
    endif()

    define_sonar_cloud_scan_target_for_c_cpp(
        ROOT_DIRECTORY ${CETL_ROOT}
        PROJECT_KEY OpenCyphal_CETL
        PROJECT_NAME CETLVaSt
        DEPENDS
            unittest-sonarqube.xml
            ${LOCAL_COVERAGE_REPORT_INDICIES}
            cetlvast_sonar
        SOURCES
            "include"
            "${CMAKE_CURRENT_SOURCE_DIR}/sonar.cpp"
        TESTS
            "${CMAKE_CURRENT_SOURCE_DIR}"
        TEST_INCLUSIONS
            "test_*.cpp"
        EXCLUDE_COVERAGE
            "${CMAKE_CURRENT_SOURCE_DIR}/**/*,**/sonar.cpp"
        EXCLUDE_CPD
            "${CMAKE_CURRENT_SOURCE_DIR}/**/*,**/sonar.cpp"
        CPP_VERSION ${CETLVAST_CPP_STANDARD}
        COVERAGE_REPORTS
            "${CMAKE_CURRENT_BINARY_DIR}/coverage.xml"
        TEST_REPORTS
            "${CMAKE_CURRENT_BINARY_DIR}/unittest-sonarqube.xml"
    )
else()
    message(STATUS "SonarScanner not found. Skipping SonarCloud integration.")
endif()
endif()
