#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
# cSpell: words fprofile fcoverage gcov tracefile objdir gcno gcda objlib tracefiles

find_program(GCOVR gcovr)
find_program(GCOV gcov)
find_program(LLVMCOV llvm-cov)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(gcovr
    REQUIRED_VARS GCOVR
)

if(NOT COMMAND enable_instrumentation)

message(STATUS
"[ gcovr ]-----------------------------------------------\n\
    GCOVR:                      ${GCOVR}\n\
    GCOV:                       ${GCOV}\n\
    LLVMCOV:                    ${LLVMCOV}\n\
-----------------------------------------------------------\n\
")

# +---------------------------------------------------------------------------+
# | Contributed helpers for enabling and processing coverage data
# +---------------------------------------------------------------------------+
define_property(DIRECTORY
    PROPERTY GCOV_TRACE_FILES
    BRIEF_DOCS "private collection used by gcovr module."
    FULL_DOCS "private collection used by gcovr module."
)

endif()

# function: enable_instrumentation
# Sets well-known compiler flags for gcc and/or clang to insert instrumentations
# into binaries that generate coverage data.
#
# param: TARGET target - The target to set compile and link options on.
#
function(enable_instrumentation)
    #+-[input]----------------------------------------------------------------+
    set(options)
    set(singleValueArgs TARGET)
    set(multiValueArgs)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    #+-[body]-----------------------------------------------------------------+
    target_compile_options(${ARG_TARGET}
        PRIVATE
            $<$<CONFIG:Coverage>:--coverage>
            $<$<AND:$<CONFIG:Coverage>,$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>>:-fprofile-instr-generate>
            $<$<AND:$<CONFIG:Coverage>,$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>>:-fcoverage-mapping>
            $<$<AND:$<CONFIG:Coverage>,$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>>:-ftest-coverage> # Create a GCNO file.
            $<$<AND:$<CONFIG:Coverage>,$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>>:-fprofile-arcs>
            $<$<AND:$<CONFIG:Coverage>,$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang>>:-fcoverage-mapping>
    )
    target_link_options(${ARG_TARGET}
        PRIVATE
            $<$<CONFIG:Coverage>:--coverage>
    )
endfunction(enable_instrumentation)

