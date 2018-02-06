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
 * Simple Log utilities.
 */
#include <sys/time.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h>

#include "vlib/log.h"
#include "vlib/util.h"

/** global vlib log state structure */
#define LOG_DATETIME_SZ 18
struct {
    pthread_mutex_t mutex;
    time_t          last_timet;
    char            datetime[LOG_DATETIME_SZ];
} g_vlib_log_global_ctx = { .mutex = PTHREAD_MUTEX_INITIALIZER, .last_timet = 0 };

/** internal log level strings */
static const char * s_log_levels_str[] = {
    "---",
    "ERR",
    "WRN",
    "INF",
    "VER",
    "DBG",
    "SCR",
    "+++"
};

static int xlog_location(FILE * out, log_flag_t flags,
                         const char * file, const char * func, int line) {
    int ret, n = 0;

    if ((flags & (LOG_FLAG_FILE | LOG_FLAG_FUNC | LOG_FLAG_LINE)) != 0) {
        if (fputc('{', out) != EOF)
            n++;
        if ((flags & LOG_FLAG_FILE) != 0 && (ret = fprintf(out, "%s", file)) > 0)
            n += ret;
        if ((flags & LOG_FLAG_LINE) != 0 && (ret = fprintf(out, ":%d", line)) > 0)
            n += ret;
        if ((flags & LOG_FLAG_FUNC) != 0 && (ret = fprintf(out, ">%s()", func)) > 0)
            n += ret;
        if (fputs("} ", out) != EOF)
            n+=2;
    }
    return n;
}

#ifndef LOG_USE_VA_ARGS
static int xvlog(log_level_t level, log_ctx_t *ctx,
                 const char *fmt, va_list arg) {
    int n = 0;
	if (ctx == NULL || ctx->level >= level)
	{
        FILE *          out = ctx ? ctx->out : stderr;
        log_flag_t      flags = ctx ? ctx->flags : LOG_FLAG_DEFAULT;
        const char *    file = "?", * func = "?";
        int             line = 0;

        flockfile();
        n += xlog_header(level, ctx, file, func, line);
	    n += vfprintf(out, fmt, arg);
        if ((flags & LOG_FLAG_LOC_TAIL) == 0) {
            if (fputc(' ', out) != EOF)
                ++n;
            n += xlog_location(out, flags, file, func, line);
        }
        if (fputc('\n', out) != EOF)
            ++n;
        funlockfile();
	}
	return n;
}
int xlog_error(log_ctx_t *ctx, const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_ERROR, ctx, fmt);
    va_end(arg);
}
int xlog_warn(log_ctx_t *ctx, const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_WARN, ctx, fmt);
    va_end(arg);
}
int xlog_info(log_ctx_t *ctx, const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_INFO, ctx, fmt, arg);
    va_end(arg);
}
int xlog_verbose(log_ctx_t *ctx, const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_VERBOSE, ctx, fmt, arg);
    va_end(arg);
}
int xlog_debug(log_ctx_t *ctx, const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_DEBUG, ctx, fmt, arg);
    va_end(arg);
}
int xlog_scream(log_ctx_t *ctx, const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_SCREAM, ctx, fmt, arg);
    va_end(arg);
}
#endif /* ! ifndef LOG_USE_VA_ARGS */

