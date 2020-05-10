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
 * Simple job launch utilities.
 */
#ifndef VLIB_JOB_H
#define VLIB_JOB_H

#ifdef __cplusplus
# include <cstdlib>
#else
# include <stdlib.h>
#endif
/* ************************************************************************ */

/** vjob_fun_t : fun ptr executing job */
#define VJOB_NO_RESULT      ((void *) -2)
#define VJOB_ERR_RESULT     ((void *) -1)
typedef void *              (*vjob_fun_t)(void *);

/** external opaque job_t */
struct vjob_s;
typedef struct vjob_s vjob_t;

/** vjob states */
typedef enum {
    VJS_NONE            = 0,
    VJS_CREATED         = 1 << 0,
    VJS_STARTED         = 1 << 1,
    VJS_DONE            = 1 << 2,
    VJS_DETACHED        = 1 << 3,
    VJS_EXIT_REQUESTED  = 1 << 4,
    VJS_INTERRUPTED     = 1 << 5,
    VJS_LOGPOOL_DISABLED= 1 << 6,
} vjob_state_t;

#ifdef __cplusplus
extern "C" {
#endif
/* ************************************************************************ */

/** free a job (kill and wait for termination if running)
 * @return job result or VJOB_ERR_RESULT or VJOB_NO_RESULT */
void *          vjob_free(vjob_t * job);

/** run a job in a new thread
 * @param fun the function to execute
 * @param user_data the data to give to function
 * @return job handle or NULL on error */
vjob_t *        vjob_run(vjob_fun_t fun, void * user_data);

/** get state of job
 * @return bit combination of vjob_state_t */
unsigned int    vjob_state(vjob_t * job);

/** check whether job is completed
 * @return non-null if job is done or interrupted, 0 otherwise. */
int             vjob_done(vjob_t * job);

/** wait for job completion, get job return value
 * @return  result of vjob_fun_t
 *          VJOB_ERR_RESULT on error,
 *          VJOB_NO_RESULT if job not done */
void *          vjob_wait(vjob_t * job);

/** kill job with waiting
 * @return VJOB_ERR_RESULT on error, VJOB_NO_RESULT if job not done, or job result
 * @notes: Warning, if the kill mode is enabled, the job could let locked
 *   mutexes locked depending on pthread implementation.
 *   To Avoid this, the job (vjob_fun_t) can call at startup
 *   vjob_killmode(0, 0, NULL, NULL) and vjob_testkill() in safe
 *   locations during processing, or/and use pthread_clean{push/pop}().
 *   See below functions vjob_killmode(). */
void *          vjob_kill(vjob_t * job);

/** kill job without waiting. vjob_wait() or job_free() needed.
 * see vjob_kill().
 * @notes: warning logpool is disabled until vjob_{kill,wait,free) call. */
void *          vjob_killnowait(vjob_t * job);

/** wait job completion, get return value and free job.
 * @return result of vjob_wait() */
void *          vjob_waitandfree(vjob_t * job);

/** kill job, wait completion, and free job
 * @return result of vjob_kill() */
void *          vjob_killandfree(vjob_t * job);

/** can be called inside the job function (vjob_fun_t) to add a
 * cancelation point for vjob_kill().
 * @notes: calling this function implicitly re-enable the
 * kill mode then restore it before return. */
void            vjob_testkill();

/** turn on or off asynchronous job kill mode
 * to be called from the vjob_fun_t function.
 * @param enable mode : if 0 thread connot be killed unless during
 *   vjob_testkill() called by job function (vjob_fun_t);
 * @param async mode : if not 0, job can be killed at any moment
 *   as long as kill mode is enabled.
 *   (dangerous if system does not clean mutex at thread exit)
 *   if 0, job can be killed at system cancelation points (refer to
 *   man 3 pthread_setcancelstate), as long as kill mode is enabled.
 * @param old_enable, if not NULL, the previous enable state is stored here.
 * @param old_async, if not NULL, the previous async state is stored here.
 * @return 0 on success, -1 on error */
int             vjob_killmode(
                    int             enable,
                    int             async,
                    int *           old_enable,
                    int *           old_async);

/** @return number of available CPUs */
unsigned int    vjob_cpu_nb();

/* ************************************************************************ */
#if 0
/** disabled features */

/** run job and forget it (let it run)
 * @return 0 on success */
int             vjob_runandfree(vjob_fun_t fun, void * data);

#endif /* ! disabled features */
/* ************************************************************************ */

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

