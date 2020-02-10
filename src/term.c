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
# ifdef VTERM_INITSCR
#  include <termios.h>
# endif
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
#  define SCREEN void
int             setupterm(char *, int, int *);
void            filter();
void *          initscr();
void *          newterm(void*, int, int);
int             endwin();
int             tigetnum(char *);
char *          tigetstr(char *);
char *          tparm(const char *, ...);
int             del_curterm(void *);
extern void *   cur_term;
bool            has_colors();
bool            start_color();
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
    SCREEN * screen;
    char *          ti_col;
#   if defined(VLIB_CURSESDL)
    void *          lib;
    int             (*tigetnum)(char*);
#   endif
#  endif
} s_vterm_info = {
    VTF_DEFAULT, VTERM_FD_FREE, 0
#  if CONFIG_CURSES
    , NULL, NULL
#   if CONFIG_CURSES && defined(VLIB_CURSESDL)
    , NULL, NULL
#   endif
#  endif
};

/* Default colors strings corresponding to vterm_color_t */
static struct { char str[24]; int libcolor; } s_vterm_colors[VCOLOR_EMPTY+1] = {
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
    { { 0x1b, '[', '0', '7', 'm', 0, }, 0 }, /* Standout */
    /* Reserved internal values */
    { { 0x1b, '[', '0', '0', 'm', 0, }, COLOR_BLACK },      /* Reset */
    { { 0, }, 0 }                                           /* empty */
};

int vterm_enable(int enable) {
    if (enable) {
        if (s_vterm_info.fd < 0)
            s_vterm_info.fd = VTERM_FD_FREE;
    } else {
        if (s_vterm_info.fd >= 0)
            vterm_free();
        s_vterm_info.fd = VTERM_FD_BUSY;
    }
    LOG_VERBOSE(g_vlib_log, "vterm: changed enable state to %d", enable);
    return VTERM_OK;
}

int vterm_has_colors(int fd) {
    if (vterm_init(fd, s_vterm_info.flags) != VTERM_OK) {
       return 0;
    }
    return s_vterm_info.has_colors;
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

ssize_t vterm_putcolor(FILE * out, unsigned int colors) {
    ssize_t n;
    unsigned int     fg, bg, s;

    if ( ! vterm_has_colors(fileno(out))) {
        return 0;
    }

    if ((fg = VCOLOR_GET_FORE(colors)) > VCOLOR_EMPTY)
        fg = VCOLOR_EMPTY;

    if ((bg = VCOLOR_GET_BACK(colors)) > VCOLOR_EMPTY || bg < VCOLOR_BG)
        bg = VCOLOR_EMPTY;

    if ((s = VCOLOR_GET_STYLE(colors)) > VCOLOR_EMPTY || s < VCOLOR_STYLE)
        s = VCOLOR_EMPTY;

    n = fprintf(out, "%s%s%s",
                s_vterm_colors[fg].str,
                s_vterm_colors[bg].str,
                s_vterm_colors[s].str);

    if (n > 0) {
        return n;
    }

    return 0;
}

char * vterm_buildcolor(int fd, unsigned int colors, char * buffer, size_t * psize) {
    ssize_t         n;
    unsigned int    fg, bg, s;

    if (buffer == NULL || ! vterm_has_colors(fd)) {
        if (psize)
            *psize = 0;
        if (buffer != NULL)
            *buffer = 0;
        return buffer;
    }

    if ((fg = VCOLOR_GET_FORE(colors)) > VCOLOR_EMPTY)
        fg = VCOLOR_EMPTY;

    if ((bg = VCOLOR_GET_BACK(colors)) > VCOLOR_EMPTY || bg < VCOLOR_BG)
        bg = VCOLOR_EMPTY;

    if ((s = VCOLOR_GET_STYLE(colors)) > VCOLOR_EMPTY || s < VCOLOR_STYLE)
        s = VCOLOR_EMPTY;

    if (psize == NULL) {
        n = sprintf(buffer, "%s%s%s",
                    s_vterm_colors[fg].str,
                    s_vterm_colors[bg].str,
                    s_vterm_colors[s].str);

    } else {
        n = snprintf(buffer, *psize, "%s%s%s",
                     s_vterm_colors[fg].str,
                     s_vterm_colors[bg].str,
                     s_vterm_colors[s].str);
    }
    if (n <= 0) {
        *buffer = 0;
        if (psize != NULL)
            *psize = 0;
    } else {
        if (psize)
            *psize = n;
    }

    return buffer;
}

#if ! CONFIG_CURSES
/*******************************************************************
 * vterm implementation WITHOUT ncurses.
 ******************************************************************/

int vterm_init(int fd, unsigned int flags) {
    if (!isatty(fd) || s_vterm_info.fd == VTERM_FD_BUSY) { /* first thing to be done here */
        return VTERM_NOTTY;
    }
    if (s_vterm_info.fd != VTERM_FD_FREE)
        return VTERM_OK; /* already initialized */

    s_vterm_info.fd = VTERM_FD_BUSY;

    LOG_DEBUG(g_vlib_log, "vterm: initializing (flags=%u)", flags);

    s_vterm_info.has_colors = (flags & VTF_FORCE_COLORS) != 0;
    s_vterm_info.flags = flags;
    s_vterm_info.fd = fd; /* must be last */

    LOG_VERBOSE(g_vlib_log, "vterm: initialized (flags:%u has_colors:%d)",
                flags, s_vterm_info.has_colors);

    return VTERM_OK;
}

int vterm_free() {
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...", flags);

    LOG_VERBOSE(g_vlib_log, "%s(): done.", __func__);
    s_vterm_info.fd = VTERM_FD_FREE; /*must be last or LOG_* or other calls could redo vterm_init*/

    return VTERM_OK;
}

int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, s_vterm_info.flags)) == VTERM_NOTTY)
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

