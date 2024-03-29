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

/* using libdl to get curses is deprecated / and currently broken */
/* #define VLIB_CURSESDL */

/* checks ncurses includes */
#ifndef CONFIG_CURSES
# define CONFIG_CURSES 0
#endif
#ifndef CONFIG_CURSES_H
# define CONFIG_CURSES_H 0
#endif
#ifndef CONFIG_CURSES_SCR
# define CONFIG_CURSES_SCR 0
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
char *          tigetstr(char *);
char *          tparm(const char *, ...);
int             del_curterm(void *);
extern void *   cur_term;
#  if CONFIG_CURSES_SCR
#   define SCREEN void
bool            has_colors();
bool            start_color();
void            filter();
void *          initscr();
void *          newterm(void*, int, int);
int             endwin();
void            delscreen(SCREEN *);
int             mvprintw(int, int, const char *, ...);
void            refresh();
#  endif
# endif
#endif /* ! CONFIG_CURSES */
#if ! CONFIG_CURSES || ! CONFIG_CURSES_H || defined(VLIB_CURSESDL)
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

#include <unistd.h>
#include <sys/select.h>
#include <sys/ioctl.h>
#include <poll.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdarg.h>
#include <termios.h>
#include <signal.h>
#include <ctype.h>

#include "vlib/log.h"
#include "vlib/term.h"
#include "vlib/util.h"

#include "vlib_private.h"

/* ************************************************************************* */

#if defined(TIOCGWINSZ)
# define VTERM_IOCTL_GETWINSZ   (TIOCGWINSZ)
# define VTERM_WINSIZE_T        struct winsize
# define VTERM_WINSZ_COLS(s)    (s)->ws_col
# define VTERM_WINSZ_ROWS(s)    (s)->ws_row
#elif defined(TIOCGSIZE)
# define VTERM_IOCTL_GETWINSZ   (TIOCGSIZE)
# define VTERM_WINSZ_T          struct ttysize
# define VTERM_WINSZ_COLS(s)    (s)->ts_cols
# define VTERM_WINSZ_ROWS(s)    (s)->ts_lines
#else
# undef VTERM_IOCTL_GETWINSZ
#endif

/** historical setf/setb
 * black     COLOR_BLACK       0     0, 0, 0
 * blue      COLOR_BLUE        1     0,0,max
 * green     COLOR_GREEN       2     0,max,0
 * cyan      COLOR_CYAN        3     0,max,max
 * red       COLOR_RED         4     max,0,0
 * magenta   COLOR_MAGENTA     5     max,0,max
 * yellow    COLOR_YELLOW      6     max,max,0
 * white     COLOR_WHITE       7     max,max,max */
#define COLOR_HIST_BLACK        0
#define COLOR_HIST_BLUE         1
#define COLOR_HIST_GREEN        2
#define COLOR_HIST_CYAN         3
#define COLOR_HIST_RED          4
#define COLOR_HIST_MAGENTA      5
#define COLOR_HIST_YELLOW       6
#define COLOR_HIST_WHITE        7

/* ************************************************************************* */

#define VTERM_FD_FREE   (-1)
#define VTERM_FD_BUSY   (-2)

static struct {
    unsigned int        flags;
    int                 fd;
    int                 has_colors;
    vterm_colorset_t    term_fgbg;
    int                 fixedwinsz;
#  if CONFIG_CURSES
    char *              ti_col;
    char *              ti_cup;
    char *              ti_clr;
#   if CONFIG_CURSES_SCR
    SCREEN *            screen;
#   endif
#   if defined(VLIB_CURSESDL)
    void *              lib;
    int                 (*tigetnum)(char*);
#   endif
    struct termios      termio_bak;
#  endif
} s_vterm_info = {
    .flags = VTF_DEFAULT, .fd = VTERM_FD_FREE, .has_colors = 0,
    .term_fgbg = VCOLOR_NULL, .fixedwinsz = -1
#  if CONFIG_CURSES
    , .ti_col = NULL, .ti_cup = NULL, .ti_clr = NULL
#   if CONFIG_CURSES_SCR
    , .screen = NULL
#   endif
#   if CONFIG_CURSES && defined(VLIB_CURSESDL)
    , .lib = NULL, .tigetnum = NULL
#   endif
#  endif
};

/* Default colors strings corresponding to vterm_color_t
 * !!! must be in same order as enum vterm_color_t */
enum {
    VCOLOR_STATIC = 0,
    VCOLOR_ALLOC = 1,
};
enum {              /* indexes of libcolor */
    VCOLOR_SETAF    = 0,
    VCOLOR_SETF     = 1,
};
typedef struct { char * str; int libcolor[5]; unsigned int size; int alloc; } vterm_colorinfo_t;
static const vterm_colorinfo_t s_vterm_colors_default[VCOLOR_EMPTY+1] = {
    /* Foreground Colors */
    { "\x1b[30m", { COLOR_BLACK, COLOR_HIST_BLACK,},    0, VCOLOR_STATIC },
    { "\x1b[31m", { COLOR_RED, COLOR_HIST_RED,},        0, VCOLOR_STATIC },
    { "\x1b[32m", { COLOR_GREEN, COLOR_HIST_GREEN,},    0, VCOLOR_STATIC },
    { "\x1b[33m", { COLOR_YELLOW, COLOR_HIST_YELLOW,},  0, VCOLOR_STATIC },
    { "\x1b[34m", { COLOR_BLUE, COLOR_HIST_BLUE,},      0, VCOLOR_STATIC },
    { "\x1b[35m", { COLOR_MAGENTA, COLOR_HIST_MAGENTA,},0, VCOLOR_STATIC },
    { "\x1b[36m", { COLOR_CYAN, COLOR_HIST_CYAN,},      0, VCOLOR_STATIC },
    { "\x1b[37m", { COLOR_WHITE, COLOR_HIST_WHITE,},    0, VCOLOR_STATIC },
    /* Background Colors */
    { "\x1b[40m", { COLOR_BLACK, COLOR_HIST_BLACK,},    0, VCOLOR_STATIC },
    { "\x1b[41m", { COLOR_RED, COLOR_HIST_RED,},        0, VCOLOR_STATIC },
    { "\x1b[42m", { COLOR_GREEN, COLOR_HIST_GREEN,},    0, VCOLOR_STATIC },
    { "\x1b[43m", { COLOR_YELLOW, COLOR_HIST_YELLOW,},  0, VCOLOR_STATIC },
    { "\x1b[44m", { COLOR_BLUE, COLOR_HIST_BLUE,},      0, VCOLOR_STATIC },
    { "\x1b[45m", { COLOR_MAGENTA, COLOR_HIST_MAGENTA,},0, VCOLOR_STATIC },
    { "\x1b[46m", { COLOR_CYAN, COLOR_HIST_CYAN,},      0, VCOLOR_STATIC },
    { "\x1b[47m", { COLOR_WHITE, COLOR_HIST_WHITE,},    0, VCOLOR_STATIC },
    /* Styles */
    { "\x1b[00m", {0,}, 0, VCOLOR_STATIC }, /* Normal */
    { "\x1b[01m", {0,}, 0, VCOLOR_STATIC }, /* Bold */
    { "\x1b[02m", {0,}, 0, VCOLOR_STATIC }, /* Dark */
    { "\x1b[03m", {0,}, 0, VCOLOR_STATIC }, /* Italic */
    { "\x1b[04m", {0,}, 0, VCOLOR_STATIC }, /* Underlined */
    { "\x1b[05m", {0,}, 0, VCOLOR_STATIC }, /* Blinking */
    { "\x1b[07m", {0,}, 0, VCOLOR_STATIC }, /* Standout */
    /* Reserved internal values */
    { "\x1b[00m", {0,}, 0, VCOLOR_STATIC }, /* Reset */
    { "",         {0,}, 0, VCOLOR_STATIC }  /* empty */
};
static vterm_colorinfo_t s_vterm_colors[VCOLOR_EMPTY+1] = { { NULL, {0,}, 0, 0 }, };

/** various terminal capabilty strings such as keys, ...
 * !!! must be in same order as enum vterm_cap_t */
typedef struct { char * str; char * caps[5];
                 int id; int alloc; unsigned int size; } vterm_caps_t;
