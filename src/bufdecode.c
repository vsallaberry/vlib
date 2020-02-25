/*
 * Copyright (C) 2018-2020 Vincent Sallaberry
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
 * buffer decoding utilities: supports char[], char *[], zlib.
 */
#ifdef HAVE_VERSION_H
# include "version.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

/* ************************************************************************ */
#ifndef BUILD_VLIB
# define BUILD_VLIB 0
#endif
#if BUILD_VLIB
# include "vlib/util.h"
# include "vlib/log.h"
#else
# define VDECODEBUF_STRTAB_MAGIC    ((const char *) 0x0abcCafeUL)
# define VDECODEBUF_RAW_MAGIC       "\x0c\x0a\x0f\x0e"
# define LOG_INFO(log,...)
# define LOG_DEBUG(log,...)
# define LOG_DEBUG_BUF(log,...)
# define g_vlib_log NULL
#endif
#ifndef CONFIG_ZLIB
# define CONFIG_ZLIB 0
#endif
#ifndef CONFIG_ZLIB_H
# define CONFIG_ZLIB_H 0
#endif
#if CONFIG_ZLIB_H
# include <zlib.h>
#else
/* no zlib header on this system */
#define ZLIB_VERSION    "1.2.5"
#define Z_NULL          0
#define Z_NO_FLUSH      0
#define Z_OK            0
#define Z_STREAM_END    1
#define Z_NEED_DICT     2
#define Z_STREAM_ERROR (-2)
#define Z_DATA_ERROR   (-3)
#define Bytef           unsigned char
typedef struct {
    Bytef *             next_in;
    unsigned            avail_in;
    unsigned long       total_in;
    Bytef *             next_out;
    unsigned            avail_out;
    unsigned long       total_out;
    char *              msg;
    void *              state;
    int                 (*zalloc)();
    int                 (*zfree)();
    void *              opaque;
    int                 data_type;
    unsigned long       adler;
    unsigned long       reserved;
} z_stream;
int inflateInit2_(z_stream * z, int flags, const char * version, int struct_size);
#define inflateInit2(strm, windowBits) \
        inflateInit2_((strm), (windowBits), zlibVersion(), (int)sizeof(z_stream))
int inflate(z_stream * z, int flags);
int inflateEnd(z_stream * z);
const char * zlibVersion();
#endif

/* ************************************************************************ */
typedef struct {
    int (*inflate_init) (z_stream *, int);
    int (*inflate_end)  (z_stream *);
    int (*inflate_do)   (z_stream *, int);
} decode_wrapper_t;

typedef struct {
    z_stream            z;
    size_t              off;
    void *              user_data;
    decode_wrapper_t *  lib;
} decodebuf_t; /* ##ZSRC_BEGIN */

/* ************************************************************************ */
static int inflate_init_raw(z_stream * z, int flags) {
    (void) z;
    (void) flags;
    return Z_OK;
}
static int inflate_end_raw(z_stream * z) {
    (void) z;
    return Z_OK;
}
static int inflate_raw(z_stream * z, int flags) {
    size_t size = z->avail_in;
    (void) flags;

    if (size == 0) {
        return Z_STREAM_END;
    }
    if (size > z->avail_out) {
        size = z->avail_out;
    }
    memcpy(z->next_out, z->next_in, size);
    z->avail_out -= size;
    z->avail_in -= size;
    return Z_OK;
}
static int inflate_strtab(z_stream * z, int flags) {
    size_t      n;
    char **     ptr = ((char **) z->msg);
    char *      end;
    (void) flags;

    if (ptr == NULL || *ptr == NULL || z->next_in == NULL) {
        return Z_STREAM_END;
    }
    while (z->avail_out > 0) {
        end = stpncpy((char *) z->next_out, (const char *) z->next_in, z->avail_out);
        n = (end - (char *) z->next_out);
        z->avail_out -= n;
        z->next_out += n;
        z->next_in += n;
        if (*z->next_in == 0) { /* 0 was encoutered in next_in */
            ++ptr;
            z->msg = (char *) ptr;
            z->next_in = (Bytef *) (*ptr);
            z->avail_in -= sizeof(char *);
            if (z->next_in == NULL) {
                break ;
            }
        }
    }
    return Z_OK;
}