static void vterm_init_colors(int flags) {
    int     i;
    char *  capstr;
    char *  str;

    //cap = set_a_foreground ? set_a_foreground : set_foreground;
    if (((capstr = tigetstr("setaf")) == NULL || capstr == (char*) -1)
    &&  (capstr = tigetstr("setf")) == (char*)-1)
        capstr = NULL;

    for (i = VCOLOR_FG; i < VCOLOR_BG; i++) {
        if (capstr != NULL
        &&  (str = tparm (capstr, s_vterm_colors[i].libcolor)) != NULL) {
            str0cpy(s_vterm_colors[i].str, str, sizeof((*s_vterm_colors).str)
                    /sizeof(*(*s_vterm_colors).str));
        } else if ((flags & VTF_FORCE_COLORS) == 0) {
            *s_vterm_colors[i].str = 0;
        }
        LOG_DEBUG_BUF(g_vlib_log,
                   &(s_vterm_colors[i].str[0]), strlen(&s_vterm_colors[i].str[0]),
                   "colorfg[%02d] ", i);
    }


    //capstr = set_a_background ? set_a_background : set_background;
    if (((capstr = tigetstr("setab")) == NULL || capstr == (char*) -1)
    &&  (capstr = tigetstr("setb")) == (char*)-1)
        capstr = NULL;

    for (i = VCOLOR_BG; i < VCOLOR_STYLE; i++) {
        if (capstr != NULL
        &&  (str = tparm (capstr, s_vterm_colors[i].libcolor)) != NULL) {
            str0cpy(s_vterm_colors[i].str, str, sizeof((*s_vterm_colors).str)
                    /sizeof(*(*s_vterm_colors).str));
        } else if ((flags & VTF_FORCE_COLORS) == 0) {
            *s_vterm_colors[i].str = 0;
        }
        LOG_DEBUG_BUF(g_vlib_log,
                   s_vterm_colors[i].str, strlen(s_vterm_colors[i].str),
                   "colorbg[%02d] ", i);
    }

    static const struct cap_check_s { char * name; int idx; } caps_check[] = {
        { "sgr0",   VCOLOR_NORMAL },
        { "sgr0",   VCOLOR_RESET },
        { "blink",  VCOLOR_BLINK },
        { "bold",   VCOLOR_BOLD },
        { "sitm",   VCOLOR_ITALIC },
        { "smul",   VCOLOR_UNDERLINED },
        { "sshm",   VCOLOR_DARK },
        { "dim",    VCOLOR_DARK },
        { "smso",   VCOLOR_STANDOUT },
        { NULL,     0 }
    };
    for (const struct cap_check_s * cap = caps_check; cap->name != NULL; ++cap) {
        if ((capstr = tigetstr(cap->name)) != NULL && capstr != (char*)-1) {
            str0cpy(s_vterm_colors[cap->idx].str, capstr,
                    sizeof((*s_vterm_colors).str) / sizeof(*(*s_vterm_colors).str));
        } else {
            LOG_DEBUG(g_vlib_log, "vterm: cap '%s' not found for vcolor index %d",
                      cap->name, cap->idx);
            if ((flags & VTF_FORCE_COLORS) == 0)
                *s_vterm_colors[cap->idx].str = 0;
        }
    }

#   ifdef _DEBUG
    for (i = VCOLOR_STYLE; i < VCOLOR_RESERVED; i++) {
        LOG_BUFFER(LOG_LVL_DEBUG, g_vlib_log,
                       s_vterm_colors[i].str, strlen(s_vterm_colors[i].str),
                       "style[%02d] ", i);
    }
#   endif
}

