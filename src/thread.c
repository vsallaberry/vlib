/*
 * Copyright (C) 2018-2020 Vincent Sallaberry
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
#include "vlib/util.h"
#include "vlib/job.h"

#include "vlib_private.h"

/*****************************************************************************/
#define VLIB_THREAD_PSELECT

#if defined(_DEBUG) && (defined(__APPLE__) || defined(BUILD_SYS_darwin))
# include <sys/types.h>
# include <sys/param.h>
# include <sys/sysctl.h>
# define VALGRIND_DEBUG_WORKAROUND 1
#endif

/*****************************************************************************/
typedef struct vthread_priv_s {
    pthread_mutex_t             mutex;
    pthread_rwlock_t            pipe_mutex;
    pthread_cond_t              cond;
    sigset_t                    block_sigset;
    slist_t *                   event_list;
    unsigned long               process_timeout;
    vthread_state_t             state;
    int                         control_fd;
} vthread_priv_t;

/*****************************************************************************/
typedef struct {
    vthread_event_t             event;
    union {
        int                     fd;
        void *                  ptr;
        int                     sig;
    } ev;
    vthread_callback_t          callback;
    void *                      callback_data;
} vthread_event_data_t;

/*****************************************************************************/
typedef int             vthread_control_t; // data passed to control fd
typedef unsigned int    vthread_control_len_t; //type of control length if given
#define VTHREAD_CONTROL_LENGTH  ((vthread_control_t) (0x80000000))
#define VTHREAD_CONTROL_MASK    ((vthread_control_t) (0x0fffffff))
#define VTHREAD_CONTROL_NOTIFY  ((vthread_control_t) (0x01000000))

/*****************************************************************************/
static void *                   vthread_body(void * data);
static void                     vthread_ctx_destroy(vthread_t * vthread);
static int                      vthread_notify(vthread_t * vthread);
static int                      vthread_control_pipe_cb(
                                    vthread_t *             vthread,
                                    vthread_event_t         ev,
                                    void *                  ev_data,
                                    void *                  cb_data);
static int                      vthread_closefd(
                                    vthread_t *             vthread,
                                    vthread_event_t         event,
                                    void *                  event_data,
                                    void *                  callback_user_data);
static int                      vthread_register_event_unlocked(
                                    vthread_t *             vthread,
                                    vthread_event_t         event,
                                    void *                  event_data,
                                    vthread_callback_t      callback,
                                    void *                  callback_user_data);
static int                      vthread_event_cmp(const void * vev1, const void * vev2);
static int                      vthread_ignore_sigpipe(vthread_t * vthread);
static void                     vthread_sig_handler(int sig);
static volatile sig_atomic_t    s_last_signal = 0;

/*****************************************************************************/
vthread_t *     vthread_create(
                            unsigned long               process_timeout,
                            log_t *                     log) {
    vthread_t *         vthread = NULL;
    vthread_priv_t *    priv = NULL;

    /* vthread_t allocation and initialization */
    if ((vthread = calloc(1, sizeof(vthread_t))) == NULL) {
        LOG_ERROR(g_vlib_log, "cannot malloc vthread_t: %s", strerror(errno));
        return NULL;
    }
    if (log) {
        vthread->log = log;
    } else {
        vthread->log = g_vlib_log;
    }
    if ((priv = calloc(1, sizeof(vthread_priv_t))) == NULL) {
        LOG_ERROR(vthread->log, "cannot malloc vthread_priv_t: %s", strerror(errno));
        vthread_ctx_destroy(vthread);
        return NULL;
    }
    vthread->result = VTHREAD_RESULT_CANCELED;
    vthread->priv = priv;
    priv->state = VTS_CREATING;
    priv->process_timeout = process_timeout;
    pthread_mutex_init(&priv->mutex, NULL);
    pthread_rwlock_init(&priv->pipe_mutex, NULL);
    pthread_cond_init(&priv->cond, NULL);
    sigemptyset(&priv->block_sigset);

    if (vthread_ignore_sigpipe(vthread) != 0
    ||  (priv->control_fd = vthread_pipe_create(vthread, vthread_control_pipe_cb, NULL)) < 0) {
        LOG_ERROR(vthread->log, "cannot create thread control fd: %s", strerror(errno));
        vthread_ctx_destroy(vthread);
        return NULL;
    }

    /* create and run the thread */
    if (pthread_create(&vthread->tid, NULL, vthread_body, vthread) != 0) {
        LOG_ERROR(vthread->log, "pthread_create error: %s", strerror(errno));
        vthread_ctx_destroy(vthread);
        return NULL;
    }

    LOG_VERBOSE(vthread->log, "thread: created");
    return vthread;
}

