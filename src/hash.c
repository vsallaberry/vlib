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
 * Simple Hash Table.
 */
#include <stdlib.h>
#include <string.h>

#include "vlib/hash.h"

struct hash_s {
    unsigned int    size;
    int             flags;
    slist_t **      table;
    hash_fun_t      hashfun;
    hash_free_fun_t freefun;
    hash_cmp_fun_t  cmpfun;
};

hash_t * hash_alloc(unsigned int size, int flags,
                    hash_fun_t hashfun, hash_cmp_fun_t cmpfun, hash_free_fun_t freefun) {
    hash_t * hash;

    if (!hashfun || !size || !cmpfun)
        return NULL;
    hash = malloc(sizeof(hash_t));
    if (!hash) {
        return NULL;
    }
    if ((hash->table = calloc(size, sizeof(slist_t *))) == NULL) {
        free(hash);
        return NULL;
    }
    hash->size = size;
    hash->flags = flags;
    hash->hashfun = hashfun;
    hash->cmpfun = cmpfun;
    hash->freefun = freefun;
    return hash;
}

int hash_insert(hash_t * hash, void * data) {
    unsigned int index;
    if (!hash) {
        return HASH_ERROR;
    }
    index = hash->hashfun(hash, data);
    if (index >= hash->size) {
        return HASH_ERROR;
    }
    if ((hash->flags & HASH_FLAG_DOUBLES) == 0 && hash->table[index] != NULL) {
        if (slist_find(hash->table[index], data, hash->cmpfun)) {
            return HASH_EALREADY;
        }
    }
    if ((hash->table[index] = slist_prepend(hash->table[index], data)) != NULL) {
        return HASH_SUCCESS;
    }
    return HASH_ERROR;
}

slist_t * hash_find_next(hash_t * hash, const void * data, slist_t * prev_match) {
    if (!hash) {
        return NULL;
    }
    if (!prev_match) {
        unsigned int index = hash->hashfun(hash, data);
        if (index >= hash->size) {
            return NULL;
        }
        prev_match = hash->table[index];
        if (prev_match == NULL) {
            return NULL;
        }
    }
    return slist_find(prev_match, data, hash->cmpfun);
}

void * hash_find(hash_t * hash, const void * data) {
    slist_t * elt;

    if ((elt = hash_find_next(hash, data, NULL)) != NULL) {
        return elt->data;
    }
    return NULL;
}

int hash_remove(hash_t * hash, const void * data) {
    unsigned int index;

    if (!hash) {
        return HASH_ERROR;
    }
    index = hash->hashfun(hash, data);
    if (index >= hash->size) {
        return HASH_ERROR;
    }
    hash->table[index] = slist_remove(hash->table[index], data, hash->cmpfun, hash->freefun);
    return HASH_SUCCESS; // FIXME don't know if found
}

void hash_free(hash_t * hash) {
    if (!hash)
        return ;
    for (unsigned int i = 0; i < hash->size; i++) {
        slist_free(hash->table[i], hash->freefun);
    }
    free(hash->table);
    hash->table = NULL;
    free(hash);
}

int hash_str(const hash_t * hash, const void * key) {
    const char *    ptr;
    int             value;
    int             tmp;

    value = 0;
    ptr = key;
    while (*ptr) {
        value = (value << 4) + *ptr;
        if ((tmp = (value & 0xf0000000))) {
            value = value ^ (tmp >> 24);
            value = value ^ tmp;
        }
        ptr++;
    }
    value &= 0x7ffffff;
    return value % hash->size;
}

int hash_strn(const hash_t * hash, const void * key, unsigned int len) {
    const char *    ptr;
    int             value;
    int             tmp;

    value = 0;
    ptr = key;
    while (len--) {
        value = (value << 4) + *ptr;
        if ((tmp = (value & 0xf0000000))) {
            value = value ^ (tmp >> 24);
            value = value ^ tmp;
        }
        ptr++;
    }
    value &= 0x7ffffff;
    return value % hash->size;
}

int hash_ptr(const hash_t * hash, const void * key) {
    char            s[sizeof(void *)];
    unsigned long   mask = 0xff;

    for(unsigned char i = 0; i < sizeof(void *); i++, mask <<= 8) {
        s[i] = (char) (((unsigned long) key & mask) >> (i * 8));
    }
    return hash_strn(hash, s, sizeof(void *));
}

int hash_ptrcmp(const void * key1, const void * key2) {
    return ((long) key1 - (long) key2);
}

int hash_stats_get(hash_t * hash, hash_stats_t * stats) {
    unsigned int i;
    unsigned int list_len;

    if (hash == NULL || stats == NULL) {
        return HASH_ERROR;
    }

    memset(stats, 0, sizeof(hash_stats_t));
    stats->hash_size = hash->size;
    stats->hash_flags = hash->flags;

    for (i = 0; i < hash->size; i++) {
        list_len = slist_length(hash->table[i]);
        if (list_len > 1) {
            stats->n_collisions += (list_len - 1);
            stats->n_indexes_with_collision++;
        }
        if (list_len > 0) {
            stats->n_indexes++;
            stats->n_elements += list_len;
        }
    }
    return HASH_SUCCESS;
}

