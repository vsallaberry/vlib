/*
 * Copyright (C) 2020,2023 Vincent Sallaberry
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

#include "vlib_private.h"

/** internal */
typedef struct vjob_cleanup_s vjob_cleanup_t;

struct vjob_s {
    void *                  retval;
    void *                  user_data;
    vjob_fun_t              user_fun;
    vjob_cleanup_t *        cleanup;
    pthread_t               tid;
    pthread_mutex_t         mutex;
    pthread_cond_t          cond;
    unsigned int            state;
};

/* ************************************************************************ */

struct vjob_cleanup_s {
    vjob_t *            job;
};

static void job_cleanup(void * vdata) {
    vjob_cleanup_t *    cleanup = (vjob_cleanup_t *) vdata;

    if (cleanup != NULL && cleanup->job != NULL) {
        vjob_t * job = cleanup->job;
        pthread_mutex_lock(&(job->mutex));
        job->state |= VJS_INTERRUPTED;
        cleanup->job = NULL;
        pthread_mutex_unlock(&(job->mutex));
    }
}

static void * job_runner(void * vdata) {
    vjob_t *                job         = (vjob_t *) vdata;
    void *                  retval      = VJOB_NO_RESULT;
    vjob_cleanup_t          cleanup     = { job };
    vjob_fun_t              user_fun;
    void *                  user_data;

    pthread_mutex_lock(&(job->mutex));
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);
    user_fun = job->user_fun;
    user_data = job->user_data;
    job->state |= VJS_STARTED;
    job->retval = retval;
    job->cleanup = &cleanup;
    if ((job->state & VJS_DETACHED) != 0) {
        cleanup.job = NULL;
        pthread_detach(pthread_self());
    }
    if ((job->state & VJS_EXIT_REQUESTED) != 0) {
        job->state |= VJS_INTERRUPTED;
        pthread_mutex_unlock(&(job->mutex));
        pthread_exit(retval);
    }

    pthread_cleanup_push(job_cleanup, &cleanup);
    /* synchro with vjob_run() */
    pthread_cond_signal(&(job->cond));
    pthread_cond_wait(&(job->cond), &(job->mutex));
    pthread_mutex_unlock(&(job->mutex));

    /* prepare start, enable cancelation */
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    pthread_testcancel();

    /* Run the actual job */
    retval = user_fun(user_data);

    /* finish and set job done */
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_pop(0);

    if (cleanup.job != NULL) {
        pthread_mutex_lock(&(job->mutex));
        job->state |= VJS_DONE;
        job->retval = retval;
        job->cleanup = NULL;
        pthread_mutex_unlock(&(job->mutex));
    }

    /* exit */
    return retval;
}

/* ************************************************************************ */
void vjob_free_ctx(vjob_t * job) {
    pthread_mutex_destroy(&job->mutex);
    pthread_cond_destroy(&job->cond);
    free(job);
}

/* ************************************************************************ */
void * vjob_free(vjob_t * job) {
    void * ret = VJOB_ERR_RESULT;

    if (job == NULL)
        return ret;
    vjob_kill(job);
    ret = vjob_wait(job);
    vjob_free_ctx(job);
    return ret;
}

/* ************************************************************************ */
vjob_t * vjob_run(vjob_fun_t fun, void * user_data) {
    vjob_t * job;

    if (fun == NULL)
        return NULL;

    if ((job = malloc(sizeof(*job))) == NULL) {
        return NULL;
    }

    pthread_mutex_init(&(job->mutex), NULL);
    pthread_cond_init(&(job->cond), NULL);

    job->user_fun = fun;
    job->user_data = user_data;
    job->retval = VJOB_NO_RESULT;
    job->state = VJS_CREATED;
    job->cleanup = NULL;

    pthread_mutex_lock(&(job->mutex));
    if (pthread_create(&(job->tid), NULL, job_runner, job) != 0) {
        pthread_mutex_unlock(&(job->mutex));
        vjob_free(job);
        return NULL;
    }
    #if 0 && defined(_DEBUG)
    if (vthread_valgrind(0, NULL)) {
        pthread_detach(job->tid);
    }
    #endif

    /* synchro with job_runner() */
    pthread_cond_wait(&(job->cond), &(job->mutex));
    pthread_cond_signal(&(job->cond));
    pthread_mutex_unlock(&(job->mutex));

    return job;
}

/* ************************************************************************ */
unsigned int vjob_state(vjob_t * job) {
    unsigned int state;

    if (job == NULL)
        return VJS_NONE;

    pthread_mutex_lock(&(job->mutex));
    state = job->state;
    pthread_mutex_unlock(&(job->mutex));

    return state;
}

/* ************************************************************************ */
int vjob_done(vjob_t * job) {
    return ((vjob_state(job) & (VJS_DONE | VJS_INTERRUPTED)) != 0);
}

