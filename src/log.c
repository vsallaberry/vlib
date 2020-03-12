/*
 * Copyright (C) 2017-2020 Vincent Sallaberry
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
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <ctype.h>
#include <pthread.h>

#include "vlib/log.h"
#include "vlib/util.h"
#include "vlib/hash.h"
#include "vlib/options.h"
#include "vlib/term.h"

/** internal vlib log instance */
static log_t s_vlib_log_default = {
    .level      = LOG_LVL_INFO,
    .out        = NULL,
    .flags      = LOG_FLAG_DEFAULT,
    .prefix     = "vlib"
};

/** global internal vlib log instance, shared between vlib components */
log_t * g_vlib_log = &s_vlib_log_default;

/** global vlib log state structure */
#define LOG_DATETIME_SZ 18
static struct {
    pthread_mutex_t mutex;
    time_t          last_timet;
    char            datetime[LOG_DATETIME_SZ];
} g_vlib_log_global_ctx = { .mutex = PTHREAD_MUTEX_INITIALIZER, .last_timet = 0 };

/** internal log level strings and colors (IN SAME ORDER as log_level_t) */
static struct loglevel_info_s {
    const char *    str;
    vterm_color_t   colorfg;
    vterm_color_t   colorbg; /* Not Supported : RFU */
    vterm_color_t   style;
    vterm_color_t   reset;
    int             colorlen;
} s_log_levels_info[] = {
    { "---", VCOLOR_EMPTY,  VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_EMPTY, INT_MAX },
    { "ERR", VCOLOR_RED,    VCOLOR_EMPTY, VCOLOR_BOLD,  VCOLOR_RESET, INT_MAX },
    { "WRN", VCOLOR_YELLOW, VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_RESET, INT_MAX },
    { "INF", VCOLOR_BLUE,   VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_RESET, INT_MAX },
    { "VER", VCOLOR_EMPTY,  VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_EMPTY, INT_MAX },
    { "DBG", VCOLOR_EMPTY,  VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_EMPTY, INT_MAX },
    { "SCR", VCOLOR_EMPTY,  VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_EMPTY, INT_MAX },
    { "+++", VCOLOR_EMPTY,  VCOLOR_EMPTY, VCOLOR_EMPTY, VCOLOR_EMPTY, INT_MAX }
};

/** internal log flags strings */
static struct { log_flag_t flag; const char * name; } s_log_flag_str[] = {
    { LOG_FLAG_NONE,        "None" },
    { LOG_FLAG_DATETIME,    "DateTime" },
    { LOG_FLAG_MODULE,      "Module" },
    { LOG_FLAG_LEVEL,       "Level" },
    { LOG_FLAG_PID,         "Pid" },
    { LOG_FLAG_TID,         "Tid" },
    { LOG_FLAG_FILE,        "File" },
    { LOG_FLAG_FUNC,        "Func" },
    { LOG_FLAG_LINE,        "Line" },
    { LOG_FLAG_LOC_TAIL,    "LocTail" },
    { LOG_FLAG_LOC_ERR,     "LocErr" },
    { LOG_FLAG_ABS_TIME,    "AbsTime" },
    { LOG_FLAG_COLOR,       "Color" },
    { LOG_FLAG_SILENT,      "Silent" },
};
#define LOG_FLAG_STR_NB (sizeof(s_log_flag_str) / sizeof(*s_log_flag_str))

const char * log_level_name(log_level_t level) {
    return s_log_levels_info[level > LOG_LVL_NB ? LOG_LVL_NB : level].str;
}

log_level_t log_level_from_name(const char * name) {
    for (int i = 0; i < LOG_LVL_NB; ++i) {
        if (!strcasecmp(name, s_log_levels_info[i].str))
            return i;
    }
    return LOG_LVL_NB;
}

