/*
 * Copyright (C) 2018-2020,2023 Vincent Sallaberry
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
#ifndef VLIB_THREAD_H
#define VLIB_THREAD_H

#include <stdlib.h>
#include <signal.h>
#include <pthread.h>

#include "vlib/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/** vthread_state bits */
typedef enum {
    VTS_NONE            = 0,
    VTS_CREATING        = 1 << 0,
    VTS_CREATED         = 1 << 1,
    VTS_STARTED         = 1 << 2,
    VTS_RUNNING         = 1 << 3,
    VTS_FINISHING       = 1 << 4,
    VTS_FINISHED        = 1 << 5,
    VTS_WAITING         = 1 << 7,
    VTS_ERROR           = 1 << 8,
    VTS_EXIT_REQUESTED  = 1 << 15,
} vthread_state_t;

/** vthread result */
#define VTHREAD_RESULT_OK           (((PTHREAD_CANCELED) == (void *) 1) ? (void *) -1 : (void *) 1)
#define VTHREAD_RESULT_CANCELED     (PTHREAD_CANCELLED)
#define VTHREAD_RESULT_ERROR        (NULL)

/** opaque struct vthread_priv_s */
struct vthread_priv_s;

/** vthread_t */
typedef struct {
    pthread_t                   tid;
    log_t *                     log;
    struct vthread_priv_s * priv;
} vthread_t;

/** vthread_event */
typedef enum {
    VTE_NONE            = 0,
    VTE_SIG             = 1 << 0, /* action_data is SIG id */
    VTE_INIT            = 1 << 1, /* action_data is NULL */
    VTE_CLEAN           = 1 << 2, /* action_data is NULL */
    VTE_PROCESS_START   = 1 << 3, /* action_data is NULL */
    VTE_PROCESS_END     = 1 << 4, /* action_data is NULL */
    VTE_FD_READ         = 1 << 5, /* action_data is fd */
    VTE_FD_WRITE        = 1 << 6, /* action_data is fd */
    VTE_FD_ERR          = 1 << 7, /* action_data is fd */
} vthread_event_t;

/** cast macros for the event_data parameter of vthread_(un)register_event() */
#define VTE_DATA_FD(fd)         ((void *)((ssize_t)(fd)))
#define VTE_DATA_SIG(sig)       ((void *)((ssize_t)(sig)))
#define VTE_DATA_PTR(ptr)       ((void *)(ptr))

/** cast macros for parameter event_data of event callback handlers */
#define VTE_FD_DATA(data)       ((int)((ssize_t)(data)))
#define VTE_SIG_DATA(data)      ((int)((ssize_t)(data)))

/** vlib thread callback : see vthread_register_event() */
typedef int         (*vthread_callback_t)(
                            vthread_t *             vthread,
                            vthread_event_t         event,
                            void *                  event_data,
                            void *                  callback_user_data);

/** initialize a select thread which will be waiting for
 * vthread_start() call, allowing customizations before start.
 * (vthread_register_event(), cthread_pipe_create(), ...).
 *
 * @param timeout the select timeout in milli seconds, 0 to wait forever.
 *        if not 0, events VTE_PROCESS* will run if any on each timeout.
 * @param log the log instance to use in this thread. g_vlib_log is used if log is NULL.
 * @return the vthread context, or NULL on error.
 */
vthread_t *         vthread_create(
                            unsigned long           timeout,
                            log_t *                 log);

/** start the thread */
int                 vthread_start(
                            vthread_t *             vthread);

/** stop and clean the thread: mandatory to clean resources
 * even if the thread exited before calling this function
 * @return VTHREAD_RESULT_ERROR (or NULL with errno set) on error,
 *         VTHREAD_RESULT_CANCELED if thread was canceled,
 *         VTHREAD_RESULT_OK or pointer (or NULL with errno=0) on success. */
void *               vthread_stop(
                            vthread_t *             vthread);