/* ************************************************************************ */
#if CONFIG_ZLIB
static int inflate_init_zlib(z_stream * z, int flags) {
    /* InflateInit2 can be macros, this wrapper is needed to use a function pointer */
    return inflateInit2(z, flags);
}
static decode_wrapper_t s_decode_zlib = {
    .inflate_init   = inflate_init_zlib,
    .inflate_end    = inflateEnd,
    .inflate_do     = inflate,
};
#endif
/* ************************************************************************ */
static decode_wrapper_t s_decode_raw = {
    .inflate_init   = inflate_init_raw,
    .inflate_end    = inflate_end_raw,
    .inflate_do     = inflate_raw,
};
/* ************************************************************************ */
static decode_wrapper_t s_decode_strtab = {
    .inflate_init   = inflate_init_raw,
    .inflate_end    = inflate_end_raw,
    .inflate_do     = inflate_strtab,
};

/* ************************************************************************ */
ssize_t             vdecode_buffer(
                        FILE *          out,
                        char *          outbuf,
                        size_t          outbufsz,
                        void **         ctx,
                        const char *    inbuf,
                        size_t inbufsz) {
    int                internalbuf = 0, ret = Z_DATA_ERROR;
    ssize_t         n = 0;
    decodebuf_t *   pctx = ctx ? *ctx : NULL;

    if (outbuf == NULL) {
        if (out == NULL || (outbuf = malloc((internalbuf = outbufsz = 4096))) == NULL) {
            if (pctx != NULL) {
                pctx->lib->inflate_end(&pctx->z);
                free(pctx);
                if (ctx != NULL)
                    *ctx = NULL;
            }
            return -1; /* cannot use internal if no file given */
        }
    } else if (ctx == NULL || outbufsz == 0) {
        if (pctx != NULL) {
            pctx->lib->inflate_end(&pctx->z);
            free(pctx);
            if (ctx != NULL)
                *ctx = NULL;
        }
        return -1; /* fail if giving a buf with size 0 or without ctx */
    }
    if (pctx == NULL) { /* init inflate stream */
        if ((pctx = malloc(sizeof(decodebuf_t))) == NULL) {
            if (internalbuf)
                free(outbuf);
            if (ctx) {
                *ctx = NULL;
            }
            return -1;
        }
        memset(&pctx->z, 0, sizeof(z_stream));
        pctx->z.next_in = Z_NULL;
        pctx->z.zalloc = Z_NULL;
        pctx->z.zfree = Z_NULL;
        pctx->z.opaque = Z_NULL;
        pctx->z.avail_in = 0;
        pctx->off = 0;
        pctx->user_data = NULL;

        if (inbufsz >= 3 && inbuf
        && inbuf[0] == 31 && (unsigned char)(inbuf[1]) == 139 && inbuf[2] == 8) {
            /* GZIP MAGIC */
#          if CONFIG_ZLIB
            pctx->lib = &s_decode_zlib;
#          else
            pctx->lib = NULL;
#          endif
        } else if (inbufsz >=4 && inbuf
        && inbuf[0] == 0x0c && inbuf[1] == 0x0a && inbuf[2] == 0x0f && inbuf[3] == 0x0e) {
            /* RAW (char array) MAGIC */
            pctx->lib = &s_decode_raw;
            pctx->off = 4;
        } else if (inbufsz >= sizeof(void *) && inbuf
        && (const char *)(*((const char*const*)inbuf)) == VDECODEBUF_STRTAB_MAGIC) {
            /* STRTAB (string array) MAGIC */
            char ** ptr = (char **) inbuf;
            ++ptr;
            pctx->lib = &s_decode_strtab;
            pctx->z.msg = ((char *) ptr);
            pctx->z.next_in = (Bytef *) *ptr;
            pctx->z.avail_in = inbufsz - sizeof(char *);
        } else {
            /* UNKNOWN MAGIC, assuming it is raw */
            pctx->lib = &s_decode_raw;
        }
        if (pctx->lib == NULL || inbuf == NULL
        ||  (ret = pctx->lib->inflate_init(&pctx->z, 31/*15(max_window)+16(gzip)*/)) != Z_OK) {
            if(pctx)
                free(pctx);
            if(internalbuf)
                free(outbuf);
            if (ctx)
                *ctx = NULL;
            return -1;
        }
        if (ctx != NULL)
            *ctx = pctx;
    }
    do { /* decompress buffer */
        if (pctx->z.avail_in == 0 && pctx->off < inbufsz) {
            pctx->z.avail_in = pctx->off + outbufsz > inbufsz ? inbufsz - pctx->off : outbufsz;
            pctx->z.next_in = (Bytef*) inbuf + pctx->off;
            pctx->off += outbufsz;
        }
        pctx->z.avail_out = outbufsz;
        pctx->z.next_out = (Bytef*) outbuf;
        if ((ret = pctx->lib->inflate_do(&pctx->z, Z_NO_FLUSH)) != Z_OK && ret != Z_STREAM_END)
            break ;
        n = outbufsz - pctx->z.avail_out;
        if (n > 0 && out && (fwrite(outbuf, 1, n, out) != (size_t)n
        ||  ferror(out)) && ((ret = Z_STREAM_ERROR) || 1))
            break ;
    } while (ret != Z_STREAM_END && (internalbuf || (ret == Z_OK && n == 0)));
    if (internalbuf) {
        free(outbuf);
        n = 0;
    }
    if (n <= 0 || (ret != Z_OK && ret != Z_STREAM_END)) { /* last call */
        pctx->lib->inflate_end(&pctx->z);
        free(pctx);
        if (ctx)
            *ctx = NULL;
    }
    return ret == Z_OK || ret == Z_STREAM_END ? n : -1;
}