const char * log_flag_name(log_flag_t flag) {
    for (unsigned int i = 0; i < LOG_FLAG_STR_NB; ++i) {
        if (flag == s_log_flag_str[i].flag) {
            return s_log_flag_str[i].name;
        }
    }
    return NULL;
}

log_flag_t log_flag_from_name(const char * name) {
    for (unsigned int i = 0; i < LOG_FLAG_STR_NB; ++i) {
        if (!strcasecmp(name, s_log_flag_str[i].name)) {
            return s_log_flag_str[i].flag;
        }
    }
    return LOG_FLAG_UNKNOWN;
}

int log_describe_option(char * buffer, int * size, const char *const* modules,
                        slist_t * modules_list, const char *(module_get)(const void *)) {
    int     n = 0, ret;
    char    sep[3] = { 0, ' ', 0 };

    /* sanity checks */
    if (buffer == NULL || size == NULL) {
        return OPT_ERROR(OPT_EFAULT);
    }

    /* describe log levels */
    n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "- levels: '");
    for (int lvl = LOG_LVL_NONE; lvl < LOG_LVL_NB; ++lvl, *sep = ',') {
        n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "%s%d|%s",
                    sep, lvl, s_log_levels_info[lvl].str);
    }
    n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "'");

    /* describe log flags */
    *sep = 0; sep[1] = 0;
    n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "\r- flags: '");
    for (unsigned int i = 0; i < LOG_FLAG_STR_NB; ++i, *sep = '|') {
        n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "%s%s",
                    sep, s_log_flag_str[i].name);
    }
    n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "'");

    /* describe modules */
    n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "\r- modules: '");
    *sep = 0; sep[1] = 0;
    if (modules != NULL) { /* modules array */
        for (const char *const* mod = modules; *mod; mod++, *sep = ',') {
            n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "%s%s", sep, *mod);
        }
    }
    if (modules_list != NULL) { /* modules list */
        for (slist_t * mod = modules_list; mod; mod = mod->next, *sep = ',') {
            const char * name = module_get ? module_get(mod->data) : (const char *) mod->data;
            n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "%s%s", sep, name);
        }
    }
    n += VLIB_SNPRINTF(ret, buffer + n, *size - n, "'");
    *size = n;

    return OPT_CONTINUE(1);
}

log_t * log_set_vlib_instance(log_t * log) {
    log_t * old_log = g_vlib_log;
    FILE * out = g_vlib_log && g_vlib_log->out ? g_vlib_log->out : stderr;

    if (out) {
        flockfile(out);
        fflush(out);
    }
    if (log == NULL) {
        log = &s_vlib_log_default;
    }
    g_vlib_log = log;
    if (out)
        funlockfile(out);
    return old_log;
}

static int log_location(FILE * out, log_flag_t flags, log_level_t level,
                        const char * file, const char * func, int line) {
    int ret, n = 0;

    /* Print location if requested. if LOG_FLAG_LOC_ERR is on, the location will be displayed
     * only on levels ERR, WARN and >= DEBUG */
    if ((flags & (LOG_FLAG_FILE | LOG_FLAG_FUNC | LOG_FLAG_LINE)) != 0
    &&  ((flags & LOG_FLAG_LOC_ERR) == 0
          || level == LOG_LVL_ERROR || level == LOG_LVL_WARN || level >= LOG_LVL_DEBUG)) {
        if (fputc('{', out) != EOF)
            n++;
        if ((flags & LOG_FLAG_FILE) != 0 && file && (ret = fprintf(out, "%s", file)) > 0)
            n += ret;
        if ((flags & LOG_FLAG_LINE) != 0 && (ret = fprintf(out, ":%d", line)) > 0)
            n += ret;
        if ((flags & LOG_FLAG_FUNC) != 0 && func && (ret = fprintf(out, ">%s()", func)) > 0)
            n += ret;
        if (fputs("} ", out) != EOF)
            n+=2;
    }
    return n;
}

