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

#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * This will copy at maximum <len> bytes of <src> in <dst>.
 * dst whose size is <maxlen> will always be terminated by 0,
 * causing possibe src truncation.
 * @return the length of <dst> after copy.
 */
int strn0cpy(char *dst, const char *src, size_t len, size_t maxlen);

/**
 * //TODO improve doc
 *
 * Reentrant strtok/strsep which does not update buffers with ending 0 chars.
 *
 * @param token [out] the pointer to current string token
 * @param seps [in] the set of delimiters
 * @param next [in/out] the string to be parsed. set to next token after call
 * @param maxlen the maximum size of *next used for parsing. updated after call.
 * @param flags parsing options
 *              1: if seps not found, token is considered as not found (size 0)
 *                 and the next is not updated
 * @return the length of token
 */
int         strtok_ro_r(const char ** token, const char * seps,
                        const char ** next, size_t * maxlen,
                        int flags);

/** Bench Decl */
#define BENCH_DECL(name)    struct {  \
                                struct timeval t0; \
                                struct timeval t1; \
                            } name;

/** Bench Start */
#define BENCH_START(name)   gettimeofday(&((name).t0), NULL)

/** Bench Stop & Display */
#define BENCH_PRINT(name, ...) do { \
                gettimeofday(&((name).t1), NULL); \
                timersub(&((name).t1), &((name).t0), &((name).t1)); \
                fprintf(stderr, __VA_ARGS__); \
                fprintf(stderr, "DURATION = %u.%06u\n", \
                        (unsigned int) ((name).t1).tv_sec, \
                        (unsigned int) ((name).t1).tv_usec);\
            } while(0)

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

