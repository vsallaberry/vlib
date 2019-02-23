/*
 * Copyright (C) 2019 Vincent Sallaberry
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

#define VLIB_CURSESDL

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
#  ifndef OK
#   define OK 0
#  endif
# elif CONFIG_CURSES_H
   /* link to libncurses and include ncurses headers */
#  include <curses.h>
#  include <term.h>
# else
   /* link to libncurses but no header found. define symbols */
#  ifndef OK
#   define OK 0
#  endif
int             setupterm(char *, int, int *);
int             tigetnum(char *);
int             del_curterm(void *);
extern void *   cur_term;
# endif
#endif
/* END NCURSES includes */

#include "vlib/log.h"
#include "vlib_private.h"

/* ************************************************************************* */

#ifdef VLIB_CURSESDL
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wpedantic"
#endif
int vterm_get_columns(int fd) {
    if (!isatty(fd)) {
        return 0;
    }
#if CONFIG_CURSES
# ifndef VLIB_CURSESDL
    int ret;
    if (setupterm(NULL, fd, &ret) != OK) {
        if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
        else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term not found.");
        else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db not found.");
        else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
        return -1;
    } else if ((ret = tigetnum("cols")) <= 0
    &&         (ret = tigetnum("columns")) <= 0
    &&         (ret = tigetnum("COLUMNS")) <= 0) {
        LOG_ERROR(g_vlib_log, "tigetnum(cols,columns,COLUMNS): cannot get value");
        ret = -1;
    }
    del_curterm(cur_term);
    return ret;
# else
    int max_columns = -1;
    int ret;
    void * lib = NULL;
    char ** path;
    int (*setup)(char*, int, int*);
    int (*getnum)(char*);
    int (*delterm)(void *);
    void **curterm;
    char * libs[] = {
        "libtinfo.so", "libtinfo.dylib", "libtinfo.so.5",
        "libncurses.so", "libncurses.dylib", "libcurses.so", "libcurses.dylib",
        "libncurses.so.5", "libcurses.so.5", NULL
    };
    for (path = libs; *path && (lib = dlopen(*path, RTLD_LAZY)) == NULL; path++)
        ; /* loop */
    if (lib) {
        if ((setup = (int(*)(char*,int,int*)) dlsym(lib, "setupterm"))
        &&  (getnum = (int(*)(char*)) dlsym(lib, "tigetnum"))) {
            LOG_DEBUG(g_vlib_log, "options: found ncurses <%s>", *path);
            if (setup(NULL, fd, &ret) != OK) {
                if      (ret == 1)  LOG_ERROR(g_vlib_log, "setupterm(): term is hardcopy.");
                else if (ret == 0)  LOG_ERROR(g_vlib_log, "setupterm(): term not found.");
                else if (ret == -1) LOG_ERROR(g_vlib_log, "setupterm(): term db not found.");
                else                LOG_ERROR(g_vlib_log, "setupterm(): unknown error.");
            } else {
                if ((ret = getnum("cols")) > 0
                || (ret = getnum("columns")) > 0
                || (ret = getnum("COLUMNS")) > 0) {
                    max_columns = ret;
                }
                if ((delterm = (int(*)(void*)) dlsym(lib, "del_curterm")) != NULL
                &&  (curterm = (void**) dlsym(lib, "cur_term")) != NULL) {
                    delterm(*curterm);
                }
            }
        }
        dlclose(lib);
    }
    return max_columns;
# endif
#else /* ifdef CONFIG_CURSES */
    return -1;
#endif
}
#ifdef VLIB_CURSESDL
# pragma GCC diagnostic pop
#endif