#ifndef LOG_USE_VA_ARGS
static int xvlog(log_level_t level, log_t * log,
                 const char *fmt, va_list arg) {
    int n = 0;
    if (LOG_CAN_LOG(log, level))
    {
        FILE *          out;
        unsigned int    flags;
        const char *    file = "?", * func = "?";
        int             line = 0;

        out = log_getfile_locked(log);
        flags = log ? log->flags : LOG_FLAG_DEFAULT;

        if (fmt == NULL) {
            line = fputc('\n', out) != EOF ? 1 : 0;
            funlockfile(out);
            return line;
        }

        n += log_header(level, log, file, func, line);
        n += vfprintf(out, fmt, arg);
        if ((flags & LOG_FLAG_LOC_TAIL) == 0) {
            if (fputc(' ', out) != EOF)
                ++n;
            n += log_location(out, flags, level, file, func, line);
        }
        if (fputc('\n', out) != EOF)
            ++n;
        funlockfile(out);
    }
    return n;
}
int log_error(log_t * log, const char * fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_ERROR, log, fmt);
    va_end(arg);
}
int log_warn(log_t * log, const char * fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_WARN, log, fmt);
    va_end(arg);
}
int log_info(log_t * log, const char * fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_INFO, log, fmt, arg);
    va_end(arg);
}
int log_verbose(log_t * log, const char * fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_VERBOSE, log, fmt, arg);
    va_end(arg);
}
int log_debug(log_t * log, const char * fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_DEBUG, log, fmt, arg);
    va_end(arg);
}
int log_scream(log_t * log, const char * fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    xvlog(LOG_LVL_SCREAM, log, fmt, arg);
    va_end(arg);
}
#endif /* ! ifndef LOG_USE_VA_ARGS */

