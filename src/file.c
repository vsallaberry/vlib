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
#include <limits.h>
#include <stdlib.h>

#include "vlib/util.h"

int         vabspath(char * dst, size_t maxlen, const char * path, const char * cwd) {
    size_t          len;
    const char *    next, * token;
    size_t          token_len;

    if (dst == NULL || path == NULL || maxlen <= 2) {
        if (dst != NULL && maxlen > 0)
            *dst = 0;
        return 0;
    }

    len = 0;
    *dst = 0;
    if (*path != '/') {
        /* prefix path with current working directory if not starting with '/' */
        if (cwd != NULL) {
            len = str0cpy(dst, cwd, maxlen - 2);
        } else if (getcwd(dst, maxlen - 2) == NULL) {
            len = str0cpy(dst, path, maxlen);
            return len;
        } else {
            len = strlen(dst);
        }
    }

    /* split path between '/', get rid of '..', '.' or '//' */
    next = path;
    while (len < maxlen - 1 && ((token_len = strtok_ro_r(&token, "/", &next, NULL, 0)) > 0 || *next)) {
        if (token_len == 0)
            continue ; /* ignore '//' */
        if (token_len == 1 && *token == '.')
            continue ; /* ignore '.' */
        if (token_len == 2 && *token == '.' && token[1] == '.') { /* handle '..' */
            while (len > 0 && dst[len] != '/') {
                --len;
            }
            dst[len] = 0;
            continue ;
        }
        dst[len++] = '/';
        len += strn0cpy(dst + len, token, token_len, maxlen - len);
    }

    if (len == 0)
        dst[len++] = '/';

    dst[len] = 0;

    if (len > 0) {
        /* finally get rid of symbolic links in case file is valid
         * we clean path before as realpath works only on existing files */
        char rpath[PATH_MAX*2];
        if (realpath(dst, rpath) != NULL) {
            len = str0cpy(dst, rpath, maxlen);
        }
    }

    return len;
}