int xlog_header(log_level_t level, log_ctx_t *ctx,
                const char * file, const char * func, int line) {
    FILE *          out = NULL;
    const char *    prefix = NULL;
    log_flag_t      flags = LOG_FLAG_DEFAULT;
    int             n = 0, ret;

    if (ctx) {
        out = ctx->out;
        prefix = ctx->prefix;
        flags = ctx->flags;
    }
    if (out == NULL)
        out = stderr;
    if (prefix == NULL)
        prefix = "*";

    if ((flags & LOG_FLAG_DATETIME) != 0) {
        //static time_t * const plast_timet = NULL;
        static time_t * const   plast_timet = &g_vlib_log_global_ctx.last_timet;
        time_t                  tim;

        /* do localtime_r each time */
        struct timeval tv;
        if (gettimeofday(&tv, NULL) >= 0) {
            tim = (time_t) tv.tv_sec;
        } else {
            tim = time(NULL);
            tv.tv_usec = 0;
        }
        if (plast_timet == NULL) {
            struct tm tm;
            if (localtime_r(&tim, &tm) == NULL) {
                memset(&tm, 0, sizeof(tm));
            }
            if ((ret = fprintf(out, "%04d.%02d.%02d %02d:%02d:%02d.%03d ",
                               tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                               tm.tm_hour, tm.tm_min, tm.tm_sec, tv.tv_usec / 1000)) > 0)
                n += ret;
        } else {
            /* do localtime_r on minute change */
            static pthread_mutex_t * const  mutex           = &g_vlib_log_global_ctx.mutex;
            static char * const             old_datetime    = g_vlib_log_global_ctx.datetime;
            char                            datetime[LOG_DATETIME_SZ];
            pthread_mutex_lock(mutex);
            if (tim/60 != *plast_timet) {
                struct tm tm;
                *plast_timet = tim/60;
                if (localtime_r(&tim, &tm) == NULL) {
                    memset(&tm, 0, sizeof(tm));
                }
                snprintf(old_datetime, LOG_DATETIME_SZ,
                         "%04d.%02d.%02d %02d:%02d:",
                         tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                         tm.tm_hour, tm.tm_min);
            }
            strcpy(datetime, old_datetime);
            pthread_mutex_unlock(mutex);
            if ((ret = fprintf(out, "%s%02ld.%03d ", datetime, tim % 60, tv.tv_usec/1000)) > 0)
                n += ret;
        }
    }
    if ((flags & LOG_FLAG_LEVEL) != 0
    && (ret = fprintf(out, "%s ", s_log_levels_str[level > LOG_LVL_NB ? LOG_LVL_NB : level])) > 0)
        n += ret;
    if ((flags & LOG_FLAG_MODULE) != 0 && (ret = fprintf(out, "[%s] ", prefix)) >0)
        n += ret;
    if ((flags & LOG_FLAG_LOC_TAIL) == 0) {
        n += xlog_location(out, flags, file, func, line);
    }
    return n;
}

int	xlog(log_level_t level, log_ctx_t *ctx,
         const char * file, const char * func, int line,
         const char *fmt, ...)
{
    int total = 0;

	if (ctx == NULL || ctx->level >= level)
	{
        FILE *      out = ctx ? ctx->out : stderr;
        log_flag_t  flags = ctx ? ctx->flags : LOG_FLAG_DEFAULT;
        va_list     arg;
        int         n;

        flockfile(out);
        total = xlog_header(level, ctx, file, func, line);

        va_start(arg, fmt);
		if ((n = vfprintf(out, fmt, arg)) >= 0)
            total += n;
        va_end(arg);

        if ((flags & LOG_FLAG_LOC_TAIL) != 0) {
            if (fputc(' ', out) != EOF)
                ++total;
            total += xlog_location(out, flags, file, func, line);
        }
        if (fputc('\n', out) != EOF)
            ++total;
        funlockfile(out);
	}
	return total;
}

#define INCR_GE0(call, test, total) if (((test) = (call)) >= 0) (total) += (test)

int	xlog_buffer(log_level_t level, log_ctx_t *ctx,
                const void * pbuffer, size_t len,
                const char * file, const char * func, int line,
                const char *fmt_header, ...)
{
    int total = 0;

	if (ctx == NULL || ctx->level >= level)
	{
        FILE *          out = ctx ? ctx->out : stderr;
    	va_list         arg;
        const size_t    chars_per_line = 16;
        const char *    buffer = (const char *) pbuffer;
        log_flag_t      flags = ctx ? ctx->flags : LOG_FLAG_DEFAULT;
        int             n;

        if (buffer == NULL || len == 0) {
            flockfile(out);
            INCR_GE0(xlog_header(level, ctx, file, func, line), n, total);
            va_start(arg, fmt_header);
		    INCR_GE0(vfprintf(out, fmt_header, arg), n, total);
            INCR_GE0(fprintf(out, "<empty>"), n, total);
            va_end(arg);
            if ((flags & LOG_FLAG_LOC_TAIL) != 0) {
                if (fputc(' ', out) != EOF)
                    ++total;
                total += xlog_location(out, flags, file, func, line);
            }
            if (fputc('\n', out) != EOF)
                ++total;
            funlockfile(out);
            return total;
        }

        flockfile(out);
        for (size_t i_buf = 0; i_buf < len; i_buf += chars_per_line) {
            size_t i_char;

            INCR_GE0(xlog_header(level, ctx, file, func, line), n, total);
            va_start(arg, fmt_header);
		    INCR_GE0(vfprintf(out, fmt_header, arg), n, total);
            va_end(arg);

            INCR_GE0(fprintf(out, "%04zx:", i_buf), n, total);
            for (i_char = i_buf; i_char < i_buf + chars_per_line && i_char < len; i_char++) {
                INCR_GE0(fprintf(out, " %02x", buffer[i_char] & 0xff), n, total);
            }
            while ((i_char++ % chars_per_line) != 0)
                INCR_GE0(fprintf(out, "   "), n, total);
            INCR_GE0(fprintf(out, " | "), n, total);
            for (i_char = i_buf; i_char < i_buf + chars_per_line && i_char < len; i_char++) {
                char ch = buffer[i_char] & 0xff;
                if (!isgraph(ch))
                    ch = '?';
                INCR_GE0(fprintf(out, "%c", ch), n, total);
            }
            if ((flags & LOG_FLAG_LOC_TAIL) != 0) {
                if (fputc(' ', out) != EOF)
                    ++total;
                total += xlog_location(out, flags, file, func, line);
            }
            if (fputc('\n', out) != EOF)
                ++total;
        }
        funlockfile(out);
	}
	return total;
}

