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
# if ! CONFIG_CURSES_H || defined(VLIB_CURSESDL)
/* link to libncurses but no header found. define symbols */
#define bool char
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
#endif /* ! CONFIG_CURSES */
#if ! CONFIG_CURSSES || ! CONFIG_CURSES_H || defined(VLIB_CURSESDL)
#define COLOR_BLACK     0
#define COLOR_RED       1
#define COLOR_GREEN     2
#define COLOR_YELLOW    3
#define COLOR_BLUE      4
#define COLOR_MAGENTA   5
#define COLOR_CYAN      6
#define COLOR_WHITE     7
#endif
/* END NCURSES includes */

#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "vlib/log.h"
#include "vlib/term.h"
#include "vlib/util.h"

#include "vlib_private.h"

/* ************************************************************************* */

#define VTERM_FD_FREE   (-1)
#define VTERM_FD_BUSY   (-2)

static struct {
    unsigned int    flags;
    int             fd;
    int             has_colors;
#  if CONFIG_CURSES
    char *          ti_col;
#   if defined(VLIB_CURSESDL)
    void *          lib;
    int             (*tigetnum)(char*);
#   endif
#  endif
} s_vlib_term_info = {
    VTF_DEFAULT, VTERM_FD_FREE, 0
#  if CONFIG_CURSES
    , NULL
#   if CONFIG_CURSES && defined(VLIB_CURSESDL)
    , NULL, NULL
#   endif
#  endif
};

/* Default colors strings corresponding to vterm_color_t */
static struct { char str[16]; int libcolor; } s_vterm_colors[VCOLOR_EMPTY+1] = {
    /* Foreground Colors */
    { { 0x1b, '[', '3', '0', 'm', 0, }, COLOR_BLACK },      /* black */
    { { 0x1b, '[', '3', '1', 'm', 0, }, COLOR_RED },        /* red */
    { { 0x1b, '[', '3', '2', 'm', 0, }, COLOR_GREEN },      /* green */
    { { 0x1b, '[', '3', '3', 'm', 0, }, COLOR_YELLOW },     /* yellow */
    { { 0x1b, '[', '3', '4', 'm', 0, }, COLOR_BLUE },       /* blue */
    { { 0x1b, '[', '3', '5', 'm', 0, }, COLOR_MAGENTA },    /* magenta */
    { { 0x1b, '[', '3', '6', 'm', 0, }, COLOR_CYAN },       /* cyan */
    { { 0x1b, '[', '3', '7', 'm', 0, }, COLOR_WHITE },      /* white */
    /* Background Colors */
    { { 0x1b, '[', '4', '0', 'm', 0, }, COLOR_BLACK },      /* black */
    { { 0x1b, '[', '4', '1', 'm', 0, }, COLOR_RED },        /* red */
    { { 0x1b, '[', '4', '2', 'm', 0, }, COLOR_GREEN },      /* green */
    { { 0x1b, '[', '4', '3', 'm', 0, }, COLOR_YELLOW },     /* yellow */
    { { 0x1b, '[', '4', '4', 'm', 0, }, COLOR_BLUE },       /* blue */
    { { 0x1b, '[', '4', '5', 'm', 0, }, COLOR_MAGENTA },    /* magenta */
    { { 0x1b, '[', '4', '6', 'm', 0, }, COLOR_CYAN },       /* cyan */
    { { 0x1b, '[', '4', '7', 'm', 0, }, COLOR_WHITE },      /* white */
    /* Styles */
    { { 0x1b, '[', '0', '0', 'm', 0, }, 0 }, /* Normal */
    { { 0x1b, '[', '0', '1', 'm', 0, }, 0 }, /* Bold */
    { { 0x1b, '[', '0', '2', 'm', 0, }, 0 }, /* Dark */
    { { 0x1b, '[', '0', '3', 'm', 0, }, 0 }, /* Italic */
    { { 0x1b, '[', '0', '4', 'm', 0, }, 0 }, /* Underlined */
    { { 0x1b, '[', '0', '5', 'm', 0, }, 0 }, /* Blinking */
    /* Reserved internal values */
    { { 0x1b, '[', '0', '0', 'm', 0, }, COLOR_BLACK },      /* Reset */
    { { 0, }, 0 }                                           /* empty */
};

