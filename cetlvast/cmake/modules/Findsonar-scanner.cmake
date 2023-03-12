#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

#
# Find sonar-scanner.
#

find_program(SONAR_SCANNER sonar-scanner)

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(sonar-scanner
    REQUIRED_VARS SONAR_SCANNER
)
