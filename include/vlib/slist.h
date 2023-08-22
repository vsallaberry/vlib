/*
 * Copyright (C) 2017-2020,2023 Vincent Sallaberry
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
 * Simple single linked list.
 */
#ifndef VLIB_SLIST_H
#define VLIB_SLIST_H

#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif

/** single linked list element */
typedef struct slist_s {
    struct slist_s *    next;
    void *              data; /* must be last for the  slist_*_sized() functions */
} slist_t;

/** struct with slist head and slist tail */
#define SHLIST_INITIALIZER()    ((shlist_t) { .head = NULL, .tail = NULL })
typedef struct {
    slist_t *           head;
    slist_t *           tail;
} shlist_t;

typedef void    (*slist_free_fun_t)(void *);
typedef int     (*slist_cmp_fun_t)(const void *, const void *);

slist_t *       slist_prepend(slist_t * list, void * data);
slist_t *       slist_append(slist_t * list, void * data);

/* append data at (*last) and set (*last) to appended element
 * last can be NULL, if *last is NULL, append at end of list */
slist_t *       slist_appendto(slist_t * list, void * data, slist_t ** last);

slist_t *       slist_insert_sorted(slist_t * list, void * data, slist_cmp_fun_t cmpfun);

slist_t *       slist_concat(slist_t * list1, slist_t * list2);

const slist_t * slist_find(const slist_t * list, const void * data, slist_cmp_fun_t cmpfun);
const slist_t * slist_find_ptr(const slist_t * list, const void * data);

slist_t *       slist_remove(slist_t * list, const void * data,
                             slist_cmp_fun_t cmpfun, slist_free_fun_t freefun);
slist_t *       slist_remove_ptr(slist_t * list, const void * data);

unsigned int    slist_length(const slist_t * list);

void            slist_free_1(slist_t * list, slist_free_fun_t freefun);
void            slist_free(slist_t * list, slist_free_fun_t freefun);

/* special slist_t allocation which incorporates the data with given size
 * For iteration, use the macro SLIST*_FOREACH_PDATA(). data can be NULL. */
slist_t *       slist_prepend_sized(slist_t * list, const void * data, size_t data_sz);
slist_t *       slist_append_sized(slist_t * list, const void * data, size_t data_sz);
slist_t *       slist_appendto_sized(slist_t * list, const void * data, size_t data_sz, slist_t ** last);
slist_t *       slist_insert_sorted_sized(slist_t * list, const void * data, size_t data_sz, slist_cmp_fun_t cmpfun);
const slist_t * slist_find_sized(const slist_t * list, const void * data, slist_cmp_fun_t cmpfun);
slist_t *       slist_remove_sized(slist_t * list, const void * data,
                                   slist_cmp_fun_t cmpfun, slist_free_fun_t freefun);
void            slist_free_1_sized(slist_t * list, slist_free_fun_t freefun);
void            slist_free_sized(slist_t * list, slist_free_fun_t freefun);

/**
 * for loop iterating on each 'slist_t *' element of the list
 * Can be folowed by { } block.
 * Eg: SLIST_FOREACH_ELT(list, elt) printf("%s\n", (char *)elt->data);
 * */
#define SLIST_FOREACH_ELT_T(_TYPE, _list, _iter) \
                for (_TYPE _iter = (_list); (_iter); (_iter) = (_iter)->next)

#define SLIST_FOREACH_ELT(_list, _iter) \
                SLIST_FOREACH_ELT_T(slist_t *, _list, _iter)

#define SLISTC_FOREACH_ELT(_list, _iter) \
                SLIST_FOREACH_ELT_T(const slist_t *, _list, _iter)

/**
 * for loop iterating on each 'type' data element of slist_t * node
 * Can be followed by { } block.
 * Eg: SLIST_FOREACH_DATA(list, str, char *) { printf("%s\n", str); }
 */
#define SLIST_DATA(list)    ((list)->data)
#define SLIST_PDATA(list)   ( (void *) &((list)->data) )

#define SLIST_FOREACH_DATA_T(_TYPE, _list, _iter, _dtype, _getdata) \
                for(_TYPE _it_list = (_list); (_it_list); (_it_list) = NULL) \
                    for(_dtype _iter; \
                        (_it_list) && (((_iter) = (_dtype)(_getdata(_it_list))) || 1); \
                        _it_list = (_it_list)->next)

#define SLIST_FOREACH_DATA(_list, _iter, _type) \
            SLIST_FOREACH_DATA_T(slist_t *, _list, _iter, _type, SLIST_DATA)

#define SLISTC_FOREACH_DATA(_list, _iter, _type) \
            SLIST_FOREACH_DATA_T(const slist_t *, _list, _iter, _type, SLIST_DATA)

#define SLIST_FOREACH_PDATA(_list, _iter, _type) \
            SLIST_FOREACH_DATA_T(slist_t *, _list, _iter, _type, SLIST_PDATA)

#define SLISTC_FOREACH_PDATA(_list, _iter, _type) \
            SLIST_FOREACH_DATA_T(const slist_t *, _list, _iter, _type, SLIST_PDATA)

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */
