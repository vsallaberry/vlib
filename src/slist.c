/*
 * Copyright (C) 2017 Vincent Sallaberry
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
#include <stdlib.h>

#include "vlib/slist.h"

static slist_t * slist_alloc() {
    return malloc(sizeof(slist_t));
}

slist_t * slist_prepend(slist_t * list, void * data) {
    slist_t * new = slist_alloc();
    if (new) {
        new->data = data;
        new->next = list;
        return new;
    }
    return list;
}

slist_t * slist_append(slist_t * list, void * data) {
    slist_t * new = slist_alloc();
    if (new) {
        slist_t * tail;

        new->data = data;
        new->next = NULL;
        if (!list)
            return new;
        for (tail = list; tail->next; tail = tail->next)
            ; /* go to list tail */
        tail->next = new;
    }
    return list;
}

slist_t * slist_concat(slist_t * list1, slist_t * list2) {
    slist_t * tail;

    if (!list2)
        return list1;
    if (!list1)
        return list2;

    for (tail = list1; tail->next; tail = tail->next)
        ; /* go to list tail */
    tail->next = list2;
    return list1;
}

slist_t * slist_find_ptr(slist_t * list, const void * data) {
    for ( ; list; list = list->next) {
        if (list->data == data) {
            return list;
        }
    }
    return NULL;
}

slist_t * slist_find(slist_t * list, const void * data, slist_cmp_fun_t cmpfun) {
    if (!cmpfun) {
        return NULL;
    }
    for ( ; list; list = list->next) {
        if (cmpfun(list->data, data) == 0) {
            return list;
        }
    }
    return NULL;
}

slist_t * slist_remove(slist_t * list, const void * data,
                       slist_cmp_fun_t cmpfun, slist_free_fun_t freefun) {
    slist_t * head = list;
    slist_t * tofree;

    if (!list || !cmpfun) {
        return list;
    }
    if (cmpfun(list->data, data) == 0) {
        head = list->next;
        slist_free_1(list, freefun);
        return head;
    }
    for ( ; list->next && cmpfun(list->next->data, data); list = list->next)
        ; // go up to matching element or NULL
    if (list->next) {
        tofree = list->next;
        list->next = list->next->next;
        slist_free_1(tofree, freefun);
    }
    return head;
}

slist_t * slist_remove_ptr(slist_t * list, const void * data) {
    (void)data;
    return list;
}

unsigned int    slist_length(slist_t * list) {
    unsigned int len = 0;
    for ( ; list; list = list->next) {
        len++;
    }
    return len;
}


void slist_free_1(slist_t * list, slist_free_fun_t freefun) {
    if (list) {
        if (freefun) {
            freefun(list->data);
        }
        free(list);
    }
}

void slist_free(slist_t * list, slist_free_fun_t freefun) {
    slist_t * tofree;
    while (list) {
        tofree = list;
        list = list->next;
        slist_free_1(tofree, freefun);
    }
}

