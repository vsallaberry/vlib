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
 *   VTE_SIG: event_data is signal value, callback is flag pointer,
 *            callback_user_data is flag value. This flag cannot be combined.
 * @param event_data see parameter 'action'
 * @param callback the callback to be called on this event (or flag ptr for VTE_SIG)
 * @param callback_user_data the pointer to be passed to callback (or flag val for VTE_SIG)
 * @return 0 on SUCCESS, other value on error
 */
int                 vlib_thread_register_event(
                            vlib_thread_t *             vthread,
                            vlib_thread_event_t         event,
                            void *                      event_data,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data);

/** setup a custom signal to stop the thread
 * @param vthread the vlib thread context
 * @param exit_signal, the signal used to finish the thread
 *        use default VLIB_THREAD_EXIT_SIG
 * @return 0 on SUCCESS, other value on error
 */
int                 vlib_thread_set_exit_signal(
                            vlib_thread_t *             vthread,
                            int                         exit_signal);

int vlib_thread_valgrind(int argc, const char *const* argv);
/*
/ ** register a flag for modification on signal reception * /
int                 vlib_thread_sig_register_flag(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            sig_atomic_t *              flag_ptr,
                            unsigned int                value);

/ ** unregister a flag for modification on signal reception * /
int                 vlib_thread_sig_unregister_flag(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            sig_atomic_t *              flag_ptr);

/ ** register a function on a signal reception * /
int                 vlib_thread_sig_register_func(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            vlib_thread_callback_t      callback,
                            void *                      callback_data);

/ ** unregister a function on a signal reception * /
int                 vlib_thread_sig_unregister_func(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            vlib_thread_callback_t      callback);

/ ** register a fd for the select call * /
int                 vlib_thread_fd_register(
                            vlib_thread_t *             vthread,
                            int                         fd,
                            vlib_thread_action_t        action,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data);

/ ** unregister a fd for the select call * /
int                 vlib_thread_fd_unregister(
                            vlib_thread_t *             vthread,
                            int                         fd,
                            vlib_thread_action_t        action,
                            vlib_thread_callback_t      callback);
*/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

