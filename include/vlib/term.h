/*
 * Copyright (C) 2020 Vincent Sallaberry
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
 * vlib terminal utilities.
 */
#ifndef VLIB_TERM_H
#define VLIB_TERM_H

#ifdef __cplusplus
# include <cstring>
# include <cstdio>
# include <climits>
# include <cstdint>
#else
# include <string.h>
# include <stdio.h>
# include <limits.h>
# include <stdint.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** flags for vterm_init() */
typedef enum {
    VTF_NONE            = 0,
    VTF_FORCE_COLORS    = 1 << 0,
    VTF_NO_COLORS       = 1 << 1,
    VTF_INITSCR         = 1 << 2, /* dedicate term screen to application, with cursor move */
    VTF_NB, /* must be last (except for VTF_DEFAULT) */
    VTF_DEFAULT = VTF_NONE
} vterm_flag_t;

/** colors for vterm_color() */
typedef enum {
    /* Foreground */
    VCOLOR_FG = 0,
    VCOLOR_BLACK = VCOLOR_FG,
    VCOLOR_RED,
    VCOLOR_GREEN,
    VCOLOR_YELLOW,
    VCOLOR_BLUE,
    VCOLOR_MAGENTA,
    VCOLOR_CYAN,
    VCOLOR_WHITE,
    /* Background */
    VCOLOR_BG,
    VCOLOR_BG_BLACK = VCOLOR_BG,
    VCOLOR_BG_RED,
    VCOLOR_BG_GREEN,
    VCOLOR_BG_YELLOW,
    VCOLOR_BG_BLUE,
    VCOLOR_BG_MAGENTA,
    VCOLOR_BG_CYAN,
    VCOLOR_BG_WHITE,
    /* Styles */
    VCOLOR_STYLE,
    VCOLOR_NORMAL = VCOLOR_STYLE,
    VCOLOR_BOLD,
    VCOLOR_DARK,
    VCOLOR_ITALIC,
    VCOLOR_UNDERLINED,
    VCOLOR_BLINK,
    VCOLOR_STANDOUT,
    /* Reserved */
    VCOLOR_RESERVED,
    VCOLOR_RESET = VCOLOR_RESERVED,
    VCOLOR_EMPTY
} vterm_color_t;

#if INT_MAX < ((1 << 31) - 1)
# error "sizeof(int) < 4 "
#endif

/** vterm_colorset_t, a bit combination of vterm_color_t */
typedef unsigned int    vterm_colorset_t;

#define         VCOLOR_BUILD(fore, back, style) \
                    ((fore) | ((back) << VCOLOR_BG) | ((style) << VCOLOR_STYLE))

#define         VCOLOR_GET_FORE(colors) \
                    ((colors) & ((1 << VCOLOR_BG) - 1))
#define         VCOLOR_GET_BACK(colors) \
                    (((colors) & ((1 << VCOLOR_STYLE) - 1)) >> VCOLOR_BG)
#define         VCOLOR_GET_STYLE(colors) \
                    (((colors) & ((1 << VCOLOR_RESERVED) - 1)) >> VCOLOR_STYLE)

/* return values for vterm_*() functions */
#define         VTERM_OK        (0)
#define         VTERM_NOTTY     (-2)
#define         VTERM_ERROR     (-1)

/** init the terminal attached to <fd>.
 * @param fd the fd connected to target terminal
 * @param flags bit combination of log_flag_t
 * @return VTERM_OK on successfull initialization
 *         VTERM_OK if terminal already initialized
 *         VTERM_NOTTY if terminal is not a TTY
 *         VTERM_ERROR on error
 * vterm_free() or vterm_enable(0) must be called when this routine
 *   returns success.
 */
int             vterm_init(int fd, unsigned int flags);

/** Enable terminal or disable and free terminal resources.
 * @param enable,
 *  * if 0, the terminal is freed (vterm_free), and further
 *    reinitializations are blocked.
 *  * if not 0, the terminal is unblocked and further vterm_init()
 *    will succeed.
 * @return VTERM_OK on success, VTERM_ERROR on error. */
int             vterm_enable(int enable);

