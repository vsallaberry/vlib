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
 * Simple thread utilities.
 */

#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "vlib/thread.h"
#include "vlib/slist.h"
#include "vlib/log.h"
#include "vlib_private.h"

#define SIG_SPECIAL_VALUE 0
#define VLIB_THREAD_PSELECT

typedef struct vlib_thread_priv_s {
    pthread_mutex_t             mutex;
    pthread_cond_t              cond;
    int                         exit_signal;
    slist_t *                   event_list;
    unsigned long               process_timeout;
} vlib_thread_priv_t;

typedef struct {
    vlib_thread_event_t         event;
    union {
        int                     fd;
        void *                  ptr;
        int                     sig;
    } ev;
    union {
        struct {
            vlib_thread_callback_t  fun;
            void *                  data;
        } callback;
        struct {
            volatile sig_atomic_t * flag;
            int                     value;
        } flag;
    } act;
} vlib_thread_event_data_t;

static pthread_mutex_t      flag_mutex = PTHREAD_MUTEX_INITIALIZER;
static void                 flag_sig_handler(int sig, siginfo_t * sig_info,  void * context);
static void *               vlib_thread(void * data);
static void                 vlib_thread_ctx_destroy(vlib_thread_t * vthread);

int callback(
        vlib_thread_t * vthread,
        vlib_thread_event_t event,
        void * event_data,
        void * user_data) {
    return 0;
}
#define VTE_DATA_FD(fd)         ((void *)((long)fd))
#define VTE_DATA_SIG(sig)       ((void *)((long)sig))
#define VTE_DATA_FLAG(flag)     ((vlib_thread_callback_t)(flag))
#define VTE_DATA_FLAGVAL(val)   ((void *)((long)val))
void testcb() {
    int fd = 1;
    vlib_thread_t * vthread = NULL;
    int sig = 1;
    const char * str = "aaa";
    volatile sig_atomic_t flag;
    int flag_value = 1 << 7;
    vlib_thread_register_event(vthread, VTE_FD_READ, VTE_DATA_FD(fd), callback, vthread);
    vlib_thread_register_event(vthread, VTE_INIT, str, callback, vthread);
    vlib_thread_register_event(vthread, VTE_SIG, VTE_DATA_SIG(sig),
            VTE_DATA_FLAG(&flag), VTE_DATA_FLAGVAL(flag_value));
}

vlib_thread_t *     vlib_thread_create(
                            unsigned long               process_timeout,
                            log_t *                     log) {
    vlib_thread_t *         vthread = NULL;
    vlib_thread_priv_t *    priv = NULL;
    sigset_t                sigset, sigsetsave;

    if ((vthread = calloc(1, sizeof(vlib_thread_t))) == NULL) {
        LOG_ERROR(g_vlib_log, "cannot malloc vlib_thread_t: %s", strerror(errno));
        return NULL;
    }
    if (log)
        vthread->log = log;
    else
        vthread->log = g_vlib_log;
    if ((priv = calloc(1, sizeof(vlib_thread_priv_t))) == NULL) {
        LOG_ERROR(vthread->log, "cannot malloc vlib_thread_priv_t: %s", strerror(errno));
        vlib_thread_ctx_destroy(vthread);
        return NULL;
    }
    vthread->priv = priv;
    priv->process_timeout = process_timeout;
    priv->exit_signal = VLIB_THREAD_EXIT_SIG;
    /* set the signal mask to nothing but the exit_signal
     * so that the thread is created with it already blocked */
    sigemptyset(&sigset);
    sigaddset(&sigset, priv->exit_signal);
    sigaddset(&sigset, SIGINT);
    pthread_sigmask(SIG_SETMASK, &sigset, &sigsetsave);

    pthread_mutex_init(&priv->mutex, NULL);
    pthread_cond_init(&priv->cond, NULL);
    pthread_mutex_lock(&priv->mutex);

    if (pthread_create(&vthread->tid, NULL, vlib_thread, vthread) != 0) {
        LOG_ERROR(vthread->log, "%s(): pthread_create error: %s", strerror(errno));
        pthread_mutex_unlock(&priv->mutex);
        vlib_thread_ctx_destroy(vthread);
        pthread_sigmask(SIG_SETMASK, &sigsetsave, NULL);
        return NULL;
    }
    /* wait for the thread blocking exit_signal */
    pthread_cond_wait(&priv->cond, &priv->mutex);
    pthread_mutex_unlock(&priv->mutex);
    pthread_sigmask(SIG_SETMASK, &sigsetsave, NULL);
    LOG_VERBOSE(vthread->log, "thread: created");
    return vthread;
}

