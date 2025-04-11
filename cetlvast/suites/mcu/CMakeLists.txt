#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#
cmake_minimum_required(VERSION 3.22.0)

project(cetlvast_mcu_tests CXX)

add_executable(test_text
     ${CMAKE_CURRENT_SOURCE_DIR}/test_text.cpp
     ${CMAKE_CURRENT_SOURCE_DIR}/dummysys.c
)

target_include_directories(test_text PUBLIC ${CETL_ROOT}/include)
set_target_properties(test_text PROPERTIES SUFFIX ".elf")


target_link_options(test_text PUBLIC
     "-T" "${CMAKE_CURRENT_SOURCE_DIR}/samd21g18a.ld"
     "LINKER:-Map=${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/test_text.map,--cref"
)

add_custom_command(OUTPUT $<CONFIG>/test_text-disassembly.S
     DEPENDS test_text
     COMMAND ${CMAKE_OBJDUMP} -d $<TARGET_FILE:test_text>
     -marm
     --demangle
     --disassemble-zeroes
     --disassembler-options=reg-names-std
     --syms
     --special-syms
     --all-headers
     --wide > $<CONFIG>/test_text-disassembly.S
     COMMENT "Creating disassembly from test_text.elf"
)

add_custom_target(test_text-disassembly DEPENDS $<CONFIG>/test_text-disassembly.S)

add_custom_target(
     build_mcu_tests
     DEPENDS test_text
)