static vterm_caps_t s_vterm_caps_default[VTERM_CAPS_NB + 1] = {
    { "",                   { NULL,},       VTERM_CAP_EMPTY,    VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x41",       {"cuu1",NULL},  VTERM_CAP_UP,       VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x42",       {"cud1",NULL},  VTERM_CAP_DOWN,     VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x44",       {"cub1",NULL},  VTERM_CAP_LEFT,     VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x43",       {"cuf1",NULL},  VTERM_CAP_RIGHT,    VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x3f\x31\x68\x1b\x3d",{"smkx",NULL},VTERM_CAP_KB_KEYCODE,VCOLOR_STATIC,0 },
    { "\x1b\x5b\x3f\x31\x6c\x1b\x3e",{"rmkx",NULL},VTERM_CAP_NO_KEYCODE,VCOLOR_STATIC,0 },
    { "",                   { NULL,},       VTERM_KEY_UNKNOWN,  VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x41",       {"kcuu1",NULL}, VTERM_KEY_UP,       VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x42",       {"kcud1",NULL}, VTERM_KEY_DOWN,     VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x44",       {"kcub1",NULL}, VTERM_KEY_LEFT,     VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x43",       {"kcuf1",NULL}, VTERM_KEY_RIGHT,    VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x31\x3b\x32\x44",{"kLFT",NULL},VTERM_KEY_SH_LEFT,VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x31\x3b\x32\x43",{"kRIT",NULL},VTERM_KEY_SH_RIGHT,VCOLOR_STATIC, 0 },
    { "\x8",                {"kbs",NULL},   VTERM_KEY_BACKSPACE,VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x33\x7e",   {"kdch1",NULL}, VTERM_KEY_DEL,      VCOLOR_STATIC, 0 },
    { "\x9",                {"ktab",NULL},  VTERM_KEY_TAB,      VCOLOR_STATIC, 0 },
    { "\x1b",               {"kext",NULL},  VTERM_KEY_ESC,      VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x48",       {"khome",NULL}, VTERM_KEY_HOME,     VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x46",       {"kend",NULL},  VTERM_KEY_END,      VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x35\x7e",   {"kpp",NULL},   VTERM_KEY_PAGEUP,   VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x36\x7e",   {"knp",NULL},   VTERM_KEY_PAGEDOWN, VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x50",       {"kf1",NULL},   VTERM_KEY_F1,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x51",       {"kf2",NULL},   VTERM_KEY_F2,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x52",       {"kf3",NULL},   VTERM_KEY_F3,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x53",       {"kf4",NULL},   VTERM_KEY_F4,       VCOLOR_STATIC, 0 },
    { "\x1b\x5b\x31\x35\x7e",{"kf5",NULL},  VTERM_KEY_F5,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x31\x37\x7e",{"kf6",NULL},  VTERM_KEY_F6,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x31\x38\x7e",{"kf7",NULL},  VTERM_KEY_F7,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x31\x39\x7e",{"kf8",NULL},  VTERM_KEY_F8,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x32\x30\x7e",{"kf9",NULL},  VTERM_KEY_F9,       VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x32\x31\x7e",{"kf10",NULL}, VTERM_KEY_F10,      VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x32\x33\x7e",{"kf11",NULL}, VTERM_KEY_F11,      VCOLOR_STATIC, 0 },
    { "\x1b\x4f\x32\x34\x7e",{"kf12",NULL}, VTERM_KEY_F12,      VCOLOR_STATIC, 0 },
    /* TODO: use struct termios.c_cc[] */
    { "\x1a"                ,{"kspd",NULL}, VTERM_KEY_STOP,     VCOLOR_STATIC, 0 },
    { "\x4"                 ,{NULL},        VTERM_KEY_EOF,      VCOLOR_STATIC, 0 },
    { "\x3"                 ,{"kcan",NULL}, VTERM_KEY_INT,      VCOLOR_STATIC, 0 },
    { "", { NULL, }, 0, 0, 0 }
};
static vterm_caps_t s_vterm_caps[VTERM_CAPS_NB+1] = { { NULL, {NULL,}, 0, 0, 0 }, };

/* ************************************************************************* */
static int vterm_init_default_colors(int set_static) {
    for (unsigned int i = 0; i <= VCOLOR_EMPTY; ++i) {
        s_vterm_colors[i] = s_vterm_colors_default[i];
        s_vterm_colors[i].size = strlen(s_vterm_colors[i].str);
        if (set_static)
            s_vterm_colors[i].alloc = VCOLOR_STATIC;
    }
    s_vterm_info.term_fgbg = VCOLOR_NULL;
    s_vterm_info.fixedwinsz = -1;
    return VTERM_OK;
}

/* ************************************************************************* */
static int vterm_free_colors() {
    for (unsigned int i = 0; i <= VCOLOR_EMPTY; ++i) {
        if (s_vterm_colors[i].alloc != VCOLOR_STATIC
        &&  s_vterm_colors[i].str != NULL) {
            free(s_vterm_colors[i].str);
            s_vterm_colors[i].alloc = VCOLOR_STATIC;
            s_vterm_colors[i].str = NULL;
        }
    }
    return VTERM_OK;
}

/* ************************************************************************* */
static int vterm_init_default_caps(int set_static) {
    for (unsigned int i = 0; i <= VTERM_CAPS_NB; ++i) {
        s_vterm_caps[i] = s_vterm_caps_default[i];
        s_vterm_caps[i].size = strlen(s_vterm_caps[i].str);
        s_vterm_caps_default[i].size = s_vterm_caps[i].size;
        if (set_static)
            s_vterm_caps[i].alloc = VCOLOR_STATIC;
    }
    return VTERM_OK;
}

/* ************************************************************************* */
static int vterm_free_caps() {
    for (unsigned int i = 0; i <= VTERM_CAPS_NB; ++i) {
        if (s_vterm_caps[i].alloc != VCOLOR_STATIC
        &&  s_vterm_caps[i].str != NULL) {
            free(s_vterm_caps[i].str);
            s_vterm_caps[i].alloc = VCOLOR_STATIC;
            s_vterm_caps[i].str = NULL;
        }
    }
    return VTERM_OK;
}

/* ************************************************************************* */
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

/* ************************************************************************* */
int vterm_has_colors(int fd) {
    if (vterm_init(fd, s_vterm_info.flags) != VTERM_OK) {
       return 0;
    }
    return s_vterm_info.has_colors;
}

/* ************************************************************************* */
vterm_colorset_t vterm_termfgbg(int fd) {
    static const vterm_colorset_t termfgbg_default
        = VCOLOR_BUILD(VCOLOR_WHITE, VCOLOR_BG_BLACK, VCOLOR_EMPTY);
    const char *    env;    

    if (vterm_init(fd, s_vterm_info.flags) != VTERM_OK) {
        LOG_DEBUG(g_vlib_log, "%s(): no tty or init failed, using default (%u,%u,%u)",
                  __func__, VCOLOR_GET_FORE(termfgbg_default),
                  VCOLOR_GET_BACK(termfgbg_default),
                  VCOLOR_GET_STYLE(termfgbg_default));
        return termfgbg_default;
    }

    if (s_vterm_info.term_fgbg != VCOLOR_NULL) {
        LOG_DEBUG(g_vlib_log, "%s(): reusing termfgbg: (%u,%u,%u)",
                  __func__, VCOLOR_GET_FORE(s_vterm_info.term_fgbg),
                  VCOLOR_GET_BACK(s_vterm_info.term_fgbg),
                  VCOLOR_GET_STYLE(s_vterm_info.term_fgbg));
        return s_vterm_info.term_fgbg;
    }

    if ((env = getenv("COLORFGBG")) != NULL) {
        const char *    token;
        size_t          len;
        unsigned long   fg, bg;
    
        len = strtok_ro_r(&token, ";", &env, NULL, 0);
        // TODO COLOR CONVERSION NOT 100% accurate
        if (len > 0) {
            fg = strtol(token, NULL, 10);
            fg = (fg % (VCOLOR_BG - VCOLOR_FG)) + VCOLOR_FG;
        } else {
            fg = VCOLOR_GET_FORE(termfgbg_default);
        }
        if (len > 0 && env && *env) {
            bg = strtol(env, NULL, 10);
            bg = (bg % (VCOLOR_STYLE - VCOLOR_BG)) + VCOLOR_BG;
        } else {
            bg = VCOLOR_GET_BACK(termfgbg_default);
        }

        s_vterm_info.term_fgbg = VCOLOR_BUILD(fg, bg, VCOLOR_EMPTY);

        LOG_DEBUG(g_vlib_log, "%s(): COLORFGBG (%u,%u,%u)", __func__,
                  VCOLOR_GET_FORE(s_vterm_info.term_fgbg),
                  VCOLOR_GET_BACK(s_vterm_info.term_fgbg),
                  VCOLOR_GET_STYLE(s_vterm_info.term_fgbg));

        return s_vterm_info.term_fgbg;
    }

    struct termios term_conf, term_conf_bak;
    tcgetattr(fd, &term_conf_bak);
    memcpy(&term_conf, &term_conf_bak, sizeof(term_conf));
    cfmakeraw(&term_conf); // no ECHO, VMIN=0, VTIME=0
    tcsetattr(fd, TCSANOW, &term_conf);

    if (write(fd, "\033]11;?\033\\", 8) == 8) { // -> "11;rgb:0f0f/0e0e/1111"
        char            buf[128];
        ssize_t         n;
        struct pollfd   pollfd = { .fd = STDIN_FILENO, .events = POLLIN };
        
        buf[sizeof(buf)-1] = 0;
        if (poll(&pollfd, 1, 50) > 0 && (pollfd.revents & POLLIN) != 0
        &&  (n = read(STDIN_FILENO, buf, sizeof(buf)-1)) > 0
        &&  strstr(buf, "11;rgb:") != NULL) {
            char * rgb = strchr(buf, ':');
            int R = strtol(rgb+1, &rgb, 16) % 256;
            int G = strtol(rgb+1, &rgb, 16) % 256;
            int B = strtol(rgb+1, &rgb, 16) % 256;                        
            int gray = 0.2126 * R + 0.7152 * G + 0.0722 * B;
            
            tcsetattr(fd, TCSANOW, &term_conf_bak);
            s_vterm_info.term_fgbg = VCOLOR_BUILD(
                gray < 128 ? VCOLOR_WHITE : VCOLOR_BLACK, 
                gray < 128 ? VCOLOR_BG_BLACK : VCOLOR_BG_WHITE, VCOLOR_EMPTY);
            
            LOG_DEBUG(g_vlib_log, "%s(): xterm-esc-seq (%u,%u,%u) [%d/%d/%d %d]",
                      __func__, VCOLOR_GET_FORE(s_vterm_info.term_fgbg),
                      VCOLOR_GET_BACK(s_vterm_info.term_fgbg),
                      VCOLOR_GET_STYLE(s_vterm_info.term_fgbg), R, G, B, gray);
            return s_vterm_info.term_fgbg;
        } else {
            tcsetattr(fd, TCSANOW, &term_conf_bak);
        }
    }

    LOG_DEBUG(g_vlib_log, "%s(): no COLORFGBG, using default (%u,%u,%u)",
              __func__, VCOLOR_GET_FORE(termfgbg_default),
              VCOLOR_GET_BACK(termfgbg_default),
              VCOLOR_GET_STYLE(termfgbg_default));
    s_vterm_info.term_fgbg = termfgbg_default;
    return s_vterm_info.term_fgbg;    
}

/* ************************************************************************* */
static inline unsigned int vterm_color_index(int fd, vterm_color_t color) {
    if (vterm_has_colors(fd)) {
        if ((unsigned int)color >= sizeof(s_vterm_colors) / sizeof(*s_vterm_colors)) {
            return VCOLOR_RESET;
        } else {
            return color;
        }
    } else if (s_vterm_colors[0].str == NULL) {
        vterm_init_default_colors(0);
    }
    return VCOLOR_EMPTY;
}

/* ************************************************************************* */
const char * vterm_color(int fd, vterm_color_t color) {
    return s_vterm_colors[vterm_color_index(fd, color)].str;
}

/* ************************************************************************* */
unsigned int vterm_color_size(int fd, vterm_color_t color) {
    return s_vterm_colors[vterm_color_index(fd, color)].size;
}

/* ************************************************************************* */
unsigned int vterm_color_maxsize(int fd) {
    unsigned int i, size, max = 0;

    for (i = 0; i <= VCOLOR_EMPTY; ++i) {
        if ((size = s_vterm_colors[vterm_color_index(fd, i)].size) > max)
            max = size;
    }
    return max;
}

/* ************************************************************************* */
ssize_t vterm_putcolor(FILE * out, vterm_colorset_t colors) {
    ssize_t             n;
    vterm_colorset_t    fg, bg, s; /* could be vterm_color_t, no pb as bounds are checked */

    if ( out == NULL || ! vterm_has_colors(fileno(out))) {
        return 0;
    }

    if ((fg = VCOLOR_GET_FORE(colors)) >= VCOLOR_BG && fg != VCOLOR_RESET)
        fg = VCOLOR_EMPTY;

    if ((bg = VCOLOR_GET_BACK(colors)) >= VCOLOR_STYLE || bg < VCOLOR_BG)
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

/* ************************************************************************* */
char * vterm_buildcolor(int fd, vterm_colorset_t colors, char * buffer, size_t * psize) {
    ssize_t             n;
    vterm_colorset_t    fg, bg, s; /* could be vterm_color_t, no pb as bounds are checked */
    int ret;

    if ((buffer != NULL && psize == NULL)
    ||  (psize != NULL && *psize <= 1) || ! vterm_has_colors(fd)) {
        if (psize != NULL) {
            if (*psize == 0) {
                return buffer == NULL ? strdup("") : NULL;
            }
            *psize = 0;
        }
        if (buffer != NULL) {
            *buffer = 0;
            return buffer;
        }
        return strdup("");
    }

    if ((fg = VCOLOR_GET_FORE(colors)) >= VCOLOR_BG && fg != VCOLOR_RESET)
        fg = VCOLOR_EMPTY;

    if ((bg = VCOLOR_GET_BACK(colors)) >= VCOLOR_STYLE || bg < VCOLOR_BG)
        bg = VCOLOR_EMPTY;

    if ((s = VCOLOR_GET_STYLE(colors)) > VCOLOR_EMPTY || s < VCOLOR_STYLE)
        s = VCOLOR_EMPTY;

    if (buffer == NULL) {
        n = s_vterm_colors[fg].size + s_vterm_colors[bg].size
          + s_vterm_colors[s].size + 1;
        if ((buffer = malloc(n)) == NULL) {
            if (psize != NULL)
                *psize = 0;
            return NULL;
        }
    } else
        n = *psize;

    n = VLIB_SNPRINTF(ret, buffer, n, "%s%s%s",
            s_vterm_colors[fg].str,
            s_vterm_colors[bg].str,
            s_vterm_colors[s].str);

    if (n <= 0) {
        *buffer = 0;
        if (psize != NULL)
            *psize = 0;
    } else if (psize != NULL) {
        *psize = n;
    }

    return buffer;
}

/* ************************************************************************* */
static inline unsigned int vterm_cap_index(int fd, vterm_cap_t cap) {
    int ret = vterm_init(fd, s_vterm_info.flags);
    if (ret == VTERM_OK) {
        if ((unsigned int)cap >= VTERM_CAPS_NB) {
            return VTERM_CAP_EMPTY;
        } else {
            return cap;
        }
    } else if (s_vterm_caps[0].str == NULL) {
        vterm_init_default_caps(0);
    }
    return VTERM_CAP_EMPTY;
}

/* ************************************************************************* */
const char * vterm_cap(int fd, vterm_cap_t cap) {
    return s_vterm_caps[vterm_cap_index(fd, cap)].str;
}

/* ************************************************************************* */
unsigned int vterm_cap_size(int fd, vterm_cap_t cap) {
    return s_vterm_caps[vterm_cap_index(fd, cap)].size;
}

/* ************************************************************************* */
unsigned int vterm_cap_maxsize(int fd) {
    unsigned int i, size, max = 0;

    for (i = 0; i < VTERM_CAPS_NB; ++i) {
        if ((size = s_vterm_caps[vterm_cap_index(fd, i)].size) > max)
            max = size;
    }
    return max;
}

/* ************************************************************************* */
int vterm_cap_check(int fd, vterm_cap_t cap,
                    const char * buffer, unsigned int buf_size) {
    unsigned int    idx = vterm_cap_index(fd, cap);
    int             ret;

    if ((ret = (buf_size == s_vterm_caps[idx].size
                && 0 == memcmp(buffer, s_vterm_caps[idx].str, buf_size)))) {
        return ret;
    }
    /* this is a bad hack, very very bad */
    if (cap >= VTERM_KEY_UP && cap <= VTERM_KEY_RIGHT) {
        if ((ret = (buf_size == s_vterm_caps[idx-VTERM_KEY_UP+VTERM_CAP_UP].size
                    && 0 == memcmp(buffer,
                                   s_vterm_caps[idx-VTERM_KEY_UP+VTERM_CAP_UP].str,
                                   buf_size)))) {
            return ret;
        }
        return (buf_size == s_vterm_caps_default[idx].size
                && 0 == memcmp(buffer, s_vterm_caps_default[idx].str, buf_size));
    } else if (cap >= VTERM_KEY_HOME && cap <= VTERM_KEY_PAGEDOWN) {
        return (buf_size == s_vterm_caps_default[idx].size
                && 0 == memcmp(buffer, s_vterm_caps_default[idx].str, buf_size));
    } else if (cap == VTERM_KEY_STOP || cap == VTERM_KEY_INT || cap == VTERM_KEY_EOF) {
        return (buf_size == s_vterm_caps_default[idx].size
                && 0 == memcmp(buffer, s_vterm_caps_default[idx].str, buf_size));
    } else {
        return 0;
    }
}

/* ************************************************************************* */
static inline int vterm_get_env_int(const char * env) {
    char *s = getenv(env);
    char *endp = NULL;
    long res;

    if (s == NULL)
        return VTERM_ERROR;

    errno = 0;
    res = strtol(s, &endp, 0);
    if (endp == NULL || *endp != 0 || errno != 0)
        return VTERM_ERROR;

    return res;
}

/* ************************************************************************* */
int vterm_get_winsize(int fd, unsigned int * p_lines, unsigned int * p_columns) {
    int ret, tcols = -1, tlines = -1;

    if ((ret = vterm_init(fd, s_vterm_info.flags)) != VTERM_OK) {
        return ret;
    }

#if defined(VTERM_IOCTL_GETWINSZ)
    VTERM_WINSIZE_T ws;
    if (s_vterm_info.fixedwinsz <= 0 && ioctl(fd, VTERM_IOCTL_GETWINSZ, &ws, sizeof(ws)) == 0) {
        tlines = VTERM_WINSZ_ROWS(&ws);
        tcols = VTERM_WINSZ_COLS(&ws);
        if (s_vterm_info.fixedwinsz < 0) {
            int tmp;
            s_vterm_info.fixedwinsz = 0;
            if (((tmp = vterm_get_env_int("LINES")) > 0 && tmp < tlines)) {
                s_vterm_info.fixedwinsz = 1;
                tlines = tmp;
            }
            if (((tmp = vterm_get_env_int("COLUMNS")) > 0 && tmp < tcols)) {
                s_vterm_info.fixedwinsz = 1;
                tcols = tmp;
            }
            if (s_vterm_info.fixedwinsz > 0) {
                LOG_WARN(g_vlib_log, "warning env COLUMNS or LINES overrides real term size");
            }
        }
    }
#endif

#if CONFIG_CURSES
    if (p_columns != NULL && tcols <= 0) {
        if (s_vterm_info.ti_col != NULL) {
            tcols = tigetnum(s_vterm_info.ti_col);
        }
    }
    if (p_lines != NULL && tlines <= 0) {
        tlines = tigetnum("lines");
    }
#endif

    if (p_lines != NULL) {
        if (tlines <= 0)
            tlines = vterm_get_env_int("LINES");
        if (tlines > 0)
            *p_lines = tlines;
        else
            return VTERM_ERROR;
    }
    if (p_columns != NULL) {
        if (tcols <= 0)
            tcols = vterm_get_env_int("COLUMNS");
        if (tcols > 0)
            *p_columns = tcols;
        else
            return VTERM_ERROR;
    }
    return VTERM_OK;
}

/* ************************************************************************* */
int vterm_get_lines(int fd) {
    int ret;
    unsigned int tlines;
    if ((ret = vterm_get_winsize(fd, &tlines, NULL)) == VTERM_OK)
        return tlines;
    if (ret == VTERM_NOTTY)
        return 0;
    return VTERM_ERROR;
}

/* ************************************************************************* */
int vterm_get_columns(int fd) {
    int ret;
    unsigned int tcols;
    if ((ret = vterm_get_winsize(fd, NULL, &tcols)) == VTERM_OK)
        return tcols;
    if (ret == VTERM_NOTTY)
        return 0;
    return VTERM_ERROR;
}

/* ************************************************************************* */
int vterm_clear_rect(FILE * out, int row, int col, int end_row, int end_col) {
    char *  buf;
    int     cols    = end_col - col + 1;

    if (end_row - row + 1 <= 0 || cols <= 0
    || (buf = malloc(cols + 3)) == NULL) {
        return VTERM_ERROR;
    }

    memset(buf, ' ', cols);
    buf[cols] = '\r';
    buf[cols+1] = '\n';
    buf[cols+2] = 0;
    flockfile(out);
    vterm_goto(out, row, col);
    for (int i = row; i <= end_row; ++i) {
        fwrite(buf, 1, cols + 2, out);
    }
    fflush(out);
    funlockfile(out);
    free(buf);
    return VTERM_OK;
}

/* ************************************************************************* */
static int vterm_clear_manual(FILE * out) {
    unsigned int cols = 0, rows = 0;
    int fd = out != NULL ? fileno(out) : -1;

    vterm_get_winsize(fd, &rows, &cols);
    return vterm_clear_rect(out, 0, 0, rows - 1, cols - 1);
}

/* ************************************************************************* */
static int vterm_readline_internal(
                FILE * in,
                FILE * out,
                char * buf,
                unsigned int inbuf_sz,
                unsigned int maxsize,
                vterm_readline_t * rl_data) {
    char            key[16];
    struct termios  tios_bak, tios;
    unsigned int    i;
    int             c = 0;
    int             fdin, fdout;
    (void) rl_data;

    if (buf == NULL || in == NULL || out == NULL)
        return VTERM_ERROR;
    if ((c = vterm_init(fileno(out), s_vterm_info.flags)) != VTERM_OK)
        return c;
    if (maxsize == 1)
        *buf = 0;
    if (maxsize <= 1)
        return 0;

    fdin = fileno(in);
    fdout = fileno(out);
    #if CONFIG_CURSES
    if (s_vterm_info.ti_cup == NULL)
    #endif
    {
        tcgetattr(fdout, &tios_bak);
        tios = tios_bak;
        cfmakeraw(&tios);
        tcsetattr(fdout, TCSANOW, &tios);
    }

    for (i = inbuf_sz; i < maxsize - 1; /* no incr */) {
        if (read(fdin, key, sizeof(key) / sizeof(*key)) != 1)
            continue ;
        c = *key;
        if (c == EOF || c == 27 || c == '\n' || c == '\r' || c == 0x03 || c == 0x04)
            break ;
        if (c == 0x7f || c == 8) { //TODO use terminfo
            if (i > 0) {
                --i;
                fputs("\b \b", out);
                fflush(out);
            }
            continue ;
        }
        if (!isprint(c) || i == maxsize - 2)
            continue ;
        fputc(c, out);
        fflush(out);
        buf[i] = c;
        ++i;
    }

    #if CONFIG_CURSES
    if (s_vterm_info.ti_cup == NULL)
    #endif
    {
        tcsetattr(fdout, TCSANOW, &tios_bak);
    }
    buf[i] = 0;
    return (c == 27 || c == EOF || c == 0x03) ? VTERM_ERROR : (int) i;
}

/* ************************************************************************* */
int vterm_readline(FILE * in, FILE * out, char * buf,
                   unsigned int maxsize, vterm_readline_t * rl_data) {
    return vterm_readline_internal(in, out, buf, 0, maxsize, rl_data);
}

/* ************************************************************************* */
size_t vterm_strlen(int fd, const char * str, size_t * size, size_t maxlen) {
    size_t len = 0, ntrunc = 0;

    if (str == NULL)
        return 0;
    vterm_init(fd, s_vterm_info.flags);

    for (size_t j = 0; (size == NULL || j < *size) && str[j] != 0; /* no incr */) {
        int i_found = -1;
        for (int i_col = 0; i_col < VCOLOR_EMPTY; ++i_col) {
            if (((i_found < 0 && s_vterm_colors[i_col].size > 0)
                  || s_vterm_colors[i_col].size > s_vterm_colors[i_found].size)
                &&  (size == NULL || j + s_vterm_colors[i_col].size <= *size)
                &&  strncmp(str + j, s_vterm_colors[i_col].str,
                            s_vterm_colors[i_col].size) == 0) {
                i_found = i_col;
            }
        }
        if (i_found >= 0) {
            if (ntrunc == j) {
                ntrunc += s_vterm_colors[i_found].size;
            }
            j += s_vterm_colors[i_found].size;
        } else {
            ++j;
            if (maxlen == 0 || len < maxlen) {
                ++len;
                ntrunc = j;
            }
        }
    }
    if (size != NULL)
        *size = ntrunc;
    return len;
}

/* ************************************************************************* */
int vterm_prompt(
            const char *        prompt,
            FILE *              in,
            FILE *              out,
            char *              buf,
            unsigned int        maxsize,
            int                 flags,
            vterm_readline_t *  rl_data) {
    int             res;
    unsigned int    len, prompt_len, default_len;
    int             fd;
    (void) rl_data;

    if (out == NULL || buf == NULL || in == NULL || maxsize == 0)
        return VTERM_ERROR;
    fd = fileno(out);
    if ((res = vterm_init(fd, s_vterm_info.flags)) != VTERM_OK)
        return res;
    if (prompt == NULL)
        prompt_len = 0;
    else {
        prompt_len = vterm_strlen(fd, prompt, NULL, 0);
        fputs(prompt, out);
    }
    if ((flags & VTERM_PROMPT_WITH_DEFAULT) != 0) {
        for (default_len = 0; buf[default_len] != 0
                              && default_len < maxsize - 1; ++default_len) {
            fputc(buf[default_len], out);
        }
        buf[default_len] = 0;
    } else {
        *buf = 0;
        default_len = 0;
    }
    if ((flags & VTERM_PROMPT_ERASE) != 0) {
        for (len = default_len; len < maxsize - 1; ++len)
            fputc(' ', out);
        for (len = default_len; len < maxsize - 1; ++len)
            fputc('\b', out);
    }
    fflush(out);

    res = len = vterm_readline_internal(in, out, buf, default_len, maxsize, NULL);

    if (res < 0)
        for (len = 0; buf[len] != 0; ++len)
            ; /* nothing but loop */
    if ((flags & VTERM_PROMPT_ERASE) != 0) {
        fputs(s_vterm_colors[VCOLOR_RESET].str, out);
        while (len++ < maxsize)
            fputc(' ', out);
        prompt_len += maxsize;
        while (prompt_len--)
            fputs("\b \b", out);
        fflush(out);
    }

    return res;
}

/* ************************************************************************* */
int vterm_printxy(FILE * out, int row, int col, const char * fmt, ...) {
    int         ret;
    va_list     valist;

    flockfile(out);
    if ((ret = vterm_goto(out, row, col)) < 0) {
        funlockfile(out);
        return ret;
    }

    va_start(valist, fmt);
    ret = vfprintf(out, fmt, valist);
    va_end(valist);

    funlockfile(out);
    return ret;
}

#if ! CONFIG_CURSES
/*******************************************************************
 * vterm implementation WITHOUT ncurses.
 ******************************************************************/

/* ************************************************************************* */
int vterm_init(int fd, unsigned int flags) {
    if (s_vterm_info.fd == VTERM_FD_BUSY || !isatty(fd)) { /* first thing to be done here */
        return VTERM_NOTTY;
    }
    if (s_vterm_info.fd != VTERM_FD_FREE)
        return VTERM_OK; /* already initialized */

    s_vterm_info.fd = VTERM_FD_BUSY;

    LOG_DEBUG(g_vlib_log, "vterm: initializing (fd=%d flags=%u)", fd, flags);

    s_vterm_info.has_colors = (flags & VTF_FORCE_COLORS) != 0
                              && (flags & VTF_NO_COLORS) == 0;

    vterm_init_default_colors(1);
    vterm_init_default_caps(1);

    s_vterm_info.flags = flags;
    s_vterm_info.fd = fd; /* must be last */

    LOG_VERBOSE(g_vlib_log, "vterm: initialized (fd:%d flags:%u has_colors:%d)",
                fd, flags, s_vterm_info.has_colors);

    return VTERM_OK;
}

/* ************************************************************************* */
int vterm_free() {
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...");

    if (s_vterm_info.fd < 0) {
        return VTERM_OK;
    }

    if (s_vterm_info.has_colors)
        vterm_free_colors();

    vterm_free_caps();

    LOG_VERBOSE(g_vlib_log, "%s(): done.", __func__);
    s_vterm_info.fd = VTERM_FD_FREE; /*must be last or LOG_* or other calls could redo vterm_init*/

    return VTERM_OK;
}

/* ************************************************************************* */
int             vterm_clear(FILE * out) {
    int ret, fd = out != NULL ? fileno(out) : -1;

    if ((ret = vterm_init(fd, s_vterm_info.flags)) == VTERM_NOTTY)
        return VTERM_NOTTY;
    if (ret != VTERM_OK) {
        return VTERM_ERROR;
    }

    flockfile(out);
    vterm_clear_manual(out);
    fflush(out);
    funlockfile(out);

    return VTERM_OK;
}

/* ************************************************************************* */
int vterm_goto(FILE *out, int r, int c) {
    int ret;
    int fd;

    if (out == NULL || (ret = vterm_init((fd = fileno(out)), s_vterm_info.flags)) == VTERM_NOTTY)
        return VTERM_ERROR;
    if (ret != VTERM_OK) {
        return VTERM_ERROR;
    }

    (void)r;
    (void)c;

    return VTERM_ERROR;
}

/* ************************************************************************* */
int vterm_goto_enable(int fd, int enable) {
    if (enable)
        return VTERM_ERROR;
    (void)fd;
    return VTERM_OK;
}

# elif ! defined(VLIB_CURSESDL) /* ! else ! CONFIG_CURSES  */
/***************************************************************************
 * vterm implementation WITH ncurses.
 **************************************************************************/

/* ************************************************************************* */
static void vterm_init_colors(int flags) {
    int             i;
    char *          capstr;
    char *          str;
    unsigned int    caplen;
    int             bcolor = VCOLOR_EMPTY, fcolor = VCOLOR_EMPTY;

    /* first set default colors */
    vterm_init_default_colors(0);

    //cap = set_a_foreground ? set_a_foreground : set_foreground;
    if ((capstr = tigetstr("setaf")) != NULL && capstr != (char*) -1)
        fcolor = VCOLOR_SETAF; /* index of s_vterm_colors[].libcolor */
    else if ((capstr = tigetstr("setf")) != NULL && capstr != (char*)-1)
        fcolor = VCOLOR_SETF; /* index of s_vterm_colors[].libcolor */
    else
        capstr = NULL;

    for (i = VCOLOR_FG; i < VCOLOR_BG; i++) {
        if (capstr != NULL
        &&  (str = tparm (capstr, s_vterm_colors[i].libcolor[fcolor])) != NULL) {
            caplen = strlen(str);
            s_vterm_colors[i].str = strdup(str);
            s_vterm_colors[i].size = caplen;
            s_vterm_colors[i].alloc = VCOLOR_ALLOC;
        } else if ((flags & VTF_FORCE_COLORS) == 0) {
            s_vterm_colors[i] = s_vterm_colors[VCOLOR_EMPTY];
        } else {
            s_vterm_colors[i].alloc = VCOLOR_STATIC;
        }
        LOG_DEBUG_BUF(g_vlib_log,
                   &(s_vterm_colors[i].str[0]), strlen(&s_vterm_colors[i].str[0]),
                   "vterm: colorfg[%02d] ", i);
    }

    //capstr = set_a_background ? set_a_background : set_background;
    if ((capstr = tigetstr("setab")) != NULL && capstr != (char*) -1)
        bcolor = VCOLOR_SETAF; /* index of s_vterm_colors[].libcolor */
    else if ((capstr = tigetstr("setb")) != NULL && capstr != (char*)-1)
        bcolor = VCOLOR_SETF; /* index of s_vterm_colors[].libcolor */
    else
        capstr = NULL;

    for (i = VCOLOR_BG; i < VCOLOR_STYLE; i++) {
        if (capstr != NULL
        &&  (str = tparm (capstr, s_vterm_colors[i].libcolor[bcolor])) != NULL) {
            caplen = strlen(str);
            s_vterm_colors[i].str = strdup(str);
            s_vterm_colors[i].size = caplen;
            s_vterm_colors[i].alloc = VCOLOR_ALLOC;
        } else if ((flags & VTF_FORCE_COLORS) == 0) {
            s_vterm_colors[i] = s_vterm_colors[VCOLOR_EMPTY];
        } else {
            s_vterm_colors[i].alloc = VCOLOR_STATIC;
        }
        LOG_DEBUG_BUF(g_vlib_log,
                   s_vterm_colors[i].str, strlen(s_vterm_colors[i].str),
                   "vterm: colorbg[%02d] ", i);
    }

    static struct cap_check_s { char * names[5]; int idx; } caps_check[] = {
        { { "sgr0", NULL, },            VCOLOR_NORMAL },
        { { "sgr0", NULL, },            VCOLOR_RESET },
        { { "blink", NULL, },           VCOLOR_BLINK },
        { { "bold", NULL, },            VCOLOR_BOLD },
        { { "sitm", NULL, },            VCOLOR_ITALIC },
        { { "smul", NULL, },            VCOLOR_UNDERLINED },
        { { "dim", "sshm", NULL, },     VCOLOR_DARK },
        { { "smso", NULL, },            VCOLOR_STANDOUT },
        { { NULL, },                    0 }
    };
    for (struct cap_check_s * cap = caps_check; cap->names[0] != NULL; ++cap) {
        for (char ** capname = cap->names; *capname != NULL; ++capname) {
            if ((capstr = tigetstr(*capname)) != NULL && capstr != (char*)-1) {
                caplen = strlen(capstr);
                s_vterm_colors[cap->idx].str = strdup(capstr);
                s_vterm_colors[cap->idx].size = caplen;
                s_vterm_colors[cap->idx].alloc = VCOLOR_ALLOC;
                break ;
            } else {
                LOG_DEBUG(g_vlib_log, "vterm: cap '%s' not found for vcolor index %d",
                          *capname, cap->idx);
                if ((flags & VTF_FORCE_COLORS) == 0)
                    s_vterm_colors[cap->idx] = s_vterm_colors[VCOLOR_EMPTY];
                else
                    s_vterm_colors[cap->idx].alloc = VCOLOR_STATIC;
            }
        }
    }

#   ifdef _DEBUG
    for (i = VCOLOR_STYLE; i < VCOLOR_RESERVED; i++) {
        LOG_BUFFER(LOG_LVL_DEBUG, g_vlib_log,
                       s_vterm_colors[i].str, strlen(s_vterm_colors[i].str),
                       "vterm: style[%02d] ", i);
    }
#   endif
}

/* ************************************************************************* */
static void vterm_init_caps(int flags) {
    int             i;
    char *          capstr;
    unsigned int    caplen;

    /* first set default caps */
    vterm_init_default_caps(0);

    for (i = VTERM_CAP_EMPTY+1; i < VTERM_CAPS_NB; ++i) {
        char ** cap;
        for (cap = s_vterm_caps[i].caps; *cap != NULL; ++cap) {
            if ((capstr = tigetstr(*cap)) != NULL && capstr != (char*)-1) {
                caplen = strlen(capstr);
                s_vterm_caps[i].str = strdup(capstr);
                s_vterm_caps[i].size = caplen;
                s_vterm_caps[i].alloc = VCOLOR_ALLOC;
                break ;
            }
        }
        if (*cap == NULL) {
            if ((flags & VTF_FORCE_COLORS) == 0) {
                s_vterm_caps[i] = s_vterm_caps[VTERM_CAP_EMPTY];
                s_vterm_caps[i].id = i;
            } else {
                s_vterm_caps[i].alloc = VCOLOR_STATIC;
            }
        }
        LOG_DEBUG_BUF(g_vlib_log,
                   &(s_vterm_caps[i].str[0]), strlen(&s_vterm_caps[i].str[0]),
                   "vterm: cap[%02d] ", i);
    }
}

/* ************************************************************************* */
int vterm_free() {
    LOG_DEBUG(g_vlib_log, "vterm: cleaning...");

    if (s_vterm_info.fd < 0) {
        return VTERM_OK;
    }

    vterm_goto_enable(s_vterm_info.fd, 0);
    if (s_vterm_info.ti_cup != NULL)
        free(s_vterm_info.ti_cup);
    s_vterm_info.ti_cup = NULL;
    if (s_vterm_info.ti_col != NULL)
        free(s_vterm_info.ti_col);
    s_vterm_info.ti_col = NULL;
    if (s_vterm_info.ti_clr != NULL)
        free(s_vterm_info.ti_clr);
    s_vterm_info.ti_clr = NULL;
    if (s_vterm_info.has_colors)
        vterm_free_colors();
    vterm_free_caps();
#   if CONFIG_CURSES_SCR
    if ((s_vterm_info.flags & VTF_INITSCR) != 0) { //TODO
        if (s_vterm_info.screen != NULL) {
            delscreen(s_vterm_info.screen);
            s_vterm_info.screen = NULL;
        }
        endwin();
    } else
#   endif
    {
        if(cur_term != NULL) {
            del_curterm(cur_term);
            cur_term=NULL;
        }
    }

    LOG_VERBOSE(g_vlib_log, "%s(): done.", __func__);
    s_vterm_info.fd = VTERM_FD_FREE;/* must be last or vterm could be reinit by log */

    return VTERM_OK;
}

/* ************************************************************************* */
int vterm_init(int fd, unsigned int flags) {
    /* checking tty and BUSY must be first thing to be done here */
    if (s_vterm_info.fd == VTERM_FD_BUSY || (
#if CONFIG_CURSES
        (s_vterm_info.ti_cup == NULL || s_vterm_info.fd != fd) &&
#endif
         !isatty(fd))) {
        return VTERM_NOTTY;
    }
    if (s_vterm_info.fd != VTERM_FD_FREE) {
        return VTERM_OK;
    }
    s_vterm_info.fd = VTERM_FD_BUSY;

    /* IGNORING experimental VTF_INITSCR TODO */
    if ((flags & VTF_INITSCR) != 0) {
        LOG_INFO(g_vlib_log, "%s(): disabling expemental flag VTF_INITSCR", __func__);
        flags &= ~VTF_INITSCR;
    }

    int             ret;
    char *          ti_col;
    char *          setf;

    LOG_DEBUG(g_vlib_log, "vterm: initializing (fd=%d flags=%u)", fd, flags);

    #if CONFIG_CURSES_SCR
    struct termios  term_conf, term_conf_bak;
    if ((flags & VTF_INITSCR) != 0) {
        //filter();
        /*s_vterm_info.screen = newterm(getenv("TERM"), fd == 1?stdout:stderr, stdin);*/
        //tcgetattr(STDOUT_FILENO, &term_conf_bak);
        if (initscr() == NULL) {
            LOG_ERROR(g_vlib_log, "initscr(): unknown error.");
            s_vterm_info.fd = VTERM_FD_FREE;
            return VTERM_ERROR;
        }
        /* temporarily setup terminal for logging */
        tcgetattr(fd, &term_conf_bak);
        memcpy(&term_conf, &term_conf_bak, sizeof(term_conf));
        term_conf.c_oflag |= ONLRET | ONLCR | ECHO | ECHOE;
        tcsetattr(fd, TCSANOW, &term_conf);
        if (fd == STDOUT_FILENO)
            tcsetattr(STDERR_FILENO, TCSANOW, &term_conf);
        else if (fd == STDERR_FILENO)
            tcsetattr(STDOUT_FILENO, TCSANOW, &term_conf);
    } else
    #endif
    {
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
    }

    /* init vterm caps */
    vterm_init_caps(flags);

    if ((ti_col = tigetstr("clear")) != NULL && ti_col != (char *) -1) {
        s_vterm_info.ti_clr = strdup(ti_col);
    } else {
        s_vterm_info.ti_clr = NULL;
    }

    if ((ret = tigetnum(ti_col = "cols")) <= 0
    &&         (ret = tigetnum(ti_col = "columns")) <= 0
    &&         (ret = tigetnum(ti_col = "COLUMNS")) <= 0) {
        LOG_ERROR(g_vlib_log, "tigetnum(cols,columns,COLUMNS): cannot get value");
        ti_col = NULL;
    }


    if ((flags & VTF_NO_COLORS) != 0) {
        s_vterm_info.has_colors = 0;
    } else {
        ret = tigetnum("colors");
        if (((setf = tigetstr("setaf")) == NULL || setf == (char*) -1)
        &&  (setf = tigetstr("setf")) == (char*)-1)
            setf = NULL;
        LOG_DEBUG(g_vlib_log, "vterm: setf:%lx max_colors:%d", (unsigned long) setf, ret);

        //if ((s_vterm_info.has_colors = has_colors()) == TRUE) { /*not always ok wihtout initscr()*/
        if ((s_vterm_info.has_colors = (setf != NULL && ret > 0)) != 0) {
            /*nothing*/; //start_color();
        } else if ((flags & VTF_FORCE_COLORS) != 0) {
            s_vterm_info.has_colors = 1;
        }
    }
    if (s_vterm_info.has_colors) {
        vterm_init_colors(flags);
    } else {
        vterm_init_default_colors(1);
    }

    #if CONFIG_CURSES_SCR
    if ((flags & VTF_INITSCR) != 0) {
        tcsetattr(fd, TCSANOW, &term_conf_bak);
        if (fd == STDOUT_FILENO)
            tcsetattr(STDERR_FILENO, TCSANOW, &term_conf_bak);
        else if (fd == STDERR_FILENO)
            tcsetattr(STDOUT_FILENO, TCSANOW, &term_conf_bak);
    }
    #endif

    s_vterm_info.ti_col = ti_col != NULL ? strdup(ti_col) : NULL;
    s_vterm_info.flags = flags;
    s_vterm_info.fd = fd; /* last thing to be done here, but LOG_* can be done after this */

    #if CONFIG_CURSES_SCR
    if ((flags & VTF_INITSCR) != 0) {
        /* don't log after initscr */
    } else
    #endif
    {
        LOG_VERBOSE(g_vlib_log, "vterm: initialized (fd:%d flags:%u has_colors:%d)",
                    fd, flags, s_vterm_info.has_colors);
    }
    return VTERM_OK;
}

/* ************************************************************************* */
static ssize_t vterm_fdwrite(int fd, const void * buf, size_t n) {
    ssize_t ret;
    while ((ret = write(fd, buf, n)) < 0 && errno == EINTR)
        ; /*nothing but loop*/
    if (fd == STDERR_FILENO)
        while (write(STDOUT_FILENO, buf, n) < 0 && errno == EINTR)
            ;/*nothing but loop*/
    else if (fd == STDOUT_FILENO)
        while (write(STDERR_FILENO, buf, n) < 0 && errno == EINTR);
    return ret;
}

/* ************************************************************************* */
static int vterm_fdflush(int fd) {
    int ret;

    ret = fsync(fd);
    if (fd == STDERR_FILENO) {
        fsync(STDOUT_FILENO);
        fflush(stdout);
        fflush(stderr);
    } else if (fd == STDOUT_FILENO) {
        fsync(STDERR_FILENO);
        fflush(stdout);
        fflush(stderr);
    }
    fsync(STDIN_FILENO);
    fflush(stdin);

    return ret;
}

/* ************************************************************************* */
int             vterm_clear(FILE * out) {
    int ret, fd = out != NULL ? fileno(out) : -1;

    if ((ret = vterm_init(fd, s_vterm_info.flags)) == VTERM_NOTTY)
        return VTERM_NOTTY;
    if (ret != VTERM_OK) {
        return VTERM_ERROR;
    }
    if (s_vterm_info.ti_clr != NULL) {
        fputs(s_vterm_info.ti_clr, out);
    } else {
        vterm_clear_manual(out);
    }
    fflush(out);

    return VTERM_OK;
}

/* ************************************************************************* */
int vterm_goto_enable(int fd, int enable) {
    char * cap, * cupcap;
    size_t caplen;
    int ret;

    if ((ret = vterm_init(fd, s_vterm_info.flags)) == VTERM_NOTTY)
        return VTERM_NOTTY;
    if (ret != VTERM_OK) {
        return VTERM_ERROR;
    }

    if (enable) {
        struct termios  termio;

        if (s_vterm_info.ti_cup != NULL) { /* goto already enabled */
            return VTERM_OK;
        }
        if ((cap = tigetstr("rmcup")) == NULL || cap == (char*) -1) {
            LOG_WARN(g_vlib_log, "%s(): terminfo cap rmcup unavailable (%d)",
                      __func__, (int)((unsigned long)cap));
        }
        if ((cap = tigetstr("smcup")) == NULL || cap == (char*) -1) {
            LOG_WARN(g_vlib_log, "%s(): terminfo cap smcup unavailable (%d)",
                      __func__, (int)((unsigned long)cap));
        }
        if ((cupcap = tigetstr("cup")) == NULL || cupcap == (char*) -1
        ||  (cupcap = strdup(cupcap)) == NULL) {
            LOG_ERROR(g_vlib_log, "%s(): terminfo cap cup unavailable (%d)",
                      __func__, (int)((unsigned long)cupcap));
            return VTERM_ERROR;
        }
        s_vterm_info.fd = fd; /* goto will be enabled, force the fd to avoid isatty calls */
        tcgetattr(fd, &s_vterm_info.termio_bak);
        memcpy(&termio, &s_vterm_info.termio_bak, sizeof(termio));
        cfmakeraw(&termio);
        tcsetattr(fd, TCSANOW, &termio);

        if (cap != NULL && cap != (char *) -1) {
            /* write smcup to terminal */
            caplen = strlen(cap);
            vterm_fdwrite(fd, cap, caplen);
        } else {
            /* display 'LINES' \r\n to not lose what was displayed before */
            for (int i = 0; i < vterm_get_lines(fd); ++i)
                while (write(fd, "\r\n", 2) < 0 && errno == EINTR) ; /*loop */
            vterm_fdflush(fd);
            /* send terminfo clear if available */
            if ((cap = tigetstr("clear")) != NULL && cap != (char*) -1) {
                caplen = strlen(cap);
                while (write(fd, cap, caplen) < 0 && errno == EINTR) ;/* loop */
            } else {
                /* not necessary TODO to be removed
                for (int i = 0; i < vterm_get_lines(fd); ++i) {
                    for (int j = 0; j < vterm_get_columns(fd); ++j)
                        while (write(fd, " ", 1) < 0 && errno == EINTR) ;/ * loop * /
                    while (write(fd, "\r\n", 2) < 0 && errno == EINTR) ; / *loop * /
                }*/
            }
        }
        /* enter keyboard-transmit mode */
        if (s_vterm_caps[VTERM_CAP_KB_KEYCODE].size > 0) {
            while (write(fd, s_vterm_caps[VTERM_CAP_KB_KEYCODE].str,
                             s_vterm_caps[VTERM_CAP_KB_KEYCODE].size) < 0 && errno == EINTR)
                ; /* loop */
        }
        /* end set goto ok */
        s_vterm_info.ti_cup = cupcap;
    } else {
        if (s_vterm_info.ti_cup == NULL)
            return VTERM_OK; /* goto already OFF */

        vterm_fdflush(fd);
        cupcap = tparm(s_vterm_info.ti_cup, vterm_get_lines(fd) - 1,
                                            vterm_get_columns(fd) - 1);
        if (cupcap) {
            while (write(fd, cupcap, strlen(cupcap)) < 0 && errno == EINTR) ;/* loop */
        }

        if (s_vterm_caps[VTERM_CAP_NO_KEYCODE].size > 0) {
            while (write(fd, s_vterm_caps[VTERM_CAP_NO_KEYCODE].str,
                             s_vterm_caps[VTERM_CAP_NO_KEYCODE].size) < 0 && errno == EINTR)
                ; /* loop */
        }

        if ((cap = tigetstr("rmcup")) != NULL && cap != (char*) -1) {
            caplen = strlen(cap);
            vterm_fdwrite(fd, cap, caplen);
        } else {
            while (write(fd, "\r\n", 2) < 0 && errno == EINTR) ;/* loop */
        }

        tcsetattr(fd, TCSANOW, &s_vterm_info.termio_bak);
        if (s_vterm_info.ti_cup != NULL)
            free(s_vterm_info.ti_cup);
        s_vterm_info.ti_cup = NULL;
    }
    vterm_fdflush(fd);

    return VTERM_OK;
}

/* ************************************************************************* */
int vterm_goto(FILE * out, int r, int c) {
    int ret;
    int fd;

    if (out == NULL)
        return VTERM_ERROR;
    fd = fileno(out);
    if ((fd != s_vterm_info.fd || s_vterm_info.ti_cup == NULL) &&!isatty(fd))
        return VTERM_NOTTY;
    if (s_vterm_info.ti_cup == NULL)
        return VTERM_ERROR;

    ret = fprintf(out, "%s%s",
                  tparm(s_vterm_info.ti_cup, r, c),
                  vterm_color(fd, VCOLOR_RESET));

    return ret;
}

#endif /* ! CONFIG_CURSES */

/*************************************************************
 * vterm_screen_loop() implementation with or without curses
 ************************************************************/

/** global running state used by signal handler */
#define VTERM_SIGNAL_NONE (INT_MAX)
static volatile sig_atomic_t s_vterm_last_signal = VTERM_SIGNAL_NONE;
/** signal handler */
static void vterm_sig_handler(int sig) {
    s_vterm_last_signal = sig;
}

#ifndef FD_COPY
#define FD_COPY(pset_in, pset_out) (memcpy(pset_out, pset_in, sizeof(fd_set)))
#endif

/* ************************************************************************* */
static inline void vterm_screen_handle_cb_result(
                        unsigned int result, vterm_screen_ev_data_t * evdata,
                        unsigned int * timer_ms, struct itimerval *timer) {
    if ((result & VTERM_SCREEN_CB_NEWTIMER) != 0) {
        unsigned int        new_timer_ms = evdata->newtimer_ms;
        struct itimerval    newtimer = { .it_value = { .tv_sec = 0, .tv_usec= 1 } };

        newtimer.it_interval = (struct timeval)
        { .tv_sec = new_timer_ms / 1000, .tv_usec = (new_timer_ms % 1000) * 1000 };

        if (setitimer(ITIMER_REAL, &newtimer, NULL) != 0) {
            LOG_ERROR(g_vlib_log, "%s(): setitimer(): %s", __func__, strerror(errno));
        } else {
            LOG_VERBOSE(g_vlib_log, "%s(): timer updated to %u ms", __func__, new_timer_ms);
            *timer = newtimer;
            *timer_ms = new_timer_ms;
        }
    }
}
/* ************************************************************************* */
int vterm_screen_loop(
        FILE *                  out,
        unsigned int            timer_ms,
        fd_set *                fduserset_in,
        vterm_screen_callback_t display_callback,
        void *                  callback_data
) {
    const unsigned int  buffer_size = 32;
    vterm_screen_ev_data_t * evdata = NULL;
    char *              buffer = NULL;
    struct timeval *    elapsed = NULL;
    fd_set              fdset_in;
    sigset_t            select_sigset, sigset_bak;
    struct itimerval    timer_bak, timer = { .it_value = { .tv_sec = 0, .tv_usec= 1 } };
    struct sigaction    sa;
    int                 ret = VTERM_ERROR;
    int                 got_sig_tstp = 0;
    unsigned int        i, cb_ret;
    int                 fd;
    int                 goto_enabled = 0, userinit_done = 0;
    struct { int sig; struct sigaction sa_bak; int done; } sigs[] = {
        { .sig = SIGALRM, }, { .sig = SIGINT, }, { .sig = SIGCONT, }, { .sig = SIGTSTP, }
    };

    /* sanity checks */
    if (out == NULL || display_callback == NULL) {
        LOG_ERROR(g_vlib_log, "%s(): wrong inputs file:%lx, callback:%lx", __func__,
                  (unsigned long) out, (unsigned long) display_callback);
        return VTERM_ERROR;
    }

    /* check if terminal is initialized */
    fd = fileno(out);
    if (vterm_init(fd, VTF_DEFAULT) != VTERM_OK) {
        LOG_ERROR(g_vlib_log, "%s(): cannot initialize terminal", __func__);
        return VTERM_ERROR;
    }

    /* alloc vterm_screen_ev_data_t */
    if ((evdata = malloc(sizeof(*evdata))) == NULL
    ||  (elapsed = malloc(sizeof(*elapsed))) == NULL
    ||  (buffer = malloc(sizeof(*buffer) * buffer_size)) == NULL) {
        LOG_ERROR(g_vlib_log, "%s(): malloc(evdata/timval/buf): %s", __func__, strerror(errno));
        if (evdata != NULL)
            free(evdata);
        if (elapsed != NULL)
            free(elapsed);
        return VTERM_ERROR;
    }

    /* block signals handled by pselect */
    sigemptyset(&select_sigset);
    for (i = 0; i < PTR_COUNT(sigs); ++i) {
        sigaddset(&select_sigset, sigs[i].sig);
        sigs[i].done = 0;
    }
    if (sigprocmask(SIG_BLOCK, &select_sigset, &sigset_bak) < 0) {
        LOG_ERROR(g_vlib_log, "%s(): sigprocmask(): %s", __func__, strerror(errno));
        free(evdata);
        free(elapsed);
        free(buffer);
        return VTERM_ERROR;
    }

    /* setup timer interval */
    timer.it_interval = (struct timeval)
                { .tv_sec = timer_ms / 1000, .tv_usec = (timer_ms % 1000) * 1000 };

    /* set start time 0 for sensor_update_get */
    memset(elapsed, 0, sizeof(*elapsed));

    /* init backups to know what to restore */
    timer_bak.it_value.tv_sec = INT_MAX;

    do {
        /* prepare pselect signal mask (list of ignored signals) */
        sigemptyset(&select_sigset);

        /* Setup signals */
        s_vterm_last_signal = VTERM_SIGNAL_NONE;
        sa.sa_flags = SA_RESTART;
        sa.sa_handler = vterm_sig_handler;
        sigfillset(&sa.sa_mask);
        for (i = 0; i < PTR_COUNT(sigs); ++i) {
            if (sigaction(sigs[i].sig, &sa, &(sigs[i].sa_bak)) < 0) {
                LOG_ERROR(g_vlib_log, "%s(): sigaction(%d): %s", __func__,
                          sigs[i].sig, strerror(errno));
                break ;
            }
            sigs[i].done = 1;
        }
        if (i < PTR_COUNT(sigs))
            break ;

        /* Setup timer */
        if (setitimer(ITIMER_REAL, &timer, &timer_bak) < 0) {
            LOG_ERROR(g_vlib_log, "%s(): setitimer(): %s", __func__, strerror(errno));
            break ;
        }

        LOG_INFO(g_vlib_log, "%s(): initializing screen...", __func__);

        /* prepare init */
        if (((cb_ret = display_callback(VTERM_SCREEN_INIT, out, elapsed, evdata,
                                callback_data)) & VTERM_SCREEN_CB_EXIT) != 0) {
            break ;
        }
        vterm_screen_handle_cb_result(cb_ret, evdata, &timer_ms, &timer);
        userinit_done = 1;

        if (vterm_goto_enable(fd, 1) != VTERM_OK) {
            LOG_ERROR(g_vlib_log, "%s(): vterm_goto_enable() failed", __func__);
            break ;
        }

        goto_enabled = 1;
        ret = VTERM_OK;

        /* print header */
        if (((cb_ret = display_callback(VTERM_SCREEN_START, out, elapsed, evdata,
                                callback_data)) & VTERM_SCREEN_CB_EXIT) != 0) {
            break ;
        }
        vterm_screen_handle_cb_result(cb_ret, evdata, &timer_ms, &timer);

        while (1) {

            if (((cb_ret = display_callback(VTERM_SCREEN_LOOP, out, elapsed, evdata,
                                    callback_data)) & VTERM_SCREEN_CB_EXIT) != 0) {
                ret = VTERM_OK;
                break ;
            }
            vterm_screen_handle_cb_result(cb_ret, evdata, &timer_ms, &timer);

            /* Wait for timeout or key pressed */
            if (fduserset_in != NULL)
                FD_COPY(fduserset_in, &fdset_in);
            else
                FD_ZERO(&fdset_in);
            FD_SET(STDIN_FILENO, &fdset_in);

            if ((ret = pselect(STDIN_FILENO+1, &fdset_in, NULL, NULL, NULL, &select_sigset)) > 0) {
                /* Check Key Pressed */
                FD_COPY(&fdset_in, &(evdata->input.fdset_in));
                evdata->input.key = VTERM_KEY_EMPTY;
                if (((cb_ret = display_callback(VTERM_SCREEN_INPUT, out, elapsed, evdata,
                                        callback_data)) & VTERM_SCREEN_CB_EXIT) != 0) {
                    ret = VTERM_OK;
                    break ;
                }
            } else if (ret < 0) {
                if (errno == EINTR) {
                    /* interrupted by signal */
                    sig_atomic_t last_signal = s_vterm_last_signal;
                    s_vterm_last_signal = VTERM_SIGNAL_NONE;
                    if (last_signal == SIGINT || last_signal == SIGTERM) {
                        /* this is sigint */
                        ret = VTERM_OK;
                        break ;
                    } else if (last_signal == SIGTSTP) {
                        sigset_t sigset, sigset_bak;
                        flockfile(out);
                        vterm_goto(out, vterm_get_lines(fd) - 1, 0);
                        fputs("\r\n\r\n", out);
                        got_sig_tstp = 1;
                        vterm_goto_enable(fd, 0);
                        funlockfile(out);
                        sigemptyset(&sigset);
                        sigaddset(&sigset, SIGCONT);
                        sigprocmask(SIG_SETMASK, &sigset, &sigset_bak);
                        kill(0, SIGSTOP);
                        sigsuspend(&sigset);
                        sigprocmask(SIG_SETMASK, &sigset_bak, NULL);
                        continue ;
                    } else if (last_signal == SIGCONT) {
                        if (got_sig_tstp == 0 && s_vterm_info.ti_cup != NULL) {
                            /* maybe there is a good method to know if we got SIGSTOP... */
                            struct termios termio;
                            if (tcgetattr(fd, &termio) == 0) {
                                struct termios termioraw;
                                memcpy(&termioraw, &termio, sizeof(termio));
                                cfmakeraw(&termioraw);
                                if (memcmp(&termioraw, &termio, sizeof(termio)) == 0) {
                                    LOG_VERBOSE(g_vlib_log, "%s() SIGCONT ignored", __func__);
                                    continue ;
                                }
                            }
                        }
                        got_sig_tstp = 0;
                        flockfile(out);
                        vterm_goto_enable(fd, 0);
                        vterm_goto_enable(fd, 1);
                        funlockfile(out);
                        LOG_INFO(g_vlib_log, "%s(): CONTINUE after STOP", __func__);
                        /* print header */
                        if (((cb_ret = display_callback(VTERM_SCREEN_START, out, elapsed, evdata,
                                callback_data)) & VTERM_SCREEN_CB_EXIT) != 0) {
                            ret = VTERM_OK;
                            break ;
                        }
                        vterm_screen_handle_cb_result(cb_ret, evdata, &timer_ms, &timer);
                        continue ;
                    } else if (last_signal != SIGALRM) {
                        continue ;
                    }
                    /* this is SIGALARM */
                    elapsed->tv_usec += timer_ms * 1000;
                    if (elapsed->tv_usec >= 1000000) {
                        elapsed->tv_sec += (elapsed->tv_usec / 1000000);
                        elapsed->tv_usec %= 1000000;
                    }

                    if (((cb_ret = display_callback(VTERM_SCREEN_TIMER, out, elapsed, evdata,
                                        callback_data)) & VTERM_SCREEN_CB_EXIT) != 0) {
                        ret = VTERM_OK;
                        break ;
                    }
                } else {
                    /* select error */
                    fprintf(out, "SELECT ERROR");
                    ret = VTERM_ERROR;
                    break ;
                }
           } else {
                /* Should not be reached. */
                fprintf(out, "SELECT ERROR");
                ret = VTERM_ERROR;
                break ;
            }
            vterm_screen_handle_cb_result(cb_ret, evdata, &timer_ms, &timer);
        }
        /* callback informing termination */
        display_callback(VTERM_SCREEN_END, out, elapsed, evdata, callback_data);

    } while (0);

    /* uninstall timer */
    if (timer_bak.it_value.tv_sec != INT_MAX && setitimer(ITIMER_REAL, &timer_bak, NULL) == 0) {
        timer_bak.it_value.tv_sec = INT_MAX;
    }
    /* get pending Timer signals */
    sigemptyset(&select_sigset);
    while (sigpending(&select_sigset) == 0 && sigismember(&select_sigset, SIGALRM)) {
        sigemptyset(&select_sigset);
        sigsuspend(&select_sigset);
    }
    /* restore screen */
    if (goto_enabled) {
        vterm_goto_enable(fd, 0);
    }
    /* callback for end of goto mode */
    if (userinit_done) {
        display_callback(VTERM_SCREEN_EXIT, out, elapsed, evdata, callback_data);
    }

    if (timer_bak.it_value.tv_sec != INT_MAX) {
        LOG_ERROR(g_vlib_log, "%s(): restore setitimer(): %s", __func__, strerror(errno));
    }
    /* restore signal mask */
    if (sigprocmask(SIG_SETMASK, &sigset_bak, NULL) < 0) {
        LOG_ERROR(g_vlib_log, "%s(): restore signal masks: %s", __func__, strerror(errno));
    }
    /* uninstall signals */
    for (i = 0; i < PTR_COUNT(sigs); ++i) {
        if (sigs[i].done
        && sigaction(sigs[i].sig, &(sigs[i].sa_bak), NULL) < 0) {
            LOG_ERROR(g_vlib_log, "%s(): restore signal(%d): %s", __func__,
                      sigs[i].sig, strerror(errno));
        }
    }
    /* free data */
    free(evdata);
    free(elapsed);
    free(buffer);

    LOG_INFO(g_vlib_log, "%s(): exiting with status %d.", __func__, ret);

    return ret;
}

/* ******************************************
 * DEPRECATED AND OUTDATED NOT BUILDING CODE
 * *******************************************/
#if defined(VLIB_CURSESDL) && CONFIG_CURSES

/* ************************************************************************* */
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

/* ************************************************************************* */
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

/* ************************************************************************* */
int vterm_init(int fd, unsigned int flags) {
    if (s_vterm_info.fd == VTERM_FD_BUSY || !isatty(fd)) {
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

# endif /* VLIB_CURSESDL && CONFIG_CURSES */

