/*
 * Copyright (C) 2017-2018 Vincent Sallaberry
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
 * Simple Log Utilities.
 */
#ifndef VLIB_LOG_H
#define VLIB_LOG_H

#include <stdio.h>

#include "slist.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Supported Log Levels */
typedef enum {
    LOG_LVL_NONE    = 0,
    LOG_LVL_ERROR   = 1,
    LOG_LVL_WARN    = 2,
    LOG_LVL_INFO    = 3,
    LOG_LVL_VERBOSE = 4,
    LOG_LVL_DEBUG   = 5,
    LOG_LVL_SCREAM  = 6,
    LOG_LVL_NB      = 7
} log_level_t;

# define    LOG_PREFIX_SZ   20
typedef struct {
    log_level_t     level;
    FILE *          out;
    unsigned int    flags;
    char            prefix[LOG_PREFIX_SZ];
} log_ctx_t;

# define    LOG_LVL_DEFAULT     LOG_LVL_INFO

# define    LOG_USE_VA_ARGS

# ifdef LOG_USE_VA_ARGS
#  define   LOG_ERROR(ctx,...)      xlog(LOG_LVL_ERROR,ctx,   __VA_ARGS__)
#  define   LOG_WARN(ctx,...)       xlog(LOG_LVL_WARN,ctx,    __VA_ARGS__)
#  define   LOG_INFO(ctx,...)       xlog(LOG_LVL_INFO,ctx,    __VA_ARGS__)
#  define   LOG_VERBOSE(ctx,...)    xlog(LOG_LVL_VERBOSE,ctx, __VA_ARGS__)
#  ifdef _DEBUG
#   define  LOG_DEBUG(ctx,...)      xlog(LOG_LVL_DEBUG,ctx,   __VA_ARGS__)
#   define  LOG_SCREAM(ctx,...)     xlog(LOG_LVL_SCREAM,ctx,  __VA_ARGS__)
#   define  LOG_DEBUG_BUF(ctx,buf,sz,...) xlog_buffer(LOG_LVL_DEBUG,ctx,buf,sz,__VA_ARGS__)
#  else
#   define  LOG_DEBUG(ctx,...)
#   define  LOG_DEBUG_BUF(ctx,...)
#   define  LOG_SCREAM(ctx,...)
#  endif /* ! _DEBUG */
# else /*! LOG_USE_VA_ARGS */
#  define   LOG_ERROR               xlog_error
#  define   LOG_WARN                xlog_warn
#  define   LOG_INFO                xlog_info
#  define   LOG_VERBOSE             xlog_verbose
#  define   LOG_DEBUG               xlog_debug
#  define   LOG_DEBUG_BUF           xlog_debug_buffer
#  define   LOG_SCREAM              xlog_scream
# endif /* ! LOG_USE_VA_ARGS */

/*
#ifdef LOG_DEBUG_HANG
# define DEBUG_HANG(cond, ctx) do { if ((cond)) { log_destroy(ctx); exit(254); } } while(0)
#else
# define DEBUG_HANG(cond, ctx) do {} while(0)
#endif
*/

/**
 * Log printf-like data.
 * @param level the level among log_level_t.
 *              Logging will not be done If level defined in ctx is inferior.
 * @param ctx the log context. If NULL, Log is done on stderr with level 0.
 * @param fmt the printf-like format string.
 * @param args the printf-like format arguments.
 * @return the number of written chars.
 */
int         xlog(log_level_t level, log_ctx_t *ctx, const char *fmt, ...);

/** Log a buffer with hex and ascii data. See xlog() */
int	xlog_buffer(log_level_t level, log_ctx_t *ctx,
                const void * pbuffer, size_t len,
                const char *fmt_header, ...);


log_ctx_t * xlog_create(log_ctx_t *from);
slist_t *   xlog_create_from_cmdline(slist_t * logs,
                                     const char * log_levels, const char *const* modules);
void        xlog_list_free(slist_t *logs);

int         xlog_header(log_level_t level, log_ctx_t *ctx);
int         xlog_list_prefixcmp(const void * vlist1_data, const void * vlist2_data);
int         xlog_list_prefixfind(const void * vvalue, const void *vlist_data);
void        xlog_close(log_ctx_t * ctx);
void        xlog_close_and_free(void * vlog);

#ifdef __cplusplus
}
#endif

#endif /* !ifndef *_H */
