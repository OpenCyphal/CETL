#
# Copyright (C) OpenCyphal Development Team  <opencyphal.org>
# Copyright Amazon.com Inc. or its affiliates.
# SPDX-License-Identifier: MIT
#

# +---------------------------------------------------------------------------+
# | STYLE
# +---------------------------------------------------------------------------+
find_package(clangformat REQUIRED)

create_check_style_target(format-check ${CETLVAST_STYLE_CHECK} "${CETL_INCLUDE}/**/*.h")