#
# function: define_gcovr_tracefile_target
#
# param: TARGET target      - The target that will run to produce TARGET_EXECUTION_DEPENDS.
#                             While TARGET_EXECUTION_DEPENDS is used as the dependency of
#                             the tracefile target created by this function the target itself
#                             is queried for properties that help filter the coverage data.
# param: ROOT_DIRECTORY string              - The root directory of the source to be covered.
# param: TARGET_EXECUTION_DEPENDS target    - A target that will be a dependency of the tracefile
#                                             target defined by this function.
# param: EXCLUDES list[target]              - A list of targets to exclude from the coverage data.
#                                             This relies on the target property SOURCE_DIR which may not be stable
#                                             when including this method from another repository.
# param: EXCLUDE_PATHS list[path]           - A list of paths to exclude from the coverage data.
# param: OBJECT_LIBRARY target              - An object library annotated with gcno and gcda paths
#                                             as a POST_BUILD_INSTRUMENTATION_BYPRODUCTS property.
# option: EXCLUDE_TARGET                    - If set the target itself will be excluded from the
#                                             coverage data.
# option: EXCLUDE_TEST_FRAMEWORKS           - If set the test frameworks will be excluded from the
#                                             coverage data. This uses the custom TEST_FRAMEWORK_LINK_LIBRARIES
#                                             property to determine the test frameworks.
# options: ENABLE_INSTRUMENTATION           - If specified the target and object library will have necessary
#                                             compile and link options added to enable coverage data instrumentation.
#                                             If not set the assumption is that the target and object library
#                                             have already been configured to generate coverage data by some other
#                                             means.
# param: OUT_TRACEFILE_VARIABLE string      - The name of a variable to set to the tracefile
#                                             that the custom command defined by this function
#                                             will generate.
#
function(define_gcovr_tracefile_target)
    #+-[input]----------------------------------------------------------------+
    set(options EXCLUDE_TARGET EXCLUDE_TEST_FRAMEWORKS ENABLE_INSTRUMENTATION)
    set(singleValueArgs ROOT_DIRECTORY TARGET TARGET_EXECUTION_DEPENDS OBJECT_LIBRARY OUT_TRACEFILE_VARIABLE)
    set(multiValueArgs EXCLUDES EXCLUDE_PATHS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_ROOT_DIRECTORY)
        set(ARG_ROOT_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()
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
    set(LOCAL_EXCLUDE_PATHS ${ARG_EXCLUDE_PATHS})

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

    list(APPEND LOCAL_GCOV_EXE_ARGS "--gcov-executable")
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        if (NOT LLVMCOV)
            message(WARNING "llvm-cov was not found. Coverage reporting for ${CMAKE_CXX_COMPILER_ID} will probably fail.")
        else()
            list(APPEND LOCAL_GCOV_EXE_ARGS "${LLVMCOV} gcov")
        endif()
    else()
        if (NOT GCOV)
            message(WARNING "gcov was not found. Coverage reporting for ${CMAKE_CXX_COMPILER_ID} will probably fail.")
        else()
            list(APPEND LOCAL_GCOV_EXE_ARGS ${GCOV})
        endif()
    endif()

    set(LOCAL_OBJDIR_ARGS )
    if (ARG_OBJECT_LIBRARY)
        get_target_property(LOCAL_OBJECT_LIB_POST_BUILD_INSTRUMENTATION_BYPRODUCTS ${ARG_OBJECT_LIBRARY} POST_BUILD_INSTRUMENTATION_BYPRODUCTS)

        if (NOT LOCAL_OBJECT_LIB_POST_BUILD_INSTRUMENTATION_BYPRODUCTS MATCHES ".*-NOTFOUND$")
            list(POP_FRONT LOCAL_OBJECT_LIB_POST_BUILD_INSTRUMENTATION_BYPRODUCTS LOCAL_OBJLIB_FILE)
            cmake_path(GET LOCAL_OBJLIB_FILE PARENT_PATH LOCAL_OBJLIB_DIR)
            list(APPEND LOCAL_OBJDIR_ARGS "--object-directory")
            list(APPEND LOCAL_OBJDIR_ARGS "\"${LOCAL_OBJLIB_DIR}\"")
            set(LOCAL_TARGET_RUNTIME_DIR ${LOCAL_OBJLIB_DIR})
        endif()
    endif()

    add_custom_command(
        COMMAND # Generate tracefile from tests.
            ${GCOVR}
                ${LOCAL_GCOV_EXE_ARGS}
                --root ${ARG_ROOT_DIRECTORY}
                --json
                --output ${LOCAL_TRACEFILE_PATH}
                ${LOCAL_EXCLUDE_ARGUMENTS}
                ${LOCAL_OBJDIR_ARGS}
                ${LOCAL_TARGET_RUNTIME_DIR}
        WORKING_DIRECTORY ${LOCAL_TARGET_RUNTIME_DIR}
        OUTPUT ${LOCAL_TRACEFILE_PATH}
        DEPENDS ${ARG_TARGET_EXECUTION_DEPENDS}
    )

    if (${ARG_ENABLE_INSTRUMENTATION})
        enable_instrumentation(TARGET ${ARG_TARGET})
        enable_instrumentation(TARGET ${ARG_OBJECT_LIBRARY})
    endif()

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
# function: enable_coverage_report - Creates a custom target that will generate a html report using gcovr
# for the current directory. Be sure to call this only after all calls to define_gcovr_tracefile_target
# have been made.
#
# param: COVERAGE_REPORT_FORMATS - Supports html or sonarqube
# param: ROOT_DIRECTORY string - The root directory of the source to be covered.
# param: OUT_REPORT_INDICES list[string] - The name of a variable to set to a list of index files of the reports.
#
function (enable_coverage_report)
    #+-[input]----------------------------------------------------------------+
    set(options "")
    set(singleValueArgs OUT_REPORT_INDICES ROOT_DIRECTORY)
    set(multiValueArgs COVERAGE_REPORT_FORMATS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    get_property(LOCAL_TRACEFILES DIRECTORY ${CMAKE_CURRENT_BINARY_DIR} PROPERTY GCOV_TRACE_FILES)

    if (NOT ARG_ROOT_DIRECTORY)
        set(ARG_ROOT_DIRECTORY ${CMAKE_SOURCE_DIR})
    endif()
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

    foreach(LOCAL_REPORT_FORMAT ${ARG_COVERAGE_REPORT_FORMATS})
        set(LOCAL_FORMAT_ARGS)
        if (LOCAL_REPORT_FORMAT STREQUAL "html")
            set(LOCAL_REPORT_INDEX "gcovr_html/coverage.html")
            file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/gcovr_html")
            list(APPEND LOCAL_FORMAT_ARGS "--html-details")
            list(APPEND LOCAL_FORMAT_ARGS "${LOCAL_REPORT_INDEX}")
        elseif (LOCAL_REPORT_FORMAT STREQUAL "sonarqube")
            set(LOCAL_REPORT_INDEX "coverage.xml")
            list(APPEND LOCAL_FORMAT_ARGS "--sonarqube")
            list(APPEND LOCAL_FORMAT_ARGS "${LOCAL_REPORT_INDEX}")
        else()
            message(FATAL_ERROR "${LOCAL_REPORT_FORMAT} is not a supported coverage report format.")
        endif()
        list(APPEND LOCAL_REPORT_INDICES ${LOCAL_REPORT_INDEX})

        add_custom_command(
            OUTPUT ${LOCAL_REPORT_INDEX}
            COMMAND
                ${GCOVR}
                    ${LOCAL_ALL_GCOV_ARGS}
                    --root ${ARG_ROOT_DIRECTORY}
                    ${LOCAL_FORMAT_ARGS}
            WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
            DEPENDS ${LOCAL_ALL_TRACEFILES}
        )

        add_custom_target(
            gcovr_${LOCAL_REPORT_FORMAT}_report_for_${LOCAL_DIRECTORY_NAME}
            DEPENDS ${LOCAL_REPORT_INDEX}
        )

    endforeach()

    #+-[output]---------------------------------------------------------------+

    if (NOT ARG_OUT_REPORT_INDICES STREQUAL "")
        set(${ARG_OUT_REPORT_INDICES} "${LOCAL_REPORT_INDICES}" PARENT_SCOPE)
    endif()

endfunction(enable_coverage_report)
