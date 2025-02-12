#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

if(NOT TARGET gmock)

include(${CMAKE_CURRENT_LIST_DIR}/ExternalDependenciesGitHub.cmake.in)

#
# Googletest uses CMAKE_BINARY_DIR to determine where to put its output files.
# This changes this to a specified directory for each library provided.
# param GTEST_LIBS ...              The list of google test libraries to fix.
# param GTEST_COMPILE_OPTIONS ...   A list of compile options to apply to each google test library.
# param OUTPUT_DIRECTORY            The directory under which all google test build outputs will reside.
#
function (_fix_gtest_library_properties)
    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs OUTPUT_DIRECTORY)
    set(multiValueArgs GTEST_LIBS GTEST_COMPILE_OPTIONS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_OUTPUT_DIRECTORY)
        set(ARG_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    #+-[body]-----------------------------------------------------------------+
    foreach(LOCAL_GTEST_LIB ${ARG_GTEST_LIBS})
        set_target_properties(${LOCAL_GTEST_LIB}
            PROPERTIES
                ARCHIVE_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}/lib"
                LIBRARY_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}/lib"
                RUNTIME_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}/bin"
                PDB_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}/bin"
                COMPILE_PDB_OUTPUT_DIRECTORY "${ARG_OUTPUT_DIRECTORY}/lib"
        )

        target_compile_options(${LOCAL_GTEST_LIB}
            PRIVATE
                ${ARG_GTEST_COMPILE_OPTIONS}
        )
    endforeach()

endfunction()

enable_testing()

include(FetchContent)
include(FindPackageHandleStandardArgs)

list(APPEND LOCAL_PATCH_COMMAND ${CMAKE_CURRENT_LIST_DIR}/ExternalDependenciesPatch.sh "${CMAKE_CURRENT_LIST_DIR}/patches/googletest.patch")

FetchContent_Declare(
    googletest
    SOURCE_DIR      "${CETLVAST_EXTERNAL_ROOT}/googletest"
    GIT_REPOSITORY  "https://github.com/google/googletest.git"
    GIT_TAG         ${GIT_TAG_googletest}
    PATCH_COMMAND   ${LOCAL_PATCH_COMMAND}
    GIT_SHALLOW     ON
    GIT_SUBMODULES_RECURSE OFF
)

set(INSTALL_GTEST OFF CACHE BOOL "We don't want to install googletest; just use it locally.")
set(cxx_base_flags "" CACHE STRING "")
set(cxx_no_rtti_flags "" CACHE STRING "")
set(cxx_exception_flags "" CACHE STRING "")
set(cxx_no_exception_flags "" CACHE STRING "")
set(cxx_strict_flags "" CACHE STRING "")

FetchContent_MakeAvailable(
    googletest
)

_fix_gtest_library_properties(
    GTEST_LIBS
        gmock
        gmock_main
        gtest
        gtest_main
    GTEST_COMPILE_OPTIONS
        "-Wno-sign-conversion"
        "-Wno-zero-as-null-pointer-constant"
        "-Wno-switch-enum"
        "-Wno-float-equal"
        "-Wno-double-promotion"
        "-Wno-conversion"
        "-Wno-missing-declarations"
    OUTPUT_DIRECTORY
        ${CMAKE_BINARY_DIR}/googletest
)

find_package_handle_standard_args(googletest
    REQUIRED_VARS googletest_SOURCE_DIR
)

endif()

# +---------------------------------------------------------------------------+
# | Contributed helpers for building and running gtest-based unit tests.
# +---------------------------------------------------------------------------+

set(_PRIVATE_GOOGLETEST_OBJLIB_SUFFIX "__googletest_objlib")


