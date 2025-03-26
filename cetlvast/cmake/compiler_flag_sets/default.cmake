#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# C, CXX, LD, and AS flags for building native unit tests. These flags also include
# instrumentation for code coverage.
#

set(C_FLAG_SET )
set(EXE_LINKER_FLAG_SET )
set(DEFINITIONS_SET )

#
# Diagnostics for C and C++
#
list(APPEND C_FLAG_SET
                "-pedantic"
                "-Wall"
                "-Wextra"
                "-Werror"
                "-Wfloat-equal"
                "-Wconversion"
                "-Wunused-parameter"
                "-Wunused-variable"
                "-Wunused-value"
                "-Wcast-align"
                "-Wmissing-declarations"
                "-Wmissing-field-initializers"
                "-Wdouble-promotion"
                "-Wswitch-enum"
                "-Wtype-limits"
                "-Wno-error=array-bounds"
                $<$<CONFIG:Release>:-O3>
                $<$<CONFIG:ReleaseEP>:-Os>
                $<$<CONFIG:Coverage>:-O0>
                $<$<CONFIG:Release,ReleaseEP>:-fno-delete-null-pointer-checks> # https://github.com/OpenCyphal-Garage/libcyphal/pull/347#discussion_r1572254288
                $<$<COMPILE_LANGUAGE:ASM>:-x$<SEMICOLON>assembler-with-cpp>
                $<$<NOT:$<CONFIG:Release,ReleaseEP>>:-O0>
                $<$<NOT:$<CONFIG:Release,ReleaseEP>>:-DDEBUG>
                $<$<NOT:$<CONFIG:Release,ReleaseEP>>:-ggdb>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:ReleaseEP,DebugEP>>:-fno-exceptions>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:ReleaseEP,DebugEP>>:-fno-rtti>
                $<$<AND:$<COMPILE_LANGUAGE:CXX>,$<CONFIG:ReleaseEP,DebugEP>>:-fno-use-cxa-atexit>
)

set(CXX_FLAG_SET ${C_FLAG_SET})
set(ASM_FLAG_SET ${C_FLAG_SET})

#
# C++ only diagnostics
#
list(APPEND CXX_FLAG_SET
                "-Wsign-conversion"
                "-Wsign-promo"
                "-Wold-style-cast"
                "-Wzero-as-null-pointer-constant"
                "-Wnon-virtual-dtor"
                "-Woverloaded-virtual"
)

if (DEFINED CETLVAST_TARGET_PLATFORM AND (CETLVAST_TARGET_PLATFORM STREQUAL "native32"))
    message(STATUS "CETLVAST_TARGET_PLATFORM is native32. Adding -m32 to compiler flags.")
    list(APPEND C_FLAG_SET "-m32")
    list(APPEND EXE_LINKER_FLAG_SET "-m32")
endif()

list(APPEND CXX_FLAG_SET ${C_FLAG_SET})
list(APPEND ASM_FLAG_SET ${C_FLAG_SET})

add_compile_options("$<$<COMPILE_LANGUAGE:C>:${C_FLAG_SET}>")
add_compile_options("$<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAG_SET}>")
add_compile_options("$<$<COMPILE_LANGUAGE:ASM>:${ASM_FLAG_SET}>")
add_link_options(${EXE_LINKER_FLAG_SET})
add_definitions(${DEFINITIONS_SET})