/** clean the terminal allocated resources.
 * @return VTERM_OK on success, VTERM_ERROR on error.
 * @WARNING: LOG_* or other calls could reinitialize terminal implicitly,
 *           call vterm_enable(0) instead to avoid implicit initialization. */
int             vterm_free();

/** get number of columns of terminal attached to <fd>.
 * @return columns
 *         or 0 if fd is not a terminal
 *         or VTERM_ERROR on error
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_get_columns(int fd);

/** get number of lines of terminal attached to <fd>.
 * @return lines
 *         or 0 if fd is not a terminal
 *         or VTERM_ERROR on error
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_get_lines(int fd);

/** get color capability of terminal attached to <fd>.
 * @return 1 if terminal with color capability.
 *         or 0 if terminal does not support colors
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_has_colors(int fd);

/** get color string for terminal attached to <fd>.
 * vterm_color is reentrant and can be used several times in *printf().
 * @param color a simple color for foreground or background or style
 * @return the color string, or empty ("") on error.
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
const char *    vterm_color(int fd, vterm_color_t color);

/** setup the given color combination (fore, back, style) on the terminal
 * @param file the FILE* attached to the terminal
 * @param colors the result of VCOLOR_BUILD(fore, back, style)
 * @return amount of written characters or 0 on error.
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
ssize_t         vterm_putcolor(FILE *out, vterm_colorset_t colors);

/** setup the given color combination (fore, back, style) on the terminal
 * reentrancy of vtermbuild_color depend on reentrancy of buffer and psize.
 * @param fd the file descriptor of the terminal
 * @param colors the result of VCOLOR_BUILD(fore, back, style)
 * @param buffer the buffer storing the color string of max size *psize
 * @param psize the maxsize of buffer and the output amount of written characters, or NULL
 * @return input buffer containing color string or empty string on error.
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
char *          vterm_buildcolor(int fd, vterm_colorset_t colors, char * buffer, size_t * psize);

/**
 * @return the size of given color string, or 0 if not found. */
unsigned int    vterm_color_size(int fd, vterm_color_t color);

/**
 * @return the maximum size of a color string on given terminal. */
unsigned int    vterm_color_maxsize(int fd);

/** enable goto mode, clear the screen
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed.
 * @return VTERM_SUCCESS if success or already enabled,
 *         VTERM_ERROR on error
 *         VTERM_NOTTY if terminal is not a tty */
int             vterm_goto_enable(int fd, int enable);

/** put the cursor at given position
 * @return positive or null value on success
 *         VTERM_NOTTY if terminal is not a tty
 *         VTERM_ERROR on error or if vterm_goto_enable() has not been called before
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_goto(FILE *out, int r, int c);

/** Events values for vterm_screen_loop() */
typedef enum {
    VTERM_SCREEN_START = 0, /* after vterm_goto_enable(), before loop */
    VTERM_SCREEN_LOOP,      /* at each loop or event */
    VTERM_SCREEN_TIMER,     /* when the timer (timer_ms) expires */
    VTERM_SCREEN_INPUT,     /* when STDIN_FILENO or file in fduserset_in is
                               ready for read. Callback uses FD_ISSET(fd, fdset_in)*/
    VTERM_SCREEN_END        /* when loop stops, before vterm_goto_enable(0)
                               Callback can return this at any moment to end the loop*/
} vterm_screen_event_t;

/** callback for vterm_screen_loop() */
typedef int     (*vterm_screen_callback_t)(
                    vterm_screen_event_t    event,
                    FILE *                  out,
                    struct timeval *        now,
                    fd_set *                fdset_in,
                    void *                  data);

/** run a screen loop on FILE out and call display_callback when events
 * of type vterm_screen_event_t occur. Usually the callback returns the given
 * event, or VTERM_SCREEN_END to end the screen loop.
 *  It is recommanded to call logpool_enable(0) on VTERM_SCREEN_START,
 * and logpool_enable(1) on VTERM_SCREEN_END to avoid logs disturbing display.
 *  Callback has to call fflush(out) to update terminal display.
 * @return VTERM_OK on success, VTERM_ERROR otherwise.
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_screen_loop(
                    FILE *                  out,
                    unsigned int            timer_ms,
                    fd_set *                fduserset_in,
                    vterm_screen_callback_t display_callback,
                    void *                  callback_data);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

