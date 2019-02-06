/*
 * Copyright (C) 2017-2019 Vincent Sallaberry
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
#else
# include <stdio.h>
# include <time.h>
#endif

#include "vlib/slist.h"

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
    LOG_LVL_NB      = 7,
    LOG_LVL_DEFAULT = LOG_LVL_INFO
} log_level_t;

/** Supported Log Flags */
typedef enum {
    LOG_FLAG_UNKNOWN    = -1,
    LOG_FLAG_NONE       = 0,
    LOG_FLAG_DATETIME   = 1 << 0,   /* insert date/time */
    LOG_FLAG_MODULE     = 1 << 1,   /* insert module name (log_t.prefix) */
    LOG_FLAG_LEVEL      = 1 << 2,   /* insert level name (ERR,WRN,...) */
    LOG_FLAG_PID        = 1 << 3,   /* insert process id */
    LOG_FLAG_TID        = 1 << 4,   /* insert thread id */
    LOG_FLAG_FILE       = 1 << 5,   /* insert current logging source file */
    LOG_FLAG_FUNC       = 1 << 6,   /* insert current logging function in source */
    LOG_FLAG_LINE       = 1 << 7,   /* insert current logging line in source */
    LOG_FLAG_LOC_TAIL   = 1 << 8,   /* append file,func,line at the end of log line */
    LOG_FLAG_LOC_ERR    = 1 << 9,   /* print file,func,line only on levels err,wrn,>=debug */
    LOG_FLAG_ABS_TIME   = 1 << 10,  /* insert absolute monotonic timestamp */
    LOG_FLAG_CLOSEFILE  = 1 << 16,  /* the file will be closed by destroy/close if not std* */
    LOG_FLAG_FREEPREFIX = 1 << 17,  /* log_t.prefix is considered allocated and freed on destroy */
    LOG_FLAG_FREELOG    = 1 << 18,  /* the log will be freed on log_destroy() */
    LOG_FLAG_CUSTOM     = 1 << 24,  /* first bit available for log custom flags */
    LOG_FLAG_DEFAULT    = LOG_FLAG_DATETIME | LOG_FLAG_MODULE | LOG_FLAG_LEVEL
                        | LOG_FLAG_LOC_ERR | LOG_FLAG_LOC_TAIL
                        | LOG_FLAG_FILE | LOG_FLAG_FUNC | LOG_FLAG_LINE
                        | LOG_FLAG_CLOSEFILE | LOG_FLAG_FREELOG
} log_flag_t;

/** log context */
typedef struct {
    log_level_t     level;
    FILE *          out;
    log_flag_t      flags;
    char *          prefix;
} log_t;

# define    LOG_USE_VA_ARGS
# define    LOG_CHECK_LVL_BEFORE_CALL
/**
 * The are the MACROS to use for logging.
 * See vlog() doc below for details, parameters, return value.
 */
# ifdef LOG_USE_VA_ARGS
#  ifdef LOG_CHECK_LVL_BEFORE_CALL
    /* check if level is OK before to make the call */
#   define   LOG_CHECK_LOG(log, lvl, ...)                                           \
                ( ((log) == NULL || ((log_t*)(log))->level >= (lvl))                \
                  ? vlog((lvl), (log), __FILE__, __func__, __LINE__, __VA_ARGS__)   \
                  : 0)
#   define   LOG_CHECK_LOGBUF(log, lvl, buf, sz, ...)                               \
                ( ((log) == NULL || ((log_t*)(log))->level >= (lvl))                \
                  ? log_buffer((lvl),(log),(buf),(sz),__FILE__,__func__,__LINE__,__VA_ARGS__) \
                  : 0)
#  else
    /* let the function check the level */
#   define   LOG_CHECK_LOG(log, lvl, ...)                                           \
                vlog((lvl), (log), __FILE__, __func__, __LINE__, __VA_ARGS__)
#   define   LOG_CHECK_LOGBUF(log, lvl, buf, sz, ...)                               \
                log_buffer((lvl),(log),(buf),(sz),__FILE__,__func__,__LINE__,__VA_ARGS__)
#  endif /* ! ifdef LOG_CHECK_BEFORE_CALL */

