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

/** opaque struct logpool_s */
typedef struct logpool_s    logpool_t;

/** logpool_create_from_cmdline() */
logpool_t *         logpool_create_from_cmdline(
                        logpool_t *         pool,
                        const char *        log_levels,
                        const char *const*  modules);

/** logpool_free() */
void                logpool_free(
                        logpool_t *         pool);

/** logpool_prefixcmp() */
int                 logpool_prefixcmp(
                        const void *        vpool1_data,
                        const void *        vpool2_data);

/** logpool_prefixfind() */
int                 logpool_prefixfind(
                        const void *        vvalue,
                        const void *        vpool_data);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

