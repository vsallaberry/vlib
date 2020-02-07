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
#else
# include <string.h>
# include <stdio.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** flags for vterm_init() */
typedef enum {
    VTF_NONE = 0,
    /*VTF_1 = 1 << 0, */
    /*VTF_2 = 1 << 1, */
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
    /* Reserved */
    VCOLOR_RESERVED,
    VCOLOR_RESET = VCOLOR_RESERVED,
    VCOLOR_EMPTY
} vterm_color_t;

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
 * vterm_free() must be called when this routine returns success.
 */
int             vterm_init(int fd, unsigned int flags);

/** clean the terminal allocated resources.
 * @return VTERM_OK on success, VTERM_ERROR on error. */
int             vterm_free();

/** get number of columns of terminal attached to <fd>.
 * @return columns
 *         or 0 if fd is not a terminal
 *         or VTERM_ERROR on error
 * @notes implicit call to cterm_init() is done, vterm_free() needed. */
int             vterm_get_columns(int fd);

/** get color capability of terminal attached to <fd>.
 * @return 1 if terminal with color capability.
 *         or 0 if terminal does not support colors
 * @notes implicit call to cterm_init() is done, vterm_free() needed. */
int             vterm_has_colors(int fd);

/** get color string for terminal attached to <fd>.
 * @return the color string, or empty ("") on error */
const char *    vterm_color(int fd, vterm_color_t color);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

