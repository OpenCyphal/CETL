/*
 * Copyright 2023 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 */
#include "cetl/cetl.h"
#include "cetl/span.h"

#ifndef CETLVAST_COMPILETEST_PRECHECK
static_assert(__cplusplus >= CETL_CPP_STANDARD_17, "We simply pass this test (i.e. fail to compile) for C++14");
#endif

int main()
{
    cetl::span<int,0> subject;
#ifndef CETLVAST_COMPILETEST_PRECHECK
    subject.empty(); // this should fail because nodiscard is available
#endif
    return 0;
}