int                 vlib_thread_start(
                            vlib_thread_t *             vthread) {
    vlib_thread_priv_t * priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;
    int ret;

    if (priv == NULL) {
        LOG_ERROR(g_vlib_log, "bad thread context");
        return -1;
    }

    pthread_mutex_lock(&priv->mutex);
    ret = pthread_cond_signal(&priv->cond);
    pthread_mutex_unlock(&priv->mutex);

    if (ret != 0)
        LOG_ERROR(vthread->log, "thread: cannot wake up");

    return ret;
}

/** stop and clean the thread
 * @param vthread will be freed by this function
 * @return the return value of the thread */
void *                  vlib_thread_stop(
                            vlib_thread_t *             vthread) {
    vlib_thread_priv_t  * priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;
    void * ret_val = (void *) -1;
    int ret;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return NULL;
    }

    LOG_DEBUG(vthread->log, "locking flag_mutex...");
    pthread_mutex_lock(&flag_mutex);

    LOG_DEBUG(vthread->log, "locking priv_mutex...");
    pthread_mutex_lock(&priv->mutex);

    ret = pthread_kill(vthread->tid, priv->exit_signal);
    LOG_DEBUG(vthread->log, "pthread_kill ret %d", ret);

    ret = pthread_cond_signal(&priv->cond);
    LOG_DEBUG(vthread->log, "pthread_cond_signal ret %d", ret);

    pthread_mutex_unlock(&priv->mutex);

    LOG_DEBUG(vthread->log, "pthread_join...");
    pthread_join(vthread->tid, &ret_val);
    pthread_mutex_unlock(&flag_mutex);
    LOG_DEBUG(vthread->log, "destroy vthread and return...");
    vlib_thread_ctx_destroy(vthread);

    return ret_val;
}

