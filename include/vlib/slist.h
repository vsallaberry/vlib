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
 * Simple single linked list.
 */
#ifndef VLIB_SLIST_H
#define VLIB_SLIST_H

#ifdef __cplusplus
extern "C" {
#endif

/** single linked list element */
typedef struct slist_s {
    void *              data;
    struct slist_s *    next;
} slist_t;

typedef void    (*slist_free_fun_t)(void *);
typedef int     (*slist_cmp_fun_t)(const void *, const void *);

slist_t *       slist_prepend(slist_t * list, void * data);
slist_t *       slist_append(slist_t * list, void * data);

slist_t *       slist_concat(slist_t * list1, slist_t * list2);

slist_t *       slist_find(slist_t * list, const void * data, slist_cmp_fun_t cmpfun);
slist_t *       slist_find_ptr(slist_t * list, const void * data);

slist_t *       slist_remove(slist_t * list, const void * data,
                             slist_cmp_fun_t cmpfun, slist_free_fun_t freefun);
slist_t *       slist_remove_ptr(slist_t * list, const void * data);

unsigned int    slist_length(slist_t * list);

void            slist_free_1(slist_t * list, slist_free_fun_t freefun);
void            slist_free(slist_t * list, slist_free_fun_t freefun);

/**
 * for loop iterating on each 'slist_t *' element of the list
 * Can be folowed by { } block.
 * Eg: FOREACH_SLIST_ELT(list elt) printf("%s\n", (char *)elt->data);
 * */
#define SLIST_FOREACH_ELT(list, iter) \
                for (slist_t * iter = list; iter; iter = iter->next)

/**
 * for loop iterating on each 'type' data element of slist_t * node
 * Can be followed by { } block.
 * Eg: FOREACH_SLIST_DATA(list, str, char *) { printf("%s\n", str); }
 */
#define SLIST_FOREACH_DATA(list, iter, type) \
                for(slist_t * it_list = (list); it_list; ) \
                    for(type iter; \
                        (it_list) && ((iter = (type)((it_list)->data))||1); \
                        it_list = (it_list)->next)

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */
