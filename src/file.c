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
 * Simple file utilities.
 */
#include <unistd.h>
#include <fcntl.h>

#include "vlib/util.h"

int         vabspath(char * dst, size_t maxlen, const char * path, const char * cwd) {
    int     len;
    char *  last_slash;
    int     cwd_fd = -1;

    if (dst == NULL || path == NULL || maxlen <= 2) {
        if (dst != NULL && maxlen > 0)
            *dst = 0;
        return 0;
    }

    last_slash = strrchr(path, '/');
    if (last_slash == path) {
        len = str0cpy(dst, path, maxlen);
    } else {
        if (last_slash != NULL) {
            *last_slash = 0;
            cwd_fd = open(".", O_RDONLY);
            if (cwd != NULL)
                chdir(cwd);
        }
        if ((last_slash != NULL && chdir(path) != 0)
                || getcwd(dst, maxlen - 2) == NULL) {
            if (last_slash != NULL)
                *last_slash = '/';
            len = str0cpy(dst, path, maxlen);
        } else {
            if (cwd != NULL)
                len = str0cpy(dst, cwd, maxlen - 2);
            else
                len = strlen(dst);
            dst[len++] = '/';
            len += str0cpy(dst + len, last_slash ? last_slash + 1: path,
                           maxlen - len);
        }
    }
    if (cwd_fd >= 0) {
        fchdir(cwd_fd);
        close(cwd_fd);
    }
    return len;
}

