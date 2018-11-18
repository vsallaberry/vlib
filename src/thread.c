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
#include <fcntl.h>
#include <time.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdint.h>
#include <limits.h>

#include "vlib/thread.h"
#include "vlib/slist.h"
#include "vlib/log.h"
#include "vlib_private.h"

/*****************************************************************************/
#define SIG_SPECIAL_VALUE   INT_MIN
#define VLIB_THREAD_PSELECT

#if defined(_DEBUG) && (defined(__APPLE__) || defined(BUILD_SYS_darwin))
# include <sys/types.h>
# include <sys/param.h>
# include <sys/sysctl.h>
# define VALGRIND_DEBUG_WORKAROUND 1
#endif

/*****************************************************************************/
typedef struct vlib_thread_priv_s {
    pthread_mutex_t             mutex;
    pthread_cond_t              cond;
    int                         exit_signal;
    slist_t *                   event_list;
    unsigned long               process_timeout;
    vlib_thread_state_t         state;
} vlib_thread_priv_t;

/*****************************************************************************/
typedef struct {
    vlib_thread_event_t         event;
    union {
        int                     fd;
        void *                  ptr;
        int                     sig;
    } ev;
    vlib_thread_callback_t      callback;
    void *                      callback_data;
} vlib_thread_event_data_t;

/*****************************************************************************/
static void *                   vlib_thread_body(void * data);
static void                     vlib_thread_ctx_destroy(vlib_thread_t * vthread);
static int                      vlib_thread_notify(vlib_thread_t * vthread);
static int                      vlib_thread_set_exit_signal_internal(
                                    vlib_thread_t *             vthread,
                                    int                         exit_signal,
                                    sigset_t *                  block_sigset);
static int                      vlib_thread_closefd(
                                    vlib_thread_t *             vthread,
                                    vlib_thread_event_t         event,
                                    void *                      event_data,
                                    void *                      callback_user_data);
static int                      vlib_thread_register_event_unlocked(
                                    vlib_thread_t *             vthread,
                                    vlib_thread_event_t         event,
                                    void *                      event_data,
                                    vlib_thread_callback_t      callback,
                                    void *                      callback_user_data);
static void                     vlib_thread_sig_handler(int sig);
static volatile sig_atomic_t    s_last_signal = 0;

/*****************************************************************************/
vlib_thread_t *     vlib_thread_create(
                            unsigned long               process_timeout,
                            log_t *                     log) {
    vlib_thread_t *         vthread = NULL;
    vlib_thread_priv_t *    priv = NULL;

    /* vlib_thread_t allocation and initialization */
    if ((vthread = calloc(1, sizeof(vlib_thread_t))) == NULL) {
        LOG_ERROR(g_vlib_log, "cannot malloc vlib_thread_t: %s", strerror(errno));
        return NULL;
    }
    if (log) {
        vthread->log = log;
    } else {
        vthread->log = g_vlib_log;
    }
    if ((priv = calloc(1, sizeof(vlib_thread_priv_t))) == NULL) {
        LOG_ERROR(vthread->log, "cannot malloc vlib_thread_priv_t: %s", strerror(errno));
        vlib_thread_ctx_destroy(vthread);
        return NULL;
    }
    vthread->priv = priv;
    priv->state = VTS_CREATING;
    priv->process_timeout = process_timeout;
    priv->exit_signal = VLIB_THREAD_EXIT_SIG;
    pthread_mutex_init(&priv->mutex, NULL);
    pthread_cond_init(&priv->cond, NULL);

    /* create and run the thread */
    if (pthread_create(&vthread->tid, NULL, vlib_thread_body, vthread) != 0) {
        LOG_ERROR(vthread->log, "pthread_create error: %s", strerror(errno));
        vlib_thread_ctx_destroy(vthread);
        vthread = NULL;
    }

    LOG_VERBOSE(vthread->log, "thread: created");
    return vthread;
}

/*****************************************************************************/
int                 vlib_thread_start(
                            vlib_thread_t *             vthread) {
    vlib_thread_priv_t * priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;
    int ret;

    if (priv == NULL) {
        LOG_ERROR(g_vlib_log, "bad thread context");
        return -1;
    }

    pthread_mutex_lock(&priv->mutex);
    if ((priv->state & VTS_CREATING) != 0) {
        /* need to wait until the thread is ready to receive pthread_cond_signal() */
        pthread_cond_wait(&priv->cond, &priv->mutex);
    }
    ret = pthread_cond_signal(&priv->cond);
    pthread_mutex_unlock(&priv->mutex);

    if (ret != 0)
        LOG_ERROR(vthread->log, "thread: cannot wake up");

    return ret;
}

