/*
 * Copyright (C) 2017-2018 Vincent Sallaberry
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
 * Simple utilities.
 */
#ifndef VLIB_UTIL_H
#define VLIB_UTIL_H

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

/**
 * This will copy at maximum <len> bytes of <src> in <dst>.
 * dst whose size is <maxlen> will always be terminated by 0,
 * causing possibe src truncation.
 * @return the length of <dst> after copy.
 */
int         strn0cpy(char *dst, const char *src, size_t len, size_t maxlen);

/** strtok_ro_r flags */
#define VLIB_STRTOK_MANDATORY_SEP   (1 << 0)    /* token not found (len=0) if sep no found */
#define VLIB_STRTOK_INCLUDE_SEP     (1 << 1)    /* sep included in returned token */

/**
 * strtok_ro_r()
 * Reentrant strtok/strsep which does not update buffers with ending 0 chars.
 * Search for the next token in <next>, set its address in <token>, return
 * its length, update <next> and <maxlen> for the next call.
 *
 * @param token  [out] the pointer to parsed string token
 * @param seps   [in] the set of delimiters
 * @param next   [in/out] the string to be parsed. set to next token after call
 * @param maxlen [in/out] the maximum size of *next used for parsing. updated
 *               after call.
 * @param flags  [in] parsing options
 *               * VLIB_STRTOK_MANDATORY_SEP: if seps not found, token is
 *               considered as not found (size 0) and the next is not updated.
 *               * VLIB_STRTOK_INCLUDE_SEP: include the separator in the token (last char)
 * @return the length of token if any, or 0. Parsing is finished when *maxlen or *next is 0.
 */
size_t      strtok_ro_r(const char ** token, const char * seps,
                        const char ** next, size_t * maxlen,
                        int flags);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

