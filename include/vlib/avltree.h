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
} avltree_node_t;

/** how to visit the tree (direction) */
typedef enum {
    AFL_DEFAULT,
} avltree_flags_t;


/** avltree_t */
typedef struct {
    avltree_node_t *            root;
    avltree_flags_t             flags;
    avltree_cmpfun_t            cmp;
    avltree_freefun_t           free;
} avltree_t;

/** avltree_visitfun_t return value */
enum {
    AVS_GO_LEFT     = -24, /* only go to left node, but continue visit */
    AVS_GO_RIGHT    = -23, /* only go to right node, but continue visit */
    AVS_SKIP        = -22, /* skip this node, but continue visit */
    AVS_CONT        = -21, /* continue visit */
    AVS_ERROR       = -1,  /* stop visit, report error */
    AVS_FINISHED    = 0,   /* stop visit, report success */
} avltree_visit_status_t;

/** avltree_node_t visit function called on each node by avltree_visit()
 * @param tree the tree beiing visited, cannot be NULL (ensured by avltree_visit())
 * @param node the node beiing visited, cannot be NULL (ensured by avltree_visit())
 * @param data specific data for visit function
 * @return avltree_visit_status_t
 */
typedef int     (*avltree_visitfun_t) (avltree_t * tree, avltree_node_t * node, void * data);

/** how to visit the tree (direction) */
typedef enum {
    AVH_LEFT_LEAF   = 0,
    AVH_RIGHT_LEAF,
    AVH_LEFT_RIGHT,
    AVH_RIGHT_LEFT,
    AVH_DEFAULT = AVH_LEFT_RIGHT,
} avltree_visit_how_t;

/*****************************************************************************/

/** avltree_create() */
avltree_t *       avltree_create(
                    avltree_flags_t     flags,
                    avltree_cmpfun_t    cmpfun,
                    avltree_freefun_t   freefun);

/** avltree_insert() */
avltree_node_t *  avltree_insert(
                    avltree_t *         tree,
                    void *              data);

/** avltree_free() */
void            avltree_free(
                    avltree_t *         tree);

/** avltree_find() */
void *          avltree_find(
                    avltree_t *         tree,
                    const void *        data);

/** avltree_visit() */
int             avltree_visit(
                    avltree_t *         tree,
                    avltree_visitfun_t  visit,
                    void *              visit_data,
                    avltree_visit_how_t how);

/** avltree_remove() */
void *          avltree_remove(
                    avltree_t *         tree,
                    const void *        data);

/*****************************************************************************/

#ifdef __cplusplus
}
#endif

#endif /* ! ifndef *_H */