#
# function: _get_internal_output_path_for_source - Given a source file, returns the
#           output path for the object file that will be generated for it.
#
# This is a hack I don't know how to get rid of. We're not supposed to "know" about the
# CMakeFiles directory nor its internal structure but we have to list binary
# byproducts when enabling coverage to make sure the clean target works properly.
# the use of an OBJECT library at least enforces that these intermediates are available.
#
# param: SOURCEFILE path                        - The source file to get the output path for.
# param: SOURCEFILE_STEM_SUFFIX string          - An optional suffix to append to the SOURCEFILE
#                                                 after the stem but before the extension.
# param: OUT_INTERNAL_DIRECTORY_VARIABLE path   - Set to the output path for the object file.
#
function(_get_internal_output_path_for_source)

    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs SOURCEFILE OUT_INTERNAL_DIRECTORY_VARIABLE SOURCEFILE_STEM_SUFFIX)
    set(multiValueArgs "")
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    #+-[body]-----------------------------------------------------------------+

    cmake_path(GET ARG_SOURCEFILE PARENT_PATH LOCAL_SOURCEFILE_REL_PATH)

    cmake_path(GET ARG_SOURCEFILE STEM LOCAL_SOURCEFILE_NAME)
    cmake_path(SET LOCAL_RESULT "CMakeFiles")
    cmake_path(APPEND LOCAL_RESULT "${LOCAL_SOURCEFILE_NAME}${ARG_SOURCEFILE_STEM_SUFFIX}.dir")

    if (NOT "${LOCAL_SOURCEFILE_REL_PATH}" STREQUAL "")
        cmake_path(APPEND LOCAL_RESULT "${LOCAL_SOURCEFILE_REL_PATH}")
    endif()

    #+-[output]---------------------------------------------------------------+
    set(${ARG_OUT_INTERNAL_DIRECTORY_VARIABLE} ${LOCAL_RESULT} PARENT_SCOPE)
endfunction()


