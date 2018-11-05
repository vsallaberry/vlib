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
 * Simple avl tree utilities.
 * AVL: 1962, Georgii Adelson-Velsky & Evguenii Landis.
 */
#ifndef VLIB_AVLTREE_H
#define VLIB_AVLTREE_H

#include "rbuf.h"

#ifdef __cplusplus
extern "C" {
#endif

/** avltree_node_t data compare function, strcmp-like */
typedef int     (*avltree_cmpfun_t)   (const void *, const void *);
/** avltree_node_t data free function, free-like */
typedef void    (*avltree_freefun_t)  (void *);

/** avltree_node_t */
typedef struct avltree_node_s {
    void *                      data;
    struct avltree_node_s *     left;
    struct avltree_node_s *     right;
    char                        balance;
} avltree_node_t;

/** tree flags */
typedef enum {
    AFL_NONE            = 0,
    AFL_SHARED_STACK    = 1 << 0,
    AFL_DEFAULT         = AFL_SHARED_STACK,
} avltree_flags_t;

/** avltree_t */
typedef struct {
    avltree_node_t *            root;
    avltree_flags_t             flags;
    struct rbuf_s *             stack;
    avltree_cmpfun_t            cmp;
    avltree_freefun_t           free;
} avltree_t;

/** avltree_visitfun_t return value */
typedef enum {
    AVS_GO_LEFT     = -24, /* only go to left node, but continue visit */
    AVS_GO_RIGHT    = -23, /* only go to right node, but continue visit */
    AVS_SKIP        = -22, /* skip this node, but continue visit */
    AVS_CONT        = -21, /* continue visit */
    AVS_ERROR       = -1,  /* stop visit, report error */
    AVS_FINISHED    = 0,   /* stop visit, report success */
} avltree_visit_status_t;

/** data to be passed to avltree_visitfun_t functions */
typedef struct {
    size_t                  level;      /* current node level (depth) */
    size_t                  index;      /* current node index in level */
    const rbuf_t *          stack;      /* current stack */
    void *                  user_data;  /* specific user data from avltree_visit() */
} avltree_visit_data_t;

/** avltree_node_t visit function called on each node by avltree_visit()
 * @param tree the tree beiing visited, cannot be NULL (ensured by avltree_visit())
 * @param node the node beiing visited, cannot be NULL (ensured by avltree_visit())
 * @param visit_data avltree_visit_data_t struct with infos and user_data from avltree_visit()
 * @return avltree_visit_status_t
 */
typedef int     (*avltree_visitfun_t) (
                    avltree_t *             tree,
                    avltree_node_t *        node,
                    avltree_visit_data_t *  visit_data);

/** how to visit the tree (direction) */
typedef enum {
    AVH_INFIX_L     = 0,    /* infix visit(2nd), starting from left (increasing order) */
    AVH_INFIX_R,            /* infix visit(2nd), starting from right (decreasing order) */
    AVH_LARG_L,             /* width visit, starting from left */
    AVH_LARG_R,             /* width visit, starting from right */
    AVH_PREFIX_L,           /* prefix visit(1st), left child before right child */
    AVH_PREFIX_R,           /* prefix visit(1st), right child before left child */
    AVH_SUFFIX_L,           /* suffix visit(3rd), left child before right child */
    AVH_SUFFIX_R,           /* suffix visit(3rd), right child before left child */
    AVH_DEFAULT = AVH_INFIX_L,
} avltree_visit_how_t;

/*****************************************************************************/

/** AVLNODE : shortcut for avltree_node_create() */
#define AVLNODE(value, left, right)         avltree_node_create(NULL, value, left, right)

/** avltree_create() */
avltree_t *         avltree_create(
                        avltree_flags_t         flags,
                        avltree_cmpfun_t        cmpfun,
                        avltree_freefun_t       freefun);

/** avltree_node_create */
avltree_node_t *    avltree_node_create(
                        avltree_t *             tree,
                        void *                  data,
                        avltree_node_t *        left,
                        avltree_node_t *        right);

/** avltree_insert() */
avltree_node_t *    avltree_insert(
                        avltree_t *             tree,
                        void *                  data);

/** avltree_free() */
void                avltree_free(
                        avltree_t *             tree);

/** avltree_find() */
void *              avltree_find(
                        avltree_t *             tree,
                        const void *            data);

/** avltree_visit() */
int                 avltree_visit(
                        avltree_t *             tree,
                        avltree_visitfun_t      visit,
                        void *                  user_data,
                        avltree_visit_how_t     how);

/** avltree_remove() */
void *              avltree_remove(
                        avltree_t *             tree,
                        const void *            data);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