/** register an action on the vlib thread
 * @param vthread the vlib thread context
 * @param event the type of event
 *   VTE_{INIT,CLEAN,PROCESS*}: event_data is ignored. This flags can be combined together.
 *   VTE_FD_{READ,WRITE,ERR}: event_data is fd. This flags can be combined together.
 *   VTE_SIG: event_data is signal value. This flag cannot be combined.
 * @param event_data see parameter 'event'
 * @param callback the callback to be called on this event.
 *        thread will exit if callback returns negative value.
 * @param callback_user_data the pointer to be passed to callback
 * @return 0 on SUCCESS, other value on error
 */
int                 vthread_register_event(
                            vthread_t *             vthread,
                            vthread_event_t         event,
                            void *                  event_data,
                            vthread_callback_t      callback,
                            void *                  callback_user_data);

/** unregister an action on the vlib thread
 * @param vthread the vlib thread context
 * @param event the type of event
 *   VTE_{INIT,CLEAN,PROCESS*}: event_data is ignored. This flags can be combined together.
 *   VTE_FD_{READ,WRITE,ERR}: event_data is fd. This flags can be combined together.
 *   VTE_SIG: event_data is signal value. This flag cannot be combined.
 * @param event_data see parameter 'action'
 * @return 0 on SUCCESS, other value on error
 */
int                 vthread_unregister_event(
                            vthread_t *             vthread,
                            vthread_event_t         event,
                            void *                  event_data);

/** create a pipe whose in_fd will be registered by thread. This function
 * is a shortcut to vthread_register_event, with additionally pipe
 * creation/cleaning, SIGPIPE for caller is ignored (SIGIGN) if not handled (SIGDFL).
 * Kernel guaranties atomic writes of PIPE_BUF.
 * @param vthread the vlib thread context
 * @param callback the callback to be called on this event
 * @param callback_user_data the pointer to be passed to callback
 * @return the out_fd of the created pipe on -1 on error
 */
int                 vthread_pipe_create(
                            vthread_t *             vthread,
                            vthread_callback_t      callback,
                            void *                  callback_user_data);

/** write on the pipe. If size exceeds PIPE_BUF, thread mutex is locked.
 * @param vthread the vlib thread context
 * @param pipefd_out the fd to write on
 * @param data the data the be written
 * @param size the size to be written
 * @return number of written bytes or -1 on error */
ssize_t             vthread_pipe_write(
                            vthread_t *             vthread,
                            int                     pipe_fdout,
                            const void *            data,
                            size_t                  size);

/** get thread state
 * @param vthread the vlib thread context
 * @return state the bit combination of vthread_state_t
 */
unsigned int        vthread_state(
                            vthread_t *             vthread);

/** can be called inside the vthread event callbacks (vthread_callback_t) to add a
 * cancelation point for pthread_cancel().
 * @notes: calling this function implicitly re-enable the
 * kill mode then restore it before return. */
void                vthread_testkill();

/** turn on or off asynchronous thread kill mode
 * to be called from the vthread event callbacks.
 * @param enable mode : if 0 thread cannot be killed unless during
 *   vthread_testkill() called by vthread event callbacks;
 * @param async mode : if not 0, thread can be killed at any moment
 *   as long as kill mode is enabled.
 *   (dangerous if system does not clean mutex at thread exit)
 *   if 0, thread can be killed at system cancelation points (refer to
 *   man 3 pthread_setcancelstate), as long as kill mode is enabled.
 * @param old_enable, if not NULL, the previous enable state is stored here.
 * @param old_async, if not NULL, the previous async state is stored here.
 * @return 0 on success, -1 on error */
int                 vthread_killmode(
                            int                 enable,
                            int                 async,
                            int *               old_enable,
                            int *               old_async);

/** to be called at start of program with argc > 0 and *argv valid.
 * next calls can be done with argc == 0 and argv == NULL
 * @return 1 if valgrind was detected, 0 otherwise */
int vthread_valgrind(int argc, const char *const* argv);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

