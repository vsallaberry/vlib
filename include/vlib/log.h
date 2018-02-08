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

#include <sys/time.h>

#ifdef __cplusplus
# include <cstdio>
# include <ctime>
extern "C" {
#else
# include <stdio.h>
# include <time.h>
#endif

#include "slist.h"

/** Supported Log Levels */
typedef enum {
    LOG_LVL_NONE    = 0,
    LOG_LVL_ERROR   = 1,
    LOG_LVL_WARN    = 2,
    LOG_LVL_INFO    = 3,
    LOG_LVL_VERBOSE = 4,
    LOG_LVL_DEBUG   = 5,
    LOG_LVL_SCREAM  = 6,
    LOG_LVL_NB      = 7,
    LOG_LVL_DEFAULT = LOG_LVL_INFO
} log_level_t;

/** Supported Log Flags */
typedef enum {
    LOG_FLAG_NONE       = 0,
    LOG_FLAG_DATETIME   = 1 << 0,   /* insert date/time */
    LOG_FLAG_MODULE     = 1 << 1,   /* insert module name (log_ctx_t.prefix) */
    LOG_FLAG_LEVEL      = 1 << 2,   /* insert level name (ERR,WRN,...) */
    LOG_FLAG_PID        = 1 << 3,   /* insert process id */
    LOG_FLAG_TID        = 1 << 4,   /* insert thread id */
    LOG_FLAG_FILE       = 1 << 5,   /* insert current logging source file */
    LOG_FLAG_FUNC       = 1 << 6,   /* insert current logging function in source */
    LOG_FLAG_LINE       = 1 << 7,   /* insert current logging line in source */
    LOG_FLAG_LOC_TAIL   = 1 << 8,   /* append file,func,line at the end of log line */
    LOG_FLAG_LOC_ERR    = 1 << 9,   /* print file,func,line only on levels err,wrn,>=debug */
    LOG_FLAG_CLOSEFILE  = 1 << 29,  /* the file will be closed by destroy/close if not std* */
    LOG_FLAG_FREEPREFIX = 1 << 30,  /* log_ctx_t is considered allocated and freed on destroy */
    LOG_FLAG_DEFAULT    = LOG_FLAG_DATETIME | LOG_FLAG_MODULE | LOG_FLAG_LEVEL
                        | LOG_FLAG_LOC_ERR | LOG_FLAG_LOC_TAIL
                        | LOG_FLAG_FILE | LOG_FLAG_FUNC | LOG_FLAG_LINE
                        | LOG_FLAG_CLOSEFILE,
} log_flag_t;

/** log context */
typedef struct {
    log_level_t     level;
    FILE *          out;
    log_flag_t      flags;
    char *          prefix;
} log_ctx_t;

# define    LOG_USE_VA_ARGS
/**
 * The are the MACROS to use for logging.
 * See xlog() doc below for details, parameters, return value.
 */
# ifdef LOG_USE_VA_ARGS
#  define   LOG_ERROR(ctx,...)      xlog(LOG_LVL_ERROR,ctx,   __FILE__,__func__,__LINE__,__VA_ARGS__)
#  define   LOG_WARN(ctx,...)       xlog(LOG_LVL_WARN,ctx,    __FILE__,__func__,__LINE__,__VA_ARGS__)
#  define   LOG_INFO(ctx,...)       xlog(LOG_LVL_INFO,ctx,    __FILE__,__func__,__LINE__,__VA_ARGS__)
#  define   LOG_VERBOSE(ctx,...)    xlog(LOG_LVL_VERBOSE,ctx, __FILE__,__func__,__LINE__,__VA_ARGS__)
#  define   LOG_BUFFER(lvl,ctx,buf,sz,...) \
                xlog_buffer(lvl, ctx, buf, sz, __FILE__,__func__,__LINE__,__VA_ARGS__);
#  ifdef _DEBUG
#   define  LOG_DEBUG(ctx,...)      xlog(LOG_LVL_DEBUG,ctx,   __FILE__,__func__,__LINE__,__VA_ARGS__)
#   define  LOG_SCREAM(ctx,...)     xlog(LOG_LVL_SCREAM,ctx,  __FILE__,__func__,__LINE__,__VA_ARGS__)
#   define  LOG_DEBUG_BUF(ctx,buf,sz,...) \
                LOG_BUFFER(LOG_LVL_DEBUG,ctx,buf,sz,__VA_ARGS__)
#  else
#   define  LOG_DEBUG(ctx,...)      0
#   define  LOG_DEBUG_BUF(ctx,...)  0
#   define  LOG_SCREAM(ctx,...)     0
#  endif /* ! _DEBUG */
# else /*! LOG_USE_VA_ARGS */
int     xlog_error(log_ctx_t *ctx, const char *fmt, ...);
int     xlog_warn(log_ctx_t *ctx, const char *fmt, ...);
int     xlog_info(log_ctx_t *ctx, const char *fmt, ...);
int     xlog_verbose(log_ctx_t *ctx, const char *fmt, ...);
int     xlog_debug(log_ctx_t *ctx, const char *fmt, ...);
int     xlog_debug_buffer(log_ctx_t *ctx, const char *fmt, ...);
int     xlog_scream(log_ctx_t *ctx, const char *fmt, ...);
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
 * The end-of-line ('\n') is appended at the end of log.
 * It is recommended to use macros LOG_ERROR, LOG_* rather than directly this function
 * in order to have the Location and level conditional features.
 *
 * @param level the level among log_level_t.
 *              Logging will not be done If level defined in ctx is inferior.
 * @param ctx the log context. If NULL, Log is done on stderr with level 0.
 * @param fmt the printf-like format string. If null, '\n' is displayed without header.
 * @param args the printf-like format arguments.
 *
 * @return the number of written chars.
 */
int         xlog(log_level_t level, log_ctx_t *ctx,
                 const char * file, const char * func, int line,
                 const char *fmt, ...);

/** Log a buffer with hex and ascii data. See xlog() */
int	        xlog_buffer(log_level_t     level,
                        log_ctx_t *     ctx,
                        const void *    pbuffer,
                        size_t          len,
                        const char * file, const char * func, int line,
                        const char *    fmt_header, ...);

log_ctx_t * xlog_create(log_ctx_t *from);
slist_t *   xlog_create_from_cmdline(slist_t * logs,
                                     const char * log_levels, const char *const* modules);
void        xlog_list_free(slist_t *logs);

int         xlog_header(log_level_t level, log_ctx_t *ctx,
                        const char * file, const char * func, int line);
int         xlog_list_prefixcmp(const void * vlist1_data, const void * vlist2_data);
int         xlog_list_prefixfind(const void * vvalue, const void *vlist_data);
void        xlog_close(log_ctx_t * ctx);
void        xlog_destroy(void * vlog);

#ifdef __cplusplus
}
#endif

#endif /* !ifndef *_H */

