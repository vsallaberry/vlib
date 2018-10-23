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

#define SIG_SPECIAL_VALUE 0 /* FIXME */

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
                            int                         exit_signal,
                            unsigned long               process_timeout,
                            log_t *                     log) {
    vlib_thread_t *         vthread = NULL;
    vlib_thread_priv_t *    priv = NULL;

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

    pthread_mutex_init(&priv->mutex, NULL);
    pthread_cond_init(&priv->cond, NULL);
    priv->exit_signal = exit_signal;
    pthread_mutex_lock(&priv->mutex);

    if (pthread_create(&vthread->tid, NULL, vlib_thread, vthread) != 0) {
        LOG_ERROR(vthread->log, "%s(): pthread_create error: %s", strerror(errno));
        pthread_mutex_unlock(&priv->mutex);
        vlib_thread_ctx_destroy(vthread);
        return NULL;
    }
    pthread_cond_wait(&priv->cond, &priv->mutex);
    pthread_mutex_unlock(&priv->mutex);
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

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return NULL;
    }

    pthread_mutex_lock(&flag_mutex);
    pthread_kill(vthread->tid, priv->exit_signal);
    pthread_join(vthread->tid, &ret_val);
    pthread_mutex_unlock(&flag_mutex);
    vlib_thread_ctx_destroy(vthread);

    return ret_val;
}

int                 vlib_thread_register_event(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data) {
    vlib_thread_priv_t  * priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;

    if (priv == NULL)
        return -1;


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
    struct sigaction        sa = { .sa_sigaction = flag_sig_handler, .sa_flags = SA_SIGINFO };
    sigset_t                sigset;
    volatile sig_atomic_t   thread_running = 1;
    struct timeval          select_timeout, * p_select_timeout;
    int                     select_ret, ret, fd_max = -1;

    LOG_VERBOSE(vthread->log, "thread: initializing");
    pthread_mutex_lock(&priv->mutex);
    pthread_cond_signal(&priv->cond);

    /* call sig_handler to register this thread with thread_running ptr */
    if (sigaction(SIG_SPECIAL_VALUE, &sa, NULL) == 0) {
        fprintf(stderr, "%s(): error sigaction(%d) is accepted, "
                        "choose another value for SIG_SPECIAL_VALUE",
                        __func__, SIG_SPECIAL_VALUE);
        pthread_mutex_unlock(&priv->mutex);
        return (void*) -1;
    }
    sigemptyset(&sigset);
    sigaddset(&sigset, priv->exit_signal);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    flag_sig_handler(SIG_SPECIAL_VALUE, NULL, (void *) &thread_running);
    sigemptyset(&sa.sa_mask);
    if (sigaction(priv->exit_signal, &sa, NULL) < 0) {
        fprintf(stderr, "%s(): error sigaction(%d): %s",
                __func__, priv->exit_signal, strerror(errno));
        pthread_mutex_unlock(&priv->mutex);
        return (void*) -1;
    }

    /* wait for vlib_thread_start() and signal back when we are ready */
    LOG_VERBOSE(vthread->log, "thread: waiting for launch");
    sigprocmask(SIG_UNBLOCK, &sigset, NULL);
    pthread_cond_wait(&priv->cond, &priv->mutex);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
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
            p_select_timeout->tv_usec = (priv->process_timeout % 1000) * 1000;
        } else
            p_select_timeout = NULL;

        sigprocmask(SIG_UNBLOCK, &sigset, NULL);
        pthread_mutex_unlock(&priv->mutex);
        select_ret = select(fd_max + 1, &readfds, &writefds, &errfds, p_select_timeout);
        pthread_mutex_lock(&priv->mutex);
        sigprocmask(SIG_BLOCK, &sigset, NULL);

        SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
            if ((data->event & VTE_PROCESS_START) != 0) {
                ret = data->act.callback.fun(vthread, VTE_PROCESS_START, NULL, data->act.callback.data);
            }
        }

        if (select_ret == 0) {
            LOG_VERBOSE(vthread->log, "thread: select timeout");
            break ;continue ;
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

    pthread_mutex_unlock(&priv->mutex);

    return (void*) 0;
}

static void flag_sig_handler(int sig, siginfo_t * sig_info,  void * data) {
    /* list of registered threads */
    static struct threadlist_s {
        pthread_t tid; volatile sig_atomic_t * running; struct threadlist_s * next;
    } *         threadlist = NULL;
    pthread_t   tself = pthread_self();

    if (sig != SIG_SPECIAL_VALUE) {
        if (sig_info && sig_info->si_pid == getpid()) {
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
                    break ;
                }
            }
        }
        return ;
    }
    /* following is not a signal */
    sigset_t block, save;
    sigemptyset(&block);
    // FIXME sigaddset(&block, priv->exit_signal);
    pthread_sigmask(SIG_BLOCK, &block, &save);
    pthread_mutex_lock(&flag_mutex);
    /* special mode to delete threadlist, but if thread exited normally
     * this should not be necessary */
    if (sig == SIG_SPECIAL_VALUE && sig_info == NULL && data == NULL) {
        for (struct threadlist_s * cur = threadlist; cur; ) {
            struct threadlist_s * to_delete = cur;
            cur = cur->next;
            free(to_delete);
        }
        threadlist = NULL;
    } else {
        /* if this is reached we register the data as running ptr for pthread_self() */
        struct threadlist_s * new = malloc(sizeof(struct threadlist_s));
        if (new) {
            new->tid = tself;
            new->running = data;
            new->next = threadlist;
            threadlist = new;
        } else {
            fprintf(stderr, "%s(): malloc threadlist error: %s\n", __func__, strerror(errno));
        }
    }
    pthread_mutex_unlock(&flag_mutex);
    pthread_sigmask(SIG_SETMASK, &save, NULL);
}