/*****************************************************************************/
void *                  vlib_thread_stop(
                            vlib_thread_t *             vthread) {
    vlib_thread_priv_t  * priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;
    void * ret_val = (void *) NULL;
    int ret;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return ret_val;
    }

    LOG_DEBUG(vthread->log, "locking priv_mutex...");
    pthread_mutex_lock(&priv->mutex);

    priv->state |= VTS_EXIT_REQUESTED;
    if ((priv->state & (VTS_CREATED | VTS_CREATING)) != 0) {
        ret = pthread_cond_signal(&priv->cond);
        LOG_DEBUG(vthread->log, "pthread_cond_signal ret %d", ret);
    }

    /* signal the running thread about configuration change */
    vlib_thread_notify(vthread);

#   ifdef VALGRIND_DEBUG_WORKAROUND
    int valgrind = vlib_thread_valgrind(0, NULL);
    if (valgrind) {
        vlib_thread_state_t state = priv->state;
        pthread_mutex_unlock(&priv->mutex);
        LOG_DEBUG(vthread->log, "VALGRIND: waiting thread...");
        while ((state & VTS_FINISHED) == 0) {
            pthread_mutex_lock(&priv->mutex);
            state = priv->state;
            pthread_mutex_unlock(&priv->mutex);
            usleep(10000);
        }
        usleep(50000);
        ret_val = (void *) 1UL;
    } else
#   endif
    {
        pthread_mutex_unlock(&priv->mutex);
        LOG_DEBUG(vthread->log, "pthread_join...");
        pthread_join(vthread->tid, &ret_val);
    }

    LOG_DEBUG(vthread->log, "destroy vthread and return...");
    vlib_thread_ctx_destroy(vthread);

    return ret_val;
}

/*****************************************************************************/
unsigned int        vlib_thread_state(
                            vlib_thread_t *             vthread) {
    vlib_thread_priv_t *    priv = vthread ? vthread->priv : NULL;
    unsigned int            state;

    /* sanity checks */
    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return VTS_NONE;
    }
    /* LOCK mutex */
    pthread_mutex_lock(&priv->mutex);
    state = priv->state;
    pthread_mutex_unlock(&priv->mutex);

    return state;
}

/*****************************************************************************/
int                 vlib_thread_set_exit_signal(
                            vlib_thread_t *             vthread,
                            int                         exit_signal) {
    vlib_thread_priv_t *    priv = vthread ? vthread->priv : NULL;
    int                     ret;

    /* sanity checks */
    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    pthread_mutex_lock(&priv->mutex);
    ret = vlib_thread_notify(vthread);
    priv->exit_signal = exit_signal;
    pthread_mutex_unlock(&priv->mutex);

    LOG_VERBOSE(vthread->log, "new exit signal %d", exit_signal);
    return 0;
}

/*****************************************************************************/
int                 vlib_thread_register_event(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data) {
    vlib_thread_priv_t *    priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;
    int                     ret;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    LOG_VERBOSE(vthread->log, "register event %d ev_data:%lx callback_data:%lx",
                event, (long) event_data, (long) callback_user_data);

    pthread_mutex_lock(&priv->mutex);
    ret = vlib_thread_register_event_unlocked(vthread, event, event_data,
                                              callback, callback_user_data);
    /* signal the thread about configuration change */
    vlib_thread_notify(vthread);
    pthread_mutex_unlock(&priv->mutex);

    return ret;
}

/*****************************************************************************/
int                 vlib_thread_unregister_event(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data) {
    vlib_thread_priv_t  * priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    LOG_VERBOSE(vthread->log, "unregister event %d ev_data:%lx",
                event, (long) event_data);

    pthread_mutex_lock(&priv->mutex);
    //TODO
    LOG_WARN(vthread->log, "warning: %s() NOT IMPLEMENTED", __func__);

    /* signal the thread about configuration change */
    vlib_thread_notify(vthread);
    pthread_mutex_unlock(&priv->mutex);

    return 0;
}