/*****************************************************************************/
int                 vthread_start(
                            vthread_t *             vthread) {
    vthread_priv_t * priv = vthread ? (vthread_priv_t *) vthread->priv : NULL;
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
void *                  vthread_stop(
                            vthread_t *             vthread) {
    vthread_priv_t  *   priv = vthread ? (vthread_priv_t *) vthread->priv : NULL;
    void *              ret_val;
    int                 ret;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return VTHREAD_RESULT_ERROR;
    }

    LOG_DEBUG(vthread->log, "locking priv_mutex...");
    pthread_mutex_lock(&priv->mutex);

    /* set the thread state to EXIT */
    priv->state |= VTS_EXIT_REQUESTED;

    if ((priv->state & (VTS_CREATED | VTS_CREATING)) != 0) {
        ret = pthread_cond_signal(&priv->cond);
        LOG_DEBUG(vthread->log, "pthread_cond_signal ret %d", ret);
#      ifndef _DEBUG
        (void)ret;
#      endif
    }

    /* signal the running thread about configuration change */
    vthread_notify(vthread);

    /* finally wait for end of thread and destroy its context */
    pthread_mutex_unlock(&priv->mutex);
    ret_val = vthread_wait_and_free(vthread);

    return ret_val;
}

void * vthread_wait_and_free(vthread_t * vthread) {
    vthread_priv_t  *   priv = vthread ? (vthread_priv_t *) vthread->priv : NULL;
    void *              ret_val;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return VTHREAD_RESULT_ERROR;
    }

#   ifdef VALGRIND_DEBUG_WORKAROUND
    int valgrind = vthread_valgrind(0, NULL);
    if (valgrind) {
        vthread_state_t state = priv->state;
        LOG_DEBUG(vthread->log, "VALGRIND: waiting thread...");
        while ((state & VTS_FINISHED) == 0) {
            pthread_mutex_lock(&priv->mutex);
            state = priv->state;
            pthread_mutex_unlock(&priv->mutex);
            usleep(10000);
        }
        usleep(50000);
        ret_val = vthread->result;
    } else
#   endif
    {
        LOG_DEBUG(vthread->log, "pthread_join...");
        pthread_join(vthread->tid, &ret_val);
    }

    LOG_DEBUG(vthread->log, "destroy vthread and return join:%lx, ret:%lx...",
              (unsigned long)((size_t)ret_val), (unsigned long)((size_t)vthread->result));
    ret_val = vthread->result;
    vthread_ctx_destroy(vthread);

    return ret_val;
}

