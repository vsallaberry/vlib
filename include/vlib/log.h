/*
 * Copyright (C) 2017-2020,2023 Vincent Sallaberry
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
    LOG_FLAG_COLOR      = 1 << 11,  /* colorize header */
    LOG_FLAG_CLOSEFILE  = 1 << 14,  /* the file will be closed by destroy/close if not std* */
    LOG_FLAG_FREEPREFIX = 1 << 15,  /* log_t.prefix is considered allocated and freed on destroy */
    LOG_FLAG_FREELOG    = 1 << 16,  /* the log will be freed on log_destroy() */
    LOG_FLAG_SILENT     = 1 << 17,  /* the log will be disabled() */
    LOG_FLAG_CLOSING    = 1 << 19,  /* the log will be closed */
    LOG_FLAG_CUSTOM     = 1 << 20,  /* first bit available for log custom flags */
    LOG_FLAG_DEFAULT    = LOG_FLAG_DATETIME | LOG_FLAG_MODULE | LOG_FLAG_LEVEL
                        | LOG_FLAG_LOC_ERR | LOG_FLAG_LOC_TAIL | LOG_FLAG_COLOR
                        | LOG_FLAG_FILE | LOG_FLAG_FUNC | LOG_FLAG_LINE
                        | LOG_FLAG_CLOSEFILE | LOG_FLAG_FREELOG
} log_flag_t;

/** log context */
typedef struct {
    unsigned int    level:4;
    unsigned int    flags:28;
    FILE *          out;
    char *          prefix;
} log_t;

# define    LOG_VLIB_PREFIX_DEFAULT         "vlib"
# define    LOG_OPTIONS_PREFIX_DEFAULT      "options"
# define    LOG_FILE_DEFAULT                stderr
# define    LOG_USE_VA_ARGS
# define    LOG_CHECK_LVL_BEFORE_CALL
/**
 * Theses are the MACROS to use for logging.
 * See vlog() doc below for details, parameters, return value.
 */
# define    LOG_CAN_LOG(log, lvl)                                                   \
                ((void*)(log) == NULL                                               \
                  || ((int)(((log_t*)(log))->level) >= (int)(lvl)                   \
                      && (((log_t *)log)->flags & LOG_FLAG_SILENT) == 0))

# ifdef LOG_USE_VA_ARGS
#  ifdef LOG_CHECK_LVL_BEFORE_CALL
    /* check if level is OK before to make the call
     * the cast ((void*)(log) avoids &log==NULL warning on gcc */
#   define   LOG_CHECK_LOG(log, lvl, ...)                                           \
                ( LOG_CAN_LOG(log, lvl)                                             \
                  ? vlog_nocheck((lvl), (log),                                      \
                                 __FILE__, __func__, __LINE__, __VA_ARGS__)         \
                  : 0)
