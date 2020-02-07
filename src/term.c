/*
 * Copyright (C) 2019-2020 Vincent Sallaberry
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
 * Terminal utilities.
 * + vterm_get_columns was before implemented as static function in options.c
 * + call to setupterm, del_curterm, tigetnum, cur_term need linking with
 *   -lcurses or -lncurses or -ltinfo
*/
#include <unistd.h>

#include "version.h"

/* using libdl to get curses is deprecated */
/* #define VLIB_CURSESDL */

/* checks ncurses includes */
#ifndef CONFIG_CURSES
# define CONFIG_CURSES 0
#endif
#ifndef CONFIG_CURSES_H
# define CONFIG_CURSES_H 0
#endif
#if CONFIG_CURSES
# ifdef VLIB_CURSESDL
   /* call ncurses via dlopen/dlsym */
#  include <dlfcn.h>
# elif CONFIG_CURSES_H
   /* link to libncurses and include ncurses headers */
#  include <curses.h>
#  include <term.h>
# else
int             setupterm(char *, int, int *);
int             tigetnum(char *);
int             del_curterm(void *);
extern void *   cur_term;
bool            has_colors();
# endif
# if ! defined(CONFIG_CURSES_H) || defined(VLIB_CURSESDL)
/* link to libncurses but no header found. define symbols */
#  ifndef OK
#   define OK 0
#  endif
#  ifndef TRUE
#   define TRUE 1
#  endif
#  ifndef FALSE
#   define FALSE 0
#  endif
# endif
#endif
/* END NCURSES includes */

#include "vlib/log.h"
#include "vlib/term.h"

#include "vlib_private.h"

/* ************************************************************************* */
#if ! CONFIG_CURSES
/*******************************************************************
 * vterm implementation WITHOUT ncurses.
 ******************************************************************/
static const char * const s_vterm_color_empty = "";

int vterm_init(int fd, unsigned int flags) {
    (void) flags;
    if (!isatty(fd)) {
        return VTERM_NOTTY;
    }
    return VTERM_OK;
}
int vterm_free() {
    return VTERM_OK;
}
int vterm_get_columns() {
    return 0;
}
int vterm_has_colors(int fd) {
    (void)fd;
    return 0;
}
const char * vterm_color(int fd, vterm_color_t color) {
    (void)fd;
    (void)color;
    return s_vterm_color_empty;
}
# else /* CONFIG_CURSES */
/***************************************************************************
 * vterm implementation WITH ncurses.
 **************************************************************************/

static struct {
    unsigned int    flags;
    int             fd;
    int             has_colors;
    char *          ti_col;
#   ifdef VLIB_CURSESDL
    void *          lib;
    int             (*tigetnum)(char*);
#   endif
} s_vlib_term_info = {
    VTF_DEFAULT, -1, 0, NULL
#   ifdef VLIB_CURSESDL
    , NULL, NULL
#   endif
};

/* Colors strings corresponding to vterm_color_t */
static const char * const s_vterm_colors[VCOLOR_EMPTY+1] = {
    /* Foreground Colors */
    "\x1b[30m", /* black */
    "\x1b[31m", /* red */
    "\x1b[32m", /* green */
    "\x1b[33m", /* yellow */
    "\x1b[34m", /* blue */
    "\x1b[35m", /* magenta */
    "\x1b[36m", /* cyan */
    "\x1b[37m", /* white */
    /* Background Colors */
    "\x1b[40m", /* black */
    "\x1b[41m", /* red */
    "\x1b[42m", /* green */
    "\x1b[43m", /* yellow */
    "\x1b[44m", /* blue */
    "\x1b[45m", /* magenta */
    "\x1b[46m", /* cyan */
    "\x1b[47m", /* white */
    /* Styles */
    "\x1b[00m", /* Normal */
    "\x1b[01m", /* Bold */
    "\x1b[02m", /* Dark */
    "\x1b[03m", /* Italic */
    "\x1b[04m", /* Underlined */
    "\x1b[05m", /* Blinking */
    /* Reserved internal values */
    "\x1b[00m", /* Reset */
    ""          /* empty */
};

const char * vterm_color(int fd, vterm_color_t color) {
    if (vterm_has_colors(fd)) {
        if ((int)color < 0 || color >= sizeof(s_vterm_colors) / sizeof(*s_vterm_colors)) {
            return s_vterm_colors[VCOLOR_RESET];
        } else {
            return s_vterm_colors[color];
        }
    }
    return s_vterm_colors[VCOLOR_EMPTY];
}

int vterm_has_colors(int fd) {
    if (vterm_init(fd, VTF_DEFAULT) != VTERM_OK) {
       return 0;
    }
    return s_vlib_term_info.has_colors;
}

