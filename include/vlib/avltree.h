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
    AVS_NEXTVISIT   = -20, /* stop current visit and start next one (prefix,infix,suffix) */
    AVS_ERROR       = -1,  /* stop visit, report error */
    AVS_FINISHED    = 0,   /* stop visit, report success */
} avltree_visit_status_t;

/** how to visit the tree (direction) */
typedef enum {
    AVH_PREFIX  = 1 << 0,   /* prefix (first visit, before the two childs) */
    AVH_INFIX   = 1 << 1,   /* infix (second visit between the two childs) */
    AVH_SUFFIX  = 1 << 2,   /* suffix (third visit, after the two childs */
    AVH_BREADTH = 1 << 3,   /* breadth-first (width visit) */
    AVH_RIGHT   = 1 << 7,   /* visit modifier: visit right child before left child */
    AVH_DEFAULT = AVH_PREFIX,
} avltree_visit_how_t;

/** data to be passed to avltree_visitfun_t functions */
typedef struct {
    avltree_visit_how_t         how;    /* current visit state (prefix,infix,...) */
    size_t                      level;  /* current node level (depth) */
    size_t                      index;  /* current node index in level */
    const rbuf_t *              stack;  /* current stack */
} avltree_visit_context_t;

/** avltree_node_t visit function called on each node by avltree_visit()
 * @param tree the tree beiing visited, cannot be NULL (ensured by avltree_visit())
 * @param node the node beiing visited, cannot be NULL (ensured by avltree_visit())
 * @param context avltree_visit_context_t struct with node infos and visit state,
 *        cannot be NULL (ensured by avltree_visit())
 * @param user_data the specific user data given to avltree_visit
 * @return avltree_visit_status_t
 */
typedef int         (*avltree_visitfun_t) (
                        avltree_t *                 tree,
                        avltree_node_t *            node,
                        avltree_visit_context_t *   context,
                        void *                      user_data);

/*****************************************************************************/

/** AVLNODE : shortcut for avltree_node_create() */
#define AVLNODE(value, left, right)     avltree_node_create(NULL, value, left, right)

/** avltree_create() */
avltree_t *         avltree_create(
                        avltree_flags_t             flags,
                        avltree_cmpfun_t            cmpfun,
                        avltree_freefun_t           freefun);

/** avltree_node_create */
avltree_node_t *    avltree_node_create(
                        avltree_t *                 tree,
                        void *                      data,
                        avltree_node_t *            left,
                        avltree_node_t *            right);

/** avltree_insert() */
avltree_node_t *    avltree_insert(
                        avltree_t *                 tree,
                        void *                      data);

/** avltree_free() */
void                avltree_free(
                        avltree_t *                 tree);

/** avltree_find() */
void *              avltree_find(
                        avltree_t *                 tree,
                        const void *                data);

/** avltree_visit()
 * The given function will be called on each node in an order specified by how.
 * @param tree the tree to visit
 * @param visit the visit function (avltree_visitfun_t)
 * @param user_data the specific user_data to give to visit function
 * @param how a combination of avltree_visit_how_t, giving visit type(s) (prefix|infix|...) */
int                 avltree_visit(
                        avltree_t *                 tree,
                        avltree_visitfun_t          visit,
                        void *                      user_data,
                        avltree_visit_how_t         how);

/** avltree_remove() */
void *              avltree_remove(
                        avltree_t *                 tree,
                        const void *                data);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

