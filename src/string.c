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
 * Simple string utilities.
 */
#include <inttypes.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>

#include "vlib/util.h"

/* ************************************************************************ */
size_t str0cpy(char *dst, const char *src, size_t maxlen) {
    size_t len;

    if (maxlen == 0 || dst == NULL) {
        return 0;
    }
    if (src == NULL) {
        *dst = 0;
        return 0;
    }
    len = stpncpy(dst, src, maxlen - 1) - dst;
    if (len >= maxlen) { /* should not happend */
        *dst = 0;
        return 0;
    }
    dst[len] = 0;
    return len;
}

/* ************************************************************************ */
size_t strn0cpy(char *dst, const char *src, size_t len, size_t maxlen) {
    if (maxlen == 0 || dst == NULL) {
        return 0;
    }
    if (src == NULL) {
        *dst = 0;
        return 0;
    }
    if (len >= maxlen) {
        len = maxlen - 1;
    }
    len = stpncpy(dst, src, len) - dst;
    if (len >= maxlen) { /* should not happend */
        *dst = 0;
        return 0;
    }
    dst[len] = 0;
    return len;
}

/* ************************************************************************ */
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

/* ************************************************************************ */
int fnmatch_patternidx(const char * str) {
    size_t idx ;

    if (str == NULL)
        return -1;

    idx = strcspn(str, "*[?");
    if (str[idx] == 0)
        return -1;

    if (idx == 0 || str[idx - 1] != '\\')
        return idx;

    for (idx = 0; str[idx] != 0; ++idx) {
        if (str[idx] == '\\' && str[idx + 1] != 0) {
            ++idx;
        } else if (str[idx] == '*' || str[idx] == '[' || str[idx] == '?') {
            return idx;
        }
    }
    return -1;
}

/* ************************************************************************ */
#define VSTRTONUM_BODY(_str, _endptr, _base, _result, _TYPE, _FUN, _no_minus) \
    char * end;                                                     \
    _TYPE res;                                                      \
                                                                    \
    if ((_str) == NULL || (_result) == NULL) {                      \
        errno = EFAULT;                                             \
        return -1;                                                  \
    }                                                               \
    if (_no_minus) {                                                \
        while (isspace(*(_str))) ++(_str);                          \
        if (*(_str) == '-') {                                       \
            if ((_endptr) != NULL) *(_endptr) = (char*)(_str);      \
            errno = ERANGE;                                         \
            return -1;                                              \
        }                                                           \
    }                                                               \
    errno = 0;                                                      \
    end = NULL;                                                     \
    res = _FUN(_str, &end, _base);                                  \
    if (errno != 0) {                                               \
        return -1;                                                  \
    }                                                               \
    if ((_endptr) == NULL) {                                        \
        if (end == NULL || end == (_str) || *(_str) == 0 ||  *end != 0) { \
            errno = EINVAL;                                         \
            return -1;                                              \
        }                                                           \
    } else {                                                        \
        *(_endptr) = end;                                           \
    }                                                               \
    *(_result) = res;                                               \
    return 0;

int vstrtoimax(const char * str, char ** endptr, int base, intmax_t * imax) {
    VSTRTONUM_BODY(str, endptr, base, imax, intmax_t, strtoimax, 0);
}
int vstrtoumax(const char * str, char ** endptr, int base, uintmax_t * umax) {
    VSTRTONUM_BODY(str, endptr, base, umax, uintmax_t, strtoumax, 1);
}
int vstrtol(const char * str, char ** endptr, int base, long * l) {
    VSTRTONUM_BODY(str, endptr, base, l, long, strtol, 0);
}
int vstrtoul(const char * str, char ** endptr, int base, unsigned long * ul) {
    VSTRTONUM_BODY(str, endptr, base, ul, unsigned long, strtoul, 1);
}

static inline double s_strtod(const char * str, char ** endptr, int base) {
    (void) base;
    return strtod(str, endptr);
}
static inline long double s_strtold(const char * str, char ** endptr, int base) {
    (void) base;
    return strtold(str, endptr);
}
int vstrtold(const char * str, char ** endptr, long double * ld) {
    VSTRTONUM_BODY(str, endptr, 0, ld, long double, s_strtold, 0);
}
int vstrtod(const char * str, char ** endptr, double * d) {
    VSTRTONUM_BODY(str, endptr, 0, d, double, s_strtod, 0);
}