#if 0
static char * s_vterm_colors[VCOLOR_EMPTY+1] = {
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
#endif

int vterm_has_colors(int fd) {
    if (vterm_init(fd, s_vlib_term_info.flags) != VTERM_OK) {
       return 0;
    }
    return s_vlib_term_info.has_colors;
}

const char * vterm_color(int fd, vterm_color_t color) {
    if (vterm_has_colors(fd)) {
        if ((int)color < 0 || color >= sizeof(s_vterm_colors) / sizeof(*s_vterm_colors)) {
            return s_vterm_colors[VCOLOR_RESET].str;
        } else {
            return s_vterm_colors[color].str;
        }
    }
    return s_vterm_colors[VCOLOR_EMPTY].str;
}

#if ! CONFIG_CURSES
/*******************************************************************
 * vterm implementation WITHOUT ncurses.
 ******************************************************************/

int vterm_init(int fd, unsigned int flags) {
    if (!isatty(fd) || s_vlib_term_info.fd == VTERM_FD_BUSY) { /* first thing to be done here */
        return VTERM_NOTTY;
    }
    if (s_vlib_term_info.fd != VTERM_FD_FREE)
        return VTERM_OK; /* already initialized */

    s_vlib_term_info.fd = VTERM_FD_BUSY;

    LOG_DEBUG(g_vlib_log, "vterm: initializing (flags=%u)", flags);

    s_vlib_term_info.has_colors = (flags & VTF_FORCE_COLORS) != 0;
    s_vlib_term_info.flags = flags;
    s_vlib_term_info.fd = fd;
    return VTERM_OK;
}

int vterm_free() {
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...", flags);

    s_vlib_term_info.fd = VTERM_FD_FREE;
    return VTERM_OK;
}

int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, s_vlib_term_info.flags)) == VTERM_NOTTY)
        return 0;
    if (ret != VTERM_OK) {
        return VTERM_ERROR;
    }

    char *s = getenv("COLUMNS");
    char *endp = NULL;
    long res;

    if (s == NULL)
        return 0;

    errno = 0;
    res = strtol(s, &endp, 0);
    if (endp == NULL || *endp != 0 || errno != 0)
        return VTERM_ERROR;

    return res;
}

# else /* CONFIG_CURSES */
/***************************************************************************
 * vterm implementation WITH ncurses.
 **************************************************************************/

# if ! defined(VLIB_CURSESDL)

# if CONFIG_CURSES_H
static void vterm_init_colors() {
    int     i;
    char *  cap;
    char *  str;

    cap = set_a_foreground ? set_a_foreground : set_foreground;
    if (cap != NULL) {
        for (i = VCOLOR_FG; i < VCOLOR_BG; i++) {
            if ((str = tparm (cap, s_vterm_colors[i].libcolor)) != NULL) {
                str0cpy(s_vterm_colors[i].str, str, sizeof((*s_vterm_colors).str)
                                                    /sizeof(*(*s_vterm_colors).str));
            }
            LOG_BUFFER(LOG_LVL_DEBUG, g_vlib_log,
                       &(s_vterm_colors[i].str[0]), strlen(&s_vterm_colors[i].str[0]),
                       "colorfg[%02d] ", i);
        }
    }
    cap = set_a_background ? set_a_background : set_background;
    if (cap != NULL) {
        for (i = VCOLOR_BG; i < VCOLOR_STYLE; i++) {
            if ((str = tparm (cap, s_vterm_colors[i].libcolor)) != NULL) {
                str0cpy(s_vterm_colors[i].str, str, sizeof((*s_vterm_colors).str)
                                                    /sizeof(*(*s_vterm_colors).str));
            }
            LOG_BUFFER(LOG_LVL_DEBUG, g_vlib_log,
                       s_vterm_colors[i].str, strlen(s_vterm_colors[i].str),
                       "colorbg[%02d] ", i);
        }
    }
}
# else
static void vterm_init_colors() {}
# endif

int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, s_vlib_term_info.flags)) == VTERM_NOTTY)
        return 0;
    if (ret != VTERM_OK || s_vlib_term_info.ti_col == NULL) {
        return VTERM_ERROR;
    }
    return tigetnum(s_vlib_term_info.ti_col);
}

int vterm_free() {
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...");
    if (s_vlib_term_info.fd >= 0 && cur_term != NULL) {
        del_curterm(cur_term);
    }
    s_vlib_term_info.fd = VTERM_FD_FREE;
    return VTERM_OK;
}

