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
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "vlib/thread.h"
#include "vlib/slist.h"

#define SIG_SPECIAL_VALUE 0 /* FIXME */
#define SIG_KILL_THREAD     SIGUSR1 /* FIXME */

typedef struct vlib_thread_priv_s {
    vlib_thread_callback_t      init;
    vlib_thread_callback_t      clean;
    vlib_thread_callback_t      process_start;
    vlib_thread_callback_t      process_end;
    void *                      callback_data;
    slist_t *                   fd_list;
    struct timespec             select_timeout;
} vlib_thread_priv_t;

static pthread_mutex_t          flag_mutex = PTHREAD_MUTEX_INITIALIZER;

static void flag_sig_handler(int sig, siginfo_t * sig_info,  void * data) {
    /* list of registered threads */
    static struct threadlist_s {
        pthread_t tid; volatile sig_atomic_t * running; struct threadlist_s * next;
    } *         threadlist = NULL;
    pthread_t   tself = pthread_self();

    if (sig != SIG_SPECIAL_VALUE) {
        if (sig_info && sig_info->si_pid == getpid()) {
            /* looking for tid in threadlist and update running if found, then delete entry */
            for (struct threadlist_s * prev = NULL, * cur = threadlist; cur; prev = cur, cur = cur->next) {
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
    sigaddset(&block, SIG_KILL_THREAD);
    pthread_sigmask(SIG_BLOCK, &block, &save);
    pthread_mutex_lock(&flag_mutex);
    /* special mode to delete threadlist, but if thread exited normally this should not be necessary */
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

