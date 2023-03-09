#
# Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
#


#
# Finds programs needed to build the CETL documentation
#
find_package(docs REQUIRED)

create_docs_target(docs OFF)