/*****************************************************************************/
int                 vlib_thread_pipe_create(
                            vlib_thread_t *             vthread,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data) {
    struct sigaction        sa;
    vlib_thread_priv_t *    priv = vthread ? vthread->priv : NULL;
    int                     pipefd[2];
    int                     fd_flags;

    /* sanity checks */
    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return VTS_NONE;
    }
    /* ignore SIGPIPE for caller if not already set or ignored */
    if (sigaction(SIGPIPE, NULL, &sa) != 0) {
        LOG_ERROR(vthread->log, "error sigaction(get SIGPIPE): %s", strerror(errno));
        return -1;
    }
    if ((sa.sa_flags & SA_SIGINFO) == 0 && sa.sa_handler == SIG_DFL) {
        LOG_DEBUG(vthread->log, "thread: ignore SIGPIPE for vlib_thread_pipe_create() caller");
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGPIPE, &sa, NULL) != 0) {
            LOG_ERROR(vthread->log, "error sigaction(set SIGPIPE): %s", strerror(errno));
            return -1;
        }
    }
    /* create the pipe, set it to non blocking */
    if (pipe(pipefd) != 0) {
        LOG_ERROR(vthread->log, "error pipe(): %s", strerror(errno));
        return -1;
    }
    for (unsigned i = 0; i < 2; i++) {
        fd_flags = fcntl(pipefd[i], F_GETFL);
        if (fd_flags == -1 || fcntl(pipefd[i], F_SETFL, (fd_flags | O_NONBLOCK)) == -1) {
            LOG_ERROR(vthread->log, "error fcntl(pipe) : %s", strerror(errno));
            close(pipefd[1]);
            close(pipefd[0]);
            return -1;
        }
    }
    /* LOCK mutex */
    pthread_mutex_lock(&priv->mutex);
    if (vlib_thread_register_event_unlocked(vthread, VTE_FD_READ, VTE_DATA_FD(pipefd[0]),
                                            callback, callback_user_data) != 0
    ||  vlib_thread_register_event_unlocked(vthread, VTE_CLEAN, NULL,
                                            vlib_thread_closefd, VTE_DATA_FD(pipefd[1])) != 0) {
        LOG_ERROR(vthread->log, "error vlib_thread_register_event");
        close(pipefd[1]);
        close(pipefd[0]);
        pipefd[1] = -1;
    }
    /* notify the thread about configuration change */
    vlib_thread_notify(vthread);
    pthread_mutex_unlock(&priv->mutex);

    return pipefd[1];
}

/*****************************************************************************/
ssize_t                 vlib_thread_pipe_write(
                            vlib_thread_t *             vthread,
                            int                         pipe_fdout,
                            void *                      data,
                            size_t                      size) {
    vlib_thread_priv_t *    priv = vthread ? vthread->priv : NULL;
    int                     locked = 0;
    ssize_t                 ret;

    /* sanity checks */
    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    /* checks whether the size exceeds PIPE_BUF */
    if (size > PIPE_BUF) {
        pthread_mutex_lock(&priv->mutex);
        locked = 1;
    }
    ret = write(pipe_fdout, data, size);
    if (locked) {
        pthread_mutex_unlock(&priv->mutex);
    }
    return ret;
}