log_ctx_t * xlog_create(log_ctx_t *from) {
    log_ctx_t * log = calloc(1, sizeof(log_ctx_t));
    if (log != NULL) {
        if (from != NULL) {
            *log = *from;
            if (from->prefix != NULL) {
                log->prefix = strdup(from->prefix);
                log->flags |= LOG_FLAG_FREEPREFIX;
            }
        } else {
            log->level = LOG_LVL_INFO;
            log->out = stderr;
            log->flags = LOG_FLAG_DEFAULT | LOG_FLAG_FREEPREFIX;
            log->prefix = strdup("main");
        }
    }
    return log;
}

int xlog_list_prefixfind(const void * vvalue, const void * vlist_data) {
    const char *        prefix  = (const char *) vvalue;
    const log_ctx_t *   log     = (const log_ctx_t *) vlist_data;

    if (prefix == NULL || log == NULL || log->prefix == NULL) {
        return 1;
    }
    return strcmp(prefix, log->prefix);
}

int xlog_list_prefixcmp(const void * vlist1_data, const void * vlist2_data) {
    const log_ctx_t * log1 = (const log_ctx_t *) vlist1_data;

    if (log1 == NULL || vlist2_data == NULL || log1->prefix) {
        return (vlist1_data == vlist2_data) ? 0 : 1;
    }

    return xlog_list_prefixfind(log1->prefix, vlist2_data);
}

slist_t * xlog_create_from_cmdline(slist_t * logs,
                                   const char * log_levels, const char *const* modules) {
    // FIXME: work in progress
    if (log_levels == NULL) {
        return NULL;
    }
    //char            prefix[LOG_PREFIX_SZ];
    //char            log_path[PATH_MAX];
    //const char *    next_mod_lvl;
    //log_ctx_t       default_log = { .level = LOG_LVL_DEFAULT, .out = stderr, .prefix = "main", .flags = 0};
(void)modules;

    /* Parse log levels string with strtok_ro_r/strcspn instead of strtok_r or strsep
     * as those cool libc functions change the token by replacing sep with 0 */
    //const char * next = log_levels;
    size_t maxlen;
    size_t len;
    const char * next_tok;
    const char * mod_name;
    const char * mod_lvl;
    const char * mod_file;
    for (const char *next = log_levels; next && *next; /* no_incr */) {
        /* Get the following LOG configuration separated by ',' used for next loop */
        maxlen = strtok_ro_r(&next_tok, ",", &next, NULL, 0);
        LOG_DEBUG_BUF(NULL, next_tok, maxlen, "log_line ");

        /* Get the Module Name that must be followed by '=' */
        len = strtok_ro_r(&mod_name, "=", &next_tok, &maxlen, 1);
        //fprintf(stderr, "'%c'(%d)\n", *next_tok, *next_tok);
        //if (maxlen == 0) { maxlen += len; next = mod_name; len = 0; }
        LOG_DEBUG_BUF(NULL, mod_name, len, "mod_name ");

        /* Get the Module Level that can be followed by '@' or end of string. */
        len = strtok_ro_r(&mod_lvl, "@", &next_tok, &maxlen, 0);
        LOG_DEBUG_BUF(NULL, mod_lvl, len, "mod_lvl ");

        //len = strtok_ro_r(&mod_file, "\0", &next_tok, &maxlen, 0);
        mod_file = next_tok;
        len = maxlen;
        LOG_DEBUG_BUF(NULL, mod_file, len, "mod_file ");
    }
    return logs;
}

void xlog_close(log_ctx_t * ctx) {
    if (ctx) {
        if (ctx->out != stderr && ctx->out != stdout
        &&  (ctx->flags & LOG_FLAG_CLOSEFILE) != 0) {
            fclose(ctx->out);
        }
        ctx->out = NULL;
    }
}

void xlog_destroy(void * vlog) {
    if (vlog) {
        log_ctx_t * log = (log_ctx_t *) vlog;

        xlog_close(log);
        if (log->prefix && (log->flags & LOG_FLAG_FREEPREFIX) != 0) {
            free(log->prefix);
        }
        log->prefix = NULL;
        free(log);
    }
}

void xlog_list_free(slist_t *logs) {
    slist_free(logs, xlog_destroy);
}

