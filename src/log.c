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
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <ctype.h>

#include "vlib/log.h"
#include "vlib/util.h"

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

#ifndef LOG_USE_VA_ARGS
int	xvlog(log_level_t level, log_ctx_t *ctx, const char *fmt, va_list arg) {
    int n = 0;
	if (ctx == NULL || ctx->level >= level)
	{
        FILE * out = ctx ? ctx->out : stderr;
        flockfile();
        n += xlog_header(level, ctx);
	    n += vfprintf(out, fmt, arg);
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

int xlog_header(log_level_t level, log_ctx_t *ctx) {
    time_t          tim;
    struct tm       tm;
    int             n;
    FILE *          out;
    const char *    prefix;
    const char *    level_str;

    if (ctx) {
        out = ctx->out ? ctx->out : stderr;
        prefix = ctx->prefix;
    } else {
        out = stderr;
        prefix = "<null>";
    }
    level_str = s_log_levels_str[level > LOG_LVL_NB ? LOG_LVL_NB : level];
    tim = time(NULL);
    localtime_r(&tim, &tm);
    if ((n = fprintf(out, "%04d.%02d.%02d %02d:%02d:%02d: [%s]: %s ",
                    tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                    tm.tm_hour, tm.tm_min, tm.tm_min, prefix, level_str)) < 0) {
        return 0;
    }
    return n;
}

int	xlog(log_level_t level, log_ctx_t *ctx, const char *fmt, ...)
{
    int total = 0;

	if (ctx == NULL || ctx->level >= level)
	{
        FILE *  out = ctx ? ctx->out : stderr;
    	va_list arg;
        int     n;

        flockfile(out);
        if ((n = xlog_header(level, ctx)) >= 0) {
            total += n;
    		va_start(arg, fmt);
		    if ((n = vfprintf(out, fmt, arg)) >= 0)
                total += n;
    		va_end(arg);
        }
        funlockfile(out);
	}
	return total;
}

#define INCR_GE0(call, test, total) if (((test) = (call)) >= 0) (total) += (test)

int	xlog_buffer(log_level_t level, log_ctx_t *ctx,
                const void * pbuffer, size_t len,
                const char *fmt_header, ...)
{
    int total = 0;

	if (ctx == NULL || ctx->level >= level)
	{
        FILE *          out = ctx ? ctx->out : stderr;
    	va_list         arg;
        const size_t    chars_per_line = 16;
        const char *    buffer = (const char *) pbuffer;
        int             n;

        if (buffer == NULL || len == 0) {
            flockfile(out);
            INCR_GE0(xlog_header(level, ctx), n, total);
		    va_start(arg, fmt_header);
		    INCR_GE0(vfprintf(out, fmt_header, arg), n, total);
            INCR_GE0(fprintf(out, "<empty>\n"), n, total);
		    va_end(arg);
            funlockfile(out);
            return total;
        }

        flockfile(out);
        for (size_t i_buf = 0; i_buf < len; i_buf += chars_per_line) {
            size_t i_char;

            INCR_GE0(xlog_header(level, ctx), n, total);
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
            INCR_GE0(fprintf(out, "\n"), n, total);
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
        } else {
            log->level = LOG_LVL_INFO;
            log->out = stderr;
            log->flags = 0;
            strn0cpy(log->prefix, "main", LOG_PREFIX_SZ, LOG_PREFIX_SZ);
        }
    }
    return log;
}

int xlog_list_prefixfind(const void * vvalue, const void * vlist_data) {
    if (vvalue == NULL || vlist_data == NULL) {
        return 1;
    }
    const char *        prefix  = (const char *) vvalue;
    const log_ctx_t *   log     = (const log_ctx_t *) vlist_data;

    return strncmp(prefix, log->prefix, LOG_PREFIX_SZ);
}

int xlog_list_prefixcmp(const void * vlist1_data, const void * vlist2_data) {
    if (vlist1_data == NULL || vlist2_data == NULL) {
        return (vlist1_data == vlist2_data) ? 0 : 1;
    }
    const log_ctx_t * log1 = (const log_ctx_t *) vlist1_data;

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
        fprintf(stderr, "** LOG CMD\n");

        /* Get the following LOG configuration separated by ',' used for next loop */
        maxlen = strtok_ro_r(&next_tok, ",", &next, NULL, 0);
        xlog_buffer(0, NULL, next_tok, maxlen, "log_line ");

        /* Get the Module Name that must be followed by '=' */
        len = strtok_ro_r(&mod_name, "=", &next_tok, &maxlen, 1);
        //fprintf(stderr, "'%c'(%d)\n", *next_tok, *next_tok);
        //if (maxlen == 0) { maxlen += len; next = mod_name; len = 0; }
        xlog_buffer(0, NULL, mod_name, len, "mod_name ");

        /* Get the Module Level that can be followed by '@' or end of string. */
        len = strtok_ro_r(&mod_lvl, "@", &next_tok, &maxlen, 0);
        xlog_buffer(0, NULL, mod_lvl, len, "mod_lvl ");

        //len = strtok_ro_r(&mod_file, "\0", &next_tok, &maxlen, 0);
        mod_file = next_tok;
        len = maxlen;
        xlog_buffer(0, NULL, mod_file, len, "mod_file ");
    }
    return logs;
}

void xlog_close(log_ctx_t * ctx) {
    if (ctx) {
        if (ctx->out != stderr && ctx->out != stdout) {
            fclose(ctx->out);
        }
    }
}

void xlog_close_and_free(void * vlog) {
    if (vlog) {
        log_ctx_t * log = (log_ctx_t *) vlog;
        xlog_close(log);
        free(log);
    }
}

void xlog_list_free(slist_t *logs) {
    slist_free(logs, xlog_close_and_free);
}

