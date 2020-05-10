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

typedef enum {
    VTERM_CAP_EMPTY = 0,
    VTERM_KEY_EMPTY = VTERM_CAP_EMPTY,
    VTERM_CAP_UP,
    VTERM_CAP_DOWN,
    VTERM_CAP_LEFT,
    VTERM_CAP_RIGHT,
    VTERM_CAP_KB_KEYCODE,
    VTERM_CAP_NO_KEYCODE,
    VTERM_KEY_UNKNOWN,
    VTERM_KEY_UP,
    VTERM_KEY_DOWN,
    VTERM_KEY_LEFT,
    VTERM_KEY_RIGHT,
    VTERM_KEY_SH_LEFT,
    VTERM_KEY_SH_RIGHT,
    VTERM_KEY_BACKSPACE,
    VTERM_KEY_DEL,
    VTERM_KEY_TAB,
    VTERM_KEY_ESC,
    VTERM_KEY_HOME,
    VTERM_KEY_END,
    VTERM_KEY_PAGEUP,
    VTERM_KEY_PAGEDOWN,
    VTERM_KEY_F1,
    VTERM_KEY_F2,
    VTERM_KEY_F3,
    VTERM_KEY_F4,
    VTERM_KEY_F5,
    VTERM_KEY_F6,
    VTERM_KEY_F7,
    VTERM_KEY_F8,
    VTERM_KEY_F9,
    VTERM_KEY_F10,
    VTERM_KEY_F11,
    VTERM_KEY_F12,
    VTERM_KEY_STOP,
    VTERM_KEY_EOF,
    VTERM_KEY_INT,
    VTERM_CAPS_NB /* must be last */
} vterm_cap_t;

/** vterm_colorset_t, a bit combination of vterm_color_t */
typedef unsigned int    vterm_colorset_t;

#define         VCOLOR_NULL     UINT_MAX

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
 * @param fd the descriptor of terminal
 * @param [OUT] p_lines where to store number of lines (can be NULL)
 * @param [OUT] p_columns where to store number of columns (can be NULL)
 * @return VTERM_OK on SUCCESS
 *         or VTERM_NOTTY if fd is not a terminal
 *         or VTERM_ERROR on error
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_get_winsize(int fd, unsigned int * p_lines, unsigned int * p_columns);

/** get number of columns of terminal attached to <fd>.
 * @return columns
 *         or 0 if fd is not a terminal
 *         or VTERM_ERROR on error
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed.
 * @notes use vterm_get_winsize rather than calling vterm_get_lines after this */
int             vterm_get_columns(int fd);

/** get number of lines of terminal attached to <fd>.
 * @return lines
 *         or 0 if fd is not a terminal
 *         or VTERM_ERROR on error
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed.
 * @notes use vterm_get_winsize rather than calling vterm_get_columns after this */
int             vterm_get_lines(int fd);

/** clear screen on terminal attached to <out>.
 * @return VTERM_OK on success
 *         VTERM_NOTTY if not tty
 *         VTERM_ERROR otherwise
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_clear(FILE * out);

/* ************************************************************************* */
/** clear given area on terminal attached to <out>.
 * @return VTERM_OK on success
 *         VTERM_NOTTY if not tty
 *         VTERM_ERROR otherwise
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_clear_rect(FILE * out, int row, int col, int end_row, int end_col);

/** get color capability of terminal attached to <fd>.
 * @return 1 if terminal with color capability.
 *         or 0 if terminal does not support colors
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
int             vterm_has_colors(int fd);

/** get foreground/background color of the terminal attached to <fd>.
 * @return colors the result of VCOLOR_BUILD(fore, back, style)
 * @notes not supported on all terminals, COLORFGBG env can be set manually,
 *        with format 'foreground;background': "15;0" for dark, "0;15" for light.
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
vterm_colorset_t vterm_termfgbg(int fd);

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

/** Setup the given color combination (fore, back, style) on the terminal
 * reentrancy of vtermbuild_color depend on reentrancy of buffer and psize.
 * If buffer NULL, new buffer is allocated and returned.
 * @param fd the file descriptor of the terminal
 * @param colors the result of VCOLOR_BUILD(fore, back, style)
 * @param buffer the buffer storing the color string of max size *psize
 * @param psize the maxsize of buffer and the output amount of written characters,
 *        NULL accepted only if buffer is NULL,
 * @return input buffer containing colors string or NULL or empty string on error.
 *         When buffer is NULL, NULL is only returned if out of memory, otherwise,
 *         strdup("") is returned. When buffer is not NULL, NULL is only returned
 *         if *psize == 0 (meaning no space in buffer, cannot put NUL char).
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
char *          vterm_buildcolor(int fd, vterm_colorset_t colors, char * buffer, size_t * psize);

/**
 * @return the size of given color string, or 0 if not found. */
unsigned int    vterm_color_size(int fd, vterm_color_t color);

/**
 * @return the maximum size of a color string on given terminal. */
unsigned int    vterm_color_maxsize(int fd);

/** actual number of visible characters in string.
 * @param fd the file descriptor of the terminal
 * @param str the string
 * @param size ptr to max number of characters to check or NULL to check all.
 *        Before return *size is updated to a valid truncated real len,
 *        switable for a call to write(2) or fwrite(3).
 * @param maxlen if not 0 the maximum returned len
 * @return the size of string without color escapes */
size_t          vterm_strlen(int fd, const char * str, size_t * size, size_t maxlen);

