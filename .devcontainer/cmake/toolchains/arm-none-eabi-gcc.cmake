#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# armv7m cross-compilation
#
set(CMAKE_C_COMPILER arm-none-eabi-gcc CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER  arm-none-eabi-g++ CACHE FILEPATH "C++ compiler")
set(CMAKE_ASM_COMPILER  arm-none-eabi-gcc CACHE FILEPATH "assembler")

set(CMAKE_SYSTEM_NAME Generic)
# This should be board-specific but we also should have this set by the toolchain. Need to fix then when we add new
# target processors.
set(CMAKE_SYSTEM_PROCESSOR "cortex-m7")

set(TOOLCHAIN_PREFIX arm-none-eabi)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)


# This are compile options for all modules we compile. Architecture-specific stuff should be inherited from the
# board library's compile options since it's that module that defines what CPU we're running on.
add_compile_options(
    -fdata-sections
    -ffunction-sections
    -fverbose-asm
    -mcpu=cortex-m7
    -mthumb
    -mlittle-endian
    -specs=nano.specs
    -specs=nosys.specs
    -nostartfiles
    -save-temps
)

add_link_options(
    LINKER:--print-memory-usage
    --specs=nano.specs
    --specs=nosys.specs
    -nostartfiles
)
