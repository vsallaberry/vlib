/*
 * Copyright (C) 2020 Vincent Sallaberry
 * vlib <https://github.com/vsallaberry/vlib>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */
/* ------------------------------------------------------------------------
 * Simple unit tests utilities.
 */
#ifndef VLIB_TEST_H
#define VLIB_TEST_H

#ifdef __cplusplus
# include <cstdlib>
# include <cstring>
# include <cstdio>
# include <cstdarg>
# include <cerrno>
#else
# include <stdlib.h>
# include <string.h>
# include <stdio.h>
# include <stdarg.h>
# include <errno.h>
#endif
#include <unistd.h>

#include "vlib/log.h"
#include "vlib/logpool.h"
#include "vlib/time.h"
#include "vlib/slist.h"

/* ************************************************************************ */

/** opaque testpool_t */
typedef struct testpool_s       testpool_t;

/** testgroup_t */
typedef struct {
    testpool_t *    testpool;
    unsigned int    flags;
    log_t *         log;
    char *          name;
    slist_t *       results;
    unsigned long   n_tests;
    unsigned long   n_ok;
    unsigned long   n_errors;
    BENCH_TM_DECL   (tm_bench);
    BENCH_DECL      (cpu_bench);
    log_level_t     ok_loglevel;
} testgroup_t;

/** testresult_t */
#define TEST_ERRNO_UNCHANGED    INT_MAX /* testresult_t.checkerrno value if unchanged */
#define TEST_ERRNO_DISABLED     INT_MIN /* testresult_t.checkerrno value if not set */
typedef struct {
    testgroup_t *   testgroup;
    char *          checkname;
    char *          msg;
    unsigned long   id;
    char *          file;
    char *          func;
    unsigned int    line;
    int             success;
    int             checkerrno;
    BENCH_TM_DECL   (tm_bench);
    BENCH_DECL      (cpu_bench);
} testresult_t;

/** tests_create() flags, will be copied to group on group creation,
 * then the group can update its own flags independently */
typedef enum {
    TPF_NONE            = 0,
    TPF_LOGTRUEPREFIX   = 1 << 0,   /* same behavior as LPG_TRUEPREFIX */
    TPF_STORE_RESULTS   = 1 << 1,   /* store all TEST_CHECK results */
    TPF_STORE_ERRORS    = 1 << 2,   /* sto TEST_CHECK results on error */
    TPF_BENCH_RESULTS   = 1 << 3,   /* bench each TEST_CHECK: warning cpu cost */
    TPF_TESTOK_SCREAM   = 1 << 4,   /* display TEST_CHECK OK only with level SCREAM */
    TPF_CHECK_ERRNO     = 1 << 5,   /* TEST_CHECK: check and display errno update */
    TPF_LOG_TESTPREFIX  = 1 << 6,   /* getlog prefix = tests/<TESTNAME> */
    TPF_DEFAULT         = (TPF_STORE_ERRORS | TPF_NONE | TPF_CHECK_ERRNO
                           | TPF_LOG_TESTPREFIX),
    TPF_FINISHED        = 1 << 15,
    TPF_INTERNAL        = 1 << 16
} testpool_flags_t;

/** DEFAULT TESTPOOL LOGPREFIX if TPF_LOGTRUEPREFIX is not given to tests_create() */
#define TESTPOOL_LOG_PREFIX     "tests"

/* ************************************************************************
 * Usage:
 *  // create testpool
 *  testpool_t * testpool = tests_create(NULL, TPF_DEFAULT);
 *  testgroup_t * test;
 *  unsigned int nerrors = 0;
 *  test = TEST_START(testpool, "TEST01");
 *  TEST_CHECK(test, "CHECK01", 1 == 1);
 *  TEST_CHECK(test, "CHECK02", 1 == 0);
 *  if (test) test->flags |= TPF_STORE_RESULTS | TPF_BENCH_RESULTS;
 *  TEST_CHECK(test, "CHECK03", 1 == 1);
 *  TEST_CHECK2(test, LOG_LVL_INFO, 1 == 0, "CHECK04 checking %s", "something");
 *  nerrors += TEST_END(test);
 *  tests_print(testpool, TPR_DEFAULT);
 *  tests_free(testpool);
 */
/* ************************************************************************ */

/** start a group of tests: return a (testgroup_t *) not be shared with other threads */
#define TEST_START(                                     \
            /*(testpool_t *)*/      _TESTPOOL,          \
            /*(const char *)*/      _TESTNAME)          \
        TEST_START2(_TESTPOOL, _TESTNAME, "%s", "")

/** end a group of tests: return number of errors */
#define TEST_END(                                       \
            /*(testgroup_t *)*/     _TESTGROUP)         \
        TEST_END2(_TESTGROUP, "%s", "")

