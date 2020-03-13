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
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <string.h>

#include "vlib/job.h"
#include "vlib/thread.h"

/** internal */
enum {
    VJS_NONE        = 0,
    VJS_CREATED     = 1 << 0,
    VJS_STARTED     = 1 << 1,
    VJS_DONE        = 1 << 2
};

/** internal */
struct vjob_s {
    void *                  user_data;
    volatile sig_atomic_t * pfreed;
    vjob_fun_t              user_fun;
    pthread_t               tid;
    pthread_mutex_t         mutex;
    pthread_cond_t          cond;
    unsigned int            state;
};

/* ************************************************************************ */
typedef struct {
    vjob_t *                job;
    volatile sig_atomic_t   freed;
} job_runner_cleanup_t;

static void job_runner_cleanup(void * vdata) {
    job_runner_cleanup_t * data = (job_runner_cleanup_t *) vdata;
    if (data->freed == 0) {
        data->job->state |= VJS_DONE;
        data->job->pfreed = NULL;
        pthread_mutex_unlock(&(data->job->mutex));
    }
}

static void * job_runner(void * vdata) {
    vjob_t *                job = (vjob_t *) vdata;
    void *                  ret;
    job_runner_cleanup_t    cleanup = { job, 0 };

    pthread_mutex_lock(&job->mutex);

    job->pfreed = &(cleanup.freed);
    pthread_cleanup_push(job_runner_cleanup, &cleanup);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_ASYNCHRONOUS, NULL);
    job->state |= VJS_STARTED;
    pthread_cond_signal(&(job->cond));
    pthread_cond_wait(&(job->cond), &(job->mutex));

    ret = job->user_fun(job->user_data);

    pthread_cleanup_pop(1);

    return ret;
}

/* ************************************************************************ */
int vjob_free(vjob_t * job) {
    if (job == NULL)
        return -1;
    if (job->pfreed != NULL)
        *(job->pfreed) = 1;
    pthread_mutex_destroy(&job->mutex);
    pthread_cond_destroy(&job->cond);
    free(job);
    return 0;
}

/* ************************************************************************ */
vjob_t * vjob_run(vjob_fun_t fun, void * data) {
    vjob_t * job;

    if (fun == NULL)
        return NULL;

    if ((job = malloc(sizeof(*job))) == NULL) {
        return NULL;
    }

    pthread_mutex_init(&(job->mutex), NULL);
    pthread_cond_init(&(job->cond), NULL);
    pthread_mutex_lock(&(job->mutex));

    job->user_fun = fun;
    job->user_data = data;
    job->pfreed = NULL;
    job->state = VJS_CREATED;

    if (pthread_create(&(job->tid), NULL, job_runner, job) != 0) {
        vjob_free(job);
        return NULL;
    }
    pthread_cond_wait(&(job->cond), &(job->mutex));
    pthread_cond_signal(&(job->cond));
    pthread_mutex_unlock(&(job->mutex));

    return job;
}

/* ************************************************************************ */
int vjob_runandfree(vjob_fun_t fun, void * data) {
    vjob_t * job;

    if ((job = vjob_run(fun, data)) == NULL) {
        return -1;
    }
    vjob_free(job);
    return 0;
}

/* ************************************************************************ */
int vjob_done(vjob_t * job) {
    if (job == NULL)
        return -1;

    if (pthread_mutex_trylock(&job->mutex) == 0) {
        /* mutex can be aqcuired if not started or if done. need to check state */
        int done = (job->state & VJS_DONE) != 0;
        pthread_mutex_unlock(&job->mutex);
        return done;
    }
    return 0;
}

/* ************************************************************************ */
void * vjob_wait(vjob_t * job) {
    void *  retval = (void *) -1;

    if (job == NULL)
        return retval;
    if (vlib_thread_valgrind(0, NULL)) {
        while (!vjob_done(job)) {
            usleep(100000);
        }
    } else {
        pthread_join(job->tid, &retval);
    }
    return retval;
}

/* ************************************************************************ */
void * vjob_kill(vjob_t * job) {
    void *  retval = (void *) -1;

    if (job == NULL)
        return retval;

    if (pthread_cancel(job->tid) == 0)
        retval = vjob_wait(job);
    return retval;
}

/* ************************************************************************ */
void * vjob_waitandfree(vjob_t * job) {
    void * retval = vjob_wait(job);
    vjob_free(job);
    return retval;
}

/* ************************************************************************ */
void * vjob_killandfree(vjob_t * job) {
    void * retval = vjob_kill(job);
    vjob_free(job);
    return retval;
}

/* ************************************************************************ */
void vjob_testkill() {
    pthread_testcancel();
}

/* ************************************************************************ */
int vjob_killasync(int async) {
    return pthread_setcanceltype(async ? PTHREAD_CANCEL_ASYNCHRONOUS
                                       : PTHREAD_CANCEL_DEFERRED, NULL);
}


