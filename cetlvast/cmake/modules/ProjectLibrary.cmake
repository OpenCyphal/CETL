#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# +---------------------------------------------------------------------------+
# | SUPPORT FOR EXPORTING LIBRARIES AND HEADER FILES
# +---------------------------------------------------------------------------+

# Function: add_project_library
#
# Add and install a library. This method combines all the most common setup for libcyphal library definitions
# including defining install rules. Additional properties, dependencies, etc can be added after this function
# using the library target named by the `NAME` argument you passed in.
#
# By default, the library will be static, unless you pass the SHARED option.
# If no sources are passed in, and neither SHARED nor STATIC is specified, a cmake interface library will be defined
# with just headers.
#
# ## Using The HEADER_PATH Argument
#
# Given the following structure:
#
# ```
# foo/
#   + include/
#       + bar/
#           + bar.h
# ```
#
# ... if HEADER_PATH is `${CMAKE_CURRENT_SOURCE_DIR}/foo/include` then the output will be:
#
# ```
# ${CMAKE_INSTALL_PREFIX}/
#     + include/
#        + include/
#           + bar/
#              + bar.h
# ```
#
# ... which is probably not what you want. Instead do `${CMAKE_CURRENT_SOURCE_DIR}/foo/include/` (notice the trailing slash):
#
# ```
# ${CMAKE_INSTALL_PREFIX}/
#     + include/
#        + bar/
#           + bar.h
# ```
#
# ... which means consumers would include bar.h as `include "bar/bar.h"`. If the prefix `bar` should be omitted
# define HEADER_PATH as `${CMAKE_CURRENT_SOURCE_DIR}/include/bar/` which results in this:
#
# ```
# ${CMAKE_INSTALL_PREFIX}/
#     + include/
#         + bar.h
# ```
#
# Usage:
#   > add_project_library(NAME <library_name> SOURCES <file>... HEADER_PATH <path> HEADER_INCLUDE_PATTERNS <string>... [LIBRARIES <library>...] [FPIC] [STATIC|SHARED])
#
# Parameters:
#   NAME                    - what to call your library (do not include prefixes (e.g. "lib") or suffixes (e.g. ".a"))
#   SOURCES                 - list of files to build into your library
#   HEADER_PATH             - Directory under which all headers for the library reside.
#   HEADER_INCLUDE_PATTERNS - List of posix file glob expressions to use to include files found under HEADER_PATH. If omitted
#                             a default set of patterns are used (e.g. *.hpp). Note, this has no effect on files available to
#                             the local build.
#   LIBRARIES               - list of additional libraries to link against.
#
# Options:
#   STATIC - create a static archive
#   SHARED - create a shared library
#   FPIC   - Build position-independent code.
#
function(add_project_library)
    # parse the arguments provided to add_project_library (see cmake_parse_arguments docs)
    set(options STATIC SHARED FPIC)
    set(singleValueArgs NAME HEADER_PATH)
    set(multiValueArgs SOURCES HEADER_INCLUDE_PATTERNS LIBRARIES)
    cmake_parse_arguments(PARSE_ARGV 0 LOCAL "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if(NOT LOCAL_NAME)
        message(FATAL_ERROR "You must provide a name!")
    endif()

    cmake_path(IS_ABSOLUTE LOCAL_HEADER_PATH HEADER_PATH_IS_ABSOLUTE)

    if(NOT HEADER_PATH_IS_ABSOLUTE)
        set(LOCAL_HEADER_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${LOCAL_HEADER_PATH}")
    endif()

    # Cmake doesn't have block scope but setting them before the if branches below
    # makes the easier to read.
    set(LOCAL_LIBRARY_INSTALL_TYPE "")
    set(LOCAL_LIBRARY_SCOPE "PUBLIC")

    if(LOCAL_SHARED)
        # We are creating a shared library. Neat.
        set(LOCAL_LIBRARY_INSTALL_TYPE "LIBRARY")
        if(LOCAL_STATIC)
            message(FATAL_ERROR "SHARED defined. Cannot define both SHARED and STATIC using this function.")
        endif()
        add_library("${LOCAL_NAME}" SHARED ${LOCAL_SOURCES})
    else()
        # We are creating some sort of archive. Either an object archive (.a) or a cmake interface archive.
        set(LOCAL_LIBRARY_INSTALL_TYPE "ARCHIVE")
        if(NOT LOCAL_SOURCERS AND NOT LOCAL_STATIC)
            # This is a cmake concept only but it allows us to define a group of headers along with the
            # dependencies you'll need to use the headers and some of the properties you should inherit
            # when building objects that include these headers. No .a nor .so file will be produced for
            # these, however.
            message(DEBUG "Neither SHARED nor STATIC defined and no sources provided to add_project_library(${LOCAL_NAME}). Using default of INTERFACE.")
            add_library("${LOCAL_NAME}" INTERFACE)
            set(LOCAL_LIBRARY_SCOPE "INTERFACE")
        else()
            # Good old fashioned object archive!
            add_library("${LOCAL_NAME}" STATIC ${LOCAL_SOURCES})
        endif()
    endif()

    set(LOCAL_HEADERS_DESTINATION "include")

    if (NOT LOCAL_HEADER_INCLUDE_PATTERNS)
        list(APPEND LOCAL_HEADER_INCLUDE_PATTERNS "*.h")
        list(APPEND LOCAL_HEADER_INCLUDE_PATTERNS "*.hpp")
    endif()

    # Recursively search for include files under LOCAL_HEADER_PATH
    foreach(LOCAL_PATTERN ${LOCAL_HEADER_INCLUDE_PATTERNS})
        install(
            DIRECTORY
                ${LOCAL_HEADER_PATH}
            DESTINATION
                ${LOCAL_HEADERS_DESTINATION}
            FILES_MATCHING PATTERN
                ${LOCAL_PATTERN}
        )
   endforeach()

    # Other options that are common enough we do it here. Anything not in this section can be done
    # after this function exits using set_property
    if (LOCAL_FPIC)
        set_property(TARGET "${LOCAL_NAME}" PROPERTY POSITION_INDEPENDENT_CODE ON)
    endif()

    target_link_libraries("${LOCAL_NAME}" ${LOCAL_LIBRARY_SCOPE} ${LOCAL_LIBRARIES})

    # TODO: support multiple include paths using $<SEMICOLON>
    target_include_directories("${LOCAL_NAME}" ${LOCAL_LIBRARY_SCOPE}
        $<BUILD_INTERFACE:${LOCAL_HEADER_PATH}>
        $<INSTALL_INTERFACE:${LOCAL_HEADERS_DESTINATION}>
    )

    # Now setup the installation targets. Of course, these don't run at configuration time
    # like everything else in this function does. Note that the EXPORT argument
    # is a key to group all install calls together into a single export package. So, each
    # call with the same key appends more export work.
    install(
            TARGETS "${LOCAL_NAME}"
            EXPORT "${LOCAL_NAME}-export"
            ${LOCAL_LIBRARY_INSTALL_TYPE}
            DESTINATION lib
        )

    # We also export ${LOCAL_NAME}-export to support find_package/find_library
    # in dependant packages. See
    # https://cmake.org/cmake/help/latest/guide/tutorial/Adding%20Export%20Configuration.html
    install(EXPORT "${LOCAL_NAME}-export"
            DESTINATION include/cmake
            FILE ${LOCAL_NAME}.cmake)

endfunction(add_project_library)
