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
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>

#include "vlib/avltree.h"
#include "vlib/log.h"
#include "vlib/logpool.h"
#include "vlib/job.h"
#include "vlib/time.h"
#include "vlib/slist.h"
#include "vlib/term.h"

#include "vlib/test.h"

#include "vlib_private.h"

/* ************************************************************************ */

struct testpool_s {
    avltree_t *         tests;
    logpool_t *         logs;
    unsigned int        flags;
    pthread_rwlock_t    rwlock;
};

enum {
    TPF_FREE_LOGPOOL    = TPF_INTERNAL,
};

/* ************************************************************************ */
static void tests_result_free(void * vdata) {
    testresult_t * result = (testresult_t *) vdata;

    if (result != NULL) {
        if (result->checkname != NULL)
            free(result->checkname);
        if (result->msg != NULL)
            free(result->msg);
        if (result->file != NULL)
            free(result->file);
        if (result->func != NULL)
            free(result->func);
        free(result);
    }
}
static void tests_group_free(void * vdata) {
    testgroup_t * group = (testgroup_t *) vdata;

    if (group != NULL) {
        if (group->name != NULL)
            free(group->name);
        if (group->results != NULL)
            slist_free(group->results, tests_result_free);
        free(group);
    }
}
static int tests_group_cmp(const void * v1, const void * v2) {
    testgroup_t * g1 = (testgroup_t *) v1;
    testgroup_t * g2 = (testgroup_t *) v2;

    if (g1 == g2) {
        return 0;
    }
    if (g1 == NULL || g2 == NULL) {
        return g1 - g2;
    }
    return strcmp(g1->name, g2->name);
}

/* ************************************************************************ */
testpool_t *            tests_create(
                            logpool_t *         logs,
                            unsigned int        flags) {
    (void) flags;
    testpool_t * tests = calloc(1, sizeof(testpool_t));

    if (tests == NULL) {
        return NULL;
    }
    pthread_rwlock_init(&tests->rwlock, NULL);
    tests->flags = flags;

    if (logs == NULL) {
        tests->logs = logpool_create();
        tests->flags |= TPF_FREE_LOGPOOL;
    } else {
        tests->logs = logs;
    }
    tests->tests = avltree_create(AFL_DEFAULT, tests_group_cmp, tests_group_free);

    if (tests->logs == NULL || tests->tests == NULL) {
        tests_free(tests);
        return NULL;
    }

    return tests;
}

/* ************************************************************************ */
int                     tests_free(
                            testpool_t *        tests) {
    if (tests != NULL) {
        pthread_rwlock_wrlock(&(tests->rwlock));
        if ((tests->flags & TPF_FREE_LOGPOOL) != 0) {
            logpool_free(tests->logs);
        }
        avltree_free(tests->tests);
        tests->tests = NULL;
        pthread_rwlock_unlock(&(tests->rwlock));
        pthread_rwlock_destroy(&(tests->rwlock));
        free(tests);
        return 0;
    }
    return -1;
}

