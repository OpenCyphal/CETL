# Copyright (C) OpenCyphal Development Team <opencyphal.org>
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 3.22.0)

# Also support a small, dependency-free build: cmake -S cetlvast/suites/fuzz -B cetlvast/build_fuzz.
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(cetlvast_fuzz CXX)
    set(CETLVAST_CPP_STANDARD "14" CACHE STRING "C++ standard to use when compiling.")
    set(CMAKE_CXX_STANDARD ${CETLVAST_CPP_STANDARD})
    set(CMAKE_CXX_STANDARD_REQUIRED ON)
    set(CMAKE_CXX_EXTENSIONS OFF)
    add_compile_definitions(CETL_ENABLE_DEBUG_ASSERT=1)
    add_library(cetl INTERFACE)
    target_include_directories(cetl INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/../../../include")
    enable_testing()
endif()

if(CMAKE_CROSSCOMPILING OR NOT CMAKE_CXX_COMPILER_ID MATCHES "^(Apple)?Clang$")
    message(FATAL_ERROR "CETL fuzz tests require native Clang with libFuzzer, ASan, and UBSan.")
endif()

# Check the link as well as compilation; some Clang distributions omit the fuzzer runtime.
include(CheckCXXSourceCompiles)
include(CMakePushCheckState)
cmake_push_check_state(RESET)
set(CMAKE_REQUIRED_FLAGS "-fsanitize=fuzzer,address,undefined")
check_cxx_source_compiles("
    #include <cstddef>
    #include <cstdint>
    extern \"C\" int LLVMFuzzerTestOneInput(const std::uint8_t*, std::size_t) { return 0; }
" CETLVAST_HAS_LIBFUZZER)
cmake_pop_check_state()
if(NOT CETLVAST_HAS_LIBFUZZER)
    message(FATAL_ERROR "Clang cannot link -fsanitize=fuzzer,address,undefined; install the compiler-rt runtimes.")
endif()

# Each target is a libFuzzer entry point with a seed corpus in corpus/<name>; generated inputs and crash artifacts
# stay in the build tree so the seed corpus is left unchanged.
set(FUZZ_TARGETS
    variable_length_array
    saturation_arithmetic
)
set(FUZZ_SMOKE_COMMANDS "")
foreach(FUZZ_NAME ${FUZZ_TARGETS})
    set(FUZZ_TARGET "fuzz_${FUZZ_NAME}")
    add_executable(${FUZZ_TARGET} ${FUZZ_TARGET}.cpp)
    target_link_libraries(${FUZZ_TARGET} PRIVATE cetl)
    target_compile_options(${FUZZ_TARGET} PRIVATE
        -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all
        -Wall -Wextra -Wpedantic -Werror)
    target_link_options(${FUZZ_TARGET} PRIVATE
        -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)
    set(FUZZ_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/${FUZZ_NAME}")
    file(MAKE_DIRECTORY "${FUZZ_WORK_DIR}/corpus" "${FUZZ_WORK_DIR}/artifacts")
    set(FUZZ_SMOKE_ARGS
        "${FUZZ_WORK_DIR}/corpus" "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${FUZZ_NAME}"
        -runs=10000 -seed=1 -max_len=1024 -timeout=10
        "-artifact_prefix=${FUZZ_WORK_DIR}/artifacts/")
    add_test(NAME ${FUZZ_TARGET}_smoke COMMAND ${FUZZ_TARGET} ${FUZZ_SMOKE_ARGS})
    set_tests_properties(${FUZZ_TARGET}_smoke PROPERTIES LABELS fuzz TIMEOUT 120)
    list(APPEND FUZZ_SMOKE_COMMANDS COMMAND $<TARGET_FILE:${FUZZ_TARGET}> ${FUZZ_SMOKE_ARGS})
endforeach()
add_custom_target(run_fuzztests
    ${FUZZ_SMOKE_COMMANDS}
    DEPENDS fuzz_variable_length_array fuzz_saturation_arithmetic
    USES_TERMINAL VERBATIM)