/* ************************************************************************ */
static inline ssize_t vdecode_getline(
                        char **         pline,
                        size_t *        pline_capacity,
                        size_t          line_maxsz,
                        void **         ctx,
                        vdecode_fun_t   decodefun,
                        const char *    inbuf,
                        size_t inbufsz) {
    char *          eol;
    decodebuf_t *   pctx;
    size_t          lineoff;

    if (pline == NULL || pline_capacity == NULL || ctx == NULL
    ||  (decodefun == NULL && (inbuf == NULL || inbufsz == 0))) {
        //LOG_ERROR(g_vlib_log, "%s(): pline/pline_capacity/ctx NULL");
        return -1;
    }
    if (*pline == NULL || *ctx == NULL) {
        /* first call */
        if (*pline == NULL) {
            lineoff = line_maxsz == 0 ? 512
                      : (line_maxsz >= 32 ? line_maxsz / 8 : line_maxsz);
            *pline = malloc(lineoff * sizeof(char));
            *pline_capacity = lineoff;
        }
        lineoff = 0;
        **pline = 0;
    } else {
        pctx = *ctx;
        lineoff = (size_t) pctx->user_data;
        if ((eol = strchr(*pline, '\n')) == NULL) {
            eol = *pline + strlen(*pline) - 1;
        }
        *(eol + 1) = (*pline)[*pline_capacity - 1];
        memmove(*pline, eol + 1, (lineoff + *pline - eol));
        lineoff -= eol - *pline + 1;
    }
    while (1) {
        ssize_t n;

        eol = strchr(*pline, '\n');

        if (eol == NULL) {
            /* eol not found, need to get more data */
            if (lineoff + 2 >= *pline_capacity) {
                /* buffer need to be increased */
                if (line_maxsz > 0 && *pline_capacity * 2 > line_maxsz) {
                    /* buffer cannot be increased */
                    eol = *pline + lineoff - 1;
                    //fprintf(stderr, "PFFF n=%zd next=%d\n", n, eol[1]);
                    break ;
                }
                *pline = realloc(*pline, *pline_capacity * 2);
                if (*pline == NULL) {
                    //LOG_ERROR(g_vlib_log, "%s(): cannot alloc line above %zu", *pline_capacity);
                    return -1;
                }
                *pline_capacity *= 2;
            }
            /* get more data */
            if (inbuf != NULL)
                n = vdecode_buffer(NULL, *pline + lineoff, *pline_capacity - lineoff - 2,
                        ctx, inbuf, inbufsz);
            else
                n = decodefun(NULL, *pline + lineoff,
                              *pline_capacity - lineoff - 2, ctx);

            pctx = *ctx;
            if (n < 0 || (n > 0 && *ctx == NULL)) {
                //LOG_ERROR(g_vlib_log, "%s(): vdecode_buffer error");
                return -1;
            }
            (*pline)[lineoff+n] = 0;
            lineoff += n;
            if (n > 0) {
                /* there is more data, loop again to search '\n' */
                continue ;
            } else {
                eol = *pline + lineoff - 1;
            }
        }
        break ;
    }
    (*pline)[*pline_capacity - 1] = *(eol + 1);
    *(eol + 1) = 0;
    if (pctx != NULL)
        pctx->user_data = (void *) lineoff;

    return (eol - *pline + 1);
}

ssize_t vdecode_getline_fun(
                        char **         pline,
                        size_t *        pline_capacity,
                        size_t          line_maxsz,
                        void **         ctx,
                        vdecode_fun_t   decodefun) {
    return vdecode_getline(pline, pline_capacity, line_maxsz, ctx, decodefun, NULL, 0);
}

ssize_t vdecode_getline_buf(
                        char **         pline,
                        size_t *        pline_capacity,
                        size_t          line_maxsz,
                        void **         ctx,
                        const char *    inbuf,
                        size_t inbufsz) {
   return vdecode_getline(pline, pline_capacity, line_maxsz, ctx, NULL, inbuf, inbufsz);
}

