/*
 * Copyright (C) 2017-2019 Vincent Sallaberry
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
 * Simple string utilities.
 */
#include "vlib/util.h"

int strn0cpy(char *dst, const char *src, size_t len, size_t maxlen) {
    if (maxlen == 0 || dst == NULL || src == NULL) {
        return 0;
    }
    if (len >= maxlen) {
        len = maxlen - 1;
    }
    strncpy(dst, src, len);
    dst[len] = 0;
    return len;
}


size_t strtok_ro_r(const char ** token, const char * seps,
                   const char ** next, size_t * maxlen,
                   int flags) {
    size_t token_len;

    /* sanity checks */
    if (!token || !next || !seps || !*next || (maxlen && *maxlen == 0)) {
        return 0;
    }

    /* token_len: index in *next of any character of seps or index of '0'.
     * It will also be the length of *token */
    *token = *next;
    token_len = strcspn(*next, seps);

    /* checking whether the separator was found or not */
    int found = 1;
    if (maxlen && token_len > *maxlen) {
        found = 0;
        token_len = *maxlen;
    } else if ((*next)[token_len] == 0) {
        found = 0;
    }

    /* if separator not found, flags determines if we consider the token */
    if (found) {
        if ((flags & VLIB_STRTOK_INCLUDE_SEP) == 0)
            ++(*next);
        else
            ++token_len;
    } else if ((flags & VLIB_STRTOK_MANDATORY_SEP) != 0) {
        return 0;
    }

    /* token is taken into account, update remaining length and go to next one */
    *next += token_len;
    if (maxlen) {
        *maxlen -= (*next - *token);
    }

    return token_len;
}