/*****************************************************************************/
static void * vlib_thread_body(void * data) {
    vlib_thread_t *         vthread = (vlib_thread_t *) data;
    vlib_thread_priv_t *    priv = vthread ? (vlib_thread_priv_t *) vthread->priv : NULL;
    fd_set                  readfds, writefds, errfds;
    struct sigaction        sa;
    sigset_t                select_sigset, block_sigset;
    int                     select_ret, select_errno, ret, fd_max = -1;
    int                     old_exit_signal;
    sig_atomic_t            last_signal;
    void *                  thread_result = (void *) 1UL;
#   ifndef VLIB_THREAD_PSELECT
    struct timeval          select_timeout, * p_select_timeout;
    sigset_t                sigset_bak;
#   else
    struct timespec         select_timeout, * p_select_timeout;
#   endif

    if (priv == NULL) {
        LOG_ERROR(g_vlib_log, "bad thread context");
        return NULL;
    }
    LOG_VERBOSE(vthread->log, "thread: initializing");
    pthread_mutex_lock(&priv->mutex);

    if ((priv->state & VTS_EXIT_REQUESTED) != 0) {
        LOG_VERBOSE(vthread->log, "thread: exit requested before start -> exit");
        priv->state = VTS_FINISHED;
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
        return thread_result;
    }

    /* setup the thread exit signal */
    sigemptyset(&select_sigset);
    sigemptyset(&block_sigset);
    old_exit_signal = priv->exit_signal;
    priv->exit_signal = SIG_SPECIAL_VALUE;
    if (vlib_thread_set_exit_signal_internal(vthread, old_exit_signal, &block_sigset) != 0) {
        priv->state = VTS_FINISHED | VTS_ERROR;
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
        return NULL;
    }

    /* ignore SIGPIPE */
    sa.sa_flags = SA_RESTART;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGPIPE, &sa, NULL) < 0) {
        LOG_ERROR(vthread->log, "error sigaction(SIGPIPE)");
        priv->state = VTS_FINISHED | VTS_ERROR;
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
        return NULL;
    }

    /* notify vlib_thread_start() we are ready,
     * and WAIT for vlib_thread_start() and signal back just before loop starts.
     * This allows call to vlib_thread_{register_event,set_exit_signal} before start */
    LOG_VERBOSE(vthread->log, "thread: waiting for launch");
    priv->state = (priv->state & ~VTS_CREATING) | VTS_CREATED | VTS_WAITING;
    pthread_cond_signal(&priv->cond);
    select_ret = pthread_cond_wait(&priv->cond, &priv->mutex);

    LOG_DEBUG(vthread->log, "pthread_cond_wait exit ret=%d state=%d",
              select_ret, priv->state);
    priv->state &= ~VTS_WAITING;

    if ((priv->state & VTS_EXIT_REQUESTED) != 0) {
        LOG_VERBOSE(vthread->log, "thread: exit requested before start -> exit");
        priv->state |= VTS_FINISHED;
        pthread_mutex_unlock(&priv->mutex);
        return thread_result;
    }

    priv->state |= VTS_STARTED;
    /* call the VTE_INIT callbacks just before starting select loop */
    SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
        if (data != NULL && data->callback != NULL && (data->event & VTE_INIT) != 0) {
            ret = data->callback(vthread, VTE_INIT, NULL, data->callback_data);
        }
    }
    priv->state |= VTS_RUNNING;
    LOG_VERBOSE(vthread->log, "thread: launched");

    while ((priv->state & (VTS_RUNNING | VTS_EXIT_REQUESTED)) == VTS_RUNNING) {
        /* if exit_signal has changed, block it (will be unlocked by pselect) */
        if (!sigismember(&block_sigset, priv->exit_signal)) {
            int new_exit_signal = priv->exit_signal;
            priv->exit_signal = old_exit_signal;
            if (vlib_thread_set_exit_signal_internal(vthread, new_exit_signal, &block_sigset) != 0) {
                priv->state |= VTS_ERROR;
                break ;
            }
        }
        old_exit_signal = priv->exit_signal;

        /* fill the read, write, err fdsets, and manage new registered signals  */
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&errfds);
        SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
            if (data != NULL) {
                if ((data->event & VTE_FD_READ) != 0) {
                    FD_SET(data->ev.fd, &readfds); if (data->ev.fd > fd_max) fd_max = data->ev.fd;
                }
                if ((data->event & VTE_FD_WRITE) != 0) {
                    FD_SET(data->ev.fd, &writefds); if (data->ev.fd > fd_max) fd_max = data->ev.fd;
                }
                if ((data->event & VTE_FD_ERR) != 0) {
                    FD_SET(data->ev.fd, &errfds); if (data->ev.fd > fd_max) fd_max = data->ev.fd;
                }
                if ((data->event & VTE_SIG) != 0
                && !sigismember(&block_sigset, data->ev.sig)) {
                    //TODO: removal from block_sigset of unregitered signals.
                    sigaddset(&block_sigset, data->ev.sig);
                    if (pthread_sigmask(SIG_SETMASK, &block_sigset, NULL) != 0) {
                        LOG_ERROR(vthread->log, "error: pthread_sigmask(): %s", strerror(errno));
                        priv->state |= VTS_ERROR;
                        break ;
                    }
                }
            }
        }
        if ((priv->state & VTS_ERROR) != 0) {
            break ;
        }

        /* set select timeout */
        if (priv->process_timeout > 0) {
            p_select_timeout = &select_timeout;
            p_select_timeout->tv_sec = priv->process_timeout / 1000;
#           ifndef VLIB_THREAD_PSELECT
            p_select_timeout->tv_usec = (priv->process_timeout % 1000) * 1000;
#           else
            /* even if pselect does not modify the timespec, we allow dynamic timeout value */
            p_select_timeout->tv_nsec = (priv->process_timeout % 1000) * 1000000;
#           endif
        } else {
            p_select_timeout = NULL;
        }

        /* -------------------------------------------- */
        LOG_DEBUG(vthread->log, "start select timeout=%d exit_signal ismember ? %d",
                  priv->process_timeout, sigismember(&select_sigset, priv->exit_signal));

        priv->state |= VTS_WAITING;