int vterm_init(int fd, unsigned int flags) {
    /* checking tty and BUSY must be first thing to be done here */
    if (!isatty(fd) || s_vlib_term_info.fd == VTERM_FD_BUSY) {
        return VTERM_NOTTY;
    }
    if (s_vlib_term_info.fd != VTERM_FD_FREE) {
        return VTERM_OK;
    }
    int     ret;
    char *  ti_col;

    s_vlib_term_info.fd = VTERM_FD_BUSY;

    LOG_DEBUG(g_vlib_log, "vterm: initializing (flags=%u)", flags);

    if (setupterm(NULL, fd, &ret) != OK) {
        if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
        else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term not found.");
        else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db not found.");
        else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
        s_vlib_term_info.fd = VTERM_FD_FREE;
        return VTERM_ERROR;
    } else if ((ret = tigetnum(ti_col = "cols")) <= 0
    &&         (ret = tigetnum(ti_col = "columns")) <= 0
    &&         (ret = tigetnum(ti_col = "COLUMNS")) <= 0) {
        LOG_ERROR(g_vlib_log, "tigetnum(cols,columns,COLUMNS): cannot get value");
        ti_col = NULL;
    }

    if ((s_vlib_term_info.has_colors = has_colors()) == TRUE) {
        start_color();
        vterm_init_colors();
    } else if ((flags & VTF_FORCE_COLORS) != 0) {
        s_vlib_term_info.has_colors = 1;
    }

    s_vlib_term_info.ti_col = ti_col;
    s_vlib_term_info.flags = flags;
    s_vlib_term_info.fd = fd; /* last thing to be done here */

    return VTERM_OK;
}

# else /* VLIB_CURSESDL */

int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, s_vlib_term_info.flags)) == VTERM_NOTTY)
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
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...", flags);
    int (*delterm)(void *);
    void **curterm;

    if ((delterm = (int(*)(void*)) dlsym(s_vlib_term_info.lib, "del_curterm")) != NULL
    &&  (curterm = (void**)        dlsym(s_vlib_term_info.lib, "cur_term"))    != NULL
    &&  *curterm != NULL) {
        delterm(*curterm);
    }
    dlclose(s_vlib_term_info.lib);
    s_vlib_term_info.fd = VTERM_FD_FREE;
    s_vlib_term_info.lib = NULL;
    return VTERM_OK;
}

int vterm_init(int fd, unsigned int flags) {
    if (!isatty(fd) || s_vlib_term_info.fd == VTERM_FD_BUSY) {
        return VTERM_NOTTY;
    }
    if (s_vlib_term_info.lib != NULL) {
        return VTERM_OK; /* already initialized */
    }
    s_vlib_term_info.fd = VTERM_FD_BUSY;

    LOG_DEBUG(g_vlib_log, "vterm: initializing (flags=%u)", flags);

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
        s_vlib_term_info.fd = VTERM_FD_FREE;
        return VTERM_ERROR;
    }
    if ((setup = (int(*)(char*,int,int*)) dlsym(lib, "setupterm")) == NULL) {
        s_vlib_term_info.fd = VTERM_FD_FREE;
        return VTERM_ERROR;
    }
    LOG_DEBUG(g_vlib_log, "term: found ncurses <%s>", *path);
    if (setup(NULL, fd, &ret) != OK) {
        if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
        else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term not found.");
        else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db not found.");
        else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
        dlclose(lib);
        s_vlib_term_info.fd = VTERM_FD_FREE;
        return VTERM_ERROR;
    }
    if ((getnum = (int(*)(char*)) dlsym(lib, "tigetnum")) == NULL
            ||  (    (ret = getnum(ti_col = "cols")) <= 0
                &&  (ret = getnum(ti_col = "columns")) <= 0
                &&  (ret = getnum(ti_col = "COLUMNS")) <= 0)) {
        getnum = NULL;
        ti_col = NULL;
    }
    if ((hascolors = (int(*)(void)) dlsym(lib, "has_colors")) != NULL
    &&  (s_vlib_term_info.has_colors = hascolors()) == TRUE) {
        if ((startcolor = (int(*)(void)) dlsym(lib, "start_color")) != NULL) {
            startcolor();
        }
    } else {
        s_vlib_term_info.has_colors = (flags & VTF_FORCE_COLORS) != 0;
    }
    s_vlib_term_info.flags = flags;
    s_vlib_term_info.ti_col = ti_col;
    s_vlib_term_info.tigetnum = getnum;
    s_vlib_term_info.fd = fd;
    s_vlib_term_info.lib = lib; /* last thing to be done here */

    return VTERM_OK;
}
# pragma GCC diagnostic pop
# endif /* VLIB_CURSESDL */

#endif /* CONFIG_CURSES */

