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

/** vjob_fun_t : fun ptr executing job */
typedef void * (*vjob_fun_t)(void *);

/** external opaque job_t */
struct vjob_s;
typedef struct vjob_s vjob_t;

#ifdef __cplusplus
extern "C" {
#endif

/** free a job without stopping it (let it run) */
int         vjob_free(vjob_t * job);

/** run a job in a new thread
 * @param fun the function to execute
 * @param data the data to give to function
 * @return job handle or NULL on error */
vjob_t *    vjob_run(vjob_fun_t fun, void * data);

/** run job and forget it (let it run)
 * @return 0 on success */
int         vjob_runandfree(vjob_fun_t fun, void * data);

/** check whether job is completed
 * @return non-null if job is done, 0 otherwise. */
int         vjob_done(vjob_t * job);

/** wait for job completion, get job return value */
void *      vjob_wait(vjob_t * job);

/** kill job and wait for completion */
void *      vjob_kill(vjob_t * job);

/** wait job completion, get return value and free job. */
void *      vjob_waitandfree(vjob_t * job);

/** kill job, wait completion, and free job */
void *      vjob_killandfree(vjob_t * job);

/** can be called inside the job function (vjob_fun_t) to add a
 * cancelation point for vjob_kill(). */
void        vjob_testkill();

/** turn on or off asynchronous job kill mode
 * if async mode is off, calls to vjob_testkill() in job function
 * (vjob_fun_t) are mandatory.
 * @return 0 on success */
int         vjob_killasync(int async);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