#
# function: mark_as_test_framework - Helper function to mark a given test framework
#           library as a test-framework for a given test library or executable.
#
# param: TEST_TARGET target             - The test library or executable to annotate.
# param: FRAMEWORK_TARGETS list[target] - The test framework libraries to mark on the test target.
#
function (mark_as_test_framework)
    #+-[input]----------------------------------------------------------------+
    set(options)
    set(singleValueArgs TEST_TARGET)
    set(multiValueArgs FRAMEWORK_TARGETS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    #+-[body]-----------------------------------------------------------------+
    list(APPEND TEST_FRAMEWORK_LINK_LIBRARIES_LIST ${ARG_FRAMEWORK_TARGETS})
    get_target_property(LOCAL_TFLL ${ARG_TEST_TARGET} TEST_FRAMEWORK_LINK_LIBRARIES)

    if (NOT LOCAL_TFLL MATCHES ".*-NOTFOUND$")
        list(APPEND TEST_FRAMEWORK_LINK_LIBRARIES_LIST ${LOCAL_TFLL})
    endif()

    set_target_properties(${ARG_TEST_TARGET}
        PROPERTIES
            TEST_FRAMEWORK_LINK_LIBRARIES "${TEST_FRAMEWORK_LINK_LIBRARIES_LIST}"
    )

endfunction(mark_as_test_framework)


#
# function: define_native_gtest_unittest_library - Creates a target to build a static library
#
#
# param: TEST_SOURCE path                   - A single source file that is the test main.
# param: RUNTIME_OUTPUT_DIRECTORY path      - A path to output test binaries and coverage data under.
# param: EXTRA_TEST_LIBS targets            - A list of additional test library targets to get include
#                                             paths for.
# param: EXTRA_TEST_SOURCE paths            - A list of additional source files to compile into the test.
# param: OUT_TEST_LIB_VARIABLE target       - If set, this becomes the name of a variable set, in the parent
#                                             context, to the test case library build target name.
#
function(define_native_gtest_unittest_library)

    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs TEST_SOURCE RUNTIME_OUTPUT_DIRECTORY OUT_TEST_LIB_VARIABLE)
    set(multiValueArgs EXTRA_TEST_LIBS EXTRA_TEST_SOURCE)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_RUNTIME_OUTPUT_DIRECTORY)
        set(ARG_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    if (NOT ARG_EXTRA_TEST_SOURCE)
        set(ARG_EXTRA_TEST_SOURCE)
    endif()

    cmake_path(GET ARG_TEST_SOURCE STEM LOCAL_TEST_NAME)

    #+-[body]-----------------------------------------------------------------+

    set(LOCAL_TEST_LIB_NAME ${LOCAL_TEST_NAME}${_PRIVATE_GOOGLETEST_OBJLIB_SUFFIX})
    message(DEBUG "Defining googletest library ${LOCAL_TEST_LIB_NAME} for source file ${ARG_TEST_SOURCE}")

    # Create explicit object file target so we can find it.
    add_library(${LOCAL_TEST_LIB_NAME} OBJECT ${ARG_TEST_SOURCE})
    target_sources(${LOCAL_TEST_LIB_NAME} PRIVATE ${ARG_EXTRA_TEST_SOURCE})
    target_link_libraries(${LOCAL_TEST_LIB_NAME} PUBLIC gtest)
    target_link_libraries(${LOCAL_TEST_LIB_NAME} PUBLIC gmock)
    target_link_libraries(${LOCAL_TEST_LIB_NAME} PUBLIC ${ARG_EXTRA_TEST_LIBS})

    mark_as_test_framework(
        TEST_TARGET ${LOCAL_TEST_LIB_NAME}
        FRAMEWORK_TARGETS
            gmock
            gtest
    )

    # Annotate the library target with the byproducts of the coverage instrumentation.
    _get_internal_output_path_for_source(
        SOURCEFILE ${ARG_TEST_SOURCE}
        SOURCEFILE_STEM_SUFFIX "${_PRIVATE_GOOGLETEST_OBJLIB_SUFFIX}"
        OUT_INTERNAL_DIRECTORY_VARIABLE LOCAL_OBJLIB_FOLDER_REL
    )
    cmake_path(ABSOLUTE_PATH LOCAL_OBJLIB_FOLDER_REL
                BASE_DIRECTORY ${ARG_RUNTIME_OUTPUT_DIRECTORY}
                OUTPUT_VARIABLE LOCAL_OBJLIB_FOLDER)

    cmake_path(GET ARG_TEST_SOURCE EXTENSION LOCAL_TEST_EXT)

    # the generation of gcda files assumes "-fprofile-argcs" (or "-coverage" which includes this flag).
    set(LOCAL_BYPRODUCTS "${LOCAL_OBJLIB_FOLDER}/${LOCAL_TEST_NAME}${LOCAL_TEST_EXT}.gcda")
    # the generation of gcno files assumes "-ftest-coverage" (or "-coverage" which includes this flag)
    list(APPEND LOCAL_BYPRODUCTS "${LOCAL_OBJLIB_FOLDER}/${LOCAL_TEST_NAME}${LOCAL_TEST_EXT}.gcno")

    set_target_properties(${LOCAL_TEST_LIB_NAME}
        PROPERTIES
        POST_BUILD_INSTRUMENTATION_BYPRODUCTS "${LOCAL_BYPRODUCTS}"
    )

    #+-[output]---------------------------------------------------------------+

    if (NOT ARG_OUT_TEST_LIB_VARIABLE STREQUAL "")
        set(${ARG_OUT_TEST_LIB_VARIABLE} "${LOCAL_TEST_LIB_NAME}" PARENT_SCOPE)
    endif()

endfunction(define_native_gtest_unittest_library)

#
# function: define_native_gtest_unittest_executable - Creates a target to build a test executable
#
#
# param: TEST_LIB target                    - A test target defined by define_native_gtest_unittest_library
# param: RUNTIME_OUTPUT_DIRECTORY path      - A path to output test binaries and coverage data under.
# param: EXTRA_TEST_LIBS targets            - A list of additional test library targets to link with.
# param: OUT_TEST_EXE_VARIABLE target       - If set, this becomes the name of a variable set, in the parent
#                                             context, to the executable target name.
# option: LINK_TO_MAIN                      - If set, the test executable will be linked the googletest main
#                                             and will be a fully executable binary. If omitted the caller
#                                             must provide a valid main function.
#
function(define_native_gtest_unittest_executable)

    #+-[input]----------------------------------------------------------------+
    set(options LINK_TO_MAIN)
    set(singleValueArgs TEST_LIB RUNTIME_OUTPUT_DIRECTORY OUT_TEST_EXE_VARIABLE)
    set(multiValueArgs EXTRA_TEST_LIBS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_RUNTIME_OUTPUT_DIRECTORY)
        set(ARG_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    #+-[body]-----------------------------------------------------------------+

    string(REGEX REPLACE "__googletest_[a-z0-9]+$" "" LOCAL_TEST_NAME ${ARG_TEST_LIB})

    message(DEBUG "Defining googletest executable ${LOCAL_TEST_NAME} for source library ${ARG_TEST_LIB}")

    add_executable(${LOCAL_TEST_NAME} $<TARGET_OBJECTS:${ARG_TEST_LIB}>)
    target_link_libraries(${LOCAL_TEST_NAME} PRIVATE ${ARG_EXTRA_TEST_LIBS})

    if(ARG_LINK_TO_MAIN)
        target_link_libraries(${LOCAL_TEST_NAME} PRIVATE gmock_main)
        mark_as_test_framework(TEST_TARGET ${LOCAL_TEST_NAME} FRAMEWORK_TARGETS gmock_main)
    endif()

    get_target_property(LOCAL_TEST_LIB_TEST_FRAMEWORK_LINK_LIBRARIES ${ARG_TEST_LIB} TEST_FRAMEWORK_LINK_LIBRARIES)

    if (NOT LOCAL_TEST_LIB_TEST_FRAMEWORK_LINK_LIBRARIES MATCHES ".*-NOTFOUND$")
        mark_as_test_framework(TEST_TARGET ${LOCAL_TEST_NAME} FRAMEWORK_TARGETS ${LOCAL_TEST_LIB_TEST_FRAMEWORK_LINK_LIBRARIES})
    endif()

    get_target_property(LOCAL_TEST_LIB_POST_BUILD_INSTRUMENTATION_BYPRODUCTS ${ARG_TEST_LIB} POST_BUILD_INSTRUMENTATION_BYPRODUCTS)

    if (NOT LOCAL_TEST_LIB_POST_BUILD_INSTRUMENTATION_BYPRODUCTS MATCHES ".*-NOTFOUND$")
        set_target_properties(${LOCAL_TEST_NAME}
            PROPERTIES
            POST_BUILD_INSTRUMENTATION_BYPRODUCTS "${LOCAL_TEST_LIB_POST_BUILD_INSTRUMENTATION_BYPRODUCTS}"
        )
    endif()

    set_target_properties(${LOCAL_TEST_NAME}
        PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${ARG_RUNTIME_OUTPUT_DIRECTORY}"
    )

    #+-[output]---------------------------------------------------------------+
    if (NOT ARG_OUT_TEST_EXE_VARIABLE STREQUAL "")
        set(${ARG_OUT_TEST_EXE_VARIABLE} ${LOCAL_TEST_NAME} PARENT_SCOPE)
    endif()

endfunction(define_native_gtest_unittest_executable)

#
# function: define_native_gtest_unittest_run - Creates a cutom target to run a test executable.
#
#
# param: TEST_EXECUTABLE target             - A test target defined by define_native_gtest_unittest_executable
# param: RUNTIME_OUTPUT_DIRECTORY path      - A path to output test binaries and coverage data under.
# param: BYPRODUCTS                         - A list of byproducts expected when running the test.
# param: OUT_TEST_REPORT_VARIABLE target    - If set, this becomes the name of a variable set, in the parent
#                                             context, to a report artifact generated by a test run.
#
function(define_native_gtest_unittest_run)

    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs TEST_EXECUTABLE RUNTIME_OUTPUT_DIRECTORY OUT_TEST_REPORT_VARIABLE)
    set(multiValueArgs BYPRODUCTS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_RUNTIME_OUTPUT_DIRECTORY)
        set(ARG_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    #+-[body]-----------------------------------------------------------------+

    set(LOCAL_TESTRESULT ${ARG_TEST_EXECUTABLE}-gtest.xml)
    set(LOCAL_RUNTARGET run_${ARG_TEST_EXECUTABLE})

    message(DEBUG "Using --gtest_output=xml: expecting test to generate: ${LOCAL_TESTRESULT}")

    get_target_property(LOCAL_ARG_TEST_EXECUTABLE_POST_BUILD_INSTRUMENTATION_BYPRODUCTS ${ARG_TEST_EXECUTABLE} POST_BUILD_INSTRUMENTATION_BYPRODUCTS)

    if (NOT LOCAL_ARG_TEST_EXECUTABLE_POST_BUILD_INSTRUMENTATION_BYPRODUCTS MATCHES ".*-NOTFOUND$")
        list(APPEND ARG_BYPRODUCTS ${LOCAL_ARG_TEST_EXECUTABLE_POST_BUILD_INSTRUMENTATION_BYPRODUCTS})
    endif()

    add_custom_command(
        OUTPUT ${LOCAL_TESTRESULT}
        COMMAND ${ARG_TEST_EXECUTABLE} --gtest_output=xml:${LOCAL_TESTRESULT}
        DEPENDS ${ARG_TEST_EXECUTABLE}
        WORKING_DIRECTORY ${ARG_RUNTIME_OUTPUT_DIRECTORY}
        BYPRODUCTS ${ARG_BYPRODUCTS}
    )

    message(DEBUG "Defining googletest run target ${LOCAL_RUNTARGET} for source library ${ARG_TEST_EXECUTABLE}")

    add_custom_target(
        ${LOCAL_RUNTARGET}
        DEPENDS ${LOCAL_TESTRESULT}
    )

    #+-[output]---------------------------------------------------------------+
    if (NOT ARG_OUT_TEST_REPORT_VARIABLE STREQUAL "")
        set(${ARG_OUT_TEST_REPORT_VARIABLE} ${LOCAL_TESTRESULT} PARENT_SCOPE)
    endif()

endfunction(define_native_gtest_unittest_run)

#
# function: define_native_gtest_unittest_targets - Convenience function for succinct unittest definitions.
#
#           Equivalent to calls to define_native_gtest_unittest_library,
#           define_native_gtest_unittest_executable, and define_native_gtest_unittest_run
#           while setting the given extra test libraries on the unittest library target.
#
# param: TEST_SOURCE path                   - A single source file that is the test main.
# param: EXTRA_TEST_SOURCE paths            - An optional list of additional source files to compile into the
#                                             test.
# param: RUNTIME_OUTPUT_DIRECTORY path      - A path to output test binaries and coverage data under.
# param: EXTRA_TEST_LIBS targets            - A list of additional test library targets to link with.
# option: LINK_TO_MAIN                      - If set, the test executable will be linked the googletest main
#                                             and will be a fully executable binary. If omitted the caller
#                                             must provide a valid main function.
# param: OUT_TEST_LIB_VARIABLE target       - If set, this becomes the name of a variable set, in the parent
#                                             context, to the test case library build target name.
# param: OUT_TEST_EXE_VARIABLE target       - If set, this becomes the name of a variable set, in the parent
#                                             context, to the executable target name.
# param: OUT_TEST_REPORT_VARIABLE target    - If set, this becomes the name of a variable set, in the parent
#                                             context, to a report artifact generated by a test run.
#
function(define_native_gtest_unittest_targets)

    #+-[input]----------------------------------------------------------------+
    set(options LINK_TO_MAIN)
    set(singleValueArgs TEST_SOURCE RUNTIME_OUTPUT_DIRECTORY OUT_TEST_LIB_VARIABLE OUT_TEST_EXE_VARIABLE OUT_TEST_REPORT_VARIABLE)
    set(multiValueArgs EXTRA_TEST_LIBS EXTRA_TEST_SOURCE)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_RUNTIME_OUTPUT_DIRECTORY)
        set(ARG_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    if (ARG_LINK_TO_MAIN)
        set(ARG_LINK_TO_MAIN_FORWARD LINK_TO_MAIN)
    else()
        set(ARG_LINK_TO_MAIN_FORWARD)
    endif()

    if (NOT ARG_EXTRA_TEST_SOURCE)
        set(ARG_EXTRA_TEST_SOURCE)
    endif()

    #+-[body]-----------------------------------------------------------------+

    define_native_gtest_unittest_library(
        TEST_SOURCE
            ${ARG_TEST_SOURCE}
        EXTRA_TEST_SOURCE
            ${ARG_EXTRA_TEST_SOURCE}
        RUNTIME_OUTPUT_DIRECTORY
            ${ARG_RUNTIME_OUTPUT_DIRECTORY}
        EXTRA_TEST_LIBS
            ${ARG_EXTRA_TEST_LIBS}
        OUT_TEST_LIB_VARIABLE
            "LOCAL_TEST_LIB"
    )

    define_native_gtest_unittest_executable(
        TEST_LIB
            ${LOCAL_TEST_LIB}
        RUNTIME_OUTPUT_DIRECTORY
            ${ARG_RUNTIME_OUTPUT_DIRECTORY}
        EXTRA_TEST_LIBS
            ${ARG_EXTRA_TEST_LIBS}
        ${ARG_LINK_TO_MAIN_FORWARD}
        OUT_TEST_EXE_VARIABLE
            "LOCAL_TEST_EXE"
    )

    define_native_gtest_unittest_run(
        TEST_EXECUTABLE
            ${LOCAL_TEST_EXE}
        RUNTIME_OUTPUT_DIRECTORY
            ${ARG_RUNTIME_OUTPUT_DIRECTORY}
        OUT_TEST_REPORT_VARIABLE
            "LOCAL_TEST_REPORT"
    )

    #+-[output]---------------------------------------------------------------+

    if (NOT ARG_OUT_TEST_LIB_VARIABLE STREQUAL "")
        set(${ARG_OUT_TEST_LIB_VARIABLE} ${LOCAL_TEST_LIB} PARENT_SCOPE)
    endif()

    if (NOT ARG_OUT_TEST_EXE_VARIABLE STREQUAL "")
        set(${ARG_OUT_TEST_EXE_VARIABLE} ${LOCAL_TEST_EXE} PARENT_SCOPE)
    endif()

    if (NOT ARG_OUT_TEST_REPORT_VARIABLE STREQUAL "")
        set(${ARG_OUT_TEST_REPORT_VARIABLE} ${LOCAL_TEST_REPORT} PARENT_SCOPE)
    endif()

endfunction(define_native_gtest_unittest_targets)