int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, s_vterm_info.flags)) == VTERM_NOTTY)
        return 0;
    if (ret != VTERM_OK || s_vterm_info.ti_col == NULL) {
        return VTERM_ERROR;
    }
    return tigetnum(s_vterm_info.ti_col);
}

int vterm_free() {
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...");
    if (s_vterm_info.fd >= 0) {
#      ifdef VTERM_INITSCR
        if (s_vterm_info.screen != NULL) {
            delscreen(s_vterm_info.screen);
            s_vterm_info.screen = NULL;
        }
        endwin();
#      else
        if(cur_term != NULL) {
            del_curterm(cur_term);
            cur_term=NULL;
        }
#      endif
    }
    LOG_VERBOSE(g_vlib_log, "%s(): done.", __func__);
    s_vterm_info.fd = VTERM_FD_FREE;/* must be last or vterm could be reinit by log */

    return VTERM_OK;
}

int vterm_init(int fd, unsigned int flags) {
    /* checking tty and BUSY must be first thing to be done here */
    if (!isatty(fd) || s_vterm_info.fd == VTERM_FD_BUSY) {
        return VTERM_NOTTY;
    }
    if (s_vterm_info.fd != VTERM_FD_FREE) {
        return VTERM_OK;
    }
    int             ret;
    char *          ti_col;
    char *          setf;

    s_vterm_info.fd = VTERM_FD_BUSY;

    LOG_DEBUG(g_vlib_log, "vterm: initializing (flags=%u)", flags);

#ifndef VTERM_INITSCR
    if (setupterm(NULL, fd, &ret) != OK) {
        if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
        else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term '%s' not found.",
                                      (setf = getenv("TERM")) == NULL ? "" : setf);
        else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db for '%s' not found.",
                                      (setf = getenv("TERM")) == NULL ? "" : setf);
        else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
        s_vterm_info.fd = VTERM_FD_BUSY; /* never try again */
        return VTERM_ERROR;
    }
#else
    struct termios  term_conf, term_conf_bak;

    filter();
    /*s_vterm_info.screen = newterm(getenv("TERM"), fd == 1?stdout:stderr, stdin);*/
    if (initscr() == NULL) {
        LOG_ERROR(g_vlib_log, "initscr(): unknown error.");
        s_vterm_info.fd = VTERM_FD_FREE;
        return VTERM_ERROR;
    }

    tcgetattr(fd, &term_conf_bak);
    memcpy(&term_conf, &term_conf_bak, sizeof(term_conf));
    term_conf.c_oflag |= ONLRET | ONLCR | ECHO | ECHOE;
    tcsetattr(fd, TCSANOW, &term_conf);
    if (fd == STDOUT_FILENO)
        tcsetattr(STDERR_FILENO, TCSANOW, &term_conf);
    else if (fd == STDERR_FILENO)
        tcsetattr(STDOUT_FILENO, TCSANOW, &term_conf);

    fprintf(stderr, tigetstr("rmcup"));
    fprintf(stdout, tigetstr("rmcup"));