/* ************************************************************************ */
typedef struct {
    log_t *         log;
    unsigned int    flags;
    int             do_eol;
} tests_print_visit_t;
static avltree_visit_status_t   tests_printgroup_visit(
                                    avltree_t *                         tree,
                                    avltree_node_t *                    node,
                                    const avltree_visit_context_t *     context,
                                    void *                              user_data) {
    tests_print_visit_t *   data    = (tests_print_visit_t *) user_data;
    testgroup_t *           group   = (testgroup_t *) node->data;
    int                     fd      = data != NULL && data->log != NULL
                                      && data->log->out != NULL
                                      && (data->log->flags & LOG_FLAG_COLOR) != 0
                                      ? fileno(data->log->out) : -1;
    (void) context;
    (void) tree;

    if (group != NULL) {
        if ((data->flags & TPR_PRINT_GROUPS) != 0) {
            const char * color, * color_reset;

            color_reset = vterm_color(fd, VCOLOR_RESET);
            color = vterm_color(fd, group->n_errors > 0 ? VCOLOR_RED : VCOLOR_GREEN);
            LOG_INFO(data->log, "%-16s: %s%s%lu error%s%s, %lu/%lu, %lu.%03lus (cpus:%lu.%03lus)",
                group->name, color, vterm_color(fd, VCOLOR_BOLD),
                group->n_errors, group->n_errors > 1 ? "s" : "", color_reset,
                group->n_ok, group->n_tests,
                BENCH_TM_GET(group->tm_bench) / 1000UL, BENCH_TM_GET(group->tm_bench) % 1000UL,
                BENCH_GET(group->cpu_bench) / 1000UL, BENCH_GET(group->cpu_bench) % 1000UL);
            if (data->do_eol == 0) data->do_eol = 1;
        }
        if ((data->flags & (TPR_PRINT_ERRORS | TPR_PRINT_OK)) != 0) {
            SLIST_FOREACH_DATA(group->results, result, testresult_t *) {
                if (result != NULL
                    &&  (   ((data->flags & TPR_PRINT_ERRORS) != 0  && result->success == 0)
                         || ((data->flags & TPR_PRINT_OK) != 0      && result->success != 0))) {
                        const char *    color, * color_reset;
                        char            errno_msg[64] = { 0, };

                        color_reset = vterm_color(fd, VCOLOR_RESET);
                        color = vterm_color(fd, result->success == 0 ? VCOLOR_RED : VCOLOR_GREEN);

                        if (result->checkerrno != TEST_ERRNO_UNCHANGED
                        &&  result->checkerrno != TEST_ERRNO_DISABLED) {
                            int ret;
                            if ((ret = strerror_r(result->checkerrno, errno_msg,
                                                  sizeof(errno_msg) / sizeof(*errno_msg))) != 0
                            && ret != EINVAL && ret != ERANGE)
                                *errno_msg = 0;
                        }

                        LOG_INFO(data->log, "  [" "%s%s" "%s" "%s" "] %s/%lu: %s%s%s [%s], "
                                            "%lu.%03lus (cpus:%lu/%03lus), %s():%s:%u",
                            color, vterm_color(fd, VCOLOR_BOLD),
                            result->success ? "  OK  " : "FAILED", color_reset,
                            result->testgroup->name, result->id, result->msg,
                            *errno_msg != 0 ? ", errno: " : "",
                            errno_msg, result->checkname,
                            BENCH_TM_GET(result->tm_bench) / 1000UL,
                            BENCH_TM_GET(result->tm_bench) % 1000UL,
                            BENCH_GET(result->cpu_bench) / 1000UL,
                            BENCH_GET(result->cpu_bench) % 1000UL,
                            result->func, result->file, result->line);
                        if (data->do_eol == 0) data->do_eol = 1;
                }
            }
        }
    }
    return AVS_CONTINUE;
}

/* ************************************************************************ */
int                     tests_print(
                            testpool_t *        tests,
                            unsigned int        flags) {
    tests_print_visit_t data = { .flags = flags, .do_eol = 0 };
    int ret;

    if (tests == NULL) {
        return -1;
    }

    if ((flags & (TPR_PRINT_GROUPS | TPR_PRINT_ERRORS | TPR_PRINT_OK)) == 0) {
        return 0;
    }

    /* ---------------------------------------------------------------------*/
    pthread_rwlock_rdlock(&(tests->rwlock));
    data.log = logpool_getlog(tests->logs, TESTPOOL_LOG_PREFIX, LPG_DEFAULT);
    ret = avltree_visit(tests->tests, tests_printgroup_visit, &data, AVH_INFIX);
    if (data.do_eol != 0) {
        LOG_INFO(data.log, NULL);
    }
    pthread_rwlock_unlock(&(tests->rwlock));
    /* ---------------------------------------------------------------------*/

    return ret == AVS_FINISHED ? 0 : -1;
}

/* ************************************************************************ */
log_t *                 tests_getlog(
                            testpool_t *        tests,
                            const char *        testname) {
    unsigned int    flags;
    log_t *         log;

    if (tests == NULL) {
        return NULL;
    }
    if ((tests->flags & TPF_LOGTRUEPREFIX) != 0) {
        flags = LPG_TRUEPREFIX;
    } else {
        flags = LPG_DEFAULT;
        testname = TESTPOOL_LOG_PREFIX;
    }
    log = logpool_getlog(tests->logs, testname, flags);

    return log;
}

