/*
 * Copyright (C) 2018 Vincent Sallaberry
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
 * Simple account utilities.
 * Initially written for vrunas (<https://github.com/vsallaberry/vrunas>,
 *                               Copyright (C) 2018 Vincent Sallaberry.)
 */
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "vlib/account.h"
#include "vlib_private.h"

static int nam2id_alloc_r(char ** pbuf, size_t * pbufsz) {
    if ((pbuf == NULL || pbufsz == NULL) && (errno = EFAULT))
        return -1;
    if (*pbuf == NULL) {
        static const int    confs[] = { _SC_GETPW_R_SIZE_MAX, _SC_GETGR_R_SIZE_MAX };
        long                size = 0, ret;
        for (size_t i = 0; i < sizeof(confs) / sizeof(*confs); i++) {
            if ((ret = sysconf(confs[i])) > size)
                size = ret;
        }
        *pbufsz = (size > 0 ? size : 16384);
        if ((*pbuf = malloc(*pbufsz)) == NULL)
            LOG_VERBOSE(g_vlib_log, "nam2id_alloc(malloc): %s", strerror(errno));
    }
    return *pbuf ? 0 : -1;
}

int pwfind_r(const char * str, struct passwd * pw, char ** pbuf, size_t * pbufsz) {
    struct passwd *     pwres;
    int                 ret = -1;

    if (((str == NULL || pw == NULL) && (errno = EFAULT))
    ||  nam2id_alloc_r(pbuf, pbufsz) != 0) {
        return -1;
    }

    if (getpwnam_r(str, pw, *pbuf, *pbufsz, &pwres) != 0
    ||  (pwres != pw && (errno = EINVAL))) {
        LOG_VERBOSE(g_vlib_log, "user `%s` (getpwnam_r): %s", str, strerror(errno));
    } else {
        ret = errno = 0;
    }

    return ret;
}

int grfind_r(const char * str, struct group * gr, char ** pbuf, size_t * pbufsz) {
    struct group *      grres;
    int                 ret = -1;

    if (((str == NULL || gr == NULL) && (errno = EFAULT))
    ||  nam2id_alloc_r(pbuf, pbufsz) != 0) {
        return -1;
    }

    if (getgrnam_r(str, gr, *pbuf, *pbufsz, &grres) != 0
    ||  (grres != gr && (errno = EINVAL))) {
        LOG_VERBOSE(g_vlib_log, "group `%s` (getgrnam_r): %s", str, strerror(errno));
    } else {
        ret = errno = 0;
    }

    return ret;
}

int pwfindid_r(const char * str, uid_t *uid, char ** pbuf, size_t * pbufsz) {
    struct passwd       pw;
    char *              buf = NULL;
    size_t              bufsz;
    int                 ret;

    if ((uid == NULL && (errno = EFAULT))) {
        return -1;
    }

    if (pbuf == NULL) {
        pbuf    = &buf;
        pbufsz  = &bufsz;
    }

    if ((ret = pwfind_r(str, &pw, pbuf, pbufsz)) == 0) {
        errno = 0;
        *uid = pw.pw_uid;
    }

    if (pbuf == &buf)
        free(*pbuf);
    return ret;
}

int grfindid_r(const char * str, gid_t *gid, char ** pbuf, size_t * pbufsz) {
    struct group        gr;
    char *              buf = NULL;
    size_t              bufsz;
    int                 ret;

    if ((gid == NULL && (errno = EFAULT))) {
        return -1;
    }

    if (pbuf == NULL) {
        pbuf    = &buf;
        pbufsz  = &bufsz;
    }

    if ((ret = grfind_r(str, &gr, pbuf, pbufsz)) == 0) {
        errno = 0;
        *gid = gr.gr_gid;
    }

    if (pbuf == &buf)
        free(*pbuf);
    return ret;
}


