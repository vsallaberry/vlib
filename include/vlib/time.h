/*
 * Copyright (C) 2018 Vincent Sallaberry
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
 * Simple utilities.
 */
#ifndef VLIB_TIME_H
#define VLIB_TIME_H

#include <sys/types.h>
#include <sys/time.h>

#ifdef __cplusplus
# include <cstring>
# include <cstdio>
# include <ctime>
# include <climits>
#else
# include <string.h>
# include <stdio.h>
# include <time.h>
# include <limits.h>
#endif

#include "vlib/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/** define constants used by clock_gettime if not existing */
#undef VLIB_CLOCK_GETTIME_WRAPPER
#ifndef CLOCK_MONOTONIC
# define VLIB_CLOCK_GETTIME_WRAPPER
# define CLOCK_MONOTONIC        -130
#elif ! defined(CLOCK_MONOTONIC_RAW)
# define CLOCK_MONOTONIC_RAW    CLOCK_MONOTONIC
#endif
#ifndef CLOCK_MONOTONIC_RAW
# define CLOCK_MONOTONIC_RAW    -131
#endif
#ifndef CLOCK_REALTIME
# define CLOCK_REALTIME         -132
#endif

/* vtimespecsub: similar to timersub but for 'struct timespec' */
#define vtimespecsub(tsop1, tsop2, tsres) \
            do { \
                (tsres)->tv_sec = (tsop1)->tv_sec - (tsop2)->tv_sec; \
                (tsres)->tv_nsec = (tsop1)->tv_nsec - (tsop2)->tv_nsec; \
                if ((tsres)->tv_nsec < 0) { \
                    (tsres)->tv_sec--; \
                    (tsres)->tv_nsec += 1000000000; \
                } \
            } while (0)
/* vtimespeccmp: compare timespec, return 0 if =, <0 if op1<op2, >0 if op1>op2 */
#define vtimespeccmp(tsop1, tsop2) \
            ((tsop1)->tv_sec == (tsop2)->tv_sec ? (tsop1)->tv_nsec - (tsop2)->tv_nsec \
                                                : (tsop1)->tv_sec - (tsop2)->tvsec)
/*
 * vclock_gettime() wrapper to clock_gettime() or
 * other available clock service on the system.
 */
int vclock_gettime(int id, struct timespec * ts);

/* ******************************************
 * CLOCK BENCH: measures cpu tick of process
 ********************************************/

/** Bench Decl */
#define BENCH_DECL(name)    struct {  \
                                clock_t t; \
                            } name

/**
 * Bench Start: start a bench using a bench variable previously declared
 * with BENCH_DECL. It uses clock() then sleep time is not taken into account.
 */
#define BENCH_START(name)       (name).t = clock()

/** Bench Decl_Start : declare bench variable and start bench.
 * Be carefull when using BENCH_DECL_START, as it declares a variable outside
 * a block and it generates 2 instructions not inside a block. */
#define BENCH_DECL_START(name)  BENCH_DECL(name); BENCH_START(name)

/** Stop Bench */
#define BENCH_STOP(name) \
            do { \
                clock_t __t = clock(); \
                if ((name).t == (clock_t) -1 || __t == (clock_t) -1) { \
                    (name).t = (clock_t) -1; \
                } else { \
                    (name).t = (__t - (name).t); \
                } \
            } while (0)

/* Get Bench value (ms) */
#define BENCH_GET(name)     (long)(((name).t * 1000) / CLOCKS_PER_SEC)
/* Get Bench value (us) */
#define BENCH_GET_US(name)  (long)(((name).t * 1000000) / (CLOCKS_PER_SEC))
/* Get Bench value (ns) */
#define BENCH_GET_NS(name)  (long)(((name).t * 1000000000) / (CLOCKS_PER_SEC))

