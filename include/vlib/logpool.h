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
 * log utilities: structure holding a pool of log_t, sharing their files.
 * To see the full git history, some of theses functions were before in log.c.
 */
#ifndef VLIB_LOGPOOL_H
#define VLIB_LOGPOOL_H

#include "vlib/log.h"
#include "vlib/slist.h"

#ifdef __cplusplus
extern "C" {
#endif

/*****************************************************************************/
typedef enum {
    LOGPOOL_FLAG_DEFAULT        = LOG_FLAG_CUSTOM << 0,
    LOGPOOL_FLAG_TEMPLATE       = LOG_FLAG_CUSTOM << 1,
    LOGPOOL_FLAG_PATTERN        = LOG_FLAG_CUSTOM << 2,
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
 * @param enable 0 to disable, others to enable
 * @param prev_enable if not NULL, where to store previous enable state */
int                 logpool_enable(
                        logpool_t *         pool,
                        log_t *             log,
                        int                 enable,
                        int *               prev_enable);

/** logpool_add()
 * Add or replace a copy of log in the pool.
 * If new entry is added, its use_count is 1,
 * otherwise the use_count of a replaced log entry is unchanged.
 * @param pool the logpool
 * @param log the log to duplicate and add in pool
 * @param path the file to be used for log or NULL to use log->out.
 * @return the new added log_t* on success, NULL otherwise */
log_t *             logpool_add(
                        logpool_t *         pool,
                        log_t *             log,
                        const char *        path);

/** logpool_release()
 * - log entry removed only when its use counter is 0 (erno EBUSY).
 *   each call to logpool_getlog() increments the use counter.
 *   call to logpool_add() increments use counter ONLY if new entry is added.
 * - Log entry templates (created with logpool_create_from_cmdline)
 *   are not removed (errno EACCES).
 * @return 0 on success, -1 on error, positive value (use counter) if not removed. */
int                 logpool_release(
                        logpool_t *         pool,
                        log_t *             log);

/** logpool_remove()
 * @notes: Warning: log use counter and template flag are not checked.
 *         use logpool_release() for a safe removal.
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
    LPG_NO_PATTERN  = 1 << 2,
    LPG_DEFAULT     = LPG_TRUEPREFIX
} logpool_getlog_flags_t;

/** logpool_getlog()
 * Look for a compatible log into pool and return it duplicated or not.
 * @param pool the log pool
 * @param prefix the log prefix to look for.
 * @param flags
 *        + LPG_NO_DEFAULT: return NULL rather than default log if not found
 *        + LPG_NO_PATTERN: don't look for a matching pattern in the pool
 *        + LPG_TRUEPREFIX: if matching log has not the same prefix, return a
 *                          copy of it, with updated prefix.
 * @return log entry if found, NULL otherwise
 * @notes: each call to logpool_getlog() increments an internal use counter
 *         which is decremented by logpool_release(). */
log_t *             logpool_getlog(
                        logpool_t *         pool,
                        const char *        prefix,
                        int                 flags);

/** logpool_memorysize()
 * complexity: O(n)
 * @return estimation of memory used by the logpool (except size of FILE structures)
 *         or 0 on error with errno set (errno is NOT changed on success). */
size_t              logpool_memorysize(
                        logpool_t *         pool);

/** logpool_print()
 * complexity: O(n)
 * @return 0 */
 int                 logpool_print(
                        logpool_t *         pool,
                        log_t *             log);

/** struct used by logpool_findbypath and logpool_replacefile */
typedef struct {
    log_t *         log;
    char *          path;
} logpool_logpath_t;

/** free list of logpool_logpath_t
 * to be called after usage of logpool_findbypath() or logpool_replacefile() */
int                 logpool_logpath_free(
                        logpool_t *         pool,
                        slist_t *           list);

/** return list of logs using the specified file
 * complexity O(n) (<number_of_matchs*4> malloc/free needed after call)
 * @param pool the logpool
 * @param path the pattern to match path of log file, NULL to find stdout/stderr
 * @return slist_t * list (of logpool_logpath_t *),
 *         to be freed with logpool_logpath_free() */
slist_t *           logpool_findbypath(
                        logpool_t *         pool,
                        const char *        path);

/** replace files of log in logs list
 * complexity: O(number of logs to be replaced)
 * @param pool the logpool
 * @param logs the list of logs (logpool_logpath_t *),
 *        or NULL to replace logs using stdout/stderr
 * @param newpath the path to use for logs, or NULL to use paths stored in list
 * @param [OUT] the pointer to backup list of logpool_logpath_t *,
 *         to be freed with logpool_logpath_free(), or NULL to disable backup.
 * @return 0 on success, negative value on fatal error,
 *         or number of non-fatal errors (positive). */
int                 logpool_replacefile(
                        logpool_t *         pool,
                        slist_t *           logs,
                        const char *        newpath,
                        slist_t **          pbackup);

/** change the logpool log rotation parameters
 * complexity: O(1)
 * @param pool the logpool
 * @param log_max_size the maximum size of a log before being rotated
 * @param log_max_rotate the maximum number of log rotations,
 * @param p_log_max_size if not NULL, previous log_max_size is put inside
 * @param p_log_max_rotate if not NULL, previous log_max_rotate is put inside,
 * @return 0 on success, negative value on error */
int                 logpool_set_rotation(
                        logpool_t *         pool,
                        size_t              log_max_size,
                        unsigned char       log_max_rotate,
                        size_t *            p_log_max_size,
                        unsigned char *     p_log_max_rotate);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