int log_header(log_level_t level, log_t * log,
               const char * file, const char * func, int line) {
    FILE *          out = NULL;
    const char *    prefix = NULL;
    log_flag_t      flags = LOG_FLAG_DEFAULT;
    int             n = 0, ret, log_colors, fd;

    if (log) {
        out = log->out;
        prefix = log->prefix;
        flags = log->flags;
    }
    if (out == NULL)
        out = stderr;
    if (prefix == NULL)
        prefix = "*";

    fd = fileno(out);
    log_colors = (flags & LOG_FLAG_COLOR) != 0 && vterm_has_colors(fd);

    if ((flags & LOG_FLAG_DATETIME) != 0) {
        //static time_t * const plast_timet = NULL;
        static time_t * const   plast_timet = &g_vlib_log_global_ctx.last_timet;
        time_t                  tim;

        /* do gettimeofday each time, used to call or not to call localtime_r */
        struct timeval tv;
        if (gettimeofday(&tv, NULL) >= 0) {
            tim = (time_t) tv.tv_sec;
        } else {
            tim = time(NULL);
            tv.tv_usec = 0;
        }
        if (plast_timet == NULL) {
            /* do localtime_r each time if plast_timet is null */
            struct tm tm;
            if (localtime_r(&tim, &tm) == NULL) {
                memset(&tm, 0, sizeof(tm));
            }
            if ((ret = fprintf(out, "%04d.%02d.%02d %02d:%02d:%02d.%03u ",
                               tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
                               tm.tm_hour, tm.tm_min, tm.tm_sec,
                               (unsigned int)(tv.tv_usec / 1000))) > 0)
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
                         "%04u.%02u.%02u %02u:%02u:",
                         (tm.tm_year + 1900U) % 10000U, (tm.tm_mon + 1U) % 100U, (tm.tm_mday % 100U),
                         tm.tm_hour % 100U, tm.tm_min % 100U);
            }
            strncpy(datetime, old_datetime, LOG_DATETIME_SZ);
            pthread_mutex_unlock(mutex);
            if ((ret = fprintf(out, "%s%02u.%03u ", datetime,
                               (unsigned int)(tim % 60),
                               (unsigned int)(tv.tv_usec/1000))) > 0)
                n += ret;
        }
    }
    if ((flags & LOG_FLAG_LEVEL) != 0) {
        if (log_colors) {
            struct loglevel_info_s *
                lvlinfo = &s_log_levels_info[level > LOG_LVL_NB ? LOG_LVL_NB : level];

            if (lvlinfo->colorlen == INT_MAX) {
                pthread_mutex_lock(&g_vlib_log_global_ctx.mutex);
            }
            ret = fprintf(out, "%s%s%s%s ", vterm_color(fd, lvlinfo->colorfg),
                          vterm_color(fd, lvlinfo->style), lvlinfo->str,
                          vterm_color(fd, lvlinfo->reset));
            if (lvlinfo->colorlen == INT_MAX) {
                int slen = strlen(lvlinfo->str);
                if (ret >= slen + 1)
                    lvlinfo->colorlen = ret - slen - 1;
                pthread_mutex_unlock(&g_vlib_log_global_ctx.mutex);
            }
            if (ret > lvlinfo->colorlen) {
                n += ret - lvlinfo->colorlen;
            }
        } else if ((ret = fprintf(out, "%s ", s_log_levels_info[level > LOG_LVL_NB
                                                                ? LOG_LVL_NB : level].str)) > 0)
            n += ret;
    }
    if ((flags & (LOG_FLAG_MODULE | LOG_FLAG_PID | LOG_FLAG_TID)) != 0) {
        const char * space = "";
        if (fputc('[', out) != EOF)
            ++n;
        if ((flags & LOG_FLAG_MODULE) != 0 && (ret = fprintf(out, "%s", prefix)) >0) {
            n += ret;
            space = ",";
        }
        if ((flags & LOG_FLAG_PID) != 0
        && (ret = fprintf(out, "%spid:%u", space, (unsigned int) getpid())) > 0) {
            n += ret;
            space = ",";
        }
        if ((flags & LOG_FLAG_TID) != 0
        && (ret = fprintf(out, "%stid:%lx", space, (unsigned long) pthread_self())) >0 )
            n += ret;
        if (fputs("] ", out) != EOF)
            n += 2;
    }
    if ((flags & LOG_FLAG_LOC_TAIL) == 0) {
        n += log_location(out, flags, level, file, func, line);
    }
    return n;
}

int vlog(log_level_t level, log_t * log,
         const char * file, const char * func, int line,
         const char * fmt, ...)
{
    int total = 0;

    if (LOG_CAN_LOG(log, level))
    {
        FILE *          out;
        unsigned int    flags;
        va_list         arg;
        int             n;

        out = log_getfile_locked(log);
        flags = log ? log->flags : LOG_FLAG_DEFAULT;

        if (fmt == NULL) {
            n = fputc('\n', out) != EOF ? 1 : 0;
            funlockfile(out);
            return n;
        }

        total = log_header(level, log, file, func, line);

        va_start(arg, fmt);
        if ((n = vfprintf(out, fmt, arg)) >= 0)
            total += n;
        va_end(arg);

        if ((flags & LOG_FLAG_LOC_TAIL) != 0) {
            if (fputc(' ', out) != EOF)
                ++total;
            total += log_location(out, flags, level, file, func, line);
        }
        if (fputc('\n', out) != EOF)
            ++total;
        funlockfile(out);
    }
    return total;
}

#define INCR_GE0(call, test, total) if (((test) = (call)) >= 0) (total) += (test)