#   define   LOG_CHECK_LOGBUF(log, lvl, buf, sz, ...)                               \
                ( LOG_CAN_LOG(log, lvl)                                             \
                  ? log_buffer_nocheck((lvl),(log),(buf),(sz),                      \
                                       __FILE__,__func__,__LINE__,__VA_ARGS__)      \
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
                                    LOG_CHECK_LOGBUF(log,lvl,buf,sz,__VA_ARGS__)
#  ifdef _DEBUG
#   define  LOG_DEBUG(log,...)      LOG_CHECK_LOG(log, LOG_LVL_DEBUG,   __VA_ARGS__)
#   define  LOG_DEBUG_LVL(lvl,log,...) LOG_CHECK_LOG(log, lvl,          __VA_ARGS__)
#   define  LOG_SCREAM(log,...)     LOG_CHECK_LOG(log, LOG_LVL_SCREAM,  __VA_ARGS__)
#   define  LOG_DEBUG_BUF(log,buf,sz,...) \
                                    LOG_BUFFER(LOG_LVL_DEBUG,log,buf,sz,__VA_ARGS__)
#   define  LOG_DEBUG_BUF_LVL(lvl,log,buf,sz,...) \
                                    LOG_BUFFER(lvl,          log,buf,sz,__VA_ARGS__)
#  else
static inline int log_dummy() { return 0; } /* to avoid -Wunused-value on gcc */
#   define  LOG_DEBUG(log,...)      log_dummy()
#   define  LOG_DEBUG_LVL(log,...)  log_dummy()
#   define  LOG_DEBUG_BUF(log,...)  log_dummy()
#   define  LOG_DEBUG_BUF_LVL(lvl,log,...)  log_dummy()
#   define  LOG_SCREAM(log,...)     log_dummy()
#  endif /* ! _DEBUG */
# else /*! LOG_USE_VA_ARGS */
int     log_error(log_t * log, const char * fmt, ...);
int     log_warn(log_t * log, const char * fmt, ...);
int     log_info(log_t * log, const char * fmt, ...);
int     log_verbose(log_t * log, const char * fmt, ...);
int     log_debug(log_t * log, const char * fmt, ...);
int     log_debug_lvl(log_level_t lvl, log_t * log, const char * fmt, ...);
int     log_debug_buffer(log_t * log, const void * p, size_t l, const char * fmt, ...);
int     log_buffer_nova(log_level_t lvl, log_t * log, const void *p, size_t l, const char * fmt, ...);
int     log_scream(log_t * log, const char * fmt, ...);
#  define   LOG_ERROR               log_error
#  define   LOG_WARN                log_warn
#  define   LOG_INFO                log_info
#  define   LOG_VERBOSE             log_verbose
#  define   LOG_BUFFER              log_buffer_nova
#  ifdef _DEBUG
#   define   LOG_DEBUG               log_debug
#   define   LOG_DEBUG_LVL           log_debug_lvl
#   define   LOG_DEBUG_BUF           log_debug_buffer
#   define   LOG_DEBUG_BUF_LVL       log_buffer_nova
#   define   LOG_SCREAM              log_scream
#  else
static inline int log_dummy() { return 0; } /* to avoid -Wunused-value on gcc */
#   define  LOG_DEBUG(log,...)      log_dummy()
#   define  LOG_DEBUG_LVL(log,...)  log_dummy()
#   define  LOG_DEBUG_BUF(log,...)  log_dummy()
#   define  LOG_DEBUG_BUF_LVL(lvl,log,...)  log_dummy()
#   define  LOG_SCREAM(log,...)     log_dummy()
#  endif /* ! _DEBUG */
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
int         vlog(
                log_level_t     level,
                log_t *         log,
                const char *    file,
                const char *    func,
                int             line,
                const char * fmt, ...) __attribute__((format(printf,6,7)));

/** same as vlog but without checking log->level and log->flags*/
int         vlog_nocheck(
                log_level_t     level,
                log_t *         log,
                const char *    file,
                const char *    func,
                int             line,
                const char * fmt, ...) __attribute__((format(printf,6,7)));

/** Log a buffer with hex and ascii data. See vlog() */
int	        log_buffer(
                log_level_t     level,
                log_t *         log,
                const void *    pbuffer,
                size_t          len,
                const char *    file,
                const char *    func,
                int             line,
                const char *    fmt_header, ...) __attribute__((format(printf,8,9)));

/** same as log_buffer but without checking log->level and log->flags*/
int	        log_buffer_nocheck(
                log_level_t     level,
                log_t *         log,
                const void *    pbuffer,
                size_t          len,
                const char *    file,
                const char *    func,
                int             line,
                const char *    fmt_header, ...) __attribute__((format(printf,8,9)));

/** split strings and log them line by line, return number of chars written. */
int         vlog_strings(
                log_level_t     level,
                log_t *         log,
                const char *    file,
                const char *    func,
                int             line,
                const char *    strings_fmt, ...) __attribute__((format(printf, 6, 7)));

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

/** Warning the log level is not checked inside, use before LOG_CAN_LOG(log, lvl) */
int         log_header(log_level_t level, log_t * log,
                       const char * file, const char * func, int line);
int         log_footer(log_level_t level, log_t * log,
                       const char * file, const char * func, int line);

void        log_close(log_t * log);
void        log_destroy(void * vlog);

/* get and lock the file associated with given log
 * @return file locked or locked stderr if log or log->out is NULL */
FILE *      log_getfile_locked(log_t * log);

/** set internal vlib log instance, shared between vlib components
 * @param log the new vlib log instance. If NULL, default will be used.
 * @return the previous vlib log instance
 */
log_t *     log_set_vlib_instance(log_t * log);


#ifdef __cplusplus
}
#endif

#endif /* !ifndef *_H */