#endif

    if ((ret = tigetnum(ti_col = "cols")) <= 0
    &&         (ret = tigetnum(ti_col = "columns")) <= 0
    &&         (ret = tigetnum(ti_col = "COLUMNS")) <= 0) {
        LOG_ERROR(g_vlib_log, "tigetnum(cols,columns,COLUMNS): cannot get value");
        ti_col = NULL;
    }

    ret = tigetnum("colors");
    if (((setf = tigetstr("setaf")) == NULL || setf == (char*) -1)
    &&  (setf = tigetstr("setf")) == (char*)-1)
        setf = NULL;
    LOG_DEBUG(g_vlib_log, "vterm: setf:%lx max_colors:%d", (unsigned long) setf, ret);

    //if ((s_vterm_info.has_colors = has_colors()) == TRUE) { /*not always ok wihtout initscr()*/
    if ((s_vterm_info.has_colors = (setf != NULL && ret > 0)) != 0) {
        start_color();
    } else if ((flags & VTF_FORCE_COLORS) != 0) {
        s_vterm_info.has_colors = 1;
    }
    if (s_vterm_info.has_colors)
        vterm_init_colors(flags);

#ifdef VTERM_INITSCR
    tcsetattr(fd, TCSANOW, &term_conf_bak);
    if (fd == STDOUT_FILENO)
        tcsetattr(STDERR_FILENO, TCSANOW, &term_conf_bak);
    else if (fd == STDERR_FILENO)
        tcsetattr(STDOUT_FILENO, TCSANOW, &term_conf_bak);

    endwin();
#endif

    s_vterm_info.ti_col = ti_col;
    s_vterm_info.flags = flags;
    s_vterm_info.fd = fd; /* last thing to be done here */

    LOG_VERBOSE(g_vlib_log, "vterm: initialized (flags:%u has_colors:%d)",
                flags, s_vterm_info.has_colors);

    return VTERM_OK;
}

# else /* VLIB_CURSESDL */

int vterm_get_columns(int fd) {
    int ret;

    if ((ret = vterm_init(fd, s_vterm_info.flags)) == VTERM_NOTTY)
        return 0;
    if (ret != VTERM_OK || s_vterm_info.ti_col == NULL || s_vterm_info.lib == NULL) {
        return VTERM_ERROR;
    }
    return s_vterm_info.tigetnum(s_vterm_info.ti_col);
}

# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wpedantic"

int vterm_free() {
    if (s_vterm_info.lib == NULL) {
        return VTERM_OK;
    }
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...", flags);
    int (*delterm)(void *);
    void **curterm;

    if ((delterm = (int(*)(void*)) dlsym(s_vterm_info.lib, "del_curterm")) != NULL
    &&  (curterm = (void**)        dlsym(s_vterm_info.lib, "cur_term"))    != NULL
    &&  *curterm != NULL) {
        delterm(*curterm);
    }
    dlclose(s_vterm_info.lib);
    s_vterm_info.fd = VTERM_FD_FREE;
    s_vterm_info.lib = NULL;
    return VTERM_OK;
}

int vterm_init(int fd, unsigned int flags) {
    if (!isatty(fd) || s_vterm_info.fd == VTERM_FD_BUSY) {
        return VTERM_NOTTY;
    }
    if (s_vterm_info.lib != NULL) {
        return VTERM_OK; /* already initialized */
    }
    s_vterm_info.fd = VTERM_FD_BUSY;

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
        s_vterm_info.fd = VTERM_FD_FREE;
        return VTERM_ERROR;
    }
    if ((setup = (int(*)(char*,int,int*)) dlsym(lib, "setupterm")) == NULL) {
        s_vterm_info.fd = VTERM_FD_FREE;
        return VTERM_ERROR;
    }
    LOG_DEBUG(g_vlib_log, "term: found ncurses <%s>", *path);
    if (setup(NULL, fd, &ret) != OK) {
        if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
        else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term not found.");
        else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db not found.");
        else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
        dlclose(lib);
        s_vterm_info.fd = VTERM_FD_FREE;
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
    &&  (s_vterm_info.has_colors = hascolors()) == TRUE) {
        if ((startcolor = (int(*)(void)) dlsym(lib, "start_color")) != NULL) {
            startcolor();
        }
    } else {
        s_vterm_info.has_colors = (flags & VTF_FORCE_COLORS) != 0;
    }
    s_vterm_info.flags = flags;
    s_vterm_info.ti_col = ti_col;
    s_vterm_info.tigetnum = getnum;
    s_vterm_info.fd = fd;
    s_vterm_info.lib = lib; /* last thing to be done here */

    return VTERM_OK;
}
# pragma GCC diagnostic pop
# endif /* VLIB_CURSESDL */

#endif /* CONFIG_CURSES */

