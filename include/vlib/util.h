/*
 * Copyright (C) 2017-2020 Vincent Sallaberry
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
# include <cinttypes>
#else
# include <string.h>
# include <stdio.h>
# include <inttypes.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** snprintf wrapper which returns the real number of characters
 * stored in the string, rather than the number of character which would
 * have been stored in an unlimited buffer
 * @notes: WARNING: size is evaluated several times
 * @notes: WARNING: ret_var must be a lvalue and is intermediate variable */
#define VLIB_SNPRINTF(ret_var, buffer, size, ...) \
            (((ret_var) = snprintf(buffer, size, __VA_ARGS__)) < 0 \
              || (size) == 0 ? 0 : ((ssize_t)(ret_var) >= (ssize_t)(size) \
                                    ? (ssize_t) ((size) - 1) : (ret_var)))

/** see VLIB_SNPRINTF */
#define VLIB_VSNPRINTF(ret_var, buffer, size, fmt, valist) \
            (((ret_var) = vsnprintf(buffer, size, fmt, valist)) < 0 \
              || (size) == 0 ? 0 : ((ssize_t)(ret_var) >= (ssize_t)(size) \
                                    ? (ssize_t) ((size) - 1) : (ret_var)))

/**
 * This will copy at maximum <maxlen-1> bytes of <src> in <dst>.
 * dst will always be terminated by 0, causing possibe src truncation.
 * This is the same as strlcpy with in addition handling of NULL for dst & src.
 * @return the length of <dst> after copy
 *         or 0 if src or maxlen is NULL
 *         or 0 with NUL char in *dst if src is NULL. */
size_t      str0cpy(char *dst, const char *src, size_t maxlen);

/**
 * This will copy at maximum <len> bytes of <src> in <dst>.
 * dst whose size is <maxlen> will always be terminated by 0,
 * causing possibe src truncation.
 * This is the same as strlcpy with in addition handling of NULL for dst & src and
 * possibility to copy only a part of src (len, which could be greater than dst size).
 * @return the length of <dst> after copy
 *         or 0 if src or maxlen is NULL
 *         or 0 with NUL char in *dst if src is NULL. */
size_t      strn0cpy(char *dst, const char *src, size_t len, size_t maxlen);

/** strerror_r POSIX version */
int         vstrerror_r(int err, char * buffer, size_t size);

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

/** same as strtol() but returns 0 on success, and strict conv. if endptr NULL */
int         vstrtol(const char * str, char ** endptr, int base, long * l);

/** same as strtoul() but returns 0 on success, force >=0, and strict conv. if endptr NULL */
int         vstrtoul(const char * str, char ** endptr, int base, unsigned long * ul);

/** same as strtoimax() but returns 0 on success, and strict conv. if endptr NULL */
int         vstrtoimax(const char * str, char ** endptr, int base, intmax_t * imax);

/** same as strtoumax() but returns 0 on success, force >= 0 and strict conv. if endptr NULL */
int         vstrtoumax(const char * str, char ** endptr, int base, uintmax_t * umax);

/** same as strtod() but returns 0 on success, and strict conv. if endptr NULL */
int         vstrtod(const char * str, char ** endptr, double * d);

/** same as strtold() but returns 0 on success, and strict conv. if endptr NULL */
int         vstrtold(const char * str, char ** endptr, long double * ld);

/** vabspath: get file absolute path
 * @param dst where to store absolute path
 * @param maxlen the capacity of dst buffer
 * @param path the path to convert
 * @param the current working directory (getcwd() is used if NULL)
 * @return length of string stored in dst
 * */
int         vabspath(char * dst, size_t maxlen, const char * path, const char * cwd);

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

typedef int     (*vdecode_fun_t)(FILE *, char *, unsigned, void **);

/** return a 0 terminated line from data returned by vdecode_fun
 * it contains \n if it is not the last line.
 * WARNING, the content of pline and pline_capacity MUST NOT BE CHANGED
 * Caller must free *pline if not NULL after final call
 * See vdecode_buffer().
 * @param pline cannot be NULL, *pline can be NULL
 * @parm pline_capacity cannot be NULL, *pline_capacity can be 0
 * @param line_maxsz, maximum size of line, infinite if 0
 * @param ctx cannot be NULL, *ctx must be NULL on first call
 * @param decodefun, a function similar to vdecode_buffer or vlib_get_source()
 * @return length of line including \n if any, 0 is empty, -1 on error */
ssize_t     vdecode_getline_fun(
                char **         pline,
                size_t *        pline_capacity,
                size_t          line_maxsz,
                void **         ctx,
                vdecode_fun_t   decodefun);

/** return a 0 terminated line from data returned by vdecode_buffer
 * it contains \n if it is not the last line.
 * WARNING, the content of pline and pline_capacity MUST NOT BE CHANGED
 * Caller must free *pline if not NULL after final call
 * See vdecode_buffer().
 * @param pline cannot be NULL, *pline can be NULL
 * @parm pline_capacity cannot be NULL, *pline_capacity can be 0
 * @param line_maxsz, maximum size of line, infinite if 0
 * @param ctx cannot be NULL, *ctx must be NULL on first call
 * @param inbuf, where are vdecode_buffer input data
 * @param inbufsz the size of vdecode_buffer input data
 * @return length of line including \n if any, 0 is empty, -1 on error */
ssize_t     vdecode_getline_buf(
                char **         pline,
                size_t *        pline_capacity,
                size_t          line_maxsz,
                void **         ctx,
                const char *    inbuf,
                size_t          inbufsz);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