int                 vlib_thread_set_exit_signal(
                            vlib_thread_t *             vthread,
                            int                         exit_signal) {
    vlib_thread_priv_t * priv = vthread ? vthread->priv : NULL;

    pthread_mutex_lock(&priv->mutex);
    //TODO: pseudo code not working
    if (priv == NULL) {
        pthread_mutex_unlock(&priv->mutex);
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    /* restore previous signal handler if any */
    if (priv->exit_signal != SIG_SPECIAL_VALUE) {
        if (sigaction(priv->exit_signal, NULL, NULL) /*sa_save, NULL);*/ != 0) {
            pthread_mutex_unlock(&priv->mutex);
            LOG_ERROR(vthread->log, "sigaction uninstall exit_signal: %s", strerror(errno));
            return -1;
        }
    }
    priv->exit_signal = exit_signal;
    /* install new signal handler */
    if (sigaction(priv->exit_signal, NULL, NULL)/*&sa, &save);*/ != 0) {
        pthread_mutex_unlock(&priv->mutex);
        LOG_ERROR(vthread->log, "sigaction install exit_signal: %s", strerror(errno));
        return -1;
    }
    pthread_mutex_unlock(&priv->mutex);
    return 0;
}

int                 vlib_thread_register_event(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data) {
    vlib_thread_priv_t  * priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;

    pthread_mutex_lock(&priv->mutex);
    if (priv == NULL) {
        pthread_mutex_unlock(&priv->mutex);
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }

    pthread_mutex_unlock(&priv->mutex);

    return 0;
}


/** register a flag for modification on signal reception */
int                 vlib_thread_sig_register_flag(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            sig_atomic_t *              flag_ptr,
                            unsigned int                value);

static void                 vlib_thread_ctx_destroy(vlib_thread_t * vthread) {
    if (vthread == NULL)
        return ;
    vlib_thread_priv_t  * priv = (vlib_thread_priv_t *) vthread->priv;
    if (priv) {
        pthread_mutex_destroy(&priv->mutex);
        pthread_cond_destroy(&priv->cond);
        free(priv);
    }
    free(vthread);
}

static void * vlib_thread(void * data) {
    vlib_thread_t *         vthread = (vlib_thread_t *) data;
    vlib_thread_priv_t *    priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;

    fd_set                  readfds, writefds, errfds;
    struct sigaction        sa = { .sa_sigaction = flag_sig_handler,
                                   .sa_flags = SA_SIGINFO | SA_RESTART };
    sigset_t                sigset;
    volatile sig_atomic_t   thread_running = 1;
#   ifndef VLIB_THREAD_PSELECT
    struct timeval          select_timeout, * p_select_timeout;
#   else
    struct timespec         select_timeout, * p_select_timeout;
#   endif
    int                     select_ret, ret, fd_max = -1;

    LOG_VERBOSE(vthread->log, "thread: initializing");
    pthread_mutex_lock(&priv->mutex);
    /* let know the creator (vlib_thread_create) that we are running, to avoid
     * vlib_thread_{start,stop} executing before the thread is ready to handle it. */
    pthread_cond_signal(&priv->cond);

    /* remove the exit_signal from sigset which will be passed to pselect
     * the caller (vlib_thread_create is reponsible for blocking exit_signal before
     * creating the thread */
    sigemptyset(&sigset);
    sigaddset(&sigset, SIGINT);

    /* call sig_handler to register this thread with thread_running ptr */
    if (sigaction(SIG_SPECIAL_VALUE, &sa, NULL) == 0) {
        LOG_ERROR(vthread->log, "error sigaction(%d) is accepted, "
                                "choose another value for SIG_SPECIAL_VALUE",
                                SIG_SPECIAL_VALUE);
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
        return (void*) -1;
    }
    flag_sig_handler(SIG_SPECIAL_VALUE, NULL, (void *) &thread_running);
    sigemptyset(&sa.sa_mask);
    if (sigaction(priv->exit_signal, &sa, NULL) < 0) {
        LOG_ERROR(vthread->log, "error sigaction(%d): %s", priv->exit_signal, strerror(errno));
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
        return (void*) -1;
    }

    /* wait for vlib_thread_start() and signal back just before loop starts
     * this allows call to vlib_thread_{register_event,set_exit_signal} before start */
    LOG_VERBOSE(vthread->log, "thread: waiting for launch");
    select_ret = pthread_cond_wait(&priv->cond, &priv->mutex);

    LOG_DEBUG(vthread->log, "pthread_cond_wait exit ret=%d RUNNING=%d",
              select_ret, thread_running);

    SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
        if ((data->event & VTE_INIT) != 0) {
            ret = data->act.callback.fun(vthread, VTE_INIT, NULL, data->act.callback.data);
        }
    }
    LOG_VERBOSE(vthread->log, "thread: launched");
    pthread_cond_signal(&priv->cond);

    while (thread_running) {
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&errfds);
        SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
            if (data && (data->event & (VTE_FD_READ | VTE_FD_WRITE | VTE_FD_ERR)) != 0) {
                if ((data->event & VTE_FD_READ) != 0)
                    FD_SET(data->ev.fd, &readfds);
                if ((data->event & VTE_FD_WRITE) != 0)
                    FD_SET(data->ev.fd, &writefds);
                if ((data->event & VTE_FD_ERR) != 0)
                    FD_SET(data->ev.fd, &errfds);
                if (data->ev.fd > fd_max)
                    fd_max = data->ev.fd;
            }
        }

        if (priv->process_timeout > 0) {
            p_select_timeout = &select_timeout;
            p_select_timeout->tv_sec = priv->process_timeout / 1000;
#           ifndef VLIB_THREAD_PSELECT
            p_select_timeout->tv_usec = (priv->process_timeout % 1000) * 1000;
#           else
            /* even if pselect does not modify the timespec, we allow dynamic timeout value */
            p_select_timeout->tv_nsec = (priv->process_timeout % 1000) * 1000000;
#           endif
        } else
            p_select_timeout = NULL;

        /* -------------------------------------------- */
        LOG_DEBUG(vthread->log, "start select ismember %d",
                  sigismember(&sigset, priv->exit_signal));

        pthread_mutex_unlock(&priv->mutex);

#       ifndef VLIB_THREAD_PSELECT
        select_ret = select(fd_max + 1, &readfds, &writefds, &errfds, p_select_timeout);
#       else
        select_ret = pselect(fd_max + 1, &readfds, &writefds, &errfds, p_select_timeout, &sigset);
