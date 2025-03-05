#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# Toolchain for using gcc on what-ever-platform-this-is (aka "native").
#
set(CMAKE_C_COMPILER gcc-7 CACHE FILEPATH "C compiler")
set(CMAKE_CXX_COMPILER g++-7 CACHE FILEPATH "C++ compiler")
set(CMAKE_ASM_COMPILER gcc-7 CACHE FILEPATH "assembler")