/** Bench Stop & Display
 * BENCH_STOP_PRINT(name, fprintf, stderr, "something") is
 * not supported unless we use ##__VA_ARGS__ which is gnu extension
 * you can use BENCH_STOP_PRINT(name, fprintf, stderr, "somthg%s", "").
 * And because fmt must be a string constant, you should use
 * BENCH_STOP_PRINT(name, fprintf, stderr, "%s", string_variable). */
#define BENCH_STOP_PRINT(name, printf_fun, arg, fmt, ...) \
            do { \
                long __t; \
                BENCH_STOP(name); \
                __t = BENCH_GET(name); \
                printf_fun(arg, fmt "DURATION(cpu) = %ld.%03lds", \
                           __VA_ARGS__, \
                           __t / 1000, \
                           __t % 1000); \
            } while(0)
#define BENCH_STOP_PRINTF(name, ...) \
            do { \
                BENCH_STOP_PRINT(name, fprintf, stdout, __VA_ARGS__); \
                fputc('\n', stdout); \
            } while (0)
#define BENCH_STOP_LOG(name, log, ...) \
                BENCH_STOP_PRINT(name, LOG_INFO, log, __VA_ARGS__)

/* ******************************
 * TIME BENCH: measures time.
 ********************************/

/** Bench Time Decl */
#define BENCH_TM_DECL(name)     struct {  \
                                    struct timespec t0; \
                                    struct timespec t1; \
                                } name
/**
 * Bench Start: start a time bench using a bench variable previously declared
 * with BENCH_TM_DECL.
 */
#define BENCH_TM_START(name)       vclock_gettime(CLOCK_MONOTONIC_RAW, &((name).t0))

/** Bench Time Decl_Start
 * Be carefull when using BENCH_TM_DECL_START, as it declares a variable outside
 * a block and it generates 2 instructions not inside a block.
 */
#define BENCH_TM_DECL_START(name)   BENCH_TM_DECL(name); BENCH_TM_START(name)

/** Bench Time Stop & Display */
#define BENCH_TM_STOP(name) \
            do { \
                vclock_gettime(CLOCK_MONOTONIC_RAW, &((name).t1)); \
                vtimespecsub(&((name).t1), &((name).t0), &((name).t1)); \
            } while(0)

/** Get Bench Time */
#define BENCH_TM_GET(name)      (long)( (((name).t1).tv_sec * 1000) \
                                        + (((name).t1).tv_nsec / 1000000))
#define BENCH_TM_GET_US(name)   (long)( (((name).t1).tv_sec * 1000000) \
                                        + (((name).t1).tv_nsec / 1000))
#define BENCH_TM_GET_NS(name)   (long)( (((name).t1).tv_sec * 1000000000) \
                                        + (((name).t1).tv_nsec))

/** Bench Time Stop & Display
 * BENCH_TM_STOP_PRINT(name, fprintf, stderr, "something") is
 * not supported unless we use ##__VA_ARGS__ which is gnu extension
 * you can use BENCH_TM_STOP_PRINT(name, fprintf, stderr, "somthg%s", "").
 * And because fmt must be a string constant, you should use
 * BENCH_TM_STOP_PRINT(name, fprintf, stderr, "%s", string_variable). */
#define BENCH_TM_STOP_PRINT(name, printf_fun, arg, fmt, ...) \
            do { \
                long __t; \
                BENCH_TM_STOP(name); \
                __t = BENCH_TM_GET(name); \
                printf_fun(arg, fmt "DURATION = %ld.%03lds", \
                           __VA_ARGS__, \
                           __t / 1000, \
                           __t % 1000); \
            } while(0)
#define BENCH_TM_STOP_PRINTF(name, ...) \
            do { \
                BENCH_TM_STOP_PRINT(name, fprintf, stdout, __VA_ARGS__); \
                fputc('\n', stdout); \
            } while (0)
#define BENCH_TM_STOP_LOG(name, log, ...) \
                BENCH_TM_STOP_PRINT(name, LOG_INFO, log, __VA_ARGS__)

/* *************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