#  define   LOG_ERROR(log,...)      LOG_CHECK_LOG(log, LOG_LVL_ERROR,   __VA_ARGS__)
#  define   LOG_WARN(log,...)       LOG_CHECK_LOG(log, LOG_LVL_WARN,    __VA_ARGS__)
#  define   LOG_INFO(log,...)       LOG_CHECK_LOG(log, LOG_LVL_INFO,    __VA_ARGS__)
#  define   LOG_VERBOSE(log,...)    LOG_CHECK_LOG(log, LOG_LVL_VERBOSE, __VA_ARGS__)
#  define   LOG_BUFFER(lvl,log,buf,sz,...) \
                LOG_CHECK_LOGBUF(log,lvl,buf,sz,__FILE__,__func__,__LINE__,__VA_ARGS__);
#  ifdef _DEBUG
#   define  LOG_DEBUG(log,...)      LOG_CHECK_LOG(log, LOG_LVL_DEBUG,   __VA_ARGS__)
#   define  LOG_SCREAM(log,...)     LOG_CHECK_LOG(log, LOG_LVL_SCREAM,  __VA_ARGS__)
#   define  LOG_DEBUG_BUF(log,buf,sz,...) \
                                    LOG_BUFFER(LOG_LVL_DEBUG,log,buf,sz,__VA_ARGS__)
#  else
#   define  LOG_DEBUG(log,...)      0
#   define  LOG_DEBUG_BUF(log,...)  0
#   define  LOG_SCREAM(log,...)     0
#  endif /* ! _DEBUG */
# else /*! LOG_USE_VA_ARGS */
int     log_error(log_t * log, const char * fmt, ...);
int     log_warn(log_t * log, const char * fmt, ...);
int     log_info(log_t * log, const char * fmt, ...);
int     log_verbose(log_t * log, const char * fmt, ...);
int     log_debug(log_t * log, const char * fmt, ...);
int     log_debug_buffer(log_t * log, const char * fmt, ...);
int     log_scream(log_t * log, const char * fmt, ...);
#  define   LOG_ERROR               log_error
#  define   LOG_WARN                log_warn
#  define   LOG_INFO                log_info
#  define   LOG_VERBOSE             log_verbose
#  define   LOG_DEBUG               log_debug
#  define   LOG_DEBUG_BUF           log_debug_buffer
#  define   LOG_SCREAM              log_scream
# endif /* ! LOG_USE_VA_ARGS */

/*
#ifdef LOG_DEBUG_HANG
# define DEBUG_HANG(cond, log) do { if ((cond)) { log_destroy(log); exit(254); } } while(0)
#else
# define DEBUG_HANG(cond, log) do {} while(0)
#endif
*/

/**
 * Log printf-like data.
 * The end-of-line ('\n') is appended at the end of log.
 * It is recommended to use macros LOG_ERROR, LOG_* rather than directly this function
 * in order to have the Location and level conditional features.
 *
 * @param level the level among log_level_t.
 *              Logging will not be done If level defined in log context is inferior.
 * @param log the log context. If NULL, Log is done on stderr with level 0.
 * @param fmt the printf-like format string. If null, '\n' is displayed without header.
 * @param args the printf-like format arguments.
 *
 * @return the number of written chars.
 */
int         vlog(log_level_t level, log_t * log,
                 const char * file, const char * func, int line,
                 const char * fmt, ...);

/** Log a buffer with hex and ascii data. See vlog() */
int	        log_buffer(log_level_t  level,
                       log_t *      log,
                       const void * pbuffer,
                       size_t       len,
                       const char * file, const char * func, int line,
                       const char * fmt_header, ...);

/** get name of a given level */
const char *log_level_name(log_level_t level);

/** get log level from name
 * @return level or LOG_LVL_NB if not found. */
log_level_t log_level_from_name(const char * name);

/** get name of a given flag
 * @return string representing flag or NULL if unknown. */
const char *log_flag_name(log_flag_t flag);

/** get log flag from name
 * @return flag or LOG_FLAG_UNKNOWN if not found. */
log_flag_t log_flag_from_name(const char * name);

/** default function describing the log-level command-line option
 *  to be called from opt_callback_t option handler -
 *  see vlib/log.h/OPT_DESCRIBE_OPTION */
int         log_describe_option(char * buffer, int * size, const char *const* modules,
                                slist_t * modules_list, const char *(module_get)(const void *));

log_t *     log_create(log_t * from);
int         log_header(log_level_t level, log_t * log,
                       const char * file, const char * func, int line);
void        log_close(log_t * log);
void        log_destroy(void * vlog);

/** set internal vlib log instance, shared between vlib components
 * @param log the new vlib log instance. If NULL, default will be used.
 * @return the previous vlib log instance
 */
log_t *     log_set_vlib_instance(log_t * log);

#ifdef __cplusplus
}
#endif

#endif /* !ifndef *_H */

