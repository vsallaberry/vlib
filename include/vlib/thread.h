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
#ifndef VLIB_THREAD_H
#define VLIB_THREAD_H

#include <signal.h>
#include <pthread.h>

#include "vlib/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/** vlib_thread_state bits */
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
} vlib_thread_state_t;

/** opaaue struct vlib_thread_priv_s */
struct vlib_thread_priv_s;

/** vlib_thread_t */
typedef struct {
    pthread_t                   tid;
    log_t *                     log;
    struct vlib_thread_priv_s * priv;
} vlib_thread_t;

/** vlib_thread_event */
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
} vlib_thread_event_t;

/** cast macros for the event_data parameter of vlib_thread_(un)register_event() */
#define VTE_DATA_FD(fd)         ((void *)((long)(fd)))
#define VTE_DATA_SIG(sig)       ((void *)((long)(sig)))
#define VTE_DATA_PTR(ptr)       ((void *)(ptr))

/** vlib thread callback : see vlib_thread_register_event() */
typedef int         (*vlib_thread_callback_t)(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data,
                            void *                      callback_user_data);

/** the default signal used to kill the thread,
 * can be changed with vlib_thread_set_exit_signal() */
#define VLIB_THREAD_EXIT_SIG    SIGUSR1

/** initialize a select thread which will be waiting for
 * vlib_thread_start() call, allowing customizations before start.
 * By default the thread will use VLIB_THREAD_EXIT_SIG to exit.
 *
 * @param timeout the select timeout in milli seconds, 0 to wait forever.
 *        if not 0, events VTE_PROCESS* will run if any on each timeout.
 * @param log the log instance to use in this thread. g_vlib_log is used if log is NULL.
 * @return the vlib_thread context, or NULL on error.
 */
vlib_thread_t *     vlib_thread_create(
                            unsigned long               timeout,
                            log_t *                     log);

/** start the thread */
int                 vlib_thread_start(
                            vlib_thread_t *             vthread);

/** stop and clean the thread: mandatory to clean resources
 * even if the thread exited before calling this function */
void *               vlib_thread_stop(
                            vlib_thread_t *             vthread);

/** register an action on the vlib thread
 * @param vthread the vlib thread context
 * @param event the type of event
 *   VTE_{INIT,CLEAN,PROCESS*}: event_data is ignored. This flags can be combined together.
 *   VTE_FD_{READ,WRITE,ERR}: event_data is fd. This flags can be combined together.
 *   VTE_SIG: event_data is signal value. This flag cannot be combined.
 * @param event_data see parameter 'action'
 * @param callback the callback to be called on this event
 * @param callback_user_data the pointer to be passed to callback
 * @return 0 on SUCCESS, other value on error
 */
int                 vlib_thread_register_event(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data);

/** unregister an action on the vlib thread
 * @param vthread the vlib thread context
 * @param event the type of event
 *   VTE_{INIT,CLEAN,PROCESS*}: event_data is ignored. This flags can be combined together.
 *   VTE_FD_{READ,WRITE,ERR}: event_data is fd. This flags can be combined together.
 *   VTE_SIG: event_data is signal value. This flag cannot be combined.
 * @param event_data see parameter 'action'
 * @return 0 on SUCCESS, other value on error
 */
int                 vlib_thread_unregister_event(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data);

/** setup a custom signal to stop the thread
 * @param vthread the vlib thread context
 * @param exit_signal, the signal used to finish the thread
 *        use default VLIB_THREAD_EXIT_SIG
 * @return 0 on SUCCESS, other value on error
 */
int                 vlib_thread_set_exit_signal(
                            vlib_thread_t *             vthread,
                            int                         exit_signal);

/** create a pipe whose in_fd will be registered by thread. This function
 * is a shortcut to vlib_thread_register_event, with additionally pipe
 * creation/cleaning, SIGPIPE for caller is ignored (SIGIGN) if not handled (SIGDFL).
 * Kernel guaranties atomic writes of PIPE_BUF.
 * @param vthread the vlib thread context
 * @param callback the callback to be called on this event
 * @param callback_user_data the pointer to be passed to callback
 * @return the out_fd of the created pipe on -1 on error
 */
int                 vlib_thread_pipe_create(
                            vlib_thread_t *             vthread,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data);

/** write on the pipe. If size exceeds PIPE_BUF, thread mutex is locked.
 * @param vthread the vlib thread context
 * @param pipefd_out the fd to write on
 * @param data the data the be written
 * @param size the size to be written
 * @return number of written bytes or -1 on error */
ssize_t             vlib_thread_pipe_write(
                            vlib_thread_t *             vthread,
                            int                         pipe_fdout,
                            void *                      data,
                            size_t                      size);

/** get thread state
 * @param vthread the vlib thread context
 * @return state the bit combination of vlib_thread_state_t
 */
unsigned int        vlib_thread_state(
                            vlib_thread_t *             vthread);

/** to be called at start of program with argc > 0 and *argv valid.
 * next calls can be done with argc == 0 and argv == NULL
 * @return 1 if valgrind was detected, 0 otherwise */
int vlib_thread_valgrind(int argc, const char *const* argv);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