/* ************************************************************************ */
void * vjob_wait(vjob_t * job) {
    void *  retval = VJOB_ERR_RESULT;
    unsigned int state;

    if (job == NULL)
        return retval;

    /* ------------------------------------- */
    pthread_mutex_lock(&(job->mutex));
    state = job->state;

    if ((state & (VJS_DETACHED)) != 0) {
        retval = job->retval;
        pthread_mutex_unlock(&(job->mutex));
        LOG_DEBUG(g_vlib_log, "%s(): detached. state %x (job:%lx)",
                  __func__, state, (unsigned long) job);
        return retval;
    }

    job->state |= VJS_DETACHED;
    pthread_mutex_unlock(&(job->mutex));
    /* ------------------------------------- */
    LOG_DEBUG(g_vlib_log, "%s(): state %x (job:%lx)",
              __func__, state, (unsigned long) job);

    #ifdef _DEBUG
    if (vthread_valgrind(0, NULL)) {
        while (!vjob_done(job)) {
            usleep(100000);
        }
        retval = job->retval;
        usleep(100000);
    } else
    #endif
    {
        if (pthread_join(job->tid, &retval) == 0
        && retval != PTHREAD_CANCELED) {
            job->retval = retval;
        } else {
            retval = job->retval;
        }
    }

    pthread_mutex_lock(&(job->mutex));
    if ((job->state & VJS_LOGPOOL_DISABLED) != 0) {
        logpool_enable(g_vlib_logpool, NULL, 1, NULL);
        job->state &= ~VJS_LOGPOOL_DISABLED;
    }
    pthread_mutex_unlock(&(job->mutex));

    return retval;
}

/* ************************************************************************ */
void * vjob_killnowait(vjob_t * job) {
    void *          retval  = VJOB_ERR_RESULT;
    unsigned int    state;
    int             prev_enable;

    if (job == NULL)
        return retval;

    pthread_mutex_lock(&(job->mutex));
    state = job->state;
    job->state |= VJS_EXIT_REQUESTED;
    if ((state & (VJS_DONE | VJS_DETACHED | VJS_INTERRUPTED | VJS_EXIT_REQUESTED)) != 0
    || (state & VJS_STARTED) == 0) {
        retval = job->retval;
        pthread_mutex_unlock(&(job->mutex));
        return retval;
    }
    logpool_enable(g_vlib_logpool, NULL, 0, &prev_enable);
    if (prev_enable)
        job->state |= VJS_LOGPOOL_DISABLED;
    pthread_mutex_unlock(&(job->mutex));

    LOG_DEBUG(g_vlib_log, "%s(): state %x", __func__, state);

    fflush(NULL);

    pthread_cancel(job->tid);

    return VJOB_NO_RESULT;
}

/* ************************************************************************ */
void * vjob_kill(vjob_t * job) {
    void *          retval;

    if (job == NULL) {
        return VJOB_ERR_RESULT;
    }

    vjob_killnowait(job);
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
    int state;
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &state);
    pthread_testcancel();
    pthread_setcancelstate(state, NULL);
}

/* ************************************************************************ */
int vjob_killmode(int enable, int async, int *old_enable, int *old_async) {
    int ret, old_state;

    ret = pthread_setcancelstate(enable ? PTHREAD_CANCEL_ENABLE
                                        : PTHREAD_CANCEL_DISABLE, &old_state);
    if (ret != 0)
        return -1;

    if (old_enable != NULL)
        *old_enable = (old_state == PTHREAD_CANCEL_ENABLE ? 1 : 0);

    ret = pthread_setcanceltype(async ? PTHREAD_CANCEL_ASYNCHRONOUS
                                      : PTHREAD_CANCEL_DEFERRED, old_async);
    if (ret != 0) {
        pthread_setcancelstate(old_state, NULL);
        return -1;
    }

    if (old_async != NULL)
        *old_async = (*old_async == PTHREAD_CANCEL_ASYNCHRONOUS ? 1 : 0);
    return 0;
}

/* ************************************************************************ */
static inline int vjob_detach_internal(vjob_t * job, int is_self) {
    if (job == NULL) {
        errno = EFAULT;
        return -1;
    }
    pthread_mutex_lock(&(job->mutex));
    if ((!is_self || job->tid == pthread_self())
    &&  job->cleanup != NULL
    &&  (job->state & (VJS_DONE | VJS_INTERRUPTED | VJS_DETACHED)) == 0) {
        job->state |= VJS_DETACHED;
        job->cleanup->job = NULL;
        pthread_detach(job->tid);
        pthread_mutex_unlock(&(job->mutex));
        vjob_free_ctx(job);
        LOG_SCREAM(g_vlib_log, "job %lx detached.", (unsigned long) job);
        return 0;
    }
    pthread_mutex_unlock(&(job->mutex));
    return -1;
}

/* ************************************************************************ */
int vjob_detachme(vjob_t * job) {
    return vjob_detach_internal(job, 1);
}

/* ************************************************************************ */
int vjob_detach(vjob_t * job) {
    return vjob_detach_internal(job, 0);
}

/* ************************************************************************ */
int vjob_runandfree(vjob_fun_t fun, void * data) {
    vjob_t * job;

    if ((job = vjob_run(fun, data)) == NULL) {
        return -1;
    }
    if (vjob_detach(job) != 0) {
        vjob_free(job);
        return -1;
    }
    return 0;
}

/* ************************************************************************ */
unsigned int vjob_cpu_nb() {
    static int ncpus = -1;
    if (ncpus > 0) {
        return ncpus;
    }
    if (ncpus < 0) {
        ncpus = 0;
        long sc = sysconf(_SC_NPROCESSORS_ONLN);
        if (sc > 0) {
            ncpus = sc;
        } else {
            ncpus = 1;
        }
    }
    return ncpus;
}

