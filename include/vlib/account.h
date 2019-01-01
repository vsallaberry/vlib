/*
 * Copyright (C) 2018-2019 Vincent Sallaberry
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
 */
#ifndef VLIB_ACCOUNT_H
#define VLIB_ACCOUNT_H

#include <sys/types.h>
#include <pwd.h>
#include <grp.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * pwfind_r(): wrapper to getpwnam_r() with automatic memory allocation,
 * retrieving the passwd struct from user name.
 * @param str [in] the user_name to look for
 * @param pw  [out] the resulting passwd struct
 * @param pbuf [in/out] the pointer to buffer used by getpwnam_r and getgrnam_r.
 *   * function will fail if it is NULL.
 *   * if not null and allocated, it is used,
 *   * if not null and not allocated, it is malloced.
 *   When pbuf not null, the caller must free *pbuf.
 *   *pbuf can be shared between pwfind_r, pwfindid_r(), grfind_r() and grfindid_r(),
 *   knowing that data in *pbuf can be overriden at each call.
 * @param pbufsz [in/out] the pointer to size of *pbuf. Is ignored when pbuf is NULL.
 * @return 0 on success, -1 otherwise.
 */
int     pwfind_r(const char * str, struct passwd * pw, char ** pbuf, size_t * pbufsz);

/**
 * pwfindbyid_r(): wrapper to getpwuid_r() with automatic memory allocation,
 * retrieving the passwd struct from uid.
 * See pwfind_r().
 */
int     pwfindbyid_r(uid_t uid, struct passwd * gr, char ** pbuf, size_t * pbufsz);

/**
 * grfind_r(): wrapper to getgrnam_r() with automatic memory allocation,
 * retrieving the group struct from group name.
 * See pwfind_r().
 */
int     grfind_r(const char * str, struct group * gr, char ** pbuf, size_t * pbufsz);

/**
 * grfindbyid_r(): wrapper to getgrgid_r() with automatic memory allocation,
 * retrieving the group struct from gid.
 * See pwfind_r().
 */
int     grfindbyid_r(gid_t gid, struct group * gr, char ** pbuf, size_t * pbufsz);

/**
 * pwfindid_r(): wrapper to getpwnam_r() with automatic memory allocation,
 * retrieving the uid from user name.
 * @param str the user_name to look for
 * @param uid the resulting uid
 * @param pbuf the pointer to buffer used by getpwnam_r and getgrnam_r.
 *   * if NULL it is malloced and freed,
 *   * if not null and allocated, it is used,
 *   * if not null and not allocated, it is malloced.
 *   When pbuf not null, the caller must free *pbuf.
 *   *pbuf can be shared between pwnam2id() and grnam2id().
 * @param pbufsz the pointer to size of *pbuf. Is ignored when pbuf is NULL.
 * @return 0 on success, -1 otherwise.
 */
int     pwfindid_r(const char * str, uid_t * uid, char ** pbuf, size_t * pbufsz);

/**
 * grfindid_r(): wrapper to getgrnam_r() with automatic memory allocation,
 * retrieving the gid from group name.
 * See pwfindid_r() */
int     grfindid_r(const char * str, gid_t * gid, char ** pbuf, size_t * pbufsz);

#ifdef __cplusplus
}
#endif
#endif /* ! ifndef *_H */