/** get cap string for terminal attached to <fd>.
 * vterm_cap is reentrant and can be used several times in *printf().
 * @param cap a cap ID
 * @return the cap string, or empty ("") on error.
 * @notes implicit call to vterm_init, vterm_enable(0) or vterm_free needed. */
const char *    vterm_cap(int fd, vterm_cap_t color);

/**
 * @return the size of given cap string, or 0 if not found. */
unsigned int    vterm_cap_size(int fd, vterm_cap_t color);

/**
 * @return the maximum size of a cap string on given terminal. */
unsigned int    vterm_cap_maxsize(int fd);

/** check if a string matches a given vterm cap
 * @return non-zero if true, 0 otherwise */
int             vterm_cap_check(int fd, vterm_cap_t cap,
                                const char * buffer, unsigned int buf_size);

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

/** print string at given row/columns
 * @return VTERM_NOTTY, VTERM_ERROR, or printf return value */
int             vterm_printxy(FILE * out, int row, int col, const char * fmt,
                              ...) __attribute__((format(printf, 4, 5)));

/** RFU vterm_prompt() and vterm_readline() data */
typedef struct  vterm_readline_s vterm_readline_t;

/** read user input until EOL or buffer full
 * @return size of result string or VTERM_ERROR, VTERM_NOTTY on error */
int             vterm_readline(FILE * in, FILE * out, char * buf,
                               unsigned int maxsize, vterm_readline_t * rl_data);

/** read user input until EOL or buffer full, with prompt and optional erasing.
 * @return size of result string or VTERM_ERROR, VTERM_NOTTY on error */
#define         VTERM_PROMPT_ERASE          (1 << 0)
#define         VTERM_PROMPT_WITH_DEFAULT   (1 << 1) /* accept a default value (in buf) */
int             vterm_prompt(
                    const char *            prompt,
                    FILE *                  in,
                    FILE *                  out,
                    char *                  buf,
                    unsigned int            maxsize,
                    int                     flags,
                    vterm_readline_t *      rl_data);

/** Events values for vterm_screen_loop() */
typedef enum {
    VTERM_SCREEN_INIT = 0,  /* before vterm_goto_enable(1) and VTERM_SCREEN_START */
    VTERM_SCREEN_START,     /* after vterm_goto_enable(1), before loop, or after SIG STOP/CONT */
    VTERM_SCREEN_LOOP,      /* at each loop or event */
    VTERM_SCREEN_TIMER,     /* when the timer (timer_ms) expires */
    VTERM_SCREEN_INPUT,     /* when STDIN_FILENO or file in fduserset_in is
                               ready for read. Callback uses FD_ISSET(fd, fdset_in)*/
    VTERM_SCREEN_END,       /* when loop stops, before vterm_goto_enable(0) */
    VTERM_SCREEN_EXIT       /* when loop stops, after vterm_goto_enable(0) */
} vterm_screen_event_t;

/** vterm_screen_callback_t (callback) return value, for vterm_screen_loop() */
typedef enum {
    VTERM_SCREEN_CB_OK          = 1 << 0,   /* callback ok */
    VTERM_SCREEN_CB_EXIT        = 1 << 1,   /* exit requested by callback */
    VTERM_SCREEN_CB_NEWTIMER    = 1 << 2,   /* timer update requested by callback */
} vterm_screen_cb_result_t;

/** screen event data given to callback (IN), and given back to screenloop (OUT).
 * IN:  Used fields depends on event (vterm_screen_event_t)
 * OUT: Used fields depends on result (vterm_screen_cb_result_t) */
typedef union {
    /* event VTERM_SCREEN_INPUT */
    struct {
        fd_set          fdset_in; /* read fd_set containing FDs having input */
        vterm_cap_t     key;      /* RFU: pressed key or VTERM_KEY_UNKNOWN if
                                     unknown, VTERM_KEY_EMPTY if not read */
        const char *    key_buffer; /* RFU: key buffer if not VTERM_KEY_EMPTY */
        unsigned int    key_size;   /* RFU: key size if not VTERM_KEY_EMPTY */
    }                               input;
    /* NOT IMPLEMENTED: event VTERM_SCREEN_RESIZE */
    struct {
        unsigned int    newcols;
        unsigned int    newrows;
    }                               resize;
    /* callback result VTERM_SCREEN_CB_NEWTIMER */
    unsigned int                    newtimer_ms;
    /* event RFU */
    void *                          ptr;
} vterm_screen_ev_data_t;

/** callback for vterm_screen_loop()
 * @param evdata IN/OUT event data, see vterm_screen_ev_data_t
 */
typedef unsigned int (*vterm_screen_callback_t)(
                    vterm_screen_event_t    event,
                    FILE *                  out,
                    struct timeval *        now,
                    vterm_screen_ev_data_t *evdata,
                    void *                  user_data);

/** run a screen loop on FILE out and call display_callback when events
 * of type vterm_screen_event_t occur. Usually the callback returns
 * VTERM_SCREEN_CB_OK, or a bit combination of vterm_screen_cb_result_t.
 * (VTERM_SCREEN_CB_EXIT will end the loop, VTERM_SCREEN_CB_NEWTIMER will update timer).
 *  It is recommanded to call (logpool_replacefile(out, &backup) or logpool_enable(0))
 *  on VTERM_SCREEN_INIT, and (logpool_replacefile(backup) or logpool_enable(1))
 *  on VTERM_SCREEN_EXIT to avoid logs disturbing display.
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

