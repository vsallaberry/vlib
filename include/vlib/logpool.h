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
 * log utilities: structure holding a pool of log_t, sharing their files.
 * To see the full git history, some of theses functions were before in log.c.
 */
#ifndef VLIB_LOGPOOL_H
#define VLIB_LOGPOOL_H

#include "vlib/log.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
typedef enum {
    LOGPOOL_FLAG_DEFAULT        = LOG_FLAG_CUSTOM << 0,
    LOGPOOL_FLAG_TEMPLATE       = LOG_FLAG_CUSTOM << 1,
} logpool_flag_t;

/*****************************************************************************/

/** opaque struct logpool_s */
typedef struct logpool_s    logpool_t;

/** logpool_create(): create empty logpool
 * return create logpool on success, NULL otherwise */
logpool_t *         logpool_create();

/** logpool_free() */
void                logpool_free(
                        logpool_t *         pool);

/** logpool_create_from_cmdline()
 * return create logpool on success, NULL otherwise */
logpool_t *         logpool_create_from_cmdline(
                        logpool_t *         pool,
                        const char *        log_levels,
                        const char *const*  modules);

/** logpool_enable()
 * Enable/Disable logs in the pool.
 * Currently only NULL value for log parameter is supported.
 * @param pool the logpool
 * @param log a particular log to enable/disable or NULL for all
 * @param enable 0 to disable, others to enable */
int                 logpool_enable(
                        logpool_t *         pool,
                        log_t *             log,
                        int                 enable);

/** logpool_add()
 * Add a copy of log to the pool.
 * @param pool the logpool
 * @param log the log to duplicate and add in pool
 * @param path the file to be used for log or NULL to use log->out.
 * @return the new added log_t* on success, NULL otherwise */
log_t *             logpool_add(
                        logpool_t *         pool,
                        log_t *             log,
                        const char *        path);

/** logpool_remove()
 * @return 0 on success, -1 otherwise */
int                 logpool_remove(
                        logpool_t *         pool,
                        log_t *             log);

/** logpool_find()
 * @return log entry if found, NULL otherwise */
log_t *             logpool_find(
                        logpool_t *         pool,
                        const char *        prefix);

/** logpool_getlog() flags */
typedef enum {
    LPG_NONE        = 0,
    LPG_NODEFAULT   = 1 << 0,
    LPG_TRUEPREFIX  = 1 << 1,
    LPG_DEFAULT     = LPG_TRUEPREFIX
} logpool_getlog_flags_t;

/** logpool_getlog()
 * Look for a compatible log into pool and return it duplicated or not.
 * @param pool the log pool
 * @param prefix the log prefix to look for.
 * @param flags
 *        + LPG_NO_DEFAULT: return NULL rather than default log if not found
 *        + LPG_TRUEPREFIX: if matching log has not the same prefix, return a
 *                          copy of it, with updated prefix.
 * @return log entry if found, NULL otherwise */
log_t *             logpool_getlog(
                        logpool_t *         pool,
                        const char *        prefix,
                        int                 flags);

/** logpool_memorysize()
 * complexity: O(1)
 * @return estimation of memory used by the logpool (except size of FILE structures)
 *         or 0 on error with errno set (errno is NOT changed on success). */
size_t              logpool_memorysize(
                        logpool_t *         pool);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

