#
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

cmake_minimum_required(VERSION 3.22.0)

# +---------------------------------------------------------------------------+
# | PROJECT
# +---------------------------------------------------------------------------+

project(cetlvast_docs C CXX)
add_subdirectory(examples)

# +---------------------------------------------------------------------------+
# | DOXYGEN
# +---------------------------------------------------------------------------+

find_package(Doxygen)
find_program(TAR tar)

# :function: create_docs_target
# Create a target that generates documentation.
#
# :param str        DOCS_DOXY_ROOT:             The folder under which all doxygen configuration, templates,
#                                               styles, and examples can be found.
# :param str        OPEN_CYPHAL_PROJECT_NAME:   The name of the org.opencyphal project this documentation is for.
# :param str        OPEN_CYPHAL_PROJECT_VERSION A string that contains the version of the cyphal project to display
#                                               in the generated documentation.
# :param str        OPEN_CYPHAL_PROJECT_BRIEF:  A one-line description of the project to be included in the generated
#                                               documents.
# :param path       EXAMPLES_PATH:              A path to example code to be included in the docs.
# :param path       MAIN_PAGE_PATH:             A file to use as the main page (e.g. ${PROJECT_ROOT}/README.md).
# :param path       INCLUDE_PREFIX_STRIP:       A path to strip from the beginning of all include paths.
# :param list[path] INPUT_LIST                  A list of source files to generate documentation for.
# :param list[str]  DEPENDS:                    A list of dependencies the docs build should take.
# :option W_ERROR                               If specified then warnings are treated as errors.
#
# :param[out] target: OUT_DOC_BUILD_TARGET:     Set to the name of a variable, in the parent scope, to a build target
#                                               that will generate the docs.
# :param[out] target: OUT_DOC_TAR_TARGET:       Set to the name of a variable, in the parent scope, to a build target
#                                               that will create a tarball of the docs after they are generated.
function (create_docs_target)

    #+-[input]----------------------------------------------------------------+
    set(options W_ERROR)
    set(singleValueArgs DOCS_DOXY_ROOT
                        OPEN_CYPHAL_PROJECT_NAME
                        OPEN_CYPHAL_PROJECT_VERSION
                        OPEN_CYPHAL_PROJECT_BRIEF
                        EXAMPLES_PATH
                        MAIN_PAGE_PATH
                        INCLUDE_PREFIX_STRIP
                        OUT_DOC_BUILD_TARGET
                        OUT_DOC_TAR_TARGET)
    set(multiValueArgs  INPUT_LIST
                        DEPENDS)
    cmake_parse_arguments(PARSE_ARGV 0 ARG "${options}" "${singleValueArgs}" "${multiValueArgs}")

    if (NOT ARG_RUNTIME_OUTPUT_DIRECTORY)
        set(ARG_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    endif()

    #+-[body]-----------------------------------------------------------------+
    set(DOXYGEN_SOURCE ${ARG_DOCS_DOXY_ROOT})
    set(DOXYGEN_RDOMAIN org.opencyphal)
    set(DOXYGEN_RDOMAIN_W_PROJECT org.opencyphal.${ARG_OPEN_CYPHAL_PROJECT_NAME})
    set(DOXYGEN_PROJECT_NAME "${ARG_OPEN_CYPHAL_PROJECT_NAME}")
    set(DOXYGEN_PROJECT_VERSION "${ARG_OPEN_CYPHAL_PROJECT_VERSION}")
    set(DOXYGEN_PROJECT_BRIEF "${ARG_OPEN_CYPHAL_PROJECT_BRIEF}")
    set(DOXYGEN_PROJECT_INCLUDE_PREFIX_STRIP "${ARG_INCLUDE_PREFIX_STRIP}")
    if (${ARG_W_ERROR})
        set(DOXYGEN_WARN_AS_ERROR "YES")
    else()
        set(DOXYGEN_WARN_AS_ERROR "NO")
    endif()
    set(DOXYGEN_OUTPUT_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set(DOXYGEN_CONFIG_FILE ${DOXYGEN_OUTPUT_DIRECTORY}/doxygen.config)
    set(DOXYGEN_EXAMPLE_PATH ${ARG_EXAMPLES_PATH})

    list(APPEND DOXYGEN_INPUT_LIST ${ARG_INPUT_LIST})
    list(JOIN DOXYGEN_INPUT_LIST "\\\n    " DOXYGEN_INPUT )
    set(DOXYGEN_MAINPAGE "${ARG_MAIN_PAGE_PATH}")
    set(DOXYGEN_OUTPUT_TAGFILE "${DOXYGEN_OUTPUT_DIRECTORY}/${ARG_OPEN_CYPHAL_PROJECT_NAME}.tag.xml")
    set(DOXYGEN_COMPILE_COMMANDS_DIR ${CMAKE_BINARY_DIR})

    # +-----------------------------------------------------------------------+
    # | HTML
    # +-----------------------------------------------------------------------+
    set(DOXYGEN_HTML_EXTRA_FILES "${DOXYGEN_SOURCE}/.nojekyll")
    set(DOXYGEN_HTML_STYLESHEET ${DOXYGEN_OUTPUT_DIRECTORY}/stylesheet.css)
    set(DOXYGEN_HTML_EXTRA_STYLESHEET ${DOXYGEN_OUTPUT_DIRECTORY}/doxygen-flat-style.css)
    set(DOXYGEN_HTML_HEADER ${DOXYGEN_OUTPUT_DIRECTORY}/header.html)
    set(DOXYGEN_HTML_FOOTER ${DOXYGEN_OUTPUT_DIRECTORY}/footer.html)
    set(DOXYGEN_IMAGE_PATH ${DOXYGEN_SOURCE}/images)
    set(DOXYGEN_LOGO ${DOXYGEN_SOURCE}/images/html/opencyphal_logo.svg)
    set(DOXYGEN_TAGFILES "${DOXYGEN_SOURCE}/cppreference-doxygen-web.tag.xml=http://en.cppreference.com/w/")

    # a bit of a hack, but we copy all the images to the output to allow us to serve them
    # for anything other that the docs that wants them.
    file(COPY ${DOXYGEN_SOURCE}/images/html DESTINATION ${DOXYGEN_OUTPUT_DIRECTORY})

    configure_file(${DOXYGEN_SOURCE}/header.html
                    ${DOXYGEN_OUTPUT_DIRECTORY}/header.html
                )
    configure_file(${DOXYGEN_SOURCE}/footer.html
                    ${DOXYGEN_OUTPUT_DIRECTORY}/footer.html
                )
    configure_file(${DOXYGEN_SOURCE}/stylesheet.css
                    ${DOXYGEN_OUTPUT_DIRECTORY}/stylesheet.css
                )
    configure_file(${DOXYGEN_SOURCE}/doxygen-flat-style.css
                    ${DOXYGEN_OUTPUT_DIRECTORY}/doxygen-flat-style.css
                )
    configure_file(${DOXYGEN_SOURCE}/doxygen.ini
                    ${DOXYGEN_CONFIG_FILE}
                )
    add_custom_command(OUTPUT ${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html
                        COMMAND ${DOXYGEN_EXECUTABLE} ${DOXYGEN_CONFIG_FILE}
                        DEPENDS
                            ${ARG_DEPENDS}
                            ${DOXYGEN_CONFIG_FILE}
                            ${DOXYGEN_INPUT_LIST}
                        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
                        COMMENT "Generating intermediate documentation."
                    )
    add_custom_target(generate_${ARG_OPEN_CYPHAL_PROJECT_NAME}_docs DEPENDS ${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html)

    if (TAR)
        add_custom_command(OUTPUT ${DOXYGEN_OUTPUT_DIRECTORY}/html.gz
                        COMMAND ${TAR} -vzcf html.gz ./html
                        DEPENDS ${DOXYGEN_OUTPUT_DIRECTORY}/html/index.html
                        WORKING_DIRECTORY ${DOXYGEN_OUTPUT_DIRECTORY}
                        COMMENT "Creating html tarball."
                        )

        add_custom_target(archive_${ARG_OPEN_CYPHAL_PROJECT_NAME}_docs DEPENDS ${DOXYGEN_OUTPUT_DIRECTORY}/html.gz)
    endif()

     #+-[output]---------------------------------------------------------------+

    if(NOT ARG_OUT_DOC_BUILD_TARGET STREQUAL "")
        set(${ARG_OUT_DOC_BUILD_TARGET} generate_${ARG_OPEN_CYPHAL_PROJECT_NAME}_docs PARENT_SCOPE)
    endif()

    if(NOT ARG_OUT_DOC_TAR_TARGET STREQUAL "")
        set(${ARG_OUT_DOC_TAR_TARGET} archive_${ARG_OPEN_CYPHAL_PROJECT_NAME}_docs PARENT_SCOPE)
    endif()

endfunction(create_docs_target)


if (${Doxygen_FOUND})

file(GLOB_RECURSE DOXYGEN_INPUT_LIST
    LIST_DIRECTORIES false
    CONFIGURE_DEPENDS
    ${CETL_ROOT}/include/**/*.hpp
)

get_property(LOCAL_EXAMPLES DIRECTORY "examples" PROPERTY IN_BUILD_TESTS)

list(TRANSFORM LOCAL_EXAMPLES PREPEND "examples/")

create_docs_target( DOCS_DOXY_ROOT ${CMAKE_CURRENT_SOURCE_DIR}
                    OPEN_CYPHAL_PROJECT_NAME "CETL"
                    OPEN_CYPHAL_PROJECT_VERSION ${CYPHAL_PROJECT_VERSION}
                    OPEN_CYPHAL_PROJECT_NAME_BRIEF
                        "Cyphal Embedded Template Library"
                    EXAMPLES_PATH
                        ${CMAKE_CURRENT_SOURCE_DIR}/examples
                    MAIN_PAGE_PATH
                        "${CETL_ROOT}/README.md"
                    INCLUDE_PREFIX_STRIP
                        "\"${CETL_ROOT}/include\""
                    INPUT_LIST
                        ${DOXYGEN_INPUT_LIST}
                    DEPENDS
                        ${LOCAL_EXAMPLES}
                    W_ERROR
)

else()
message(STATUS "Doxygen was not found. Documentation targets will not be enabled.")

endif()
