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

/** vdecode_buffer : decode <inbuf> (zlib or raw (char[]), char * tab[])
 * This function must be called until it returns 0 or -1.
 * To release resources before it returns 0 or -1, call it with (NULL, 0, &ctx, NULL, 0).
 * @param out if not NULL, the decoded data is writen to file out
 * @param outbuf if not NULL, the decoded data is added in outbuf
 * @param outbufsz the maximum size of outbuf
 * @param ctx the pointer to decoding context, can be NULL only if decoding to file
 *        (out != NULL), otherwise it must be a void ** with *ctx = NULL on first call,
 *        and the updated value on next calls.
 *        The function will allocate and free memory internally.
 * @param inbuf the buffer containing data to be decoded.
 *   1) gzip (it will start with 0x1f,0x8b,0x08), C array can be generated with:
 *      $ echo "string" | gzip -c | od -An -tuC
 *        | sed -e 's/[[:space:]][[:space:]]*0*\([0-9][0-9]*\)/\1,/g'
 *  2) array of strings (first (char *) will be 0x0abcCafe)
 *     const char *const array[] = { 0x0abcCafe, "String1", "String2", NULL };
 *  3) raw data (char arrary, starting with 0x0c, 0x0a, 0x0f, 0x0e)
 *     const char array[] = { 0x0c, 0x0a, 0x0f, 0x0e, 1, 2, 3, 4, 5, 6, 7 };
 * @param inbufsz the size of buffer
 * @return number of decoded bytes, 0 when finished, -1 on error */
#define     VDECODEBUF_STRTAB_MAGIC ((const char *) 0x0abcCafeUL)
#define     VDECODEBUF_RAW_MAGIC    "\x0c\x0a\x0f\x0e"
ssize_t     vdecode_buffer(
                FILE *          out,
                char *          outbuf,
                size_t          outbufsz,
                void **         ctx,
                const char *    inbuf,
                size_t          inbufsz);

/** get number of columns of terminal.
 * @return columns
 *         or 0 if fd is not a terminal
 *         or -1 on error */
int         vterm_get_columns(int fd);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