/*****************************************************************************/
unsigned int        vthread_state(
                            vthread_t *             vthread) {
    vthread_priv_t *    priv = vthread ? vthread->priv : NULL;
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
int                 vthread_register_event(
                            vthread_t *             vthread,
                            vthread_event_t         event,
                            void *                      event_data,
                            vthread_callback_t      callback,
                            void *                      callback_user_data) {
    vthread_priv_t *    priv = vthread ? (vthread_priv_t *) vthread->priv : NULL;
    int                     ret;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    LOG_VERBOSE(vthread->log, "register event %d ev_data:%lx callback_data:%lx",
                event, (long) event_data, (long) callback_user_data);

    pthread_mutex_lock(&priv->mutex);
    ret = vthread_register_event_unlocked(vthread, event, event_data,
                                              callback, callback_user_data);
    /* signal the thread about configuration change */
    vthread_notify(vthread);
    pthread_mutex_unlock(&priv->mutex);

    return ret;
}

/*****************************************************************************/
int                 vthread_unregister_event(
                            vthread_t *             vthread,
                            vthread_event_t         event,
                            void *                  event_data) {
    vthread_priv_t  *       priv = vthread ? (vthread_priv_t *) vthread->priv : NULL;
    vthread_event_data_t    ev;

    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    LOG_VERBOSE(vthread->log, "unregister event %d ev_data:%lx",
                event, (long) event_data);

    pthread_mutex_lock(&priv->mutex);

    ev.event = event;

    if ((event & (VTE_FD_READ | VTE_FD_WRITE | VTE_FD_ERR | VTE_FD_CLOSE)) != 0) {
        ev.ev.fd = (int) ((ssize_t) event_data);
    } else if ((event & VTE_SIG) != 0) {
        // we could have a global s_SIG_STATE[NSIG] with sa_bak and use_count
        // with restore of sa_bak if use_count becomes 0.
        ev.ev.sig = (int) ((ssize_t) event_data);
        sigaddset(&priv->block_sigset, ev.ev.sig);
    } else {
        ev.ev.ptr = event_data;
    }

    errno = 0;
    priv->event_list = slist_remove(priv->event_list, &ev, vthread_event_cmp, free);
    if (errno != 0) {
        LOG_WARN(vthread->log, "cannot remove event %u(%lx) from event_list : %s",
                 event, (unsigned long)ev.ev.ptr, strerror(errno));
        pthread_mutex_unlock(&priv->mutex);
        return -1;
    }

    /* signal the thread about configuration change */
    vthread_notify(vthread);
    pthread_mutex_unlock(&priv->mutex);

    return 0;
}

/*****************************************************************************/
int                 vthread_pipe_create(
                            vthread_t *             vthread,
                            vthread_callback_t      callback,
                            void *                  callback_user_data) {
    vthread_priv_t *        priv = vthread ? vthread->priv : NULL;
    int                     pipefd[2];
    int                     fd_flags;

    /* sanity checks */
    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    /* ignore SIGPIPE for caller if not already set or ignored */
    if (vthread_ignore_sigpipe(vthread) != 0) {
        return -1;
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
    /* register event read for pipefd_in, and event clean to close pipefd_out
     * note: as slist_prepend() is used, add pipefd_out last so as it will be first to close */
    if (vthread_register_event_unlocked(vthread, VTE_FD_READ | VTE_FD_CLOSE,
                                        VTE_DATA_FD(pipefd[0]), callback, callback_user_data) != 0
    ||  vthread_register_event_unlocked(vthread, VTE_CLEAN, NULL,
                                            vthread_closefd, VTE_DATA_FD(pipefd[1])) != 0) {
        LOG_ERROR(vthread->log, "error vthread_register_event");
        close(pipefd[1]);
        close(pipefd[0]);
        pipefd[1] = -1;
    }
    /* notify the thread about configuration change */
    vthread_notify(vthread);
    pthread_mutex_unlock(&priv->mutex);

    return pipefd[1];
}

/** ignore SIGPIPE for caller if not already set or ignored */
static int vthread_ignore_sigpipe(vthread_t * vthread) {
    struct sigaction    sa;

    if (sigaction(SIGPIPE, NULL, &sa) != 0) {
        LOG_ERROR(vthread->log, "error sigaction(get SIGPIPE): %s", strerror(errno));
        return -1;
    }
    if ((sa.sa_flags & SA_SIGINFO) == 0 && sa.sa_handler == SIG_DFL) {
        LOG_DEBUG(vthread->log, "thread: ignore SIGPIPE for vthread_pipe_create() caller");
        sigfillset(&sa.sa_mask);
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = SIG_IGN;
        if (sigaction(SIGPIPE, &sa, NULL) != 0) {
            LOG_ERROR(vthread->log, "error sigaction(set SIGPIPE): %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

/****************************************************************************/
static int write_nonblock(int fd, const void * buf, size_t size, unsigned int timeout_ms) {
    int ret;

    while ((ret = write(fd, buf, size)) < 0) {
        if (errno == EINTR)
            continue ;
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            struct timeval timeout;
            fd_set set;
            do {
                timeout.tv_sec = timeout_ms / 1000;
                timeout.tv_usec = (timeout_ms % 1000) * 1000;
                FD_ZERO(&set);
                FD_SET(fd, &set);
                ret = select(fd + 1, NULL, &set, NULL, &timeout);
            } while (ret < 0 && errno == EINTR);
            if (ret <= 0)
                return -1;
            continue ;
        }
        break ;
    }
    return ret;
}

/*****************************************************************************/
ssize_t                 vthread_pipe_write(
                            vthread_t *             vthread,
                            int                         pipe_fdout,
                            const void *                data,
                            size_t                      size) {
    vthread_priv_t *    priv = vthread ? vthread->priv : NULL;
    ssize_t                 ret;
    size_t                  offset;

    /* sanity checks */
    if (priv == NULL) {
        LOG_WARN(g_vlib_log, "bad thread context");
        return -1;
    }
    /* lock pipe lock */
    /* checks whether the size exceeds PIPE_BUF */
    if (size <= PIPE_BUF) {
        pthread_rwlock_rdlock(&priv->pipe_mutex);
        ret = write_nonblock(pipe_fdout, data, size, 1000);
        pthread_rwlock_unlock(&priv->pipe_mutex);
        return ret;
    }
    pthread_rwlock_wrlock(&priv->pipe_mutex);
    for (offset = 0; offset < size; /*no incr*/ ) {
        ret = write_nonblock(pipe_fdout, (const char *)data + offset,
                             offset + PIPE_BUF > size ? size - offset : PIPE_BUF, 1000);
        if (ret < 0) {
            pthread_rwlock_unlock(&priv->pipe_mutex);
            return ret;
        }
        offset += ret;
    }
    pthread_rwlock_unlock(&priv->pipe_mutex);
    return offset;
}

/* ************************************************************************ */
void vthread_testkill() {
    vjob_testkill();
}

/* ************************************************************************ */
int vthread_killmode(int enable, int async, int *old_enable, int *old_async) {
     return vjob_killmode(enable, async, old_enable, old_async);
}

/*****************************************************************************/
static int vthread_control_pipe_cb(
    vthread_t * vthread, vthread_event_t ev, void * ev_data, void * cb_data) {

    vthread_control_t       id;
    vthread_control_len_t   len = 0, total = 0;
    ssize_t                 n;
    (void)                  vthread;
    (void)                  cb_data;

    if (ev == VTE_FD_READ) {
        int fd = VTE_FD_DATA(ev_data);
        do {
            // read ID
            while ((n = read(fd, &id, sizeof(id))) < 0 && errno == EINTR)
                ; /* loop on EINTR */
            if (n == sizeof(id)) {
                // read LEN if BIT activated in ID
                if ((id & VTHREAD_CONTROL_LENGTH) != 0) {
                    char buf[128];
                    while ((n = read(fd, &len, sizeof(len))) < 0 && errno == EINTR)
                        ; /* loop on EINTR */
                    if (n != sizeof(len))
                        len = 0;
                    // READ <LEN> bytes of data / TODO
                    for (total = 0; total < len; total += n) {
                        size_t to_read = total + sizeof(buf) > len ? len-total : sizeof(buf);
                        while ((n = read(fd, buf, to_read)) < 0 && errno == EINTR)
                            ; /* loop */
                        if (n == -1 || ((size_t) n) < to_read)
                            break ;
                    }
                }
                switch(id & VTHREAD_CONTROL_MASK) {
                    case VTHREAD_CONTROL_NOTIFY:
                        break ;
                    default:
                        break ;
                }
            }
        } while (n > 0);
    }
    // non negative value will just stop the select() loop,
    // register/unregister events, check thread state, then re-loop.
    return 0;
}

/*****************************************************************************/
static void vthread_body_cleanup(void * data) {
    vthread_t *         vthread = (vthread_t *) data;
    vthread_priv_t *    priv    = vthread ? (vthread_priv_t *) vthread->priv : NULL;
    int                 ret;

    if (priv == NULL) {
        LOG_WARN(vthread->log, "thread: shutdown: bad context, cannot clean up");
        return ;
    }

    LOG_VERBOSE(vthread->log, "thread: shutting down");

    if ((priv->state & VTS_WAITING) != 0) {
        if ((priv->state & VTS_STARTED) != 0) {
            ret = pthread_mutex_lock(&(priv->mutex)); // we were interrupted by pthread_cancel while mutex not locked
        } else {
            // we were interrupted by pthread_cancel in pthread_cond_wait(). At least on macOS, pthread_testcancel()
            // in pthread_cond_wait can be called before the cond mutex is unlocked, that's whe we trylock.
            ret = pthread_mutex_trylock(&(priv->mutex));
        }
    } else {
        ret = 0;
    }
    (void)ret;
    LOG_DEBUG(vthread->log, "thread: state: %x, result: %lx, lockret: %d",
              priv->state, (unsigned long)((size_t)vthread->result), ret);

    priv->state = (priv->state & ~(VTS_RUNNING|VTS_WAITING)) | VTS_FINISHING;

    if ((priv->state & VTS_STARTED) != 0) {
        SLIST_FOREACH_DATA(priv->event_list, data, vthread_event_data_t *) {
            if (data->callback != NULL && (data->event & VTE_CLEAN) != 0) {
                data->callback(vthread, VTE_CLEAN, data->ev.ptr,
                        data->callback_data);
            }
            if ((data->event & (VTE_FD_CLOSE)) != 0) {
                close(data->ev.fd);
            }
        }
    }

    priv->state = (priv->state & ~VTS_FINISHING) | VTS_FINISHED;
    pthread_mutex_unlock(&priv->mutex);

    LOG_DEBUG(vthread->log, "vthread cleanup: exiting (result:%lx)",
              (unsigned long)((size_t)vthread->result));
}

/*****************************************************************************/
static void * vthread_body(void * data) {
    static const int        blocked_sigs[] = {
        SIGHUP, SIGINT, /*SIGPIPE,*/ SIGALRM, SIGTSTP, SIGTERM, SIGVTALRM, SIGPROF, SIGUSR1, SIGUSR2 };
    vthread_t *             vthread = (vthread_t *) data;
    vthread_priv_t *        priv = vthread ? (vthread_priv_t *) vthread->priv : NULL;
    fd_set                  readfds, writefds, errfds;
    struct sigaction        sa;
    int                     select_ret, select_errno, ret;
    volatile int            fd_max = -1;
    sig_atomic_t            last_signal;
    int                     thread_cancel_state;
#   ifndef VLIB_THREAD_PSELECT
    struct timeval          select_timeout, * p_select_timeout;
    sigset_t                sigset_bak;
#   else
    struct timespec         select_timeout, * p_select_timeout;
#   endif

    if (priv == NULL) {
        LOG_ERROR(g_vlib_log, "bad thread context");
        errno = EFAULT;
        return VTHREAD_RESULT_ERROR;
    }
    LOG_VERBOSE(vthread->log, "thread: initializing");
    pthread_mutex_lock(&priv->mutex);

    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_setcanceltype(PTHREAD_CANCEL_DEFERRED, NULL);

    if ((priv->state & VTS_EXIT_REQUESTED) != 0) {
        LOG_VERBOSE(vthread->log, "thread: exit requested before start -> exit");
        priv->state = VTS_FINISHED;
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
        return vthread->result;
    }

    // block 'user' signals which terminate the program.
    // (note: block_sigset emptied in vthread_create())
    for (size_t i = 0; i < PTR_COUNT(blocked_sigs); ++i) {
        int sig = blocked_sigs[i];
        //if SIG{INT,PROF,TERM} have default handler, do not block them.
        if ((sig == SIGINT || sig == SIGPROF || sig == SIGTERM)
        &&  sigaction(sig, NULL, &sa) == 0 && sa.sa_handler == SIG_DFL) {
            continue ;
        }
        sigaddset(&priv->block_sigset, sig);
    }
    pthread_sigmask(SIG_SETMASK, &priv->block_sigset, NULL);

    /* ignore SIGPIPE */
    if (vthread_ignore_sigpipe(vthread) != 0) {
        int errno_save = errno;
        priv->state = VTS_FINISHED | VTS_ERROR;
        pthread_cond_signal(&priv->cond);
        pthread_mutex_unlock(&priv->mutex);
        errno = errno_save;
        return VTHREAD_RESULT_ERROR;
    }

    /* from now pthread_cancel() when enabled will call vthread_body_cleanup() */
    pthread_cleanup_push(vthread_body_cleanup, vthread);

    /* notify vthread_start() we are ready,
     * and WAIT for vthread_start() and signal back just before loop starts.
     * This allows call to vthread_{register_event} before start */
    LOG_VERBOSE(vthread->log, "thread: waiting for launch");
    priv->state = (priv->state & ~VTS_CREATING) | VTS_CREATED;
    pthread_cond_signal(&priv->cond);
    pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);
    priv->state |= VTS_WAITING;
    select_ret = pthread_cond_wait(&priv->cond, &priv->mutex);
    priv->state &= ~VTS_WAITING;
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_pop(0);

    LOG_DEBUG(vthread->log, "pthread_cond_wait exit ret=%d state=%d",
              select_ret, priv->state);

    if ((priv->state & VTS_EXIT_REQUESTED) != 0) {
        LOG_VERBOSE(vthread->log, "thread: exit requested before start -> exit");
        priv->state |= VTS_FINISHED;
        pthread_mutex_unlock(&priv->mutex);
        return vthread->result;
    }
    pthread_cleanup_push(vthread_body_cleanup, vthread);
    priv->state |= VTS_STARTED;
    vthread->result = VTHREAD_RESULT_OK;

    /* call the VTE_INIT callbacks just before starting select loop */
    SLIST_FOREACH_DATA(priv->event_list, data, vthread_event_data_t *) {
        if (data != NULL && data->callback != NULL && (data->event & VTE_INIT) != 0) {
            ret = data->callback(vthread, VTE_INIT, data->ev.ptr, data->callback_data);
            if (ret < 0)
                priv->state |= VTS_EXIT_REQUESTED;
        }
    }
    priv->state |= VTS_RUNNING;
    LOG_VERBOSE(vthread->log, "thread: launched");

    while ((priv->state & (VTS_RUNNING | VTS_EXIT_REQUESTED)) == VTS_RUNNING) {
        /* fill the read, write, err fdsets, and manage new registered signals  */
        FD_ZERO(&readfds);
        FD_ZERO(&writefds);
        FD_ZERO(&errfds);
        SLIST_FOREACH_DATA(priv->event_list, data, vthread_event_data_t *) {
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
                &&  sigismember(&priv->block_sigset, data->ev.sig)) {
                    sigdelset(&priv->block_sigset, data->ev.sig);
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
        LOG_DEBUG(vthread->log, "start select timeout=%ld", priv->process_timeout);

        priv->state |= VTS_WAITING;

#      ifndef VLIB_THREAD_PSELECT
        pthread_sigmask(SIG_SETMASK, &priv->block_sigset, &sigset_bak);
        pthread_mutex_unlock(&priv->mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &thread_cancel_state);

        select_ret = select(fd_max + 1, &readfds, &writefds, &errfds, p_select_timeout);

        last_signal = s_last_signal;
        select_errno = errno;
        pthread_sigmask(SIG_SETMASK, &sigset_bak, NULL);
#      else
        pthread_mutex_unlock(&priv->mutex);
        pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, &thread_cancel_state);

        select_ret = pselect(fd_max + 1, &readfds, &writefds, &errfds,
                             p_select_timeout, &priv->block_sigset);
        last_signal = s_last_signal;
        select_errno = errno;
#      endif

        pthread_setcancelstate(thread_cancel_state, NULL);
        pthread_mutex_lock(&priv->mutex);

        LOG_DEBUG(vthread->log, "select return %d errno %d %s",
                  select_ret, select_errno, select_errno ? strerror(select_errno) : "");

        priv->state &= ~VTS_WAITING;
        LOG_DEBUG(vthread->log, "mutex relocked, cancelstate %d", thread_cancel_state);
        /* -------------------------------------------- */

        /* we have just been released by select, call callbacks with select result
         * and update pthread_sigmask in case of configuration change */
        SLIST_FOREACH_DATA(priv->event_list, data, vthread_event_data_t *) {
            if (data != NULL && data->callback != NULL
            && (data->event & VTE_PROCESS_START) != 0) {
                ret = data->callback(vthread, VTE_PROCESS_START,
                                             (void *)((long) select_ret),
                                             data->callback_data);
                if (ret < 0)
                    priv->state |= VTS_EXIT_REQUESTED;
            }
        }

        if (select_ret == 0) {
            LOG_VERBOSE(vthread->log, "thread: select timeout");
            continue ;
        }
        if (select_ret < 0 && select_errno == EINTR) {
            LOG_VERBOSE(vthread->log, "thread: interrupted by signal: %s", strsignal(last_signal));
            /* check callbacks registered to signal */
            SLIST_FOREACH_DATA(priv->event_list, data, vthread_event_data_t *) {
                if (data != NULL && data->callback != NULL
                && (data->event & VTE_SIG) != 0 && (last_signal == data->ev.sig)) {
                    ret = data->callback(vthread, VTE_SIG, VTE_DATA_SIG(data->ev.sig),
                                                  data->callback_data);
                    if (ret < 0)
                        priv->state |= VTS_EXIT_REQUESTED;
                }
            }
            continue ;
        }
        if (select_ret < 0) {
            LOG_VERBOSE(vthread->log, "thread: select error: %s", strerror(select_errno));
            priv->state |= VTS_ERROR;
            break ;
        }
        SLIST_FOREACH_DATA(priv->event_list, data, vthread_event_data_t *) {
            if (data != NULL && data->callback != NULL) {
                if ((data->event & VTE_FD_READ) != 0 && FD_ISSET(data->ev.fd, &readfds)) {
                    ret = data->callback(vthread, VTE_FD_READ, VTE_DATA_FD(data->ev.fd),
                                            data->callback_data);
                    if (ret < 0)
                        priv->state |= VTS_EXIT_REQUESTED;
                }
                if ((data->event & VTE_FD_WRITE) != 0 && FD_ISSET(data->ev.fd, &writefds)) {
                    ret = data->callback(vthread, VTE_FD_WRITE, VTE_DATA_FD(data->ev.fd),
                                            data->callback_data);
                    if (ret < 0)
                        priv->state |= VTS_EXIT_REQUESTED;
                }
                if ((data->event & VTE_FD_ERR) != 0 && FD_ISSET(data->ev.fd, &errfds)) {
                    ret = data->callback(vthread, VTE_FD_ERR, VTE_DATA_FD(data->ev.fd),
                                            data->callback_data);
                    if (ret < 0)
                        priv->state |= VTS_EXIT_REQUESTED;
                }
            }
        }
        /*
        if (priv->process_end) {
            ret = priv->process_end(vthread, VTE_PROCESS_END,
                                    (void *) select_ret, priv->callback_data);
        }*/
    } // ! while(running)

    // shutting down, generate VTE_CLEAN events, set finished state, unlock mutex
    pthread_setcancelstate(PTHREAD_CANCEL_DISABLE, NULL);
    pthread_cleanup_pop(0);

    vthread_body_cleanup(vthread);

    if (vthread->result == VTHREAD_RESULT_ERROR && errno != 0) {
        errno = 0;
    }

    return vthread->result;
}

/*****************************************************************************/
static int                      vthread_closefd(
                                    vthread_t *             vthread,
                                    vthread_event_t         event,
                                    void *                      event_data,
                                    void *                      callback_user_data) {
    int fd = (int)((long) callback_user_data);
    (void) vthread;
    (void) event;
    (void) event_data;

    return close(fd);
}

/*****************************************************************************/
static void vthread_ctx_destroy(vthread_t * vthread) {
    if (vthread == NULL)
        return ;
    vthread_priv_t  * priv = (vthread_priv_t *) vthread->priv;
    if (priv) {
        slist_free(priv->event_list, free);
        pthread_mutex_destroy(&priv->mutex);
        pthread_rwlock_destroy(&priv->pipe_mutex);
        pthread_cond_destroy(&priv->cond);
        close(priv->control_fd);
        vthread->priv = NULL;
        free(priv);
    }
    free(vthread);
}

/*****************************************************************************/
/** signal the running thread about configuration change
 * must be called under lock */
static int vthread_notify(vthread_t * vthread) {
    vthread_priv_t * priv = vthread->priv;

    if ((priv->state & VTS_RUNNING) != 0) {
        vthread_control_t id = VTHREAD_CONTROL_NOTIFY;
        if (vthread_pipe_write(vthread, priv->control_fd, &id, sizeof(id)) != sizeof(id)) {
            LOG_ERROR(vthread->log, "error: control_fd write: %s", strerror(errno));
            return -1;
        }
    }
    return 0;
}

/*****************************************************************************/
static int          vthread_register_event_unlocked(
                            vthread_t *             vthread,
                            vthread_event_t         event,
                            void *                  event_data,
                            vthread_callback_t      callback,
                            void *                  callback_user_data) {
    vthread_priv_t *        priv = vthread->priv;
    vthread_event_data_t *  ev;
    slist_t *               new;
    struct sigaction        sa_bak;

    if ((ev = malloc(sizeof(vthread_event_data_t))) == NULL) {
        LOG_ERROR(vthread->log, "error: cannot malloc event_data : %s", strerror(errno));
        return -1;
    }
    ev->event = event;
    ev->callback = callback;
    ev->callback_data = callback_user_data;

    if ((event & (VTE_FD_READ | VTE_FD_WRITE | VTE_FD_ERR | VTE_FD_CLOSE)) != 0) {
        ev->ev.fd = (int)((ssize_t) event_data);
    } else if ((event & VTE_SIG) != 0) {
        ev->ev.sig = (int)((ssize_t) event_data);
    } else {
        ev->ev.ptr = event_data;
    }

    if ((event & VTE_SIG) != 0) {
        // we could have a global s_SIG_STATE[NSIG] with sa_bak and use_count
        // with backup in sa_bak and incr of use_count on register.
        struct sigaction sa;

        if (sigaction(ev->ev.sig, NULL, &sa) == 0 && (sa.sa_flags & SA_SIGINFO) == 0
        && sa.sa_handler != vthread_sig_handler && sa.sa_handler != SIG_DFL) {
            LOG_WARN(vthread->log, "warning the signal %d handler is already handled", ev->ev.sig);
        }
        sigfillset(&sa.sa_mask);
        sa.sa_handler = vthread_sig_handler;
        sa.sa_flags = SA_RESTART;
        if (sigaction(ev->ev.sig, &sa, &sa_bak) != 0) {
            LOG_ERROR(vthread->log, "sigaction() error, event %u (%lx): %s",
                    event, (unsigned long) event_data, strerror(errno));
            free(ev);
            return -1;
        }
        sigdelset(&priv->block_sigset, ev->ev.sig);
    }

    new = slist_prepend(priv->event_list, ev);
    if (new == priv->event_list) {
        LOG_ERROR(vthread->log, "error: cannot add event %u (%lx) in event_list : %s",
                  event, (unsigned long) event_data, strerror(errno));
        if ((event & VTE_SIG) != 0 && sigaction(ev->ev.sig, &sa_bak, NULL) != 0) {
            LOG_WARN(vthread->log, "restore sigaction() error, event %u (%lx): %s",
                     event, (unsigned long) event_data, strerror(errno));
        }
        free(ev);
        return -1;
    }
    priv->event_list = new;

    return 0;
}

/****************************************************************************/
static int vthread_event_cmp(const void * vev1, const void * vev2) {
    const vthread_event_data_t * ev1 = (const vthread_event_data_t *) vev1;
    const vthread_event_data_t * ev2 = (const vthread_event_data_t *) vev2;

    if (!ev1 || !ev2)
        return ev1 - ev2;
    if (ev1->event != ev2->event)
        return ev1->event - ev2->event;

    if ((ev1->event & (VTE_FD_READ | VTE_FD_WRITE | VTE_FD_ERR | VTE_FD_CLOSE)) != 0) {
        return ev1->ev.fd - ev2->ev.fd;
    } else if ((ev1->event & VTE_SIG) != 0) {
        return ev1->ev.sig - ev2->ev.sig;
    }
    return (char*)ev1->ev.ptr - (char*)ev2->ev.ptr;
}

/*****************************************************************************/
static void vthread_sig_handler(int sig) {
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
int vthread_valgrind(int argc, const char *const* argv) {
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
int vthread_valgrind(int argc, const char *const* argv) {
    (void) argc;
    (void) argv;
    return 0;
}
#endif

/*****************************************************************************/

