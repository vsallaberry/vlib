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
 * Simple utilities.
 */
#include <errno.h>
#include <signal.h>

#include "vlib/time.h"
#include "vlib_private.h"
#include "version.h"

#ifndef VLIB_CLOCK_GETTIME_WRAPPER
/* _POSIX_C_SOURCE >= 199309L */
/* assume that clock_gettime exists on this system */
/* # define vclock_gettime clock_gettime */
inline int vclock_gettime(int id, struct timespec * ts) {
    return clock_gettime(id, ts);
}
#elif defined(__APPLE__) || defined(BUILD_SYS_darwin)
/* apple/darwin implementation */
# include <mach/mach.h>
# include <mach/mach_time.h> /* for mach_timebase_info/mach_absolute_time */
# include <mach/clock.h>    /* for clock_get_time */

int vclock_gettime(int id, struct timespec * ts) {
    uint64_t                            abstime = mach_absolute_time();
    static struct mach_timebase_info    timebase;
    static volatile kern_return_t       status = KERN_INVALID_HOST;
    static volatile sig_atomic_t        init_done = 0;

    /* TODO: not thread-safe */
    if (init_done == 0 && ++init_done) {
        status = mach_timebase_info(&timebase);
        if (status != KERN_SUCCESS)
            LOG_VERBOSE(g_vlib_log, "%s(): error mach_timebase_info() %d", __func__, status);
        if (status == KERN_INVALID_HOST)
            status = KERN_INVALID_ARGUMENT;
    } else while (status == KERN_INVALID_HOST) ;

    if (ts == NULL || (id != CLOCK_MONOTONIC && id != CLOCK_MONOTONIC_RAW)) {
        errno = EINVAL;
        return -1;
    }
    ts->tv_sec = (abstime * timebase.numer / timebase.denom) / 1000000000;
    ts->tv_nsec = (abstime * timebase.numer / timebase.denom) % 1000000000;
    return 0;
}

int vclock_getrealtime(int id, struct timespec * ts) {
    static int              init_done = 0;
    static host_t           host;
    static clock_serv_t     clock_serv;
    mach_timespec_t         mts;
    (void)id;

    if (!init_done) {
        /* TODO i don't think it is thread safe */
        host = mach_host_self();
        if (host_get_clock_service(host, REALTIME_CLOCK, &clock_serv) != KERN_SUCCESS) {
            LOG_VERBOSE(g_vlib_log, "%s(): mach host_get_clock_service() error", __func__);
            errno = EINVAL;
            return -1;
        }
        init_done = 1;
    }

    if (clock_get_time(clock_serv, &mts) != KERN_SUCCESS) {
        LOG_VERBOSE(g_vlib_log, "%s(): mach clock_get_time() error", __func__);
        errno = EINVAL;
        return -1;
    }
    ts->tv_sec = mts.tv_sec;
    ts->tv_nsec = mts.tv_nsec;
    return 0;
}
#else
/* not apple/darwin and no clock_gettime -> use gettimeofday() */
# pragma message "warning, CLOCK_MONOTONIC (and maybe clock_gettime()) is not defined, using gettimeofday."
int vclock_gettime(int id, struct timespec * ts) {
    struct timeval tv;
    if (gettimeofday(&tv, NULL) < 0) {
        LOG_VERBOSE(g_vlib_log, "%s(): error gettimeofday: %s", __func__, strerror(errno));
        return -1;
    }
    ts->tv_sec = tv.tv_sec;
    ts->tv_nsec = tv.tv_usec * 1000;
    return 0;
}
#endif


