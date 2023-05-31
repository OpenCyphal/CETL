/// @file
/// CETL VerificAtion SuiTe – Enables coverage data from forked death tests
///
/// @copyright
/// Copyright (C) OpenCyphal Development Team  <opencyphal.org>
/// Copyright Amazon.com Inc. or its affiliates.
/// SPDX-License-Identifier: MIT
///

#include <signal.h>
#include <stdlib.h>

void flush_coverage_on_death(void);

#if defined(CETLVAST_COVERAGE) && defined(__GNUC__) && !defined(__clang__)

void on_signal_abort(int signum);
void dump_coverage(void);

#if __GNUC__ > 10
extern void __gcov_dump(void);

void dump_coverage(void)
{
    __gcov_dump();
}
#else
extern void __gcov_flush(void);

void dump_coverage(void)
{
    __gcov_flush();
}
#endif

void on_signal_abort(int signum)
{
    signal(signum, SIG_DFL);
    dump_coverage();
    abort();
}

void flush_coverage_on_death(void)
{
    signal(SIGABRT, &on_signal_abort);
}

#else

void flush_coverage_on_death(void){}

#endif
