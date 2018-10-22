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

#ifdef __cplusplus
extern "C" {
#endif

/** opaaue struct vlib_thread_priv_s */
struct vlib_thread_priv_s;

/** vlib_thread_t */
typedef struct {
    pthread_t                   tid;
    struct vlib_thread_priv_s * priv;
} vlib_thread_t;

/** vlib_thread_action */
typedef enum {
    VTA_NONE            = 0,
    VTA_SIG             = 1 << 0, /* action_data is SIG id */
    VTA_INIT            = 1 << 1, /* action_data is NULL */
    VTA_CLEAN           = 1 << 2, /* action_data is NULL */
    VTA_PROCESS_START   = 1 << 3, /* action_data is NULL */
    VTA_PROCESS_END     = 1 << 4, /* action_data is NULL */
    VTA_FD_READ         = 1 << 5, /* action_data is fd */
    VTA_FD_WRITE        = 1 << 6, /* action_data is fd */
    VTA_FD_ERR          = 1 << 7, /* action_data is fd */
} vlib_thread_action_t;

typedef int         (*vlib_thread_callback_t)(
                            vlib_thread_t *             vthread,
                            vlib_thread_action_t        action,
                            void *                      action_data,
                            void *                      user_data);

/** initialize a select thread
 * @return the vlib_thread context, or NULL on error. */
vlib_thread_t *     vlib_thread_create(
                            vlib_thread_callback_t      init_callback,
                            vlib_thread_callback_t      clean_callback,
                            vlib_thread_callback_t      process_start_callback,
                            vlib_thread_callback_t      process_end_callback,
                            struct timespec *           process_timeout,
                            void *                      callback_user_data);

/** register a flag for modification on signal reception */
int                 vlib_thread_sig_register_flag(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            sig_atomic_t *              flag_ptr,
                            unsigned int                value);

/** unregister a flag for modification on signal reception */
int                 vlib_thread_sig_unregister_flag(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            sig_atomic_t *              flag_ptr);

/** register a function on a signal reception */
int                 vlib_thread_sig_register_func(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            vlib_thread_callback_t      callback,
                            void *                      callback_data);

/** unregister a function on a signal reception */
int                 vlib_thread_sig_unregister_func(
                            vlib_thread_t *             vthread,
                            int                         sig,
                            vlib_thread_callback_t      callback);

/** register a fd for the select call */
int                 vlib_thread_fd_register(
                            vlib_thread_t *             vthread,
                            int                         fd,
                            vlib_thread_action_t        action,
                            vlib_thread_callback_t      callback,
                            void *                      callback_user_data);

/** unregister a fd for the select call */
int                 vlib_thread_fd_unregister(
                            vlib_thread_t *             vthread,
                            int                         fd,
                            vlib_thread_action_t        action,
                            vlib_thread_callback_t      callback);


#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