/** perform a check on current test group, no return (void) */
#define TEST_CHECK(                                     \
            /*(testgroup_t *)*/     _TESTGROUP,         \
            /*String constant*/     _msg,               \
            /*(Boolean Expr.)*/     _CHECKING)          \
        TEST_CHECK0(_TESTGROUP, _msg "%s", _CHECKING, #_CHECKING, "")

#define TEST_CHECK2(_TESTGROUP, _fmt, _CHECKING, ...)       \
        TEST_CHECK0(_TESTGROUP, _fmt, _CHECKING, #_CHECKING, __VA_ARGS__)

/* ************************************************************************ */
#ifdef __cplusplus
extern "C" {
#endif
/* ************************************************************************ */

/** create a testpool
 * @param logpool the pool to use gettings logs, can be NULL
 * @param flags bit combination of testpool_flags_t
 * @return allocated testpool to be freed with test_free() */
testpool_t *            tests_create(
                            logpool_t *         logs,
                            unsigned int        flags);

/** get log instance assiciated with current test
 * @param tests the test pool.
 * @param testname the name of test
 * @return log instance or NULL on error */
log_t *                 tests_getlog(
                            testpool_t *        tests,
                            const char *        testname);

/** free a testpool
 * @param tests the test pool
 * @return 0 on success, negative on error */
int                     tests_free(
                            testpool_t *        tests);
/** tests_print() flags */
typedef enum {
    TPR_NONE            = 0,
    TPR_PRINT_GROUPS    = 1 << 0,
    TPR_PRINT_ERRORS    = 1 << 1,
    TPR_PRINT_OK        = 1 << 2,
    TPR_DEFAULT         = (TPR_PRINT_GROUPS | TPR_PRINT_ERRORS | TPR_PRINT_OK)
} tests_print_flags_t;

/** print a testpool
 * @param tests the test pool
 * @param flags a bit combination of tests_print_flags_t
 * @return negative value on error */
int                     tests_print(
                            testpool_t *        tests,
                            unsigned int        flags);

/* ************************************************************************ */
/* INTERNAL : for MACROS TEST_{START*,CHECK*,END*} */
/* ************************************************************************ */

/** code for TEST_START macro: use TEST_START rather than this */
testgroup_t *           tests_start(
                            testpool_t *        tests,
                            const char *        testname,
                            const char *        func,
                            const char *        file,
                            int                 line,
                            const char *        fmt,
                            ...) __attribute__((format(printf,6,7)));

/** code for TEST_END macro: use TEST_END rather than this */
unsigned long           tests_end(
                            testgroup_t *       testgroup,
                            const char *        func,
                            const char *        file,
                            int                 line,
                            const char *        fmt,
                            ...) __attribute__((format(printf,5,6)));

/** code for TEST_CHECK macro: use TEST_CHECK rather than this */
int                     tests_check(
                            testresult_t *      result,
                            const char *        func,
                            const char *        file,
                            int                 line,
                            const char *        fmt,
                            ...) __attribute__((format(printf,5,6)));

/* ************************************************************************ */
#ifdef __cplusplus
}
#endif

/* ************************************************************************ */
#define TEST_START2(_TESTPOOL, _TESTNAME, _fmt, ...)                        \
    tests_start((_TESTPOOL), (_TESTNAME), __func__, __FILE__, __LINE__, _fmt, __VA_ARGS__)

#define TEST_END2(_TESTGROUP, _fmt, ...)                                    \
    tests_end((_TESTGROUP), __func__, __FILE__, __LINE__, _fmt, __VA_ARGS__)

#define TEST_CHECK0(_TESTGROUP, _fmt, _CHECKING, _CHECKING_NAME, ...)       \
        do {                                                                \
            int __errno_bak; testresult_t __result;                         \
            int __do_errno = (_TESTGROUP) == NULL                           \
                        || ((_TESTGROUP)->flags & TPF_CHECK_ERRNO) != 0;    \
            int __do_bench = (_TESTGROUP) == NULL                           \
                        || ((_TESTGROUP)->flags & TPF_BENCH_RESULTS) != 0;  \
            if (__do_errno) __errno_bak = errno; else __errno_bak = EFAULT; \
            memset(&(__result), 0, sizeof(__result));                       \
            __result.testgroup = _TESTGROUP;                                \
            __result.checkname = _CHECKING_NAME;                            \
            if (__do_bench) {                                               \
                BENCH_TM_START(__result.tm_bench);                          \
                BENCH_START(__result.cpu_bench);                            \
            }                                                               \
            if (__do_errno) errno = TEST_ERRNO_UNCHANGED; /* to see if test changed errno */\
            else __result.checkerrno = TEST_ERRNO_DISABLED;                 \
            __result.success = (_CHECKING); /* THE TEST IS DONE HERE */     \
            if (__do_errno) { __result.checkerrno = errno;                  \
                              if (__result.checkerrno != TEST_ERRNO_UNCHANGED) { \
                                  __errno_bak = __result.checkerrno;        \
                            } }                                             \
            if (__do_bench) {                                               \
                BENCH_STOP(__result.cpu_bench);                             \
                BENCH_TM_STOP(__result.tm_bench);                           \
            }                                                               \
            tests_check(&__result, __func__, __FILE__, __LINE__, _fmt, __VA_ARGS__); \
            if (__do_errno) errno = __errno_bak;                            \
        } while(0)

/* ************************************************************************ */
#endif /* ! ifndef *_H */