int log_buffer(log_level_t level, log_t * log,
               const void * pbuffer, size_t len,
               const char * file, const char * func, int line,
               const char * fmt_header, ...)
{
    int total = 0;

    if (LOG_CAN_LOG(log, level))
    {
        FILE *          out;
        va_list         arg;
        const size_t    chars_per_line = 16;
        const char *    buffer = (const char *) pbuffer;
        unsigned int    flags;
        int             n;

        out = log_getfile_locked(log);
        flags = log ? log->flags : LOG_FLAG_DEFAULT;

        if (buffer == NULL || len == 0) {
            INCR_GE0(log_header(level, log, file, func, line), n, total);
            if (fmt_header) {
                va_start(arg, fmt_header);
                INCR_GE0(vfprintf(out, fmt_header, arg), n, total);
                INCR_GE0(fprintf(out, "<empty>"), n, total);
                va_end(arg);
            }
            if ((flags & LOG_FLAG_LOC_TAIL) != 0) {
                if (fputc(' ', out) != EOF)
                    ++total;
                total += log_location(out, flags, level, file, func, line);
            }
            if (fputc('\n', out) != EOF)
                ++total;
            funlockfile(out);
            return total;
        }

        for (size_t i_buf = 0; i_buf < len; i_buf += chars_per_line) {
            size_t i_char;

            INCR_GE0(log_header(level, log, file, func, line), n, total);
            if (fmt_header) {
                va_start(arg, fmt_header);
                INCR_GE0(vfprintf(out, fmt_header, arg), n, total);
                va_end(arg);
            }

            INCR_GE0(fprintf(out, "%04zx:", i_buf), n, total);
            for (i_char = i_buf; i_char < i_buf + chars_per_line && i_char < len; i_char++) {
                INCR_GE0(fprintf(out, " %02x", buffer[i_char] & 0xff), n, total);
            }
            while ((i_char++ % chars_per_line) != 0)
                INCR_GE0(fprintf(out, "   "), n, total);
            INCR_GE0(fprintf(out, " | "), n, total);
            for (i_char = i_buf; i_char < i_buf + chars_per_line && i_char < len; i_char++) {
                char ch = buffer[i_char] & 0xff;
                if (!isprint(ch))
                    ch = '?';
                INCR_GE0(fprintf(out, "%c", ch), n, total);
            }
            if ((flags & LOG_FLAG_LOC_TAIL) != 0) {
                if (fputc(' ', out) != EOF)
                    ++total;
                total += log_location(out, flags, level, file, func, line);
            }
            if (fputc('\n', out) != EOF)
                ++total;
        }
        funlockfile(out);
    }
    return total;
}

log_t * log_create(log_t * from) {
    log_t * log = calloc(1, sizeof(log_t));
    if (log != NULL) {
        if (from != NULL) {
            *log = *from;
            if (from->prefix != NULL && (from->flags & LOG_FLAG_FREEPREFIX) != 0) {
                log->prefix = strdup(from->prefix);
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

void log_close(log_t * log) {
    if (log && log->out) {
        fflush(log->out);
        if (log->out != stderr && log->out != stdout
        &&  (log->flags & LOG_FLAG_CLOSEFILE) != 0) {
            fclose(log->out);
        }
        log->out = NULL;
    }
}

void log_destroy(void * vlog) {
    if (vlog) {
        log_t * log = (log_t *) vlog;

        log_close(log);
        if (log->prefix && (log->flags & LOG_FLAG_FREEPREFIX) != 0) {
            free(log->prefix);
        }
        log->prefix = NULL;
        if ((log->flags & LOG_FLAG_FREELOG) != 0) {
            free(log);
        }
    }
}

FILE * log_getfile_locked(log_t * log) {
    FILE * file;

    if (log == NULL || log->out == NULL) {
        file = stderr;
        flockfile(file);
        return file;
    }
    file = log->out;
    while (1) {
        flockfile(file);
        /* lock acquired, other thread must wait before writing, closing, ... */
        if (file != log->out) {
            /* file was changed */
            FILE * old = file;
            file = log->out;
            LOG_DEBUG(g_vlib_log, "%s(): file was changed, old:%p new:%p",
                __func__, (void *) old, (void *) file);
            /* release old lock, will allow other thread to close it */
            funlockfile(old);
            /* try again */
            continue ;
        }
        return file;
    }
}

