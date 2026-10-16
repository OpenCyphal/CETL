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

add_executable(fuzz_variable_length_array fuzz_variable_length_array.cpp)
target_link_libraries(fuzz_variable_length_array PRIVATE cetl)
target_compile_options(fuzz_variable_length_array PRIVATE
    -O1 -g -fno-omit-frame-pointer -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all
    -Wall -Wextra -Wpedantic -Werror)
target_link_options(fuzz_variable_length_array PRIVATE
    -fsanitize=fuzzer,address,undefined -fno-sanitize-recover=all)

# Keep generated inputs and crash artifacts in the build tree, leaving the seed corpus unchanged.
set(FUZZ_WORK_DIR "${CMAKE_CURRENT_BINARY_DIR}/variable_length_array")
file(MAKE_DIRECTORY "${FUZZ_WORK_DIR}/corpus" "${FUZZ_WORK_DIR}/artifacts")
set(FUZZ_SMOKE_ARGS
    "${FUZZ_WORK_DIR}/corpus" "${CMAKE_CURRENT_SOURCE_DIR}/corpus/variable_length_array"
    -runs=10000 -seed=1 -max_len=1024 -timeout=10
    "-artifact_prefix=${FUZZ_WORK_DIR}/artifacts/")
add_test(NAME fuzz_variable_length_array_smoke COMMAND fuzz_variable_length_array ${FUZZ_SMOKE_ARGS})
set_tests_properties(fuzz_variable_length_array_smoke PROPERTIES LABELS fuzz TIMEOUT 120)
add_custom_target(run_fuzztests
    COMMAND $<TARGET_FILE:fuzz_variable_length_array> ${FUZZ_SMOKE_ARGS}
    DEPENDS fuzz_variable_length_array
    USES_TERMINAL VERBATIM)