#       ifndef VLIB_THREAD_PSELECT
        pthread_sigmask(SIG_SETMASK, &select_sigset, &sigset_bak);
        pthread_mutex_unlock(&priv->mutex);
        select_ret = select(fd_max + 1, &readfds, &writefds, &errfds, p_select_timeout);
        last_signal = s_last_signal;
        select_errno = errno;
        LOG_DEBUG(vthread->log, "select return %d errno %d", select_ret, select_errno);
        pthread_sigmask(SIG_SETMASK, &sigset_bak, NULL);
        pthread_mutex_lock(&priv->mutex);
#       else
        pthread_mutex_unlock(&priv->mutex);
        select_ret = pselect(fd_max + 1, &readfds, &writefds, &errfds,
                             p_select_timeout, &select_sigset);
        last_signal = s_last_signal;
        select_errno = errno;
        LOG_DEBUG(vthread->log, "pselect return %d errno %d", select_ret, select_errno);
        pthread_mutex_lock(&priv->mutex);
        LOG_DEBUG(vthread->log, "mutex relocked");
#       endif
        priv->state &= ~VTS_WAITING;
        /* -------------------------------------------- */

        /* we have just been released by select, call callbacks with select result
         * and update pthread_sigmask in case of configuration change */
        SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
            if (data != NULL && data->callback != NULL
            && (data->event & VTE_PROCESS_START) != 0) {
                ret = data->callback(vthread, VTE_PROCESS_START,
                                             (void *)((long) select_ret),
                                             data->callback_data);
            }
        }

        if (select_ret == 0) {
            LOG_VERBOSE(vthread->log, "thread: select timeout");
            continue ;
        }
        if (select_ret < 0 && select_errno == EINTR) {
            LOG_VERBOSE(vthread->log, "thread: interrupted by signal: %s", strsignal(last_signal));
            /* check callbacks registered to signal */
            SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
                if (data != NULL && data->callback != NULL
                && (data->event & VTE_SIG) != 0 && (last_signal == data->ev.sig)) {
                    ret = data->callback(vthread, VTE_SIG, VTE_DATA_SIG(data->ev.sig),
                                                  data->callback_data);
                }
            }
            continue ;
        }
        if (select_ret < 0) {
            LOG_VERBOSE(vthread->log, "thread: select error: %s", strerror(select_errno));
            priv->state |= VTS_ERROR;
            break ;
        }
        SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
            if (data != NULL && data->callback != NULL) {
                if ((data->event & VTE_FD_READ) != 0 && FD_ISSET(data->ev.fd, &readfds)) {
                    ret = data->callback(vthread, VTE_FD_READ, VTE_DATA_FD(data->ev.fd),
                                            data->callback_data);
                }
                if ((data->event & VTE_FD_WRITE) != 0 && FD_ISSET(data->ev.fd, &writefds)) {
                    ret = data->callback(vthread, VTE_FD_WRITE, VTE_DATA_FD(data->ev.fd),
                                            data->callback_data);
                }
                if ((data->event & VTE_FD_ERR) != 0 && FD_ISSET(data->ev.fd, &errfds)) {
                    ret = data->callback(vthread, VTE_FD_ERR, VTE_DATA_FD(data->ev.fd),
                                            data->callback_data);
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
    priv->state = (priv->state & ~VTS_RUNNING) | VTS_FINISHING;

    SLIST_FOREACH_DATA(priv->event_list, data, vlib_thread_event_data_t *) {
        if (data != NULL) {
            if (data->callback != NULL && (data->event & VTE_CLEAN) != 0) {
                ret = data->callback(vthread, VTE_CLEAN, (void *) NULL,
                                            data->callback_data);
            }
            if ((data->event & (VTE_FD_READ | VTE_FD_WRITE | VTE_FD_ERR)) != 0) {
                close(data->ev.fd);
            }
        }
    }

    priv->state = (priv->state & ~VTS_FINISHING) | VTS_FINISHED;
    pthread_mutex_unlock(&priv->mutex);

    return thread_result;
}

/*****************************************************************************/
static int                      vlib_thread_closefd(
                                    vlib_thread_t *             vthread,
                                    vlib_thread_event_t         event,
                                    void *                      event_data,
                                    void *                      callback_user_data) {
    int fd = (int)((long) callback_user_data);
    (void) vthread;
    (void) event;
    (void) event_data;

    return close(fd);
}

/*****************************************************************************/
static void vlib_thread_ctx_destroy(vlib_thread_t * vthread) {
    if (vthread == NULL)
        return ;
    vlib_thread_priv_t  * priv = (vlib_thread_priv_t *) vthread->priv;
    if (priv) {
        slist_free(priv->event_list, free);
        pthread_mutex_destroy(&priv->mutex);
        pthread_cond_destroy(&priv->cond);
        free(priv);
    }
    free(vthread);
}

/*****************************************************************************/
/** signal the running thread about configuration change */
static int vlib_thread_notify(vlib_thread_t * vthread) {
    vlib_thread_priv_t * priv = vthread->priv;

    if ((priv->state & VTS_RUNNING) != 0) {
        if (pthread_kill(vthread->tid, priv->exit_signal) != 0) {
            LOG_ERROR(vthread->log, "error: pthread_kill(): %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

/*****************************************************************************/
static int          vlib_thread_register_event_unlocked(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data) {
    vlib_thread_priv_t *        priv = vthread->priv;
    vlib_thread_event_data_t *  ev;
    slist_t *                   new;

    if ((ev = malloc(sizeof(vlib_thread_event_data_t))) == NULL) {
        LOG_ERROR(vthread->log, "error: cannot malloc event_data : %s", strerror(errno));
        return -1;
    }
    ev->event = event;
    ev->ev.fd = (int)((long) event_data); // TODO
    ev->callback = callback;
    ev->callback_data = callback_user_data;
    new = slist_prepend(priv->event_list, ev);
    if (new == priv->event_list) {
        LOG_ERROR(vthread->log, "error: cannot add in event_list : %s", strerror(errno));
        free(ev);
        return -1;
    }
    priv->event_list = new;

    return 0;
}

/*****************************************************************************/
static int          vlib_thread_set_exit_signal_internal(
                            vlib_thread_t *             vthread,
                            int                         exit_signal,
                            sigset_t *                  block_sigset) {
    vlib_thread_priv_t *    priv = vthread ? vthread->priv : NULL;
    struct sigaction        sa;

    /* sanity checks */
    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    if (exit_signal == priv->exit_signal) {
        return 0;
    }
    LOG_VERBOSE(vthread->log, "replace exit_signal %d by %d",
                priv->exit_signal, exit_signal);
    sigemptyset(&sa.sa_mask);

    /* check wheter SIG_SPECIAL_VALUE is not a valid signal */
    sa.sa_handler = SIG_DFL;
    sa.sa_flags = 0;
    if (sigaction(SIG_SPECIAL_VALUE, &sa, NULL) == 0) {
        LOG_ERROR(vthread->log, "error: sigaction(%d) should have been rejected. "
                                "Choose another value for SIG_SPECIAL_VALUE",
                  SIG_SPECIAL_VALUE);
        return -1;
    }
    /* restore previous signal handler if any */
    if (priv->exit_signal != SIG_SPECIAL_VALUE) {
        sa.sa_handler = SIG_DFL;
        sa.sa_flags = 0;
        if (sigaction(priv->exit_signal, &sa, NULL) != 0) {
            LOG_ERROR(vthread->log, "sigaction uninstall exit_signal: %s", strerror(errno));
            return -1;
        }
        sigdelset(block_sigset, priv->exit_signal);
    }
    if (exit_signal != SIG_SPECIAL_VALUE) {
        /* add the new signal to the block mask */
        sigaddset(block_sigset, exit_signal);
        sigaddset(block_sigset, SIGINT);
    }
    /* set the new signal mask */
    if (pthread_sigmask(SIG_SETMASK, block_sigset, NULL) != 0) {
        LOG_ERROR(vthread->log, "error: pthread_sigmask(): %s", strerror(errno));
        return -1;
    }
    if (exit_signal != SIG_SPECIAL_VALUE) {
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = vlib_thread_sig_handler;
        /* install signal handler on exit_signal */
        if (sigaction(exit_signal, &sa, NULL) < 0) {
            LOG_ERROR(vthread->log, "error sigaction(%d): %s", exit_signal, strerror(errno));
            return -1;
        }
    }
    priv->exit_signal = exit_signal;

    return 0;
}

/*****************************************************************************/
static void vlib_thread_sig_handler(int sig) {
     s_last_signal = sig;
}

/*****************************************************************************/
#ifdef VALGRIND_DEBUG_WORKAROUND
/* This is workaround to SIGSEGV occuring during pthread_join when
 * valgrind is running the program
 * I am not sure yet if it is a problem on my side, on valgrind side or
 * on apple side, but some posts on the net are assuming that this is
 * valgrind or apple:
 * see:
 *  + https://bugs.kde.org/show_bug.cgi?id=349128
 *  + https://web.archive.org/web/20160304041047/https://bytecoin.org/blog/pthread-bug-osx
 *  + https://stackoverflow.com/questions/36576251/stdthread-join-sigsegv-on-mac-os-under-valgrind#37920222
 */
int vlib_thread_valgrind(int argc, const char *const* argv) {
    static int valgrind = -1;

    if (valgrind != -1)
        return valgrind;
    if (argv == NULL || argc < 1)
        return 0;

    int mib[] = {
	    CTL_KERN,
	    KERN_PROCARGS,
        getpid()
    };
    size_t kargv_len = 0;
    char * kargv;

    valgrind = 0;
    if (sysctl(mib, sizeof(mib) / sizeof(*mib), NULL, &kargv_len, NULL, 0) < 0) {
	    LOG_ERROR(g_vlib_log, "sysctl(NULL): %s", strerror(errno));
        return -1;
    }
    if ((kargv = malloc(kargv_len * sizeof(char))) == NULL) {
        LOG_ERROR(g_vlib_log, "malloc argv: %s", strerror(errno));
        valgrind = -1; // try again
	    return -1;
    }
    if (sysctl(mib, sizeof(mib) / sizeof(*mib), kargv, &kargv_len, NULL, 0) < 0) {
        LOG_ERROR(g_vlib_log, "sysctl(buf): %s", strerror(errno));
        free(kargv);
	    return -1;
    }
    LOG_DEBUG(g_vlib_log, "*** ARGV(%lu):", kargv_len);
    for (size_t i = 0; i < kargv_len; ) {
        int n = strlen(kargv + i) + 1;
        LOG_DEBUG(g_vlib_log, "%04lu:<%s>", i, kargv + i);
        if (argv && strcmp(kargv + i, *argv) == 0)
            break ;
        if (strstr(kargv + i, "valgrind") != NULL
        || strstr(kargv + i, "memcheck") != NULL) {
            valgrind = 1;
            break ;
        }
        if (n >= 0)
            i += n + 1;
        else
            break;
    }
    LOG_VERBOSE(g_vlib_log, "*** VALGRIND: %d\n", valgrind);
    free(kargv);
    return valgrind;
}
#else
int vlib_thread_valgrind(int argc, const char *const* argv) {
    (void) argc;
    (void) argv;
    return 0;
}
#endif

/*****************************************************************************/
int callback(
        vlib_thread_t * vthread,
        vlib_thread_event_t event,
        void * event_data,
        void * user_data) {
    (void) vthread;
    (void) event;
    (void) event_data;
    (void) user_data;
    return 0;
}
void testcb() {
    int fd = 1;
    vlib_thread_t * vthread = NULL;
    int sig = 1;
    const char * str = "aaa";
    vlib_thread_register_event(vthread, VTE_FD_READ, VTE_DATA_FD(fd), callback, vthread);
    vlib_thread_register_event(vthread, VTE_INIT, VTE_DATA_PTR(str), callback, vthread);
    vlib_thread_register_event(vthread, VTE_SIG, VTE_DATA_SIG(sig), callback, vthread);
}