#       endif
        LOG_DEBUG(vthread->log, "select return %d errno %d", select_ret, errno);

        pthread_mutex_lock(&priv->mutex);

        LOG_DEBUG(vthread->log, "mutex relocked");
        /* -------------------------------------------- */

        SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
            if ((data->event & VTE_PROCESS_START) != 0) {
                ret = data->act.callback.fun(vthread, VTE_PROCESS_START, NULL,
                                             data->act.callback.data);
            }
        }

        if (select_ret == 0) {
            LOG_VERBOSE(vthread->log, "thread: select timeout");
            continue ;
        }
        if (select_ret < 0 && errno == EINTR) {
            continue;
        }
        if (select_ret < 0) {
            LOG_VERBOSE(vthread->log, "thread: select error: %s", strerror(errno));
            break ;
        }
        SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
            if (data) {
                if ((data->event & VTE_FD_READ) != 0 && FD_ISSET(data->ev.fd, &readfds)
                && data->act.callback.fun) {
                    ret = data->act.callback.fun(vthread, VTE_FD_READ, VTE_DATA_FD(data->ev.fd),
                                            data->act.callback.data);
                }
                if ((data->event & VTE_FD_WRITE) != 0 && FD_ISSET(data->ev.fd, &writefds)
                && data->act.callback.fun) {
                    ret = data->act.callback.fun(vthread, VTE_FD_WRITE, VTE_DATA_FD(data->ev.fd),
                                            data->act.callback.data);
                }
                if ((data->event & VTE_FD_ERR) != 0 && FD_ISSET(data->ev.fd, &errfds)
                && data->act.callback.fun) {
                    ret = data->act.callback.fun(vthread, VTE_FD_ERR, VTE_DATA_FD(data->ev.fd),
                                            data->act.callback.data);
                }
            }
        }
        /*
        if (priv->process_end) {
            ret = priv->process_end(vthread, VTE_PROCESS_END,
                                    (void *) select_ret, priv->callback_data);
        }*/
    }
    LOG_VERBOSE(vthread->log, "thread: shutting down");

    SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
        if (data) {
            if ((data->event & VTE_CLEAN) != 0) {
                ret = data->act.callback.fun(vthread, VTE_CLEAN, (void *) NULL,
                                            data->act.callback.data);
            }
            if ((data->event & (VTE_FD_READ | VTE_FD_WRITE | VTE_FD_ERR)) != 0) {
                close(data->ev.fd);
            }
        }
    }

    /*pthread_mutex_lock(&flag_mutex);
    flag_sig_handler(priv->exit_signal,
    pthread_mutex_unlock(&flag_mutex);*/

    pthread_mutex_unlock(&priv->mutex);

    return (void*) 0;
}

static void flag_sig_handler(int sig, siginfo_t * sig_info,  void * context) {
    /* list of registered threads */
    static struct threadlist_s {
        pthread_t tid; volatile sig_atomic_t * running; struct threadlist_s * next;
    } *         threadlist = NULL;
    pthread_t   tself = pthread_self();

    LOG_DEBUG(g_vlib_log, "SIG %d inf %p pid %d context %p tself %lu",
              sig, sig_info, sig_info? sig_info->si_pid : -1, context, tself);

    if (sig != SIG_SPECIAL_VALUE) {
        if (sig_info
#       ifndef _DEBUG /* when run with gdb, the sender of signal is not getpid() */
        && sig_info->si_pid == getpid()
#       endif
        ) {
            /* looking for tid in threadlist and update running if found, then delete entry */
            for (struct threadlist_s * prev = NULL, * cur = threadlist;
                 cur;
                 prev = cur, cur = cur->next) {
                if (tself == cur->tid && cur->running) {
                    *cur->running = 0;
                    if (prev == NULL)
                        threadlist = cur->next;
                    else
                        prev->next = cur->next;
                    free(cur);
                    LOG_DEBUG(g_vlib_log, "flag_sig_handler: thread flag updated");
                    break ;
                }
            }
        }
        return ;
    }
    /* following is not a signal */
    pthread_mutex_lock(&flag_mutex);
    sigset_t block, save;
    sigemptyset(&save);
    sigfillset(&block);
    pthread_sigmask(SIG_BLOCK, &block, &save);
    /* special mode to delete threadlist, but if thread exited normally
     * this should not be necessary */
    if (sig == SIG_SPECIAL_VALUE && sig_info == NULL && context == NULL) {
        LOG_DEBUG(g_vlib_log, "flag_sig_handler deleting all");
        for (struct threadlist_s * cur = threadlist; cur; ) {
            struct threadlist_s * to_delete = cur;
            cur = cur->next;
            free(to_delete);
        }
        threadlist = NULL;
    } else {
        /* if this is reached we register the data as running ptr for pthread_self() */
        LOG_DEBUG(g_vlib_log, "flag_sig_handler registering thread %lu", tself);
        struct threadlist_s * new = malloc(sizeof(struct threadlist_s));
        if (new) {
            new->tid = tself;
            new->running = context;
            new->next = threadlist;
            threadlist = new;
        } else {
            LOG_ERROR(g_vlib_log, "malloc threadlist error: %s\n", strerror(errno));
        }
    }
    pthread_sigmask(SIG_SETMASK, &save, NULL);
    pthread_mutex_unlock(&flag_mutex);
}

