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
 * Simple ring buffer utilities, supporting stack(lifo) & queue(fifo) features.
 */
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>

#include "vlib/rbuf.h"
#include "vlib/log.h"

#include "vlib_private.h"

/*****************************************************************************/

#define RBUF_IS_EMPTY(rbuf)     ((rbuf)->end >= (rbuf)->max_size)
#define RBUF_EMPTY              SIZE_MAX

struct rbuf_s {
    void **         tab;
    size_t          init_size;
    size_t          max_size;
    rbuf_flags_t    flags;
    size_t          start;
    size_t          end;
};

/*****************************************************************************/
rbuf_t *        rbuf_create(
                    size_t          max_size,
                    rbuf_flags_t    flags) {
    rbuf_t * rbuf;

    if (max_size == 0)  {
        return NULL;
    }
    if ((rbuf = malloc(sizeof(rbuf_t))) == NULL) {
        return NULL;
    }
    if ((rbuf->tab = malloc(sizeof(void *) * max_size)) == NULL) {
        free(rbuf);
        return NULL;
    }
    rbuf->max_size = max_size;
    rbuf->init_size = max_size;
    rbuf->flags = flags;
    rbuf_reset(rbuf);
    return rbuf;
}
/*****************************************************************************/
int             rbuf_reset(
                    rbuf_t *        rbuf) {
    if (rbuf == NULL) {
        return -1;
    }

    rbuf->start = 0;
    rbuf->end = RBUF_EMPTY;

    if ((rbuf->flags & RBF_SHRINK_ON_RESET) != 0 && rbuf->max_size > rbuf->init_size) {
        void * new = realloc(rbuf->tab, sizeof(void *) * rbuf->init_size);
        if (new == NULL) {
            return -1;
        }
        rbuf->tab = new;
        rbuf->max_size = rbuf->init_size;
    }

    return 0;
}
/*****************************************************************************/
void            rbuf_free(
                    rbuf_t *        rbuf) {
    if (rbuf == NULL) {
        return ;
    }
    if (rbuf->tab != NULL) {
        free(rbuf->tab);
    }
    free(rbuf);
}
/*****************************************************************************/
size_t          rbuf_size(
                    const rbuf_t *  rbuf) {
    ssize_t size;

    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return 0;
    }
    if (RBUF_IS_EMPTY(rbuf)) {
        return 0;
    }
    if ((size = 1 + rbuf->end - rbuf->start) <= 0) {
        size += rbuf->max_size;
    }
    return size;
}
/*****************************************************************************/
size_t          rbuf_maxsize(
                    const rbuf_t *  rbuf) {
    if (rbuf == NULL) {
        errno = EINVAL;
        return 0;
    }
    return rbuf->max_size;
}
/*****************************************************************************/
size_t          rbuf_memorysize(
                    const rbuf_t *  rbuf) {
    if (rbuf == NULL) {
        errno = EINVAL;
        return 0;
    }
    return sizeof(rbuf_t) + (rbuf->max_size * sizeof(void *));
}
/*****************************************************************************/
int             rbuf_push(
                    rbuf_t *        rbuf,
                    void *          data) {
    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return -1;
    }
    LOG_SCREAM(g_vlib_log, "rbuf_push(%lx) start:%lu end:%lu maxsize:%lu",
              (size_t)data, rbuf->start, rbuf->end, rbuf->max_size);

    if (RBUF_IS_EMPTY(rbuf)) {
        rbuf->tab[(rbuf->end = rbuf->start)] = data;
        return 0;
    }
    ++rbuf->end;
    if (rbuf->end == rbuf->max_size) {
        rbuf->end = 0;
    }
    if (rbuf->end == rbuf->start) {
        /* start is reached -> buffer is full */
        if ((rbuf->flags & RBF_OVERWRITE) == 0) {
            /* OVERWRITE MODE is OFF -> increase buffer size */
            void * new = realloc(rbuf->tab, sizeof(void *) * (rbuf->max_size * 2));
            if (new == NULL) {
                return -1;
            }
            rbuf->tab = new;
            if (rbuf->end > 0) {
                /* start == end but end > 0 meaning that data in range [0,end-1] must be moved */
                LOG_SCREAM(g_vlib_log, "memcpy realloc psh:%lx start:%lu end:%lu maxsz:%lu",
                          (size_t) data, rbuf->start, rbuf->end, rbuf->max_size);
                memcpy(rbuf->tab + rbuf->max_size, rbuf->tab, rbuf->end * sizeof(void *));
            }
            rbuf->end = rbuf->max_size + rbuf->end;
            rbuf->max_size *= 2;
        } else {
            /* OVERWRITE MODE is ON -> lose the bottom element */
            rbuf->start++;
            if (rbuf->start == rbuf->max_size) {
                rbuf->start = 0;
            }
        }
    }
    rbuf->tab[rbuf->end] = data;

    LOG_SCREAM(g_vlib_log, "rbuf_push(%lx) PUSHED. start:%lu end:%lu maxsize:%lu",
              (size_t)data, rbuf->start, rbuf->end, rbuf->max_size);

    return 0;
}
/*****************************************************************************/
void *          rbuf_top(
                    const rbuf_t *  rbuf) {
    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return NULL;
    }
    if (RBUF_IS_EMPTY(rbuf)) {
        errno = ENOENT;
        return NULL;
    }
    return rbuf->tab[rbuf->end];
}
/*****************************************************************************/
void *          rbuf_pop(
                    rbuf_t *        rbuf) {
    void *  ret;

    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return NULL;
    }
    if (RBUF_IS_EMPTY(rbuf)) {
        errno = ENOENT;
        return NULL;
    }
    ret = rbuf->tab[rbuf->end];
    if (rbuf->end == rbuf->start) {
        rbuf->end = RBUF_EMPTY;
        //rbuf->start = 0;
        return ret;
    }
    if (rbuf->end-- == 0) {
        rbuf->end = rbuf->max_size - 1;
    }
    return ret;
}
/*****************************************************************************/
void *          rbuf_bottom(
                    const rbuf_t *  rbuf) {
    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return NULL;
    }
    if (RBUF_IS_EMPTY(rbuf)) {
        errno = ENOENT;
        return NULL;
    }
    return rbuf->tab[rbuf->start];
}
/*****************************************************************************/
void *          rbuf_dequeue(
                    rbuf_t *        rbuf) {
    void * ret;

    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return NULL;
    }
    if (RBUF_IS_EMPTY(rbuf)) {
        errno = ENOENT;
        return NULL;
    }
    ret = rbuf->tab[rbuf->start];

    LOG_SCREAM(g_vlib_log, "rbuf_dequeue = %lu start:%lu end:%lu maxsize:%lu",
              (size_t)ret, rbuf->start, rbuf->end, rbuf->max_size);

    if (rbuf->end == rbuf->start) {
        rbuf->end = RBUF_EMPTY;
        //rbuf->start = 0;
        return ret;
    }
    if (++rbuf->start >= rbuf->max_size) {
        rbuf->start = 0;
    }
    return ret;
}
/*****************************************************************************/
void *          rbuf_get(
                    const rbuf_t *  rbuf,
                    size_t          index) {
    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return NULL;
    }
    if (RBUF_IS_EMPTY(rbuf) || index >= rbuf_size(rbuf)) {
        errno = ENOENT;
        return NULL;
    }
    return rbuf->tab[(rbuf->start + index) % rbuf->max_size];
}
/*****************************************************************************/
int             rbuf_set(
                    rbuf_t *        rbuf,
                    size_t          index,
                    void *          data) {
    size_t real_index;

    if (rbuf == NULL || rbuf->start >= rbuf->max_size) {
        errno = EINVAL;
        return -1;
    }
    if (index >= rbuf->max_size) {
        if ((rbuf->flags & RBF_OVERWRITE) != 0) {
            errno = ENOMEM;
            return -1;
        }
        /* push null until buffer is enough large for index */
        while (rbuf->max_size - 1 < index) {
            if (rbuf_push(rbuf, NULL) != 0) {
                return -1;
            }
        }
    }
    real_index = (rbuf->start + index) % rbuf->max_size;
    rbuf->tab[real_index] = data;

    if (RBUF_IS_EMPTY(rbuf) || index >= rbuf_size(rbuf)) {
        rbuf->end = real_index;
    }

    return 0;
}
/*****************************************************************************/