# if ! defined(VLIB_CURSESDL)
int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, VTF_DEFAULT)) == VTERM_NOTTY)
        return 0;
    if (ret != VTERM_OK || s_vlib_term_info.ti_col == NULL) {
        return VTERM_ERROR;
    }
    return tigetnum(s_vlib_term_info.ti_col);
}
int vterm_free() {
    if (cur_term != NULL) {
        del_curterm(cur_term);
    }
    return VTERM_OK;
}
int vterm_init(int fd, unsigned int flags) {
    if (!isatty(fd)) {
        return VTERM_NOTTY;
    }
    if (cur_term != NULL) {
        return VTERM_OK;
    }
    int ret;
    char * ti_col;
    if (setupterm(NULL, fd, &ret) != OK) {
        if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
        else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term not found.");
        else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db not found.");
        else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
        return VTERM_ERROR;
    } else if ((ret = tigetnum(ti_col = "cols")) <= 0
    &&         (ret = tigetnum(ti_col = "columns")) <= 0
    &&         (ret = tigetnum(ti_col = "COLUMNS")) <= 0) {
        LOG_ERROR(g_vlib_log, "tigetnum(cols,columns,COLUMNS): cannot get value");
        ti_col = NULL;
    }
    s_vlib_term_info.fd = fd;
    s_vlib_term_info.ti_col = ti_col;
    s_vlib_term_info.flags = flags;
    if ((s_vlib_term_info.has_colors = has_colors()) == TRUE) {
        start_color();
    }
    return VTERM_OK;
}

# else /* VLIB_CURSESDL */
int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, VTF_DEFAULT)) == VTERM_NOTTY)
        return 0;
    if (ret != VTERM_OK || s_vlib_term_info.ti_col == NULL || s_vlib_term_info.lib == NULL) {
        return VTERM_ERROR;
    }
    return s_vlib_term_info.tigetnum(s_vlib_term_info.ti_col);
}
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wpedantic"
int vterm_free() {
    if (s_vlib_term_info.lib == NULL) {
        return VTERM_OK;
    }
    int (*delterm)(void *);
    void **curterm;

    if ((delterm = (int(*)(void*)) dlsym(s_vlib_term_info.lib, "del_curterm")) != NULL
    &&  (curterm = (void**)        dlsym(s_vlib_term_info.lib, "cur_term"))    != NULL
    &&  *curterm != NULL) {
        delterm(*curterm);
    }
    dlclose(s_vlib_term_info.lib);
    s_vlib_term_info.lib = NULL;
    return VTERM_OK;
}
int vterm_init(int fd, unsigned int flags) {
    if (!isatty(fd)) {
        return VTERM_NOTTY;
    }
    if (s_vlib_term_info.lib != NULL) {
        return VTERM_OK;
    }
    int ret;
    void * lib = NULL;
    char ** path;
    int (*setup)(char*, int, int*);
    int (*getnum)(char*);
    int (*startcolor)(void);
    int (*hascolors)(void);
    char * ti_col;
    char * libs[] = {
        "libtinfo.so", "libtinfo.dylib", "libtinfo.so.5",
        "libncurses.so", "libncurses.dylib", "libcurses.so", "libcurses.dylib",
        "libncurses.so.5", "libcurses.so.5", NULL
    };

    for (path = libs; *path && (lib = dlopen(*path, RTLD_LAZY)) == NULL; path++)
        ; /* loop */
    if (lib == NULL) {
        return VTERM_ERROR;
    }
    if ((setup = (int(*)(char*,int,int*)) dlsym(lib, "setupterm")) == NULL) {
        return VTERM_ERROR;
    }
    LOG_DEBUG(g_vlib_log, "term: found ncurses <%s>", *path);
    if (setup(NULL, fd, &ret) != OK) {
        if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
        else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term not found.");
        else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db not found.");
        else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
        dlclose(lib);
        return VTERM_ERROR;
    }
    if ((getnum = (int(*)(char*)) dlsym(lib, "tigetnum")) == NULL
            ||  (    (ret = getnum(ti_col = "cols")) <= 0
                &&  (ret = getnum(ti_col = "columns")) <= 0
                &&  (ret = getnum(ti_col = "COLUMNS")) <= 0)) {
        getnum = NULL;
        ti_col = NULL;
    }
    if ((hascolors = (int(*)(void)) dlsym(lib, "has_colors")) != NULL) {
        if ((s_vlib_term_info.has_colors = hascolors()) == TRUE) {
            if ((startcolor = (int(*)(void)) dlsym(lib, "start_color")) != NULL) {
                startcolor();
            }
        }
    } else {
        s_vlib_term_info.has_colors = 0;
    }
    s_vlib_term_info.flags = flags;
    s_vlib_term_info.ti_col = ti_col;
    s_vlib_term_info.tigetnum = getnum;
    s_vlib_term_info.lib = lib;

    return VTERM_OK;
}
# pragma GCC diagnostic pop
# endif /* VLIB_CURSESDL */

#endif /* CONFIG_CURSES */

