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
#ifndef VLIB_HASH_H
#define VLIB_HASH_H

#include "slist.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Default table size - prime number for better results */
#define HASH_DEFAULT_SIZE   4001

/** Return values */
#define HASH_ERROR          -1
#define HASH_SUCCESS        0
#define HASH_EALREADY       1

/** HASH Flags */
#define HASH_FLAG_DOUBLES   1

/** Opaque Hash Table type */
typedef struct hash_s       hash_t;

/** Function pointer to hashing function */
typedef int                 (*hash_fun_t)(const hash_t * hash, const void * key);
/** Function pointer to __data__ freeing function (can be NULL) */
typedef slist_free_fun_t    hash_free_fun_t;
/** Function pointer __data__ comparing function */
typedef slist_cmp_fun_t     hash_cmp_fun_t;

/** Hash Table Creation */
hash_t *        hash_alloc(unsigned int size, int flags,
                           hash_fun_t hashfun, hash_cmp_fun_t cmpfun, hash_free_fun_t freefun);

/** Hash Table Insertion */
int             hash_insert(hash_t * hash, void * data);

/** Hash Table Lookup : get first matching data */
void *          hash_find(hash_t * hash, const void * data);

/** Hash Table Lookup : get next matching element (first if prev_match is NULL) */
slist_t *       hash_find_next(hash_t * hash, const void * data, slist_t * prev_match);

/** Hash Table Remove element function */
int             hash_remove(hash_t * hash, const void * data);

/** Hash Table Destroy function */
void            hash_free(hash_t * hash);

/** String hashing function */
int             hash_str(const hash_t * hash, const void * key);
/** Pointer hashing function */
int             hash_ptr(const hash_t * hash, const void * key);
/** Pointer comparing function */
int             hash_ptrcmp(const void * key1, const void * key2);

/** struct for getting statistics about hash table */
typedef struct {
    unsigned int    hash_size;
    int             hash_flags;
    unsigned int    n_elements;
    unsigned int    n_indexes;
    unsigned int    n_indexes_with_collision;
    unsigned int    n_collisions;
} hash_stats_t;

/** Hash Table Statistics */
int             hash_stats_get(hash_t * hash, hash_stats_t * stats);

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