/* ************************************************************************ */
testgroup_t *           tests_start(
                            testpool_t *        tests,
                            const char *        testname) {

    testgroup_t *   group;

    if (tests == NULL) {
        return NULL;
    }
    if ((group = calloc(1, sizeof(testgroup_t))) == NULL) {
        return NULL;
    }

    group->testpool = tests;
    group->flags = tests->flags;
    group->n_tests = group->n_ok = group->n_errors = 0;
    group->log = tests_getlog(tests, testname);
    group->name = strdup(testname);
    group->ok_loglevel = (tests->flags & TPF_TESTOK_SCREAM) != 0
                         ? LOG_LVL_SCREAM : LOG_LVL_VERBOSE;

    pthread_rwlock_wrlock(&(tests->rwlock));

    if (avltree_insert(tests->tests, group) == NULL) {
        tests_group_free(group);
        group = NULL;
    }

    pthread_rwlock_unlock(&(tests->rwlock));

    BENCH_TM_START(group->tm_bench);
    BENCH_START(group->cpu_bench);

    return group;
}

/* ************************************************************************ */
unsigned long           tests_end(
                            testgroup_t *       testgroup) {
    if (testgroup == NULL) {
        return 1;
    }

    BENCH_STOP(testgroup->cpu_bench);
    BENCH_TM_STOP(testgroup->tm_bench);

    return testgroup->n_errors;
}

/* ************************************************************************ */
int                     tests_check(
                            testresult_t *      result,
                            const char *        func,
                            const char *        file,
                            int                 line,
                            const char *        fmt,
                            ...) {
    int         store_result, log_result;
    va_list     valist;
    char *      msg = NULL;

    if (result == NULL || result->testgroup == NULL) {
        return 0;
    }

    /* Update Test counters, check whether storing and/or logging result needed */
    result->id = (result->testgroup->n_tests)++;
    if (result->success) {
        ++(result->testgroup->n_ok);
        store_result = ((result->testgroup->flags & TPF_STORE_RESULTS) != 0);
        log_result = LOG_CAN_LOG(result->testgroup->log, result->testgroup->ok_loglevel);
    } else {
        ++(result->testgroup->n_errors);
        store_result = ((result->testgroup->flags & (TPF_STORE_ERRORS | TPF_STORE_RESULTS)) != 0);
        log_result = LOG_CAN_LOG(result->testgroup->log, LOG_LVL_ERROR);
    }

    if (log_result || store_result) {
        /* translate fmt and __VA_ARGS__ */
        va_start(valist, fmt);
        if (fmt == NULL || vasprintf(&msg, fmt, valist) < 0 || msg == NULL) {
            msg = strdup("<VASPRINTF_ERROR> ");
        }
        va_end(valist);

        /* LOG RESULT */
        if (log_result) {
            int             fd = TEST_LOGFD(result->testgroup);
            char            errno_msg[64] = { 0, };

            /* prepare errno message in case it has been updated by the test */
            if (result->checkerrno != TEST_ERRNO_UNCHANGED
            && result->checkerrno != TEST_ERRNO_DISABLED) {
                int ret;
                if ((ret = strerror_r(result->checkerrno, errno_msg,
                                      sizeof(errno_msg) / sizeof(*errno_msg))) != 0
                && ret != EINVAL && ret != ERANGE)
                    *errno_msg = 0;
            }

            if (result->success) {
                vlog(result->testgroup->ok_loglevel, result->testgroup->log,
                     file, func, line, "%s: %s%sOK%s: %s%s%s [%s]",
                     result->testgroup->name,
                     vterm_color(fd, VCOLOR_GREEN), vterm_color(fd, VCOLOR_BOLD),
                     vterm_color(fd, VCOLOR_RESET), msg,
                     *errno_msg != 0 ? ", errno: " : "", errno_msg, result->checkname);
            } else {
                vlog(LOG_LVL_ERROR, result->testgroup->log,
                     file, func, line, "%s: %s%sERROR%s %s%s%s [%s]",
                     result->testgroup->name,
                     vterm_color(fd, VCOLOR_RED), vterm_color(fd, VCOLOR_BOLD),
                     vterm_color(fd, VCOLOR_RESET), msg,
                     *errno_msg != 0 ? ", errno: " : "", errno_msg, result->checkname);
            }
        }

        /* STORE RESULT */
        if (store_result) {
            testresult_t * newresult = malloc(sizeof(*result));
            if (newresult != NULL) {
                memcpy(newresult, result, sizeof(*result));
                newresult->func = strdup(func);
                newresult->file = strdup(file);
                newresult->line = line;
                newresult->msg = msg;
                newresult->checkname = strdup(result->checkname);
                result->testgroup->results = slist_prepend(result->testgroup->results, newresult);
            }
        } else if (msg != NULL) {
            free(msg);
        }
    }

    return result->success;
}

/* ************************************************************************ */

